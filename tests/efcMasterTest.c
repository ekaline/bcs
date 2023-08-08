#include <arpa/inet.h>
#include <getopt.h>
#include <iterator>
#include <linux/sockios.h>
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
#include <vector>

#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <sys/time.h>

#include "EkaDev.h"

#include "Efc.h"
#include "Efh.h"
#include "Eka.h"
#include "Epm.h"
#include "Exc.h"

#include "eka_macros.h"

#include "EkaCtxs.h"
#include "EkaEfcDataStructs.h"

#include "EfhTestFuncs.h"
#include "EkaFhBatsParser.h"
#include "ekaNW.h"
#include <fcntl.h>

#include "EfcMasterTestConfig.h"
#include "EfcP4CboeTestCtx.h"

#define MASK32 0xffffffff

class TestCase;

void configureP4Test(TestCase *t);
void configureQedTest(TestCase *t);
void configureCmeFcTest(TestCase *t);
bool runP4Test(TestCase *t);
bool runQedTest(TestCase *t);
bool runCmeFcTest(TestCase *t);

typedef void (*PrepareTestConfigCb)(TestCase *t);
typedef bool (*RunTestCb)(TestCase *t);

typedef EkaOpResult (*ArmControllerCb)(EkaDev *pEkaDev,
                                       EfcArmVer ver);
typedef EkaOpResult (*DisArmControllerCb)(EkaDev *pEkaDev);
extern EkaDev *g_ekaDev;
/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;
static const uint64_t FireEntryHeapSize = 256;

bool runEfh = false;
bool fatalDebug = false;
bool report_only = false;
bool dontQuit = false;
const TestScenarioConfig *sc = nullptr;
bool exitBeforeDevInit = false;
bool printFireReports = false;

struct FireReport {
  uint8_t *buf;
  size_t len;
};
std::vector<FireReport *> fireReports = {};
std::atomic<int> nReceivedFireReports = 0;
int nExpectedFireReports = 0;

/* --------------------------------------------- */

void getFireReport(const void *p, size_t len, void *ctx) {
  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(p)};
  if (containerHdr->type == EkaEventType::kExceptionEvent)
    return;

  auto fr = new FireReport;
  if (!fr)
    on_error("!fr");

  fr->buf = new uint8_t[len];
  if (!fr->buf)
    on_error("!fr->buf");
  fr->len = len;

  memcpy(fr->buf, p, len);
  fireReports.push_back(fr);
  nReceivedFireReports++;
  return;
}
/* --------------------------------------------- */
static void tcpServer(const char *ip, uint16_t port,
                      int *sock, bool *serverSet) {
  pthread_setname_np(pthread_self(), "tcpServerParent");

  TEST_LOG("Starting TCP server: %s:%u", ip, port);
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
  EKA_LOG("Connected from: %s:%d -- sock=%d",
          EKA_IP2STR(addr.sin_addr.s_addr),
          be16toh(addr.sin_port), *sock);

  int status = fcntl(*sock, F_SETFL,
                     fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
  if (status == -1)
    on_error("fcntl error");

  return;
}
/* --------------------------------------------- */

class TestUdpConn {
public:
  TestUdpConn(const EfcUdpMcGroupParams *mcParams) {
    coreId_ = mcParams->coreId;
    mcIp_ = mcParams->mcIp;
    mcPort_ = mcParams->mcUdpPort;

    udpSock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSock_ < 0)
      on_error("failed to open UDP sock");

    mcSrc_.sin_addr.s_addr = inet_addr(udpSrcIp[coreId_]);
    mcSrc_.sin_family = AF_INET;
    mcSrc_.sin_port = 0;

    mcDst_.sin_family = AF_INET;
    mcDst_.sin_addr.s_addr = inet_addr(mcIp_);
    mcDst_.sin_port = be16toh(mcPort_);

    if (bind(udpSock_, (sockaddr *)&mcSrc_,
             sizeof(sockaddr)) != 0) {
      on_error("failed to bind server udpSock to %s:%u",
               EKA_IP2STR(mcSrc_.sin_addr.s_addr),
               be16toh(mcSrc_.sin_port));
    }

    TEST_LOG("udpSock %d of MC %s:%u is binded to %s:%u",
             udpSock_, EKA_IP2STR(mcDst_.sin_addr.s_addr),
             be16toh(mcDst_.sin_port),
             EKA_IP2STR(mcSrc_.sin_addr.s_addr),
             be16toh(mcSrc_.sin_port));
  }

  size_t printMcConnParams(char *dst) {
    return sprintf(dst, "%d, %s:%u, ", coreId_, mcIp_,
                   mcPort_);
  }

  ssize_t sendUdpPkt(const void *pkt, size_t pktLen) {
#if 0
    char pktBufStr[8192] = {};
    hexDump2str("UdpPkt", pkt, pktLen, pktBufStr,
                sizeof(pktBufStr));
    TEST_LOG("Sending pkt\n%s\n to %s:%u", pktBufStr,
             EKA_IP2STR(mcDst_.sin_addr.s_addr),
             be16toh(mcDst_.sin_port));
#endif
    auto rc =
        sendto(udpSock_, pkt, pktLen, 0,
               (const sockaddr *)&mcDst_, sizeof(sockaddr));

    if (rc < 0)
      on_error("MC trigger send failed");

    return rc;
  }

private:
  EkaCoreId coreId_;

  const char *srcIp_ = nullptr;
  const char *mcIp_ = nullptr;
  uint16_t mcPort_ = 0;

  int udpSock_ = -1;

  struct sockaddr_in mcSrc_ = {};
  struct sockaddr_in mcDst_ = {};
};
/* --------------------------------------------- */

