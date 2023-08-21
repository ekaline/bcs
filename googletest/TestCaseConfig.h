#ifndef __TEST_CASE_CONFIG_H__
#define __TEST_CASE_CONFIG_H__

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

#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <sys/time.h>

#include "EkaEfcDataStructs.h"
#include "eka_macros.h"

// #include "EfcMasterTestConfig.h"

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
extern volatile bool keep_work;
extern volatile bool serverSet;
// volatile bool rxClientReady = false;

// volatile bool triggerGeneratorDone = false;
// static const uint64_t FireEntryHeapSize = 256;

// bool runEfh = false;
// bool fatalDebug = false;
// bool report_only = false;
// bool dontQuit = false;
// const TestScenarioConfig *sc = nullptr;
// bool printFireReports = false;

/* --------------------------------------------- */
enum class TestStrategy : int {
  Invalid = 0,
  P4,
  CmeFc,
  Qed
};

static const char *printStrat(TestStrategy s) {
  switch (s) {
  case TestStrategy::Invalid:
    return "Invalid";
  case TestStrategy::P4:
    return "P4";
  case TestStrategy::CmeFc:
    return "CmeFC";
  case TestStrategy::Qed:
    return "Qed";
  default:
    on_error("Unexpected testStrategy %d", (int)s);
  }
}

static TestStrategy string2strat(const char *s) {
  if (!strcmp(s, "P4"))
    return TestStrategy::P4;

  if (!strcmp(s, "CmeFC"))
    return TestStrategy::CmeFc;

  if (!strcmp(s, "Qed"))
    return TestStrategy::Qed;

  return TestStrategy::Invalid;
}

struct TestTcpSess {
  int coreId;
  const char *srcIp;
  const char *dstIp;
  uint16_t dstPort;
};

struct TestTcpParams {
  const TestTcpSess *tcpSess;
  size_t nTcpSess;
};

static const char *udpSrcIp[] = {
    "10.0.0.10",
    "10.0.0.11",
    "10.0.0.12",
    "10.0.0.13",
};

static const TestTcpSess testDefaultTcpSess[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111},
    {2, "120.0.0.0", "10.0.0.12", 22222},
    {3, "130.0.0.0", "10.0.0.13", 22333}};

static const TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
static const TestTcpSess tcp1_s[] = {
    {1, "110.0.0.0", "10.0.0.11", 22111}};
static const TestTcpSess tcp01_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111}};

static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};

static const TestTcpParams tcp1 = {tcp1_s,
                                   std::size(tcp1_s)};

static const TestTcpParams tcp01 = {tcp01_s,
                                    std::size(tcp01_s)};

/* --------------------------------------------- */
static const EfcUdpMcGroupParams mc0[] = {
    {0, "224.0.0.0", 30300}};
static const EfcUdpMcGroupParams mc1[] = {
    {1, "224.0.0.1", 30301}};
static const EfcUdpMcGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const EfcUdpMcParams core0_1mc = {mc0,
                                         std::size(mc0)};
static const EfcUdpMcParams core1_1mc = {mc1,
                                         std::size(mc1)};
static const EfcUdpMcParams two_cores_1mc = {
    mc01, std::size(mc01)};

struct TestCaseConfig {
  TestStrategy strat;
  EfcUdpMcParams mcParams;
  TestTcpParams tcpParams;
  bool loop; // endless loop
};

