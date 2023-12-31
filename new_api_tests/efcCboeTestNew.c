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
#include "EfhTestFuncs.h"

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

static const int TEST_NUMOFSEC = 1;

static const uint64_t TEST_BATS_SEC_ID =  0x0003c40e;
static const uint64_t DUMMY_BATS_SEC_ID =  0x0003c40f;
static const uint64_t AlwaysFire      = 0xadcd;
static const uint64_t DefaultToken    = 0x1122334455667788;

//static const int      DatagramOffset  = 54; // 14 + 20 + 20
static const int      MaxTcpTestSessions     = 16;
static const int      MaxUdpTestSessions     = 64;

static const uint64_t FireEntryHeapSize = 256;

static const int      TotalInjects       = 10;

  int      ExpectedFires    = 0;
  int      ReportedFires    = 0;

/* --------------------------------------------- */

struct SecurityCtx {
    char            id[8];
    EkaLSI          groupId;
    FixedPrice      bidMinPrice;
    FixedPrice      askMaxPrice;
    uint8_t         size;
};

enum class AddOrder : int {
    Unknown = 0,
	Short,
	Long,
	Expanded
};

struct CboePitchAddOrderShort {
    sequenced_unit_header hdr;
    add_order_short       msg;
} __attribute__((packed));

struct CboePitchAddOrderLong {
    sequenced_unit_header hdr;
    add_order_long        msg;
} __attribute__((packed));

struct CboePitchAddOrderExpanded {
    sequenced_unit_header hdr;
    add_order_expanded    msg;
} __attribute__((packed));

