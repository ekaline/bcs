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

class TestCase;

void configureP4Test(EfcCtx *pEfcCtx, TestCase *t);
void configureQedTest(EfcCtx *pEfcCtx, TestCase *t);
bool runP4Test(EfcCtx *pEfcCtx, TestCase *t);
bool runQedTest(EfcCtx *pEfcCtx, TestCase *t);

typedef void (*PrepareTestConfigCb)(EfcCtx *efcCtx,
                                    TestCase *t);
typedef bool (*RunTestCb)(EfcCtx *efcCtx, TestCase *t);

extern EkaDev *g_ekaDev;
/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

volatile bool triggerGeneratorDone = false;

static const int MaxFireEvents = 10000;
static volatile EpmFireReport *FireEvent[MaxFireEvents] =
    {};
static volatile int numFireEvents = 0;

static const uint64_t FireEntryHeapSize = 256;

bool runEfh = false;
bool fatalDebug = false;
bool report_only = false;
bool dontQuit = false;
const TestScenarioConfig *sc = nullptr;
bool exitBeforeDevInit = false;

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
    char pktBufStr[8192] = {};
    hexDump2str("UdpPkt", pkt, pktLen, pktBufStr,
                sizeof(pktBufStr));
    TEST_LOG("Sending pkt\n%s\n to %s:%u", pktBufStr,
             EKA_IP2STR(mcDst_.sin_addr.s_addr),
             be16toh(mcDst_.sin_port));
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

  void sendPktToAllMcGrps(const void *pkt, size_t pktLen) {
    for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++)
      for (auto i = 0; i < nMcCons_[coreId]; i++) {
        if (!udpConn_[coreId][i])
          on_error("!udpConn_[%d][%d]", coreId, i);

        udpConn_[coreId][i]->sendUdpPkt(pkt, pktLen);
      }
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
      break;
    case TestStrategy::Qed:
      prepareTestConfig_ = configureQedTest;
      runTest_ = runQedTest;
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

    print("Created");
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

  PrepareTestConfigCb prepareTestConfig_;
  RunTestCb runTest_;
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
#if 0

static void bindUdpSock(TestCase *t) {
  t->udpParams_[0]->udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (t->udpParams_[0]->udpSock < 0)
    on_error("failed to open UDP sock");

  struct sockaddr_in triggerSourceAddr = {};
  triggerSourceAddr.sin_addr.s_addr =
      inet_addr(t->tcpSess_.dstIp.c_str());
  triggerSourceAddr.sin_family = AF_INET;
  triggerSourceAddr.sin_port = 0;

  if (bind(t->udpParams_[0]->udpSock, (sockaddr *)&triggerSourceAddr,
           sizeof(sockaddr)) != 0) {
    on_error("failed to bind server udpSock to %s:%u",
             EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),
             be16toh(triggerSourceAddr.sin_port));
  } else {
    TEST_LOG("udpSock is binded to %s:%u",
             EKA_IP2STR(triggerSourceAddr.sin_addr.s_addr),
             be16toh(triggerSourceAddr.sin_port));
  }

  t->triggerMcAddr_.sin_family = AF_INET;
  t->triggerMcAddr_.sin_addr.s_addr =
      inet_addr(t->mcParams_.mcIp.c_str());
  t->triggerMcAddr_.sin_port = be16toh(t->mcParams_.mcPort);
}
#endif

/* --------------------------------------------- */

