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
static const uint64_t AlwaysFire      = 0xadcd;
static const uint64_t DefaultToken    = 0x1122334455667788;

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
  efcPrintFireReport(pEfcCtx, (const EfcReportHdr*)fireReportBuf);
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


/* ############################################# */
int main(int argc, char *argv[]) {
  signal(SIGINT, INThandler);

  EkaDev* dev = NULL;
  EkaCoreId coreId = 0;
  EkaOpResult rc;
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

  EKA_LOG("\n==============================\n\
UDP Trigger: %s:%u, Actions Server %s:%u, Client IP %s\n\
==============================",
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
    //    "efc.group.0.mcast.addr", "233.54.12.73:18001"
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
  rc = efcInit(&pEfcCtx,dev,&initCtx);
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
  //  eka_write(dev,(uint64_t)0xf0f00,(uint64_t)0xefa0beda); // FATAL ENABLE

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
  struct sockaddr_in triggerMcAddr = {};
  triggerMcAddr.sin_addr.s_addr = inet_addr(triggerIp.c_str());
  triggerMcAddr.sin_family      = AF_INET;
  triggerMcAddr.sin_port        = be16toh(triggerUdpPort);

  /* ============================================== */
  efcEnableFiringOnSec(pEfcCtx, (uint64_t*)&TEST_NOM_SEC_ID, 1);
  EfcSecCtxHandle handle = getSecCtxHandle(pEfcCtx, TEST_NOM_SEC_ID);
  SecCtx secCtx = {
    .bidMinPrice       = 700,  //x100, should be nonzero
    .askMaxPrice       = 800,  //x100
    .size              = 1,
    .verNum            = 0xaf,
    .lowerBytesOfSecId = (uint8_t)(TEST_NOM_SEC_ID & 0xFF)
  };
  rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
  if (rc != EKA_OPRESULT__OK) on_error ("failed to efcSetStaticSecCtx");
  /* ============================================== */
  efcSetGroupSesCtx(pEfcCtx, 0, conn );
  /* ============================================== */
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
  //  efcSetFireTemplate(pEfcCtx, conn, &fireMsg, sizeof(fireMsg));
  uint32_t fireHeapOffset = 14 + 20 + 20;
  rc = epmPayloadHeapCopy(dev, 
			  0, // coreId
			  EFC_STRATEGY,
			  fireHeapOffset,
			  sizeof(fireMsg),
			  &fireMsg);
  if (rc != EKA_OPRESULT__OK) on_error("epmPayloadHeapCopy returned %d",(int)rc);

  const EpmAction sqfFireAction = {
    .type          = EpmActionType::SqfFire,                 ///< Action type
    .token         = DefaultToken,                           ///< Security token
    .hConn         = conn,                                   ///< TCP connection where segments will be sent
    .offset        = fireHeapOffset,                         ///< Offset to payload in payload heap
    .length        = (uint32_t)sizeof(fireMsg),              ///< Payload length
    .actionFlags   = AF_Valid,                               ///< Behavior flags (see EpmActionFlag)
    .nextAction    = EPM_LAST_ACTION,                        ///< Next action in sequence, or EPM_LAST_ACTION
    .enable        = AlwaysFire,                             ///< Enable bits
    .postLocalMask = AlwaysFire,                             ///< Post fire: enable & mask -> enable
    .postStratMask = AlwaysFire,                             ///< Post fire: strat-enable & mask -> strat-enable
    .user          = 0x1234567890abcdef                      ///< Opaque value copied into `EpmFireReport`.
  };
  rc = epmSetAction(dev, 
		    0,               // coreId
		    EFC_STRATEGY, 
		    0,               // epm_actionid_t must correspond to the MC gr ID
                    &sqfFireAction);
  if (rc != EKA_OPRESULT__OK) on_error("epmSetAction returned %d",(int)rc);

  /* ============================================== */
  efcEnableController(pEfcCtx, 0);
  /* ============================================== */
  efcRun(pEfcCtx, &runCtx );
  /* ============================================== */
  efcEnableController(pEfcCtx, 0);
  /* ============================================== */
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
      .option_id             = be32toh(TEST_NOM_SEC_ID),
      .price                 = be16toh(secCtx.askMaxPrice - 1),
      .size                  = be16toh(secCtx.size)
    }
  };
    /* ============================================== */
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
      .option_id             = be32toh(TEST_NOM_SEC_ID),
      .price                 = be32toh(secCtx.bidMinPrice * 100 + 1),
      .size                  = be32toh(secCtx.size)
    }
  };

  for (int i=0;i<1;i++){
  /* ============================================== */
  EKA_LOG("sending AskShort trigger to %s:%u",
	  EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),be16toh(triggerMcAddr.sin_port));
  if (sendto(triggerSock,&mdAskShortPkt,sizeof(mdAskShortPkt),0,(const sockaddr*)&triggerMcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");
    /* ============================================== */
  sleep(2);
  }
    /* ============================================== */
  for (int i=0;i<1;i++){
    EKA_LOG("sending BidLong  trigger to %s:%u",EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),be16toh(triggerMcAddr.sin_port));
    if (sendto(triggerSock,&mdBidLongPkt,sizeof(mdBidLongPkt),0,(const sockaddr*)&triggerMcAddr,sizeof(sockaddr)) < 0) 
      on_error ("MC trigger send failed");
    sleep(2);
  }
  /* ============================================== */

#ifndef _VERILOG_SIM
  sleep(2);
  EKA_LOG("--Test finished, ctrl-c to end---");
  while (keep_work) { sleep(0); }
#endif
  //  end:
  fflush(stdout);fflush(stderr);


  /* ============================================== */

  //  excClose(dev,conn);
  printf("Closing device\n");

  sleep (1);
  ekaDevClose(dev);

  return 0;
}
