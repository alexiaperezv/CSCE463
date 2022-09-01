/*
* This file includes UDP function definitions & global variables
* CSCE 463-500, Spring 2022
* By Alexia Perez
*/

#include "pch.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

/* Global Variables (used in multiple functions) */
mutex mtx;

/* Stats Thread Function */
DWORD WINAPI showStats(LPVOID lpParam) {

	Parameters* sp = (Parameters*)lpParam;				// cast args passed into thread function back to their original type
	
	// variables for keeping track of time 
	clock_t now;
	clock_t start = clock();
	clock_t prevTime = start;
	double timeInterval;

	int prevBase = 0;									// used too move the base 

	// run while-loop every 2 seconds until eventQuit or timeout
	while (WaitForSingleObject(sp->eventQuit, 2000) == WAIT_TIMEOUT) 
	{
		
		now = clock();

		// calculate received MBs
		float mb = (sp->B - prevBase) * ((MAX_PKT_SIZE - sizeof(SenderDataHeader))/1e6) * 8;
		
		prevBase = sp->B;								// update base 
		timeInterval = (now - start) / CLOCKS_PER_SEC;	// update active time interval 
		prevTime = now;									// update last time for which stats were calculated
		sp->speed = mb / timeInterval;					// update speed
		
		// print required stats
		printf("[%4d] B %8d ( %5.1f MB) N %8d T %4d F%4d W %4d S %4.3f Mbps RTT %.3f\n", 
			(now - start) / CLOCKS_PER_SEC, 
			sp->B, 
			sp->MB, 
			sp->nextSeq,
			sp->T, 
			sp->F, 
			sp->effWin, 
			sp->speed,
			sp->RTT
		);
	}
	
	return STATUS_OK;
}

/* Worker Thread Function */
DWORD WINAPI runWorker(LPVOID lpParam) 
{

	SenderSocket* ss = (SenderSocket*)lpParam;									// cast args passed into thread function back to their original type
	
	// re-size kernel
	int kernel_buffer = 20e6;
	if (setsockopt(ss->sock, SOL_SOCKET, SO_RCVBUF, (char*)&kernel_buffer, sizeof(int)) == SOCKET_ERROR) {
		printf("Failed to increase inbound kernel buffer with %d", WSAGetLastError());
		return UNKNOWN_ERROR;
	}

	kernel_buffer = 20e6; 
	if (setsockopt(ss->sock, SOL_SOCKET, SO_SNDBUF, (char*)&kernel_buffer, sizeof(int)) == SOCKET_ERROR) {
		printf("Failed to increase outbound kernel buffer with %d", WSAGetLastError());
		return UNKNOWN_ERROR;
	}

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);		// set this thread's priority to time critical

	// set up socketReceiveReady event to detect new info in the socket
	if (WSAEventSelect(ss->sock, ss->socketReceiveReady, FD_READ) == SOCKET_ERROR)
	{
		printf("WSAEventSelect failed with error %d\n", WSAGetLastError());
		return UNKNOWN_ERROR;
	}

	HANDLE events[] = { ss->socketReceiveReady, ss->full, ss->sp.finished };	// create array of event handles

	DWORD timeout;																// will store updated timeout val
	int nextToSend = ss->sp.B;													// next packet to be sent 

	while (true) 
	{
		if (ss->sp.nextSeq != ss->sndBase)										// there are packets still left in the shared queue
		{
			timeout = 1000*ss->RTO;												// set the timeout value 
			ss->sp.timeExpire = timeout;										// update packet timeout (when its time "expires")
		}
		else 
		{
			timeout = INFINITE;													// if there aren't any packets left in the queue, set the timeout val to infinite
		}
		
		int ret = WaitForMultipleObjects(3, events, false, timeout);			// wait for one of the events in the array "events" (above) to occur
		
		int index;
		int pktBytes;
		char* pktBuffer;
		Packet currentPkt;

		switch (ret)															
		{
		case WAIT_OBJECT_0:														// if the first event to occur is "socketReceiveReady", attempt to receive ACK | Ref: https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents
			ss->ReceiveACK();
			break;
		case WAIT_OBJECT_0 + 1:													// if the first event to occur is "full" (meaning there is data to be processed in the socket)
			index = nextToSend % ss->winSize;
			currentPkt = ss->pending_pkts[index];
			pktBuffer = (char*)&currentPkt;
			pktBytes = ss->pktBytes[index];

			// attempt to send the packet 
			if (sendto(ss->sock, pktBuffer, sizeof(SenderDataHeader) + pktBytes, 0, (struct sockaddr*)&ss->remote, int(sizeof(ss->remote))) == SOCKET_ERROR) 
			{
				return FAILED_SEND;
			}

			// packet successfully sent
			ss->pktRtx[index] += 1;												// update number of transmission attempts for this packet
			nextToSend++;														// update next packet to be sent 
			ss->sp.nextSeq += 1;												// update next sequence number 

			break;

		case WAIT_TIMEOUT:														// if the first event to occur is a timeout
			
			if (ss->sp.updateDone and ss->sndBase == ss->seq)					// check whether all packets were sent
			{
				return 1;
			}

			if (ss->pktRtx[ss->sndBase % ss->winSize] < MAX_ATTEMPTS)			// check that max rtx attempts were not used 
			{
				index = nextToSend % ss->winSize;
				currentPkt = ss->pending_pkts[index];
				pktBuffer = (char*)&currentPkt;
				pktBytes = ss->pktBytes[index];
				
				// attempt to rtx (send)
				if (sendto(ss->sock, pktBuffer, sizeof(SenderDataHeader) + pktBytes, 0, (struct sockaddr*)&ss->remote, int(sizeof(ss->remote))) == SOCKET_ERROR) 
				{
					return FAILED_SEND;
				}

				// packet rtx succeeded 
				ss->pktRtx[index] += 1;											// update number of rtx attempts for this packet
				ss->sp.T += 1;													// update number of packets with timeouts
			}
			else                                                                // max number of rtx attempts were used
			{
				return TIMEOUT;
			}

			break;

		case WAIT_OBJECT_0 + 2:													// worker quit
			return 1;
			
		default:																// default case 

			if (ss->sndBase == ss->seq) 
			{
				return 1;
			}

			break;
		}
	}

	return STATUS_OK;
}

