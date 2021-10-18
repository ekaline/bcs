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

#include "EkaFhBatsParser.h"
#include "eka_macros.h"

static int printBatsMsg(uint64_t pktNum,uint8_t* msg, uint64_t sequence);

using namespace Bats;

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
    pos          += sizeof(sequenced_unit_header);;
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

static int printBatsMsg(uint64_t pktNum,uint8_t* msg, uint64_t sequence) {
  MsgId enc =  (MsgId)msg[1];
  printf("%10ju,%10ju,%s(0x%x),",pktNum,sequence,EKA_BATS_PITCH_MSG_DECODE(enc),(uint8_t)enc);

  switch (enc) {
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_LONG: {
    auto m {reinterpret_cast<add_order_long*>(msg)};
    
    printf ("SID:\'%s\'(0x%016jx),\'%c\',P:%8ju,S:%8u",
	    EKA_PRINT_BATS_SYMBOL(m->symbol),symbol2secId(m->symbol),
	    m->side,m->price,m->size
	    );
  }
    break;
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_SHORT: {
    auto m {reinterpret_cast<add_order_short*>(msg)};
    
    printf ("SID:\'%s\'(0x%016jx),\'%c\',P:%8d,S:%8d",
	    EKA_PRINT_BATS_SYMBOL(m->symbol),symbol2secId(m->symbol),
	    m->side,m->price * 100,m->size
	    );
  }
    break;
    //--------------------------------------------------------------
  case MsgId::ADD_ORDER_EXPANDED: {
    auto m {reinterpret_cast<add_order_expanded*>(msg)};
    
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
