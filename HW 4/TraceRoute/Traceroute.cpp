/* This CPP file contains the definitions of all the functions declared in the Traceroute.h header file
* CSCE 463-500 Homework 4, Spring 2022
* By Alexia Perez, 127008512
*/

#include "pch.h"

/* Global Variable: saves the hop number of the destination address */
int destHop = MAX_HOPS + 1;


/* Utility Functions */
void startWinsock()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		exit(1);
	}
}

u_short ip_checksum(u_short* buffer, int size)
{
	u_long cksum = 0;

	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(u_short);
	}

	if (size)cksum += *(u_char*)buffer;

	cksum = (cksum >> 16) + (cksum & 0xffff);

	return (u_short)(~cksum);
}

double elapsedTime(clock_t start, clock_t end)
{
	double total = 1000 * (end - start) / CLOCKS_PER_SEC; // output is in ms
	return total;
}


/* Reverse Lookup Thread Function */
DWORD WINAPI reverseLookup(LPVOID lpParam)
{
	// 1. Cast function parameters as their original type (statParameters)
	statParameters* traceStats = (statParameters*)lpParam;
	
	// 2. Save the IP address of the server that sent response
	in_addr addr;
	addr.S_un.S_addr = traceStats->IP;

	// 3. Convert IP address into a char array (string)
	char* ip_ntoa = inet_ntoa(addr);
	
	// 4. Set up struct & save IP address info 
	struct addrinfo* res = 0;
	int dnsStatus = getaddrinfo(ip_ntoa, 0, 0, &res);
	
	// 5. Get response server's address "name" (domain) and store in char array
	char host[512];
	dnsStatus = getnameinfo(res->ai_addr, res->ai_addrlen, host, 512, 0, 0, 0);
	
	// 7. Error handling in case no domain was found - save domain name in traceStats
	if (strcmp(host, ip_ntoa) == 0)
	{
		memcpy(traceStats->resDomain, "<no DNS entry>", 15);
	}
	else
	{
		memcpy(traceStats->resDomain, host, 512);
	}

	return 0;
}


/* Traceroute Class Functions */
int Traceroute::dnsLookup(const char* targetHost)
{
	// 1. Get IP address from host 
	remote.sin_family = AF_INET;
	DWORD IP = inet_addr(targetHost);
	
	// 2. If IP address is not a valid one, do DNS lookup
	if (IP == INADDR_NONE)
	{
		struct hostent* server;

		// 2.1 If DNS lookup doesn't work, target host is invalid
		if ((server = gethostbyname(targetHost)) == NULL) 
		{
			printf("host does not exists %s\n", targetHost);
			return dnsLookup_error;
		}
		// 2.2 Otherwise, once we find target host address, save it
		else memcpy((char*)&(remote.sin_addr), server->h_addr, server->h_length);
	}
	// 3. If IP address is valid, we don't need to look anything up, just save it
	else
	{
		remote.sin_addr.S_un.S_addr = IP;
	}

	return 0;
}

Traceroute::Traceroute() // Constructor
{
	// Initialize ICMP socket to be used in traceroute
	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock == INVALID_SOCKET)
	{
		printf("socket initalization failed\n");
		closesocket(sock);
		WSACleanup();
		exit(socket_error);
	}
}

