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
#include "EkaFhNomParser.h"

/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static const int MaxFireEvents = 10000;
static volatile EpmFireReport* FireEvent[MaxFireEvents] = {};
static volatile int numFireEvents = 0;

static const int TEST_NUMOFSEC = 1;

static const uint64_t TEST_NOM_SEC_ID =  0x0003c40e;
static const uint64_t DUMMY_NOM_SEC_ID =  0x0003c40f;
static const uint64_t AlwaysFire      = 0xadcd;
static const uint64_t DefaultToken    = 0x1122334455667788;

static const int      DatagramOffset  = 54; // 14 + 20 + 20
static const int      MaxTcpTestSessions     = 16;
static const int      MaxUdpTestSessions     = 64;

static const uint64_t FireEntryHeapSize = 256;


/* --------------------------------------------- */
struct NomAddOrderShortPkt {
  mold_hdr             mold;
  uint16_t             msgLen;
  itto_add_order_short addOrderShort;
};
struct NomAddOrderLongPkt {
  mold_hdr            mold;
  uint16_t            msgLen;
  itto_add_order_long addOrderLong;
};
struct NomAddOrderShortLongPkt {
  mold_hdr            mold;
  uint16_t            msgLenShort;
  itto_add_order_short addOrderShort;
  uint16_t            msgLenLong;
  itto_add_order_long addOrderLong;
};
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
void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fireReportBuf, size_t size, void* cbCtx) {
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  EKA_LOG ("FIRE REPORT RECEIVED");
  //  hexDump("FireReport",fireReportBuf,size);
  efcPrintFireReport(pEfcCtx, (const EfcReportHdr*)fireReportBuf,false);
  EKA_LOG ("Rearming...\n");
  efcEnableController(pEfcCtx,1);
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
  printf("USAGE: %s -s <Connection Server IP> -p <Connection Server TcpPort> -c <Connection Client IP> -t <Trigger IP> -u <Trigger UdpPort> -l <Length of fire chain>\n",cmd); 
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   std::string* triggerIp, uint16_t* triggerUdpPort,
		   uint16_t* numTcpSess) {
  int opt; 
  while((opt = getopt(argc, argv, ":c:s:p:u:l:t:h")) != -1) {  
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


/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  // ==============================================

  std::string serverIp  = "10.0.0.10";      // Ekaline lab default
  std::string clientIp  = "100.0.0.110";    // Ekaline lab default
  std::string triggerIp = "239.255.119.16"; // Ekaline lab default
  uint16_t serverTcpBasePort   = 22222;         // Ekaline lab default
  uint16_t numTcpSess = 4;
  uint16_t serverTcpPort = serverTcpBasePort;
  uint16_t triggerUdpPort = 18000;

  getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort,&numTcpSess);

  if (numTcpSess > MaxTcpTestSessions) 
    on_error("numTcpSess %d > MaxTcpTestSessions %d",numTcpSess, MaxTcpTestSessions);
  // ==============================================
  // EkaDev general setup
  EkaDev*     dev = NULL;
  EkaCoreId   coreId = 0;
  EkaOpResult rc;
  const EkaDevInitCtx ekaDevInitCtx = {
    .logCallback  = NULL,
    .logContext   = NULL,
    .credAcquire  = credAcquire,
    .credRelease  = credRelease,
    .credContext  = NULL,
    .createThread = createThread,
    .createThreadContext = NULL
  };
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
  // Launching TCP test Servers
  int tcpSock[MaxTcpTestSessions] = {};

  for (auto i = 0; i < numTcpSess; i++) {
    bool serverSet = false;
    std::thread server = std::thread(tcpServer,
				     dev,
				     serverIp,
				     serverTcpBasePort + i,
				     &tcpSock[i],
				     &serverSet);
    server.detach();
    while (keep_work && ! serverSet) { sleep (0); }
  }
  // ==============================================
  // Establishing EXC connections for EPM/EFC fires 

  ExcConnHandle conn[MaxTcpTestSessions]    = {};

  for (auto i = 0; i < numTcpSess; i++) {
    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
    serverAddr.sin_port        = be16toh(serverTcpBasePort + i);

    int excSock = excSocket(dev,coreId,0,0,0);
    if (excSock < 0) on_error("failed to open sock %d",i);
    conn[i] = excConnect(dev,excSock,(sockaddr*) &serverAddr, sizeof(sockaddr_in));
    if (conn[i] < 0) on_error("excConnect %d %s:%u",
			      i,EKA_IP2STR(serverAddr.sin_addr.s_addr),be16toh(serverAddr.sin_port));
    const char* pkt = "\n\nThis is 1st TCP packet sent from FPGA TCP client to Kernel TCP server\n\n";
    excSend (dev, conn[i], pkt, strlen(pkt), 0);
    int bytes_read = 0;
    char rxBuf[2000] = {};
    bytes_read = recv(tcpSock[i], rxBuf, sizeof(rxBuf), 0);
    if (bytes_read > 0) EKA_LOG("\n%s",rxBuf);
  }

  // ==============================================
  // Setup EFC MC groups
  struct IpPort {
    EkaCoreId coreId;
    uint32_t  ip;
    uint16_t  port;
    char      keyStr[80];
    char      valStr[80];
  };

  EpmTriggerParams triggerParam[] = {
    {0,"233.54.12.72",18000},
    {0,"233.54.12.73",18001},
    {0,"233.54.12.74",18002},
    {0,"233.54.12.75",18003}
  };

  EfcCtx efcCtx = {};
  EfcCtx* pEfcCtx = &efcCtx;

  EfcInitCtx initCtx = {
			.feedVer = EfhFeedVer::kNASDAQ
  };
  
  rc = efcInit(&pEfcCtx,dev,&initCtx);
  if (rc != EKA_OPRESULT__OK) on_error("efcInit returned %d",(int)rc);

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
  if (rc != EKA_OPRESULT__OK) on_error("epmInitStrategies failed: rc = %d",rc);

  // ==============================================
  // Global EFC config
  EfcStratGlobCtx efcStratGlobCtx = {
    .enable_strategy = 1,
    .report_only = 0,
    .debug_always_fire_on_unsubscribed = 0,
    .debug_always_fire = 0,
    .max_size = 1000,
    .watchdog_timeout_sec = 100000,
  };
  efcInitStrategy(pEfcCtx, &efcStratGlobCtx);

  EfcRunCtx runCtx = {};
  runCtx.onEfcFireReportCb = onFireReport;

  // ==============================================
  // Subscribing on securities
  struct SecurityCtx {
    uint64_t        id;
    EkaLSI          groupId;
    EfcSecCtxHandle handle;
    FixedPrice      bidMinPrice;
    FixedPrice      askMaxPrice;
    uint8_t         size;
  };

  SecurityCtx security[] = {
    {0x1111, 0, -1, 100, 200, 1},
    {0x2222, 1, -1, 300, 400, 1},
    {0x3333, 2, -1, 500, 600, 1},
    {0x4444, 3, -1, 700, 800, 1},
  };

  uint64_t securityList[std::size(security)] = {};

  for (auto i = 0; i < (int)std::size(security); i++) {
    securityList[i] = security[i].id;
  }

  // subscribing on list of securities
  efcEnableFiringOnSec(pEfcCtx, securityList, std::size(security));

  // ==============================================
  // setting security contexts
  for (auto i = 0; i < (int)std::size(security); i++) {
    security[i].handle = getSecCtxHandle(pEfcCtx, security[i].id);
    if (security[i].handle < 0) {
      EKA_WARN("Security %ju was not fit into FPGA hash: handle = %jd",
	       security[i].id,security[i].handle);
      continue;
    }
    SecCtx secCtx = {
      .bidMinPrice       = security[i].bidMinPrice,  //x100, should be nonzero
      .askMaxPrice       = security[i].askMaxPrice,  //x100
      .size              = security[i].size,
      .verNum            = 0xaf,                     // just a number
      .lowerBytesOfSecId = (uint8_t)(security[i].id & 0xFF)
    };
    /* EKA_LOG("Setting StaticSecCtx to handle %jd:",security[i].handle); */
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(pEfcCtx, security[i].handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK) on_error ("failed to efcSetStaticSecCtx");
  }

  // ==============================================
  // SQF message format used both for Fires and Cancels

  // there is manually prepared FPGA firing template
  // matching following message format
  const SqfShortQuoteBlockMsg fireMsg = {
    .typeSub    = {'Q','Q'},
    .badge      = 0x12345678,
    .messageId  = 0x12345678aabbccdd,
    .quoteCount = 1,
    .optionId   = 0,
    .bidPrice   = 0,
    .bidSize    = 0,
    .askPrice   = 0,
    .askSize    = 0,
    .reentry    = '1'
  };

  // ==============================================
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;
  epm_actionid_t chainActionCurrId = 64; // used to offset "chain" actions from the "first" actions
  int testCase = 0;
  // ==============================================
  // preparing action chain for MC Gr#0:
  // Single Fire + single Cancel


  // ==============================================
  // ==============================================
  // 1 Fire + 1 Cancel on a MD trigger from MC GR#0

  TEST_LOG("\n===========================\nTEST %d\n===========================\n",++testCase);


  const EpmAction sqfFire0 = {
    .type          = EpmActionType::SqfFire,                 ///< Action type
    .token         = DefaultToken,                           ///< Security token
    .hConn         = conn[0],                                ///< TCP connection where segments will be sent
    .offset        = heapOffset + nwHdrOffset,               ///< Offset to payload in payload heap
    .length        = (uint32_t)sizeof(fireMsg),              ///< Payload length
    .actionFlags   = AF_Valid,                               ///< Behavior flags (see EpmActionFlag)
    .nextAction    = chainActionCurrId,                      ///< Next action in sequence, or EPM_LAST_ACTION
    .enable        = AlwaysFire,                             ///< Enable bits
    .postLocalMask = AlwaysFire,                             ///< Post fire: enable & mask -> enable
    .postStratMask = AlwaysFire,                             ///< Post fire: strat-enable & mask -> strat-enable
    .user          = 0x1234567890abcdef                      ///< Opaque value copied into `EpmFireReport`.
  };


  rc = epmPayloadHeapCopy(dev, 
			  EFC_STRATEGY,
			  sqfFire0.offset,
			  sqfFire0.length,
			  &fireMsg);
  if (rc != EKA_OPRESULT__OK) 
    on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
	     sqfFire0.offset,sqfFire0.length,(int)rc);
  rc = epmSetAction(dev, 
		    EFC_STRATEGY, 
		    0,               // epm_actionid_t must correspond to the MC gr ID
		    &sqfFire0);

  if (rc != EKA_OPRESULT__OK) on_error("epmSetAction returned %d",(int)rc);

  heapOffset += sizeof(sqfFire0) + nwHdrOffset + fcsOffset;
  heapOffset += dataAlignment - (heapOffset % dataAlignment);

  const EpmAction sqfCancel0 = {
    .type          = EpmActionType::SqfCancel,               ///< Action type
    .token         = DefaultToken,                           ///< Security token
    .hConn         = conn[0],                                ///< TCP connection where segments will be sent
    .offset        = heapOffset + nwHdrOffset,               ///< Offset to payload in payload heap
    .length        = (uint32_t)sizeof(fireMsg),              ///< Payload length
    .actionFlags   = AF_Valid,                               ///< Behavior flags (see EpmActionFlag)
    .nextAction    = EPM_LAST_ACTION,                        ///< Next action in sequence, or EPM_LAST_ACTION
    .enable        = AlwaysFire,                             ///< Enable bits
    .postLocalMask = AlwaysFire,                             ///< Post fire: enable & mask -> enable
    .postStratMask = AlwaysFire,                             ///< Post fire: strat-enable & mask -> strat-enable
    .user          = 0x1234567890abcdef                      ///< Opaque value copied into `EpmFireReport`.
  };

  rc = epmPayloadHeapCopy(dev, 
			  EFC_STRATEGY,
			  sqfCancel0.offset,
			  sqfCancel0.length,
			  &fireMsg);
  if (rc != EKA_OPRESULT__OK) 
    on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
	     sqfCancel0.offset,sqfCancel0.length,(int)rc);

  heapOffset += sizeof(sqfCancel0) + nwHdrOffset + fcsOffset;
  heapOffset += dataAlignment - (heapOffset % dataAlignment);

  rc = epmSetAction(dev, 
		    EFC_STRATEGY, 
		    chainActionCurrId,               
		    &sqfCancel0);

  if (rc != EKA_OPRESULT__OK) on_error("epmSetAction returned %d",(int)rc);
  chainActionCurrId++;

  // ==============================================
  efcEnableController(pEfcCtx, 0);
  // ==============================================
  efcRun(pEfcCtx, &runCtx );
  // ==============================================

  // ==============================================
  // Preparing UDP MC for MD trigger on GR#0

  int triggerSock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (triggerSock < 0) on_error("failed to open UDP sock");

  struct sockaddr_in triggerSourceAddr = {};
  triggerSourceAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
  triggerSourceAddr.sin_family      = AF_INET;
  triggerSourceAddr.sin_port        = 0; //be16toh(serverTcpPort);

  if (bind(triggerSock,(sockaddr*)&triggerSourceAddr, sizeof(sockaddr)) != 0 ) {
    on_error("failed to bind server triggerSock to %s:%u",
	     EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),be16toh(triggerSourceAddr.sin_port));
  } else {
    EKA_LOG("triggerSock is binded to %s:%u",
	    EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),be16toh(triggerSourceAddr.sin_port));
  }
  struct sockaddr_in triggerMcAddr = {};
  triggerMcAddr.sin_family      = AF_INET;
  triggerMcAddr.sin_addr.s_addr = inet_addr(triggerParam[0].mcIp);
  triggerMcAddr.sin_port        = be16toh(triggerParam[0].mcUdpPort);

  // ==============================================
  // MD trigger message A on GR#0, security #0

  NomAddOrderShortPkt mdAskShortPkt = {
    .mold = {
      .session_id  = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa},
      .sequence    = be64toh(123),
      .message_cnt = be16toh(1)
    },
    .msgLen = be16toh(sizeof(itto_add_order_short)),
    .addOrderShort = {
      .type                  = 'a',
      .tracking_num          = be16toh(0xbeda),
      .time_nano             = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
      .order_reference_delta = be64toh(0x1234567890abcdef),
      .side                  = 'S',
      .option_id             = be32toh(security[0].id),
      .price                 = be16toh(security[0].askMaxPrice - 1),
      .size                  = be16toh(security[0].size)
    }
  };

  // ==============================================
  // Sending MD trigger MC GR#0
  EKA_LOG("sending AskShort trigger to %s:%u",
	  EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),be16toh(triggerMcAddr.sin_port));
  if (sendto(triggerSock,&mdAskShortPkt,sizeof(mdAskShortPkt),0,(const sockaddr*)&triggerMcAddr,sizeof(sockaddr)) < 0) 
    on_error ("MC trigger send failed");

  efcEnableController(pEfcCtx, 1);
  sleep(1);

  // ==============================================
  // ==============================================
  // 4 Fires on a MD trigger from MC GR#1 + 4 Cancels

  TEST_LOG("\n===========================\nTEST %d\n===========================\n",++testCase);

  static const int numSessFires = 4;

  epm_actionid_t currActionId = 1;
  epm_actionid_t nextActionId = chainActionCurrId;


  for (auto i = 0; i < numSessFires * 2; i ++) {
    const EpmAction chainAction {
    .type          = i < numSessFires ? EpmActionType::SqfFire :  EpmActionType::SqfCancel,
    .token         = DefaultToken,
    .hConn         = conn[i],
    .offset        = heapOffset + nwHdrOffset,
    .length        = (uint32_t)sizeof(fireMsg),
    .actionFlags   = AF_Valid,
    .nextAction    = i == numSessFires * 2 - 1 ? EPM_LAST_ACTION : nextActionId,
    .enable        = AlwaysFire,
    .postLocalMask = AlwaysFire,
    .postStratMask = AlwaysFire,
    .user          = 0x1234567890abcdef,
    };

    rc = epmPayloadHeapCopy(dev, 
			    EFC_STRATEGY,
			    chainAction.offset,
			    chainAction.length,
			    &fireMsg);
    if (rc != EKA_OPRESULT__OK) 
      on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
	       chainAction.offset,chainAction.length,(int)rc);
    rc = epmSetAction(dev, 
		      EFC_STRATEGY, 
		      currActionId, 
		      &chainAction);

    if (rc != EKA_OPRESULT__OK) on_error("epmSetAction returned %d",(int)rc);


    heapOffset += sizeof(chainAction) + nwHdrOffset + fcsOffset;
    heapOffset += dataAlignment - (heapOffset % dataAlignment);

    currActionId = nextActionId++;

  };


  // ==============================================
  // MD trigger message B on GR#1, security #1

  NomAddOrderLongPkt mdBidLongPkt = {
    .mold = {
      .session_id  = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa},
      .sequence    = be64toh(124),
      .message_cnt = be16toh(1)
    },
    .msgLen = be16toh(sizeof(itto_add_order_long)),
    .addOrderLong = {
      .type                  = 'A',
      .tracking_num          = be16toh(0xbeda),
      .time_nano             = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
      .order_reference_delta = be64toh(0x1234567890abcdef),
      .side                  = 'B',
      .option_id             = be32toh(security[1].id),
      .price                 = be32toh(security[1].bidMinPrice * 100 + 1),
      .size                  = be32toh(security[1].size)
    }
  };

  // ==============================================
  // Sending MD trigger MC GR#0

  triggerMcAddr.sin_family      = AF_INET;
  triggerMcAddr.sin_addr.s_addr = inet_addr(triggerParam[1].mcIp);
  triggerMcAddr.sin_port        = be16toh(triggerParam[1].mcUdpPort);

  EKA_LOG("sending AskShort trigger to %s:%u",
	  EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),be16toh(triggerMcAddr.sin_port));
  if (sendto(triggerSock,&mdBidLongPkt,sizeof(mdBidLongPkt),0,(const sockaddr*)&triggerMcAddr,sizeof(sockaddr)) < 0) 
    on_error ("MC trigger send failed");

  efcEnableController(pEfcCtx, 1);
  sleep(10);


  // ==============================================
  // ==============================================
  // Same chain as previous test, but using SW trigger
  // 4 SQF Fires + 4 Cancels
  TEST_LOG("\n===========================\nTEST %d\n===========================\n",++testCase);

  EpmTrigger swTrigger = {
    .token    = DefaultToken,
    .strategy = EFC_STRATEGY,
    .action   = 1
  };

  rc = epmRaiseTriggers(dev, &swTrigger);
  if (rc != EKA_OPRESULT__OK) EKA_WARN("epmRaiseTriggers: rc = %d",(int)rc);

  sleep(1);

