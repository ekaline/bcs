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

#include "EkaFhPhlxTopoParser.h"
#include "eka_macros.h"

using namespace TOPO;

//###################################################
struct pcap_file_hdr {
        uint32_t magic_number;   /* magic number */
         uint16_t version_major;  /* major version number */
         uint16_t version_minor;  /* minor version number */
         int32_t  thiszone;       /* GMT to local correction */
         uint32_t sigfigs;        /* accuracy of timestamps */
         uint32_t snaplen;        /* max length of captured packets, in octets */
         uint32_t network;        /* data link type */
 };
 struct pcap_rec_hdr {
         uint32_t ts_sec;         /* timestamp seconds */
         uint32_t ts_usec;        /* timestamp microseconds */
         uint32_t cap_len;        /* number of octets of packet saved in file */
         uint32_t len;            /* actual length of packet */
 };


//###################################################
static char pcapFileName[256] = {};

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  gr_ts;
    uint64_t  expectedSeq;
};

static GroupAddr group[] = {
	{inet_addr("233.47.179.104"), 18016},
	{inet_addr("233.47.179.105"), 18017},
	{inet_addr("233.47.179.106"), 18018},
	{inet_addr("233.47.179.107"), 18019},
	{inet_addr("233.47.179.108"), 18020},
	{inet_addr("233.47.179.109"), 18021},
	{inet_addr("233.47.179.110"), 18022},
	{inet_addr("233.47.179.111"), 18023},
		     
	{inet_addr("233.47.179.168"), 18016},
	{inet_addr("233.47.179.169"), 18017},
	{inet_addr("233.47.179.170"), 18018},
	{inet_addr("233.47.179.171"), 18019},
	{inet_addr("233.47.179.172"), 18020},
	{inet_addr("233.47.179.173"), 18021},
	{inet_addr("233.47.179.174"), 18022},
	{inet_addr("233.47.179.175"), 18023}
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
  printf("USAGE: %s -f [pcapFile]\n",cmd);
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
//	printAll = true;
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
	
	auto gr = findGrp(EKA_IPH_DST(p),EKA_UDPH_DST(p));
	if (gr < 0) continue;

	p += sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);

	auto msgCnt = EKA_MOLD_MSG_CNT(p);
	uint64_t sequence = EKA_MOLD_SEQUENCE(p);
	p += sizeof(mold_hdr);

	for (auto i = 0; i < msgCnt; i++) {
	    uint16_t msgLen = be16toh(*(uint16_t*)p);
	    p += sizeof(msgLen);
	    //-----------------------------------------------------------------------------
	    auto m {reinterpret_cast<const GenericHdr*>(p)};
	    printf ("%d: %ju, \'%c\'\n",gr, sequence++,m->message_type);
	    //-----------------------------------------------------------------------------

	    p += msgLen;
	}
	
    }
    return 0;
}
