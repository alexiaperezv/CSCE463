/*
* SenderSocket class & other helpful classes/functions defined here
* CSCE 463-500, Spring 2022
* By Alexia Perez, 127008512
*/

#include "pch.h"
#include "SenderSocket.h"

void SenderSocket::calculateStats(double prevRTT)
{
	double a = 0.125, b = 0.25;
	estRTT = (1 - a) * estRTT + a * prevRTT;
	devRTT = (1 - b) * devRTT + b * abs(prevRTT - estRTT);
	RTO = estRTT + 4 * max(devRTT, 0.010);
}
// Helper function to start Winsock
void startWinsock()
{
	WSADATA wsaData; // initialize Winsock
	int code = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (code != 0)
	{
		printf("WSAStartup failed with status %d\n", code);
		exit(1);
	}
}

void SenderSynHeader::generateSyn(int senderWindow)
{
	this->lp.bufferSize = 3 + senderWindow;
	this->sdh.flags.reserved = 0;
	this->sdh.flags.SYN = 1;
	this->sdh.flags.ACK = 0;
	this->sdh.flags.FIN = 0;
	this->sdh.flags.magic = MAGIC_PROTOCOL;
	this->sdh.seq = 0;
}

void SenderSynHeader::generateFin(int senderWindow) // same as SYN but SYN & FIN flags are flipped
{
	this->lp.bufferSize = 3 + senderWindow;
	this->sdh.flags.reserved = 0;
	this->sdh.flags.SYN = 0;
	this->sdh.flags.ACK = 0;
	this->sdh.flags.FIN = 1;
	this->sdh.flags.magic = MAGIC_PROTOCOL;
	this->sdh.seq = 0;
}