class TestUdpCtx {
public:
  TestUdpCtx(const EfcUdpMcParams *mcParams) {
    memcpy(&udpConf_, mcParams, sizeof(udpConf_));

    for (auto i = 0; i < mcParams->nMcGroups; i++) {
      auto coreId = mcParams->groups[i].coreId;
      auto perCoreidx = nMcCons_[coreId];
      udpConn_[coreId][perCoreidx] =
          new TestUdpConn(&mcParams->groups[i]);
      if (!udpConn_[coreId][perCoreidx])
        on_error("Failed on new TestUdpConn");

      nMcCons_[coreId]++;
    }
  }

  size_t printAllMcParams(char *dst) {
    size_t bufLen = 0;
    for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++)
      for (auto i = 0; i < nMcCons_[coreId]; i++) {
        if (!udpConn_[coreId][i])
          on_error("!udpConn_[%d][%d]", coreId, i);
        bufLen += sprintf(dst, "\t%2d: ", i);
        bufLen += udpConn_[coreId][i]->printMcConnParams(
            dst + bufLen);
      }
    bufLen += sprintf(dst, "\n");

    return bufLen;
  }
  /* --------------------------------------------- */

  static std::pair<uint32_t, uint32_t>
  getP4stratStatistics(uint64_t fireStatisticsAddr) {
    uint64_t var_p4_cont_counter1 =
        eka_read(fireStatisticsAddr);

    auto strategyRuns =
        (var_p4_cont_counter1 >> 0) & MASK32;
    auto strategyPassed =
        (var_p4_cont_counter1 >> 32) & MASK32;

    return std::pair<uint32_t, uint32_t>{strategyRuns,
                                         strategyPassed};
  }
  /* --------------------------------------------- */

  EfcArmVer sendPktToAllMcGrps(
      const void *pkt, size_t pktLen,
      ArmControllerCb armControllerCb, EfcArmVer armVer,
      uint64_t fireStatisticsAddr, int nFiresPerUdp) {
    if (!armControllerCb)
      on_error("!armControllerCb");

    auto nextArmVer = armVer;

    for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++)
      for (auto i = 0; i < nMcCons_[coreId]; i++) {
        if (!udpConn_[coreId][i])
          on_error("!udpConn_[%d][%d]", coreId, i);

        armControllerCb(g_ekaDev, nextArmVer++);
        auto [strategyRuns_prev, strategyPassed_prev] =
            getP4stratStatistics(fireStatisticsAddr);

        udpConn_[coreId][i]->sendUdpPkt(pkt, pktLen);
        nExpectedFireReports += nFiresPerUdp;
        bool fired = false;
        while (keep_work && !fired &&
               nExpectedFireReports !=
                   nReceivedFireReports.load()) {
          auto [strategyRuns_post, strategyPassed_post] =
              getP4stratStatistics(fireStatisticsAddr);

          fired = (strategyRuns_post != strategyRuns_prev);
          std::this_thread::yield();
        }
      }
    return nextArmVer;
  }

  EfcUdpMcParams udpConf_ = {};

  TestUdpConn
      *udpConn_[EFC_MAX_CORES]
               [EFC_PREALLOCATED_P4_ACTIONS_PER_LANE] = {};
  size_t nMcCons_[EFC_MAX_CORES] = {};
};
/* --------------------------------------------- */
class TestTcpSessCtx {
public:
  TestTcpSessCtx(const TestTcpSess *conf) {
    coreId_ = conf->coreId;
    srcIp_ = conf->srcIp;
    dstIp_ = conf->dstIp;
    dstPort_ = conf->dstPort;
  }