SenderSocket::~SenderSocket()
{
	// destructor must check if threads are still running and greacefully terminate them before deleting shared data objects inside the class
}

/* Open Function - Takes care of SYN */
int SenderSocket::Open(char* targetHost, int port, int senderWindow, LinkProperties* lp)
{
	if (isOpen)															// open function was already called
	{
		return ALREADY_CONNECTED;
	}

	winSize = senderWindow;												// Set the SenderSocket window size
	pending_pkts = new Packet[winSize];									// initialize shared queue of packets to be sent
	pktBytes = new int[winSize];										// initialize shared queue of packet bytes (for each packet to be sent)
	pktTxtime = new clock_t[winSize];									// initialize shared queue of packet transmission times (for each packet to be sent)
	pktRtx = new int[winSize];											// initialize shared queue of packet rtx attempts (for each packet to be sent)

	clock_t start;

	// set up a UDP socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) 
	{
		return INVALID_SOCKET;
	}
	
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);

	// bind UDP socket
	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) 
	{

		return FAILED_BIND;
	}
	
	// set up remote server socket
	struct hostent* r;
	remote.sin_family = AF_INET;
	remote.sin_port = htons(port);

	// check if target host is an IP address & perform DNS lookup
	DWORD IP = inet_addr(targetHost);
	if (IP == INADDR_NONE)												// if this is not a valid IP
	{
		if ((r = gethostbyname(targetHost)) == NULL)					// failed DNS lookup attempt
		{
			return INVALID_NAME;
		}
		else															// DNS lookup succeeeded
		{
			memcpy((char*)&(remote.sin_addr), r->h_addr, r->h_length);
		}
	}
	else																// this is a valid IP address 
	{
		remote.sin_addr.S_un.S_addr = IP;								// copy IP into sin_addr
	}

	float rto = 1.0;													// for SYN phase, default starting RTO = 1 second.
	sockLp = *lp;														// set SenderSocket's link properties
	
	// Create SYN packet 
	SenderSynHeader ssh;
	ssh.lp = *lp;
	ssh.lp.bufferSize = senderWindow;
	ssh.sdh.flags.SYN = 1;
	ssh.sdh.seq = 0;

	int count = 0;
	while (count < MAX_ATTEMPTS) 
	{
		start = clock();
		
		// attempt to send SYN packet
		if (sendto(sock, (char*)&ssh, sizeof(ssh), 0, (struct sockaddr*)&remote, int(sizeof(remote))) == SOCKET_ERROR) 
		{
			return FAILED_SEND;
		}
		
		// Get ready to receive SYN-ACK
		fd_set fd;
		FD_ZERO(&fd);
		FD_SET(sock, &fd);

		timeval tp;
		tp.tv_sec = (long)rto;
		tp.tv_usec = (rto - tp.tv_sec) * 1e6;

		struct sockaddr_in response;
		int size = sizeof(response);
		char* recvData = new char[sizeof(ReceiverHeader)];
		int bytes = 0;													// number of bytes received 
		int status = select(0, &fd, NULL, NULL, &tp);
		
		if (status == SOCKET_ERROR) 
		{
			return UNKNOWN_ERROR;
		}
		else if (status > 0)											// there is data in the socket
		{
			bytes = recvfrom(sock, recvData, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size);

			if (bytes == SOCKET_ERROR)
			{
				return FAILED_RECV;
			}

			ReceiverHeader* rh = (ReceiverHeader*)recvData;

			if (rh->flags.SYN != 1 or rh->flags.ACK != 1)				// received packet is not what we expected (not SYN-ACK), loop again
			{
				count += 1;
				continue;
			}
			
			rto = 3 * (1000 * (clock() - start) / CLOCKS_PER_SEC);		// update the function's rto
			RTO = rto;													// set SenderSocket rto value to function's rto
			sp.RTT = RTO;												// update the shared stats parameter RTT (estRTT) to be = to the sender socket's RTO
			isOpen = true;												// connection is open

			// create semaphores + socketReceiveReady event handle for thread synchronization
			socketReceiveReady = CreateEvent(NULL, false, false, NULL);
			empty = CreateSemaphore(NULL, 0, winSize, NULL);
			full = CreateSemaphore(NULL,  0, winSize, NULL);         
			worker_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)runWorker, this, 0, NULL);


			lastReleased = min(winSize, rh->recvWnd);					// update latestReleased
			ReleaseSemaphore(empty, lastReleased, NULL);				// semaphore released by lastReleased slots

			return STATUS_OK;

		}
		else 
		{
			count += 1;													// loop again
		}
	}

    return TIMEOUT;
}