int SenderSocket::Open(char* targetHost, int port, int senderWindow, LinkProperties* lp)
{
	if (this->isOpen) // check if socket is already open
	{
		return ALREADY_CONNECTED;
	}

	// set up the socket
	startWinsock();
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0); // create socket
	if (sock == INVALID_SOCKET) // error handling for socket creation
	{
		//printf("WSA error %ld while creating socket\n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		exit(1);
	}

	// bind local socket
	memset(&(this->local), 0, sizeof(this->local));
	this->local.sin_family = AF_INET;
	this->local.sin_addr.s_addr = INADDR_ANY;
	this->local.sin_port = htons(0);
	if (bind(sock, (struct sockaddr*)&(this->local), sizeof(this->local)) == SOCKET_ERROR)
	{
		//printf("error %ld in socket bind()\n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		exit(1);
	}
	// set up remote socket
	memset(&(this->remote), 0, sizeof(this->remote));
	this->remote.sin_family = AF_INET;
	this->remote.sin_port = htons(port);

	DWORD IP = inet_addr(targetHost);
	if (IP == INADDR_NONE) // check if address is valid (by doing DNS lookup)
	{
		struct hostent* r;
		if ((r = gethostbyname(targetHost)) == NULL)
		{
			printf("target %s is invalid\n", (clock() - this->timeCreated) / (double)CLOCKS_PER_SEC, targetHost);
			closesocket(sock);
			WSACleanup();
			return INVALID_NAME;
		}
		else
		{
			memcpy((char*)&(this->remote.sin_addr), r->h_addr, r->h_length);
		}
	}
	else
	{
		this->remote.sin_addr.S_un.S_addr = inet_addr(targetHost);
	}

	// Generate SYN packet to send
	SenderSynHeader* synPacket = new SenderSynHeader();
	synPacket->lp.RTT = lp->RTT;
	synPacket->lp.speed = lp->speed;
	synPacket->lp.pLoss[0] = lp->pLoss[0];
	synPacket->lp.pLoss[1] = lp->pLoss[1];
	synPacket->generateSyn(senderWindow); // sets flags and buffer size

	// Set up - Send SYN
	int currAttempt = 0;
	//double fElapsed;
	double rto = max(1, 2*lp->RTT); // default starting value
	clock_t start;
	clock_t fStart = this->timeCreated; // set start time to time when constructor was called

	// Set up - Receive SYN-ACK
	double rtt;
	timeval tp;			// timeout value for select
	fd_set fd;			// file descriptor set

	while (currAttempt < SYN_MAX_ATTEMPTS)
	{
		//fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC; // calculate & print elapsed time since function started
		//printf("[ %.3f] --> SYN %d (attempt %d of 3, RTO %.3f) to %s\n", fElapsed, synPacket->sdh.seq, currAttempt + 1, rto, inet_ntoa(remote.sin_addr));

		// attempt to send
		start = clock();	// start timer for send function
		if (sendto(sock, (char*)synPacket, sizeof(SenderSynHeader), 0, (sockaddr*)&this->remote, sizeof(this->remote)) == SOCKET_ERROR)
		{
			printf("failed sendto with %d\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			return FAILED_SEND;
		}
		
		// reset select each time
		tp.tv_sec = floor(rto);
		tp.tv_usec = 1e6 * (rto - tp.tv_sec);
		FD_ZERO(&fd);       // clear the set
		FD_SET(sock, &fd);  // add socket to the set

		int sel = select(0, &fd, NULL, NULL, &tp);
		if (sel == SOCKET_ERROR)
		{
			printf("error %ld in select()\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			exit(1);
		}
		else if (sel == 0)
		{
			if (currAttempt == 2)
			{
				closesocket(sock);
				WSACleanup();
				return TIMEOUT;
			}

			currAttempt += 1; // next attempt
			totalRets += 1;
			totalTimeouts += 1;
			continue;
		}
		else if (sel > 0)
		{
			// attempt recv
			ReceiverHeader rh;
			struct sockaddr_in response;
			int bytes;
			int size = sizeof(response);

			if ((bytes = recvfrom(sock, (char*)&rh, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size)) == SOCKET_ERROR)
			{
				//fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC;
				printf("failed recvfrom with %ld\n", WSAGetLastError());
				closesocket(sock);
				WSACleanup();
				return FAILED_RECV;
			}
			//fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC; // calculate & print elapsed time since function started
			rtt = (clock() - start) / (double)CLOCKS_PER_SEC; // calculate rtt
			if (currAttempt == 0)
			{
				estRTT = rtt;
				this->RTO = estRTT + 4 * 0.01;
			}
			//rto = rtt * 3; // new rto is rtt of connection phase x 3
			//printf("[% .3f] <-- SYN-ACK %d window %d; setting initial RTO to %.3f\n", fElapsed, rh.ackSeq, rh.recvWnd, rto);
			break;
		}
	}
	lp->RTT = rtt;
	//RTO = rto;
	this->isOpen = true; // set variable to true after opening connection successfully
	this->sock = sock;
	return STATUS_OK;
}

int SenderSocket::Close(LinkProperties *lp, int senderWindow)
{
	// Generate FIN packet to send
	SenderSynHeader* finPacket = new SenderSynHeader();
	finPacket->lp.RTT = lp->RTT;
	finPacket->lp.speed = lp->speed * 1e6; // convert to Mb
	finPacket->lp.pLoss[0] = lp->pLoss[0];
	finPacket->lp.pLoss[1] = lp->pLoss[1];
	finPacket->generateFin(senderWindow);  // sets flags and buffer size
	finPacket->sdh.seq = nextSeq - 1;

	// Send FIN
	int currAttempt = 0;
	double fElapsed;
	double rto = max(1, 2 * lp->RTT); // last known RTO
	clock_t start;
	clock_t fStart = this->timeCreated; // set start time to time when constructor was called

	// Receive FIN-ACK
	double rtt;
	timeval tp; // timeout value for select
	fd_set fd;
	
	while (currAttempt < MAX_ATTEMPTS)
	{
		//fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC; // calculate & print elapsed time since creation of SenderSocket
		//printf("[ %.3f] --> FIN %d (attempt %d of 5, RTO %.3f) to %s\n", fElapsed, finPacket->sdh.seq, currAttempt + 1, rto, inet_ntoa(remote.sin_addr));
		// attempt to send
		start = clock();	// start timer for send function
		if (sendto(sock, (char*)finPacket, sizeof(SenderSynHeader), 0, (sockaddr*)&this->remote, sizeof(this->remote)) == SOCKET_ERROR)
		{
			printf("failed sendto with %d\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			return FAILED_SEND;
		}

		// reset select each time
		timeval tp; // timeout value for select
		fd_set fd;
		tp.tv_sec = floor(rto);
		tp.tv_usec = 1e6 * (rto - tp.tv_sec);
		FD_ZERO(&fd);       // clear the set
		FD_SET(sock, &fd);  // add socket to the set

		int sel = select(0, &fd, NULL, NULL, &tp);
		if (sel == SOCKET_ERROR)
		{
			printf("error %ld in select()\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			exit(1);
		}
		else if (sel == 0)
		{
			if (currAttempt == 4)
			{
				closesocket(sock);
				WSACleanup();
				return TIMEOUT;
			}
			currAttempt += 1;	// try again
			totalRets += 1;
			totalTimeouts += 1;
			continue;
		}
		else if (sel > 0)
		{
			// attempt to receive
			ReceiverHeader rh;
			struct sockaddr_in response;
			int bytes;
			int size = sizeof(response);

			if ((bytes = recvfrom(sock, (char*)&rh, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size)) == SOCKET_ERROR)
			{
				//fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC;
				//printf("[ %.3f] <-- failed recvfrom with %ld\n", fElapsed, WSAGetLastError());
				closesocket(sock);
				WSACleanup();
				return FAILED_RECV;
			}
			fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC; // calculate & print elapsed time since function started
			printf("[ %.3f] <-- FIN-ACK %d window %X\n", fElapsed, rh.ackSeq, rh.recvWnd);
			break; // attempt was successful, no need to loop again
		}
	}
	this->isOpen = false; // set variable to false after closing connection successfully
	return STATUS_OK;
}

int SenderSocket::Send(char* buffer, int bytes)
{
	// Set up the data packet to send out
	Packet* pkt = new Packet();
	pkt->sdh.seq = base;
	memcpy(pkt->data, buffer, bytes);

	// Send data packet 
	int currAttempt = 0;
	double fElapsed;
	double rto = RTO; // last known RTO
	clock_t start = clock();
	clock_t fStart = this->timeCreated; // set start time to time when constructor was called

	// Receive data ACK
	double rtt;
	timeval tp; // timeout value for select
	fd_set fd;

	while (currAttempt < MAX_ATTEMPTS)
	{
		if (sendto(sock, (char*)pkt, sizeof(SenderDataHeader) + strlen(pkt->data), 0, (sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
		{
			printf("failed sendto with %d\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			return FAILED_SEND;
		}

		// reset select each time
		tp.tv_sec = floor(rto);
		tp.tv_usec = 1e6 * (rto - tp.tv_sec);
		FD_ZERO(&fd);       // clear the set
		FD_SET(sock, &fd);  // add socket to the set

		int sel = select(0, &fd, NULL, NULL, &tp);
		if (sel == SOCKET_ERROR)
		{
			printf("error %ld in select()\n", WSAGetLastError());
			closesocket(sock);
			WSACleanup();
			exit(1);
		}
		else if (sel == 0)
		{
			if (currAttempt == 4)
			{
				closesocket(sock);
				WSACleanup();
				return TIMEOUT;
			}
			totalTimeouts += 1;
			totalRets += 1;
			currAttempt += 1;
			continue;
		}
		else if (sel > 0)
		{
			// attempt to receive
			ReceiverHeader rh;
			struct sockaddr_in response;
			int bytes;
			int size = sizeof(response);

			if ((bytes = recvfrom(sock, (char*)&rh, sizeof(ReceiverHeader), 0, (struct sockaddr*)&response, &size)) == SOCKET_ERROR)
			{
				fElapsed = (clock() - fStart) / (double)CLOCKS_PER_SEC;
				closesocket(sock);
				WSACleanup();
				return FAILED_RECV;
			}
			rtt = (clock() - start) / (double)CLOCKS_PER_SEC; // calculate rtt
			calculateStats(rtt);
			
			if (rh.ackSeq == nextSeq)
			{
				windowSize = min(windowSize, rh.recvWnd);
				base += 1;
				nextSeq += 1;
				ACKbytes += MAX_PKT_SIZE;
			}
			RTO = estRTT + 4 * max(0.01, devRTT);
			break; // attempt was successful, no need to loop again
		}
	}
	return STATUS_OK;
}