  ~TestTcpSessCtx() { tcpServThr_.join(); }

  void createTcpServ() {
    bool serverSet = false;
    tcpServThr_ = std::thread(tcpServer, dstIp_, dstPort_,
                              &servSock_, &serverSet);
    while (keep_work && !serverSet)
      std::this_thread::yield();
  }

  void connect() {
    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(dstIp_);

    serverAddr.sin_port = be16toh(dstPort_);

    excSock_ = excSocket(g_ekaDev, coreId_, 0, 0, 0);
    if (excSock_ < 0)
      on_error("failed on excSocket()");
    hCon_ = excConnect(g_ekaDev, excSock_,
                       (sockaddr *)&serverAddr,
                       sizeof(sockaddr_in));
    if (hCon_ < 0)
      on_error("excConnect to %s:%u",
               EKA_IP2STR(serverAddr.sin_addr.s_addr),
               be16toh(serverAddr.sin_port));

    TEST_LOG("TCP connected %s --> %s:%u", srcIp_,
             EKA_IP2STR(serverAddr.sin_addr.s_addr),
             be16toh(serverAddr.sin_port));
  }

  void set1stSeqNum(uint64_t seq) {
    efcSetSessionCntr(g_ekaDev, hCon_, seq);
    return;
  }

  void sendTestPkt() {
    char pkt[1500] = {};
    sprintf(pkt,
            "\n\nThis is 1st TCP packet sent from FPGA TCP "
            "client to Kernel TCP server: %s --> %s:%u\n\n",
            srcIp_, dstIp_, dstPort_);
    excSend(g_ekaDev, hCon_, pkt, strlen(pkt), 0);
    int bytes_read = 0;
    char rxBuf[2000] = {};
    bytes_read = recv(servSock_, rxBuf, sizeof(rxBuf), 0);
    if (bytes_read > 0)
      TEST_LOG("\n%s", rxBuf);
  }

public:
  ExcSocketHandle excSock_;
  ExcConnHandle hCon_;

  int coreId_;
  const char *srcIp_;
  const char *dstIp_;
  uint16_t dstPort_;

  int servSock_ = -1;

  std::thread tcpServThr_;
};
/* --------------------------------------------- */
class TestTcpCtx {
public:
  TestTcpCtx(const TestTcpParams *tcpParams) {
    nTcpSess_ = tcpParams->nTcpSess;
    for (auto i = 0; i < nTcpSess_; i++) {
      tcpSess_[i] =
          new TestTcpSessCtx(&tcpParams->tcpSess[i]);
      if (!tcpSess_[i])
        on_error("failed on new TestTcpSessCtx()");
    }
  }

  void connectAll() {
    for (auto i = 0; i < nTcpSess_; i++) {
      if (!tcpSess_[i])
        on_error("!tcpSess_[%d]", i);

      tcpSess_[i]->createTcpServ();
      tcpSess_[i]->connect();
      tcpSess_[i]->set1stSeqNum(i * 1000);
    }
  }

  size_t nTcpSess_ = 0;
  TestTcpSessCtx *tcpSess_[32] = {};
};
/* --------------------------------------------- */

class TestCase {
public:
  TestCase(const TestCaseConfig *tc) {
    switch (tc->strat) {
    case TestStrategy::P4:
      prepareTestConfig_ = configureP4Test;
      runTest_ = runP4Test;
      stratCtx_ = new EfcP4CboeTestCtx;
      armController_ = efcArmP4;
      disArmController_ = efcDisArmP4;
      FireStatisticsAddr_ = 0xf0340;

      break;
    case TestStrategy::Qed:
      prepareTestConfig_ = configureQedTest;
      runTest_ = runQedTest;
      armController_ = efcArmQed;
      disArmController_ = efcDisArmQed;

      FireStatisticsAddr_ = 0xf0818;

      break;
    case TestStrategy::CmeFc:
      prepareTestConfig_ = configureCmeFcTest;
      runTest_ = runCmeFcTest;
      armController_ = efcArmCmeFc;
      disArmController_ = efcDisArmCmeFc;

      FireStatisticsAddr_ = 0xf0800;

      break;
    default:
      on_error("Unexpected Test Strategy %d",
               (int)tc->strat);
    }
    strat_ = tc->strat;

    udpCtx_ = new TestUdpCtx(&tc->mcParams);
    if (!udpCtx_)
      on_error("failed on new TestUdpCtx()");

    tcpCtx_ = new TestTcpCtx(&tc->tcpParams);
    if (!tcpCtx_)
      on_error("failed on new TestTcpCtx()");

    loop_ = tc->loop;
    print("Created");
  }

