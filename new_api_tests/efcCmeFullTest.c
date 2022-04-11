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
#include "EfcCme.h"

#include "EfcTestsCallbacks.h"

using namespace Cme;

/* --------------------------------------------- */
std::string ts_ns2str(uint64_t ts);

/* --------------------------------------------- */
volatile bool keep_work = true;
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
  keep_work = false;
  TEST_LOG("Ctrl-C detected: keep_work = false, exitting..."); fflush(stdout);
  return;
}
/* --------------------------------------------- */

static int ekaUdpMcConnect(uint32_t mcIp, uint16_t mcPort,
			   uint32_t srcIp) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) on_error("failed to open UDP socket");

  TEST_LOG("Subscribing on Kernel UDP MC group %s:%u from %s",
	  EKA_IP2STR(mcIp),mcPort,
	  EKA_IP2STR(srcIp));

  int const_one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &const_one, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &const_one, sizeof(int)) < 0) 
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct sockaddr_in mcast = {};
  mcast.sin_family=AF_INET;
  mcast.sin_addr.s_addr = mcIp; // INADDR_ANY
  mcast.sin_port = be16toh(mcPort);
  if (bind(sock,(struct sockaddr*) &mcast, sizeof(struct sockaddr)) < 0) 
    on_error("Failed to bind to %d",be16toh(mcast.sin_port));

  struct ip_mreq mreq = {};
  mreq.imr_interface.s_addr = srcIp; //INADDR_ANY;
  mreq.imr_multiaddr.s_addr = mcIp;

  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) 
    on_error("Failed to join  %s",EKA_IP2STR(mreq.imr_multiaddr.s_addr));

  TEST_LOG("Kernel joined MC group %s:%u from %s",
	  EKA_IP2STR(mreq.imr_multiaddr.s_addr),be16toh(mcast.sin_port),
	  EKA_IP2STR(mcIp));
  return sock;
}
/* --------------------------------------------- */
void readMcLoop(uint32_t mcIp, uint16_t mcPort,uint32_t srcIp) {
  int mcSock = ekaUdpMcConnect(mcIp,mcPort,srcIp);
  if (mcSock < 0) on_error("Failed openning mcSock");
  while (keep_work) {
    uint8_t pkt[1536] = {};
    //      EKA_LOG("Waiting for UDP pkt...");
      
    int size = recvfrom(mcSock, pkt, sizeof(pkt), 0, NULL, NULL);
    if (size < 0) on_error("size = %d",size);
  }
  
}

/* --------------------------------------------- */
void tcpServer(EkaDev* dev, std::string ip, uint16_t port, bool* serverSet) {
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
  int tcpSock = socket(PF_INET, SOCK_STREAM, 0);
  if (tcpSock < 0) on_error("Socket");

  tcpSock = accept(sd, (struct sockaddr*)&addr,(socklen_t*) &addr_size);
  TEST_LOG("Connected from: %s:%d -- sock=%d\n",
	  inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),tcpSock);

  int status = fcntl(tcpSock, F_SETFL, fcntl(tcpSock, F_GETFL, 0) | O_NONBLOCK);
  if (status < 0)  on_error("fcntl error");
  do {
      char line[1536] = {};
      int bytes_read = recv(tcpSock, line, sizeof(line), 0);
      if (bytes_read < 0) {
	  if (errno != EAGAIN && errno != EWOULDBLOCK)
	      on_error("bytes_read = %d",bytes_read);
      }
      if (bytes_read > 0) {
	  /* TEST_LOG ("recived pkt: %s",line); */
	  /* fflush(stderr); */
	  send(tcpSock, line, bytes_read, 0);
      }
  } while (keep_work);
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
		   uint16_t* numTcpSess, bool* runEfh, bool* fatalDebug) {
	int opt; 
	while((opt = getopt(argc, argv, ":c:s:p:u:l:t:fdh")) != -1) {  
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
    	/* TEST_LOG("triggerSock is binded to %s:%u", */
    	/* 	EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),be16toh(triggerSourceAddr.sin_port)); */
    }
    struct sockaddr_in triggerMcAddr = {};
    triggerMcAddr.sin_family      = AF_INET;
    triggerMcAddr.sin_addr.s_addr = inet_addr(dstIp.c_str());
    triggerMcAddr.sin_port        = be16toh(dstPort);

#if 1
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
 
    /* TEST_LOG("sending MDIncrementalRefreshTradeSummary48 trigger to %s:%u", */
    /* 	    EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),be16toh(triggerMcAddr.sin_port)); */
    if (sendto(triggerSock,pkt,payloadLen,0,(const sockaddr*)&triggerMcAddr,sizeof(triggerMcAddr)) < 0) 
	on_error ("MC trigger send failed");
    close(triggerSock);
    return 0;
}

