#include <arpa/inet.h>
#include <iterator>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <fcntl.h>

#include <linux/sockios.h>

#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <sys/time.h>

#include "EkaBc.h"

#define TEST_LOG(...) { printf("%s@%s:%d: ",__func__,__FILE__,__LINE__); printf(__VA_ARGS__); printf("\n"); }
#define EKA_IP2STR(x)  ((std::to_string((x >> 0) & 0xFF) + '.' + std::to_string((x >> 8) & 0xFF) + '.' + std::to_string((x >> 16) & 0xFF) + '.' + std::to_string((x >> 24) & 0xFF)).c_str())

#ifndef on_error
#define on_error(...) do { const int err = errno; fprintf(stderr, "EKALINE API LIB FATAL ERROR: %s@%s:%d: ",__func__,__FILE__,__LINE__); fprintf(stderr, __VA_ARGS__); if (err) fprintf(stderr, ": %s (%d)", strerror(err), err); fprintf(stderr, "\n"); fflush(stdout); fflush(stderr); std::quick_exit(1); } while(0)
#endif

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

bool keep_work;

extern FILE *g_ekaLogFile;

/* --------------------------------------------- */
std::string ts_ns2str(uint64_t ts);

/* --------------------------------------------- */
// volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static const int MaxFireEvents = 10000;
static volatile EpmBCFireReport *FireEvent[MaxFireEvents] =
    {};
static volatile int numFireEvents = 0;

static const int MaxTcpTestSessions = 16;
static const int MaxUdpTestSessions = 64;
static const int MaxFastCancels = 10;
static const int TotalInjects = 10;
int ExpectedFires = 0;
int ReportedFires = 0;

/* --------------------------------------------- */
int getHWFireCnt(EkaDev *dev, uint64_t addr) { 
  uint64_t var_pass_counter = 0;//eka_read(dev, addr); TBD
  int real_val = (var_pass_counter >> 0) & 0xffffffff;
  return real_val;
}

/* --------------------------------------------- */
void handleFireReport(const void *p, size_t len,
                      void *ctx) {
  auto b = static_cast<const uint8_t *>(p);
  auto containerHdr{
      reinterpret_cast<const EkaBCContainerGlobalHdr *>(b)};
  switch (containerHdr->type) {
  case EkaBCExceptionEvent:
    break;
  default:
    ReportedFires++;
  }

  efcBCPrintFireReport(p, len, ctx);
  return;
}

/* --------------------------------------------- */

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG(
      "Ctrl-C detected: keep_work = false, exitting...");
  fflush(stdout);
  return;
}


/* --------------------------------------------- */
void tcpServer(EkaDev *dev, std::string ip, uint16_t port,
               int *sock, bool *serverSet) {
  pthread_setname_np(pthread_self(), "tcpServerParent");

  printf("Starting TCP server: %s:%u\n", ip.c_str(), port);
  int sd = 0;
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    on_error("Socket");
  int one_const = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one_const,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEADDR) failed");
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &one_const,
                 sizeof(int)) < 0)
    on_error("setsockopt(SO_REUSEPORT) failed");

  struct linger so_linger = {
      true, // Linger ON
      0     // Abort pending traffic on close()
  };

  if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &so_linger,
                 sizeof(struct linger)) < 0)
    on_error("Cant set SO_LINGER");

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = be16toh(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    on_error("failed to bind server sock to %s:%u",
             EKA_IP2STR(addr.sin_addr.s_addr),
             be16toh(addr.sin_port));
  if (listen(sd, 20) != 0)
    on_error("Listen");
  *serverSet = true;

  int addr_size = sizeof(addr);
  *sock = accept(sd, (struct sockaddr *)&addr,
                 (socklen_t *)&addr_size);
  TEST_LOG("Connected from: %s:%d -- sock=%d\n",
          inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),
          *sock);

  int status = fcntl(*sock, F_SETFL,
                     fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
  if (status == -1)
    on_error("fcntl error");

  return;
}

/* --------------------------------------------- */

void printUsage(char *cmd) {
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
         "\n",
         cmd);
  return;
}