  ~TestCase() {

    TEST_LOG("\n"
             "===========================\n"
             "END OT CBOE %s TEST\n"
             "===========================\n",
             printStrat(strat_));
  }

  void print(const char *msg) {
    TEST_LOG("%s: \'%s\' ", msg, printStrat(strat_));
    // printMcConf(&mcParams_);
    // printTcpConf(&tcpParams_);
  }

  TestStrategy strat_ = TestStrategy::Invalid;
  TestUdpCtx *udpCtx_ = nullptr;
  TestTcpCtx *tcpCtx_ = nullptr;
  EfcStratTestCtx *stratCtx_ = nullptr;
  uint64_t FireStatisticsAddr_ = 0;
  bool loop_ = false;

  PrepareTestConfigCb prepareTestConfig_;
  RunTestCb runTest_;
  ArmControllerCb armController_;
  DisArmControllerCb disArmController_;
};
/* --------------------------------------------- */

static std::vector<TestCase *> testCase = {};

static const TestCase *findTestCase(TestStrategy st) {
  for (const auto tc : testCase)
    if (tc->strat_ == st)
      return tc;

  return nullptr;
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

static void configureFpgaPorts(EkaDev *dev, TestCase *t) {
  for (auto i = 0; i < t->tcpCtx_->nTcpSess_; i++) {
    TEST_LOG("Initializing FPGA port %d to %s",
             t->tcpCtx_->tcpSess_[i]->coreId_,
             t->tcpCtx_->tcpSess_[i]->srcIp_);
    const EkaCoreInitCtx ekaCoreInitCtx = {
        .coreId =
            (EkaCoreId)t->tcpCtx_->tcpSess_[i]->coreId_,
        .attrs = {
            .host_ip =
                inet_addr(t->tcpCtx_->tcpSess_[i]->srcIp_),
            .netmask = inet_addr("255.255.255.0"),
            .gateway =
                inet_addr(t->tcpCtx_->tcpSess_[i]->dstIp_),
            .nexthop_mac =
                {}, // resolved by our internal ARP
            .src_mac_addr = {}, // taken from system config
            .dont_garp = 0}};
    ekaDevConfigurePort(dev, &ekaCoreInitCtx);
  }
}

/* --------------------------------------------- */

void printUsage(char *cmd) {
  TEST_LOG(
      "USAGE: %s \n"
      "\t--list <print list of available test scenarios\n"
      "\t--scenario [scenarion idx from the list]\n"

      "\t--report_only <Report Only ON>\n"
      "\t--dont_quit <Dont quit at the end>\n"

      "\t--exitBeforeDevInit to run init stage on "
      "non-FPGA server\n"
      "\t--print FireReports at the end of the test",
      cmd);
  return;
}

/* --------------------------------------------- */

static int getAttr(int argc, char *argv[]) {
  int c;
  int digit_optind = 0;

  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
        {"list", no_argument, 0, 'l'},
        {"scenario", required_argument, 0, 's'},

        {"ReportOnly", no_argument, 0, 'r'},
        {"report_only", no_argument, 0, 'r'},
        {"dont_quit", no_argument, 0, 'd'},
        {"exitBeforeDevInit", no_argument, 0, 'e'},
        {"print", no_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    c = getopt_long(argc, argv, "rdehlps:", long_options,
                    &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 0:
      TEST_LOG("option %s",
               long_options[option_index].name);
      if (optarg)
        TEST_LOG(" with arg %s", optarg);
      TEST_LOG("\n");
      break;

    case 's':
      TEST_LOG("Running scenario # %d\n", atoi(optarg));
      printSingleScenario(atoi(optarg));
      sc = &scenarios[atoi(optarg)];
      break;

    case 'l':
      TEST_LOG("%s available test scenarios:\n", argv[0]);
      printAllScenarios();
      exit(1);
      break;

    case 'h':
      printUsage(argv[0]);
      exit(1);
      break;

    case 'e':
      exitBeforeDevInit = true;
      TEST_LOG("exitBeforeDevInit = true");
      break;

    case 'p':
      printFireReports = true;
      TEST_LOG("printFireReports = true");
      break;

    case 'r':
      TEST_LOG("Report Only = ON\n");
      report_only = true;
      break;

    case 'd':
      TEST_LOG("dontQuit = TRUE\n");
      dontQuit = true;
      break;

    case '?':
      break;

    default:
      TEST_LOG("?? getopt returned character code 0%o ??\n",
               c);
    }
  }

  if (optind < argc) {
    auto badIdx = optind;
    TEST_LOG("non-option ARGV-elements: ");
    while (optind < argc)
      TEST_LOG("%s ", argv[optind++]);
    TEST_LOG("\n");
    on_error("Unexpected options %s", argv[badIdx]);
  }

  return 0;
}
/* --------------------------------------------- */
void createTestCases(const TestScenarioConfig *sc) {
  if (!sc)
    on_error("No Test scenarion selected");
  TEST_LOG("Configuring %s: \n", sc->name);
  for (const auto &tc : sc->testConf) {
    if (tc.strat != TestStrategy::Invalid) {
      TestCase *t = new TestCase(&tc);
      if (!t)
        on_error("Failed on new TestCase()");

      testCase.push_back(t);
    }
  }
}
/* --------------------------------------------- */

/* --------------------------------------------- */

enum class AddOrder : int {
  Unknown = 0,
  Short,
  Long,
  Expanded
};

/* --------------------------------------------- */

/* ############################################# */

static size_t createQEDMsg(char *dst, const char *id) {
  const uint8_t pkt[] = /*114 byte -> udp length 122,
                           remsize 122-52 = 70 (numlelel5)*/
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x34, 0x12, // dsid 1234
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00};

  size_t payloadLen = std::size(pkt);
  memcpy(dst, pkt, payloadLen);

  return 0;
}
/* ############################################# */