/* ############################################# */
static bool isEkalineLocal() {
    for (auto const& ekaLabMachine : {"xn01", "xn04"})
	if (! strcmp(std::getenv("HOSTNAME"),ekaLabMachine))
	    return true;
    return false;
}

/* ############################################# */
void sendCmeTradeMsgsLoop(int msgCnt, std::string srcIp,
		      std::string dstIp, uint16_t dstPort,
		      uint16_t cmeTradeMsgLen,
		      uint8_t cmeNoMDEntriesTicker) {
	for (int i  = 0; i < msgCnt && keep_work; i++) {
	    /* printf ("===============================\n"); */
	    /* printf ("========== %7d ============\n",i); */
	    /* printf ("===============================\n"); */
	  printf (".");
	  if (i % 40 == 0) printf ("\n");
	    sendCmeTradeMsg(srcIp,dstIp,dstPort,
			    cmeTradeMsgLen,cmeNoMDEntriesTicker);
	    sleep(0);
	}
}

/* ############################################# */

void excRecvLoop(EkaDev* dev, ExcConnHandle conn) {
  const char* fileName = "testExcRecv.log";
  std::FILE* file = fopen(fileName,"w");
  if (!file) on_error("Failed to create %s",fileName);
  
    while (keep_work) {
      char rxBuf[2000] = {};
      int bytes_read = excRecv(dev,conn,rxBuf,sizeof(rxBuf),0);
      if (bytes_read > 0)
	hexDump("Echoed back Fired Pkt",rxBuf,bytes_read,file);
    }
    fclose(file);
}

/* ############################################# */

void efcOnFireReportDummy(const void* p, size_t len, void* ctx) {}
    
