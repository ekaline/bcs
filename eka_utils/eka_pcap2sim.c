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
