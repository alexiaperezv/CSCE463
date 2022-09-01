/*
* Header file includes macros & UDP comm classes
* CSCE 463-500, Spring 2022
* By Alexia Perez
*/

#pragma once

// for estRTT and devRTT calculations
#define ALPHA				0.125
#define BETA				0.25
#define SYN_MAX_ATTEMPTS	3		// max number of attempts for SYN packets
#define MAX_ATTEMPTS		50		// max number of attempts for all other packets

#define MAGIC_PORT 22345			// receiver listens on this port
#define MAX_PKT_SIZE (1500-28)		// max UDP packet size accepted by receiver

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK			0		// no error
#define ALREADY_CONNECTED	1		// 2nd call to ss.Open() without closing connection
#define NOT_CONNECTED		2		// call to ss.Send()/Close() wihtout ss.Open()
#define INVALID_NAME		3		// ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND			4		// sendto() failed in kernel
#define TIMEOUT				5		// timeout after all retx attempts are exhausted
#define FAILED_RECV			6		// recvfrom() failed in kernel
#define UNKNOWN_ERROR	   -1		// generic errors 
#define FAILED_BIND		   -2		// socket bind failed 

// SYN packets format
#define FORWARD_PATH		0
#define RETURN_PATH			1

// Flags Header
#define MAGIC_PROTOCOL	0x8311AA


#pragma pack(push,1)
class LinkProperties
{
public:
	// transfer parameters
	float RTT;						// propagation RTT (in sec)
	float speed;					// bottleneck bandwidth (in bits/sec)
	float pLoss[2];					// probability of loss in each direction
	DWORD bufferSize;				// buffer size of emulated routers (in packets)
	LinkProperties() { memset(this, 0, sizeof(*this)); }
};

class Flags
{
public:
	DWORD reserved : 5;				// must be zero
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
	DWORD seq;						// must begin with 0
};

class SenderSynHeader
{
public:
	SenderDataHeader sdh;
	LinkProperties lp;
	//void generateSyn(int senderWindow); // for SYN packets
	//void generateFin(int senderWindow); // for FIN packets
};

class ReceiverHeader
{
public:
	Flags flags;
	DWORD recvWnd;					// receiver window for flow control (in packets)
	DWORD ackSeq;					// ack value = next expected sequence
};


class Packet 
{
public:
	//int type;						// SYN, FIN, data
	//int size;						// bytes in packet data
	//clock_t txTime;				// transmission time
	SenderDataHeader sdh;			// header
	char buf[MAX_PKT_SIZE];			// packet with header
};

class Parameters {
public:

	//SenderSocket* ss;
	//char* charBuff;
	//int byteBufferSize;

	// Obvious shared stats parameters (from PDF instructions)
	int B = 0;						// Base
	float MB = 0.0;					// Data ACKed so far (in MBs)
	int nextSeq = 0;				// next sequence number
	int T = 0;						// packets with timeouts
	int F = 0;						// packets with fast rtx
	int effWin = 1;					// effective window size
	float speed = 0.0;				// speed at which the receiver consumes data
	float RTT = 0.0;				// estRTT
	float devRTT = 0.0;

	// Additional shared parameters 
	int timeExpire = 0;
	bool updateDone = false;
	HANDLE eventQuit, finished, mutx;

};
#pragma pack(pop)

class SenderSocket {
public:
	bool isOpen = false;			// keep track of whether socket is open/closed
	SOCKET sock; 
	struct sockaddr_in remote;
	struct sockaddr_in local; 
	clock_t timeCreated = clock();	// saved time when constructor was called
	
	int winSize, sndBase = 0, seq = 0, lastReleased = 0, dup = 0;
	float  RTO;
	Packet* pending_pkts;			// will be of size winSize
	Parameters sp;					// shared stats parameters (used by open & close functions)
	
	LinkProperties sockLp;			// link properties of sender socket

	// shared stats queues 
	int* pktBytes;
	int* pktRtx;
	clock_t* pktTxtime;

	// Event handles & threads 
	HANDLE stats_thread, worker_thread;
	HANDLE empty, full, socketReceiveReady;

	~SenderSocket();				// class destructor 
	int Open(char* targetHost, int rcvPort, int senWindow, LinkProperties* lp);
	int Close(float* estRTT);
	int Send(char* buffer, int bytes);
	int ReceiveACK();

};

DWORD WINAPI showStats(LPVOID lpParam);
DWORD WINAPI runWorker(LPVOID lpParam);