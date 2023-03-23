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
#include <vector>

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

#include <fcntl.h>

/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static EkaOpResult ekaRC = EKA_OPRESULT__OK;

class FireEvent {
  public:
    FireEvent(const void* data, size_t len,void* ctx) {
	len_ = len;
	memcpy(data_,data,len);
	ctx_ = ctx;
    }
    size_t len_;
    uint8_t data_[2048];
    void* ctx_;
};
std::vector<FireEvent> fireEvents;

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

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, size_t userLength, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %.*s is acquired for %s:%hhu\n",(int)userLength,user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}
/* --------------------------------------------- */

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}
/* --------------------------------------------- */

const EpmFireReport* getEpmReportPtr(const uint8_t* p, size_t len) {
    auto b = p;
    auto containerHdr {reinterpret_cast<const EkaContainerGlobalHdr*>(b)};
    b += sizeof(*containerHdr);

    for (uint i = 0; i < containerHdr->num_of_reports; i++) {
	auto reportHdr {reinterpret_cast<const EfcReportHdr*>(b)};
	b += sizeof(*reportHdr);
	if (reportHdr->type == EfcReportType::kEpmReport)
	    return reinterpret_cast<const EpmFireReport*>(b);
	b += reportHdr->size;
	if (b - p > (int)len)
	    on_error("Out of report boundary: len=%ju, offs=%jd",
		     len,b-p);
    }
    return nullptr;
}
/* --------------------------------------------- */

