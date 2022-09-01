/* 
* Header file for SenderSocket class
* CSCE 463-500, Spring 2022
* By Alexia Perez, 127008512
*/

#pragma once

#define SYN_MAX_ATTEMPTS	3	// max number of attempts for SYN packets
#define MAX_ATTEMPTS		5	// max number of attempts for all other packets

#define MAGIC_PORT 22345		// receiver listens on this port
#define MAX_PKT_SIZE (1500-28)	// max UDP packet size accepted by receiver

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK			0	// no error
#define ALREADY_CONNECTED	1	// 2nd call to ss.Open() without closing connection
#define NOT_CONNECTED		2	// call to ss.Send()/Close() wihtout ss.Open()
#define INVALID_NAME		3	// ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND			4	// sendto() failed in kernel
#define TIMEOUT				5	// timeout after all retx attempts are exhausted
#define FAILED_RECV			6	// recvfrom() failed in kernel

// SYN packets format
#define FORWARD_PATH		0
#define RETURN_PATH			1

// Flags Header
#define MAGIC_PROTOCOL	0x8311AA

#pragma pack(push, 1)
class LinkProperties 
{
public:
	// transfer parameters
	float RTT;			// propagation RTT (in sec)
	float speed;		// bottleneck bandwidth (in bits/sec)
	float pLoss[2];		// probability of loss in each direction
	DWORD bufferSize;	// buffer size of emulated routers (in packets)
	LinkProperties() { memset(this, 0, sizeof(*this)); }
};

class Flags
{
public: 
	DWORD reserved : 5;	// must be zero
	DWORD SYN : 1; 
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;
	Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};

class SenderDataHeader
{
public: 
	Flags flags;
	DWORD seq;	// must begin with 0
};

class SenderSynHeader
{
public:
	SenderDataHeader sdh;
	LinkProperties lp;
	void generateSyn(int senderWindow); // for SYN packets
	void generateFin(int senderWindow); // for FIN packets
};

class ReceiverHeader
{
public:
	Flags flags;
	DWORD recvWnd; // receiver window for flow control (in packets)
	DWORD ackSeq;  // ack value = next expected sequence
};
#pragma pack(pop)

class SenderSocket
{
public:
	bool isOpen = false; // keep track of whether socket is open/closed
	SOCKET sock;
	struct sockaddr_in remote;
	struct sockaddr_in local;
	clock_t timeCreated = clock(); // saved time when constructor was called

	int Open(char *targetHost, int port, int senderWindow, LinkProperties *lp);
	int Close(LinkProperties* lp, int senderWindow);
	// int Send(char *buffer, int bytes);
};

double getTotalTime(); // helper function - calculates total time in main