/* --------------------------------------------- */

static int
getAttr(int argc, char *argv[], std::string *serverIp,
        uint16_t *serverTcpPort, std::string *clientIp,
        std::string *triggerIp, uint16_t *triggerUdpPort,
        uint16_t *numTcpSess, bool *runEfh,
        bool *fatalDebug, bool *dontExit) {
  int opt;
  while ((opt = getopt(argc, argv, ":c:s:p:u:l:t:fdhe")) !=
         -1) {
    switch (opt) {
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
      exit(1);
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
    free((void *)FireEvent[i]);
    FireEvent[i] = NULL;
  }
  numFireEvents = 0;
  return;
}


static int sendCmeTradeMsg(std::string serverIp,
                           std::string dstIp,
                           uint16_t dstPort) {
  // Preparing UDP MC for MD trigger on GR#0

  int triggerSock =
      socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (triggerSock < 0)
    on_error("failed to open UDP sock");

  struct sockaddr_in triggerSourceAddr = {};
  triggerSourceAddr.sin_addr.s_addr =
      inet_addr(serverIp.c_str());
  triggerSourceAddr.sin_family = AF_INET;
  triggerSourceAddr.sin_port = 0; // be16toh(serverTcpPort);

  if (bind(triggerSock, (sockaddr *)&triggerSourceAddr,
           sizeof(sockaddr)) != 0) {
    on_error("failed to bind server triggerSock to %s:%u",
             EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),
             be16toh(triggerSourceAddr.sin_port));
  } else {
    TEST_LOG("triggerSock is binded to %s:%u",
             EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),
             be16toh(triggerSourceAddr.sin_port));
  }
  struct sockaddr_in triggerMcAddr = {};
  triggerMcAddr.sin_family = AF_INET;
  triggerMcAddr.sin_addr.s_addr = inet_addr(dstIp.c_str());
  triggerMcAddr.sin_port = be16toh(dstPort);

  const uint8_t pkt[] = {
    0x0b, 0x00, 0x00, 0x00, //seq num
    0x65, 0x6f, 0x01, 0x38, 0xca, 0x42, 0xdc, 0x16, //sending time
    0x60, 0x00, //size
    0x0b, 0x00, //block len
    48,   0x00, //template id
    0x01, 0x00, 0x09, 0x00, //stam
    0x01, 0x6f, 0x01, 0x38, 0xca, 0x42, 0xdc, 0x16, //transact time (22,23,24,25,26,27,28,29)
    0x01, //match indicator (0-fire)
    0x00, 0x00, 0x20, 0x00, //stam
    0x06, //numingroup
    0x00, 0xfc, 0x2f, 0x9c, 0x9d, 0xb2, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0xcc, 0x33, 0x00, 0x00, 0x83, 0x88,
    0x26, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0xd9,
    0x7a, 0x6d, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x0e, 0x19, 0x84, 0x8e, 0x36,
    0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xb0, 0x7f, 0x8e, 0x36, 0x06, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  size_t payloadLen = std::size(pkt);

  TEST_LOG("sending MDIncrementalRefreshTradeSummary48 "
           "trigger to %s:%u",
           EKA_IP2STR(triggerMcAddr.sin_addr.s_addr),
           be16toh(triggerMcAddr.sin_port));
  if (sendto(triggerSock, pkt, payloadLen, 0,
             (const sockaddr *)&triggerMcAddr,
             sizeof(triggerMcAddr)) < 0)
    on_error("MC trigger send failed");
  return 0;
}

