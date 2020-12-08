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
//#include "eka_macros.h"

#include "eka_hsvf_box_messages4pcapTest.h"

#define on_error(...) { fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n");perror(""); fflush(stdout); fflush(stderr); exit(1); }

#define EKA_IP2STR(x)  ((std::to_string((x >> 0) & 0xFF) + '.' + std::to_string((x >> 8) & 0xFF) + '.' + std::to_string((x >> 16) & 0xFF) + '.' + std::to_string((x >> 24) & 0xFF)).c_str())

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

struct EkaEthHdr {
  uint8_t dest[6];
  uint8_t src[6];
  uint16_t type;
};

struct EkaIpHdr {
  /* version / header length */
  uint8_t _v_hl;
  /* type of service */
  uint8_t _tos;
  /* total length */
  uint16_t _len;
  /* identification */
  uint16_t _id;
  /* fragment offset field */
  uint16_t _offset;
/* #define IP_RF 0x8000U        /\* reserved fragment flag *\/ */
/* #define IP_DF 0x4000U        /\* don't fragment flag *\/ */
/* #define IP_MF 0x2000U        /\* more fragments flag *\/ */
#define EKA_IP_OFFMASK 0x1fffU   /* mask for fragmenting bits */
  /* time to live */
  uint8_t _ttl;
  /* protocol*/
  uint8_t _proto;
  /* checksum */
  uint16_t _chksum;
  /* source and destination IP addresses */
  uint32_t src;
  uint32_t dest;
};

struct EkaTcpHdr {
  uint16_t src;
  uint16_t dest;
  uint32_t seqno;
  uint32_t ackno;
  uint16_t _hdrlen_rsvd_flags;
  uint16_t wnd;
  uint16_t chksum;
  uint16_t urgp;
};

struct EkaUdpHdr {
  uint16_t src;
  uint16_t dest;
  uint16_t len;
  uint16_t chksum;
};

#define EKA_ETHTYPE_IP        0x0800
#define EKA_ETHTYPE_VLAN      0x8100


#define EKA_PROTO_TCP         0x06
#define EKA_PROTO_UDP         0x11

#define EKA_IPH_PROTO(hdr) ((hdr)->_proto)

#define EKA_ETH_TYPE(pkt)  ((uint16_t)be16toh((((EkaEthHdr*)pkt)->type)))

#define EKA_ETHER_VLAN(pkt) (EKA_ETH_TYPE(pkt) == EKA_ETHTYPE_VLAN)

#define EKA_IPH(buf)  ((EkaIpHdr*)  (((uint8_t*)buf) + sizeof(EkaEthHdr) + 4 * EKA_ETHER_VLAN(buf)))
#define EKA_TCPH(buf) ((EkaTcpHdr*) (((uint8_t*)EKA_IPH(buf) + sizeof(EkaIpHdr))))
#define EKA_UDPH(buf) ((EkaUdpHdr*) (((uint8_t*)EKA_IPH(buf) + sizeof(EkaIpHdr))))


#define EKA_ETH_TYPE(pkt)  ((uint16_t)be16toh((((EkaEthHdr*)pkt)->type)))
#define EKA_ETH_MACDA(pkt) ((uint8_t*)&(((EkaEthHdr*)pkt)->dest))
#define EKA_ETH_MACSA(pkt) ((uint8_t*)&(((EkaEthHdr*)pkt)->src))

#define EKA_IS_IP4_PKT(pkt) (EKA_ETH_TYPE(pkt) == EKA_ETHTYPE_IP)
#define EKA_IS_UDP_PKT(pkt) (EKA_IS_IP4_PKT(pkt) && (EKA_IPH_PROTO(EKA_IPH(pkt)) == EKA_PROTO_UDP))


#define EKA_IPH_SRC(hdr) ((EKA_IPH(hdr))->src)
#define EKA_IPH_DST(hdr) ((EKA_IPH(hdr))->dest)

#define EKA_UDPH_SRC(pkt) ((uint16_t)(be16toh((EKA_UDPH(pkt))->src)))
#define EKA_UDPH_DST(pkt) ((uint16_t)(be16toh((EKA_UDPH(pkt))->dest)))


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
uint getHsvfMsgLen(char* pkt, int bytes2run) {
  uint idx = 0;
  if (pkt[idx] != HsvfSom) {
    hexDump("Msg with no HsvfSom (0x2)",(void*)pkt,bytes2run);
    on_error("0x%x met while HsvfSom 0x%x is expected",pkt[idx] & 0xFF,HsvfSom);
    return 0;
  }
  do {
    idx++;
    if ((int)idx > bytes2run) {
      hexDump("Msg with no HsvfEom (0x3)",(void*)pkt,bytes2run);
      on_error("HsvfEom not met after %u characters",idx);
    }
  } while (pkt[idx] != HsvfEom);
  return idx + 1;
}
//###################################################

uint64_t getHsvfMsgSequence(char* msg) {
  HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&msg[1];
  std::string seqString = std::string(msgHdr->sequence,sizeof(msgHdr->sequence));
  return std::stoul(seqString,nullptr,10);
}
//###################################################

uint trailingZeros(char* p, uint maxChars) {
  uint idx = 0;
  while (p[idx] == 0x0 && idx < maxChars) {
    idx++; // skipping trailing '\0' chars
  }
  return idx;
}
//###################################################

inline int skipChar(char* s, char char2skip) {
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
    {inet_addr("224.0.124.2"), 21401},
    {inet_addr("224.0.124.3"), 21401},
    {inet_addr("224.0.124.4"), 21401},
    {inet_addr("224.0.124.5"), 21401},
    {inet_addr("224.0.124.6"), 21401},
    {inet_addr("224.0.124.7"), 21401},
    {inet_addr("224.0.124.8"), 21401},

    {inet_addr("224.0.124.49"), 21404},
    {inet_addr("224.0.124.50"), 21404},
    {inet_addr("224.0.124.51"), 21404},
    {inet_addr("224.0.124.52"), 21404},
    {inet_addr("224.0.124.53"), 21404},
    {inet_addr("224.0.124.54"), 21404},
    {inet_addr("224.0.124.55"), 21404},
    {inet_addr("224.0.124.56"), 21404},
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

int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  bool printAll = argc > 2;
  printf ("argc = %d\n",argc);

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
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


    pos += sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    if (printAll) printf ("%d, %s:%u Pkt %ju\n--------------------\n",gr,EKA_IP2STR(group[gr].ip),group[gr].port,pktNum);
    //###############################################
    while (pos < (int)pktLen) {
      if (pkt[pos] != HsvfSom) {
	hexDump("Pkt with no HsvfSom",&pkt[pos],pktLen);
	on_error("expected HsvfSom (0x%x) != 0x%x",HsvfSom,pkt[pos] & 0xFF);
      }
      uint msgLen       = getHsvfMsgLen(&pkt[pos],pktLen-pos);
      uint64_t sequence = getHsvfMsgSequence(&pkt[pos]);

      HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&pkt[pos+1];

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
      	SystemTimeStamp* msg = (SystemTimeStamp*)&pkt[pos+sizeof(HsvfMsgHdr)+1];
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

      pos += msgLen;
      if (msgLen - pos == 4) break;
      pos += trailingZeros(&pkt[pos],pktLen-pos );
    }

  }
  fprintf(stderr,"%ju packets processed\n",pktNum);
  fclose(pcap_file);

  return 0;
}
