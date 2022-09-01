// main.cpp : This file contains the 'main' function. Program execution begins and ends there.
// CSCE 463-500, Spring 2022
// By Alexia Perez - 127008512

#include "pch.h"
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)

int main(int argc, char* argv[])
{
    if (argc != 3) // Handle incorrect arguments provided
    {
        printf("Error: incorrect number of arguments. Usage: hw2.exe <query> <DNS server>\n");
        exit(1);
    }

    // get user input
    char* lookup = argv[1];     // e.g www.xyz.com
    strncat(lookup, "\0", 1);   // add null char at the end
    char* server = argv[2];     // DNS server IP
    strncat(lookup, "\0", 1);   // add null char at the end

    // determine query type
    unsigned int type = getType(lookup); // 1 or 12
    printf("Lookup\t: %s\nQuery\t: %s, type %d, ", lookup, lookup, type); // 1st half of output summary: Lookup, query
    
    // initialize winsock
    startWinsock();

    // create UDP socket
    SOCKET sock = udpSock();
    
    // build packet to send   
    int pktSize = sizeof(FixedDNSheader) + sizeof(QueryHeader) + strlen(lookup) + 2;
    char* packet = new char[pktSize]; // allocate memory for UDP packet

    FixedDNSheader* fdh = (FixedDNSheader *)packet; // create fixed header object
    QueryHeader* qh = (QueryHeader*)(packet + pktSize - sizeof(QueryHeader)); // create query object
  
    // initialize header and query data + append null char at the end of packet
    fdh->setFixdHeadr();
    makeDNSQuestion((char *)(fdh + 1), lookup, type); // generate question from lookup string
    qh->setQueryHdr(type);
    
    // print query summary on screen
    printf("TXID 0x%.4x\nServer\t: %s\n********************************\n",  ntohs(fdh->ID), server); // 2nd half of output summary: query, server
    
    // send and receive DNS query/response
    doDNS(sock, server, packet, pktSize);

    // deallocate memory & exit program
    delete[] packet;
    closesocket(sock);
    WSACleanup();
    return 0;
}