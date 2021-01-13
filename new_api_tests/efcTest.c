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
#include "EkaEpm.h"

#include "eka_macros.h"

#include "EkaCore.h"
#include "EkaCtxs.h"

#include <fcntl.h>

/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static const int MaxFireEvents = 10000;
static volatile EpmFireReport* FireEvent[MaxFireEvents] = {};
static volatile int numFireEvents = 0;


static const uint64_t TEST_NOM_SEC_ID = 0x0003c40e;
/* --------------------------------------------- */

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
}

/* --------------------------------------------- */

int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}
/* --------------------------------------------- */

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}
/* --------------------------------------------- */

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}
/* --------------------------------------------- */
void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fireReportBuf, size_t size) {
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  EKA_LOG ("FIRE REPORT RECEIVED");
  hexDump("FireReport",fireReportBuf,size);
  efcPrintFireReport(pEfcCtx, (EfcReportHdr*)fireReportBuf);
  EKA_LOG ("Rearming...\n");
  efcEnableController(pEfcCtx,1);
}

/* --------------------------------------------- */
void tcpServer(EkaDev* dev, std::string ip, uint16_t port, int* sock) {
  pthread_setname_np(pthread_self(),"tcpServerParent");

  printf("Starting TCP server: %s:%u\n",ip.c_str(),port);
  int sd = 0;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEADDR) failed");

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
  serverSet = true;

  int addr_size = sizeof(addr);
  *sock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
  EKA_LOG("Connected from: %s:%d -- sock=%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),*sock);

  int status = fcntl(*sock, F_SETFL, fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
  if (status == -1)  on_error("fcntl error");

  return;
}

/* --------------------------------------------- */

void printUsage(char* cmd) {
  printf("USAGE: %s -s <Connection Server IP> -p <Connection Server TcpPort> -c <Connection Client IP> -t <Trigger IP> -u <Trigger UdpPort> \n",cmd); 
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   std::string* triggerIp, uint16_t* triggerUdpPort) {
  int opt; 
  while((opt = getopt(argc, argv, ":c:s:p:u:t:h")) != -1) {  
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

static void printFireReport(EpmFireReport* report) {
  TEST_LOG("strategyId=%3d,actionId=%3d,action=%20s,error=%d,token=%016jx",
	   report->strategyId,
	   report->actionId,
	   action2string(report->action).c_str(),
	   report->error,
	   report->trigger->token
	   );
}

/* --------------------------------------------- */


/* --------------------------------------------- */

/* [6/16 5:33 PM] Igor Galitskiy */
    
/* multicast IPs: */
/* 239.255.119.16 & 239.255.119.17 */

/* nqxlxavt059d */
/*  publish: sfc0 10.120.115.53 */
/*  receive: fpga0_0 10.120.115.56 */

/* nqxlxavt061d */
/*  publish: sfc0 10.120.115.52 */
/*  receive: fpga0_0 10.120.115.54 */

/* --------------------------------------------- */
static const int NumActionsPerTestCase = 10;

struct TestStrategy {
  epm_strategyid_t id;
  epm_enablebits_t enableBitmap;
};

struct TestAction {
  epm_actionid_t   id;
  epm_enablebits_t enableBitmap;
  epm_enablebits_t postLocalMask;
  epm_enablebits_t postStratMask;
  uint32_t         validFlag;
  EkaCoreId        coreId;
  uint64_t         user;
  epm_token_t      token;
};

struct TestCase {
  const char       *testCaseName;
  ExcConnHandle    conn;
  int              tcpServerSock;
  int              triggerSock;
  const sockaddr*  triggerMcAddr;
  TestStrategy     testStrategy;
  TestAction       testAction[NumActionsPerTestCase] = {};
  bool             raiseTriggerFromSw;
  EpmTrigger       epmTrigger;
  uint64_t         user;
  epm_actionid_t   expectedAction[NumActionsPerTestCase] = {};
};
/* ############################################# */
bool runTestCase(EkaDev *dev, TestCase *testCase) {
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;

  uint coreId = excGetCoreId(testCase->conn);

  EkaOpResult ekaRC = epmEnableController(dev,coreId,false);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  epmSetStrategyEnableBits(dev, coreId, testCase->testStrategy.id, testCase->testStrategy.enableBitmap);
  /* ============================================== */

  int testActionsNum = sizeof(testCase->testAction)/sizeof(testCase->testAction[0]);
  for (auto i = 0; i < testActionsNum ; i++) {
    TestAction *currAction = &testCase->testAction[i];
    if (currAction->id == EPM_LAST_ACTION) break;
    epm_actionid_t nextActionId = testCase->testAction[i+1].id;

    char pkt2send[1000] = {};
    sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%3u, next=%5u EOP\n",
	    testCase->testStrategy.id,i,static_cast<uint>(currAction->id),nextActionId);

    heapOffset = heapOffset + dataAlignment - (heapOffset % dataAlignment) + nwHdrOffset;

    EpmAction epmAction = {
      .token         = currAction->token,                      ///< Security token
      .hConn         = testCase->conn,                         ///< TCP connection where segments will be sent
      .offset        = heapOffset,                             ///< Offset to payload in payload heap
      .length        = (uint32_t)strlen(pkt2send),             ///< Payload length
      .actionFlags   = currAction->validFlag,                  ///< Behavior flags (see EpmActionFlag)
      .nextAction    = nextActionId,                           ///< Next action in sequence, or EPM_LAST_ACTION
      .enable        = currAction->enableBitmap,               ///< Enable bits
      .postLocalMask = currAction->postLocalMask,              ///< Post fire: enable & mask -> enable
      .postStratMask = currAction->postStratMask,              ///< Post fire: strat-enable & mask -> strat-enable
      .user          = static_cast<uintptr_t>(testCase->user)  ///< Opaque value copied into `EpmFireReport`.
    };
    ekaRC = epmPayloadHeapCopy(dev, coreId,
			       static_cast<epm_strategyid_t>(testCase->testStrategy.id),
			       heapOffset,
			       epmAction.length,
			       (const void *)pkt2send);

    if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %d",ekaRC);
    heapOffset += epmAction.length + fcsOffset;
	
    ekaRC = epmSetAction(dev, coreId, testCase->testStrategy.id, currAction->id, &epmAction);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %d",ekaRC);
    //    printf("%d --> ",currAction->id);
  }
  //  printf("EPM_LAST_ACTION\n");
  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  /* ============================================== */

  if (testCase->raiseTriggerFromSw) {
    ekaRC = epmRaiseTriggers(dev,coreId, &testCase->epmTrigger);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmRaiseTriggers failed: ekaRC = %d",ekaRC);
  } else {
    if (sendto(testCase->triggerSock,&testCase->epmTrigger,sizeof(EpmTrigger),0,testCase->triggerMcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");
  }
  /* ============================================== */

  sleep(1);
  char rxBuf[2000] = {};
  int bytes_read = recv(testCase->tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("EPM fires received by TCP:\n%s",rxBuf);
  else EKA_LOG("No fires\n");
  /* ============================================== */

  bool testPassed = true;

  for (auto i = 0; i < numFireEvents; i++) {
    if (FireEvent[i] == NULL) on_error("FireEvent[%d] == NULL",i);

    EpmFireReport* report = (EpmFireReport*)FireEvent[i];
    printFireReport(report);

    if (report->strategyId != testCase->testStrategy.id) {
      testPassed = false;
      TEST_FAILED("StrategyId mismatch: report->strategyId %d != testCase->testStrategy.id %d",
	       report->strategyId, testCase->testStrategy.id);
    }
    if (report->actionId != testCase->expectedAction[i]) {
      testPassed = false;
      TEST_FAILED("ActionId mismatch: report->actionId %d != expectedAction[%d] %d",
	       report->actionId, i, testCase->expectedAction[i]);
    }
  }

  if (testCase->expectedAction[numFireEvents] != 0) {
      testPassed = false;
      TEST_FAILED("Expected more events than fired: %d",numFireEvents);
  }

  /* ============================================== */
  cleanFireEvents();
  /* ============================================== */

  if (testPassed) {
    TEST_PASSED("%s: PASSED",testCase->testCaseName);
  } else {
    TEST_FAILED("%s: FAILED",testCase->testCaseName);
  }
  return testPassed;
}


/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  EkaDev* dev = NULL;
  EkaCoreId coreId = 0;

  EkaDevInitCtx ekaDevInitCtx = {
    .logCallback  = NULL,
    .logContext   = NULL,
    .credAcquire  = credAcquire,
    .credRelease  = credRelease,
    .credContext  = NULL,
    .createThread = createThread,
    .createThreadContext = NULL
  };
  ekaDevInit(&dev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  std::string triggerIp = "233.54.12.73"  ; // Ekaline lab default
  std::string serverIp  = "10.0.0.10";      // Ekaline lab default
  std::string clientIp  = "100.0.0.110";    // Ekaline lab default

  uint16_t serverTcpPort  = 22222;          // Ekaline lab default
  uint16_t triggerUdpPort = 18001;          // Ekaline lab default

  getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort);

  EKA_LOG("\n==============================\nUDP Trigger: %s:%u, Actions Server %s:%u, Client IP %s\n==============================",
	  triggerIp.c_str(),triggerUdpPort,serverIp.c_str(),serverTcpPort,clientIp.c_str());

  /* ============================================== */
  /* Launching TCP Server */
  int tcpServerSock = -1;
  std::thread server = std::thread(tcpServer,dev,serverIp,serverTcpPort,&tcpServerSock);
  server.detach();

  while (keep_work && ! serverSet) { sleep (0); }

  EkaCoreInitCtx ekaCoreInitCtx = {
    .coreId = coreId,
    .attrs = {
      .host_ip      = inet_addr(clientIp.c_str()),
      .netmask      = inet_addr("255.255.255.0"),
      .gateway      = inet_addr(clientIp.c_str()),
      .nexthop_mac  = {}, // resolved by our internal ARP
      .src_mac_addr = {}, // taken from system config
      .dont_garp    = 0
    }
  };
  ekaDevConfigurePort (dev, (const EkaCoreInitCtx*) &ekaCoreInitCtx);


  /* ============================================== */
  /* Establishing TCP connection for EPM fires */

  struct sockaddr_in serverAddr = {};
  serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
  serverAddr.sin_family      = AF_INET;
  serverAddr.sin_port        = be16toh(serverTcpPort);

  int ekaSock = excSocket(dev,coreId,0,0,0);
  if (ekaSock < 0) on_error("failed to open sock");
  ExcConnHandle conn = excConnect(dev,ekaSock,(struct sockaddr*) &serverAddr, sizeof(struct sockaddr_in));
  if (conn < 0) on_error("excConnect %s:%u",EKA_IP2STR(serverAddr.sin_addr.s_addr),be16toh(serverAddr.sin_port));
  const char* pkt = "\n\nThis is 1st TCP packet sent from FPGA TCP client to Kernel TCP server\n\n";
  excSend (dev, conn, pkt, strlen(pkt));

  int bytes_read = 0;
  char rxBuf[2000] = {};

  bytes_read = recv(tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("\n%s",rxBuf);

  /* ============================================== */
  EkaProp initCtxEntries[] = {
    "efc.group.0.mcast.addr", "233.54.12.73:18001"
  };
  EkaProps ekaProps = {
    .numProps = std::size(initCtxEntries),
    .props = initCtxEntries
  };
  const EfcInitCtx initCtx = {
    .ekaProps = &ekaProps,
    .mdCoreId = 0
  };
  EfcCtx efcCtx = {};
  EfcCtx* pEfcCtx = &efcCtx;
  EkaOpResult rc = efcInit(&pEfcCtx,dev,&initCtx);
  if (rc != EKA_OPRESULT__OK) on_error("efcInit returned %d",(int)rc);
  /* ============================================== */
  EfcStratGlobCtx efcStratGlobCtx = {
    .enable_strategy = 1,
    .report_only = 0,
  //  .no_report_on_exception = 0,
    .debug_always_fire_on_unsubscribed = 0,
    .debug_always_fire = 0,
    .max_size = 1000,
    .watchdog_timeout_sec = 100000,
  };
  efcInitStrategy(pEfcCtx, &efcStratGlobCtx);
  /* ============================================== */
  EfcRunCtx runCtx = {};
  runCtx.onEfcFireReportCb = onFireReport;
  /* ============================================== */

  /* ============================================== */
  /* Opening UDP MC socket to send Epm Triggers */

  int triggerSock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (triggerSock < 0) on_error("failed to open UDP sock");
  if (bind(triggerSock,(sockaddr*)&serverAddr, sizeof(sockaddr)) != 0 ) {
    on_error("failed to bind server triggerSock to %s:%u",EKA_IP2STR(serverAddr.sin_addr.s_addr),be16toh(serverAddr.sin_port));
  } else {
    EKA_LOG("triggerSock is binded to %s:%u",EKA_IP2STR(serverAddr.sin_addr.s_addr),be16toh(serverAddr.sin_port));
  }

  /* ============================================== */
  efcEnableFiringOnSec(pEfcCtx, &TEST_NOM_SEC_ID, 1);
  EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, TEST_NOM_SEC_ID);
  SecCtx secCtx = {
    .bidMinPrice       = 1, //x100, should be nonzero
    .askMaxPrice       = 0,  //x100
    .size              = 1,
    .verNum            = 0xaf,
    .lowerBytesOfSecId = TEST_NOM_SEC_ID & 0xFF
  };
  rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
  if (rc != EKA_OPRESULT__OK) on_error ("failed to efcSetStaticSecCtx");

  /* ============================================== */

  efcRun(pEfcCtx, &runCtx );

  EKA_LOG("After efcRun");
  //  while (keep_work) { sleep(0); }

  //  end:
  fflush(stdout);fflush(stderr);


  /* ============================================== */

  //  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}
