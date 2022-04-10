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

#include "EkaFhCmeParser.h"
#include "eka_macros.h"

using namespace Cme;


//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;
static bool printTrig = false;
static int  numPriceLevels = 1;
static int  maxMsgSize = 1500;

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  baseTime;
    uint64_t  expectedSeq;
};

static GroupAddr group[] = {
    {inet_addr("224.0.31.2"),  14311, 0, 0},     // Options incrementFeed A
    {inet_addr("224.0.31.44"), 14311, 0, 0},     // Options definitionFeed A
    {inet_addr("224.0.31.23"), 14311, 0, 0},     // Options snapshotFeed A

    {inet_addr("224.0.31.1"),  14310, 0, 0},     // Futures incrementFeed A
    {inet_addr("224.0.31.43"), 14310, 0, 0},     // Futures definitionFeed A
    {inet_addr("224.0.31.22"), 14310, 0, 0},     // Futures snapshotFeed A

    {inet_addr("224.0.31.10"), 14319, 0, 0},     // Options excludes E-mini S&P incrementFeed A
    {inet_addr("224.0.31.52"), 14319, 0, 0},     // Options excludes E-mini S&P definitionFeed A
    {inet_addr("224.0.31.31"), 14319, 0, 0},     // Options excludes E-mini S&P snapshotFeed A

    {inet_addr("224.0.31.9"),  14318, 0, 0},     // Futures excludes E-mini S&P incrementFeed A
    {inet_addr("224.0.31.51"), 14318, 0, 0},     // Futures excludes E-mini S&P definitionFeed A
    {inet_addr("224.0.31.30"), 14318, 0, 0},     // Futures excludes E-mini S&P snapshotFeed A

    {inet_addr("224.0.32.1"),  15310, 0, 0},     // Futures incrementFeed B
    {inet_addr("224.0.32.2"),  15311, 0, 0},     // Options incrementFeed B
    
    {inet_addr("224.0.31.66"), 14342, 0, 0},     // Patrick

    {inet_addr("224.0.74.0"),  30301, 0, 0},     // Fast Cancels

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
  fprintf(stderr,"USAGE: %s [options] -f [pcapFile]\n",cmd);
  fprintf(stderr,"          -l [Number of Price Levels] -- default 1\n");
  fprintf(stderr,"          -s [Max Message Size]       -- default 1500\n");
  fprintf(stderr,"          -p        Print all messages\n");
  fprintf(stderr,"          -t        Print strategy trigger\n");

}

//###################################################

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":f:d:l:s:tph")) != -1) {  
    switch(opt) {  
    case 'f':
      strcpy(pcapFileName,optarg);
      fprintf(stderr,"pcapFile = %s\n", pcapFileName);  
      break;
    case 'l':
      numPriceLevels = atoi(optarg);
      fprintf(stderr,"numPriceLevels = %d\n", numPriceLevels);  
      break;
    case 's':
      maxMsgSize = atoi(optarg);
      fprintf(stderr,"maxMsgSize = %d\n", maxMsgSize);  
      break;  
    case 'p':  
      printAll = true;
      fprintf(stderr,"printAll\n");
      break;  
    case 't':  
      printTrig = true;
      fprintf(stderr,"printTrig\n");
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
      fprintf(stderr,"unknown option: %c\n", optopt); 
      break;  
    }  
  }  
  return 0;
}

//###################################################

inline uint32_t printTrigger(const uint8_t* pkt, const int payloadLen,
			     uint64_t pktNum, int pLevels, int maxMsgSize) {
    auto p {pkt};
    auto pktHdr {reinterpret_cast<const PktHdr*>(p)};

    p += sizeof(*pktHdr);

    //    while (p - pkt < payloadLen) { // 1st msg only
    auto m {p};
    auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};

    m += sizeof(*msgHdr);

    switch (msgHdr->templateId) {
      /* ##################################################################### */
    case MsgId::MDIncrementalRefreshTradeSummary48 : {
      /* ------------------------------- */
      //      auto rootBlock {reinterpret_cast<const MDIncrementalRefreshTradeSummary48_mainBlock*>(m)};

      m += msgHdr->blockLen;
      /* ------------------------------- */
      auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
      if (pGroupSize->numInGroup < pLevels) break;
      if (msgHdr->size > maxMsgSize) break;
	
      m += sizeof(*pGroupSize);
      auto e {reinterpret_cast<const MDIncrementalRefreshTradeSummary48_mdEntry*>(m)};

      printf("Trigger,");
      printf("%ju,", pktNum);
      printf("%s,", ts_ns2str(pktHdr->time).c_str());
      printf("%u,", pktHdr->seq);
      printf("%u,", msgHdr->size);
      printf("%u,", pGroupSize->numInGroup);
      //      printf("%16jd,",(int64_t) (e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE));
      printf("%16jd,",(int64_t) e->MDEntryPx );
      printf("\n");

    }
      break;	
      /* ##################################################################### */
    default:
      break;
		
    }
    /* ----------------------------- */

    p += msgHdr->size;
    //    } //  while (p - pkt < payloadLen)
    return 0;
  } // printTrigger()
//###################################################

int main(int argc, char *argv[]) {
    char buf[1600] = {};
    FILE *pcapFile;
    getAttr(argc,argv);

    if ((pcapFile = fopen(pcapFileName, "rb")) == NULL) {
	fprintf(stderr,"Failed to open dump file %s\n",pcapFileName);
	printUsage(argv[0]);
	exit(1);
    }
    if (fread(buf,sizeof(pcap_file_hdr),1,pcapFile) != 1) 
	on_error ("Failed to read pcap_file_hdr from the pcap file");

    uint64_t pktNum {0};
    if (printTrig) {
      printf("Trigger,pkt,ts,seq,msgSize,numInGroup,firstPrice\n"); 
    }
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

      auto udpHdr {p + sizeof(EkaEthHdr) + sizeof(EkaIpHdr)};
      int payloadLen = EKA_UDPHDR_LEN(udpHdr) - sizeof(EkaUdpHdr);
      p += sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);

      auto sequence = getPktSeq(p);
      /* -------------------------------------------------- */
      if (group[grId].expectedSeq != 0 &&
	  group[grId].expectedSeq != sequence) {
	printf (RED "%d: expectedSeq %ju != sequence %u\n" RESET,
		grId,group[grId].expectedSeq,sequence);
      }
      /* -------------------------------------------------- */
      if (printAll) {
	printf("GR%-2d: ",grId);
	printPkt(p, payloadLen, pktNum);
      }
      /* -------------------------------------------------- */
      if (printTrig) {
	printTrigger(p, payloadLen, pktNum, numPriceLevels, maxMsgSize);
      }
      /* -------------------------------------------------- */

    }
    
    return 0;
}