/* ############################################# */
int main(int argc, char *argv[]) {

  signal(SIGINT, INThandler);
  // ==============================================

  std::string serverIp = "10.0.0.10"; // Ekaline lab default
  std::string clientIp =
      "100.0.0.110"; // Ekaline lab default
  std::string triggerIp = "224.0.74.0"; // C1 C feed
  uint16_t triggerUdpPort = 30301;      // C1 CC gr#0
  uint16_t serverTcpBasePort = 22345; // Ekaline lab default
  uint16_t numTcpSess = 1;
  uint16_t serverTcpPort = serverTcpBasePort;
  bool runEfh = false;
  bool fatalDebug = false;
  bool dontExit = true;

  const EkaBcAffinityConfig ekaBcAffinityConfig = {
    .servThreadCpuId                =  0,
    .tcpRxThreadCpuId               =  1,
    .fireReportThreadCpuId          = -1,
    .igmpThreadCpuId                = -1,
    .tcpInternalCountersThreadCpuId = -1 
  };

  getAttr(argc, argv, &serverIp, &serverTcpPort, &clientIp,
          &triggerIp, &triggerUdpPort, &numTcpSess, &runEfh,
          &fatalDebug, &dontExit);

  if (numTcpSess > MaxTcpTestSessions)
    on_error("numTcpSess %d > MaxTcpTestSessions %d",
             numTcpSess, MaxTcpTestSessions);
  // ==============================================
  // EkaDev general setup
  EkaDev *dev = NULL;
  uint32_t armVer = 0;
  int8_t coreId = 0;
  int rc;

  //    ekaDevInit(&dev, &ekaDevInitCtx);
  dev = ekaBcOpenDev(&ekaBcAffinityConfig);

  // ==============================================
  // 10G Port (core) setup
  const EkaBcPortAttrs ekaCoreInitCtx = {
      .host_ip = inet_addr(clientIp.c_str()),
      .netmask = inet_addr("255.255.255.0"),
      .gateway = inet_addr("10.0.0.10"),
      .nexthop_mac = {}, // resolved by our internal ARP
      .src_mac_addr = {} // taken from system config
  };

  ekaBcConfigurePort(dev, coreId, &ekaCoreInitCtx);

  // ==============================================
  // Launching TCP test Servers
  int tcpSock[MaxTcpTestSessions] = {};

  for (auto i = 0; i < numTcpSess; i++) {
    bool serverSet = false;
    std::thread server = std::thread(
        tcpServer, dev, serverIp, serverTcpBasePort + i,
        &tcpSock[i], &serverSet);
    server.detach();
    while (keep_work && !serverSet)
      sleep(0);
  }
  // ==============================================
  // Establishing EXC connections for EPM/EFC fires

  int conn[MaxTcpTestSessions] = {};

  for (uint16_t i = 0; i < numTcpSess; i++) {
    conn[i] = ekaBcTcpConnect(dev,
			      coreId,
			      serverIp.c_str(),
                              serverTcpBasePort + i);
    if (conn[i] < 0)
      on_error("failed to open sock %d", i);
    const char *pkt =
      "\n\nThis is 1st TCP packet sent from FPGA TCP "
      "client to Kernel TCP server\n\n";
    ekaBcSend(dev, conn[i], pkt, strlen(pkt));
    int bytes_read = 0;
    char rxBuf[2000] = {};
    bytes_read = recv(tcpSock[i], rxBuf, sizeof(rxBuf), 0);
    if (bytes_read > 0)
      TEST_LOG("\n%s", rxBuf);
  }

  // ==============================================
  // Setup EFC MC groups

  EkaBcTriggerParams triggerParam[] = {
      {coreId, "224.0.74.0", 30301},
      /* {0,"224.0.74.1",30302}, */
      /* {0,"224.0.74.2",30303}, */
      /* {0,"224.0.74.3",30304}, */
  };

  rc = ekaBcFcInit(dev);

  if (rc != EKABC_OPRESULT__OK)
    on_error("efcInit returned %d", (int)rc);

  // ==============================================
  // Configuring EFC as EPM Strategy


  const EkaBcFcMdParams mdParams = {
      .triggerParams = triggerParam,
      .numTriggers = std::size(triggerParam),
  };
  rc = ekaBcCmeFcMdInit(dev, &mdParams);

  if (rc != EKABC_OPRESULT__OK)
    on_error("epmInitStrategies failed: rc = %d", rc);

  // ==============================================
  // Global EFC config
  EkaBcCmeFastCanceGlobalParams efcStratGlobCtx = {
      .report_only = 0,
      .watchdog_timeout_sec = 1,
  };
  ekaBcCmeFcGlobalInit(dev, &efcStratGlobCtx);

  EkaBcFcRunCtx runCtx = {};
  runCtx.onReportCb = handleFireReport;
  // ==============================================

  // CME FastCancel EFC config
  static const uint16_t CmeTestFastMinTimeDiff =  99; // <=99
  static const uint8_t CmeTestFastCancelMinNoMDEntries = 5; //<=5

  auto cmeHwCancelIdx = ekaBcAllocateFcAction(dev);

  EkaBcActionParams actionParams = {
      .tcpSock = conn[0],
			.nextAction = EPM_BC_LAST_ACTION};

  rc = ekaBcSetActionParams(dev, cmeHwCancelIdx,
                      &actionParams);
  if (rc != EKABC_OPRESULT__OK)
    on_error("efcSetAction returned %d", (int)rc);

  // ==============================================
  // Manually prepared CmeTestFastCancel message fired by
  // FPGA
  const char CmeTestFastCancelMsg[] =
      "CME Fast Cancel: Sequence = |____| With Dummy "
      "payload";

  rc = ekaBcSetActionPayload(dev, cmeHwCancelIdx,
                             &CmeTestFastCancelMsg,
                             strlen(CmeTestFastCancelMsg));
  if (rc != EKABC_OPRESULT__OK)
    on_error("efcSetActionPayload failed");

  const EkaBcCmeFcAlgoParams algoParams = {
      .fireActionId = cmeHwCancelIdx,
      .minTimeDiff = CmeTestFastMinTimeDiff, 
      .minNoMDEntries = CmeTestFastCancelMinNoMDEntries};

  // ==============================================
  ekaBcCmeFcAlgoInit(dev, &algoParams);
  // ==============================================
  ekaBcEnableController(dev, false);
  // ==============================================
  ekaBcFcRun(dev, &runCtx);
  // ==============================================

  ekaBcCmeSetILinkAppseq(dev, conn[0], 0x1);

  for (auto i = 0; i < TotalInjects; i++) {
    if (rand() % 3) {
      ekaBcEnableController(
          dev, true, armVer++); // arm and promote
      ExpectedFires++;
    } else {
      ekaBcEnableController(dev, true,
                            armVer - 1); // should be no arm
    }

    sendCmeTradeMsg(serverIp, triggerIp, triggerUdpPort                  
                    );
    usleep(300000);
  }

  if (fatalDebug) {
    TEST_LOG(RED "\n=====================\nFATAL DEBUG: "
                 "ON\n=====================\n" RESET);
    //    eka_write(dev, 0xf0f00, 0xefa0beda); //TBD
  }

  // ==============================================
  ekaBcEnableController(
			dev, true, armVer++); // arm and promote

  // test watchdog
  for (auto i = 0; i < 10; i++) {
    usleep(300000);
    ekaBcSwKeepAliveSend(dev);    
  }

  ekaBcEnableController(dev, false);
  
  int hw_fires = getHWFireCnt(dev, 0xf0800);

  printf("\n===========================\nEND OT TESTS : ");
  bool testPass = true;

#ifndef _VERILOG_SIM
  if (ReportedFires == ExpectedFires &&
      ReportedFires == hw_fires) {
    printf(GRN);
    printf("PASS, ExpectedFires == ReportedFires == "
           "HWFires == %d\n",
           ExpectedFires);
  } else {
    printf(RED);
    printf("FAIL, ExpectedFires = %d  ReportedFires = %d "
           "hw_fires = %d\n",
           ExpectedFires, ReportedFires, hw_fires);
    testPass = false;
  }
  printf(RESET);
  printf("===========================\n\n");
  keep_work = dontExit;
  sleep(1);
  TEST_LOG("--Test finished, ctrl-c to end---");
  while (keep_work) {
    sleep(0);
  }
#endif

  fflush(stdout);
  fflush(stderr);

  /* ============================================== */

  printf("Closing device\n");

  ekaBcCloseDev(dev);

  if (testPass)
    return 0;
  else
    return 1;
}
