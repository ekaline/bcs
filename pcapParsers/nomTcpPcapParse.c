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
static char tcpFileName[256] = {};
static char udpFileName[256] = {};
static int grId = -1;

struct GroupAddr {
    uint32_t  mcIp;
    uint16_t  mcPort;
    uint32_t  glimpseIp;
    uint16_t  glimpsePort;
};


static GroupAddr group[] = {
    {inet_addr("206.200.43.72"),18300,inet_addr("233.54.12.72" ),18000},
    {inet_addr("206.200.43.73"),18301,inet_addr("233.54.12.73" ),18001},
    {inet_addr("206.200.43.74"),18302,inet_addr("233.54.12.74" ),18002},
    {inet_addr("206.200.43.75"),18303,inet_addr("233.54.12.75" ),18003},
};

void printGroup(int id) {
  printf ("NOM_ITTO:%d : Glimpse=%s:%u, MC=%s:%u\n",
	  id,
	  EKA_IP2STR(group[id].mcIp),group[id].mcPort,
	  EKA_IP2STR(group[id].glimpseIp),group[id].glimpsePort);	  
}
//###################################################
void printUsage(char* cmd) {
  printf("USAGE: %s [options]\n",cmd);
  printf("          -t [tcpPcapFile]\n");
  printf("          -u [udpPcapFile]\n");
  printf("          -g [NomGrId in [0..3]]\n");
  printf("          -l List supported NOM groups configs\n");
  printf("          -h Print help\n");
}

//###################################################

static int getAttr(int argc, char *argv[]) {
  int opt; 
  while((opt = getopt(argc, argv, ":t:u:g:lh")) != -1) {  
    switch(opt) {  
      case 't':
	strcpy(tcpFileName,optarg);
	printf("TcpPcapFile = %s\n", tcpFileName);  
	break;  
      case 'u':
	strcpy(udpFileName,optarg);
	printf("UdpPcapFile = %s\n", udpFileName);  
	break;  
      case 'g':  
	grId = atoi(optarg);
	if (grId < 0 || grId > 3)
	  on_error("Bad grId = %d",grId);
	printGroup(grId);
	break;  
      case 'l':  
	for (int i = 0; i < 4; i++)
	  printGroup(i);
	exit (1);
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
EkaFhMode nextOp(EkaFhMode op) {
  switch (op) {
  case EkaFhMode::UNINIT :
    return EkaFhMode::DEFINITIONS;
    
  case EkaFhMode::DEFINITIONS :
    return EkaFhMode::SNAPSHOT;
    
  default:
    on_error("Unexpected EkaFhMode change at %s",
	     EkaFhMode2STR(op));
  }
}

//###################################################

int main(int argc, char *argv[]) {
    getAttr(argc,argv);

    auto nomGr = new EkaFhNomGr(grId);
    auto pktHndl = new TcpPcapHandler(tcpFileName,
				      group[grId].glimpseIp,
				      group[grId].glimpsePort);

    EfhRunCtx efhRunCtx = {
      .onEfhOptionDefinitionMsgCb  = onOptionDefinition,
      .onEfhTradeMsgCb             = onTrade,
      .onEfhQuoteMsgCb             = onQuote,
    };

    int rc = 0;
    EkaFhMode op = EkaFhMode::UNINIT;
    while (1) {
      SoupBinTcp::Hdr soupbinHdr = {};
      rc = pktHndl->getData(&soupbinHdr,SoupBinTcp::getHdrLen());
      if (rc < 0) break;

      uint8_t pkt[1536];
      rc = pktHndl->getData(pkt,SoupBinTcp::getPayloadLen(&soupbinHdr));
      if (rc < 0) break;

      switch (SoupBinTcp::getType(&soupbinHdr)) {
      case 'S' :
      case 'U' :
	if (op != EkaFhMode::UNINIT) {
	  printMsg<NomFeed>(g_ekaLogFile,0,pkt);
	  nomGr->parseMsg(&efhRunCtx,pkt,0,op);
	}
	break;
      case 'A' :
	op = nextOp(op);
	TEST_LOG("Login accepted: %s",EkaFhMode2STR(op));
	break;
      case 'H' :
	TEST_LOG("Soupbin Heartbeat");
	break;
      case '+' :
	TEST_LOG("Soupbin Debug Message: \'%s\'",(char*)pkt);
	break;
      case 'Z' :
	on_error("Unexpected End-of-session \'Z\'");
      default :
	on_error("Unexpected Soupbin type \'%c\' (0x%x), pktSize=%jd",
		 SoupBinTcp::getType(&soupbinHdr),
		 SoupBinTcp::getType(&soupbinHdr),
		 SoupBinTcp::getPayloadLen(&soupbinHdr));
      }
    } // while()

    fprintf (g_ekaLogFile,"Processed %ju packets\n",
	     pktHndl->getPktNum());
    delete pktHndl;
    return 0;
}
