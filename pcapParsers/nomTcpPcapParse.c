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
    getAttr(argc,argv);

    SoupbinHdr soupbinHdr = {};
    uint8_t pkt[1536];

    auto pktHndl = new TcpPcapHandler(pcapFileName,
				      inet_addr("206.200.43.72"), 18300);
    
    while (1) {
      ssize_t hdrSize = pktHndl->getData(&soupbinHdr,sizeof(soupbinHdr));
      if (hdrSize != sizeof(soupbinHdr)) {
	TEST_LOG("hdrSize %jd != sizeof(soupbinHdr) %jd",
		 hdrSize,sizeof(soupbinHdr));
	break;
      }
      ssize_t size2read = be16toh(soupbinHdr.length) - sizeof(soupbinHdr.type);
      ssize_t pktSize = pktHndl->getData(pkt,size2read);
      if (pktSize != size2read) {
	TEST_LOG("pktSize %jd != size2read %jd",
		 pktSize,size2read);
	break;
      }
      printf ("\'%c\' (0x%x), %jd\n",
	      soupbinHdr.type,soupbinHdr.type,size2read);
      switch (soupbinHdr.type) {
      case 'S' :
      case 'U' :
	printMsg<NomFeed>(stdout,0,pkt);
	break;
      case 'A' :
      case 'H' :
      case '+' :
	break;
      default :
	//	hexDump("bad soupbin pkt",tcpPayload,tcpPayloadLen);
	  
	on_error("Unexpected Soupbin type \'%c\' (0x%x), pktSize=%jd",
		 soupbinHdr.type,soupbinHdr.type,pktSize);
      }
    } // while()

    delete pktHndl;
    return 0;
}
