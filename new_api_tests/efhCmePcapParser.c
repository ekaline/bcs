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

#include "ekaNW.h"
#include "eka_macros.h"

#include "eka_hsvf_box_messages4pcapTest.h"

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
struct PktHdr {
  uint32_t seq;
  uint64_t time;
} __attribute__((packed));
  
enum class MsgId : uint16_t {
  MDIncrementalRefreshBook                = 46, 
    MDIncrementalRefreshOrderBook         = 47, 
    MDIncrementalRefreshVolume            = 37, 
    MDIncrementalRefreshTradeSummary      = 48, 
    MDIncrementalRefreshSessionStatistics = 51,
    Unknown                               = 101
    };

struct MsgHdr {
  uint16_t       size;
  uint16_t       blockLen;
  MsgId          templateId;
  uint16_t       schemaId;
  uint16_t       version;
} __attribute__((packed));


struct RootBlock { // 11
  uint64_t    TransactTime;       // 8
  uint8_t     MatchEventIndicator;// 1
  uint8_t     pad[2];             // 2
} __attribute__((packed));

struct GroupSize {
  uint16_t blockLen;
  uint8_t  numInGroup;
} __attribute__((packed));

struct MdEntry { // 32 
  uint64_t price;                 // 8
  uint32_t size;                  // 4
  uint32_t SecurityId;            // 4
  uint32_t seq;                   // 4
  uint32_t numOrders;             // 4
  uint8_t  priceLevel;            // 1
  uint8_t  MDUpdateAction;        // 1
  char     MDEntryType;           // 1
  uint8_t  pad[5];                // 5
} __attribute__((packed));


#define EKA_CME_SIDE(x) \
    x == '0'   ? "Offer" : \
    x == '1'   ? "Ask  " : \
    x == '2'   ? "Trade" : \
    x == '4'   ? "OpenPrice            " : \
    x == '6'   ? "SettlementPrice      " : \
    x == '7'   ? "TradingSessionHighPrice" : \
    x == '8'   ? "TradingSessionLowPrice " : \
    x == 'B'   ? "ClearedVolume       " : \
    x == 'C'   ? "OpenInterest        " : \
    x == 'E'   ? "ImpliedBid          " : \
    x == 'F'   ? "ImpliedOffer        " : \
    x == 'J'   ? "BookReset           " : \
    x == 'N'   ? "SessionHighBid      " : \
    x == 'O'   ? "SessionLowOffer     " : \
    x == 'W'   ? "FixingPrice         " : \
    x == 'e'   ? "ElectronicVolume    " : \
    x == 'g'   ? "ThresholdLimitsandPriceBandVariation" : \
    "UNEXPECTED"


#define EKA_CME_ACTION(x) \
  x == '0'   ? "New       " : \
    x == '1' ? "Change    " : \
    x == '2' ? "Delete    " : \
    x == '3' ? "DeleteThru" : \
    x == '4' ? "DeleteFrom" : \
    x == '5' ? "Overlay   " : \
    "UNEXPECTED"

//###################################################

#if 1
static void hexDump (const char* desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    if (desc != NULL) printf("%s:\n", desc);
    if (len == 0) { printf("  ZERO LENGTH\n"); return; }
    if (len < 0)  { printf("  NEGATIVE LENGTH: %i\n",len); return; }
    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) printf("  %s\n", buff);
            printf("  %04x ", i);
        }
        printf(" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))  buff[i % 16] = '.';
        else buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) { printf("   "); i++; }
    printf("  %s\n", buff);
}
#endif
//#########################################################

inline int skipChar(char* s, char char2skip) {
  int p = 0;
  while (s[p++] != char2skip) {}
  return p;
}

//###################################################

int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) 
    on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pktNum = 0;
  char timeStamp[16] = {}; 
  //  char sequence[10] = {};
  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    uint pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536) on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

    char pkt[1536] = {};
    if (fread(pkt,pktLen,1,pcap_file) != 1) on_error ("Failed to read %d packet bytes",pktLen);
    ++pktNum;
    //    TEST_LOG("pkt = %ju, pktLen = %u",pktNum, pktLen);
    if (! EKA_IS_UDP_PKT(pkt)) continue;

    int pos = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    pktLen -= 4;

    const PktHdr* pktHdr = (const PktHdr*)&pkt[pos];
    hexDump("pkt",pkt,pktLen);
    printf ("##############\n%8ju: %8u, %20ju\n",pktNum,pktHdr->seq,pktHdr->time);
    pos += sizeof(PktHdr);
    while (pos < (int)pktLen) {
      const MsgHdr* msgHdr = (const MsgHdr*)&pkt[pos];
      printf ("size=%u,blockLen=%u,MsgId=%d\n",
	      msgHdr->size,msgHdr->blockLen,msgHdr->templateId);
      switch (msgHdr->templateId) {
      case MsgId::MDIncrementalRefreshBook : {
	uint msgPos = pos;
	/* ------------------------------- */
	uint rootBlockPos = msgPos + msgHdr->blockLen;
	const RootBlock* rootBlock = (const RootBlock*)&pkt[msgPos];
	printf ("IncrementalRefreshBook: TransactTime=%ju, MatchEventIndicator=0x%x\n",
		rootBlock->TransactTime,rootBlock->MatchEventIndicator);
	/* ------------------------------- */
	uint groupSizePos = rootBlockPos + msgHdr->blockLen;
	const GroupSize* groupSize = (const GroupSize*)&pkt[groupSizePos];
	/* ------------------------------- */
	uint entryPos = groupSizePos + sizeof(GroupSize);
	for (uint i = 0; i < groupSize->numInGroup; i++) {
	  const MdEntry* e = (const MdEntry*) &pkt[entryPos];
	  printf("\t\tsecId=%8u,%s,%s,plvl=%u,p=%8ju,s=%u\n",
		 e->SecurityId,
		 EKA_CME_SIDE(e->MDUpdateAction),
		 EKA_CME_SIDE(e->MDEntryType),
		 e->priceLevel,
		 e->price,
		 e->size);
	  entryPos += groupSize->blockLen;
	}
	pos += msgHdr->size;
      }
	break;
      default:
	printf ("MsgId = \'%d\'\n",(int)msgHdr->templateId);
      }
    }
  }
  fprintf(stderr,"%ju packets processed\n",pktNum);
  fclose(pcap_file);

  return 0;
}
