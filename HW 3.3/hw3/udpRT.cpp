/*
* Main cpp file, program excution begins and ends here
* CSCE 463-500, Spring 2022
* By Alexia Perez
*/

#include "pch.h"
using namespace std;

/* "Helper" Function to start Winsock */
void startWinsock()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		exit(1);
	}
}

/* Main Program Execution Starts Here */
int main(int argc, char** argv)
{
	
	if (argc != 8)
	{
		printf("Invalid number of arguments.\nCorrect Usage: udpTL.exe <dest host> <power of 2 buffer size> <sender window in packets> <round-trip propagation delay in sec> <probability of loss forward dir> <probability of loss reverse dir> <speed of bottleneck in Mbps>\n");
		exit(1);
	}

	//Initialize WinSock; once per program run
	startWinsock();
	
	// 1. Parse command-line args:
	char* targetHost = argv[1];
	int power = atoi(argv[2]);							// command-line specified integer
	int senderWindow = atoi(argv[3]);					// command-line specified integer
	double speed = atof(argv[7]);						// to use in print summary

	// Perform "silent checks"
	if (speed <= 0.0 || (speed / 1000) > 10.0)			// invalid link speed
	{
		exit(1);
	}
	if (atof(argv[4]) >= 30.0 || atof(argv[4]) < 0.0)	// invalid RTT
	{
		exit(1);
	}
	if (atof(argv[5]) < 0.0 || atof(argv[5]) >= 1.0)	// invalid pLoss (forward)
	{
		exit(1);
	}
	if (atof(argv[6]) < 0.0 || atof(argv[6]) >= 1.0)	// invalid pLoss (reverse)
	{
		exit(1);
	}
	if (senderWindow < 1 || senderWindow > 1e6)			// invalid "router buffer size" (aka senderWindow)
	{
		exit(1);
	}

	LinkProperties lp;									// link properties = args 4-7
	lp.RTT = atof(argv[4]);								// rtt in seconds
	lp.speed = 1e6 * atof(argv[7]);						// convert to megabits
	lp.pLoss[FORWARD_PATH] = atof(argv[5]);				// probability of loss (forward direction)
	lp.pLoss[RETURN_PATH] = atof(argv[6]);				// probability of loss (reverse direction)

	// Print summary 
	printf("Main:\tsender W = %d, RTT = %.3f sec, loss %g / %g, link %d Mbps\n", senderWindow, lp.RTT, lp.pLoss[FORWARD_PATH], lp.pLoss[RETURN_PATH], (int)speed);

	// 2. Create DWORD buffer:
	printf("Main:\tinitializing DWORD array with 2^%d elements... ", power);
	clock_t start = clock();

	UINT64 dwordBuffSize = (UINT64)1 << power;
	DWORD* dwordBuff = new DWORD[dwordBuffSize];		// user-requested buffer
	for (int i = 0; i < dwordBuffSize; i++)				// required initialization of buffer
	{
		dwordBuff[i] = i;
	}
	printf("done in %d ms\n", 1000 * (clock() - start) / CLOCKS_PER_SEC);

	// 3. Set up UDP sender socket - SYN & SYN-ACK
	SenderSocket ss;									// instace of SenderSocket class

	// Create events before Open function
	ss.sp.mutx = CreateMutex(NULL, false, NULL);
	ss.sp.eventQuit = CreateEvent(NULL, true, false, NULL);
	ss.sp.finished = CreateEvent(NULL, true, false, NULL);
	
	// Create stats thread
	ss.stats_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)showStats, &ss.sp, 0, NULL);
	if (ss.stats_thread == NULL)
	{
		printf("Stats thread CreateThread failed with error %d\n", GetLastError());
		return -1;
	}

	int status;
	if ((status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK) {
		// error handling: print status and quit
		printf("Main:\t connect failed with status %d\n", status);
		delete dwordBuff;
		exit(0);
	}
	printf("Main:\tconnected to %s in %.3f sec, packet size %d bytes\n", targetHost, (double)(clock() - start) / CLOCKS_PER_SEC, MAX_PKT_SIZE);
	
	/* Send function will go here */
	start = clock();
	char* charBuf = (char*)dwordBuff;
	UINT64 byteBufferSize = dwordBuffSize << 2;
	UINT64 off = 0;										// current position in buffer
	while (off < byteBufferSize)
	{
		// decide the size of next chunk
		int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));

		// send chunk into socket 
		if ((status = ss.Send(charBuf + off, bytes)) != STATUS_OK) {
			//error handing: print status and quit
			printf("Main:\Send failed with status %d", status);
			delete dwordBuff;
			exit(1);
		}
		off += bytes;
	}

	double timeElapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
	float estRTT;

	// 4. Close UDP sender socket - FIN & FIN-ACK
	if ((status = ss.Close(&estRTT)) != STATUS_OK)
	{
		printf("Main:\tclose failed with status %d\n", status);
		exit(1);
	}
	/* End of new code - print statements should remain the same */

	Checksum cs;
	DWORD check = cs.CRC32((unsigned char*)charBuf, byteBufferSize);
	printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum %X\n", timeElapsed, (double)dwordBuffSize * 32 / (double)(1000 * timeElapsed), check);
	printf("Main:\testRTT %.3f, ideal rate %.2f Kbps\n", estRTT, senderWindow * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) / (estRTT * 1000));

	// 5. Cleanup and finish
	closesocket(ss.sock);
	WSACleanup();
	return 0;
}