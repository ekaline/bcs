#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iterator>
#include <thread>

#include <linux/sockios.h>

#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/time.h>
#include <chrono>

#include "EkaDev.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Efh.h"
#include "Epm.h"

#include "eka_macros.h"

#include "EkaCtxs.h"
#include "EkaEfcDataStructs.h"
#include "EkaFhCmeParser.h"

#include <fcntl.h>
#include "ekaNW.h"
#include "EfhTestFuncs.h"
#include "EfcCme.h"

using namespace Bats;

extern TestCtx* testCtx;

/* --------------------------------------------- */
std::string ts_ns2str(uint64_t ts);

/* --------------------------------------------- */
//volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static const int MaxFireEvents = 10000;
static volatile EpmFireReport* FireEvent[MaxFireEvents] = {};
static volatile int numFireEvents = 0;

static const int      MaxTcpTestSessions     = 16;
static const int      MaxUdpTestSessions     = 64;

/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  testCtx->keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
  return;
}

/* --------------------------------------------- */

int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}
/* --------------------------------------------- */

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, size_t userLength, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %.*s is acquired for %s:%hhu\n",(int)userLength,user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}
/* --------------------------------------------- */

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}
/* --------------------------------------------- */


/* --------------------------------------------- */
void tcpServer(EkaDev* dev, std::string ip, uint16_t port, int* sock, bool* serverSet) {
  pthread_setname_np(pthread_self(),"tcpServerParent");

  printf("Starting TCP server: %s:%u\n",ip.c_str(),port);
  int sd = 0;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &one_const, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct linger so_linger = {
    true, // Linger ON
    0     // Abort pending traffic on close()
  };

  if (setsockopt(sd,SOL_SOCKET,SO_LINGER,&so_linger,sizeof(struct linger)) < 0) 
    on_error("Cant set SO_LINGER");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = be16toh(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sd,(struct sockaddr*)&addr, sizeof(addr)) != 0 ) 
    on_error("failed to bind server sock to %s:%u",EKA_IP2STR(addr.sin_addr.s_addr),be16toh(addr.sin_port));
  if ( listen(sd, 20) != 0 ) on_error("Listen");
  *serverSet = true;

  int addr_size = sizeof(addr);
  *sock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
  EKA_LOG("Connected from: %s:%d -- sock=%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),*sock);

  int status = fcntl(*sock, F_SETFL, fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
  if (status == -1)  on_error("fcntl error");

  return;
}

/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s "
	 "-s <Connection Server IP>"
	 "-p <Connection Server TcpPort>"
	 "-c <Connection Client IP>"
	 "-t <Trigger IP>"
	 "-u <Trigger UdpPort>"
	 "-l <Num of TCP sessions>"
	 "-f <Run EFH for raw MD>"
	 "-d <FATAL DEBUG ON>"
	 "\n",cmd);
  return;
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   std::string* triggerIp, uint16_t* triggerUdpPort,
		   uint16_t* numTcpSess, bool* runEfh,
       bool *receiveOnConnect, bool *cmePreSend,
       bool* fatalDebug) {
	int opt; 
	while((opt = getopt(argc, argv, ":c:s:p:u:l:t:fdhrS")) != -1) {
		switch(opt) {  
		case 's':  
			*serverIp = std::string(optarg);
			printf("serverIp = %s\n", (*serverIp).c_str());  
			break;  
		case 'c':  
			*clientIp = std::string(optarg);
			printf("clientIp = %s\n", (*clientIp).c_str());  
			break;  
		case 'p':  
			*serverTcpPort = atoi(optarg);
			printf("serverTcpPort = %u\n", *serverTcpPort);  
			break;  
		case 't':  
			*triggerIp = std::string(optarg);
			printf("triggerIp = %s\n", (*triggerIp).c_str());  
			break;  
		case 'u':  
			*triggerUdpPort = atoi(optarg);
			printf("triggerUdpPort = %u\n", *triggerUdpPort);  
			break;  
		case 'l':  
			*numTcpSess = atoi(optarg);
			printf("numTcpSess = %u\n", *numTcpSess);  
			break;  
		case 'f':  
			printf("runEfh = true\n");
			*runEfh = true;
			break;
		case 'd':  
			printf("fatalDebug = ON\n");
			*fatalDebug = true;
			break;
    case 'r':
      printf("receive on connect = OFF");
      *receiveOnConnect = false;
      break;
    case 'S':
      printf("CME pre send = ON");
      *cmePreSend = true;
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

static void cleanFireEvents() {
  for (auto i = 0; i < numFireEvents; i++) {
    free((void*)FireEvent[i]);
    FireEvent[i] = NULL;
  }
  numFireEvents = 0;
  return;
}

/* --------------------------------------------- */
static std::string action2string(EpmTriggerAction action) {
  switch (action) {
  case Unknown:         return std::string("Unknown");
  case Sent:            return std::string("Sent");
  case InvalidToken:    return std::string("InvalidToken");
  case InvalidStrategy: return std::string("InvalidStrategy");
  case InvalidAction:   return std::string("InvalidAction");
  case DisabledAction:  return std::string("DisabledAction");
  case SendError:       return std::string("SendError");
  default:              on_error("Unexpected action %d",action);
  }
    
};
/* --------------------------------------------- */

/* static void printFireReport(EpmFireReport* report) { */
/*   TEST_LOG("strategyId=%3d,actionId=%3d,action=%20s,error=%d,token=%016jx", */
/* 	   report->strategyId, */
/* 	   report->actionId, */
/* 	   action2string(report->action).c_str(), */
/* 	   report->error, */
/* 	   report->trigger->token */
/* 	   ); */
/*   return; */
/* } */


/* --------------------------------------------- */


static int sendCmeTradeMsg(std::string serverIp,
			   std::string dstIp,
			   uint16_t dstPort,
			   uint16_t cmeMsgSize,
			   uint8_t  noMDEntries) {
    // Preparing UDP MC for MD trigger on GR#0

    int triggerSock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (triggerSock < 0)
    	on_error("failed to open UDP sock");

    struct sockaddr_in triggerSourceAddr = {};
    triggerSourceAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
    triggerSourceAddr.sin_family      = AF_INET;
    triggerSourceAddr.sin_port        = 0; //be16toh(serverTcpPort);

    if (bind(triggerSock,(sockaddr*)&triggerSourceAddr, sizeof(sockaddr)) != 0 ) {
    	on_error("failed to bind server triggerSock to %s:%u",
    		 EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),be16toh(triggerSourceAddr.sin_port));
    } else {
    	TEST_LOG("triggerSock is binded to %s:%u",
    		EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),be16toh(triggerSourceAddr.sin_port));
    }
    struct sockaddr_in triggerMcAddr = {};
    triggerMcAddr.sin_family      = AF_INET;
    triggerMcAddr.sin_addr.s_addr = inet_addr(dstIp.c_str());
    triggerMcAddr.sin_port        = be16toh(dstPort);

#if 0    
    uint8_t pkt[1536] = {};

    auto p {pkt};
    auto pktHdr {reinterpret_cast<Cme::PktHdr*>(p)};
    p += sizeof(*pktHdr);
    
    auto msgHdr {reinterpret_cast<Cme::MsgHdr*>(p)};
    msgHdr->templateId = Cme::MsgId::MDIncrementalRefreshTradeSummary48;
    msgHdr->size = cmeMsgSize;
    p += sizeof(*msgHdr);

    auto rootBlock {reinterpret_cast<Cme::MDIncrementalRefreshTradeSummary48_mainBlock*>(p)};
    p += sizeof(*rootBlock);

    auto pGroupSize {reinterpret_cast<Cme::groupSize_T*>(p)};
    pGroupSize->numInGroup = noMDEntries;
    p += sizeof(*pGroupSize);

    const char* data = "Trade message XXXXXXXXXXX";
    strcpy ((char*)p,data);
    p += strlen(data);

    size_t payloadLen = p - pkt;
#else    
    const uint8_t pkt[] =
      {0x22, 0xa5, 0x0d, 0x02, 0xa5, 0x6f, 0x01, 0x38, 0xca, 0x42, 0xdc, 0x16, 0x60, 0x00, 0x0b, 0x00,
       0x30, 0x00, 0x01, 0x00, 0x09, 0x00, 0x41, 0x23, 0xff, 0x37, 0xca, 0x42, 0xdc, 0x16, 0x01, 0x00,
       0x00, 0x20, 0x00, 0x01, 0x00, 0xfc, 0x2f, 0x9c, 0x9d, 0xb2, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
       0x5b, 0x33, 0x00, 0x00, 0x83, 0x88, 0x26, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0xd9, 0x7a,
       0x6d, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0e, 0x19, 0x84, 0x8e,
       0x36, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x7f, 0x8e,
       0x36, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    size_t payloadLen = std::size(pkt);
#endif  
 
    TEST_LOG("sending MDIncrementalRefreshTradeSummary48 trigger to %s:%u",
	    EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),be16toh(triggerMcAddr.sin_port));
    if (sendto(triggerSock,pkt,payloadLen,0,(const sockaddr*)&triggerMcAddr,sizeof(triggerMcAddr)) < 0) 
	on_error ("MC trigger send failed");
    return 0;
}

/* ############################################# */
int main(int argc, char *argv[]) {
  
    signal(SIGINT, INThandler);
    testCtx = new TestCtx;
    if (!testCtx) on_error("testCtx == NULL");
    // ==============================================

    std::string serverIp        = "10.0.0.10";      // Ekaline lab default
    std::string clientIp        = "100.0.0.110";    // Ekaline lab default
    std::string triggerIp       = "224.0.74.0";     // Ekaline lab default (C1 CC feed)
    uint16_t triggerUdpPort     = 30301;            // C1 CC gr#0
    uint16_t serverTcpBasePort  = 22345;            // Ekaline lab default
    uint16_t numTcpSess         = 1;
    uint16_t serverTcpPort      = serverTcpBasePort;
    bool     runEfh             = false;
    bool     fatalDebug         = false;
    bool     receiveOnConnect   = true;
    bool     cmePreSend         = false;
  
    getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort,
	    &numTcpSess,&runEfh,&receiveOnConnect,&cmePreSend,&fatalDebug);

    if (numTcpSess > MaxTcpTestSessions) 
	on_error("numTcpSess %d > MaxTcpTestSessions %d",numTcpSess, MaxTcpTestSessions);
    // ==============================================
    // EkaDev general setup
    EkaDev*     dev = NULL;
    EkaCoreId   coreId = 0;
    EkaOpResult rc;
    const EkaDevInitCtx ekaDevInitCtx = {};

    ekaDevInit(&dev, &ekaDevInitCtx);

    // ==============================================
    // 10G Port (core) setup
    const EkaCoreInitCtx ekaCoreInitCtx = {
	.coreId = coreId,
	.attrs = {
	    .host_ip      = inet_addr(clientIp.c_str()),
	    .netmask      = inet_addr("255.255.255.0"),
	    //	    .gateway      = inet_addr("10.120.10.1"), // from nqxlxavt060p
	    .gateway      = inet_addr("10.0.0.10"),
	    
	    .nexthop_mac  = {}, // resolved by our internal ARP
	    .src_mac_addr = {}, // taken from system config
	    .dont_garp    = 0
	}
    };
    ekaDevConfigurePort (dev, &ekaCoreInitCtx);

    // ==============================================
    // Launching TCP test Servers
    int tcpSock[MaxTcpTestSessions] = {};

    for (auto i = 0; i < numTcpSess; i++) {
	bool serverSet = false;
	std::thread server = std::thread(tcpServer,
					 dev,
					 serverIp,
           serverTcpPort + i,
					 &tcpSock[i],
					 &serverSet);
	server.detach();
	while (testCtx->keep_work && ! serverSet)
	    sleep (0);
    }
    // ==============================================
    // Establishing EXC connections for EPM/EFC fires 

    ExcConnHandle conn[MaxTcpTestSessions]    = {};

    for (auto i = 0; i < numTcpSess; i++) {
      struct sockaddr_in serverAddr = {};
      serverAddr.sin_family      = AF_INET;
      serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
      serverAddr.sin_port        = be16toh(serverTcpPort + i);

      int excSock = excSocket(dev,coreId,0,0,0);
      if (excSock < 0) on_error("failed to open sock %d",i);
      conn[i] = excConnect(dev,excSock,(sockaddr*) &serverAddr, sizeof(sockaddr_in));
      if (conn[i] < 0) on_error("excConnect %d %s:%u",
              i,EKA_IP2STR(serverAddr.sin_addr.s_addr),be16toh(serverAddr.sin_port));
      const char* pkt = "\n\nThis is 1st TCP packet sent from FPGA TCP client to Kernel TCP server\n\n";
      excSend(dev, conn[i], pkt, strlen(pkt),0);
      if (receiveOnConnect) {
        int bytes_read = 0;
        char rxBuf[2000] = {};
        bytes_read = recv(tcpSock[i], rxBuf, sizeof(rxBuf), 0);
        if (bytes_read > 0)
          EKA_LOG("\n%s", rxBuf);
      }
    }

    // ==============================================
    // Setup EFC MC groups

    EpmTriggerParams triggerParam[] = {
	{0,"224.0.74.0",30301},
	/* {0,"224.0.74.1",30302}, */
	/* {0,"224.0.74.2",30303}, */
	/* {0,"224.0.74.3",30304}, */
    };

    EfcCtx efcCtx = {};
    EfcCtx* pEfcCtx = &efcCtx;

    EfcInitCtx initCtx = {
	.feedVer = EfhFeedVer::kCME
    };  
    rc = efcInit(&pEfcCtx,dev,&initCtx);
    if (rc != EKA_OPRESULT__OK)
	on_error("efcInit returned %d",(int)rc);

    // ==============================================
    // Configuring EFC as EPM Strategy

    const EpmStrategyParams efcEpmStrategyParams = {
	.numActions    = 256,          // just a number
	.triggerParams = triggerParam,         
	.numTriggers   = std::size(triggerParam),
	.reportCb      = NULL,         // set via EfcRunCtx
	.cbCtx         = NULL
    };
    rc = epmInitStrategies(dev, &efcEpmStrategyParams, 1);
    if (rc != EKA_OPRESULT__OK)
	on_error("epmInitStrategies failed: rc = %d",rc);

    // ==============================================
    // Global EFC config
    EfcStratGlobCtx efcStratGlobCtx = {
	.enable_strategy      = 1,
	.report_only          = 0,
	.watchdog_timeout_sec = 100000,
    };
    efcInitStrategy(pEfcCtx, &efcStratGlobCtx);

    EfcRunCtx runCtx = {};
    runCtx.onEfcFireReportCb = efcPrintFireReport;
    // ==============================================
    // CME FastCancel EFC config
    static const uint64_t CmeTestFastCancelAlwaysFire = 0xadcd;
    static const uint64_t CmeTestFastCancelToken = 0x1122334455667788;
    static const uint64_t CmeTestFastCancelUser  = 0xaabbccddeeff0011;
    static const uint16_t CmeTestFastCancelMaxMsgSize = 1300;
    static const uint8_t  CmeTestFastCancelMinNoMDEntries = 1;

    const EfcCmeFastCancelParams params = {
	.maxMsgSize     = CmeTestFastCancelMaxMsgSize,
	.minNoMDEntries = CmeTestFastCancelMinNoMDEntries,
	.token          = CmeTestFastCancelToken
    };

    uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
    uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
    uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
    uint heapOffset    = 0;
    uint32_t maxPayloadLen = 1536 - nwHdrOffset - fcsOffset;
    
    EpmAction cmeAction[(size_t)EfcCmeActionId::Count] = {};

    for (epm_actionid_t actionId = 0; actionId < (epm_actionid_t)EfcCmeActionId::Count; actionId++) {
	cmeAction[actionId].type = efcCmeActionId2Type((EfcCmeActionId)actionId);
	cmeAction[actionId].token         = params.token;
	cmeAction[actionId].hConn         = conn[0];
	cmeAction[actionId].offset        = heapOffset + nwHdrOffset;
	cmeAction[actionId].length        = maxPayloadLen;
	cmeAction[actionId].actionFlags   = AF_Valid;
	cmeAction[actionId].nextAction    = EPM_LAST_ACTION;
	cmeAction[actionId].enable        = CmeTestFastCancelAlwaysFire;
	cmeAction[actionId].postLocalMask = CmeTestFastCancelAlwaysFire;
	cmeAction[actionId].postStratMask = CmeTestFastCancelAlwaysFire;
	cmeAction[actionId].user          = CmeTestFastCancelUser;
	
	epmSetAction(dev,EFC_STRATEGY,actionId,&cmeAction[actionId]);
	
	heapOffset += cmeAction[actionId].length + nwHdrOffset + fcsOffset;
	heapOffset += dataAlignment - (heapOffset % dataAlignment);
    }


    // ==============================================
    // Manually prepared CmeTestFastCancel message fired by FPGA
    const char CmeTestFastCancelMsg[] = "CME Fast Cancel: Sequence = |____| With Dummy payload";
    rc = epmPayloadHeapCopy(dev, 
			    EFC_STRATEGY,
			    cmeAction[(size_t)EfcCmeActionId::HwCancel].offset,
			    strlen(CmeTestFastCancelMsg),
			    CmeTestFastCancelMsg);
    cmeAction[(size_t)EfcCmeActionId::HwCancel].length = strlen(CmeTestFastCancelMsg);
    epmSetAction(dev,EFC_STRATEGY,(epm_actionid_t)EfcCmeActionId::HwCancel,
		             &cmeAction[(size_t)EfcCmeActionId::HwCancel]);
    
    if (rc != EKA_OPRESULT__OK) 
	    on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
		           cmeAction[(size_t)EfcCmeActionId::HwCancel].offset,
		           (uint)strlen(CmeTestFastCancelMsg),(int)rc);

    // ==============================================
    efcCmeFastCancelInit(dev,&params);
    // ==============================================
    efcEnableController(pEfcCtx, 0);
    // ==============================================
    efcRun(pEfcCtx, &runCtx );
    // ==============================================

    EpmTrigger cmeTrigger = {
	.token = CmeTestFastCancelToken,         ///< Security token
	.strategy = EFC_STRATEGY,                ///< Strategy this trigger applies to
	.action = (epm_actionid_t)EfcCmeActionId::HwCancel       ///< First action in linked sequence
    };
