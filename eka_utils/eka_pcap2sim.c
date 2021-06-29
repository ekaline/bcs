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


int main(int argc, char *argv[]) {
  char buf[1600] = {};
  FILE *pcap_file;

  if ((pcap_file = fopen(argv[1], "rb")) == NULL) {
    on_error("Failed to open dump file %s\n",argv[1]);
  }
  
  if (fread(buf,sizeof(pcap_file_hdr),1,pcap_file) != 1) 
    on_error ("Failed to read pcap_file_hdr from the pcap file");


  FILE* lenghFile = fopen("packet.length","wb");
  if (lenghFile == NULL) on_error("failed to create packet.length");
  
  uint64_t pktNum = 0;

  while (fread(buf,sizeof(pcap_rec_hdr),1,pcap_file) == 1) {
    pcap_rec_hdr *pcap_rec_hdr_ptr = (pcap_rec_hdr *) buf;
    int pktLen = pcap_rec_hdr_ptr->len;
    if (pktLen > 1536) on_error("Probably wrong PCAP format: pktLen = %d ",pktLen);

    uint64_t pkt[2000 / 8] = {};
    if (fread(pkt,pktLen,1,pcap_file) != 1) 
      on_error ("Failed to read %d packet bytes at pkt %ju",pktLen,pktNum);
    fprintf(lenghFile,"%x\n",pktLen);

    char packetFileName[100] = {};
    sprintf(packetFileName,"packet.readmemh%jd",pktNum);
    FILE* packetFile = fopen(packetFileName,"wb");
    if (packetFile == NULL) on_error("failed to create %s",packetFileName);

    for (auto i = 0; i < pktLen / 8 + 1; i++) {
      //      fprintf(packetFile,"%016jx\n",be64toh(pkt[i]));
      fprintf(packetFile,"%016jx\n",pkt[i]);
    }

    fclose(packetFile);
    pktNum++;

    

  }
  fclose(pcap_file);
  fclose(lenghFile);
  return 0;
}