static size_t createCmeFcMsg(char *dst, const char *id) {
  const uint8_t pkt[] = {
      0x22, 0xa5, 0x0d, 0x02, 0xa5, 0x6f, 0x01, 0x38, 0xca,
      0x42, 0xdc, 0x16, 0x60, 0x00, 0x0b, 0x00, 0x30, 0x00,
      0x01, 0x00, 0x09, 0x00, 0x41, 0x23, 0xff, 0x37, 0xca,
      0x42, 0xdc, 0x16, 0x01, 0x00, 0x00, 0x20, 0x00, 0x01,
      0x00, 0xfc, 0x2f, 0x9c, 0x9d, 0xb2, 0x00, 0x00, 0x01,
      0x00, 0x00, 0x00, 0x5b, 0x33, 0x00, 0x00, 0x83, 0x88,
      0x26, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0xd9,
      0x7a, 0x6d, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02, 0x0e, 0x19, 0x84, 0x8e, 0x36,
      0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0xb0, 0x7f, 0x8e, 0x36, 0x06, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  size_t payloadLen = std::size(pkt);
  memcpy(dst, pkt, payloadLen);

  return 0;
}
/* ############################################# */

void configureP4Test(TestCase *t) {
  auto dev = g_ekaDev;

  if (!t)
    on_error("!t");
  TEST_LOG("\n"
           "=========== Configuring P4 Test ===========");
  auto udpMcParams = &t->udpCtx_->udpConf_;

  struct EfcP4Params p4Params = {
      .feedVer = EfhFeedVer::kCBOE,
      .fireOnAllAddOrders = true,
      .max_size = 10000,
  };

  int rc =
      efcInitP4Strategy(g_ekaDev, udpMcParams, &p4Params);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitP4Strategy returned %d", (int)rc);

  auto p4TestCtx =
      dynamic_cast<EfcP4CboeTestCtx *>(t->stratCtx_);
  if (!p4TestCtx)
    on_error("!p4TestCtx");

  // subscribing on list of securities
  efcEnableFiringOnSec(g_ekaDev, p4TestCtx->secList,
                       p4TestCtx->nSec);

  // ==============================================
  // setting security contexts
  for (size_t i = 0; i < p4TestCtx->nSec; i++) {
    auto handle =
        getSecCtxHandle(g_ekaDev, p4TestCtx->secList[i]);

    if (handle < 0) {
      EKA_WARN("Security[%ju] %s was not "
               "fit into FPGA hash: handle = %jd",
               i,
               cboeSecIdString(p4TestCtx->security[i].id, 8)
                   .c_str(),
               handle);
      continue;
    }

    SecCtx secCtx = {};
    p4TestCtx->getSecCtx(i, &secCtx);

    EKA_TEST(
        "Setting StaticSecCtx[%ju] \'%s\' secId=0x%016jx,"
        "handle=%jd,bidMinPrice=%u,askMaxPrice=%u,"
        "bidSize=%u,askSize=%u,"
        "versionKey=%u,lowerBytesOfSecId=0x%x",
        i,
        cboeSecIdString(p4TestCtx->security[i].id, 8)
            .c_str(),
        p4TestCtx->secList[i], handle, secCtx.bidMinPrice,
        secCtx.askMaxPrice, secCtx.bidSize, secCtx.askSize,
        secCtx.versionKey, secCtx.lowerBytesOfSecId);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(g_ekaDev, handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK)
      on_error("failed to efcSetStaticSecCtx");
  }

  // ==============================================
  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    for (auto mcGr = 0; mcGr < t->udpCtx_->nMcCons_[coreId];
         mcGr++) {
      // 1st in the chain is already preallocated
      int currActionIdx =
          coreId * EFC_PREALLOCATED_P4_ACTIONS_PER_LANE +
          mcGr;

      for (auto tcpConn = 0;
           tcpConn < t->tcpCtx_->nTcpSess_; tcpConn++) {
        rc = setActionTcpSock(
            g_ekaDev, currActionIdx,
            t->tcpCtx_->tcpSess_[tcpConn]->excSock_);
        if (rc != EKA_OPRESULT__OK)
          on_error("setActionTcpSock failed for Action %d",
                   currActionIdx);

        int nextActionIdx =
            tcpConn == t->tcpCtx_->nTcpSess_ - 1
                ? EPM_LAST_ACTION
                : efcAllocateNewAction(
                      g_ekaDev, EpmActionType::BoeFire);
        rc = setActionNext(dev, currActionIdx,
                           nextActionIdx);
        if (rc != EKA_OPRESULT__OK)
          on_error("setActionNext failed for Action %d",
                   currActionIdx);

        char fireMsg[1500] = {};
        sprintf(fireMsg,
                "BoeFireA: %03d         "
                "    MC:%d:%d "
                "Tcp:%d ",
                currActionIdx, coreId, mcGr, tcpConn);
        rc = efcSetActionPayload(
            dev, currActionIdx, fireMsg,
            sizeof(BoeQuoteUpdateShortMsg));
        if (rc != EKA_OPRESULT__OK)
          on_error(
              "efcSetActionPayload failed for Action %d",
              currActionIdx);

        currActionIdx = nextActionIdx;
      }
    }
  }
}
/* ############################################# */