/* --------------------------------------------- */
int getHWFireCnt(EkaDev* dev, uint64_t addr) {
    uint64_t var_pass_counter = eka_read(dev, addr);
    int real_val =  (var_pass_counter>>32) & 0xffffffff;
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

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",user,EKA_EXCH_DECODE(group.source),group.localId);
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
	 "-e <Exit at the end of the test>"
	 "\n",cmd);
  return;
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[],
		   std::string* serverIp, uint16_t* serverTcpPort, 
		   std::string* clientIp, 
		   std::string* triggerIp, uint16_t* triggerUdpPort,
		   uint16_t* numTcpSess, bool* runEfh, bool* fatalDebug, bool* dontExit,
		   bool *reportOnly) {
	int opt; 
	while((opt = getopt(argc, argv, ":c:s:p:u:l:t:fdher")) != -1) {  
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
		case 'r':  
			printf("reportOnly = ON\n");
			*reportOnly = true;
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

static int sendAddOrderShort (int sock, const sockaddr_in* addr,
															char* secId, uint32_t sequence,
															char side, uint16_t price,
															uint16_t size) {
	CboePitchAddOrderShort pkt = {
		.hdr = {
	    .length   = sizeof(pkt),
	    .count    = 1,
	    .unit     = 1, // just a number
	    .sequence = sequence,
		},
		.msg = {
	    .header = {
				.length = sizeof(pkt.msg),
				.type   = (uint8_t)MsgId::ADD_ORDER_SHORT,
				.time   = 0x11223344,  // just a number
	    },
	    .order_id   = 0xaabbccddeeff5566,
	    .side       = side,
	    .size       = size,
	    .symbol     = {secId[2],secId[3],secId[4],secId[5],
										 secId[6],secId[7]},
	    .price      = price,
	    .flags      = 0xFF,
		}
	};
	TEST_LOG("sending AddOrderShort trigger to %s:%u",
					 EKA_IP2STR(addr->sin_addr.s_addr),
					 be16toh(addr->sin_port));
	if (sendto(sock,&pkt,sizeof(pkt),0,
						 (const sockaddr*)addr,sizeof(sockaddr)) < 0) 
		on_error ("MC trigger send failed");

	return 0;
}
/* --------------------------------------------- */

static int sendAddOrderLong (int sock, const sockaddr_in* addr,
														 char* secId, uint32_t sequence,
														 char side, uint64_t price,
														 uint32_t size) {
	CboePitchAddOrderLong pkt = {
		.hdr = {
	    .length   = sizeof(pkt),
	    .count    = 1,
	    .unit     = 1, // just a number
	    .sequence = sequence,
		},
		.msg = {
	    .header = {
				.length = sizeof(pkt.msg),
				.type   = (uint8_t)MsgId::ADD_ORDER_LONG,
				.time   = 0x11223344,  // just a number
	    },
	    .order_id   = 0xaabbccddeeff5566,
	    .side       = side,
	    .size       = size,
	    .symbol     = {secId[2],secId[3],secId[4],secId[5],
										 secId[6],secId[7]},
	    .price      = price,
	    .flags      = 0xFF,
		}
	};
	TEST_LOG("sending AddOrderLong trigger to %s:%u",
					 EKA_IP2STR(addr->sin_addr.s_addr),
					 be16toh(addr->sin_port));
	if (sendto(sock,&pkt,sizeof(pkt),0,
						 (const sockaddr*)addr,sizeof(sockaddr)) < 0) 
		on_error ("MC trigger send failed");

	return 0;
}

/* --------------------------------------------- */

static int sendAddOrderExpanded (int sock, const sockaddr_in* addr,
																 char* secId, uint32_t sequence,
																 char side, uint16_t price,
																 uint16_t size) {
	CboePitchAddOrderExpanded pkt = {
		.hdr = {
	    .length   = sizeof(pkt),
	    .count    = 1,
	    .unit     = 1, // just a number
	    .sequence = sequence,
		},
		.msg = {
	    .header = {
				.length = sizeof(pkt.msg),
				.type   = (uint8_t)MsgId::ADD_ORDER_EXPANDED,
				.time   = 0x11223344,  // just a number
	    },
	    .order_id   = 0xaabbccddeeff5566,
	    .side       = side,
	    .size       = size,
	    .exp_symbol = {secId[2],secId[3],secId[4],secId[5],
										 secId[6],secId[7],' ',' '},
	    .price      = price, 
	    .flags      = 0xFF,
	    .participant_id     = {},
	    .customer_indicator = 'C',
	    .client_id          = {}
		}
	};

	TEST_LOG("sending AddOrderExpanded trigger to %s:%u",
					 EKA_IP2STR(addr->sin_addr.s_addr),
					 be16toh(addr->sin_port));
	if (sendto(sock,&pkt,sizeof(pkt),0,
						 (const sockaddr*)addr,sizeof(sockaddr)) < 0) 
		on_error ("MC trigger send failed");

	return 0;
}
/* --------------------------------------------- */

static int sendPreloadedPkt (int sock, const sockaddr_in* addr,
			     const char* pkt, size_t size) {
  struct CboePitchAddOrderExpanded {
    sequenced_unit_header hdr;
    add_order_expanded    msg;
  } __attribute__((packed));

  TEST_LOG("preloadedPkt = \'%s\'",pkt);
  uint8_t bytes[1536] = {};
  std::string hex = std::string(pkt);
  for (unsigned int i = 0; i < hex.length(); i += 2) {
    std::string byteString = hex.substr(i, 2);
    bytes[i/2] = (uint8_t) strtol(byteString.c_str(), NULL, 16);
  }
  hexDump("preloadedPkt",bytes,size/2);
  TEST_LOG("sending preloaded pkt to %s:%u",
	   EKA_IP2STR(addr->sin_addr.s_addr),be16toh(addr->sin_port));
  if (sendto(sock,bytes,1400,0,(const sockaddr*)addr,
						 sizeof(sockaddr)) < 0) 
    on_error ("MC trigger send failed");

  return 0;
}
/* --------------------------------------------- */
static int sendAddOrder (AddOrder type, int sock,
												 const sockaddr_in* addr, char* secId,
												 uint32_t sequence, char side,
												 uint64_t price, uint32_t size) {
	switch (type) {
	case AddOrder::Short :
		return sendAddOrderShort (sock, addr, secId,
															sequence, side, price, size);
	case AddOrder::Long :
		return sendAddOrderLong (sock, addr, secId,
														 sequence, side, price, size);
	case AddOrder::Expanded :
		return sendAddOrderExpanded (sock, addr, secId,
																 sequence, side, price, size);
	default:
		on_error("Unexpected type %d",(int)type);
	}
	return 0;
}


/* ############################################# */
int main(int argc, char *argv[]) {
  
  signal(SIGINT, INThandler);
  testCtx = new TestCtx;
  if (!testCtx) on_error("testCtx == NULL");
  // ==============================================

  std::string serverIp        = "10.0.0.10";  // Ekaline lab default
  std::string clientIp        = "100.0.0.110";// Ekaline lab default
  std::string triggerIp       = "224.0.74.0"; // (C1 CC feed)
  uint16_t triggerUdpPort     = 30301;        // C1 CC gr#0
  uint16_t serverTcpBasePort  = 22222;        // Ekaline lab default
  uint16_t numTcpSess         = 1;
  uint16_t serverTcpPort      = serverTcpBasePort;
  bool     runEfh             = false;
  bool     fatalDebug          = false;
  bool     dontExit          = true;
  bool     reportOnly         = false;
  
  getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,
					&triggerIp,&triggerUdpPort,&numTcpSess,&runEfh,
					&fatalDebug,&dontExit,&reportOnly);

  if (numTcpSess > MaxTcpTestSessions) 
    on_error("numTcpSess %d > MaxTcpTestSessions %d",
						 numTcpSess, MaxTcpTestSessions);
  // ==============================================
  // EkaDev general setup
  EfcArmVer   armVer = 0;
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
    while (testCtx->keep_work && ! serverSet) { sleep (0); }
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
    conn[i] = excConnect(dev,excSock,(sockaddr*) &serverAddr,
												 sizeof(sockaddr_in));
    if (conn[i] < 0)
			on_error("excConnect %d %s:%u",
							 i,EKA_IP2STR(serverAddr.sin_addr.s_addr),
							 be16toh(serverAddr.sin_port));
    const char* pkt = "\n\nThis is 1st TCP packet sent "
			"from FPGA TCP client to Kernel TCP server\n\n";
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
    {0,"224.0.74.0",30301},
    /* {0,"224.0.74.1",30302}, */
    /* {0,"224.0.74.2",30303}, */
    /* {0,"224.0.74.3",30304}, */
  };

  EfcCtx efcCtx = {};
  EfcCtx* pEfcCtx = &efcCtx;

  EfcInitCtx initCtx = {
			.feedVer = EfhFeedVer::kCBOE,
			.testRun = true	
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
    .enable_strategy = 1,
    .report_only          = (uint8_t)reportOnly,
    .debug_always_fire_on_unsubscribed = 1,
    .debug_always_fire = 1,
    .max_size = 1000,
    .watchdog_timeout_sec = 100000,
  };
  efcInitStrategy(pEfcCtx, &efcStratGlobCtx);

  EfcRunCtx runCtx = {};
  runCtx.onEfcFireReportCb = handleFireReport;

  // ==============================================
  // Subscribing on securities


  SecurityCtx security[] = {
      {{'\0','\0','0','0','T','E','S','T'}, 0, 1000, 2000, 1},
      {{'\0','\0','0','1','T','E','S','T'}, 0, 3000, 4000, 1},
      {{'\0','\0','0','2','T','E','S','T'}, 0, 5000, 6000, 1},
      {{'\0','\0','0','3','T','E','S','T'}, 0, 7000, 8000, 1},
      {{'\0','\0','0','2','A','B','C','D'}, 0, 7000, 8000, 1},
      {{'\0','\0','0','2','g','I','P','T'}, 0, 64090, 65010, 1},
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
      EKA_WARN("Security[%d] '%c%c%c%c%c%c%c%c' was not "
							 "fit into FPGA hash: handle = %jd",
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
			.bidMinPrice       = static_cast<decltype(secCtx.bidMinPrice)>
			(security[i].bidMinPrice / 100),  //x100, should be nonzero
			.askMaxPrice       = static_cast<decltype(secCtx.askMaxPrice)>
			(security[i].askMaxPrice / 100),  //x100
			.bidSize           = security[i].size,
			.askSize           = security[i].size,
			.versionKey        = (uint8_t)i,
			.lowerBytesOfSecId = (uint8_t)(securityList[i] & 0xFF)
    };
    EKA_TEST("Setting StaticSecCtx[%d] secId=0x%016jx, handle=%jd",
						 i,securityList[i],handle);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK)
			on_error ("failed to efcSetStaticSecCtx");
  }

  // ==============================================
  // Manually prepared FPGA firing template

  const BoeNewOrderMsg fireMsg = {
      .StartOfMessage    = 0xBABA,
      .MessageLength     = sizeof(BoeNewOrderMsg) - 2,
      .MessageType       = 0x38,
      .MatchingUnit      = 0,
      .SequenceNumber    = 0,

      // low 8 bytes of ClOrdID are replaced at FPGA by AppSeq number
      .ClOrdID           = {'s','e','q','-','>','s','t',
														'1','2','3','4','5','6','<',
														'-','-','A','B','C','D'},
	  
      .Side              = '_',  // '1'-Bid, '2'-Ask
      .OrderQty          = 0,

      .NumberOfBitfields = 4, 
      .NewOrderBitfield1 = 1 | 2 | 4 | 16 | 32, // ClearingFirm,ClearingAccount,Price,OrdType,TimeInForce
      .NewOrderBitfield2 = 1 | 64,              // Symbol,Capacity
      .NewOrderBitfield3 = 1,                   // Account
      .NewOrderBitfield4 = 0,
      /* .NewOrderBitfield5 = 0,  */
      /* .NewOrderBitfield6 = 0,  */
      /* .NewOrderBitfield7 = 0, */ 

      .ClearingFirm      = {'C','L','F','M'},
      .ClearingAccount   = {'C','L','A','C'},
      .Price             = 0,
      .OrdType           = '2',  // '1' = Market,'2' = Limit (default)
			                           // '3' = Stop,  '4' = Stop Limit
      .TimeInForce       = '3',  // '3' - IOC
			// last 2 Symbol's padding chars to be set to ' '
      .Symbol            = {' ',' ',' ',' ',' ',' ',' ',' '}, 
      .Capacity          = 'C',  // 'C','M','F',etc.
      .Account           = {'1','2','3','4','5',
														'6','7','8','9','0',
														'A','B','C','D','E','F'},
      .OpenClose         = 'O'
  };

  // ==============================================

  const EfcAction fire0 = {
    .type          = EpmActionType::BoeFire,        ///< Action type
    .token         = DefaultToken,                  ///< Security token
    .hConn         = conn[0],                       ///< TCP connection where segments will be sent
		//    .offset        = heapOffset + nwHdrOffset,      ///< Offset to payload in payload heap
		//    .length        = 67,     ///< Payload length
    .actionFlags   = AF_Valid,                      ///< Behavior flags (see EpmActionFlag)
    .nextAction    = EPM_LAST_ACTION,               ///< Next action in sequence, or EPM_LAST_ACTION
    .enable        = AlwaysFire,                    ///< Enable bits
    .postLocalMask = AlwaysFire,                    ///< Post fire: enable & mask -> enable
    .postStratMask = AlwaysFire,                    ///< Post fire: strat-enable & mask -> strat-enable
    .user          = 0x1234567890abcdef             ///< Opaque value copied into `EpmFireReport`.
  };

	rc = efcSetAction(dev,0,&fire0);

  if (rc != EKA_OPRESULT__OK)
		on_error("epmSetAction returned %d",(int)rc);

	rc = efcSetActionPayload(dev,0,&fireMsg,67);
	if (rc != EKA_OPRESULT__OK) 
    on_error("efcSetActionPayload failed");
	
  // ==============================================
  efcEnableController(pEfcCtx, -1);
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

  //  uint32_t sequence = 32;

  if (fatalDebug) {
      TEST_LOG(RED "\n=====================\nFATAL DEBUG: ON\n=====================\n" RESET);
      eka_write(dev,0xf0f00,0xefa0beda);
  }
