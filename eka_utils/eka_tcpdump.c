#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>

#include "eka_macros.h"
#include "smartnic.h"
#include "EkaHwCaps.h"
#include "EkaHwExpectedVersion.h"

volatile bool keep_work = true;
/* --------------------------------------------- */

static void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected\n",__func__);
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

/* struct SnifferHdr { */
/*     uint8_t  padding[13]; */
/*     uint64_t dmaIndex; */
/*     uint64_t fpgaTimeCycles; */
/*     uint8_t  coreSource; */
/*     uint8_t  bitParams; */
/*     uint8_t  dmaType; */
/* } __attribute__((packed)); */

struct SnifferHdr {
    uint8_t  dmaType;
    uint8_t  bitParams;
    uint8_t  coreSource;
    uint64_t fpgaTimeCycles;
    uint64_t dmaIndex;
    uint8_t  padding[13];
} __attribute__((packed));

static const uint FpgaClockPeriod = 4; // ns : core clock 166mhz

static void printUsage(char* cmd) {
    printf("USAGE: %s <options> \n",cmd); 
    printf("\t-b - FPGA lane bitmap (in decimal) 1..15\n"); 
    printf("\t-i - FPGA lane number [0..3] -- ignored if \'-b\' set\n"); 
    printf("\t-t - use FPGA time stamps (PCIe 250MHz clock) for delta latency only\n"); 
    printf("\t-w - Output file path (Pcap format)\n"); 
    printf("\t-h - Print this help\n"); 
    return;
}

static int getAttr(int argc, char *argv[],
		   uint* coreId, uint8_t* coreBitmap,
		   bool* useHwTime, char* fileName) {
  
    bool ifSet = false;
    bool fileSet = false;
    int opt; 
    while((opt = getopt(argc, argv, ":i:w:b:th")) != -1) {  
	switch(opt) {  
	case 'i':  
	    *coreId = atoi(optarg);
	    printf("coreId = %u\n", *coreId);
	    ifSet = true;
	    break;
	case 'b':  
	    *coreBitmap = atoi(optarg);
	    printf("coreBitmap = 0x%x\n", *coreBitmap);
	    ifSet = true;
	    break;  	
	case 'w':  
	    strcpy(fileName,optarg);
	    printf("fileName = %s\n", fileName);
	    fileSet = true;
	    break;
	case 't':
	    *useHwTime = true;
	    printf("Using FPGA time stamps\n");
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
    if (! ifSet) on_error("FPGA interface to capture is not set");
    if (! fileSet) on_error("PCAP file name is not set. Use \'-w <file name>\' mandatory option");
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

static void enableSniffer(SC_DeviceId devId, uint8_t coreBitmap) {
    eka_write(devId,0xf0360, coreBitmap);
    TEST_LOG("Ekaline FPGA sniffer is enabled for cores 0x%x",coreBitmap);
}

/* --------------------------------------------- */

int main(int argc, char *argv[]) {
    EkaHwCaps* ekaHwCaps = new EkaHwCaps(NULL);
    if (ekaHwCaps == NULL) on_error("ekaHwCaps == NULL");
    
    if (ekaHwCaps->hwCaps.version.sniffer != EKA_EXPECTED_SNIFFER_VERSION)
	on_error("This FW version does not support %s",argv[0]);

    uint coreId = -1;
    uint8_t coreBitmap = 0;
    bool useHwTime = false;
    char fileName[256] = {};
    static const uint16_t SnifferUserChannelId = 7;
    signal(SIGINT, INThandler);

    getAttr(argc,argv,&coreId,&coreBitmap,&useHwTime,fileName);

    FILE* out_pcap_file = fopen(fileName, "w+");
    if (out_pcap_file == NULL) on_error("Failed opening pcapFile file \'%s\'\n",fileName);


    PcapFileHdr pcapFileHdr = {
//	.magic_number  = 0xa1b2c3d4, // old us format
	.magic_number  = 0xa1b23c4d, // ns  format
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

    if (coreBitmap) {
	if (coreBitmap > 0xF) on_error("wrong coreBitmap 0x%x",coreBitmap);
	enableSniffer(devId,coreBitmap);
    } else { 
	if (coreId > 3) on_error("coreId %u is not supported",coreId);
	enableSniffer(devId,0x1 << coreId);
    }
  
    while (keep_work) {
	const SC_Packet * pPacket = SC_GetNextPacket(channelId, pPrevPacket, SN_TIMEOUT_NONE);
	if (pPacket == NULL) continue;

	const uint8_t * payload = SC_GetUserLogicPayload(pPacket);
	uint16_t payloadLength  = SC_GetUserLogicPayloadLength(pPacket);

	auto snifferHdr {reinterpret_cast<const SnifferHdr*>(payload)};
	payload       += sizeof(*snifferHdr);
	payloadLength -= sizeof(*snifferHdr);	

	//	hexDump("snifferHdr",snifferHdr,sizeof(*snifferHdr));
	if (sizeof(*snifferHdr) != 32) on_error("sizeof(*snifferHdr) = %jd",sizeof(*snifferHdr));
	/* +++++++++++++++++++++++++++++++++++++ */
	
	uint32_t seconds = 0;
	uint32_t ns = 0;
	if (useHwTime) {
	    long double fpgaNs = be64toh(snifferHdr->fpgaTimeCycles) * (long double)(1000.0/EKA_FPGA_FREQUENCY) ;
	    //	    printf ("fpgaNs = %Lf  clock = %ju (0x%jx)\n",fpgaNs,be64toh(snifferHdr->fpgaTimeCycles),be64toh(snifferHdr->fpgaTimeCycles));
	    seconds = (uint32_t) (fpgaNs / 1000000000);
	    ns      = fpgaNs - (seconds * 1000000000);
	} else {
	    struct timespec t;
	    clock_gettime(CLOCK_REALTIME, &t); // 
	    seconds = (uint32_t) t.tv_sec;
	    ns      = (uint32_t) t.tv_nsec;
	}
	PcapRecHdr pcapRecHdr = {
	    .ts_sec  = seconds,
	    .ts_usec = ns,
	    .cap_len = payloadLength,	    
	    .len     = payloadLength
	};
    

	fwrite(&pcapRecHdr,1,sizeof(PcapRecHdr),out_pcap_file); 
	fwrite(payload,1,payloadLength,out_pcap_file); 

	/* +++++++++++++++++++++++++++++++++++++ */

	if (SC_UpdateReceivePtr(channelId, pPacket) != SC_ERR_SUCCESS) on_error("error on SC_UpdateReceivePtr");

	if (++packetCount % 10000 == 0)
	    printf("%ju packets dumped\n",packetCount);

	pPrevPacket = pPacket;

    }
    disableSniffer(devId);
    fclose(out_pcap_file);
    printf ("%ju packets dumped to %s\n",packetCount,fileName);
    return 0;
}