void configureQedTest(TestCase *t) {
  auto dev = g_ekaDev;

  if (!t)
    on_error("!t");
  TEST_LOG("\n"
           "=========== Configuring Qed Test ===========");
  auto udpMcParams = &t->udpCtx_->udpConf_;

  static const uint16_t QEDTestPurgeDSID = 0x1234;
  static const uint8_t QEDTestMinNumLevel = 5;

  EfcQedParams qedParams = {};
  int active_set = 3;
  qedParams.product[active_set].ds_id = QEDTestPurgeDSID;
  qedParams.product[active_set].min_num_level =
      QEDTestMinNumLevel;
  qedParams.product[active_set].enable = true;

  int rc =
      efcInitQedStrategy(g_ekaDev, udpMcParams, &qedParams);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitQedStrategy returned %d", (int)rc);

  auto qedHwPurgeIAction =
      efcAllocateNewAction(dev, EpmActionType::QEDHwPurge);

  efcQedSetFireAction(g_ekaDev, qedHwPurgeIAction,
                      active_set);

  const char QEDTestPurgeMsg[] =
      "QED Purge Data With Dummy payload";

  rc = setActionTcpSock(dev, qedHwPurgeIAction,
                        t->tcpCtx_->tcpSess_[0]->excSock_);

  if (rc != EKA_OPRESULT__OK)
    on_error("setActionTcpSock failed for Action %d",
             qedHwPurgeIAction);

  rc = efcSetActionPayload(dev, qedHwPurgeIAction,
                           QEDTestPurgeMsg, 67);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcSetActionPayload failed for Action %d",
             qedHwPurgeIAction);
}

/* ############################################# */

