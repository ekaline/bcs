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

#include "EkaFhXdpParser.h"
#include "eka_macros.h"

using namespace Xdp;

//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  baseTime   = 0;
    int       numStreams = 0;
    Stream*   stream[MAX_STREAMS] = {};
};

static GroupAddr group[] = {
    {inet_addr("224.0.41.32"), 11032, 0, 0, {}},
    {inet_addr("224.0.41.33"), 11033, 0, 0, {}},
    {inet_addr("224.0.41.34"), 11034, 0, 0, {}},
    {inet_addr("224.0.41.35"), 11035, 0, 0, {}},
    {inet_addr("224.0.41.36"), 11036, 0, 0, {}},
    {inet_addr("224.0.41.37"), 11037, 0, 0, {}},
    {inet_addr("224.0.41.38"), 11038, 0, 0, {}},
    {inet_addr("224.0.41.39"), 11039, 0, 0, {}},
    {inet_addr("224.0.41.40"), 11040, 0, 0, {}},
    {inet_addr("224.0.41.41"), 11041, 0, 0, {}},
    {inet_addr("224.0.41.42"), 11042, 0, 0, {}},
    {inet_addr("224.0.41.43"), 11043, 0, 0, {}},
    {inet_addr("224.0.41.44"), 11044, 0, 0, {}},
    {inet_addr("224.0.41.45"), 11045, 0, 0, {}},
    {inet_addr("224.0.41.46"), 11046, 0, 0, {}},
    {inet_addr("224.0.41.47"), 11047, 0, 0, {}},
    {inet_addr("224.0.41.48"), 11048, 0, 0, {}},
    {inet_addr("224.0.41.49"), 11049, 0, 0, {}},
    {inet_addr("224.0.41.50"), 11050, 0, 0, {}},
    {inet_addr("224.0.41.51"), 11051, 0, 0, {}},
    {inet_addr("224.0.41.52"), 11052, 0, 0, {}},
    {inet_addr("224.0.41.53"), 11053, 0, 0, {}},
    {inet_addr("224.0.41.54"), 11054, 0, 0, {}},
    {inet_addr("224.0.41.55"), 11055, 0, 0, {}},
    {inet_addr("224.0.41.56"), 11056, 0, 0, {}},
    {inet_addr("224.0.41.57"), 11057, 0, 0, {}},
    {inet_addr("224.0.41.160"), 12160, 0, 0, {}},
    {inet_addr("224.0.41.161"), 12161, 0, 0, {}},
    {inet_addr("224.0.41.162"), 12162, 0, 0, {}},
    {inet_addr("224.0.41.163"), 12163, 0, 0, {}},
    {inet_addr("224.0.41.164"), 12164, 0, 0, {}},
    {inet_addr("224.0.41.165"), 12165, 0, 0, {}},
    {inet_addr("224.0.41.166"), 12166, 0, 0, {}},
    {inet_addr("224.0.41.167"), 12167, 0, 0, {}},
    {inet_addr("224.0.41.168"), 12168, 0, 0, {}},
    {inet_addr("224.0.41.169"), 12169, 0, 0, {}},
    {inet_addr("224.0.41.170"), 12170, 0, 0, {}},
    {inet_addr("224.0.41.171"), 12171, 0, 0, {}},
    {inet_addr("224.0.41.172"), 12172, 0, 0, {}},
    {inet_addr("224.0.41.173"), 12173, 0, 0, {}},
    {inet_addr("224.0.41.174"), 12174, 0, 0, {}},
    {inet_addr("224.0.41.175"), 12175, 0, 0, {}},
    {inet_addr("224.0.41.176"), 12176, 0, 0, {}},
    {inet_addr("224.0.41.177"), 12177, 0, 0, {}},
    {inet_addr("224.0.41.178"), 12178, 0, 0, {}},
    {inet_addr("224.0.41.179"), 12179, 0, 0, {}},
    {inet_addr("224.0.41.180"), 12180, 0, 0, {}},
    {inet_addr("224.0.41.181"), 12181, 0, 0, {}},
    {inet_addr("224.0.41.182"), 12182, 0, 0, {}},
    {inet_addr("224.0.41.183"), 12183, 0, 0, {}},
    {inet_addr("224.0.41.184"), 12184, 0, 0, {}},
    {inet_addr("224.0.41.185"), 12185, 0, 0, {}},
    {inet_addr("224.0.58.32"), 11032, 0, 0, {}},
    {inet_addr("224.0.58.33"), 11033, 0, 0, {}},
    {inet_addr("224.0.58.34"), 11034, 0, 0, {}},
    {inet_addr("224.0.58.35"), 11035, 0, 0, {}},
    {inet_addr("224.0.58.36"), 11036, 0, 0, {}},
    {inet_addr("224.0.58.37"), 11037, 0, 0, {}},
    {inet_addr("224.0.58.38"), 11038, 0, 0, {}},
    {inet_addr("224.0.58.39"), 11039, 0, 0, {}},
    {inet_addr("224.0.58.40"), 11040, 0, 0, {}},
    {inet_addr("224.0.58.41"), 11041, 0, 0, {}},
    {inet_addr("224.0.58.42"), 11042, 0, 0, {}},
    {inet_addr("224.0.58.43"), 11043, 0, 0, {}},
    {inet_addr("224.0.58.44"), 11044, 0, 0, {}},
    {inet_addr("224.0.58.45"), 11045, 0, 0, {}},
    {inet_addr("224.0.58.46"), 11046, 0, 0, {}},
    {inet_addr("224.0.58.47"), 11047, 0, 0, {}},
    {inet_addr("224.0.58.48"), 11048, 0, 0, {}},
    {inet_addr("224.0.58.49"), 11049, 0, 0, {}},
    {inet_addr("224.0.58.50"), 11050, 0, 0, {}},
    {inet_addr("224.0.58.51"), 11051, 0, 0, {}},
    {inet_addr("224.0.58.52"), 11052, 0, 0, {}},
    {inet_addr("224.0.58.53"), 11053, 0, 0, {}},
    {inet_addr("224.0.58.54"), 11054, 0, 0, {}},
    {inet_addr("224.0.58.55"), 11055, 0, 0, {}},
    {inet_addr("224.0.58.56"), 11056, 0, 0, {}},
    {inet_addr("224.0.58.57"), 11057, 0, 0, {}},
    {inet_addr("224.0.58.160"), 12160, 0, 0, {}},
    {inet_addr("224.0.58.161"), 12161, 0, 0, {}},
    {inet_addr("224.0.58.162"), 12162, 0, 0, {}},
    {inet_addr("224.0.58.163"), 12163, 0, 0, {}},
    {inet_addr("224.0.58.164"), 12164, 0, 0, {}},
    {inet_addr("224.0.58.165"), 12165, 0, 0, {}},
    {inet_addr("224.0.58.166"), 12166, 0, 0, {}},
    {inet_addr("224.0.58.167"), 12167, 0, 0, {}},
    {inet_addr("224.0.58.168"), 12168, 0, 0, {}},
    {inet_addr("224.0.58.169"), 12169, 0, 0, {}},
    {inet_addr("224.0.58.170"), 12170, 0, 0, {}},
    {inet_addr("224.0.58.171"), 12171, 0, 0, {}},
    {inet_addr("224.0.58.172"), 12172, 0, 0, {}},
    {inet_addr("224.0.58.173"), 12173, 0, 0, {}},
    {inet_addr("224.0.58.174"), 12174, 0, 0, {}},
    {inet_addr("224.0.58.175"), 12175, 0, 0, {}},
    {inet_addr("224.0.58.176"), 12176, 0, 0, {}},
    {inet_addr("224.0.58.177"), 12177, 0, 0, {}},
    {inet_addr("224.0.58.178"), 12178, 0, 0, {}},
    {inet_addr("224.0.58.179"), 12179, 0, 0, {}},
    {inet_addr("224.0.58.180"), 12180, 0, 0, {}},
    {inet_addr("224.0.58.181"), 12181, 0, 0, {}},
    {inet_addr("224.0.58.182"), 12182, 0, 0, {}},
    {inet_addr("224.0.58.183"), 12183, 0, 0, {}},
    {inet_addr("224.0.58.184"), 12184, 0, 0, {}},
    {inet_addr("224.0.58.185"), 12185, 0, 0, {}},
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

inline auto findOrInstallStream(int grId, int streamId, uint32_t curSeq) {
    for (auto i = 0; i < group[grId].numStreams; i ++)
	if (group[grId].stream[i]->getId() == streamId) return i;
    if (group[grId].numStreams == MAX_STREAMS)
	on_error("numStreams == MAX_STREAMS (%u), cant add stream %u",
		 group[grId].numStreams,streamId);
    group[grId].stream[group[grId].numStreams] = new Stream(streamId,curSeq);
    if (group[grId].stream[group[grId].numStreams] == NULL)
	on_error("stream[%u] == NULL",group[grId].numStreams);
    return group[grId].numStreams++;
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

	/* ------------------------------------------------------------- */
	auto pktHdr {reinterpret_cast<const XdpPktHdr*>(p)};
	
	auto msgCnt   = EKA_XDP_MSG_CNT(p);
	auto sequence = EKA_XDP_SEQUENCE(p);
	auto streamId = EKA_XDP_STREAM_ID(p);
	auto pktType  = EKA_XDP_PKT_TYPE(p);

	auto streamIdx = findOrInstallStream(grId, streamId, sequence);

	auto stream = group[grId].stream[streamIdx];
	if (stream == NULL) on_error("stream == NULL");

	if (pktType == (uint8_t)DELIVERY_FLAG::SequenceReset)
	    stream->resetExpectedSeq();

	if (stream->getExpectedSeq() != 0 && stream->getExpectedSeq() != sequence) {
	    printf (RED "%d:%d expectedSeq %u != sequence %u\n" RESET,
		    grId,streamId,stream->getExpectedSeq(),sequence);
	}

	p += sizeof(*pktHdr);
	
	for (auto i = 0; i < msgCnt; i++) {
	    uint16_t msgLen = EKA_XDP_MSG_LEN(p);

	    //-----------------------------------------------------------------------------
	    auto msgType = reinterpret_cast<const XdpMsgHdr*>(p)->MsgType;
	    uint64_t ts = pktHdr->time.SourceTime * 1e9 + pktHdr->time.SourceTimeNS;
	    switch (msgType) {
	    case MSG_TYPE::REFRESH_QUOTE :
	    case MSG_TYPE::QUOTE : 
	    case MSG_TYPE::TRADE : 
	    case MSG_TYPE::SERIES_STATUS :
	    case MSG_TYPE::TRADE_CANCEL :
	    case MSG_TYPE::TRADE_CORRECTION :
	    case MSG_TYPE::IMBALANCE :
	    case MSG_TYPE::BOLD_RFQ :
	    case MSG_TYPE::SUMMARY :
	    case MSG_TYPE::UNDERLYING_STATUS :
	    case MSG_TYPE::REFRESH_TRADE :
	    case MSG_TYPE::REFRESH_IMBALANCE :
	    case MSG_TYPE::COMPLEX_QUOTE :
	    case MSG_TYPE::COMPLEX_TRADE :
	    case MSG_TYPE::COMPLEX_CROSSING_RFQ :
	    case MSG_TYPE::COMPLEX_STATUS :
	    case MSG_TYPE::COMPLEX_REFRESH_QUOTE :
	    case MSG_TYPE::COMPLEX_REFRESH_TRADE : {
		auto msgTimeHdr {reinterpret_cast<const XdpTimeHdr*>(p + sizeof(XdpMsgHdr))};
		ts = msgTimeHdr->SourceTime * 1e9 + msgTimeHdr->SourceTimeNS;
	    }
		break;
	    default:
	    	break;
	    }
	    if (printAll)
	    	printf ("%jd, %d:%d, %ju, %u, \'%u\'\n",
			pktNum,grId,streamId,ts,sequence,(uint)msgType);
	    sequence++;
	    //-----------------------------------------------------------------------------
	    stream->setExpectedSeq(sequence);

	    p += msgLen;
	}
	stream->setExpectedSeq(sequence);
	
    }
    return 0;
}
