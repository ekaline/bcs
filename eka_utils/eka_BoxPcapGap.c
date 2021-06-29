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

//#include "ekaNW.h"
#include "eka_macros.h"

#include "eka_hsvf_box_messages4pcapTest.h"

/* #define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); } */

/* #define EKA_IP2STR(x)  ((std::to_string((x >> 0) & 0xFF) + '.' + std::to_string((x >> 8) & 0xFF) + '.' + std::to_string((x >> 16) & 0xFF) + '.' + std::to_string((x >> 24) & 0xFF)).c_str()) */


#define EKA_ETHTYPE_VLAN      0x8100
#define EKA_ETHER_VLAN(pkt) (EKA_ETH_TYPE(pkt) == EKA_ETHTYPE_VLAN)

//#########################################################
uint getHsvfMsgLen(const uint8_t* pkt, int bytes2run) {
  uint idx = 0;
  if (pkt[idx] != HsvfSom) {
    hexDump("Msg with no HsvfSom (0x2)",
	    (void*)pkt,bytes2run);
    on_error("0x%x met while HsvfSom 0x%x is expected",pkt[idx] & 0xFF,HsvfSom);
    return 0;
  }
  do {
    idx++;
    if ((int)idx > bytes2run) {
      hexDump("Msg with no HsvfEom (0x3)",(void*)pkt,bytes2run);
      TEST_LOG("bytes2run=%d",bytes2run);
      on_error("HsvfEom not met after %u characters",idx);
    }
  } while (pkt[idx] != HsvfEom);
  return idx + 1;
}
//###################################################

uint64_t getHsvfMsgSequence(const uint8_t* msg) {
  HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&msg[1];
  std::string seqString = std::string(msgHdr->sequence,sizeof(msgHdr->sequence));
  return std::stoul(seqString,nullptr,10);
}
//###################################################

uint trailingZeros(const uint8_t* p, uint maxChars) {
  uint idx = 0;
  while (p[idx] == 0x0 && idx < maxChars) {
    idx++; // skipping trailing '\0' chars
  }
  return idx;
}
//###################################################

inline int skipChar(const uint8_t* s, char char2skip) {
  int p = 0;
  while (s[p++] != char2skip) {}
  return p;
}

//###################################################
  struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    char      timestamp[64] = {};
    char      msgTimestamp[64] = {};
    uint64_t  expectedSeq;
    uint hour = 0;
  };

  GroupAddr group[] = {
    {inet_addr("224.0.124.1"), 21401},
    {inet_addr("224.0.124.2"), 22401},
    {inet_addr("224.0.124.3"), 23401},
    {inet_addr("224.0.124.4"), 24401},
    {inet_addr("224.0.124.5"), 25401},
    {inet_addr("224.0.124.6"), 26401},
    {inet_addr("224.0.124.7"), 27401},
    {inet_addr("224.0.124.8"), 28401},

    {inet_addr("224.0.124.49"), 21404},
    {inet_addr("224.0.124.50"), 22404},
    {inet_addr("224.0.124.51"), 23404},
    {inet_addr("224.0.124.52"), 24404},
    {inet_addr("224.0.124.53"), 25404},
    {inet_addr("224.0.124.54"), 26404},
    {inet_addr("224.0.124.55"), 27404},
    {inet_addr("224.0.124.56"), 28404},
  };
//###################################################

int findGrp(uint32_t ip, uint16_t port) {
  for (auto i = 0; i < (int)(sizeof(group)/sizeof(group[0])); i++) {
    if (group[i].ip == ip && group[i].port == port)
      return i;
  }
  //  on_error("%s:%u is not found",EKA_IP2STR(ip),port);
  return -1;
}

//###################################################
void printUsage(char* cmd) {
  printf("USAGE: %s -f [pcapFile]\n",cmd);
  printf("          \t\t\t\t-p     -- printAll\n");
  printf("          \t\t\t\t-d [number of Pkt to dump]\n");
}
//###################################################

static bool     printAll = false;
static uint64_t pkt2dump = -1;
static char     pcapFile[256] = {};

