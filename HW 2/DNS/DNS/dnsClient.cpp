#include "pch.h"
#include "dnsClient.h"
#pragma warning(disable : 4996)
using namespace std;

void FixedDNSheader::setFixdHeadr()
{
    this->ID = htons(1);
    this->flags = htons(DNS_QUERY | DNS_RD | DNS_STDQUERY);
    this->questions = htons(1);
    this->answers = 0;
    this->authRR = 0;
    this->additionalRR = 0;
}

void FixedDNSheader::getRHdr()
{
    this->ID = ntohs(this->ID);
    this->flags = ntohs(this->flags);
    this->questions = ntohs(this->questions);
    this->answers = ntohs(this->answers);
    this->authRR = ntohs(this->authRR);
    this->additionalRR = ntohs(this->additionalRR);
    printf("  TXID 0x%.4x, flags 0x%.4x, questions %d, answers %d, authority %d, additional %d\n", this->ID, this->flags, this->questions, this->answers, this->authRR, this->additionalRR);
}

void QueryHeader::setQueryHdr(int type)
{
    if (type == 1)
    {
        this->qType = htons(1);
    }
    else
    {
        this->qType = htons(12);
    }
    this->qClass = htons(1);
}

int getType(char* lookupStr) // get query type (A or Ptr)
{
    DWORD IP = inet_addr(lookupStr);
    if (IP == INADDR_NONE) {
        return 1; // type A
    }
    else
    {
        return 12; // type Ptr
    }
}

vector<Label> splitStr(char* str)
{
    char* temp = (char*)malloc(strlen(str)); // copy contents of str to use within the function without changing str itself
    strncpy(temp, str, strlen(str));
    memcpy(temp + strlen(str), "\0", 1);
    
    vector<Label> labels;
    Label token;

    token.label = strtok(temp, ".");
    while (token.label != NULL)
    {
        token.size = strlen(token.label);
        labels.push_back(token);
        token.label = strtok(NULL, ".");
    }
    return labels;
}

char* rDNS(char* lookup)
{
    char* rDNS = (char*)malloc(strlen(lookup));
    char tail[] = "arpa.inet-addr.";
    strncpy(rDNS, tail, strlen(tail));
    strncpy(rDNS + strlen(tail), lookup, strlen(lookup));
    memcpy(rDNS + strlen(tail) + strlen(lookup), "\0", 1);
    return rDNS;
}

void makeDNSQuestion(char *question, char* lookup, int type)
{
    vector<Label> parts;
    string qStr = "";
    if (type == 1)
    {
        parts = splitStr(lookup);
        for (int i = 0; i < parts.size(); i++)
        {
            qStr += parts.at(i).size;
            qStr += parts.at(i).label;
        }
        qStr += '\0';
        memcpy(question, qStr.c_str(), qStr.size());
    }
    else if (type == 12)
    {
        char *rQuery = rDNS(lookup);
        parts = splitStr(rQuery);
        for (int i = parts.size()-1; i >= 0; i--) // remove labels & sizes from vector in reverse order
        {
            qStr += parts.at(i).size;
            qStr += parts.at(i).label;
        }
        qStr += '\0';
        memcpy(question, qStr.c_str(), qStr.size());
    }
}

void startWinsock()
{
    WSADATA wsaData; // initialize winsock
    int code = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (code != 0) 
    {
        printf("WSAStartup failed with status %d\n", code);
        exit(1);
    }
}

SOCKET udpSock()
{
    SOCKET sockt = socket(AF_INET, SOCK_DGRAM, 0); // create UDP socket + error handling
    if (sockt == INVALID_SOCKET)
    {
        printf("socket error %ld\n", WSAGetLastError());
        closesocket(sockt);
        WSACleanup();
        exit(1);
    }
    return sockt;
}

int verifyID(char* data, char* packet)
{
    FixedDNSheader* recvFdh = (FixedDNSheader*)data;
    FixedDNSheader* sentFdh = (FixedDNSheader*)packet;
    USHORT sentID = ntohs(sentFdh->ID);
    USHORT recvID = recvFdh->ID;
    if (sentID != recvID)
    {
        printf("  ++ invalid reply: TXID mismatch, sent 0x%.4x, received 0x%.4x\n", sentID, recvID);
        return -1;
    }
    return 1;
}

unsigned char *decompress(unsigned char* name, unsigned int curr)
{
    int i;
    for (i = 0; i < strlen((const char *)name); i++)
    {
        curr = name[i];
        for (int j = 0; j < (int)curr; j++)
        {
            name[i] = name[i + 1];
            i++;
        }
        name[i] = '.';
    }
    name[i-1] = '\0'; // end of string, removes last "."
    return name;
}

/* parsing response data function based on sample code from  - https://www.binarytides.com/dns-query-code-in-c-with-winsock */
unsigned char* rrName(unsigned char* reader, char* data, int* count)
{
    unsigned char* rName;
    unsigned int offset, hasJumped = 0, curr = 0;
    *count = 1;
    rName = (unsigned char*)malloc(256); // research +

    // handle compression
    while (*reader != NULL) // loop until reaching the "\0" char
    {
        if (*reader >= 0xc0 || *reader >> 6 == 3)
        {
            offset = ((*reader) & 0x3f << 8 ) + *(reader + 1); // research +
            reader = (unsigned char*)(data + offset - 4);
            hasJumped = 1; // update to reflect jump to new location
        }
        else 
        {
            memset(&rName[curr++], *reader, 1);
            //rName[curr++] = *reader;
        }
        reader = reader + 1;
        if (hasJumped == 0)
        {
            *count += 1; // if we haven't jumped to new location we can continue in order
        }
    }
    rName[curr] = '\0'; // end of string
    if (hasJumped == 1)
    {
        *count += 1; // keeps track of how many steps we jumped in the packet
    }

    // convert the rrName to non-compressed format
    decompress(rName, curr);
    return rName;
}

