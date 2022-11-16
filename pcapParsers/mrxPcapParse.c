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
#include "EkaFhMrx2TopParser.h"
#include "eka_macros.h"

#include "EkaNwParser.h"

using namespace Mrx2Top;
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

static GroupAddr group[] = {
    {inet_addr("233.104.73.0"), 18000, 0, 0},
    {inet_addr("233.104.73.1"), 18001, 0, 0},
    {inet_addr("233.104.73.2"), 18002, 0, 0},
    {inet_addr("233.104.73.3"), 18003, 0, 0},
    {inet_addr("233.127.56.0"), 18000, 0, 0},
    {inet_addr("233.127.56.1"), 18001, 0, 0},
    {inet_addr("233.127.56.2"), 18002, 0, 0},
    {inet_addr("233.127.56.3"), 18003, 0, 0},
   
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
	
	auto etherP {reinterpret_cast<const uint8_t*>(pkt)};
	if (! Eth::isIp(etherP)) continue;

	auto ipP = etherP + Eth::getHdrLen();
	if (! Ip::isUdp(ipP)) continue;

	auto udpP = ipP + Ip::getHdrLen(ipP);
	
	/* printf("%8ju: %s:%u\n",pktNum, */
	/*        EKA_IP2STR(Ip::getDst(ipP)),Udp::getDst(udpP)); */
	
	auto grId = findGrp(Ip::getDst(ipP),Udp::getDst(udpP));
	if (grId < 0) continue;

	auto dataP = udpP + Udp::getHdrLen();	

	MoldUdp64::printPkt(stdout,dataP,Mrx2Top::printMsg);
	
	//	group[grId].expectedSeq = sequence;
	
    }
    return 0;
}
