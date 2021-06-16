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

#include "EkaFhMiaxParser.h"
#include "eka_macros.h"

using namespace Tom;

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
    {inet_addr("224.0.105.1"), 51001, 0, 0},
    {inet_addr("224.0.105.2"), 51002, 0, 0},
    {inet_addr("224.0.105.3"), 51003, 0, 0},
    {inet_addr("224.0.105.4"), 51004, 0, 0},
    {inet_addr("224.0.105.5"), 51005, 0, 0},
    {inet_addr("224.0.105.6"), 51006, 0, 0},
    {inet_addr("224.0.105.7"), 51007, 0, 0},
    {inet_addr("224.0.105.8"), 51008, 0, 0},
    {inet_addr("224.0.105.9"), 51009, 0, 0},
    {inet_addr("224.0.105.10"), 51010, 0, 0},
    {inet_addr("224.0.105.11"), 51011, 0, 0},
    {inet_addr("224.0.105.12"), 51012, 0, 0},
    {inet_addr("224.0.105.13"), 51013, 0, 0},
    {inet_addr("224.0.105.14"), 51014, 0, 0},
    {inet_addr("224.0.105.15"), 51015, 0, 0},
    {inet_addr("224.0.105.16"), 51016, 0, 0},
    {inet_addr("224.0.105.17"), 51017, 0, 0},
    {inet_addr("224.0.105.18"), 51018, 0, 0},
    {inet_addr("224.0.105.19"), 51019, 0, 0},
    {inet_addr("224.0.105.20"), 51020, 0, 0},
    {inet_addr("224.0.105.21"), 51021, 0, 0},
    {inet_addr("224.0.105.22"), 51022, 0, 0},
    {inet_addr("224.0.105.23"), 51023, 0, 0},
    {inet_addr("224.0.105.24"), 51024, 0, 0},
    {inet_addr("233.105.122.1"), 53001, 0, 0},
    {inet_addr("233.105.122.2"), 53002, 0, 0},
    {inet_addr("233.105.122.3"), 53003, 0, 0},
    {inet_addr("233.105.122.4"), 53004, 0, 0},
    {inet_addr("233.105.122.5"), 53005, 0, 0},
    {inet_addr("233.105.122.6"), 53006, 0, 0},
    {inet_addr("233.105.122.7"), 53007, 0, 0},
    {inet_addr("233.105.122.8"), 53008, 0, 0},
    {inet_addr("233.105.122.9"), 53009, 0, 0},
    {inet_addr("233.105.122.10"), 53010, 0, 0},
    {inet_addr("233.105.122.11"), 53011, 0, 0},
    {inet_addr("233.105.122.12"), 53012, 0, 0},
    {inet_addr("233.105.122.13"), 53013, 0, 0},
    {inet_addr("233.105.122.14"), 53014, 0, 0},
    {inet_addr("233.105.122.15"), 53015, 0, 0},
    {inet_addr("233.105.122.16"), 53016, 0, 0},
    {inet_addr("233.105.122.17"), 53017, 0, 0},
    {inet_addr("233.105.122.18"), 53018, 0, 0},
    {inet_addr("233.105.122.19"), 53019, 0, 0},
    {inet_addr("233.105.122.20"), 53020, 0, 0},
    {inet_addr("233.105.122.21"), 53021, 0, 0},
    {inet_addr("233.105.122.22"), 53022, 0, 0},
    {inet_addr("233.105.122.23"), 53023, 0, 0},
    {inet_addr("233.105.122.24"), 53024, 0, 0},
    {inet_addr("224.0.141.1"), 57001, 0, 0},
    {inet_addr("224.0.141.2"), 57002, 0, 0},
    {inet_addr("224.0.141.3"), 57003, 0, 0},
    {inet_addr("224.0.141.4"), 57004, 0, 0},
    {inet_addr("224.0.141.5"), 57005, 0, 0},
    {inet_addr("224.0.141.6"), 57006, 0, 0},
    {inet_addr("224.0.141.7"), 57007, 0, 0},
    {inet_addr("224.0.141.8"), 57008, 0, 0},
    {inet_addr("224.0.141.9"), 57009, 0, 0},
    {inet_addr("224.0.141.10"), 57010, 0, 0},
    {inet_addr("224.0.141.11"), 57011, 0, 0},
    {inet_addr("224.0.141.12"), 57012, 0, 0},
    {inet_addr("233.87.168.1"), 58001, 0, 0},
    {inet_addr("233.87.168.2"), 58002, 0, 0},
    {inet_addr("233.87.168.3"), 58003, 0, 0},
    {inet_addr("233.87.168.4"), 58004, 0, 0},
    {inet_addr("233.87.168.5"), 58005, 0, 0},
    {inet_addr("233.87.168.6"), 58006, 0, 0},
    {inet_addr("233.87.168.7"), 58007, 0, 0},
    {inet_addr("233.87.168.8"), 58008, 0, 0},
    {inet_addr("233.87.168.9"), 58009, 0, 0},
    {inet_addr("233.87.168.10"), 58010, 0, 0},
    {inet_addr("233.87.168.11"), 58011, 0, 0},
    {inet_addr("233.87.168.12"), 58012, 0, 0},
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
	
	p += sizeof(EkaEthHdr) + sizeof(EkaIpHdr);
	int payloadLen = EKA_UDPHDR_LEN(p) - sizeof(EkaUdpHdr);	
	p += sizeof(EkaUdpHdr);

	/* ------------------------------------------------------------- */
	auto processedBytes = 0;
	while (processedBytes < payloadLen) {	
	    auto sequence   = EKA_GET_MACH_SEQ(p);
	    auto machType   = EKA_GET_MACH_TYPE(p);
	    auto machPktLen = EKA_GET_MACH_LEN(p);
	    
	    uint64_t ts = 0;
	    switch (machType) {
	    case MiaxMachType::Heartbeat :
		break;
	    case MiaxMachType::StartOfSession :
		printf("StartOfSession\n");
		break;
	    case MiaxMachType::EndOfSession : 
		printf("EndOfSession\n");
		break;
	    case MiaxMachType::AppData : {
		if (group[grId].expectedSeq != 0 && group[grId].expectedSeq != sequence) {
		    printf (RED "%d: expectedSeq %ju != sequence %ju\n" RESET,
			    grId,group[grId].expectedSeq,sequence);
		}
		
		auto msgHdr {reinterpret_cast<const TomCommon*>(p + sizeof(EkaMiaxMach))};
		auto msgType {static_cast<const TOM_MSG>(msgHdr->Type)};
		switch (msgType) {
		case TOM_MSG::Time :
		    group[grId].baseTime = msgHdr->Timestamp * 1e9;
		    break;
		case TOM_MSG::SeriesUpdate :
		case TOM_MSG::SystemState :
		    break;
		case TOM_MSG::UnderlyingTradingStatus : 
		case TOM_MSG::BestBidShort : 
		case TOM_MSG::BestAskShort : 
		case TOM_MSG::BestBidLong : 
		case TOM_MSG::BestAskLong : 
		case TOM_MSG::BestBidAskShort :   
		case TOM_MSG::BestBidAskLong :  
		case TOM_MSG::Trade: 
		case TOM_MSG::TradeCancel : 
		case TOM_MSG::EndOfRefresh : 
		    ts = group[grId].baseTime + msgHdr->Timestamp;
		    break;
		default:
		    break;
		}

		if (printAll)
		    printf ("%d: %ju, %ju, \'%c\'\n",grId,ts,sequence,msgHdr->Type);
		    
		group[grId].expectedSeq = sequence + 1;

	    }
		break;		
	    default:
		on_error("Unexpected Mach Type %u",(uint8_t)machType);
	    }
	    processedBytes += machPktLen;
	    p += machPktLen;
	}
    }
    return 0;
}
