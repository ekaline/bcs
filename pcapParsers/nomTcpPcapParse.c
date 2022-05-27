#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <assert.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "EkaFhTypes.h"
#include "EkaFhNomParser.h"
#include "eka_macros.h"

//using namespace Nom;
#include "EkaFhNasdaqCommonParser.h"
#include "EkaNwParser.h"
using namespace EfhNasdaqCommon;
using namespace EkaNwParser;

//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  baseTime;
    uint64_t  expectedSeq;
};

struct SoupbinHdr {
  uint16_t    length;
  char        type;
} __attribute__((packed));

static GroupAddr group[] = {
    {inet_addr("206.200.43.72"), 18300, 0, 0},
    {inet_addr("206.200.43.73"), 18301, 0, 0},
    {inet_addr("206.200.43.74"), 18302, 0, 0},
    {inet_addr("206.200.43.75"), 18303, 0, 0},
};


//###################################################

int findGrp(uint32_t ip, uint16_t port) {
  for (size_t i = 0; i < sizeof(group)/sizeof(group[0]); i++) {
    if (group[i].ip == ip && group[i].port == port)
	    return (int)i;
  }
  //  on_error("%s:%u is not found",EKA_IP2STR(ip),port);
  return -1;
}

//###################################################
void printUsage(char* cmd) {
  printf("USAGE: %s [options] -f [pcapFile]\n",cmd);
  printf("          -p        Print all messages\n");
}

//###################################################

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":f:d:ph")) != -1) {  
    switch(opt) {  
      case 'f':
	strcpy(pcapFileName,optarg);
	printf("pcapFile = %s\n", pcapFileName);  
	break;  
      case 'p':  
	printAll = true;
	printf("printAll\n");
	break;  
      case 'd':  
//	pkt2dump = atoi(optarg);
//	printf("pkt2dump = %ju\n",pkt2dump);  
	break;  
      case 'h':  
	printUsage(argv[0]);
	exit (1);
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  
  return 0;
}

//###################################################

int main(int argc, char *argv[]) {
    char buf[1600] = {};
    FILE *pcapFile;
    getAttr(argc,argv);

    if ((pcapFile = fopen(pcapFileName, "rb")) == NULL) {
	printf("Failed to open dump file %s\n",pcapFileName);
	printUsage(argv[0]);
	exit(1);
    }
    if (fread(buf,sizeof(pcap_file_hdr),1,pcapFile) != 1) 
	on_error ("Failed to read pcap_file_hdr from the pcap file");

    uint64_t pktNum = 0;

    while (fread(buf,sizeof(pcap_rec_hdr),1,pcapFile) == 1) {
	auto pcapRecHdr {reinterpret_cast<const pcap_rec_hdr*>(buf)};
	uint pktLen = pcapRecHdr->len;
	if (pktLen > 1536)
	    on_error("Probably wrong PCAP format: pktLen = %u ",
		     pktLen);

	char pkt[1536] = {};
	if (fread(pkt,pktLen,1,pcapFile) != 1) 
	    on_error ("Failed to read %d packet bytes at pkt %ju",
		      pktLen,pktNum);
	pktNum++;

	if (! isTcpPkt(pkt)) continue;
	
	auto grId = findGrp(getIpSrc(pkt),getTcpSrc(pkt));
	if (grId < 0) continue;
	printf ("%ju: %s:%u\n",pktNum,EKA_IP2STR(getIpSrc(pkt)),getTcpSrc(pkt));
	fflush(stdout);

	auto tcpPayload = getTcpPayload(pkt);
	if (!tcpPayload) on_error("Not Tcp");

	/* auto s = reinterpret_cast<const uint8_t*>(pkt); */
	/* s += Eth::getHdrLen(); */
	/* printf ("getIpPktLen(pkt)=%u, IpHdrLen = %ju, IpPktLen=%u\n", */
	/* 	getIpPktLen(pkt),Ip::getHdrLen(s),Ip::getPktLen(s)); */
	/* s += Ip::getHdrLen(s); */
	/* printf ("TcpHdrLen = %ju\n",Tcp::getHdrLen(s)); */
	auto p = tcpPayload;
	

	ssize_t tcpPayloadLen = getTcpPayloadLen(pkt);
	if (tcpPayloadLen < 0) on_error("tcpPayloadLen = %jd",tcpPayloadLen);
	if (tcpPayloadLen == 0) continue;

	auto soupbinHdr {reinterpret_cast<const SoupbinHdr*>(p)};
	//	uint16_t pktLen = be16toh(soupbinHdr->length);
	p += sizeof(*soupbinHdr);
	printf ("%ju:\'%c\' (0x%x)\n",
		pktNum,soupbinHdr->type,soupbinHdr->type);
	switch (soupbinHdr->type) {
	case 'S' :
	case 'U' :
	  printMsg<NomFeed>(stdout,0,p);
	  break;
	case 'A' :
	case 'H' :
	case '+' :
	  break;
	default :
	  hexDump("bad soupbin pkt",tcpPayload,tcpPayloadLen);
	  
	  on_error("Unexpected Soupbin type \'%c\' (0x%x), tcpPayloadLen=%jd",
		   soupbinHdr->type,soupbinHdr->type,tcpPayloadLen);
	}
    }
    return 0;
}
