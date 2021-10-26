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

#include "EkaFhBoxParser.h"
#include "eka_macros.h"

using namespace Hsvf;


//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;
static bool printRfq = false;

struct GroupAddr {
  uint32_t  ip;
  uint16_t  port;
  char      timestamp[9];
  uint64_t  ts = 0;
  uint64_t  expectedSeq;
};

static GroupAddr group[] = {
    // Vanilla A
    {inet_addr("224.0.124.1"), 21401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.2"), 22401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.3"), 23401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.4"), 24401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.5"), 25401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.6"), 26401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.7"), 27401, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.8"), 28401, {'0','0','0','0','0','0','0','0','0'}, 0},

    // Vanilla B
    {inet_addr("224.0.124.49"), 21404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.50"), 22404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.51"), 23404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.52"), 24404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.53"), 25404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.54"), 26404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.55"), 27404, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.56"), 28404, {'0','0','0','0','0','0','0','0','0'}, 0},

    // RFQ (PIP) A   
    {inet_addr("224.0.124.17"), 21403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.18"), 22403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.19"), 23403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.20"), 24403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.21"), 25403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.22"), 26403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.23"), 27403, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.24"), 28403, {'0','0','0','0','0','0','0','0','0'}, 0},

    // Complex (Strategy) A   
    {inet_addr("224.0.124.25"), 21407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.26"), 22407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.27"), 23407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.28"), 24407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.29"), 25407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.30"), 26407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.31"), 27407, {'0','0','0','0','0','0','0','0','0'}, 0},
    {inet_addr("224.0.124.32"), 28407, {'0','0','0','0','0','0','0','0','0'}, 0},

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
  printf("          -r        Print RFQ messages\n");
}