/* Close Function - Takes care of FIN */
int SenderSocket::Close(float *estimated_RTT)
{
	if (!isOpen)											// close function was already called (connection is not open)
	{
		return NOT_CONNECTED;
	}

	sp.updateDone = true;									// shared queue of packets has been updated
	while (seq != sp.nextSeq)								// while there are still packets left to be sent in the shared queue
	{
		this_thread::sleep_for(1000ms);						// thrad waits for 1 second
	}
	
	WaitForSingleObject(sp.mutx, INFINITE);					// lock mutex to avoid synchronization issues
	SetEvent(sp.eventQuit);									// signal eventQuit
	ReleaseMutex(sp.mutx);									// unlock mutex

	while (sndBase != seq)									// while there are still ACKs pending
	{
		this_thread::sleep_for(100ms);						// thread waits for 0.1 seconds
	}

	// stop worker & stats threads
	WaitForSingleObject(sp.mutx, INFINITE);					// lock mutex to avoid synchronization issues 
	SetEvent(sp.finished);									// signal event finished
	ReleaseMutex(sp.mutx);									// unlock mutex

	WaitForSingleObject(worker_thread, INFINITE);			// wait for worker thread
	CloseHandle(worker_thread);								// close worker thread
	WaitForSingleObject(stats_thread, INFINITE);			// wait for stats thread
	CloseHandle(stats_thread);								// close stats thread

	clock_t start;
	double elapsedTime;

	// Create FIN packet
	SenderSynHeader ssh;
	ssh.lp = sockLp;
	ssh.sdh.flags.FIN = 1;
	ssh.sdh.seq = seq;
	
	float rto = RTO;										// default starting RTO for fin packets = rto found during SYN phase

	int count = 0;
	while (count < MAX_ATTEMPTS) 
	{
		// attempt to send packet
		if (sendto(sock, (char*)&ssh, sizeof(ssh), 0, (struct sockaddr*)&remote, int(sizeof(remote))) == SOCKET_ERROR) 
		{
			return FAILED_SEND;
		}

		// Get ready to receive FIN-ACK
		fd_set fd;
		FD_ZERO(&fd);
		FD_SET(sock, &fd);

		timeval tp;
		tp.tv_sec = (long)rto;
		tp.tv_usec = (rto - tp.tv_sec) * 1e6;

		struct sockaddr_in response;
		int size = sizeof(response);
		int bytes = 0;										// number of bytes received
		char* recvData = new char[sizeof(ReceiverHeader)];

		int status = select(0, &fd, NULL, NULL, &tp);
		if (status > 0) {
			bytes = recvfrom(sock, recvData, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size);

			if (bytes == SOCKET_ERROR) 
			{
				return FAILED_RECV;
			}

			ReceiverHeader* rh = (ReceiverHeader*)recvData;

			if (rh->flags.FIN != 1 or rh->flags.ACK != 1)	// received packet was not what we expected (not FIN-ACK)
			{
				count += 1;
				continue;
			}

			elapsedTime = (double)(clock() - timeCreated) / CLOCKS_PER_SEC;
			printf("[%.2f] <-- FIN-ACK %d %X\n", elapsedTime, rh->ackSeq, rh->recvWnd);
			
			isOpen = false;									// connection is now closed 								
			*estimated_RTT = sp.RTT;						// update estRTT in shared stats parameters

			return STATUS_OK;

		}
		else if (status == SOCKET_ERROR) 
		{
			return UNKNOWN_ERROR;
		}
		else 
		{
			count += 1;										// loop again
		}
	}

	return TIMEOUT;
}