#if 0

    /* ============================================== */
  NomAddOrderShortLongPkt mdAskShortBidLongPkt = {
    .mold = {
      .session_id  = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa},
      .sequence    = be64toh(125),
      .message_cnt = be16toh(2)
    },
    .msgLenShort = be16toh(sizeof(itto_add_order_short)),
    .addOrderShort = {
      .type                  = 'a',
      .tracking_num          = be16toh(0xbeda),
      .time_nano             = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
      .order_reference_delta = be64toh(0x1234567890abcdef),
      .side                  = 'S',
      .option_id             = be32toh(TEST_NOM_SEC_ID),
      .price                 = be16toh(secCtx.askMaxPrice - 1),
      .size                  = be16toh(secCtx.size)
    },
    .msgLenLong = be16toh(sizeof(itto_add_order_long)),
    .addOrderLong = {
      .type                  = 'A',
      .tracking_num          = be16toh(0xbeda),
      .time_nano             = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66},
      .order_reference_delta = be64toh(0x1234567890abcdef),
      .side                  = 'B',
      .option_id             = be32toh(DUMMY_NOM_SEC_ID),
      .price                 = be32toh(secCtx.bidMinPrice * 100 + 1),
      .size                  = be32toh(secCtx.size)
    }
  };

#endif

  TEST_LOG("\n===========================\nEND OT TESTS\n===========================\n");

#ifndef _VERILOG_SIM
  sleep(2);
  EKA_LOG("--Test finished, ctrl-c to end---");
  keep_work = false;
  while (keep_work) { sleep(0); }
#endif
  fflush(stdout);fflush(stderr);


  /* ============================================== */

  printf("Closing device\n");

  ekaDevClose(dev);

  return 0;
}