// ==============================================


#if 0
  sendAddOrder(AddOrder::Short,triggerSock,&triggerMcAddr,security[2].id,
	       sequence++,'S',security[2].askMaxPrice / 100 - 1,security[2].size);
  
  sleep(1);
  efcEnableController(pEfcCtx, 1, armVer++); //arm
#endif
// ==============================================
#if 0
  sendAddOrder(AddOrder::Short,triggerSock,&triggerMcAddr,security[2].id,
	       sequence++,'B',security[2].bidMinPrice / 100 + 1,security[2].size);
  
  sleep(1);
  efcEnableController(pEfcCtx, 1, armVer++); //arm
#endif    
// ==============================================
#if 0
  sendAddOrder(AddOrder::Long,triggerSock,&triggerMcAddr,security[2].id,
	       sequence++,'S',security[2].askMaxPrice - 1,security[2].size);
  
  sleep(1);
  efcEnableController(pEfcCtx, 1, armVer++); //arm
#endif  
// ==============================================
#if 0
  sendAddOrder(AddOrder::Long,triggerSock,&triggerMcAddr,security[2].id,
	       sequence++,'B',security[2].bidMinPrice + 1,security[2].size);
  
  sleep(1);
  efcEnableController(pEfcCtx, 1, armVer++); //arm