void doDNS(SOCKET s, char * server, char* packet, int pktSize)
{
    // set up and bind local socket
    struct sockaddr_in local; // DNS client socket (where request originates)
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(0);

    if (bind(s, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR)
    {
        printf("socket error %ld\n", WSAGetLastError());
        return;
    }
     // set up remote socket
    struct sockaddr_in  remote; // DNS target server socket
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_addr.S_un.S_addr = inet_addr(server); // server's IP
    remote.sin_port = htons(53); // DNS port on server

    // time variables
    clock_t start, end;
    timeval tp; // timeout value
    int elapsedTime = 0;
    
    // attempt to receive response from server
    int attempt = 0;
    while (attempt < MAX_ATTEMPTS)
    {
        printf("Attempt %d with %d bytes... ", attempt, pktSize);
        
        if (sendto(s, packet, pktSize, 0, (struct sockaddr*)&remote, sizeof(remote)) == SOCKET_ERROR)
        {
            printf("send() error %ld\n", WSAGetLastError());
            return;
        }
        
        // initialize time variables
        start = clock(); // start counting elapsed time
        tp.tv_sec = 10;
        tp.tv_usec = 0;

        // get ready to receive a response
        fd_set fd;
        FD_ZERO(&fd);       // clear the set
        FD_SET(s, &fd);     // add socket to the set
        int status = select(0, &fd, NULL, NULL, &tp);
        
        if (status == SOCKET_ERROR) // error while listening for response, NOT a timeout
        {
            printf("socket error %ld\n", WSAGetLastError());
            return;
        }
        else if(status == 0) // select timed out
        {
            end = clock();
            elapsedTime = (int)((1000*(end - start))/CLOCKS_PER_SEC);
            printf("timeout in %d ms\n", elapsedTime);
            attempt += 1;
        }
        else if(status > 0) // response received!
        {
            int maxSize = MAX_DNS_SIZE;          // max size of a DNS response
            char* data = (char*)malloc(maxSize); // allocate data buffer
            struct sockaddr_in response;
            int size = sizeof(response);

            int bytes = 0;
            if ((bytes = recvfrom(s, data, maxSize, 0, (struct sockaddr*)&response, &size)) == SOCKET_ERROR)
            {
                printf("recv() error %ld\n", WSAGetLastError());
                return;
            }
             // output response summary
            end = clock(); // save the current time
            elapsedTime = (int)((1000 *(end - start))/ CLOCKS_PER_SEC);
            printf("response in %d ms with %d bytes\n", elapsedTime, bytes);
            
            // check that response isn't "bogus"
            if (response.sin_addr.S_un.S_addr != remote.sin_addr.S_un.S_addr || response.sin_port != remote.sin_port)
            {
                printf("  ++ invalid response: intended remote server address and port do not match the sender's\n");
                return;
            }
            
            // make sure response has a header
            if (bytes < sizeof(FixedDNSheader)) // received less bytes than the header needs, error
            {
                printf("  ++ invalid reply: packet smaller than fixed DNS header\n");
                return;
            }
            // set and output response's fixed DNS header 
            FixedDNSheader* fdh = (FixedDNSheader*)data;
            fdh->getRHdr(); // read fdh->ID and other fields from response
            
            // check the response's query ID vs. the sent query ID
            int check = verifyID(data, packet);
            if (check < 0)
            {
                return;
            }
            
            // rcode is the last flag and its size is 4 bits, so to extract its value:
            int Rcode = (int)(((fdh->flags) & ((1u << 4) - 1)));
            if (Rcode != 0)
            {
                printf("  failed with Rcode = %d\n", Rcode);
                return;
            }
            printf("  succeeded with Rcode = %d\n", Rcode);
            
            /* Code to parse response contents heavily based on example program found at: https://www.binarytides.com/dns-query-code-in-c-with-winsock */
            int stop = 0;
            unsigned char* reader = (unsigned char*)(data + sizeof(FixedDNSheader));
            ResponseRecord question;
            if ((fdh->questions) > 0) // questions
            {
                printf("  ------------ [questions] ------------\n");
                // parse questions
                for (int i = 0; i < fdh->questions; i++)
                {
                    question.name = rrName(reader, data, &stop);
                    reader = reader + stop;
                    question.resource = (DNSanswerHdr*)(reader);
                    reader = reader + sizeof(DNSanswerHdr) + strlen((const char *)question.name);
                    question.rdata = (unsigned char*)"\0";
                    reader = reader + strlen((const char *)question.name) + stop;
                    printf("\t\b%s type %d class %d\n", question.name, (int)(ntohs(question.resource->rtype)), (int)(ntohs(question.resource->rclass))); 
                }
            }
            if ((fdh->answers) > 0) // answers
            {
                printf("  ------------  [answers]  ------------\n");
                // parse answers
            }
            // authority
            ResponseRecord auth;
            if ((fdh->authRR) > 0)
            {
                printf("  ------------ [authority] ------------\n");
            }
            // additional
            if ((fdh->additionalRR) > 0)
            {
                printf("  ------------ [additional] ------------\n");
            }
            break;
        }
    }
}