#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "ekaNW.h"
#include "eka_macros.h"
#include "EkaFhBatsParser.h"

static int printBatsMsg(uint64_t pktNum,uint8_t* msg, uint64_t sequence);

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

int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) 
    on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pktNum = 0;

  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    uint pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536) 
      on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

    uint8_t pkt[1536] = {};
    if (fread(pkt,pktLen,1,pcap_file) != 1) on_error ("Failed reading %d bytes",pktLen);
    ++pktNum;
    //    TEST_LOG("pkt = %ju, pktLen = %u",pktNum, pktLen);
    if (! EKA_IS_UDP_PKT(pkt)) continue;

    int pos = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    //    int16_t payloadLen = pktLen - (sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));

    auto msgInPkt = EKA_BATS_MSG_CNT((&pkt[pos]));
    auto sequence = EKA_BATS_SEQUENCE((&pkt[pos]));
    pos          += sizeof(batspitch_sequenced_unit_header);;
    for (uint msg=0; msg < msgInPkt; msg++) {
      uint8_t msgLen = pkt[pos];
      printBatsMsg(pktNum,&pkt[pos],sequence++);
      pos += msgLen;
    }

  }
  fprintf(stderr,"%ju packets processed\n",pktNum);
  fclose(pcap_file);

  return 0;
}

inline uint64_t symbol2secId(const char* s) {
  return be64toh(*(uint64_t*)(s - 2)) & 0x0000ffffffffffff;
}
inline uint64_t expSymbol2secId(const char* s) {
  if (s[6] != ' ' || s[7] != ' ')
    on_error("ADD_ORDER_EXPANDED message with \'%c%c%c%c%c%c%c%c\' symbol (longer than 6 chars) not supported",
	     s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7]);
  return be64toh(*(uint64_t*)(s - 2)) & 0x0000ffffffffffff;
}

static int printBatsMsg(uint64_t pktNum,uint8_t* msg, uint64_t sequence) {
  EKA_BATS_PITCH_MSG enc = (EKA_BATS_PITCH_MSG)msg[1];
  printf("%10ju,%10ju,%s(0x%x),",pktNum,sequence,EKA_BATS_PITCH_MSG_DECODE(enc),(uint8_t)enc);

  switch (enc) {
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_LONG: {
    auto m {reinterpret_cast<batspitch_add_order_long*>(msg)};
    
    printf ("SID:\'%s\'(0x%016jx),\'%c\',P:%8ju,S:%8u",
	    EKA_PRINT_BATS_SYMBOL(m->symbol),symbol2secId(m->symbol),
	    m->side,m->price,m->size
	    );
  }
    break;
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_SHORT: {
    auto m {reinterpret_cast<batspitch_add_order_short*>(msg)};
    
    printf ("SID:\'%s\'(0x%016jx),\'%c\',P:%8d,S:%8d",
	    EKA_PRINT_BATS_SYMBOL(m->symbol),symbol2secId(m->symbol),
	    m->side,m->price * 100,m->size
	    );
  }
    break;
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_EXPANDED: {
    auto m {reinterpret_cast<batspitch_add_order_expanded*>(msg)};
    
    printf ("SID:\'%s\'(0x%016jx),\'%c\',P:%8ju,S:%8u",
	    EKA_PRINT_BATS_SYMBOL_EXP(m->exp_symbol),expSymbol2secId(m->exp_symbol),
	    m->side,m->price,m->size
	    );
  }
    break;


  default:
    //    fprintf(md_file,"\n");
    break;
  }
  printf("\n");
  return 0;
}