void fireReportCb (const void* p, size_t len, void *ctx) {
    fireEvents.push_back(FireEvent(p,len,ctx));

    efcPrintFireReport(p,len,ctx);
#if 0
  TEST_LOG("StrategyId=%d,ActionId=%d,TriggerActionId=%d,TriggerSource=%s,token=%016jx,user=%016jx,"
  	   "preLocalEnable=%016jx,postLocalEnable=%016jx,preStratEnable=%016jx,postStratEnable=%016jx",
  	   report->strategyId,
  	   report->actionId,
  	   report->trigger->action,
  	   report->local ? "FROM SW" : "FROM UDP",
  	   report->trigger->token,
  	   report->user,
  	   report->preLocalEnable,
  	   report->postLocalEnable,
  	   report->preStratEnable,
  	   report->postStratEnable
  	   );
#endif
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
    on_error("failed to bind server sock to %s:%u",
	     EKA_IP2STR(addr.sin_addr.s_addr),be16toh(addr.sin_port));
  if ( listen(sd, 20) != 0 ) on_error("Listen");
  serverSet = true;

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

static void printFireReport(const EpmFireReport* report) {
  TEST_LOG("strategyId=%3d,actionId=%3d,action=%20s,error=%d,token=%016jx",
	   report->strategyId,
	   report->actionId,
	   action2string(report->action).c_str(),
	   report->error,
	   report->triggerToken
	   );
}

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
    TEST_LOG("\n==============\nExecuting: \'%s\'\n==============",testCase->testCaseName);
    uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
    uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
    uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
    uint heapOffset    = 0;

    uint coreId = excGetCoreId(testCase->conn);

    EkaOpResult ekaRC = epmEnableController(dev,coreId,false);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

    epmSetStrategyEnableBits(dev, testCase->testStrategy.id, testCase->testStrategy.enableBitmap);
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
	    .type          = EpmActionType::UserAction,
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
	ekaRC = epmPayloadHeapCopy(dev,
				   static_cast<epm_strategyid_t>(testCase->testStrategy.id),
				   heapOffset,
				   epmAction.length,
				   (const void *)pkt2send);

	if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %d",ekaRC);
	heapOffset += epmAction.length + fcsOffset;
	
	ekaRC = epmSetAction(dev, testCase->testStrategy.id, currAction->id, &epmAction);
	if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %d",ekaRC);
	//    printf("%d --> ",currAction->id);
    }
    //  printf("EPM_LAST_ACTION\n");
    /* ============================================== */
    ekaRC = epmEnableController(dev,coreId,true);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

    /* ============================================== */

    if (testCase->raiseTriggerFromSw) {
	ekaRC = epmRaiseTriggers(dev, &testCase->epmTrigger);
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

    int eventCnt = 0;
    for (auto const& event : fireEvents) {
	auto epmFireReport = getEpmReportPtr(event.data_,event.len_);
	if (!epmFireReport) on_error("!epmFireReport");
	printFireReport(epmFireReport);

	if (epmFireReport->strategyId != testCase->testStrategy.id) {
	    testPassed = false;
	    TEST_FAILED("StrategyId mismatch: report->strategyId %d != testCase->testStrategy.id %d",
			epmFireReport->strategyId, testCase->testStrategy.id);
	}
	if (epmFireReport->actionId != testCase->expectedAction[eventCnt]) {
	    testPassed = false;
	    TEST_FAILED("ActionId mismatch: report->actionId %d != expectedAction[%d] %d",
			epmFireReport->actionId, eventCnt, testCase->expectedAction[eventCnt]);
	}
	eventCnt++;
    }

    if (testCase->expectedAction[eventCnt] != 0) {
	testPassed = false;
	TEST_FAILED("Expected: %d, fired: %d",
		    NumActionsPerTestCase,
		    eventCnt);
    }

    /* ============================================== */
    fireEvents.clear();
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

  std::string triggerIp = "239.255.119.16"; // Ekaline lab default
  std::string serverIp  = "10.0.0.10";      // Ekaline lab default
  std::string clientIp  = "100.0.0.110";    // Ekaline lab default

  uint16_t serverTcpPort  = 22222;          // Ekaline lab default
  uint16_t triggerUdpPort = 18333;          // Ekaline lab default

  getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort);

  EKA_LOG("\n==============================\nUDP Trigger: %s:%u, Actions Server %s:%u, Client IP %s\n==============================",
	  triggerIp.c_str(),triggerUdpPort,serverIp.c_str(),serverTcpPort,clientIp.c_str());

  /* ============================================== */
  /* Launching TCP Server */
  int tcpServerSock = -1;
  std::thread server = std::thread(tcpServer,dev,serverIp,serverTcpPort,&tcpServerSock);
  server.detach();

  while (keep_work && ! serverSet) { sleep (0); }

  const EkaCoreInitCtx ekaCoreInitCtx = {
    .coreId = coreId,
    .attrs = {
      .host_ip      = inet_addr(clientIp.c_str()),
      .netmask      = inet_addr("255.255.255.0"),
      .gateway      = inet_addr(serverIp.c_str()),
      .nexthop_mac  = {}, // resolved by our internal ARP
      .src_mac_addr = {}, // taken from system config
      .dont_garp    = 0
    }
  };
  ekaDevConfigurePort (dev, &ekaCoreInitCtx);

  TEST_LOG("FPGA port %u is set to %s",
	   ekaCoreInitCtx.coreId,EKA_IP2STR(ekaCoreInitCtx.attrs.host_ip));

  /* ============================================== */
  /* Establishing TCP connection for EPM fires */

  struct sockaddr_in serverAddr = {};
  serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
  serverAddr.sin_family      = AF_INET;
  serverAddr.sin_port        = be16toh(serverTcpPort);

  int ekaSock = excSocket(dev,coreId,0,0,0);
  if (ekaSock < 0) on_error("failed to open sock");
  ExcConnHandle conn = excConnect(dev,ekaSock,(struct sockaddr*) &serverAddr, sizeof(struct sockaddr_in));
  if (conn < 0)
    on_error("excConnect %s:%u",
	     EKA_IP2STR(serverAddr.sin_addr.s_addr),
	     be16toh(serverAddr.sin_port));
  const char* pkt = "\n\nThis is 1st TCP packet sent from FPGA TCP client to Kernel TCP server\n\n";
  excSend (dev, conn, pkt, strlen(pkt), 0);

  int bytes_read = 0;
  char rxBuf[2000] = {};

  bytes_read = recv(tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("\n%s",rxBuf);

  // ==============================================
  // Configuring EFC for a CME (just an artifact)
  EfcCtx efcCtx = {};
  EfcCtx* pEfcCtx = &efcCtx;
  
  EfcInitCtx initCtx = {
      .feedVer = EfhFeedVer::kCME
  };  
  ekaRC = efcInit(&pEfcCtx,dev,&initCtx);
  if (ekaRC != EKA_OPRESULT__OK)
      on_error("efcInit returned %d",(int)ekaRC);
  
  /* ============================================== */
  /* Configuring Strategies */

  struct sockaddr_in triggerMcAddr = {};
  triggerMcAddr.sin_family      = AF_INET;  
  triggerMcAddr.sin_addr.s_addr = inet_addr(triggerIp.c_str());
  triggerMcAddr.sin_port        = be16toh(triggerUdpPort);

  const epm_strategyid_t NumStrategies = 4;
  const epm_actionid_t   NumActions    = 110;

  EpmStrategyParams strategyParams[NumStrategies] = {};
  for (auto i = 0; i < NumStrategies; i++) {
    strategyParams[i].numActions  = NumActions;
    //    strategyParams[i].triggerAddr = reinterpret_cast<const sockaddr*>(&triggerMcAddr);
    strategyParams[i].reportCb    = fireReportCb;
  }

  

  EKA_LOG("Configuring %u Strategies with %u Actions per Strategy",
	  NumStrategies,NumActions);
  ekaRC = epmInitStrategies(dev, strategyParams, NumStrategies);
  if (ekaRC != EKA_OPRESULT__OK)
    on_error("epmInitStrategies failed: ekaRC = %d",ekaRC);

  /* ============================================== */
  /* Opening UDP MC socket to send Epm Triggers */

  int triggerSock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (triggerSock < 0)
      on_error("failed to open UDP sock");
  if (bind(triggerSock,(sockaddr*)&serverAddr, sizeof(sockaddr)) != 0 ) {
    on_error("failed to bind server triggerSock to %s:%u",
	     EKA_IP2STR(serverAddr.sin_addr.s_addr),
	     be16toh(serverAddr.sin_port));
  } else {
    EKA_LOG("triggerSock is binded to %s:%u",
	    EKA_IP2STR(serverAddr.sin_addr.s_addr),
	    be16toh(serverAddr.sin_port));
  }




  /* ============================================== */
  /* ============================================== */
  /* ============================================== */
  {
    uint64_t user = 0;
    TestCase testCase = {
      "Basic Chain Test (from SW)", conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr,
      {1, 0xabcd}, {    // testStrategy
	{100, 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{11 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{29 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{46 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{83 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{31 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{EPM_LAST_ACTION, 0,0,0,0,0,0,0}},
      /* raiseTriggerFromSw =  */ true, {0x1122334455667788, 1, 100}, // epmTrigger
      0,                                   // user -- Opaque value copied into `EpmFireReport`
      {100, 11, 29, 46, 83, 31}
    };
    runTestCase(dev,&testCase);
  }
  //  goto end;
  /* ============================================== */
  {
    uint64_t user = 0;
    TestCase testCase = {
      "Basic Chain Test (from UDP)", conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr,
      {1, 0xabcd}, {    // testStrategy
	{100, 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{11 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{29 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{46 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{83 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{31 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{EPM_LAST_ACTION, 0,0,0,0,0,0,0}},
      /* raiseTriggerFromSw =  */false, {0x1122334455667788, 1, 100}, // epmTrigger
      0,                                   // user -- Opaque value copied into `EpmFireReport`
      {100, 11, 29, 46, 83, 31}
    };
    runTestCase(dev,&testCase);
  }

  //  goto end;
  /* ============================================== */
  {
    uint64_t user = 0;
    TestCase testCase = {
      "Action Skip Test (from UDP)", conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr,
      {2, 0xabcd}, {    // testStrategy
	{100, 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{11 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{29 , 0x5000, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{46 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{83 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{31 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{EPM_LAST_ACTION, 0,0,0,0,0,0,0}},
      /* raiseTriggerFromSw =  */ false, {0x1122334455667788, 2, 100}, // epmTrigger
      0,                                   // user -- Opaque value copied into `EpmFireReport`
      {100, 11, 46, 83, 31}
    };
    runTestCase(dev,&testCase);
  }
  //  goto end;

  /* ============================================== */
  {
    uint64_t user = 0;
    TestCase testCase = {
      "Invalid Action Test (from UDP)", conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr,
      {3, 0xabcd}, {    // testStrategy
	{100, 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{11 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{29 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{46 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{83 , 0xabcd, 0xabcd, 0xabcd, 0       , coreId, user, 0x1122334455667788},
	{31 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{EPM_LAST_ACTION, 0,0,0,0,0,0,0}},
      /* raiseTriggerFromSw =  */ false, {0x1122334455667788, 3, 100}, // epmTrigger
      0,                                   // user -- Opaque value copied into `EpmFireReport`
      {100, 11, 29, 46}
    };
    runTestCase(dev,&testCase);
  }
  //  goto end;

  /* ============================================== */
  {
    uint64_t user = 0;
    TestCase testCase = {
      "Invalid Action Test (from SW)", conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr,
      {3, 0xabcd}, {    // testStrategy
	{100, 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{11 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{29 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{46 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{83 , 0xabcd, 0xabcd, 0xabcd, 0       , coreId, user, 0x1122334455667788},
	{31 , 0xabcd, 0xabcd, 0xabcd, AF_Valid, coreId, user, 0x1122334455667788},
	{EPM_LAST_ACTION, 0,0,0,0,0,0,0}},
      /* raiseTriggerFromSw =  */ true, {0x1122334455667788, 3, 100}, // epmTrigger
      0,                                   // user -- Opaque value copied into `EpmFireReport`
      {100, 11, 29, 46}
    };
    runTestCase(dev,&testCase);
  }
  goto end;

  /* ============================================== */

  end:
  fflush(stdout);fflush(stderr);

  keep_work = false;
  sleep(1);

  /* ============================================== */

  //  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}