/* ############################################# */
int main(int argc, char *argv[]) {
  
    signal(SIGINT, INThandler);

    const char* fireReportLogFileName = "fireReport.log";
    FILE* fireReportLog = fopen(fireReportLogFileName,"w");
    if (!fireReportLog) on_error("Failed to create %s",fireReportLogFileName);

    // ==============================================

    std::string serverIp        = "10.0.0.10";      // Ekaline lab default
    std::string clientIp        = "100.0.0.110";    // Ekaline lab default
    std::string triggerIp       = "224.0.31.2";     // Ekaline lab default (Cme Vanilla Options feed)
    uint16_t triggerUdpPort     = 14311;            // Cme Vanilla Options feed
    uint16_t serverTcpPort      = 22345;            // Ekaline lab default
    uint16_t numTcpSess         = 1;
    bool     runEfh             = false;
    bool     fatalDebug          = false;

#ifdef _RUN_EFH
    EkaProp efhProp[] = {
	{"efh.CME_SBE.group.0.products","vanilla_book"},	    
	{"efh.CME_SBE.group.0.mcast.addr","224.0.31.2:14311"},     // incrementFeed
	{"efh.CME_SBE.group.0.snapshot.addr","224.0.31.44:14311"}, // definitionFeed
	{"efh.CME_SBE.group.0.recovery.addr","224.0.31.23:14311"}, // snapshotFeed
    };
    EkaProps efhProps = {
	.numProps = std::size(efhProp),
	.props = efhProp
    };
    
    const EkaGroup cmeGroups[] = {
	{EkaSource::kCME_SBE, (EkaLSI)0},
    };
#endif    
    getAttr(argc,argv,&serverIp,&serverTcpPort,&clientIp,&triggerIp,&triggerUdpPort,
	    &numTcpSess,&runEfh,&fatalDebug);

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
    if (isEkalineLocal()) {
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
    }
    // ==============================================
    // Launching TCP test Servers
    if (isEkalineLocal()) {
	bool serverSet = false;
	std::thread server = std::thread(tcpServer,
					 dev,
					 serverIp,
					 serverTcpPort,
					 &serverSet);
	server.detach();
	while (keep_work && ! serverSet)
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
	if (conn[i] < 0)
	  on_error("excConnect %d %s:%u",
		   i,EKA_IP2STR(serverAddr.sin_addr.s_addr),
		   be16toh(serverAddr.sin_port));
	const char* pkt = "\n\nThis is 1st TCP packet sent from FPGA TCP client to Kernel TCP server\n\n";
	excSetBlocking(dev, excSock, false);
	
	excSend (dev, conn[i], pkt, strlen(pkt),0);
	int bytes_read = 0;
	char rxBuf[2000] = {};
//	bytes_read = recv(tcpSock[i], rxBuf, sizeof(rxBuf), 0);
	TEST_LOG("Sent: %s\n waiting for the echo...",pkt);
	bytes_read = excRecv(dev,conn[i], rxBuf, sizeof(rxBuf), 0);
	if (bytes_read > 0) TEST_LOG("\n%s",rxBuf);
    }

    // ==============================================
    // Setup EFC MC groups

    EpmTriggerParams triggerParam[] = {
	{0,triggerIp.c_str(),triggerUdpPort},
	/* {0,"224.0.74.0",30301}, */
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
    runCtx.onEfcFireReportCb = efcPrintFireReport; // default print out routine
    runCtx.cbCtx = fireReportLog;
    //    runCtx.onEfcFireReportCb = efcOnFireReportDummy; // Empty callback
    // ==============================================
    // CME FastCancel EFC config
    static const uint64_t CmeTestFastCancelAlwaysFire = 0xadcd;
    static const uint64_t CmeTestFastCancelToken = 0x1122334455667788;
    static const uint64_t CmeTestFastCancelUser  = 0xaabbccddeeff0011;
    static const uint16_t CmeTestFastCancelMaxMsgSize     = 1400; // Allowing Firing on all messages
    static const uint8_t  CmeTestFastCancelMinNoMDEntries = 0; // Allowing Firing on all messages

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
    // TEMP solution to test the DMA CH issue
    auto mcDoNothingThr = std::thread(readMcLoop,
				      inet_addr(triggerIp.c_str()),
				      triggerUdpPort,
				      inet_addr(clientIp.c_str()));
    
    // ==============================================		   
    efcRun(pEfcCtx, &runCtx );
    // ==============================================
    efcCmeSetILinkAppseq(dev,conn[0],0x1);

#ifdef _RUN_EFH   
    TEST_LOG("\n===========================\n"
	     "Configuring EFH to run on %s:%u"
	     "\n===========================\n",
	     triggerIp.c_str(),triggerUdpPort);

    
    EfhInitCtx efhInitCtx = {
      .ekaProps       = &efhProps,
      .numOfGroups    = 1,
      .coreId         = 0,
      .getTradeTime   = getTradeTimeCb,
      .recvSoftwareMd = true,
    };

    EfhCtx* pEfhCtx = NULL;
    efhInit(&pEfhCtx,dev,&efhInitCtx);

    EfhRunCtx efhRunCtx = {
      .groups                      = cmeGroups,
      .numGroups                   = std::size(cmeGroups),
      .efhRunUserData              = 0,
      .onEfhOptionDefinitionMsgCb  = onOptionDefinition,
      .onEfhFutureDefinitionMsgCb  = onFutureDefinition,
      .onEfhComplexDefinitionMsgCb = onComplexDefinition,
      .onEfhAuctionUpdateMsgCb     = onAuctionUpdate,
      .onEfhTradeMsgCb             = onTrade,
      .onEfhQuoteMsgCb             = onQuote,
      .onEfhOrderMsgCb             = onOrder,
      .onEfhGroupStateChangedMsgCb = onEfhGroupStateChange,
      .onEkaExceptionReportCb      = onException,
      .onEfhMdCb                   = onMd,
    };

    std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx,
					     &efhRunCtx,(void**)NULL);
    efh_run_thread.detach();
#endif      
    TEST_LOG("\n===========================\n"
	     "Waiting for Market data to Fire on"
	     "\n===========================\n");

    std::thread tradeMsgGeneratorThr;
    if (isEkalineLocal()) {
	static const int msgCnt = 10000000;
	static const uint16_t cmeTradeMsgLen     = 100;
	static const uint8_t  cmeNoMDEntriesTicker = 2;

	tradeMsgGeneratorThr = std::thread(sendCmeTradeMsgsLoop,
					   msgCnt,serverIp,
					   triggerIp,triggerUdpPort,
					   cmeTradeMsgLen,
					   cmeNoMDEntriesTicker);
    }    

    auto excRecvThr = std::thread(excRecvLoop,dev,conn[0]);
    
    while (keep_work) { sleep (0); }
    /* ============================================== */
    mcDoNothingThr.join();
    excRecvThr.join();
    if (isEkalineLocal()) {
      tradeMsgGeneratorThr.join();
    }
    /* ============================================== */

    printf("Closing device\n");

    ekaDevClose(dev);
    
    fclose(fireReportLog);
    return 0;
}