/* Send Function */
int SenderSocket::Send(char* buffer, int bytes)
{
	HANDLE arr[] = { sp.eventQuit, empty };
	WaitForMultipleObjects(2, arr, false, INFINITE);
	
	int slot = seq % winSize;
	
	clock_t cur_time;
	clock_t send_time;
	clock_t send_ack_time;

	// set up current data packet
	Packet currentPkt;
	currentPkt.sdh.seq = seq;
	memcpy(currentPkt.buf, buffer, bytes);

	// update the shared queues with current packet info
	pending_pkts[slot] = currentPkt;
	pktBytes[slot] = bytes;
	pktTxtime[slot] = clock();
	pktRtx[slot] = 0;
	
	// update sequence number
	seq++;

	ReleaseSemaphore(full, 1, NULL);
	return STATUS_OK;
}

/* Receive Data ACK Function */
int SenderSocket::ReceiveACK()
{
	struct sockaddr_in response;
	int size = sizeof(response);
	char* recvData = new char[sizeof(ReceiverHeader)];
	int bytes = recvfrom(sock, recvData, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size);

	double elapsedTime;
	clock_t sendTxtime;
	clock_t sendAcktime;

	if (bytes == SOCKET_ERROR) 
	{

		return FAILED_RECV;
	}

	ReceiverHeader* rh = (ReceiverHeader*)recvData;

	sendAcktime = clock();															// get end time point

	int y = rh->ackSeq;
	int oldBase = sndBase;
	
	if(y>sndBase && pktRtx[(y-1)%winSize]==1 && pktRtx[oldBase % winSize] == 1)		// base not rtx
	{
		// compute sample rtt
		sendTxtime = pktTxtime[(y - 1) % winSize];
		elapsedTime = (double)(sendAcktime - sendTxtime) / CLOCKS_PER_SEC;
		double sampleRTT = elapsedTime;
		
		WaitForSingleObject(sp.mutx, INFINITE);										// lock mutex to avoid synchronization issues
		sp.RTT = ((1 - ALPHA) * sp.RTT) + (ALPHA * sampleRTT);						// update estRTT in shared parameters
		sp.devRTT = ((1 - BETA) * sp.devRTT) + (BETA * abs(sampleRTT - sp.RTT));	// update devRTT in shared parameters
		ReleaseMutex(sp.mutx);														// unlock mutex

		RTO = sp.RTT + 4 * max(sp.devRTT, 0.010);									// update sender socket rto
	}
	if (y > sndBase)
	{
		int difference = sndBase - y;
		
		sndBase = y;																// update base
		dup = 0;																	// update number of dup ACKs

		int effectiveWin = min(winSize, rh->recvWnd);								// calculate effective window size
		int newReleased = sndBase + effectiveWin - lastReleased;					// value to advance semaphore by
		
		ReleaseSemaphore(empty, newReleased, NULL);
		lastReleased += newReleased;												// update last released

		//todo check if mutex needed
		WaitForSingleObject(sp.mutx, INFINITE);										// lock mutex to avoid synchronization issues
		sp.MB += (MAX_PKT_SIZE * 1.0) / 1e6;										// update MBs of data ACKed (in shared stats parameters)
		sp.B = sndBase;																// upate sender base (in shared stats parameters)
		sp.effWin = effectiveWin;													// update effective window (in shared stats parameters)
		ReleaseMutex(sp.mutx);														// unlock mutex
	}
	else																			// duplicate ACK received
	{
		dup++;																		// increase total number of duplicate ACKs
		if (dup == 3)																// handle fast rtx
		{
			int index = y % winSize;
			Packet currentPkt = pending_pkts[index];
			int pktSize = pktBytes[index];

			// attempt to send
			if (sendto(sock, (char*)&currentPkt, sizeof(SenderDataHeader) + pktSize, 0, (struct sockaddr*)&remote, int(sizeof(remote))) == SOCKET_ERROR) 
			{
				return FAILED_SEND;
			}

			pktRtx[index] += 1;														// update rtx attempts for this packet
			WaitForSingleObject(sp.mutx, INFINITE);									// lock mutex to avoid synchronization issues
			sp.F++;																	// update number of fast rtx
			ReleaseMutex(sp.mutx);													// unock mutex

		}
	}
	return STATUS_OK;
}