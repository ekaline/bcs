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
#include "EkaFhBatsParser.h"

/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static const int MaxFireEvents = 10000;
static volatile EpmFireReport* FireEvent[MaxFireEvents] = {};
static volatile int numFireEvents = 0;

static const int TEST_NUMOFSEC = 1;

static const uint64_t TEST_BATS_SEC_ID =  0x0003c40e;
static const uint64_t DUMMY_BATS_SEC_ID =  0x0003c40f;
static const uint64_t AlwaysFire      = 0xadcd;
static const uint64_t DefaultToken    = 0x1122334455667788;

static const int      DatagramOffset  = 54; // 14 + 20 + 20
static const int      MaxTcpTestSessions     = 16;
static const int      MaxUdpTestSessions     = 64;

static const uint64_t FireEntryHeapSize = 256;


/* --------------------------------------------- */
struct CboePitchAddOrderShortPkt {
  batspitch_sequenced_unit_header hdr;
  batspitch_add_order_short       addOrderShort;
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
void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fireReportBuf, size_t size) {
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
    excSend (dev, conn[i], pkt, strlen(pkt),0);
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
      .feedVer = EfhFeedVer::kCBOE
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
      char            id[8];
      EkaLSI          groupId;
      FixedPrice      bidMinPrice;
      FixedPrice      askMaxPrice;
      uint8_t         size;
  };

  SecurityCtx security[] = {
      {{'\0','\0','0','2','T','E','S','T'}, 0, 100, 200, 1}, // correct SecID
      {{'T','S','E','T','2','0','\0','\0'}, 0, 300, 400, 1}, // wrong   SecID
  };


  // list of secIDs to be passed to efcEnableFiringOnSec()
  uint64_t securityList[std::size(security)] = {};


  // This Alphanumeric-to-binary converting is not needed if
  // binary security ID is received from new version of EFH
  for (auto i = 0; i < (int)std::size(security); i++) {
      securityList[i] = be64toh(*(uint64_t*)security[i].id);
      EKA_TEST("securityList[%d] = '%c%c%c%c%c%c%c%c' =  %016jx",
	       i,
	       security[i].id[0],
	       security[i].id[1],
	       security[i].id[2],
	       security[i].id[3],
	       security[i].id[4],
	       security[i].id[5],
	       security[i].id[6],
	       security[i].id[7],
	       securityList[i]);
  }

  // subscribing on list of securities
  efcEnableFiringOnSec(pEfcCtx, securityList, std::size(security));

  // ==============================================
  // setting security contexts
  for (auto i = 0; i < (int)std::size(securityList); i++) {
    auto handle = getSecCtxHandle(pEfcCtx, securityList[i]);
    if (handle < 0) {
      EKA_WARN("Security[%d] '%c%c%c%c%c%c%c%c' was not fit into FPGA hash: handle = %jd",
	       i,
	       security[i].id[0],
	       security[i].id[1],
	       security[i].id[2],
	       security[i].id[3],
	       security[i].id[4],
	       security[i].id[5],
	       security[i].id[6],
	       security[i].id[7],
	       handle);
      continue;
    }
    SecCtx secCtx = {
      .bidMinPrice       = security[i].bidMinPrice,  //x100, should be nonzero
      .askMaxPrice       = security[i].askMaxPrice,  //x100
      .size              = security[i].size,
      .verNum            = 0xaf,                     // just a number
      .lowerBytesOfSecId = (uint8_t)(securityList[i] & 0xFF)
    };
    EKA_TEST("Setting StaticSecCtx[%d] secId=0x%016jx, handle=%jd",
	     i,securityList[i],handle);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK) on_error ("failed to efcSetStaticSecCtx");
  }

  // ==============================================
  // Manually prepared FPGA firing template

  const BoeNewOrderMsg fireMsg = {
      .StartOfMessage    = 0xBABA,
      .MessageLength     = sizeof(BoeNewOrderMsg) - 2,
      .MessageType       = 0x38,
      .MatchingUnit      = 0,
      .SequenceNumber    = 0,
      .ClOrdID           = {'E','K','A','t','e','s','t'},
      .Side              = '_',  // '1'-Bid, '2'-Ask
      .OrderQty          = 0,
      .NumberOfBitfields = 0x2,
      .NewOrderBitfield1 = 0x0,
      .NewOrderBitfield2 = 0x41, // (Symbol,Capacity)
      .Symbol            = {},
      .Capacity          = 'C'   // 'C','M','F',etc.
  };

  // ==============================================
  uint dataAlignment = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_PayloadAlignment);
  uint nwHdrOffset   = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_DatagramOffset);
  uint fcsOffset     = epmGetDeviceCapability(dev,EpmDeviceCapability::EHC_RequiredTailPadding);
  uint heapOffset    = 0;

  const EpmAction fire0 = {
    .type          = EpmActionType::BoeFire,        ///< Action type
    .token         = DefaultToken,                  ///< Security token
    .hConn         = conn[0],                       ///< TCP connection where segments will be sent
    .offset        = heapOffset + nwHdrOffset,      ///< Offset to payload in payload heap
    .length        = (uint32_t)sizeof(fireMsg),     ///< Payload length
    .actionFlags   = AF_Valid,                      ///< Behavior flags (see EpmActionFlag)
    .nextAction    = EPM_LAST_ACTION,               ///< Next action in sequence, or EPM_LAST_ACTION
    .enable        = AlwaysFire,                    ///< Enable bits
    .postLocalMask = AlwaysFire,                    ///< Post fire: enable & mask -> enable
    .postStratMask = AlwaysFire,                    ///< Post fire: strat-enable & mask -> strat-enable
    .user          = 0x1234567890abcdef             ///< Opaque value copied into `EpmFireReport`.
  };


  rc = epmPayloadHeapCopy(dev, 
			  EFC_STRATEGY,
			  fire0.offset,
			  fire0.length,
			  &fireMsg);
  if (rc != EKA_OPRESULT__OK) 
    on_error("epmPayloadHeapCopy offset=%u, length=%u rc=%d",
	     fire0.offset,fire0.length,(int)rc);
  rc = epmSetAction(dev, 
		    EFC_STRATEGY, 
		    0,               // epm_actionid_t must correspond to the MC gr ID
		    &fire0);

  if (rc != EKA_OPRESULT__OK) on_error("epmSetAction returned %d",(int)rc);

  heapOffset += sizeof(fire0) + nwHdrOffset + fcsOffset;
  heapOffset += dataAlignment - (heapOffset % dataAlignment);

  // ==============================================
  efcEnableController(pEfcCtx, 0);
  // ==============================================
  efcRun(pEfcCtx, &runCtx );
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

  struct CboePitchAddOrderShort {
    batspitch_sequenced_unit_header hdr;
    batspitch_add_order_short       addOrdShort;
  } __attribute__((packed));
  
  CboePitchAddOrderShort mdAskShortPkt = {
    .hdr = {
	    .length   = sizeof(mdAskShortPkt),
	    .count    = 1,
	    .unit     = 1, // just a number
	    .sequence = 5, // just a number
    },
    .addOrdShort = {
	  .header = {
	      .length = sizeof(mdAskShortPkt.addOrdShort),
	      .type   = (uint8_t)EKA_BATS_PITCH_MSG::ADD_ORDER_SHORT,
	      .time   = 0x11223344,  // just a number
	  },
	  .order_id   = 0xaabbccddeeff5566,
	  .side       = 'S', // 'B',
	  .size       = security[0].size,
	  .symbol     = {'0','2','T','E','S','T'},
	  .price      = (uint16_t)(security[0].askMaxPrice / 100 - 1),
	  .flags      = 0xFF 
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