//###################################################

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":f:d:prh")) != -1) {  
    switch(opt) {  
      case 'f':
	strcpy(pcapFileName,optarg);
	printf("pcapFile = %s\n", pcapFileName);  
	break;  
      case 'p':  
	printAll = true;
	printf("printAll\n");
	break;
      case 'r':  
	printRfq = true;
	printf("printRfq\n");
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
void printAuctionUpdateMsg(const EfhAuctionUpdateMsg* msg) {
  //  RfqTable5,date,time,QuoteID,Name,Security,Price,Size,Capacity,ExpirationTime,Side,Exchange,Type,ActionType,Customer,Imbalance
  //  RfqTable5,20210628,10:48:20.030.380,l6s01552,TSLA,TSLA__210702P00650000,5.5,67,C,,B,B,D,N,0333SF3,0
  static const int priceScaleFactor = 100;
  const char exchName = 'B';
  printf("RfqTable5,");
  /* printf("%s,",    eka_get_date().c_str()); */
  /* printf("%s,",    eka_get_time().c_str()); */
  printf("%ju,",   msg->auctionId);
  /* printf("%s,",    currClassSymbol.c_str()); */
  /* printf("%s,",    currAvtSecName.c_str()); */
  printf("%.*f,",  decPoints(msg->price,priceScaleFactor), ((float) msg->price / priceScaleFactor));
  printf("%u,",    msg->quantity);
  printf("%s,",   ts_ns2str(msg->endTimeNanos).c_str());
  printf("%c,", msg->side == EfhOrderSide::kBid ? 'B' : 'S');
  printf("%c,",    exchName);
  /* printf("%d,",    (int)msg->type); */
  /* printf("%d,",    (int)msg->customer); */
  /* printf("%d,",    (int)msg->securityType); */

  /* printf("%s,",    std::string(msg->execBroker,sizeof(msg->execBroker)).c_str()); */
  /* printf("%s,",    std::string(msg->client    ,sizeof(msg->client    )).c_str()); */
  /* printf("%s,",    (ts_ns2str(msg->header.timeStamp)).c_str()); */
  printf("\n");

  return;
}

//###################################################

void processRfqStart(const uint8_t* msgBody, uint8_t id, uint64_t sequence, uint64_t gr_ts) {
  auto boxMsg {reinterpret_cast<const HsvfRfqStart*>(msgBody)};

  uint64_t security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);

  EfhAuctionUpdateMsg msg{};
  msg.header.msgType        = EfhMsgType::kAuctionUpdate;
  msg.header.group.source   = EkaSource::kBOX_HSVF;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0;
  msg.header.securityId     = security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = gr_ts;

  msg.side                  = getSide(boxMsg->Side,false);
  msg.auctionId             = getNumField<uint32_t>(boxMsg->RfqId,sizeof(boxMsg->RfqId));

  msg.quantity              = getNumField<uint32_t>(boxMsg->Size, sizeof(boxMsg->Size));
  msg.price                 = getNumField<uint32_t>(boxMsg->Price,sizeof(boxMsg->Price));
  //  msg.endTimeNanos          = getExpireNs(boxMsg->ExpiryTime);
  msg.endTimeNanos          = 0; //PATCH

  printf("processRfqStart: ");
  printf("%s,",std::string(boxMsg->InstrumentDescription,sizeof(boxMsg->InstrumentDescription)).c_str());
  printf("%s,",std::string(boxMsg->RfqId,                sizeof(boxMsg->RfqId)).c_str());
  printf("%jd,",msg.price);
  printf("%s,",std::string(boxMsg->Size,                 sizeof(boxMsg->Size)).c_str());
  printf("\'%c\',",boxMsg->Side);
  printf("\n");
  printAuctionUpdateMsg(&msg);
  return;
}

//###################################################

void processRfqInsert(const uint8_t* msgBody, uint8_t id, uint64_t sequence, uint64_t gr_ts) {
  auto boxMsg {reinterpret_cast<const HsvfRfqInsert*>(msgBody)};

  uint64_t security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);

  EfhAuctionUpdateMsg msg{};
  msg.header.msgType        = EfhMsgType::kAuctionUpdate;
  msg.header.group.source   = EkaSource::kBOX_HSVF;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0;
  msg.header.securityId     = security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = gr_ts;

  msg.side                  = getSide(boxMsg->OrderSide,false);

  msg.quantity              = getNumField<uint32_t>(boxMsg->Size,sizeof(boxMsg->Size));
  msg.price                 = getNumField<uint32_t>(boxMsg->LimitPrice,sizeof(boxMsg->LimitPrice));
  /* msg.endTimeNanos          = getExpireNs(boxMsg->EndOfExposition); */
  /* msg.endTimeNanos          = getExpireNs(&localTimeComponents, boxMsg->ExpiryTime); */
  msg.endTimeNanos          = 0; //PATCH
  
  if (boxMsg->OrderType == 'A') { // Initial
    msg.auctionId             = getNumField<uint32_t>(boxMsg->RfqId,sizeof(boxMsg->RfqId));
  } else if (boxMsg->OrderType == 'P') { // Exposed
    msg.auctionId             = getNumField<uint32_t>(boxMsg->OrderSequence,sizeof(boxMsg->OrderSequence));
  } else {
    on_error("Unexpected OrderType == \'%c\'",boxMsg->OrderType);
  }
  
  printf("processRfqInsert: ");
  printf("%s,",std::string(boxMsg->InstrumentDescription,sizeof(boxMsg->InstrumentDescription)).c_str());
  printf("%s,",std::string(boxMsg->RfqId,                sizeof(boxMsg->RfqId)).c_str());
  printf("%jd,",msg.price);
  printf("%d,",msg.quantity);
  printf("\'%c\',",boxMsg->OrderSide);
  printf("\n");
  printAuctionUpdateMsg(&msg);
  return;
}

//###################################################