void configureCmeFcTest(TestCase *t) {
  auto dev = g_ekaDev;

  if (!t)
    on_error("!t");
  TEST_LOG(
      "\n"
      "=========== Configuring CmeFc Test ===========");
  auto udpMcParams = &t->udpCtx_->udpConf_;

  static const uint16_t QEDTestPurgeDSID = 0x1234;
  static const uint8_t QEDTestMinNumLevel = 5;

  EfcCmeFcParams cmeParams = {.maxMsgSize = 97,
                              .minNoMDEntries = 0};

  int rc = efcInitCmeFcStrategy(g_ekaDev, udpMcParams,
                                &cmeParams);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitCmeFcStrategy returned %d", (int)rc);

  auto cmeFcHwAction =
      efcAllocateNewAction(dev, EpmActionType::CmeHwCancel);

  efcCmeFcSetFireAction(g_ekaDev, cmeFcHwAction);

  rc = setActionTcpSock(dev, cmeFcHwAction,
                        t->tcpCtx_->tcpSess_[0]->excSock_);

  if (rc != EKA_OPRESULT__OK)
    on_error("setActionTcpSock failed for Action %d",
             cmeFcHwAction);

  const char CmeTestFastCancelMsg[] =
      "CME Fast Cancel: Sequence = |____| With Dummy "
      "payload";

  rc = efcSetActionPayload(dev, cmeFcHwAction,
                           &CmeTestFastCancelMsg,
                           strlen(CmeTestFastCancelMsg));
  if (rc != EKA_OPRESULT__OK)
    on_error("efcSetActionPayload failed for Action %d",
             cmeFcHwAction);
}
/* ############################################# */

bool runP4Test(TestCase *t) {

  TEST_LOG("\n"
           "=========== Running P4 Test ===========");

  auto p4TestCtx =
      dynamic_cast<EfcP4CboeTestCtx *>(t->stratCtx_);
  if (!p4TestCtx)
    on_error("!p4TestCtx");

  EfcArmVer p4ArmVer = 0;

  // ==============================================

  uint32_t sequence = 32;
  const int LoopIterations = t->loop_ ? 10000 : 1;
  for (auto i = 0; i < LoopIterations; i++) {
#if 1
    {
      efcArmP4(g_ekaDev, p4ArmVer);
      char pktBuf[1500] = {};
      auto secCtx = &p4TestCtx->security[2];

      const char *mdSecId = secCtx->id;
      auto mdSide = SideT::ASK;
      auto mdPrice = secCtx->askMaxPrice - 1;
      auto mdSize = secCtx->size;

      auto pktLen = p4TestCtx->createOrderExpanded(
          pktBuf, mdSecId, mdSide, mdPrice, mdSize, true);

      p4ArmVer = t->udpCtx_->sendPktToAllMcGrps(
          pktBuf, pktLen, t->armController_, p4ArmVer,
          t->FireStatisticsAddr_, t->tcpCtx_->nTcpSess_);
    }
#endif

#if 1
    {
      efcArmP4(g_ekaDev, p4ArmVer);
      char pktBuf[1500] = {};
      auto secCtx = &p4TestCtx->security[3];

      const char *mdSecId = secCtx->id;
      auto mdSide = SideT::BID;
      auto mdPrice = secCtx->bidMinPrice + 1;
      auto mdSize = secCtx->size;

      auto pktLen = p4TestCtx->createOrderExpanded(
          pktBuf, mdSecId, mdSide, mdPrice, mdSize, true);

      p4ArmVer = t->udpCtx_->sendPktToAllMcGrps(
          pktBuf, pktLen, t->armController_, p4ArmVer,
          t->FireStatisticsAddr_, t->tcpCtx_->nTcpSess_);
    }
#endif
    for (auto i = 0; i < t->tcpCtx_->nTcpSess_; i++) {
      char rxBuf[8192] = {};
      int rc = recv(t->tcpCtx_->tcpSess_[i]->servSock_,
                    rxBuf, sizeof(rxBuf), 0);
    }
    //   usleep(1000);
  }
  // ==============================================
  t->disArmController_(g_ekaDev);
  TEST_LOG("\n"
           "===========================\n"
           "END OT CBOE P4 TEST\n"
           "===========================\n");
  return true;
}
/* ############################################# */

