#pragma once

/* DNS Header Flags - from HW2 PDF + netsorcery */
#define DNS_QUERY    (0 << 15)  // 0 = query, 1 = response
#define DNS_RESPONSE (1 << 15)

#define DNS_STDQUERY (0 << 11)  // opcode - 4 bits

#define DNS_AA       (1 << 10)  // authoritative answer
#define DNS_TC       (1 << 9)   // truncated
#define DNS_RD       (1 << 8)   // recursion desired
#define DNS_RA       (1 << 7)   // recursion available
#define DNS_Z        (1 << 6)   // Z
#define DNS_AD       (1 << 5)   // authenticated data
#define DNS_CD       (1 << 4)   // checking disabled


/* DNS Query Types - from HW2 PDF */
#define DNS_A           1   // host = name
#define DNS_NS          2   // name server
#define DNS_CNAME       5   // canonical name
#define DNS_PTR         12  // host = IP
#define DNS_HINFO       13  // host info/SOA
#define DNS_MX          15  // mail exchange
#define DNS_AXFR        252 // request for some transfer
#define DNS_ANY         255 // all records

/* DNS Class */
#define DNS_INET        1   // class 1 = internet

/* DNS RESPONSE VARIABLES */
#define MAX_ATTEMPTS    3   // max. # of times to attempt sending DNS packet
#define MAX_DNS_SIZE   512  // max. size of a DNS packet

/* DNS Response Flags - from HW2 PDF */
#define DNS_OK          0   // success
#define DNS_FORMAT      1   // format error (unable to interpret)
#define DNS_SERVERFAIL  2   // cannot find authority nameserver
#define DNS_ERROR       3   // no DNS entry
#define DNS_NOTIMPL     4   // not implemented
#define DNS_REFUSED     5   // server refused the query

#pragma pack(push, 1)       // sets struct padding/alignment to 1 byte
class QueryHeader
{
public:
    USHORT qType;           // either A or Ptr (1 or 12)
    USHORT qClass;          // wil always be = 1 (internet)
    void setQueryHdr(int type);
};

class FixedDNSheader
{
public:
    USHORT ID;              // can be anything
    USHORT flags;           // defined above
    USHORT questions;       // will always be = 1 
    USHORT answers;         // 0
    USHORT authRR;          // 0
    USHORT additionalRR;    // 0
    void setFixdHeadr();    // sets all of the above elements to their corresponding values
    void getRHdr(); // gets the values of the above elements from the response data
};

class DNSanswerHdr
{
public:
    USHORT rtype;
    USHORT rclass;
    USHORT rlen;
    UINT rTTL;
};
#pragma pack(pop)           // restores old packing

/* ---------------------------------------- FUNCTIONS ---------------------------------------- */
struct Label
{
    char* label;
    unsigned char size;
};

struct ResponseRecord   // for parsing response contents
{
    unsigned char* name;
    DNSanswerHdr* resource;
    unsigned char* rdata;
};

int getType(char* lookupStr); // determine query type
std::vector<Label> splitStr(char* str); // split given string at each . and return string to build question from
char* rDNS(char* lookup); // generates the reverse lookup string to use with type = PTR
void makeDNSQuestion(char* packet, char* lookup, int type); // build question string for DNS query
void startWinsock(); // initializes winsock (for the UDP sockets)
SOCKET udpSock(); // creates a new UDP socket to use for sending/receiving data
void doDNS(SOCKET s, char* server, char* packet, int pktSize); // sends the DNS packet to target, receives response, & parses it
int verifyID(char* data, char* packet); // checks DNS id is the same for sent and received queries
unsigned char* decompress(unsigned char* name, unsigned int curr); // converts from compressed string to "regular" string
unsigned char* rrName(unsigned char* reader, char* data, int* count);