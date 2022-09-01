# Resources Used for CSCE 463-500 Homework 4, Spring 2022

- Traceroute.cpp:
* startWinsock() function --> replicated my own code from previous assignments HW 1-3
* ip_checksum function --> Provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
* elapsedTime function --> based on code I previously wrote for past assignments HW 2-3
* reverseLookup thread function --> 
* dnsLookup function --> replicated my own code from previous assingments HW 1-3
* Traceroute Constructor --> Skeleton (for socket initialization) provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
* sendICMP function --> Skeleton provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
					--> sendto usage from my own code from previous assignments HW 1-3
* recvICMP function --> Skeleton provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
					--> recvfrom usage from my own code from previous assignments HW 1-3
					--> socketRecvEvent from examples & my own code from Homework assignments 3.2 and 3.3
					--> followed logic from CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions closely to check packet size, ID, and protocol (when data is received)
					--> thread functions (WaitForMultipleObjects, CloseHandle, etc.) from examples and my own code from Homework 3
* printTrace function --> attempted to replicate format of printouts from CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
* trace function --> followed logic/instructions from CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions as closely as possible (for the logical order of the functions called, syntax required no outside help as I am already familiar)

- Traceroute.h:
* ICMP Packet size and type definitions --> Provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
* Constant parameters --> Ideas directly drafted from CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
* Error codes --> I made these up myself to be able to output them in the main, not sure how useful this was
* IPHeader and ICMPHeader classes --> Provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions
* statParameters struct --> loosley based off of a similar structure/class that I wrote for Homework 3.2 to aid in passing multiple parameters to a thread function
* Traceroute Class --> Logical Skeleton provided in CSCE 463-500, Spring 2022 - Homework 4 PDF Instructions

- General Documentation:
** CSCE 463-500 Spring 2022 Lecture Slides (4-12-22)
** CSCE 463 Piazza examples: https://piazza.com/class/kyi07dr65i941q?cid=139
** Max size of DNS response: https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/DNSBehavior.html#MaxSize
** Thread functions: https://docs.microsoft.com/en-us/windows/win32/procthread/creating-threads
** High-resolution timer functions: https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency?redirectedfrom=MSDN
									https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter?redirectedfrom=MSDN
									http://stackoverflow.com/questions/1739259/how-to-use-queryperformancecounter
** ICMP Ping Example: https://tangentsoft.net/wskfaq/examples/dllping.html
** Winsock Ping with Raw Sockets: https://tangentsoft.net/wskfaq/examples/rawping.html
** Winsock - Getting local IP addresses: https://tangentsoft.net/wskfaq/examples/ipaddr.html
** Winsocj - Getting interface info: https://tangentsoft.net/wskfaq/examples/getifaces.html
http://stackoverflow.com/questions/5328070/how-to-convert-string-to-ip-address-and-vice-versa