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
#include "EkaFhBxParser.h"
#include "eka_macros.h"

//using namespace Nom;
#include "EkaFhNasdaqCommonParser.h"

using namespace EfhNasdaqCommon;


//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  baseTime;
    uint64_t  expectedSeq;
};

static GroupAddr group[] = {
    {inet_addr("233.54.12.72"), 18000, 0, 0},
    {inet_addr("233.54.12.73"), 18001, 0, 0},
    {inet_addr("233.54.12.74"), 18002, 0, 0},
    {inet_addr("233.54.12.75"), 18003, 0, 0},
    {inet_addr("233.49.196.72"), 18000, 0, 0},
    {inet_addr("233.49.196.73"), 18001, 0, 0},
    {inet_addr("233.49.196.74"), 18002, 0, 0},
    {inet_addr("233.49.196.75"), 18003, 0, 0},
   
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
	auto pcap_rec_hdr_ptr {reinterpret_cast<const pcap_rec_hdr*>(buf)};
	uint pktLen = pcap_rec_hdr_ptr->len;
	if (pktLen > 1536)
	    on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

	char pkt[1536] = {};
	if (fread(pkt,pktLen,1,pcapFile) != 1) 
	    on_error ("Failed to read %d packet bytes at pkt %ju",pktLen,pktNum);
	pktNum++;

	auto p {reinterpret_cast<const uint8_t*>(pkt)};
	if (! EKA_IS_UDP_PKT(p)) continue;
	
	auto grId = findGrp(EKA_IPH_DST(p),EKA_UDPH_DST(p));
	if (grId < 0) continue;

	p += sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);

	/* auto msgCnt = EKA_MOLD_MSG_CNT(p); */
	/* uint64_t sequence = EKA_MOLD_SEQUENCE(p); */
	/* if (group[grId].expectedSeq != 0 && group[grId].expectedSeq != sequence) { */
	/*     printf (RED "%d: expectedSeq %ju != sequence %ju\n" RESET, */
	/* 	    grId,group[grId].expectedSeq,sequence); */
	/* } */
	/* p += sizeof(mold_hdr); */

	/* for (auto i = 0; i < msgCnt; i++) { */
	/*     uint16_t msgLen = be16toh(*(uint16_t*)p); */
	/*     p += sizeof(msgLen); */
	/*     //----------------------------------------------------------------------------- */
	/*     uint64_t ts = get_ts(p); */
	/*     if (printAll) */
	/*       printMsg(stdout,p,grId,sequence,ts); */
	/*     sequence++; */
	/*     //----------------------------------------------------------------------------- */

	/*     p += msgLen; */
	/* } */

	printPkt<Bx>(stdout,p);
	
	//	group[grId].expectedSeq = sequence;
	
    }
    return 0;
}