void processRfqDelete(const uint8_t* msgBody, uint8_t id, uint64_t sequence, uint64_t gr_ts) {
  auto boxMsg {reinterpret_cast<const HsvfRfqDelete*>(msgBody)};

  uint64_t security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);

  EfhAuctionUpdateMsg msg{};
  msg.header.msgType        = EfhMsgType::kAuctionUpdate;
  msg.header.group.source   = EkaSource::kBOX_HSVF;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0;
  msg.header.securityId     = security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = gr_ts;

  msg.side                  = getSide(boxMsg->OrderSide,false);
  msg.auctionId             = getNumField<uint32_t>(boxMsg->RfqId,sizeof(boxMsg->RfqId));

  msg.quantity              = 0;
  msg.price                 = 0;
  msg.endTimeNanos          = 0;
  
  printf("processRfqDelete: ");
  printf("%s,",std::string(boxMsg->InstrumentDescription,sizeof(boxMsg->InstrumentDescription)).c_str());
  printf("%s,",std::string(boxMsg->RfqId,                sizeof(boxMsg->RfqId)).c_str());
  printf("%jd,",msg.price);
  printf("%d,",msg.quantity);
  printf("\'%c\',",boxMsg->OrderSide);
  printf("\'%c\',",boxMsg->AuctionType);
  printf("\n");
  printAuctionUpdateMsg(&msg);
  return;
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
	auto udpHdr {reinterpret_cast<const EkaUdpHdr*>(p)};

	auto payloadLen {static_cast<int>(be16toh(udpHdr->len) - sizeof(*udpHdr))};

	p += sizeof(EkaUdpHdr);
	auto payloadStart {p};
	printf ("\n--------------------%d, %s:%u Pkt %ju\n",
		grId,EKA_IP2STR(group[grId].ip),group[grId].port,pktNum);
/* ----------------------------------------- */
	while (p - payloadStart < payloadLen) {
	    if (*p != HsvfSom) {
		hexDump("Pkt with no HsvfSom",pkt,payloadLen);
		on_error("at payloadPos = %jd expected HsvfSom (0x%x) != 0x%x",
			 p - payloadStart,HsvfSom,*p & 0xFF);
	    }

	    auto msgLen   {getMsgLen     (p,payloadLen-(p - payloadStart))};
	    auto sequence {getMsgSequence(p)};


	    auto m {p + sizeof(HsvfSom)};
	    auto msgHdr   {reinterpret_cast<const HsvfMsgHdr*>(m)};

	    m += sizeof(*msgHdr);
	    
	    if (memcmp(msgHdr->MsgType,"Z ",sizeof(msgHdr->MsgType)) == 0) { // SystemTimeStamp
	      auto msg {reinterpret_cast<const HsvfSystemTimeStamp*>(p + sizeof(HsvfSom) + sizeof(*msgHdr))};
	      memcpy(group[grId].timestamp,msg->TimeStamp,sizeof(msg->TimeStamp));

	      const char* timeStamp = msg->TimeStamp;
	      uint64_t hour = getNumField<uint64_t>(&timeStamp[0],2);
	      uint64_t min  = getNumField<uint64_t>(&timeStamp[2],2);
	      uint64_t sec  = getNumField<uint64_t>(&timeStamp[4],2);
	      uint64_t ms   = getNumField<uint64_t>(&timeStamp[6],3);
    
	      group[grId].ts = ((hour * 3600 + min * 60 + sec) * 1000 + ms) * 1000000;
	    } 

	    printf("\t%8ju,",sequence);
	    printf("%c%c:%c%c:%c%c.%c%c%c,",
		   group[grId].timestamp[0],group[grId].timestamp[1],
		   group[grId].timestamp[2],group[grId].timestamp[3],
		   group[grId].timestamp[4],group[grId].timestamp[5],
		   group[grId].timestamp[6],group[grId].timestamp[7],group[grId].timestamp[8]);
	    printf("\'%c%c\'\n",msgHdr->MsgType[0],msgHdr->MsgType[1]);

	    if (printRfq) {
	      if (memcmp(msgHdr->MsgType,"M ",sizeof(msgHdr->MsgType)) == 0) // HsvfRfqStart
		processRfqStart(m,grId,sequence,group[grId].ts);
	      if (memcmp(msgHdr->MsgType,"O ",sizeof(msgHdr->MsgType)) == 0) // HsvfRfqInsert
	      	processRfqInsert(m,grId,sequence,group[grId].ts);
	      if (memcmp(msgHdr->MsgType,"T ",sizeof(msgHdr->MsgType)) == 0) // HsvfRfqDelete
	      	processRfqDelete(m,grId,sequence,group[grId].ts);
	    }
	    
	    group[grId].expectedSeq = sequence + 1;

	    p += msgLen;
	    p += trailingZeros(p, payloadLen-(p - payloadStart));
	}	
    }
    return 0;
}
