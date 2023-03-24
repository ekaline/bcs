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

#include <fcntl.h>
#include "ekaNW.h"
#include "EfhTestFuncs.h"
#include "EfcItchFS.h"

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
static const int      TotalInjects       = 10;
  int      ExpectedFires    = 0;
  int      ReportedFires    = 0;

/* --------------------------------------------- */
int getHWFireCnt(EkaDev* dev, uint64_t addr) {
    uint64_t var_pass_counter = eka_read(dev, addr);
    int real_val =  (var_pass_counter>>0) & 0xffffffff;
    return  real_val;
}

/* --------------------------------------------- */
void handleFireReport(const void* p, size_t len, void* ctx) {
  auto b = static_cast<const uint8_t*>(p);
  auto containerHdr {reinterpret_cast<const EkaContainerGlobalHdr*>(b)};
  switch (containerHdr->type) {
  case EkaEventType::kExceptionEvent:
    break;
  default:
    ReportedFires++;
  }
  
  efcPrintFireReport(p,len,ctx);
  return;
}

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
    on_error("failed to bind server sock to %s:%u",
	     EKA_IP2STR(addr.sin_addr.s_addr),be16toh(addr.sin_port));
  if ( listen(sd, 20) != 0 ) on_error("Listen");
  *serverSet = true;

  int addr_size = sizeof(addr);
  *sock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
  EKA_LOG("Connected from: %s:%d -- sock=%d\n",
	  inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),*sock);

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
	 "-e <Exit at the end of the test>"
	 "\n",cmd);
  return;
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   std::string* triggerIp, uint16_t* triggerUdpPort,
		   uint16_t* numTcpSess, bool* runEfh, bool* fatalDebug, bool* dontExit) {
	int opt; 
	while((opt = getopt(argc, argv, ":c:s:p:u:l:t:fdhe")) != -1) {  
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
		case 'e':  
			printf("dontExit = OFF\n");
			*dontExit = false;
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


static int sendFSMsg(std::string serverIp,
			   std::string dstIp,
			   uint16_t dstPort) {
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

    const uint8_t pkt[] =
      {
       0x30, 0x30, 0x30, 0x30, 0x30, 0x39, 0x37, 0x36, 0x30, 0x42, 0x00, 0x00, 0x00, 0x00, 0x03, 0x23,
       0x53, 0xfd, 0x00, 0x04, 0x00, 0x1f, 0x45, 0x18, 0x5e, 0x00, 0x02, 0x1f, 0x52, 0x61, 0x88, 0xea,
       0xd3, 0x00, 0x00, 0x00, 0x00, 0x02, 0x51, 0x01, 0x32, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x10, 0x09, 0xbc, 0x00, 0x1f, 0x45, 0x18, 0x5e, 0x00, 0x04, 0x1f, 0x52, 0x61, 0x88,
       0xea, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x03, 0x6c, 0x3c, 0xde, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x10, 0x09, 0xbd, 0x00, 0x1f, 0x45, 0x18, 0x5e, 0x00, 0x06, 0x1f, 0x52, 0x61,
       0x88, 0xea, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x03, 0x6c, 0xfe, 0x16, 0x00, 0x00, 0x01, 0x2c, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x10, 0x09, 0xbe, 0x00, 0x1f, 0x45, 0x18, 0x5e, 0x00, 0x08, 0x1f, 0x52,
       0x61, 0x88, 0xea, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x03, 0x6d, 0xc6, 0x1a, 0x00, 0x00, 0x00, 0x64,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x09, 0xbf
      };

    /* const uint8_t pkt[] = */
    /*   { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //mold session */
    /* 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //mold seq */
    /* 	0x00, 0x01, //mold msg cnt */
    /* 	0x00, 0x20, 0x45 /\*"E"*\/, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; */

    size_t payloadLen = std::size(pkt);
 
    TEST_LOG("sending Fast Sweep trigger to %s:%u",
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
    std::string triggerIp       = "233.54.12.111";     // Ekaline lab default (C1 CC feed)
    uint16_t triggerUdpPort     = 26477;            // C1 CC gr#0
    uint16_t serverTcpBasePort  = 22345;            // Ekaline lab default
    uint16_t numTcpSess         = 1;
    uint16_t serverTcpPort      = serverTcpBasePort;
    bool     runEfh             = false;
    bool     fatalDebug          = false;
    bool     dontExit          = true;

    getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort,
	    &numTcpSess,&runEfh,&fatalDebug,&dontExit);

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
	    .gateway      = inet_addr("10.0.0.10"),
	    .nexthop_mac  = {}, // resolved by our internal ARP
	    .src_mac_addr = {}, // taken from system config
	    .dont_garp    = 0
	}
    };
    ekaDevConfigurePort (dev, &ekaCoreInitCtx);

    // ==============================================
    // Setting EXC UDP connection

    ExcUdpTxConnHandle udpConn = excUdpConnect(dev,coreId,
					       {0xa,0xa,0xa,0xa,0xa,0xa},{0xb,0xb,0xb,0xb,0xb,0xb},
					       inet_addr("11.22.33.44"),inet_addr("55.66.77.88"),
					       //					       0x11223344,0x55667788,
					       11111,22222);

    // ==============================================
    // Setup EFC MC groups

    EpmTriggerParams triggerParam[] = {
	{0,"233.54.12.111",26477},
	/* {0,"224.0.74.1",30302}, */
	/* {0,"224.0.74.2",30303}, */
	/* {0,"224.0.74.3",30304}, */
    };

    EfcCtx efcCtx = {};
    EfcCtx* pEfcCtx = &efcCtx;
    EfcArmVer   armVer = 0;

    EfcInitCtx initCtx = {
	.feedVer = EfhFeedVer::kITCHFS
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
    runCtx.onEfcFireReportCb = handleFireReport; 
    // ==============================================
    // ITCH Fast Sweep config
    static const uint64_t AlwaysFire = 0xadcd;
    static const uint64_t Token = 0x1122334455667788;
    static const uint64_t User  = 0xaabbccddeeff0011;
    
    const EfcItchFastSweepParams params = {
      .minUDPSize     = 150,
      .minMsgCount    = 4,
      .token          = Token //0x8877665544332211
    };

    //    uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
    uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_UdpDatagramOffset);
    //    uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
    uint heapOffset    = 0;

    // ==============================================
    // Manually prepared ItchTestFastSweepMsg fired by FPGA
    const char ItchTestFastSweepMsg[] = "XXXXXXpadding text";
    
    EpmAction itchFSAction = {};

    itchFSAction.type          = EpmActionType::ItchHwFastSweep;
    itchFSAction.token         = params.token;
    itchFSAction.hConn         = udpConn;
    itchFSAction.offset        = heapOffset + nwHdrOffset;
    itchFSAction.length        = 18;//strlen(ItchTestFastSweepMsg);
    itchFSAction.actionFlags   = AF_Valid;
    itchFSAction.nextAction    = EPM_LAST_ACTION;
    itchFSAction.enable        = AlwaysFire;
    itchFSAction.postLocalMask = AlwaysFire;
    itchFSAction.postStratMask = AlwaysFire;
    itchFSAction.user          = User;

    TEST_LOG("heapOffset=%u, nwHdrOffset=%u",heapOffset,nwHdrOffset);
	
    rc = epmPayloadHeapCopy(dev,
   			    EFC_STRATEGY,
   			    itchFSAction.offset,
   			    itchFSAction.length,
   			    ItchTestFastSweepMsg,
			    true); //isudp
    if (rc != EKA_OPRESULT__OK)
      on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
	       itchFSAction.offset,
	       (uint)strlen(ItchTestFastSweepMsg),(int)rc);

    epmSetAction(dev,EFC_STRATEGY,0,&itchFSAction); 
    
    // ==============================================
    efcItchFastSweepInit(dev,&params);
    // ==============================================
    efcEnableController(pEfcCtx, -1);
    // ==============================================
    efcRun(pEfcCtx, &runCtx );
    // ==============================================

    /* EpmTrigger fastSweepSwTrig = { */
    /* 				  .token = Token, */
    /* 				  .strategy = 0, */
    /* 				  .action = 0 */
    /* }; */
    /* epmRaiseTriggers(dev, &fastSweepSwTrig); */


    for (auto i = 0; i < TotalInjects; i++) {
      if (rand() % 3) {
	efcEnableController(pEfcCtx, 1, armVer++); //arm and promote
	ExpectedFires++;
      }
      else {
	efcEnableController(pEfcCtx, 1, armVer-1); //should be no arm
      }
      
      sendFSMsg(serverIp,triggerIp,triggerUdpPort);
      usleep (300000);      
    }

    if (fatalDebug) {
	TEST_LOG(RED "\n=====================\n"
		 "FATAL DEBUG: ON"
		 "\n=====================\n" RESET);
	eka_write(dev,0xf0f00,0xefa0beda);
    }

// ==============================================

//    efcEnableController(pEfcCtx, 1, armVer++); //arm
    efcEnableController(pEfcCtx, -1);    
    int hw_fires  = getHWFireCnt(dev,0xf0810) ;  

    printf("\n===========================\nEND OT TESTS : ");

#ifndef _VERILOG_SIM
    bool testPass = true;
    if (ReportedFires==ExpectedFires && ReportedFires==hw_fires) {
      printf(GRN);
      printf("PASS, ExpectedFires == ReportedFires == HWFires == %d\n"  ,ExpectedFires);
    }
    else {
      printf(RED);
      printf ("FAIL, ExpectedFires = %d  ReportedFires = %d hw_fires = %d\n" ,ExpectedFires,ReportedFires,hw_fires);
      testPass = false;
    }
    printf(RESET);
    printf("===========================\n\n");  
    testCtx->keep_work = dontExit;
    sleep(1);
    EKA_LOG("--Test finished, ctrl-c to end---");
    while (testCtx->keep_work) { sleep(0); }
#endif

    fflush(stdout);fflush(stderr);


    /* ============================================== */

    printf("Closing device\n");

    ekaDevClose(dev);
  
    if (testPass)
      return 0;
    else
      return 1;

}
