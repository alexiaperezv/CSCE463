Resources Used for Homework 3.1:
- Skeleton Code & Logic from --> CSCE 463-500, Spring 2022 HW 3.1 PDF
- Set decimal precision in printf C/C++ --> http://mathcenter.oxford.emory.edu/site/cs170/printf/
- Measuring elapsed time for a function in C++ --> https://www.geeksforgeeks.org/measure-execution-time-with-high-precision-in-c-c/
- Winsock initializer function --> heavily based on code from file "dnsCLient.cpp" in my HW 2 submission
- Create UDP socket --> heavily based on code from file "socket.cpp" in my HW 1.2 submission
- DNS lookup to check if IP is valid --> heavily based on code from file "socket.cpp" in my HW 1.2 submission
- Send SYN packet --> heavily based on code from file "dnsClient.cpp" in my HW 2 submission
- Select function for receiving data --> heavily based on code from file "dnsClient.cpp" in my HW 2 submission
- Resetting timer for select (to use in while-loop) --> https://stackoverflow.com/questions/21743231/winsock-selects-timeout-on-listening-socket-causing-every-subsequent-select-c
- Receiver Header --> CSCE 463-500, Spring 2022 HW 3.1 PDF
- recvfrom function --> heavily based on code from file "dnsClient.cpp" in my HW 2 submission
- rtt calculation --> CSCE 463-500, Spring 2022 HW 3.1 PDF
- SenderSocket Close() function --> code is mostly copied & pasted from my SenderSocket::Open(...) function

**NOTE: the logic for most of the Open and Close functions is (almost) the same as logic in the files (my own) where the code was taken from.**

Resources Used for Homework 3.2 and Homework 3.3:
- Checksum class --> CSCE 463-500, Spring 2022 HW 3.2 PDF
- Packet class Skeleton --> CSCE 463-500, Spring 2022 HW 3.3 PDF
- Open function (changed slighly from HW 3.2 to HW 3.3) --> new code logic followed step-by-step "program structure" instructions from CSCE 463-500, Spring 2022 HW 3.3 PDF
- Send function Skeleton --> CSCE 463-500, Spring 2022 HW 3.3 PDF
- Close function (changed from HW 3.2 to HW 3.3) --> added checks for graceful thread termination https://docs.microsoft.com/en-us/windows/win32/procthread/terminating-a-thread
- Worker Thread Function --> https://docs.microsoft.com/en-us/windows/win32/procthread/creating-threads
						 --> Skeleton from CSCE 463-500, Spring 2022 HW 3.3 PDF
						 --> while-loop from CSCE 463-500, Spring 2022 HW 3.3 PDF
						 --> kernel re-sizing from CSCE 463-500, Spring 2022 HW 3.3 PDF
- Stats Thread Function --> Based on my previously written code for HW 3.2, with slight modifications to account for new SenderSocket parameters etc.
- New SenderSocket class members --> named afer or similarly to equivalent parameters in CSCE 463-500, Spring 2022 HW 3.3 PDF examples
- Synchronization General --> https://docs.microsoft.com/en-us/windows/win32/sync/synchronization
- Event handles --> https://docs.microsoft.com/en-us/windows/win32/sync/using-event-objects
- Semaphores --> https://docs.microsoft.com/en-us/windows/win32/sync/using-semaphore-objects
- Mutex --> https://docs.microsoft.com/en-us/windows/win32/sync/using-mutex-objects
- RTT sample calculation --> from CSCE 463-500, Spring 2022 HW 3.3 PDF
- Timer reset value when window moves forward --> from CSCE 463-500, Spring 2022 HW 3.3 PDF
- Alpha & Beta values --> from CSCE 463-500, Spring 2022 HW 3.3 PDF

Other resources (general code help, not specific to a singluar function)
- https://stackoverflow.com/questions/32234348/winsock-deprecated-no-warnings
- https://www.codeproject.com/script/Articles/ViewDownloads.aspx?aid=11046
- https://www.codeproject.com/Articles/5317505/QUDP-A-Reliable-UDP-Protocol
- https://en.wikipedia.org/wiki/UDP-based_Data_Transfer_Protocol
- https://www.youtube.com/watch?v=uIanSvWou1M