/*
* udpTL.cpp : This file contains the 'main' function. Program execution begins and ends there.
* CSCE 463-500, Spring 2022
* By Alexia Perez, 127008512
*/

#include "pch.h"
#pragma comment(lib, "ws2_32.lib")

UINT64 statsThread(LPVOID parameters)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    Parameters* p = (Parameters*)parameters;
    SenderSocket* ss = (SenderSocket*)p->ss;

    clock_t start = clock();
    clock_t now;
    clock_t prev = start;

    int prevBase = ss->base;
    int nextBase;
    while (!p->stop)
    {
        Sleep(2000);
        now = clock();
        nextBase = ss->base;
        printf("[ %3d] B\t%6d ( %3.1f MB) N\t%6d T %d F %d W %d S %.3f Mbps RTT %.3f\n", (now - start)/CLOCKS_PER_SEC, ss->base, (max(ss->base - 1, 0) * (MAX_PKT_SIZE - sizeof(SenderDataHeader)))/(double)1e6, ss->nextSeq, ss->totalTimeouts, ss->totalRets, ss->windowSize, (double)(nextBase - prevBase)*8*(MAX_PKT_SIZE - sizeof(SenderDataHeader))/(1e6 * ((double)(now - prev)/CLOCKS_PER_SEC)), ss->estRTT);
        prevBase = nextBase;
        prev = now;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 8)
    {
        printf("Invalid number of arguments.\nCorrect Usage: udpTL.exe <dest host> <power of 2 buffer size> <sender window in packets> <round-trip propagation delay in sec> <probability of loss forward dir> <probability of loss reverse dir> <speed of bottleneck in Mbps>\n");
        exit(1);
    }

    // 1. Parse command-line args:
    char* targetHost = argv[1];
    int power = atoi(argv[2]);              // command-line specified integer
    int senderWindow = atoi(argv[3]);       // command-line specified integer
    double speed = atof(argv[7]);           // to use in print summary
    
    // Perform "silent checks"
    if (speed <= 0.0 || (speed/1000) > 10.0)          // invalid link speed
    {
        exit(1);                            
    }
    if (atof(argv[4]) >= 30.0 || atof(argv[4]) < 0.0) // invalid RTT
    {
        exit(1);                            
    }
    if (atof(argv[5]) < 0.0 || atof(argv[5]) >= 1.0)  // invalid pLoss (forward)
    {
        exit(1);
    }
    if (atof(argv[6]) < 0.0 || atof(argv[6]) >= 1.0)  // invalid pLoss (reverse)
    {
        exit(1);
    }
    if (senderWindow < 1 || senderWindow > 1e6)       // invalid "router buffer size" (aka senderWindow)
    {
        exit(1);
    }

    LinkProperties lp;                      // link properties = args 4-7
    lp.RTT = atof(argv[4]);                 // rtt in seconds
    lp.speed = 1e6 * atof(argv[7]);         // convert to megabits
    lp.pLoss[FORWARD_PATH] = atof(argv[5]); // probability of loss (forward direction)
    lp.pLoss[RETURN_PATH] = atof(argv[6]);  // probability of loss (reverse direction)
    
    // Print summary 
    printf("Main:\tsender W = %d, RTT = %.3f sec, loss %g / %g, link %d Mbps\n", senderWindow, lp.RTT, lp.pLoss[FORWARD_PATH], lp.pLoss[RETURN_PATH], (int)speed);
    
    // 2. Create DWORD buffer:
    printf("Main:\tinitializing DWORD array with 2^%d elements... ", power);
    clock_t start;
    start = clock(); // start clock
    
    UINT64 dwordBuffSize = (UINT64) 1 << power;
    DWORD* dwordBuff = new DWORD[dwordBuffSize]; // user-requested buffer
    for (int i = 0; i < dwordBuffSize; i++)      // required initialization of buffer
    {
        dwordBuff[i] = i;
    }
    printf("done in %d ms\n", 1000 * (clock() - start) / CLOCKS_PER_SEC);

    // 3. Set up UDP sender socket - SYN & SYN-ACK
    SenderSocket ss = SenderSocket(); // instace of SenderSocket class
    ss.windowSize = senderWindow;

    int status;
    if ((status = ss.Open(targetHost, MAGIC_PORT, senderWindow, &lp)) != STATUS_OK)
    {
        printf("Main:\tconnect failed with status %d\n", status);
        exit(1);
    }
    printf("Main:\tconnected to %s in %.3f sec, packet size %d bytes\n", targetHost, lp.RTT, MAX_PKT_SIZE); // change print vals

    /* Send function will go here */
    char* charBuf = (char*)dwordBuff;
    UINT64 byteBufferSize = dwordBuffSize << 2;

    struct Parameters statsPars;
    statsPars.ss = &ss;
    statsPars.stop = false;

    HANDLE statsHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statsThread, &statsPars, 0, NULL);
    
    UINT64 off = 0;
    while (off < byteBufferSize)
    {
        int bytes = min(byteBufferSize - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
        if ((status = ss.Send(charBuf + off, bytes)) != STATUS_OK)
        {
            printf("send failed with status %d\n", status);
        }
        off += bytes;
    }
    double time = (clock() - ss.timeCreated) / (double)CLOCKS_PER_SEC;
    statsPars.stop = true;
    WaitForSingleObject(statsHandle, INFINITE);
    CloseHandle(statsHandle);

    // 4. Close UDP sender socket - FIN & FIN-ACK
    if ((status = ss.Close(&lp, senderWindow)) != STATUS_OK)
    {
        printf("Main:\tdisconnect failed with status %d\n", status);
        exit(1);
    }
    Checksum cs;
    DWORD check = cs.CRC32((unsigned char*)charBuf, byteBufferSize);
    printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum %X\n", time, (double)dwordBuffSize * 32 / (double)(1000*time), check);
    printf("Main:\testRTT %.3f, ideal rate %.2f Kbps\n", ss.estRTT, 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader))/(ss.estRTT * 1000));
    
    // 5. Cleanup and finish
    closesocket(ss.sock);
    WSACleanup();
    return 0;
}