bool runQedTest(TestCase *t) {
  int qedExpectedFires = 0;
  int TotalInjects = 4;
  EfcArmVer qedArmVer = 0;

  for (auto i = 0; i < TotalInjects; i++) {
    // efcArmQed(g_ekaDev, qedArmVer++); // arm and promote
    qedExpectedFires++;

    const uint8_t
        pktBuf[] = /*114 byte -> udp length 122,
                     remsize 122-52 = 70 (numlelel5)*/
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x34, 0x12, // dsid 1234
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00};

    auto pktLen = sizeof(pktBuf);

    qedArmVer = t->udpCtx_->sendPktToAllMcGrps(
        pktBuf, pktLen, t->armController_, qedArmVer,
        t->FireStatisticsAddr_, t->tcpCtx_->nTcpSess_);

    // usleep(300000);
  }
  // ==============================================
  TEST_LOG("\n"
           "===========================\n"
           "END OT QED TEST\n"
           "===========================\n");
  return true;
}

/* ############################################# */

bool runCmeFcTest(TestCase *t) {
  int cmeFcExpectedFires = 0;
  int TotalInjects = 4;
  EfcArmVer cmeFcArmVer = 0;

  for (auto i = 0; i < TotalInjects; i++) {
    // efcArmCmeFc(g_ekaDev, cmeFcArmVer++); // arm and
    // promote
    cmeFcExpectedFires++;

    const uint8_t pktBuf[] = {
        0x22, 0xa5, 0x0d, 0x02, 0xa5, 0x6f, 0x01, 0x38,
        0xca, 0x42, 0xdc, 0x16, 0x60, 0x00, 0x0b, 0x00,
        0x30, 0x00, 0x01, 0x00, 0x09, 0x00, 0x41, 0x23,
        0xff, 0x37, 0xca, 0x42, 0xdc, 0x16, 0x01, 0x00,
        0x00, 0x20, 0x00, 0x01, 0x00, 0xfc, 0x2f, 0x9c,
        0x9d, 0xb2, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x5b, 0x33, 0x00, 0x00, 0x83, 0x88, 0x26, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0xd9, 0x7a,
        0x6d, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x0e, 0x19, 0x84, 0x8e,
        0x36, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x7f, 0x8e,
        0x36, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};

    auto pktLen = sizeof(pktBuf);

    cmeFcArmVer = t->udpCtx_->sendPktToAllMcGrps(
        pktBuf, pktLen, t->armController_, cmeFcArmVer,
        t->FireStatisticsAddr_, t->tcpCtx_->nTcpSess_);

    // usleep(300000);
  }
  // ==============================================
  TEST_LOG("\n"
           "===========================\n"
           "END OT CmeFc TEST\n"
           "===========================\n");
  return true;
}
/* ############################################# */

int main(int argc, char *argv[]) {

  signal(SIGINT, INThandler);
  // ==============================================

  getAttr(argc, argv);
  createTestCases(sc);

  if (exitBeforeDevInit)
    return 1;
  // ==============================================
  // EkaDev general setup
  EkaDev *dev = NULL;
  EkaOpResult rc;
  const EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&dev, &ekaDevInitCtx);

  // ==============================================
  // 10G Port (core) setup

  for (auto t : testCase) {
    configureFpgaPorts(dev, t);
    t->tcpCtx_->connectAll();
  }

  // ==============================================

  EfcInitCtx initCtx = {
      .report_only = report_only,
      .watchdog_timeout_sec = 1000000,
  };

  rc = efcInit(dev, &initCtx);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInit returned %d", (int)rc);

  EfcRunCtx runCtx = {};
  // runCtx.onEfcFireReportCb = efcPrintFireReport;
  runCtx.onEfcFireReportCb = getFireReport;
  runCtx.cbCtx = stdout;
  // ==============================================
  for (auto t : testCase)
    t->prepareTestConfig_(t);

  // ==============================================
  efcRun(g_ekaDev, &runCtx);
  // ==============================================

  for (auto t : testCase)
    t->runTest_(t);

#ifndef _VERILOG_SIM
  sleep(2); // needed to get all fireReports printed out
  if (dontQuit)
    EKA_LOG("--Test finished, ctrl-c to end---");
  else {
    EKA_LOG("--Test finished---");
    keep_work = false;
  }
  while (keep_work) {
    std::this_thread::yield();
  }
#endif

  for (auto t : testCase)
    delete t;

  /* ============================================== */

  TEST_LOG("Received %ju fire reports",
           std::size(fireReports));
  if (printFireReports)
    for (const auto fr : fireReports)
      efcPrintFireReport(fr->buf, fr->len, stdout);

  printf("Closing device\n");
  ekaDevClose(dev);
  return 0;
}