int Traceroute::sendICMP(int ttl)
{
	// 1. Create buffer for ICMP packet
	u_char send_buf[MAX_ICMP_SIZE];
	ICMPHeader *icmp = (ICMPHeader *)send_buf;

	// 2. Set up ICMP echo request, no need to flip byte order since fields are 1 byte each
	icmp->type = ICMP_ECHO_REQUEST;
	icmp->code = 0;
	icmp->id = (u_short)GetCurrentProcessId();
	icmp->seq = ttl;
	icmp->checksum = 0;

	// 3. Calculate checksum
	int packet_size = sizeof(ICMPHeader);
	icmp->checksum = ip_checksum((u_short *)send_buf, packet_size);

	// 4. Need Ws2tcpip.h for IP_TTL, which = 4
	if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char *)&ttl, sizeof(ttl)) == SOCKET_ERROR)
	{
		printf("setsockopt failed with %d\n", WSAGetLastError());
		return sendICMP_error;
	}

	// 5. Use regular sendto function 
	if (sendto(sock, (char *)icmp, packet_size, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
	{

		printf("sendto failed with %d\n", WSAGetLastError());
		return sendICMP_error;
	}

	// 6. Update stats parameters for trace
	trStats[ttl].totalProbes += 1;								// increase number of attempts
	QueryPerformanceCounter(&trStats[ttl].timeSent);			// udpdate packet send timestamp
	trStats[ttl].ttl = ttl;										// update time to live

	return 0;
}

int Traceroute::recvICMP()
{
	// 1. Set up recv buffer etc.
	u_char rec_buf[MAX_REPLY_SIZE]; /* this buffer starts with an IP header */
	IPHeader* router_ip_hdr = (IPHeader*)rec_buf;
	ICMPHeader* router_icmp_hdr = (ICMPHeader*)(router_ip_hdr + 1);
	IPHeader* orig_ip_hdr = (IPHeader*)(router_icmp_hdr + 1);
	ICMPHeader* orig_icmp_hdr = (ICMPHeader*)(orig_ip_hdr + 1);

	struct sockaddr_in response;
	int size = sizeof(response);
	int bytes;

	bool echoRecv = false;
	bool finish = false;
	int totalThreads = 0;

	// 2. Socket read event (will return whether data was received or not), similar to select function 
	WSAEVENT socketRecvEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (WSAEventSelect(sock, socketRecvEvent, FD_READ) == SOCKET_ERROR)
	{
		printf("WSAEventSelect failed with %d\n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		return recvICMP_error;
	}

	// 3. Set timer expiration and loop
	clock_t timerExpire = clock() + TIMEOUT;
	while (!finish)
	{
		// A. Calculate timeout
		DWORD timeout = (DWORD)(timerExpire - clock());
		
		// B. Wait until socket receives data or timeout, then enter switch case
		int ret = WaitForSingleObject(socketRecvEvent, timeout);
		switch (ret)
		{
		case WAIT_OBJECT_0:	// socket received data
		{
			// C. attempt to receive data from socket
			bytes = recvfrom(sock, (char*)&rec_buf, MAX_REPLY_SIZE, 0, (struct sockaddr*)&response, &size);
			if (bytes == SOCKET_ERROR)
			{
				printf("Error while recv from socket. %d\n", WSAGetLastError());
				return recvICMP_error;
			}
			
			// D. Calculate new ttl and check packet size, check protocol and ID for TTL expired/Echo Reply & update trace stats
			int ttl;
			LARGE_INTEGER endTime, totalTime;
			if (bytes > 55 && orig_ip_hdr->proto == IPPROTO_ICMP && orig_icmp_hdr->id == (u_short)GetCurrentProcessId())
			{
				ttl = orig_icmp_hdr->seq;
				QueryPerformanceCounter(&endTime);
				totalTime.QuadPart = (1e6*(endTime.QuadPart - trStats[ttl].timeSent.QuadPart))/frequency.QuadPart;
				
				trStats[ttl].RTT = totalTime.QuadPart / 1e3;
				trStats[ttl].IP = (router_ip_hdr->source_ip);
				
				if (router_icmp_hdr->type == ICMP_TTL_EXPIRED && router_icmp_hdr->code == 0 && !trStats[ttl].pktRecv)
				{
					trStats[ttl].pktType = ICMP_TTL_EXPIRED;
					trStats[ttl].pktCode = 0;
				}
				else
				{
					if (!trStats[ttl].pktRecv)
					{
						trStats[ttl].pktType = router_icmp_hdr->type;
						trStats[ttl].pktCode = router_icmp_hdr->code;
					}
				}

				traceThreads[totalThreads++] = CreateThread(NULL, 0, reverseLookup, &trStats[ttl], 0, NULL);
				trStats[ttl].pktRecv = true;
			}
			if (bytes > 27 && router_ip_hdr->proto == IPPROTO_ICMP && router_icmp_hdr->id == (u_short)GetCurrentProcessId())
			{
				ttl = router_icmp_hdr->seq;
				QueryPerformanceCounter(&endTime);
				totalTime.QuadPart = (1e6 * (endTime.QuadPart - trStats[ttl].timeSent.QuadPart)) / frequency.QuadPart;

				trStats[ttl].RTT = totalTime.QuadPart / 1e3;
				trStats[ttl].IP = (router_ip_hdr->source_ip);
				trStats[ttl].isEcho = true;
				traceThreads[totalThreads++] = CreateThread(NULL, 0, reverseLookup, &trStats[ttl], 0, NULL);

				if (router_icmp_hdr->type == ICMP_ECHO_REPLY && router_icmp_hdr->code == 0 && !echoRecv)
				{
					trStats[ttl].pktType = ICMP_ECHO_REPLY;
					trStats[ttl].pktCode = 0;
				}
				else
				{
					if (!echoRecv)
					{
						trStats[ttl].pktType = router_icmp_hdr->type;
						trStats[ttl].pktCode = router_icmp_hdr->code;
					}
				}
				trStats[ttl].pktRecv = true;
				echoRecv = true;
				destHop = ttl;
			}
		}
		break;
 
		case WAIT_TIMEOUT:	// socket timed out
		{
			// E. Calculate updated RTO and re-transmit
			bool retransmit= false;
			DWORD maxRTO = 0;

			for (int i = 1; i < destHop; i++)
			{
				DWORD estRTO = TIMEOUT;
				if (!trStats[i].pktRecv && trStats[i].totalProbes < 3)
				{
					if (i > 1 && trStats[i - 1].pktRecv)
					{
						estRTO = (DWORD)trStats[i - 1].RTT * 2;
					}

					if (i < MAX_HOPS && trStats[i + 1].pktRecv)
					{
						if (estRTO == TIMEOUT)
						{
							estRTO = (DWORD)trStats[i + 1].RTT * 2;
						}
						else
						{
							estRTO = estRTO + (DWORD)trStats[i + 1].RTT * 2;
							estRTO = estRTO / 2;
						}
					}
					maxRTO = max(maxRTO, estRTO);
					sendICMP(i);
					retransmit = true;
				}
			}
			
			// F. If we already rtx the max amount of times, finish
			if (!retransmit)
			{
				finish = true;
			}

			// G. Update timer expiration time
			else
			{
				timerExpire = clock() + maxRTO;
			}
		}
		break;

		default:			// unknown error 
			break;
		}
	}

	// 4. Wait for all threads to finish
	WaitForMultipleObjects(totalThreads, traceThreads, TRUE, INFINITE);

	// 5. Close threads
	for (int i = 0; i < totalThreads; i++)
	{
		CloseHandle(traceThreads[i]);
	}

	return 0;
}

void Traceroute::printTrace()
{
	for (int i = 1; i <= MAX_HOPS; i++)
	{
		// 1. Print the TTL of the trace
		printf("%d ", trStats[i].ttl);

		// 2. If we reach max attempts, print asterisk
		if (trStats[i].totalProbes > 2)
		{
			printf("*\n");
			continue;
		}

		// 3. Print the domain name (URL) of the remote server 
		printf("%s ", trStats[i].resDomain);

		// 4. Print the IP address of the remote server
		in_addr addr;
		addr.S_un.S_addr = trStats[i].IP;
		char* ip_ntoa = inet_ntoa(addr);
		printf("(%s) ", ip_ntoa);

		// 5. Print rtt and total attempts for this trace
		printf(" %.3f ms (%d) ", trStats[i].RTT, trStats[i].totalProbes);
		
		// 6. Print new line and repeat until echo reply or end of loop
		printf("\n");
		if (trStats[i].isEcho)
		{
			break;
		}
	}
}

int Traceroute::trace(const char* host) // Performs TraceRoute
{
	// 1. Initialize hihg-precision timer
	if (!QueryPerformanceFrequency(&frequency))
	{
		printf("QueryPerformanceFrequency failed\n");
		closesocket(sock);
		WSACleanup();
		return traceroute_error;
	}

	// 2. Do DNS lookup 
	bool targetFile = false;
	if (dnsLookup(host) != 0)
	{
		printf("Checking if input is a file...\n");
		// check if targetHost is actually a file here 
		targetFile = true;
	}

	// 3. Send ICMP packets
	clock_t startTime = clock();
	for (int i = 0; i < MAX_HOPS; i++)
	{
		if (sendICMP(i) < 0)
		{
			closesocket(sock);
			return sendICMP_error;
		}
	}

	// 4. Receive ICMP replies
	int ret = recvICMP();
	if (ret < 0)
	{
		closesocket(sock);
		return recvICMP_error;
	}
	clock_t endTime = clock();

	// 5. Print trace stats and execution time, then return to main function
	printTrace();
	printf("\nTotal execution time: %d ms\n\n", (int)elapsedTime(startTime, endTime));

	return 0;
}