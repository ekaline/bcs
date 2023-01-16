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

#include "EkaFhTypes.h"
#include "EkaFhNomParser.h"
#include "eka_macros.h"

//using namespace Nom;
#include "EkaFhNasdaqCommonParser.h"
#include "EkaNwParser.h"
#include "EkaPcapNomGr.h"

using namespace EfhNasdaqCommon;
using namespace EkaNwParser;

int ekaDefaultLog (void* /*unused*/, const char* function, const char* file, int line, int priority, const char* format, ...) {
  va_list ap;
  const int rc1 = fprintf(g_ekaLogFile, "%s@%s:%u: ",function,file,line);
  va_start(ap, format);
  const int rc2 = vfprintf(g_ekaLogFile, format, ap);
  va_end(ap);
  const int rc3 = fprintf(g_ekaLogFile,"\n");
  return rc1 + rc2 + rc3;
}

EkaLogCallback g_ekaLogCB = ekaDefaultLog;
FILE   *g_ekaLogFile = stdout;

//###################################################
static char pcapFileName[256] = {};
static bool printAll = false;

struct GroupAddr {
    uint32_t  ip;
    uint16_t  port;
    uint64_t  baseTime;
    uint64_t  expectedSeq;
};

struct SoupbinHdr {
  uint16_t    length;
  char        type;
} __attribute__((packed));

static GroupAddr group[] = {
    {inet_addr("206.200.43.72"), 18300, 0, 0},
    {inet_addr("206.200.43.73"), 18301, 0, 0},
    {inet_addr("206.200.43.74"), 18302, 0, 0},
    {inet_addr("206.200.43.75"), 18303, 0, 0},
};

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
void* onOptionDefinition(const EfhOptionDefinitionMsg* msg,
			 EfhSecUserData secData,
			 EfhRunUserData userData) {
  std::string underlyingName = std::string(msg->commonDef.underlying,
					   sizeof(msg->commonDef.underlying));
  std::string classSymbol    = std::string(msg->commonDef.classSymbol,
					   sizeof(msg->commonDef.classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(), ' ', '\0');
  std::replace(classSymbol.begin(),    classSymbol.end(),    ' ', '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize   (strlen(classSymbol.c_str()));

  char avtSecName[32] = {};

  eka_create_avt_definition(avtSecName,msg);
  fprintf (g_ekaLogFile,"%s,%ju,%s,%s\n",
	   avtSecName,
	   msg->header.securityId,
	   underlyingName.c_str(),
	   classSymbol.c_str()
	   );

  return NULL;
}
//###################################################

void* onQuote(const EfhQuoteMsg* msg, EfhSecUserData secData,
	      EfhRunUserData userData) {
  const int64_t priceScaleFactor = 10000;
  fprintf(g_ekaLogFile,"TOB: %ju,%u,%.*f,%u,%u,%.*f,%u,%c,%s,%ju\n",
	  msg->header.securityId,

	  msg->bidSide.size,
	  decPoints(msg->bidSide.price,priceScaleFactor),
	  ((float) msg->bidSide.price / priceScaleFactor),
	  msg->bidSide.customerSize,
	  msg->askSide.size,
	  decPoints(msg->askSide.price,priceScaleFactor),
	  ((float) msg->askSide.price / priceScaleFactor),
	  msg->askSide.customerSize,
	  EKA_TS_DECODE(msg->tradeStatus),
	  (ts_ns2str(msg->header.timeStamp)).c_str(),
	  msg->header.timeStamp
	  );
  return NULL;
}
//###################################################
void* onTrade(const EfhTradeMsg* msg, EfhSecUserData secData,
	      EfhRunUserData userData) {
  fprintf(g_ekaLogFile,"Trade,");
  fprintf(g_ekaLogFile,"%ju,",msg->header.securityId);
  fprintf(g_ekaLogFile,"%ld,",msg->price);
  fprintf(g_ekaLogFile,"%u," ,msg->size);
  fprintf(g_ekaLogFile,"%d," ,(int)msg->tradeCond);
  fprintf(g_ekaLogFile,"%s," ,ts_ns2str(msg->header.timeStamp).c_str());
  fprintf(g_ekaLogFile,"%ju,",msg->header.timeStamp);
  fprintf(g_ekaLogFile,"\n");

  return NULL;
}
//###################################################

int main(int argc, char *argv[]) {
    getAttr(argc,argv);

    SoupbinHdr soupbinHdr = {};
    uint8_t pkt[1536];

    auto nomGr = new EkaFhNomGr;
    
    auto pktHndl = new TcpPcapHandler(pcapFileName,
				      inet_addr("206.200.43.72"), 18300);

    EfhRunCtx efhRunCtx = {
      .onEfhOptionDefinitionMsgCb  = onOptionDefinition,
      .onEfhTradeMsgCb             = onTrade,
      .onEfhQuoteMsgCb             = onQuote,
    };
    ssize_t size2read;
    ssize_t pktSize;
    ssize_t hdrSize;
    while (1) {
      hdrSize = pktHndl->getData(&soupbinHdr,sizeof(soupbinHdr));
      if (hdrSize != sizeof(soupbinHdr)) {
	if (hdrSize == -2) goto END; //EOF
	TEST_LOG("hdrSize %jd != sizeof(soupbinHdr) %jd",
		 hdrSize,sizeof(soupbinHdr));
	break;
      }
      size2read = be16toh(soupbinHdr.length) - sizeof(soupbinHdr.type);
      if (size2read <= 0) on_error("size2read=%jd",size2read);
      pktSize = pktHndl->getData(pkt,size2read);
      if (pktSize != size2read) {
	if (hdrSize == -2) goto END; //EOF
	TEST_LOG("pktSize %jd != size2read %jd",
		 pktSize,size2read);
	break;
      }
      /* printf ("\'%c\' (0x%x), %jd\n", */
      /* 	      soupbinHdr.type,soupbinHdr.type,size2read); */
      switch (soupbinHdr.type) {
      case 'S' :
      case 'U' :
	printMsg<NomFeed>(g_ekaLogFile,0,pkt);
	nomGr->parseMsg(&efhRunCtx,pkt,0,EkaFhMode::DEFINITIONS);
	nomGr->parseMsg(&efhRunCtx,pkt,0,EkaFhMode::SNAPSHOT);
	break;
      case 'A' :
      case 'H' :
      case '+' :
	break;
      default :
	//	hexDump("bad soupbin pkt",tcpPayload,tcpPayloadLen);
	  
	on_error("Unexpected Soupbin type \'%c\' (0x%x), pktSize=%jd",
		 soupbinHdr.type,soupbinHdr.type,pktSize);
      }
    } // while()
 END:
    fprintf (g_ekaLogFile,"Processed %ju packets\n",
	     pktHndl->getPktNum());
    delete pktHndl;
    return 0;
}