//###################################################

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":f:d:ph")) != -1) {  
    switch(opt) {  
      case 'f':
	strcpy(pcapFile,optarg);
	printf("pcapFile = %s\n", pcapFile);  
	break;  
      case 'p':  
	printAll = true;
	printf("printAll\n");
	break;  
      case 'd':  
	pkt2dump = atoi(optarg);
	printf("pkt2dump = %ju\n",pkt2dump);  
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
  FILE *pcap_file;
  getAttr(argc,argv);

  if ((pcap_file = fopen(pcapFile, "rb")) == NULL) {
    printf("Failed to open dump file %s\n",pcapFile);
    printUsage(argv[0]);
    exit(1);
  }
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) 
    on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pktNum = 0;
  uint startHour = 0;
  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    uint pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536) on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

    char pkt[1536] = {};
    if (fread(pkt,pktLen,1,pcap_file) != 1) 
      on_error ("Failed to read %d packet bytes at pkt %ju",pktLen,pktNum);
    pktNum++;
    //    TEST_LOG("pkt = %ju, pktLen = %u",pktNum, pktLen);

    int pos = 0;
    if (EKA_ETHER_VLAN(pkt)) pos += 4;
    if (! EKA_IS_UDP_PKT(&pkt[pos])) {
      continue;
    }

    int gr = findGrp(EKA_IPH_DST(&pkt[pos]),EKA_UDPH_DST(&pkt[pos]));
    if (gr < 0) continue;
    //    if (group[gr].hour > startHour) printf ("%s:%u\n",EKA_IP2STR(EKA_IPH_DST(&pkt[pos])),EKA_UDPH_DST(&pkt[pos]));

    auto udpHdr {reinterpret_cast<const EkaUdpHdr*>(&pkt[pos + sizeof(EkaEthHdr) + sizeof(EkaIpHdr)])};
    int payloadLen = be16toh(udpHdr->len) - sizeof(EkaUdpHdr);

    int payloadPos = 0;//
    
    const uint8_t* payload = (const uint8_t*)&pkt[pos + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr)];

    if (pktNum == pkt2dump) {
      TEST_LOG("pktLen = %d, payloadLen=%d",pktLen,payloadLen);
      hexDump("pkt2dump",pkt,pktLen);
    }
    if (printAll) printf ("\n--------------------%d, %s:%u Pkt %ju\n",gr,EKA_IP2STR(group[gr].ip),group[gr].port,pktNum);
    //###############################################
    while (payloadPos < payloadLen) {
      if (payload[payloadPos] != HsvfSom) {
	hexDump("Pkt with no HsvfSom",pkt,payloadLen);
	on_error("at payloadPos = %d expected HsvfSom (0x%x) != 0x%x",
		 payloadPos,HsvfSom,payload[payloadPos] & 0xFF);
      }
      uint msgLen       = getHsvfMsgLen(&payload[payloadPos],payloadLen-payloadPos);
      uint64_t sequence = getHsvfMsgSequence(&payload[payloadPos]);

      HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&payload[payloadPos+1];

      /* -------------------------------- */
      if (printAll)
	printf("\t%8ju, %s \'%c%c\'",sequence,group[gr].timestamp,msgHdr->MsgType[0],msgHdr->MsgType[1]);
      /* -------------------------------- */
      if (group[gr].expectedSeq != 0 && group[gr].expectedSeq < sequence) {
	if (printAll) 
	  printf (" --- expected %ju != actual %ju\n",
		  group[gr].expectedSeq,sequence);
	else
	  printf ("%d, %s:%u %s: expected %ju != actual %ju\n",
		  gr, EKA_IP2STR(group[gr].ip),group[gr].port, group[gr].timestamp,
		  group[gr].expectedSeq, sequence);
      } else {
	if (printAll) printf("\n");
      }
      group[gr].expectedSeq = sequence + 1;

      /* -------------------------------- */

      if (memcmp(msgHdr->MsgType,"Z ",sizeof(msgHdr->MsgType)) == 0) { // SystemTimeStamp
      	SystemTimeStamp* msg = (SystemTimeStamp*)&payload[payloadPos+sizeof(HsvfMsgHdr)+1];
	group[gr].hour =  10 * (msg->TimeStamp[0] - '0') + (msg->TimeStamp[1] - '0');
	if (group[gr].hour > startHour) {
	  sprintf (group[gr].timestamp,"%c%c:%c%c:%c%c.%c%c%c",
		   msg->TimeStamp[0],msg->TimeStamp[1],msg->TimeStamp[2],msg->TimeStamp[3],msg->TimeStamp[4],msg->TimeStamp[5],
		   msg->TimeStamp[6],msg->TimeStamp[7],msg->TimeStamp[8]
		   );
	  memcpy(group[gr].msgTimestamp,msg->TimeStamp,sizeof(msg->TimeStamp));
	  if (printAll) printf("%d: %s:%u %ju, |%c%c|,|%s| == |%s|\n",
		 gr,
		 EKA_IP2STR(group[gr].ip),group[gr].port,
		 sequence,
		 msgHdr->MsgType[0],msgHdr->MsgType[1],
		 group[gr].timestamp,group[gr].msgTimestamp);
	}
      } 
      /* -------------------------------- */

      payloadPos += msgLen;
      payloadPos += trailingZeros(&payload[payloadPos],payloadLen-payloadPos );
    }

  }
  fprintf(stderr,"%ju packets processed\n",pktNum);
  fclose(pcap_file);

  return 0;
}
