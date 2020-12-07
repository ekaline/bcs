#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>
#include <pcap/pcap.h>

#include "eka_macros.h"
#include "smartnic.h"
#include "EkaHwCaps.h"
#include "EkaHwExpectedVersion.h"

volatile bool keep_work = true;
/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected: keep_work = false\n",__func__);
  printf ("%s: exitting...\n",__func__);
  fflush(stdout);
  return;
}
/* --------------------------------------------- */

struct PcapFileHdr {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t linktype;
};

struct PcapRecHdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t cap_len;
    uint32_t len;
};

static void printUsage(char* cmd) {
  printf("USAGE: %s <flags> \n",cmd); 
  printf("\t-i - FPGA lane number [0..3]\n"); 
  printf("\t-w - Output file path (Pcap format)\n"); 
  printf("\t-h - Print this help\n"); 
  return;
}

static int getAttr(int argc, char *argv[],
		   uint* coreId, char* fileName) {
  int opt; 
  while((opt = getopt(argc, argv, ":i:w:h")) != -1) {  
    switch(opt) {  
      case 'i':  
	*coreId = atoi(optarg);
	printf("coreId = %u\n", *coreId);
	break;  
      case 'w':  
	strcpy(fileName,optarg);
	printf("fileName = %s\n", fileName);
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
/* --------------------------------------------- */

static void eka_write(SC_DeviceId devId, uint64_t addr, uint64_t val) {
  if (SC_ERR_SUCCESS != SC_WriteUserLogicRegister(devId, addr/8, val))
    on_error("SN_Write(0x%jx,0x%jx) returned smartnic error code : %d",addr,val,SC_GetLastErrorCode());
}
/* --------------------------------------------- */
static void disableSniffer(SC_DeviceId devId) {
  eka_write(devId,0xf0360,0);
  TEST_LOG("Ekaline FPGA sniffer is disabled");
}

/* --------------------------------------------- */

static void enableSniffer(SC_DeviceId devId, uint coreId) {
  eka_write(devId,0xf0360, 0x1 << coreId);
  TEST_LOG("Ekaline FPGA sniffer is enabled for coredId %u (0x%x)",coreId,0x1 << coreId);
}

/* --------------------------------------------- */

int main(int argc, char *argv[]) {
  EkaHwCaps* ekaHwCaps = new EkaHwCaps(NULL);
  if (ekaHwCaps == NULL) on_error("ekaHwCaps == NULL");
    
  if (ekaHwCaps->hwCaps.version.sniffer != EKA_EXPECTED_SNIFFER_VERSION)
    on_error("This FW version does not support %s",argv[0]);

  uint coreId = -1;
  char fileName[256] = {};
  static const uint16_t SnifferUserChannelId = 7;
  signal(SIGINT, INThandler);

  getAttr(argc,argv,&coreId,fileName);

  if (coreId > 3) on_error("coreId %u is not supported",coreId);

  FILE* out_pcap_file = fopen(fileName, "w+");
  if (out_pcap_file == NULL) on_error("Ne mogu otkrit pcapFile file %s\n",fileName);


  PcapFileHdr pcapFileHdr = {
    .magic_number  = 0xa1b2c3d4,
    .version_major = 2,
    .version_minor = 4,
    .thiszone      = 0,
    .sigfigs       = 0,
    .snaplen       = 65535,
    .linktype      = 1
  };

  fwrite(&pcapFileHdr,1,sizeof(PcapFileHdr),out_pcap_file); 

  SC_DeviceId devId = SC_OpenDevice(NULL, NULL);
  if (devId == NULL) on_error("Cannot open Smartnic Device");
  disableSniffer(devId);

  SC_ChannelId  channelId = SC_AllocateUserLogicChannel(devId, SnifferUserChannelId,NULL);
  if (channelId == NULL) on_error("Cannot open Sniffer User channel %u",SnifferUserChannelId);

  uint64_t packetCount = 0;

  const SC_Packet * pPrevPacket = SC_GetNextPacket(channelId, NULL, SN_TIMEOUT_NONE);
  if (pPrevPacket != NULL) on_error("Packet is arriving on User channel before init");

  enableSniffer(devId,coreId);

  while (keep_work) {
    const SC_Packet * pPacket = SC_GetNextPacket(channelId, pPrevPacket, SN_TIMEOUT_NONE);
    if (pPacket == NULL) continue;

    const uint8_t * payload = SC_GetUserLogicPayload(pPacket);
    uint16_t payloadLength  = SC_GetUserLogicPayloadLength(pPacket);

    payload       += 32;
    payloadLength -= 32;

    /* +++++++++++++++++++++++++++++++++++++ */
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t); // 

    PcapRecHdr pcapRecHdr = {
      .ts_sec  = (uint32_t) t.tv_sec,
      .ts_usec = (uint32_t) (t.tv_nsec / 1000),
      .cap_len = payloadLength,
      .len     = payloadLength
    };
    

    fwrite(&pcapRecHdr,1,sizeof(PcapRecHdr),out_pcap_file); 
    fwrite(payload,1,payloadLength,out_pcap_file); 

    /* +++++++++++++++++++++++++++++++++++++ */

    if (++packetCount % 1024 == 0)
      if (SC_UpdateReceivePtr(channelId, pPacket) != SC_ERR_SUCCESS) on_error("error on SC_UpdateReceivePtr");

    if (++packetCount % 1000000 == 0)
      printf("%ju packets dumped\n",packetCount);

    pPrevPacket = pPacket;

  }
  disableSniffer(devId);
  fclose(out_pcap_file);
  TEST_LOG ("%ju packets dumped to %s",packetCount,fileName);
}