#endif
// ==============================================
#if 0
  sendAddOrder(AddOrder::Expanded,triggerSock,&triggerMcAddr,security[2].id,
	       sequence++,'S',security[2].askMaxPrice - 1,security[2].size);
  
  sleep(1);
  efcEnableController(pEfcCtx, 1, armVer++); //arm
#endif  
// ==============================================
#if 0
  sendAddOrder(AddOrder::Expanded,triggerSock,&triggerMcAddr,security[2].id,
	       sequence++,'B',security[2].bidMinPrice + 1,security[2].size);
  
  sleep(1);
  efcEnableController(pEfcCtx, 1, armVer++); //arm
#endif  
// ==============================================
#if 1
  const char* pkt = "2c000201819613000a2022dd0000723f19621a22d0a29b0f035fb7086eca2f024221003032674950548a0201";

 /* sequenced_unit_header: */
 /*   length  = 2c00 */
 /*   count   = 02 */
 /*   unit    = 01 */
 /*   sequence = 81961300 */
 /* GenericHeader #0: */
 /*   length = 0a */
 /*   type   = 20  // Time */
 /*   time   = 22dd0000 */
 /* Payload #0: */
 /*  723f1962 */
    
 /* GenericHeader #1: */
 /*   length = 1a */
 /*   type   = 22 */
 /*   time   = d0a29b0f */
 /* Payload #1: */
 /*     03 5f b7 08 6e ca 2f 02 42 21 00 30 32 67 49 50 54 8a 02 01 */

	for (auto i = 0; i < TotalInjects; i++) {

		//		if (rand() % 3) {
		if (1) {
			efcEnableController(pEfcCtx, 1, armVer++); //arm and promote
			ExpectedFires++;
		}	else {
			efcEnableController(pEfcCtx, 1, armVer-1); //should be no arm
		}
      
		if (i == TotalInjects / 2)
			efcSetSessionCntr(dev,conn[0],0x12345678aabbccdd);

		sendPreloadedPkt(triggerSock,&triggerMcAddr,pkt,strlen(pkt));
		usleep (300000);
	}
  
#endif  
// ==============================================


    efcEnableController(pEfcCtx, -1);    
    int hw_fires  = getHWFireCnt(dev,0xf0340);  
    
    printf("\n===========================\nEND OT TESTS : ");

    bool testPass = true;

#ifndef _VERILOG_SIM
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


    efcEnableController(pEfcCtx, 1, armVer++); //arm and promote

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