void printUsage(char *cmd) {
  TEST_LOG(
      "USAGE: %s \n"
      "\t--list <print list of available test scenarios\n"
      "\t--scenario [scenarion idx from the list]\n"

      "\t--report_only <Report Only ON>\n"
      "\t--dont_quit <Dont quit at the end>\n"

      "\t--exitBeforeDevInit to run init stage on non-FPGA "
      "server"
      "\n",
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
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    c = getopt_long(argc, argv, "rdehls:", long_options,
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

static void cleanFireEvents() {
  for (auto i = 0; i < numFireEvents; i++) {
    free((void *)FireEvent[i]);
    FireEvent[i] = NULL;
  }
  numFireEvents = 0;
  return;
}

/* --------------------------------------------- */

enum class AddOrder : int {
  Unknown = 0,
  Short,
  Long,
  Expanded
};

/* --------------------------------------------- */

static int sendAddOrderShort(int sock,
                             const sockaddr_in *addr,
                             char *secId, uint32_t sequence,
                             char side, uint16_t price,
                             uint16_t size) {

  CboePitchAddOrderShort pkt = {
      .hdr =
          {
              .length = sizeof(pkt),
              .count = 1,
              .unit = 1, // just a number
              .sequence = sequence,
          },
      .msg = {
          .header =
              {
                  .length = sizeof(pkt.msg),
                  .type = (uint8_t)MsgId::ADD_ORDER_SHORT,
                  .time = 0x11223344, // just a number
              },
          .order_id = 0xaabbccddeeff5566,
          .side = side,
          .size = size,
          .symbol = {secId[2], secId[3], secId[4], secId[5],
                     secId[6], secId[7]},
          .price =
              price, //(uint16_t)(p4Ctx.at(2).bidMinPrice
                     /// 100 + 1),
          .flags = 0xFF,
      }};
  TEST_LOG("sending AddOrderShort trigger to %s:%u, "
           "price=%u, size=%u",
           EKA_IP2STR(addr->sin_addr.s_addr),
           be16toh(addr->sin_port), price, size);
  if (sendto(sock, &pkt, sizeof(pkt), 0,
             (const sockaddr *)addr, sizeof(sockaddr)) < 0)
    on_error("MC trigger send failed");

  return 0;
}
/* --------------------------------------------- */

static int sendAddOrderLong(int sock,
                            const sockaddr_in *addr,
                            char *secId, uint32_t sequence,
                            char side, uint64_t price,
                            uint32_t size) {

  CboePitchAddOrderLong pkt = {
      .hdr =
          {
              .length = sizeof(pkt),
              .count = 1,
              .unit = 1, // just a number
              .sequence = sequence,
          },
      .msg = {
          .header =
              {
                  .length = sizeof(pkt.msg),
                  .type = (uint8_t)MsgId::ADD_ORDER_LONG,
                  .time = 0x11223344, // just a number
              },
          .order_id = 0xaabbccddeeff5566,
          .side = side,
          .size = size,
          .symbol = {secId[2], secId[3], secId[4], secId[5],
                     secId[6], secId[7]},
          .price =
              price, //(uint16_t)(p4Ctx.at(2).bidMinPrice
                     /// 100 + 1),
          .flags = 0xFF,
      }};
  TEST_LOG("sending AddOrderLong trigger to %s:%u",
           EKA_IP2STR(addr->sin_addr.s_addr),
           be16toh(addr->sin_port));
  if (sendto(sock, &pkt, sizeof(pkt), 0,
             (const sockaddr *)addr, sizeof(sockaddr)) < 0)
    on_error("MC trigger send failed");

  return 0;
}

/* --------------------------------------------- */

static int sendAddOrderExpanded(int sock,
                                const sockaddr_in *addr,
                                char *secId,
                                uint32_t sequence,
                                char side, uint16_t price,
                                uint16_t size) {

  CboePitchAddOrderExpanded pkt = {
      .hdr =
          {
              .length = sizeof(pkt),
              .count = 1,
              .unit = 1, // just a number
              .sequence = sequence,
          },
      .msg = {
          .header =
              {
                  .length = sizeof(pkt.msg),
                  .type =
                      (uint8_t)MsgId::ADD_ORDER_EXPANDED,
                  .time = 0x11223344, // just a number
              },
          .order_id = 0xaabbccddeeff5566,
          .side = side,
          .size = size,
          //	    .exp_symbol =
          //{secId[0],secId[1],secId[2],secId[3],secId[4],secId[5],secId[6],secId[7]},
          .exp_symbol = {secId[2], secId[3], secId[4],
                         secId[5], secId[6], secId[7], ' ',
                         ' '},
          .price =
              price, //(uint16_t)(p4Ctx.at(2).bidMinPrice
                     /// 100 + 1),
          .flags = 0xFF,
          .participant_id = {},
          .customer_indicator = 'C',
          .client_id = {}}};

  TEST_LOG("sending AddOrderExpanded trigger to %s:%u",
           EKA_IP2STR(addr->sin_addr.s_addr),
           be16toh(addr->sin_port));
  if (sendto(sock, &pkt, sizeof(pkt), 0,
             (const sockaddr *)addr, sizeof(sockaddr)) < 0)
    on_error("MC trigger send failed");

  return 0;
}
/* --------------------------------------------- */
static int sendAddOrder(AddOrder type, int sock,
                        const sockaddr_in *addr,
                        char *secId, uint32_t sequence,
                        char side, uint64_t price,
                        uint32_t size) {
  switch (type) {
  case AddOrder::Short:
    return sendAddOrderShort(sock, addr, secId, sequence,
                             side, price, size);
  case AddOrder::Long:
    return sendAddOrderLong(sock, addr, secId, sequence,
                            side, price, size);
  case AddOrder::Expanded:
    return sendAddOrderExpanded(sock, addr, secId, sequence,
                                side, price, size);
  default:
    on_error("Unexpected type %d", (int)type);
  }
  return 0;
}

static size_t prepare_BoeNewOrderMsg(void *dst) {
  const BoeNewOrderMsg fireMsg = {
      .StartOfMessage = 0xBABA,
      .MessageLength = sizeof(BoeNewOrderMsg) - 2,
      .MessageType = 0x38,
      .MatchingUnit = 0,
      .SequenceNumber = 0,

      // low 8 bytes of ClOrdID are replaced at FPGA by
      // AppSeq number
      .ClOrdID = {'E', 'K', 'A', 't', 'e', 's', 't',
                  '1', '2', '3', '4', '5', '6', '7',
                  '8', '9', 'A', 'B', 'C', 'D'},

      .Side = '_', // '1'-Bid, '2'-Ask
      .OrderQty = 0,

      .NumberOfBitfields = 4,
      .NewOrderBitfield1 =
          1 | 2 | 4 | 16 |
          32, // ClearingFirm,ClearingAccount,Price,OrdType,TimeInForce
      .NewOrderBitfield2 = 1 | 64, // Symbol,Capacity
      .NewOrderBitfield3 = 1,      // Account
      .NewOrderBitfield4 = 0,
      /* .NewOrderBitfield5 = 0,  */
      /* .NewOrderBitfield6 = 0,  */
      /* .NewOrderBitfield7 = 0, */

      .ClearingFirm = {'C', 'L', 'F', 'M'},
      .ClearingAccount = {'C', 'L', 'A', 'C'},
      .Price = 0,
      .OrdType =
          '2', // '1' = Market,'2' = Limit
               // (default),'3' = Stop,'4' = Stop Limit
      .TimeInForce = '3', // '3' - IOC
      .Symbol =
          {' ', ' ', ' ', ' ', ' ', ' ', ' ',
           ' '}, // last 2 padding chars to be set to ' '
      .Capacity = 'C', // 'C','M','F',etc.
      .Account = {'1', '2', '3', '4', '5', '6', '7', '8',
                  '9', '0', 'A', 'B', 'C', 'D', 'E', 'F'},
      .OpenClose = 'O'};

  memcpy(dst, &fireMsg, sizeof(fireMsg));
  return sizeof(fireMsg);
}

static size_t prepare_BoeQuoteUpdateShortMsg(void *dst) {
  const BoeQuoteUpdateShortMsg fireMsg = {
      .StartOfMessage = 0xBABA,
      .MessageLength = sizeof(BoeQuoteUpdateShortMsg) - 2,
      .MessageType = 0x59,

      .Reserved1 = {'E', 'K', 'A'},
      .Reserved2 = {'Y', 'X'}};
  memcpy(dst, &fireMsg, sizeof(fireMsg));
  return sizeof(fireMsg);
}
/* ############################################# */

static int sendQEDMsg(TestCase *t) {
#if 0
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

  TEST_LOG(
      "sending MDIncrementalRefreshTradeSummary48 "
      "trigger to %s:%u",
      EKA_IP2STR(t->udpParams_[0]->mcDst.sin_addr.s_addr),
      be16toh(t->udpParams_[0]->mcDst.sin_port));
  if (sendto(t->udpParams_[0]->udpSock, pkt, payloadLen, 0,
             (const sockaddr *)&t->udpParams_[0]->mcDst,
             sizeof(t->udpParams_[0]->mcDst)) < 0)
    on_error("MC trigger send failed");
#endif
  return 0;
}
/* ############################################# */
#if 0
EfcUdpMcParams *createMcParams(TestCase *t) {
  auto grParams = new EfcUdpMcGroupParams;
  if (!grParams)
    on_error("failed on new EfcUdpMcGroupParams");
  grParams->coreId = t->coreId_;
  grParams->mcIp = t->mcParams_.mcIp.c_str();
  grParams->mcUdpPort = t->mcParams_.mcPort;

  auto udpMcParams = new EfcUdpMcParams;
  if (!udpMcParams)
    on_error("!udpMcParams");

  udpMcParams->groups = grParams;
  udpMcParams->nMcGroups = 1;

  return udpMcParams;
}
#endif
/* ############################################# */

void configureP4Test(EfcCtx *pEfcCtx, TestCase *t) {
  auto dev = pEfcCtx->dev;

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
      efcInitP4Strategy(pEfcCtx, udpMcParams, &p4Params);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitP4Strategy returned %d", (int)rc);

  auto p4TestCtx =
      dynamic_cast<EfcP4CboeTestCtx *>(t->stratCtx_);
  if (!p4TestCtx)
    on_error("!p4TestCtx");

  // subscribing on list of securities
  efcEnableFiringOnSec(pEfcCtx, p4TestCtx->secList,
                       p4TestCtx->nSec);

  // ==============================================
  // setting security contexts
  for (size_t i = 0; i < p4TestCtx->nSec; i++) {
    auto handle =
        getSecCtxHandle(pEfcCtx, p4TestCtx->secList[i]);

    if (handle < 0) {
      EKA_WARN("Security[%ju] %s was not "
               "fit into FPGA hash: handle = %jd",
               i, p4TestCtx->secIdString(i).c_str(),
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
        i, p4TestCtx->secIdString(i).c_str(),
        p4TestCtx->secList[i], handle, secCtx.bidMinPrice,
        secCtx.askMaxPrice, secCtx.bidSize, secCtx.askSize,
        secCtx.versionKey, secCtx.lowerBytesOfSecId);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
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
                "BOE FireMsg for MC core %d, "
                "MC gr %d, Tcp Conn %d",
                coreId, mcGr, tcpConn);
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

void configureQedTest(EfcCtx *pEfcCtx, TestCase *t) {
#if 0
  auto dev = pEfcCtx->dev;

  auto udpMcParams = &t->udpConf_;

  static const uint16_t QEDTestPurgeDSID = 0x1234;
  static const uint8_t QEDTestMinNumLevel = 5;

  EfcQedParams qedParams = {};
  int active_set = 3;
  qedParams.product[active_set].ds_id = QEDTestPurgeDSID;
  qedParams.product[active_set].min_num_level =
      QEDTestMinNumLevel;
  qedParams.product[active_set].enable = true;

  efcInitQedStrategy(pEfcCtx, udpMcParams, &qedParams);
  auto qedHwPurgeIAction =
      efcAllocateNewAction(dev, EpmActionType::QEDHwPurge);

  efcQedSetFireAction(pEfcCtx, qedHwPurgeIAction,
                      active_set);

  const char QEDTestPurgeMsg[] =
      "QED Purge Data With Dummy payload";

  int rc = setActionTcpSock(dev, qedHwPurgeIAction,
                            t->excSock_[0]);
  if (rc != EKA_OPRESULT__OK)
    on_error("setActionTcpSock failed for Action %d",
             qedHwPurgeIAction);

  rc = efcSetActionPayload(dev, qedHwPurgeIAction,
                           QEDTestPurgeMsg, 67);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcSetActionPayload failed for Action %d",
             qedHwPurgeIAction);
#endif
}

bool runP4Test(EfcCtx *pEfcCtx, TestCase *t) {

  TEST_LOG("\n"
           "=========== Running P4 Test ===========");

  auto p4TestCtx =
      dynamic_cast<EfcP4CboeTestCtx *>(t->stratCtx_);
  if (!p4TestCtx)
    on_error("!p4TestCtx");

  EfcArmVer p4ArmVer = 0;

  // ==============================================
  efcArmP4(pEfcCtx,
           p4ArmVer); //

  // ==============================================
  // Preparing UDP MC for MD trigger on GR#0

  uint32_t sequence = 32;

#if 1
  {
    char pktBuf[1500] = {};
    auto pktLen = p4TestCtx->createOrderExpanded(
        pktBuf, 0, SideT::BID, true);

    t->udpCtx_->sendPktToAllMcGrps(pktBuf, pktLen);
  }
#endif

#if 1
  {
    char pktBuf[1500] = {};
    auto pktLen = p4TestCtx->createOrderExpanded(
        pktBuf, 2, SideT::BID, true);

    t->udpCtx_->sendPktToAllMcGrps(pktBuf, pktLen);
  }
#endif

// ==============================================

// ==============================================
#if 0
  while (keep_work) {
    sendAddOrder(AddOrder::Expanded,t->udpParams_[0]->udpSock,&t->udpParams_[0]->mcDst,p4Ctx.at(2).id,
		 sequence++,'B',p4Ctx.at(2).bidMinPrice + 1,p4Ctx.at(2).size);

    sleep(1);
    efcArmP4(pEfcCtx, p4ArmVer++);
  }
#endif
  // ==============================================
  efcDisArmP4(pEfcCtx);
  TEST_LOG("\n"
           "===========================\n"
           "END OT CBOE P4 TEST\n"
           "===========================\n");
  return true;
}
/* ############################################# */

bool runQedTest(EfcCtx *pEfcCtx, TestCase *t) {
  int qedExpectedFires = 0;
  int TotalInjects = 4;
  EfcArmVer qedArmVer = 0;

  for (auto i = 0; i < TotalInjects; i++) {
    efcArmQed(pEfcCtx, qedArmVer++); // arm and promote
    qedExpectedFires++;

    sendQEDMsg(t);

    usleep(300000);
  }
  // ==============================================
  TEST_LOG("\n"
           "===========================\n"
           "END OT QED TEST\n"
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
#if 1
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

    // createTcpServ(dev, t);
    // tcpConnect(dev, &t.tcpParams_);
    // bindUdpSock(t);
  }

  // ==============================================

  EfcCtx efcCtx = {};
  EfcCtx *pEfcCtx = &efcCtx;

  EfcInitCtx initCtx = {
      .report_only = report_only,
      .watchdog_timeout_sec = 1000000,
  };

  rc = efcInit(&pEfcCtx, dev, &initCtx);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInit returned %d", (int)rc);

  EfcRunCtx runCtx = {};
  runCtx.onEfcFireReportCb = efcPrintFireReport;

  // ==============================================
  for (auto t : testCase)
    t->prepareTestConfig_(pEfcCtx, t);

  // ==============================================
  efcRun(pEfcCtx, &runCtx);
  // ==============================================

  for (auto t : testCase)
    t->runTest_(pEfcCtx, t);

#ifndef _VERILOG_SIM
  sleep(2);
  if (dontQuit)
    EKA_LOG("--Test finished, ctrl-c to end---");
  else {
    EKA_LOG("--Test finished---");
    keep_work = false;
  }
  while (keep_work) {
    sleep(0);
  }
#endif

  for (auto t : testCase)
    delete t;

  fflush(stdout);
  fflush(stderr);

  /* ============================================== */

  printf("Closing device\n");

  ekaDevClose(dev);
  sleep(1);
#endif
  return 0;
}
