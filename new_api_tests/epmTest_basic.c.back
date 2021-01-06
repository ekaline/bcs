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

#include <fcntl.h>

/* --------------------------------------------- */

volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static EkaOpResult ekaRC = EKA_OPRESULT__OK;

static const int MaxFireEvents = 10000;
static volatile EpmFireReport* FireEvent[MaxFireEvents] = {};
static volatile int numFireEvents = 0;

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

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
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

void fireReportCb (const EpmFireReport *report, int nReports, void *ctx) {
  for (auto i = 0; i < nReports; i++) {
    if (numFireEvents > MaxFireEvents) on_error("numFireEvents %d > MaxFireEvents %d",numFireEvents, MaxFireEvents);

    FireEvent[numFireEvents] = (EpmFireReport*) malloc(sizeof(EpmFireReport));
    if (FireEvent[numFireEvents] == NULL) on_error("failed on malloc");
    
    memcpy((void*)FireEvent[numFireEvents],report,sizeof(EpmFireReport));
    numFireEvents++;
  }

  /* TEST_LOG("StrategyId=%d,ActionId=%d,TriggerActionId=%d,TriggerSource=%s,token=%016jx,user=%016jx," */
  /* 	   "preLocalEnable=%016jx,postLocalEnable=%016jx,preStratEnable=%016jx,postStratEnable=%016jx", */
  /* 	   report->strategyId, */
  /* 	   report->actionId, */
  /* 	   report->trigger->action, */
  /* 	   report->local ? "FROM SW" : "FROM UDP", */
  /* 	   report->trigger->token, */
  /* 	   report->user, */
  /* 	   report->preLocalEnable, */
  /* 	   report->postLocalEnable, */
  /* 	   report->preStratEnable, */
  /* 	   report->postStratEnable */
  /* 	   ); */
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
bool basicChainTest(EkaDev* dev, ExcConnHandle conn, int tcpServerSock, int triggerSock, const sockaddr* triggerMcAddr) {
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;

  epm_enablebits_t enableBitmap = 0xabcd;
  uint64_t         user         = 0x1122334455667788;
  EkaCoreId coreId              = 0; // Unused
  epm_token_t token             = 0x5566778899aabbcc;
 
  int configuredAction[] = {
    100, 15, 21, 49, 17, 31, EPM_LAST_ACTION
  };
  int expectedAction[] = {
    100, 15, 21, 49, 17, 31
  };
  const epm_strategyid_t testStrategyId = 3;
  /* ============================================== */

  ekaRC = epmEnableController(dev,coreId,false);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  epmSetStrategyEnableBits(dev, coreId, testStrategyId, enableBitmap);

  for (auto actionIdx = 0; actionIdx < (int) (sizeof(configuredAction)/sizeof(configuredAction[0])); actionIdx++) {
    if (configuredAction[actionIdx] == EPM_LAST_ACTION) continue;

    epm_actionid_t nextAction = configuredAction[actionIdx+1];
    char pkt2send[1000] = {};
    sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%3u, next=%5u EOP\n",
	    testStrategyId,actionIdx,static_cast<uint>(configuredAction[actionIdx]),nextAction);

    heapOffset = heapOffset + dataAlignment - (heapOffset % dataAlignment) + nwHdrOffset;

    EpmAction epmAction = {
      .token         = token,                             ///< Security token
      .hConn         = conn,                              ///< TCP connection where segments will be sent
      .offset        = heapOffset,                        ///< Offset to payload in payload heap
      .length        = (uint32_t)strlen(pkt2send),        ///< Payload length
      .actionFlags   = AF_Valid,                          ///< Behavior flags (see EpmActionFlag)
      .nextAction    = nextAction,                        ///< Next action in sequence, or EPM_LAST_ACTION
      .enable        = enableBitmap,                      ///< Enable bits
      .postLocalMask = enableBitmap,                      ///< Post fire: enable & mask -> enable
      .postStratMask = enableBitmap,                      ///< Post fire: strat-enable & mask -> strat-enable
      .user          = static_cast<uintptr_t>(user)       ///< Opaque value copied into `EpmFireReport`.
    };
    ekaRC = epmPayloadHeapCopy(dev, coreId,
			       static_cast<epm_strategyid_t>(testStrategyId),
			       heapOffset,
			       epmAction.length,
			       (const void *)pkt2send);

    if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %d",ekaRC);
    heapOffset += epmAction.length + fcsOffset;
	
    ekaRC = epmSetAction(dev, coreId, testStrategyId, configuredAction[actionIdx], &epmAction);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %d",ekaRC);
  }

  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  /* ============================================== */

  const EpmTrigger epmTrigger = {
    .token    = token, 
    .strategy = testStrategyId,  
    .action   = configuredAction[0]
  };

  if (1) {
    if (sendto(triggerSock,&epmTrigger,sizeof(EpmTrigger) + 16,0,triggerMcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");
  } else {
    ekaRC = epmRaiseTriggers(dev,coreId, &epmTrigger);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmRaiseTriggers failed: ekaRC = %d",ekaRC);
  }
   /* ============================================== */

  sleep(1);
  char rxBuf[2000] = {};
  int bytes_read = recv(tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("EPM fires received by TCP:\n%s",rxBuf);
  else EKA_LOG("No fires\n");
   /* ============================================== */

  int numExpectedEvents = sizeof(expectedAction)/sizeof(expectedAction[0]);

  for (auto i = 0; i < numFireEvents; i++) {
    if (FireEvent[i] == NULL) on_error("FireEvent[%d] == NULL",i);

    EpmFireReport* report = (EpmFireReport*)FireEvent[i];
    printFireReport(report);

    if (report->strategyId != testStrategyId) 
      on_error("StrategyId mismatch: report->strategyId %d != testStrategyId %d",
	       report->strategyId, testStrategyId);

    if (report->actionId != expectedAction[i])
      on_error("ActionId mismatch: report->actionId %d != expectedAction[%d] %d",
	       report->actionId, i, expectedAction[i]);

    
  }

  if (numExpectedEvents != numFireEvents) {
    on_error(RED "Number of Fire Events mismatch: Expected %d != numFireEvents %d" RESET,
  	     numExpectedEvents, numFireEvents);
  }

  /* ============================================== */
  cleanFireEvents();
  /* ============================================== */

  EKA_LOG(GRN "%s: PASSED" RESET,__func__);
  return true;
}


/* --------------------------------------------- */
bool skipActionInChainTest(EkaDev* dev, ExcConnHandle conn, int tcpServerSock, int triggerSock, const sockaddr* triggerMcAddr) {
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;

  epm_enablebits_t enableBitmap = 0xabcd;
  uint64_t         user         = 0x1122334455667788;
  EkaCoreId        coreId       = 0; // Unused
  epm_token_t      token        = 0x5566778899aabbcc;

  epm_actionid_t configuredAction[] = {
    8, 3, 17, 11, EPM_LAST_ACTION
  };
  epm_actionid_t expectedAction[] = {
    8, 3,     11
  };

  epm_actionid_t action2skip = 17;

  const epm_strategyid_t testStrategyId = 2;
  /* ============================================== */

  ekaRC = epmEnableController(dev,coreId,false);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  epmSetStrategyEnableBits(dev, coreId, testStrategyId, enableBitmap);

  for (auto actionIdx = 0; actionIdx < (int) (sizeof(configuredAction)/sizeof(configuredAction[0])); actionIdx++) {
    if (configuredAction[actionIdx] == EPM_LAST_ACTION) continue;

    epm_actionid_t nextAction = configuredAction[actionIdx+1];
    char pkt2send[1000] = {};
    sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%3u, next=%5u EOP\n",
	    testStrategyId,actionIdx,static_cast<uint>(configuredAction[actionIdx]),nextAction);

    heapOffset = heapOffset + dataAlignment - (heapOffset % dataAlignment) + nwHdrOffset;

    epm_enablebits_t actionEnable     = enableBitmap;
    epm_enablebits_t actionPostMask   = enableBitmap;
    epm_enablebits_t strategyPostMask = enableBitmap;

    if (configuredAction[actionIdx] == action2skip)
      actionEnable = ~ enableBitmap;

    EpmAction epmAction = {
      .token         = token,                             ///< Security token
      .hConn         = conn,                              ///< TCP connection where segments will be sent
      .offset        = heapOffset,                        ///< Offset to payload in payload heap
      .length        = (uint32_t)strlen(pkt2send),        ///< Payload length
      .actionFlags   = AF_Valid,                          ///< Behavior flags (see EpmActionFlag)
      .nextAction    = nextAction,                        ///< Next action in sequence, or EPM_LAST_ACTION
      .enable        = actionEnable,                      ///< Enable bits
      .postLocalMask = actionPostMask,                    ///< Post fire: enable & mask -> enable
      .postStratMask = strategyPostMask,                  ///< Post fire: strat-enable & mask -> strat-enable
      .user          = static_cast<uintptr_t>(user)       ///< Opaque value copied into `EpmFireReport`.
    };
    ekaRC = epmPayloadHeapCopy(dev, coreId,
			       static_cast<epm_strategyid_t>(testStrategyId),
			       heapOffset,
			       epmAction.length,
			       (const void *)pkt2send);

    if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %d",ekaRC);
    heapOffset += epmAction.length + fcsOffset;
	
    ekaRC = epmSetAction(dev, coreId, testStrategyId, configuredAction[actionIdx], &epmAction);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %d",ekaRC);
  }

  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  /* ============================================== */

  const EpmTrigger epmTrigger = {
    .token    = token,           
    .strategy = testStrategyId,  
    .action   = configuredAction[0]
  };

  if (sendto(triggerSock,&epmTrigger,sizeof(EpmTrigger),0,triggerMcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");

  /* ekaRC = epmRaiseTriggers(dev,coreId, &epmTrigger); */
  /* if (ekaRC != EKA_OPRESULT__OK) on_error("epmRaiseTriggers failed: ekaRC = %d",ekaRC); */
   /* ============================================== */

  sleep(2);
  char rxBuf[2000] = {};
  int bytes_read = recv(tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("EPM fires received by TCP:\n%s",rxBuf);
  else EKA_LOG("No fires\n");
   /* ============================================== */

  int numExpectedEvents = sizeof(expectedAction)/sizeof(expectedAction[0]);

  if (numExpectedEvents != numFireEvents) {
    on_error(RED "Number of Fire Events mismatch: Expected %d != numFireEvents %d" RESET,
	     numExpectedEvents, numFireEvents);
  } 

  for (auto i = 0; i < numFireEvents; i++) {
    if (FireEvent[i] == NULL) on_error("FireEvent[%d] == NULL",i);
    EpmFireReport* report = (EpmFireReport*)FireEvent[i];
    if (report->strategyId != testStrategyId) 
      on_error("StrategyId mismatch: report->strategyId %d != testStrategyId %d",
	       report->strategyId, testStrategyId);

    if (report->actionId != expectedAction[i])
      on_error("ActionId mismatch: report->actionId %d != expectedAction[%d] %d",
	       report->actionId, i, expectedAction[i]);

  }
  /* ============================================== */
  cleanFireEvents();

  /* ============================================== */

  EKA_LOG(GRN "%s: PASSED" RESET,__func__);
  return true;
}

/* --------------------------------------------- */
bool invalidActionTest(EkaDev* dev, ExcConnHandle conn, int tcpServerSock, int triggerSock, const sockaddr* triggerMcAddr) {
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;

  epm_enablebits_t enableBitmap = 0xabcd;
  uint64_t         user         = 0x1122334455667788;
  EkaCoreId        coreId       = 0; // Unused
  epm_token_t      token        = 0x5566778899aabbcc;

  epm_actionid_t configuredAction[] = {
    9, 4, 155, 13, EPM_LAST_ACTION
  };
  epm_actionid_t expectedAction[] = {
    9, 4, 155
  };

  epm_actionid_t invalidActionId = 155;

  const epm_strategyid_t testStrategyId = 4;
  /* ============================================== */

  ekaRC = epmEnableController(dev,coreId,false);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  epmSetStrategyEnableBits(dev, coreId, testStrategyId, enableBitmap);

  for (auto actionIdx = 0; actionIdx < (int) (sizeof(configuredAction)/sizeof(configuredAction[0])); actionIdx++) {
    if (configuredAction[actionIdx] == EPM_LAST_ACTION) continue;

    epm_actionid_t nextAction = configuredAction[actionIdx+1];
    char pkt2send[1000] = {};
    sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%3u, next=%5u EOP\n",
	    testStrategyId,actionIdx,static_cast<uint>(configuredAction[actionIdx]),nextAction);

    heapOffset = heapOffset + dataAlignment - (heapOffset % dataAlignment) + nwHdrOffset;

    epm_enablebits_t actionEnable     = enableBitmap;
    epm_enablebits_t actionPostMask   = enableBitmap;
    epm_enablebits_t strategyPostMask = enableBitmap;

    uint32_t actionFlags = configuredAction[actionIdx] == invalidActionId ? 0 : AF_Valid;

    EpmAction epmAction = {
      .token         = token,                             ///< Security token
      .hConn         = conn,                              ///< TCP connection where segments will be sent
      .offset        = heapOffset,                        ///< Offset to payload in payload heap
      .length        = (uint32_t)strlen(pkt2send),        ///< Payload length
      .actionFlags   = actionFlags,                       ///< Behavior flags (see EpmActionFlag)
      .nextAction    = nextAction,                        ///< Next action in sequence, or EPM_LAST_ACTION
      .enable        = actionEnable,                      ///< Enable bits
      .postLocalMask = actionPostMask,                    ///< Post fire: enable & mask -> enable
      .postStratMask = strategyPostMask,                  ///< Post fire: strat-enable & mask -> strat-enable
      .user          = static_cast<uintptr_t>(user)       ///< Opaque value copied into `EpmFireReport`.
    };
    ekaRC = epmPayloadHeapCopy(dev, coreId,
			       static_cast<epm_strategyid_t>(testStrategyId),
			       heapOffset,
			       epmAction.length,
			       (const void *)pkt2send);

    if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %d",ekaRC);
    heapOffset += epmAction.length + fcsOffset;
	
    ekaRC = epmSetAction(dev, coreId, testStrategyId, configuredAction[actionIdx], &epmAction);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %d",ekaRC);
  }

  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  /* ============================================== */

  const EpmTrigger epmTrigger = {
    .token    = token,           
    .strategy = testStrategyId,  
    .action   = configuredAction[0]
  };

  if (sendto(triggerSock,&epmTrigger,sizeof(EpmTrigger),0,triggerMcAddr,sizeof(sockaddr)) < 0) 
    on_error ("MC trigger send failed");

  /* ============================================== */

  sleep(1);
  char rxBuf[2000] = {};
  int bytes_read = recv(tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("EPM fires received by TCP:\n%s",rxBuf);
  else EKA_LOG("No fires\n");
  /* ============================================== */

  int numExpectedEvents = sizeof(expectedAction)/sizeof(expectedAction[0]);

  for (auto i = 0; i < numFireEvents; i++) {
    if (FireEvent[i] == NULL) on_error("FireEvent[%d] == NULL",i);

    EpmFireReport* report = (EpmFireReport*)FireEvent[i];
    printFireReport(report);

    if (report->strategyId != testStrategyId) 
      on_error("StrategyId mismatch: report->strategyId %d != testStrategyId %d",
	       report->strategyId, testStrategyId);

    if (report->actionId != expectedAction[i])
      on_error("ActionId mismatch: report->actionId %d != expectedAction[%d] %d",
	       report->actionId, i, expectedAction[i]);

    
  }

  if (numExpectedEvents != numFireEvents) {
    on_error(RED "Number of Fire Events mismatch: Expected %d != numFireEvents %d" RESET,
  	     numExpectedEvents, numFireEvents);
  }


  /* ============================================== */
  cleanFireEvents();

  /* ============================================== */

  EKA_LOG(GRN "%s: PASSED" RESET,__func__);
  return true;
}
/* --------------------------------------------- */
bool invalidTokenTest(EkaDev* dev, ExcConnHandle conn, int tcpServerSock, int triggerSock, const sockaddr* triggerMcAddr) {
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;

  epm_enablebits_t enableBitmap = 0xabcd;
  uint64_t         user         = 0x1122334455667788;
  EkaCoreId        coreId       = 0; // Unused
  epm_token_t      token        = 0x5566778899aabbcc;

  epm_actionid_t configuredAction[] = {
    9, 4, 155, 13, EPM_LAST_ACTION
  };
  epm_actionid_t expectedAction[] = {
    9, 4, 155
  };

  epm_actionid_t invalidTokenActionId = 155;

  const epm_strategyid_t testStrategyId = 4;
  /* ============================================== */

  ekaRC = epmEnableController(dev,coreId,false);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  epmSetStrategyEnableBits(dev, coreId, testStrategyId, enableBitmap);

  for (auto actionIdx = 0; actionIdx < (int) (sizeof(configuredAction)/sizeof(configuredAction[0])); actionIdx++) {
    if (configuredAction[actionIdx] == EPM_LAST_ACTION) continue;

    epm_actionid_t nextAction = configuredAction[actionIdx+1];
    char pkt2send[1000] = {};
    sprintf(pkt2send,"Action Pkt: strategy=%d, action-in-chain=%d, actionId=%3u, next=%5u EOP\n",
	    testStrategyId,actionIdx,static_cast<uint>(configuredAction[actionIdx]),nextAction);

    heapOffset = heapOffset + dataAlignment - (heapOffset % dataAlignment) + nwHdrOffset;

    epm_enablebits_t actionEnable     = enableBitmap;
    epm_enablebits_t actionPostMask   = enableBitmap;
    epm_enablebits_t strategyPostMask = enableBitmap;

    uint32_t actionFlags = AF_Valid;

    EpmAction epmAction = {
      .token         = configuredAction[actionIdx] == invalidTokenActionId ? 1 : token,  ///< Security token
      .hConn         = conn,                              ///< TCP connection where segments will be sent
      .offset        = heapOffset,                        ///< Offset to payload in payload heap
      .length        = (uint32_t)strlen(pkt2send),        ///< Payload length
      .actionFlags   = actionFlags,                       ///< Behavior flags (see EpmActionFlag)
      .nextAction    = nextAction,                        ///< Next action in sequence, or EPM_LAST_ACTION
      .enable        = actionEnable,                      ///< Enable bits
      .postLocalMask = actionPostMask,                    ///< Post fire: enable & mask -> enable
      .postStratMask = strategyPostMask,                  ///< Post fire: strat-enable & mask -> strat-enable
      .user          = static_cast<uintptr_t>(user)       ///< Opaque value copied into `EpmFireReport`.
    };
    ekaRC = epmPayloadHeapCopy(dev, coreId,
			       static_cast<epm_strategyid_t>(testStrategyId),
			       heapOffset,
			       epmAction.length,
			       (const void *)pkt2send);

    if (ekaRC != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy failed: ekaRC = %d",ekaRC);
    heapOffset += epmAction.length + fcsOffset;
	
    ekaRC = epmSetAction(dev, coreId, testStrategyId, configuredAction[actionIdx], &epmAction);
    if (ekaRC != EKA_OPRESULT__OK) on_error("epmSetAction failed: ekaRC = %d",ekaRC);
  }

  /* ============================================== */
  ekaRC = epmEnableController(dev,coreId,true);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmEnableController failed: ekaRC = %d",ekaRC);

  /* ============================================== */

  const EpmTrigger epmTrigger = {
    .token    = token,           
    .strategy = testStrategyId,  
    .action   = configuredAction[0]
  };

  if (sendto(triggerSock,&epmTrigger,sizeof(EpmTrigger),0,triggerMcAddr,sizeof(sockaddr)) < 0) 
    on_error ("MC trigger send failed");

  /* ============================================== */

  sleep(1);
  char rxBuf[2000] = {};
  int bytes_read = recv(tcpServerSock, rxBuf, sizeof(rxBuf), 0);
  if (bytes_read > 0) EKA_LOG("EPM fires received by TCP:\n%s",rxBuf);
  else EKA_LOG("No fires\n");
  /* ============================================== */

  int numExpectedEvents = sizeof(expectedAction)/sizeof(expectedAction[0]);

  for (auto i = 0; i < numFireEvents; i++) {
    if (FireEvent[i] == NULL) on_error("FireEvent[%d] == NULL",i);

    EpmFireReport* report = (EpmFireReport*)FireEvent[i];
    printFireReport(report);

    if (report->strategyId != testStrategyId) 
      on_error("StrategyId mismatch: report->strategyId %d != testStrategyId %d",
	       report->strategyId, testStrategyId);

    if (report->actionId != expectedAction[i])
      on_error("ActionId mismatch: report->actionId %d != expectedAction[%d] %d",
	       report->actionId, i, expectedAction[i]);

    
  }

  if (numExpectedEvents != numFireEvents) {
    on_error(RED "Number of Fire Events mismatch: Expected %d != numFireEvents %d" RESET,
  	     numExpectedEvents, numFireEvents);
  }


  /* ============================================== */
  cleanFireEvents();

  /* ============================================== */

  EKA_LOG(GRN "%s: PASSED" RESET,__func__);
  return true;
}


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
  /* Configuring Strategies */

  struct sockaddr_in triggerMcAddr = {};
  triggerMcAddr.sin_family      = AF_INET;  
  triggerMcAddr.sin_addr.s_addr = inet_addr(triggerIp.c_str());
  triggerMcAddr.sin_port        = be16toh(triggerUdpPort);

  const epm_strategyid_t numStrategies = 7;
  const epm_actionid_t   numActions    = 200;
  EpmStrategyParams strategyParams[numStrategies] = {};
  for (auto i = 0; i < numStrategies; i++) {
    strategyParams[i].numActions  = numActions;
    strategyParams[i].triggerAddr = reinterpret_cast<const sockaddr*>(&triggerMcAddr);
    strategyParams[i].reportCb    = fireReportCb;
  }

  EKA_LOG("Configuring %u Strategies with up %u Actions per Strategy",numStrategies,numActions);
  ekaRC = epmInitStrategies(dev, coreId, strategyParams, numStrategies);
  if (ekaRC != EKA_OPRESULT__OK) on_error("epmInitStrategies failed: ekaRC = %d",ekaRC);

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
  basicChainTest        (dev, conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr);
  /* ============================================== */
  skipActionInChainTest (dev, conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr);
  /* ============================================== */
  invalidActionTest     (dev, conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr);
  /* ============================================== */
  invalidTokenTest     (dev, conn, tcpServerSock, triggerSock, (const sockaddr*) &triggerMcAddr);
  /* ============================================== */

  fflush(stdout);fflush(stderr);

  keep_work = false;
  sleep(1);

  /* ============================================== */

  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}
