/* This header file contains all the classes and functions needed for the main fuction to work properly
* CSCE 463-500 Homework 4, Spring 2022
* By Alexia Perez, 127008512
*/

#include "pch.h"
#pragma once

/* ICMP Packet Size Definitions */
#define IP_HDR_SIZE			20														// RFC 791 
#define ICMP_HDR_SIZE		8														// RFC 792 
#define MAX_SIZE			65200													// max payload size of an ICMP message originated in the program 
#define MAX_ICMP_SIZE (MAX_SIZE + ICMP_HDR_SIZE)									// max size of an ICMP datagram
#define MAX_REPLY_SIZE (IP_HDR_SIZE + ICMP_HDR_SIZE + MAX_ICMP_SIZE)				// ICMP Reply messages will most likely include only 8 bytes of the original message + the IP header (as per RFC 792

/* ICMP packet types */
#define ICMP_ECHO_REPLY		0
#define ICMP_DEST_UNREACH	3
#define ICMP_TTL_EXPIRED	11
#define ICMP_ECHO_REQUEST	8

/* Some Constant Parameters */
#define MAX_HOPS			30														//	Max. number of reachable hops by traceroute
#define MAX_PROBES			3														//  Max. number of probes (rtx) per hop
#define TIMEOUT				500														//	Default timeout value for rtx

/* Error codes to output in main function */
#define socket_error		1														// Socket initialization failed
#define dnsLookup_error		2														// DNS Lookup function failed
#define sendICMP_error		3														// Send ICMP function failed
#define recvICMP_error		4														// Receive ICMP function failed
#define traceroute_error	5														// Misc. error in trace function 

// Remember the current packing state
#pragma pack (push)
#pragma pack (1)

class IPHeader
{
public:
	u_char h_len : 4;																// lower 4 bits: length of the header in dwords 
	u_char version : 4;																// upper 4 bits: version of IP, i.e., 4 
	u_char tos;																		// type of service (TOS), ignore 
	u_short len;																	// length of packet 
	u_short ident;																	// unique identifier 
	u_short flags;																	// flags together with fragment offset - 16 bits 
	u_char ttl;																		// time to live 
	u_char proto;																	// protocol number (6=TCP, 17=UDP, etc.) 
	u_short checksum;																// IP header checksum 
	u_long source_ip;
	u_long dest_ip;
};

class ICMPHeader
{
public:
	u_char type;																	// ICMP packet type 
	u_char code;																	// type subcode 
	u_short checksum;																// checksum of the ICMP 
	u_short id;																		// application-specific ID 
	u_short seq;																	// application-specific sequence 
};

// Restore previous packing state
#pragma pack (pop)

/* Utility Functions (not in Traceroute Class) */
void startWinsock();										// function to initialize Winsock
double elapsedTime(clock_t start, clock_t end);				// function calculates elapsed time
u_short ip_checksum(u_short* buffer, int size);				// function computes internet checksum. No errors possible
DWORD WINAPI reverseLookup(LPVOID lpParam);			// thread function for reverse DNS lookups 

/* Traceroute Stat Parameters to output */
struct statParameters // Required ICMP parameters to display after Traceroute.
{
	int ttl = 0;								// time to live set for packet
	int totalProbes = 0;						// Num. of attempts (max = 3)
	int pktCode, pktType;						// ICMP code and packet type for error handling
	double RTT = 0.0;							// rtt of ICMP query
	bool isEcho = false;						// true when packet is echo probe
	bool pktRecv = false;						// true when packet was received - for duplicate responses
	char resDomain[512] = { 0 };				// domain of server/router sending response - max size = 512
	u_long IP;									// ip address of server/router that sent response
	LARGE_INTEGER timeSent;						// timestamp of sent probe
	
};

/* Traceroute Class */
class Traceroute
{
private:
	SOCKET sock;								// ICMP socket
	struct sockaddr_in remote;					// address of remote server (destination)
	
	statParameters trStats[MAX_HOPS];			// shared array of statParamters for this traceroute
	LARGE_INTEGER frequency;					// CPU frequency needed to calculate high-resolution timestamps 
	HANDLE traceThreads[MAX_HOPS];				// array of threads to receive replies in parallel

	int dnsLookup(const char* targetHost);		// performs dns lookup for given target host
	int sendICMP(int ttl);						// sends ICMP packet
	int recvICMP();								// received ICMP response 
	void printTrace();							// prints traceroute stats 

public:
	Traceroute();								// class constructor
	int trace(const char* targetHost);			// parallel traceroute function
};