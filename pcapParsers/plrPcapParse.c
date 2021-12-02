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

#include "EkaFhPlrParser.h"
#include "eka_macros.h"

using namespace Plr;


//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  baseTime;
    uint64_t  expectedSeq;
};

static GroupAddr group[] = {
    //CERT BBO Real Time
    {inet_addr("224.0.60.9" ), 41021, 0, 0},
    {inet_addr("224.0.60.10"), 41022, 0, 0},
    {inet_addr("224.0.60.11"), 41023, 0, 0},

    //CERT BBO Re-transmit
    {inet_addr("224.0.60.12"), 41024, 0, 0},
    {inet_addr("224.0.60.13"), 41025, 0, 0},
    {inet_addr("224.0.60.14"), 41026, 0, 0},

    //CERT BBO Refresh
    {inet_addr("224.0.60.15"), 41027, 0, 0},
    {inet_addr("224.0.60.16"), 41028, 0, 0},
    {inet_addr("224.0.60.17"), 41029, 0, 0},

    //CERT Trades Real Time
    {inet_addr("224.0.60.18"), 41041, 0, 0},

    //CERT Trades Re-transmit
    {inet_addr("224.0.60.19"), 41042, 0, 0},

    //CERT Trades Refresh
    {inet_addr("224.0.60.20"), 41043, 0, 0},
    
    //Production ARCA BBO Real Time
    {inet_addr("224.0.96.48"), 41051, 0, 0},
    {inet_addr("224.0.96.49"), 41052, 0, 0},
    {inet_addr("224.0.96.50"), 41053, 0, 0},
    {inet_addr("224.0.96.51"), 41054, 0, 0},
    {inet_addr("224.0.96.52"), 41055, 0, 0},
    {inet_addr("224.0.96.53"), 41056, 0, 0},
    {inet_addr("224.0.96.54"), 41057, 0, 0},
    {inet_addr("224.0.96.55"), 41058, 0, 0},
    {inet_addr("224.0.96.56"), 41059, 0, 0},
    {inet_addr("224.0.96.57"), 41060, 0, 0},
    {inet_addr("224.0.96.58"), 41061, 0, 0},
    {inet_addr("224.0.96.59"), 41062, 0, 0},
    {inet_addr("224.0.96.60"), 41063, 0, 0},
    {inet_addr("224.0.96.61"), 41064, 0, 0},

    //Production ARCA BBO Re-transmit
    {inet_addr("224.0.96.64"), 42051, 0, 0},
    {inet_addr("224.0.96.65"), 42052, 0, 0},
    {inet_addr("224.0.96.66"), 42053, 0, 0},
    {inet_addr("224.0.96.67"), 42054, 0, 0},
    {inet_addr("224.0.96.68"), 42055, 0, 0},
    {inet_addr("224.0.96.69"), 42056, 0, 0},
    {inet_addr("224.0.96.70"), 42057, 0, 0},
    {inet_addr("224.0.96.71"), 42058, 0, 0},
    {inet_addr("224.0.96.72"), 42059, 0, 0},
    {inet_addr("224.0.96.73"), 42060, 0, 0},
    {inet_addr("224.0.96.74"), 42061, 0, 0},
    {inet_addr("224.0.96.75"), 42062, 0, 0},
    {inet_addr("224.0.96.76"), 42063, 0, 0},
    {inet_addr("224.0.96.77"), 42064, 0, 0},

    //Production ARCA BBO Refresh
    {inet_addr("224.0.96.80"), 43051, 0, 0},
    {inet_addr("224.0.96.81"), 43052, 0, 0},
    {inet_addr("224.0.96.82"), 43053, 0, 0},
    {inet_addr("224.0.96.83"), 43054, 0, 0},
    {inet_addr("224.0.96.84"), 43055, 0, 0},
    {inet_addr("224.0.96.85"), 43056, 0, 0},
    {inet_addr("224.0.96.86"), 43057, 0, 0},
    {inet_addr("224.0.96.87"), 43058, 0, 0},
    {inet_addr("224.0.96.88"), 43059, 0, 0},
    {inet_addr("224.0.96.89"), 43060, 0, 0},
    {inet_addr("224.0.96.90"), 43061, 0, 0},
    {inet_addr("224.0.96.91"), 43062, 0, 0},
    {inet_addr("224.0.96.92"), 43063, 0, 0},
    {inet_addr("224.0.96.93"), 43064, 0, 0},

    //Production ARCA Trades Real Time
    {inet_addr("224.0.96.96"), 44001, 0, 0},
    //Production ARCA Trades Re-transmit
    {inet_addr("224.0.96.97"), 45001, 0, 0},
    //Production ARCA Trades Refresh
    {inet_addr("224.0.96.98"), 46001, 0, 0},

    //Production ARCA Top Summary Real Time
    {inet_addr("224.0.96.99"), 44011, 0, 0},
  
    //Production ARCA Imbalances Real Time
    {inet_addr("224.0.96.102"), 44021, 0, 0},
    //Production ARCA Imbalances Re-transmit
    {inet_addr("224.0.96.103"), 45021, 0, 0},
    //Production ARCA Imbalances Refresh
    {inet_addr("224.0.96.104"), 46021, 0, 0},

    //Production ARCA Complex Real Time
    {inet_addr("224.0.97.0"),  47001, 0, 0},
    {inet_addr("224.0.97.1"),  47002, 0, 0},
    {inet_addr("224.0.97.2"),  47003, 0, 0},
    {inet_addr("224.0.97.3"),  47004, 0, 0},
    {inet_addr("224.0.97.4"),  47005, 0, 0},
    {inet_addr("224.0.97.5"),  47006, 0, 0},
    {inet_addr("224.0.97.6"),  47007, 0, 0},
    {inet_addr("224.0.97.7"),  47008, 0, 0},
    {inet_addr("224.0.97.8"),  47009, 0, 0},
    {inet_addr("224.0.97.9"),  47010, 0, 0},
    {inet_addr("224.0.97.10"), 47011, 0, 0},
    {inet_addr("224.0.97.11"), 47012, 0, 0},
    {inet_addr("224.0.97.12"), 47013, 0, 0},
    {inet_addr("224.0.97.13"), 47014, 0, 0},

    //Production ARCA Complex Re-transmit
    {inet_addr("224.0.97.16"), 48001, 0, 0},
    {inet_addr("224.0.97.17"), 48002, 0, 0},
    {inet_addr("224.0.97.18"), 48003, 0, 0},
    {inet_addr("224.0.97.19"), 48004, 0, 0},
    {inet_addr("224.0.97.20"), 48005, 0, 0},
    {inet_addr("224.0.97.21"), 48006, 0, 0},
    {inet_addr("224.0.97.22"), 48007, 0, 0},
    {inet_addr("224.0.97.23"), 48008, 0, 0},
    {inet_addr("224.0.97.24"), 48009, 0, 0},
    {inet_addr("224.0.97.25"), 48010, 0, 0},
    {inet_addr("224.0.97.26"), 48011, 0, 0},
    {inet_addr("224.0.97.27"), 48012, 0, 0},
    {inet_addr("224.0.97.28"), 48013, 0, 0},
    {inet_addr("224.0.97.29"), 48014, 0, 0},

    //Production ARCA Complex Refresh
    {inet_addr("224.0.97.32"), 49001, 0, 0},
    {inet_addr("224.0.97.33"), 49002, 0, 0},
    {inet_addr("224.0.97.34"), 49003, 0, 0},
    {inet_addr("224.0.97.35"), 49004, 0, 0},
    {inet_addr("224.0.97.36"), 49005, 0, 0},
    {inet_addr("224.0.97.37"), 49006, 0, 0},
    {inet_addr("224.0.97.38"), 49007, 0, 0},
    {inet_addr("224.0.97.39"), 49008, 0, 0},
    {inet_addr("224.0.97.40"), 49009, 0, 0},
    {inet_addr("224.0.97.41"), 49010, 0, 0},
    {inet_addr("224.0.97.42"), 49011, 0, 0},
    {inet_addr("224.0.97.43"), 49012, 0, 0},
    {inet_addr("224.0.97.44"), 49013, 0, 0},
    {inet_addr("224.0.97.45"), 49014, 0, 0}, 
};


