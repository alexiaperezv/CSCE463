// traceroute.cpp : This file contains the 'main' function. Program execution begins and ends there.
// CSCE 463-500 Homework 4, Spring 2022
// By: Alexia Perez, 127008512

#include "pch.h"


int main(int argc, char* argv[])
{
	// 1. Check number of args provided by user
	if (argc != 2)
	{
		printf("Invalid number of arguments (%d) provided. Correct Usage: <traceroute.exe> <www.example.com>\n", argc);
		exit(1);
	}

	// 2. Store target url in variable
	const char* host = argv[1];
	printf("Tracerouting to %s\n", host);
	
	// 3. Start winsock (use helper function)
	startWinsock();

	// 4. Create parallel traceroute object and start tracing target host
	Traceroute t;					// create traceroute class object 
	int status = t.trace(host);		// trace the target host
	if (status != 0)				// error handling for trace function
	{
		printf("Parallel traceroute failed with %d\n", GetLastError());
		return -1;
	}

	// 5. Cleanup & Exit Program
	WSACleanup();
	return 0;
}