struct TestScenarioConfig {
  const char *name;
  TestCaseConfig testConf[2];
};

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
  /* -------------------------------------------- */

  size_t printMcConnParams(char *dst) {
    if (!dst)
      return printf("%d, %s:%u, ", coreId_, mcIp_, mcPort_);
    else
      return sprintf(dst, "%d, %s:%u, ", coreId_, mcIp_,
                     mcPort_);
  }
  /* -------------------------------------------- */

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
#if 0
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
#endif

  /* --------------------------------------------- */
  void printConf() {
    printf("\t%ju MC groups:\n", nMcCons_);
    for (size_t coreId = 0; coreId < EFC_MAX_CORES;
         coreId++)
      for (size_t i = 0; i < nMcCons_[coreId]; i++) {
        auto gr = udpConn_[coreId][i];
        char strBuf[256] = {};
        gr->printMcConnParams(strBuf);
        printf("\t\t%s, ", strBuf);
      }
    printf("\n");
  }
  /* --------------------------------------------- */

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
  /* --------------------------------------------- */

  ~TestTcpSessCtx() { tcpServThr_.join(); }
  /* --------------------------------------------- */

  void createTcpServ() {
    bool serverSet = false;
    tcpServThr_ = std::thread(tcpServer, dstIp_, dstPort_,
                              &servSock_, &serverSet);
    while (keep_work && !serverSet)
      std::this_thread::yield();
  }
  /* --------------------------------------------- */

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
  /* --------------------------------------------- */

  void set1stSeqNum(uint64_t seq) {
    efcSetSessionCntr(g_ekaDev, hCon_, seq);
    return;
  }
  /* --------------------------------------------- */

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
  /* --------------------------------------------- */

  /* --------------------------------------------- */

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
      tcpSess_[i]->set1stSeqNum(i * 1000); // Place holder
    }
  }

  void printConf() {
    printf("\t%ju TCP sessions:\n", nTcpSess_);
    for (auto i = 0; i < nTcpSess_; i++) {
      auto gr = tcpSess_[i];
      printf("\t\t%d, %s --> %s:%u, ", gr->coreId_,
             gr->srcIp_, gr->dstIp_, gr->dstPort_);
    }
    printf("\n");
  }

  size_t nTcpSess_ = 0;
  TestTcpSessCtx *tcpSess_[32] = {};
};
/* --------------------------------------------- */

class TestCase {
public:
  TestCase(const TestCaseConfig &tc) {
    switch (tc.strat) {
    case TestStrategy::P4:
      // prepareTestConfig_ = configureP4Test;
      // runTest_ = runP4Test;
      stratCtx_ = new EfcP4CboeTestCtx;
      armController_ = efcArmP4;
      disArmController_ = efcDisArmP4;
      FireStatisticsAddr_ = 0xf0340;

      break;
    case TestStrategy::Qed:
      // prepareTestConfig_ = configureQedTest;
      // runTest_ = runQedTest;
      armController_ = efcArmQed;
      disArmController_ = efcDisArmQed;

      FireStatisticsAddr_ = 0xf0818;

      break;
    case TestStrategy::CmeFc:
      // prepareTestConfig_ = configureCmeFcTest;
      // runTest_ = runCmeFcTest;
      armController_ = efcArmCmeFc;
      disArmController_ = efcDisArmCmeFc;

      FireStatisticsAddr_ = 0xf0800;

      break;
    default:
      on_error("Unexpected Test Strategy %d",
               (int)tc.strat);
    }
    strat_ = tc.strat;

    udpCtx_ = new TestUdpCtx(&tc.mcParams);
    if (!udpCtx_)
      on_error("failed on new TestUdpCtx()");

    tcpCtx_ = new TestTcpCtx(&tc.tcpParams);
    if (!tcpCtx_)
      on_error("failed on new TestTcpCtx()");

    loop_ = tc.loop;
    print("Created");
  }

  ~TestCase() {

    TEST_LOG("\n"
             "===========================\n"
             "END OT %s TEST\n"
             "===========================\n",
             printStrat(strat_));
  }

  void print(const char *msg) {
    TEST_LOG("\n%s: \'%s\' ", msg, printStrat(strat_));

    printTcpCtx();
    printUdpCtx();
  }

  void printTcpCtx() { return tcpCtx_->printConf(); }
  void printUdpCtx() { return udpCtx_->printConf(); }

  TestStrategy strat_ = TestStrategy::Invalid;
  TestUdpCtx *udpCtx_ = nullptr;
  TestTcpCtx *tcpCtx_ = nullptr;
  EfcStratTestCtx *stratCtx_ = nullptr;
  uint64_t FireStatisticsAddr_ = 0;
  bool loop_ = false;

  // PrepareTestConfigCb prepareTestConfig_;
  // RunTestCb runTest_;
  ArmControllerCb armController_;
  DisArmControllerCb disArmController_;
};
#endif