//###################################################

int findGrp(uint32_t ip, uint16_t port) {
  for (size_t i = 0; i < sizeof(group)/sizeof(group[0]); i++) {
    if (group[i].ip == ip && group[i].port == port)
	    return (int)i;
  }
  //  on_error("%s:%u is not found",EKA_IP2STR(ip),port);
  return -1;
}

//###################################################
void printUsage(char* cmd) {
  printf("USAGE: %s [options] -f [pcapFile]\n",cmd);
  printf("          -p        Print all messages\n");
}

//###################################################

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":f:d:ph")) != -1) {  
    switch(opt) {  
      case 'f':
	strcpy(pcapFileName,optarg);
	printf("pcapFile = %s\n", pcapFileName);  
	break;  
      case 'p':  
	printAll = true;
	printf("printAll\n");
	break;  
      case 'd':  
//	pkt2dump = atoi(optarg);
//	printf("pkt2dump = %ju\n",pkt2dump);  
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
    FILE *pcapFile;
    getAttr(argc,argv);

    if ((pcapFile = fopen(pcapFileName, "rb")) == NULL) {
	printf("Failed to open dump file %s\n",pcapFileName);
	printUsage(argv[0]);
	exit(1);
    }
    if (fread(buf,sizeof(pcap_file_hdr),1,pcapFile) != 1) 
	on_error ("Failed to read pcap_file_hdr from the pcap file");

    uint64_t pktNum = 0;

    while (fread(buf,sizeof(pcap_rec_hdr),1,pcapFile) == 1) {
	auto pcap_rec_hdr_ptr {reinterpret_cast<const pcap_rec_hdr*>(buf)};
	uint pktLen = pcap_rec_hdr_ptr->len;
	if (pktLen > 1536)
	    on_error("Probably wrong PCAP format: pktLen = %u ",pktLen);

	char pkt[1536] = {};
	if (fread(pkt,pktLen,1,pcapFile) != 1) 
	    on_error ("Failed to read %d packet bytes at pkt %ju",pktLen,pktNum);
	pktNum++;

	auto p {reinterpret_cast<const uint8_t*>(pkt)};
	if (! EKA_IS_UDP_PKT(p)) continue;

	uint32_t ip = EKA_IPH_DST(p);
	uint16_t port = EKA_UDPH_DST(p);
	auto grId = findGrp(ip,port);
	if (grId < 0) continue;

	p += sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);

	printf ("%d: %s:%u, %ju,",grId,EKA_IP2STR(ip),port,pktNum);
	printPkt(p);

    }
    return 0;
}