#if 0
    const char* swMsg = "CME Fast SW Msg: Sequence = |____| : expected incremented Sequence";
    const char* swHB  = "CME SW Heartbeat:Sequence = |____| : expected NOT incremented Sequence";

    efcCmeSetILinkAppseq(dev,conn[0],0x1);

    efcCmeSend(dev,conn[0],swMsg,strlen(swMsg),0,true);
    efcCmeSend(dev,conn[0],swMsg,strlen(swMsg),0,true);
    efcCmeSend(dev,conn[0],swMsg,strlen(swMsg),0,true);
    
    efcCmeSend(dev,conn[0],swHB,strlen(swMsg),0,false);
    efcCmeSend(dev,conn[0],swHB,strlen(swMsg),0,false);
    efcCmeSend(dev,conn[0],swHB,strlen(swMsg),0,false);
    efcCmeSend(dev,conn[0],swHB,strlen(swMsg),0,false);
    efcCmeSend(dev,conn[0],swHB,strlen(swMsg),0,false);
#endif
    if (cmePreSend) {
      const char *swMsg = "CME Fast SW Msg: Sequence = |____| : expected incremented Sequence";
      const char *swHB = "CME SW Heartbeat:Sequence = |____| : expected NOT incremented Sequence";
      efcCmeSend(dev, conn[0], swMsg, strlen(swMsg), 0, true);
      efcCmeSend(dev, conn[0], swMsg, strlen(swMsg), 0, true);
      efcCmeSend(dev, conn[0], swMsg, strlen(swMsg), 0, true);

      efcCmeSend(dev, conn[0], swHB, strlen(swMsg), 0, false);
      efcCmeSend(dev, conn[0], swHB, strlen(swMsg), 0, false);
      efcCmeSend(dev, conn[0], swHB, strlen(swMsg), 0, false);
      efcCmeSend(dev, conn[0], swHB, strlen(swMsg), 0, false);
      efcCmeSend(dev, conn[0], swHB, strlen(swMsg), 0, false);
    }
    
    epmRaiseTriggers(dev,&cmeTrigger);
    cmeTrigger.action = 1;
    epmRaiseTriggers(dev,&cmeTrigger);
    cmeTrigger.action = 2;
    epmRaiseTriggers(dev,&cmeTrigger);
    cmeTrigger.action = 0;
    epmRaiseTriggers(dev,&cmeTrigger);

    if (0) {
      sendCmeTradeMsg(serverIp, triggerIp, triggerUdpPort,
                      CmeTestFastCancelMaxMsgSize - 1, CmeTestFastCancelMinNoMDEntries + 1);
    }

    if (fatalDebug) {
	    TEST_LOG(RED "\n=====================\nFATAL DEBUG: ON\n=====================\n" RESET);
	    eka_write(dev,0xf0f00,0xefa0beda);
    }

// ==============================================


    TEST_LOG("\n===========================\nEND OT TESTS\n===========================\n");

#ifndef _VERILOG_SIM
    sleep(2);
    EKA_LOG("--Test finished, ctrl-c to end---");
//  testCtx->keep_work = false;
    while (testCtx->keep_work) { sleep(0); }
#endif

    sleep(1);
    fflush(stdout);fflush(stderr);


    /* ============================================== */

    printf("Closing device\n");

    ekaDevClose(dev);
    sleep(1);
  
    return 0;
}
