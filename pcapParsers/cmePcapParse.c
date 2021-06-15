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

#include "eka_macros.h"
#include "cmePcapParse.h"

//###################################################

int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) on_error("Failed to open dump file %s",argv[1]);
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) 
    on_error ("Failed to read pcap_file_hdr from the pcap file");

  uint64_t pktNum = 0;
  //  char timeStamp[16] = {}; 
  //  char sequence[10] = {};

  EkaFhCmeGr* gr = new EkaFhCmeGr();
  if (gr == NULL) on_error("gr == NULL");

  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    uint pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536) 
      on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

    uint8_t pkt[1536] = {};
    if (fread(pkt,pktLen,1,pcap_file) != 1) 
      on_error ("Failed to read %d packet bytes",pktLen);
    ++pktNum;
    //    TEST_LOG("pkt = %ju, pktLen = %u",pktNum, pktLen);
    if (! EKA_IS_UDP_PKT(pkt)) continue;

    int pos = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
    //    pktLen -= 4;
    int16_t payloadLen = pktLen - (sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));
    gr->processPkt(NULL,&pkt[pos],payloadLen,0);
  }
  fprintf(stderr,"%ju packets processed\n",pktNum);
  fclose(pcap_file);

  return 0;
}
