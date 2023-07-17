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

using namespace Bats;
class TestCase;

void configureP4Test(EfcCtx *pEfcCtx, TestCase *t);
void configureQedTest(EfcCtx *pEfcCtx, TestCase *t);
bool runP4Test(EfcCtx *pEfcCtx, TestCase *t);
bool runQedTest(EfcCtx *pEfcCtx, TestCase *t);

/* --------------------------------------------- */
static const int MaxTcpTestSessions = 16;
static const int MaxUdpTestSessions = 64;
static uint16_t numTcpSess = 1;

enum class TestStrategy : int {
  Invalid = 0,
  P4,
  CmeFC,
  Qed
};

static const char *printStrat(TestStrategy s) {
  switch (s) {
  case TestStrategy::Invalid:
    return "Invalid";
  case TestStrategy::P4:
    return "P4";
  case TestStrategy::CmeFC:
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
    return TestStrategy::CmeFC;

  if (!strcmp(s, "Qed"))
    return TestStrategy::Qed;

  return TestStrategy::Invalid;
}

struct TestTcpSess {
  std::string srcIp;
  std::string dstIp;
  uint16_t dstPort;
};

struct TestUdpMc {
  std::string mcIp;
  uint16_t mcPort;
};

static TestTcpSess testDefaultTcpSess[] = {
    {"100.0.0.110", "10.0.0.10", 22222},
    {"200.0.0.110", "10.0.0.11", 33333}};

static TestUdpMc testDefaultUdpMc[] = {
    {"224.0.74.0", 30300}, {"224.0.74.1", 30301}};

typedef void (*PrepareTestConfigCb)(EfcCtx *efcCtx,
                                    TestCase *t);
typedef bool (*RunTestCb)(EfcCtx *efcCtx, TestCase *t);

class TestCase {
public:
  TestCase(int coreId, TestStrategy strat,
           PrepareTestConfigCb prepareTestConfig,
           RunTestCb runTest) {
    if (strat == TestStrategy::Invalid)
      on_error("Invalid Strategy %d", (int)strat);

    coreId_ = coreId;
    strat_ = strat;

    memcpy(&tcpSess_, &testDefaultTcpSess[coreId_],
           sizeof(tcpSess_));
    memcpy(&udpMc_, &testDefaultUdpMc[coreId_],
           sizeof(udpMc_));

    prepareTestConfig_ = prepareTestConfig;
    runTest_ = runTest;

    print("Created");
  }

  void print(const char *msg) {
    TEST_LOG("%s: Lane %d Strat: \'%s\' "
             "MC : %s:%u, "
             "TCP: %s --> %s:%u ",
             msg, coreId_, printStrat(strat_),
             udpMc_.mcIp.c_str(), udpMc_.mcPort,
             tcpSess_.srcIp.c_str(), tcpSess_.dstIp.c_str(),
             tcpSess_.dstPort);
  }

  EkaCoreId coreId_ = -1;
  TestStrategy strat_ = TestStrategy::Invalid;
  TestTcpSess tcpSess_ = {};
  TestUdpMc udpMc_ = {};

  int tcpSock_[MaxTcpTestSessions] = {};
  int udpSock_ = -1;
  struct sockaddr_in triggerMcAddr_ = {};

  ExcSocketHandle excSock_[MaxTcpTestSessions] = {};
  ExcConnHandle hCon_[MaxTcpTestSessions] = {};

  std::thread tcpServThr_[MaxTcpTestSessions];

  PrepareTestConfigCb prepareTestConfig_;
  RunTestCb runTest_;
};

static std::vector<TestCase *> testCase = {};

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
  EKA_LOG("Connected from: %s:%d -- sock=%d\n",
          inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),
          *sock);

  int status = fcntl(*sock, F_SETFL,
                     fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
  if (status == -1)
    on_error("fcntl error");

  return;
}

/* --------------------------------------------- */

static void tcpConnect(EkaDev *dev, TestCase *t) {
  for (auto i = 0; i < numTcpSess; i++) {
    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr =
        inet_addr(t->tcpSess_.dstIp.c_str());
    serverAddr.sin_port = be16toh(t->tcpSess_.dstPort + i);

    t->excSock_[i] = excSocket(dev, t->coreId_, 0, 0, 0);
    if (t->excSock_[i] < 0)
      on_error("failed to open sock %d", i);
    t->hCon_[i] = excConnect(dev, t->excSock_[i],
                             (sockaddr *)&serverAddr,
                             sizeof(sockaddr_in));
    if (t->hCon_[i] < 0)
      on_error("excConnect %d %s:%u", i,
               EKA_IP2STR(serverAddr.sin_addr.s_addr),
               be16toh(serverAddr.sin_port));
    const char *pkt =
        "\n\nThis is 1st TCP packet sent from FPGA TCP "
        "client to Kernel TCP server\n\n";
    excSend(dev, t->hCon_[i], pkt, strlen(pkt), 0);
    int bytes_read = 0;
    char rxBuf[2000] = {};
    bytes_read =
        recv(t->tcpSock_[i], rxBuf, sizeof(rxBuf), 0);
    if (bytes_read > 0)
      EKA_LOG("\n%s", rxBuf);
  }
}
/* --------------------------------------------- */

static void createTcpServ(EkaDev *dev, TestCase *t) {
  for (auto i = 0; i < numTcpSess; i++) {
    bool serverSet = false;
    t->tcpServThr_[i] =
        std::thread(tcpServer, dev, t->tcpSess_.dstIp,
                    t->tcpSess_.dstPort + i,
                    &t->tcpSock_[i], &serverSet);
    while (keep_work && !serverSet)
      std::this_thread::yield();
  }
}

static void configureFpgaPorts(EkaDev *dev, TestCase *t) {
  TEST_LOG("Initializing FPGA port %d to %s", t->coreId_,
           t->tcpSess_.srcIp.c_str());
  const EkaCoreInitCtx ekaCoreInitCtx = {
      .coreId = t->coreId_,
      .attrs = {
          .host_ip = inet_addr(t->tcpSess_.srcIp.c_str()),
          .netmask = inet_addr("255.255.255.0"),
          .gateway = inet_addr(t->tcpSess_.dstIp.c_str()),
          .nexthop_mac = {}, // resolved by our internal ARP
          .src_mac_addr = {}, // taken from system config
          .dont_garp = 0}};
  ekaDevConfigurePort(dev, &ekaCoreInitCtx);
}

/* --------------------------------------------- */

static void bindUdpSock(TestCase *t) {
  t->udpSock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (t->udpSock_ < 0)
    on_error("failed to open UDP sock");

  struct sockaddr_in triggerSourceAddr = {};
  triggerSourceAddr.sin_addr.s_addr =
      inet_addr(t->tcpSess_.dstIp.c_str());
  triggerSourceAddr.sin_family = AF_INET;
  triggerSourceAddr.sin_port = 0;

  if (bind(t->udpSock_, (sockaddr *)&triggerSourceAddr,
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
      inet_addr(t->udpMc_.mcIp.c_str());
  t->triggerMcAddr_.sin_port = be16toh(t->udpMc_.mcPort);
}

bool runEfh = false;
bool fatalDebug = false;
bool report_only = false;

void printUsage(char *cmd) {
  printf("USAGE: %s \n"
         "\t-L0 <P4 or Qed or CmeFC strategy on lane 0>\n"
         "\t-L1 <P4 or Qed or CmeFC strategy on lane 1>\n"
         "\t-r <Report Only ON>\n"
         "\t-l <Num of TCP sessions>\n"
         "\t-f <Run EFH for raw MD>\n"
         "\t-d <FATAL DEBUG ON>\n"
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
        {"L0", required_argument, 0, '0'},
        {"L1", required_argument, 0, '1'},
        {"ReportOnly", no_argument, 0, 'r'},
        {"report_only", no_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    c = getopt_long(argc, argv, "rh0:1:", long_options,
                    &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 0:
      printf("option %s", long_options[option_index].name);
      if (optarg)
        printf(" with arg %s", optarg);
      printf("\n");
      break;

    case '0':
    case '1': {
      auto coreId = c - '0';
      auto strat = string2strat(optarg);

      TestCase *t = nullptr;
      switch (strat) {
      case TestStrategy::P4:
        t = new TestCase(coreId, strat, configureP4Test,
                         runP4Test);
        break;
      case TestStrategy::Qed:
        t = new TestCase(coreId, strat, configureQedTest,
                         runQedTest);
        break;
      default:
        on_error("Unexpected Test Strategy %d", (int)strat);
      }
      if (!t)
        on_error("Failed on new TestCase()");

      testCase.push_back(t);
    } break;

    case '2':
      if (digit_optind != 0 &&
          digit_optind != this_option_optind)
        printf("digits occur in two different "
               "argv-elements.\n");
      digit_optind = this_option_optind;
      printf("option %c\n", c);
      break;

    case 'h':
      printUsage(argv[0]);
      exit(1);
      break;

    case 'r':
      printf("Report Only = ON\n");
      report_only = true;
      break;

    case '?':
      break;

    default:
      printf("?? getopt returned character code 0%o ??\n",
             c);
    }
  }

  if (optind < argc) {
    auto badIdx = optind;
    printf("non-option ARGV-elements: ");
    while (optind < argc)
      printf("%s ", argv[optind++]);
    printf("\n");
    on_error("Unexpected options %s", argv[badIdx]);
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

/* --------------------------------------------- */

struct P4SecurityCtx {
  char id[8];
  EkaLSI groupId;
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t size;
};

std::vector<P4SecurityCtx> p4Ctx{};

enum class AddOrder : int {
  Unknown = 0,
  Short,
  Long,
  Expanded
};

struct CboePitchAddOrderShort {
  sequenced_unit_header hdr;
  add_order_short msg;
} __attribute__((packed));

struct CboePitchAddOrderLong {
  sequenced_unit_header hdr;
  add_order_long msg;
} __attribute__((packed));

struct CboePitchAddOrderExpanded {
  sequenced_unit_header hdr;
  add_order_expanded msg;
} __attribute__((packed));
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
  TEST_LOG("sending AddOrderShort trigger to %s:%u",
           EKA_IP2STR(addr->sin_addr.s_addr),
           be16toh(addr->sin_port));
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

  TEST_LOG("sending MDIncrementalRefreshTradeSummary48 "
           "trigger to %s:%u",
           EKA_IP2STR(t->triggerMcAddr_.sin_addr.s_addr),
           be16toh(t->triggerMcAddr_.sin_port));
  if (sendto(t->udpSock_, pkt, payloadLen, 0,
             (const sockaddr *)&t->triggerMcAddr_,
             sizeof(t->triggerMcAddr_)) < 0)
    on_error("MC trigger send failed");
  return 0;
}
/* ############################################# */

EfcUdpMcParams *createMcParams(TestCase *t) {
  auto grParams = new EfcUdpMcGroupParams;
  if (!grParams)
    on_error("failed on new EfcUdpMcGroupParams");
  grParams->coreId = t->coreId_;
  grParams->mcIp = t->udpMc_.mcIp.c_str();
  grParams->mcUdpPort = t->udpMc_.mcPort;

  auto udpMcParams = new EfcUdpMcParams;
  if (!udpMcParams)
    on_error("!udpMcParams");

  udpMcParams->groups = grParams;
  udpMcParams->nMcGroups = 1;

  return udpMcParams;
}

/* ############################################# */

void configureP4Test(EfcCtx *pEfcCtx, TestCase *t) {
  auto dev = pEfcCtx->dev;

  if (!t)
    on_error("!t");

  auto udpMcParams = createMcParams(t);

  struct EfcP4Params p4Params = {
      .feedVer = EfhFeedVer::kCBOE,
      .fireOnAllAddOrders = true,
      .max_size = 10000,
  };

  int rc =
      efcInitP4Strategy(pEfcCtx, udpMcParams, &p4Params);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitP4Strategy returned %d", (int)rc);

  // ==============================================
  // Subscribing on securities

  P4SecurityCtx security[] = {
      {{'\0', '\0', '0', '0', 'T', 'E', 'S', 'T'},
       0,
       1000,
       2000,
       1}, // correct SecID
      {{'\0', '\0', '0', '1', 'T', 'E', 'S', 'T'},
       0,
       3000,
       4000,
       1}, // correct SecID
      {{'\0', '\0', '0', '2', 'T', 'E', 'S', 'T'},
       0,
       5000,
       6000,
       1}, // correct SecID
      {{'\0', '\0', '0', '3', 'T', 'E', 'S', 'T'},
       0,
       7000,
       8000,
       1}, // correct SecID
      {{'\0', '\0', '0', '2', 'A', 'B', 'C', 'D'},
       0,
       7000,
       8000,
       1}, // correct SecID
  };

  // list of secIDs to be passed to efcEnableFiringOnSec()
  uint64_t securityList[std::size(security)] = {};

  // This Alphanumeric-to-binary converting is not needed
  // if binary security ID is received from new version of
  // EFH
  for (auto i = 0; i < (int)std::size(security); i++) {
    securityList[i] = be64toh(*(uint64_t *)security[i].id);
    EKA_TEST(
        "securityList[%d] = '%c%c%c%c%c%c%c%c' =  %016jx",
        i, security[i].id[0], security[i].id[1],
        security[i].id[2], security[i].id[3],
        security[i].id[4], security[i].id[5],
        security[i].id[6], security[i].id[7],
        securityList[i]);

    p4Ctx.push_back(security[i]);
  }

  // subscribing on list of securities
  efcEnableFiringOnSec(pEfcCtx, securityList,
                       std::size(security));

  // ==============================================
  // setting security contexts
  for (auto i = 0; i < (int)std::size(securityList); i++) {
    auto handle = getSecCtxHandle(pEfcCtx, securityList[i]);
    if (handle < 0) {
      EKA_WARN("Security[%d] '%c%c%c%c%c%c%c%c' was not "
               "fit into FPGA hash: handle = %jd",
               i, security[i].id[0], security[i].id[1],
               security[i].id[2], security[i].id[3],
               security[i].id[4], security[i].id[5],
               security[i].id[6], security[i].id[7],
               handle);
      continue;
    }
    SecCtx secCtx = {
        .bidMinPrice =
            static_cast<decltype(secCtx.bidMinPrice)>(
                security[i].bidMinPrice /
                100), // x100, should be nonzero
        .askMaxPrice =
            static_cast<decltype(secCtx.askMaxPrice)>(
                security[i].askMaxPrice / 100), // x100
        .bidSize = security[i].size,
        .askSize = security[i].size,
        .versionKey = (uint8_t)i,
        .lowerBytesOfSecId =
            (uint8_t)(securityList[i] & 0xFF)};
    EKA_TEST("Setting StaticSecCtx[%d] secId=0x%016jx, "
             "handle=%jd",
             i, securityList[i], handle);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(pEfcCtx, handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK)
      on_error("failed to efcSetStaticSecCtx");
  }

  // ==============================================
  // Manually prepared FPGA firing template

  char fireMsg[1500] = {};
  size_t fireMsgLen =
      prepare_BoeQuoteUpdateShortMsg(fireMsg);

  // ==============================================
  for (auto i = 0; i < udpMcParams->nMcGroups; i++) {
    rc = setActionTcpSock(dev, i, t->excSock_[i]);
    if (rc != EKA_OPRESULT__OK)
      on_error("setActionTcpSock failed for Action %d", i);

    rc = efcSetActionPayload(dev, i, fireMsg, fireMsgLen);
    if (rc != EKA_OPRESULT__OK)
      on_error("efcSetActionPayload failed for Action %d",
               i);
  }
}
/* ############################################# */

void configureQedTest(EfcCtx *pEfcCtx, TestCase *t) {
  auto dev = pEfcCtx->dev;

  auto udpMcParams = createMcParams(t);

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
}

bool runP4Test(EfcCtx *pEfcCtx, TestCase *t) {
  EfcArmVer p4ArmVer = 0;

  // ==============================================
  efcArmP4(pEfcCtx,
           p4ArmVer); //

  // ==============================================
  // Preparing UDP MC for MD trigger on GR#0

  uint32_t sequence = 32;

// ==============================================
#if 1
  sendAddOrder(
      AddOrder::Short, t->udpSock_, &t->triggerMcAddr_,
      p4Ctx.at(2).id, sequence++, 'S',
      p4Ctx.at(2).askMaxPrice / 100 - 1, p4Ctx.at(2).size);

  sleep(1);
  efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 1
  sendAddOrder(
      AddOrder::Short, t->udpSock_, &t->triggerMcAddr_,
      p4Ctx.at(2).id, sequence++, 'B',
      p4Ctx.at(2).bidMinPrice / 100 + 1, p4Ctx.at(2).size);

  sleep(1);
  efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 1
  sendAddOrder(AddOrder::Long, t->udpSock_,
               &t->triggerMcAddr_, p4Ctx.at(2).id,
               sequence++, 'S', p4Ctx.at(2).askMaxPrice - 1,
               p4Ctx.at(2).size);

  sleep(1);
  efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 1
  sendAddOrder(AddOrder::Long, t->udpSock_,
               &t->triggerMcAddr_, p4Ctx.at(2).id,
               sequence++, 'B', p4Ctx.at(2).bidMinPrice + 1,
               p4Ctx.at(2).size);

  sleep(1);
  efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 1
  sendAddOrder(AddOrder::Expanded, t->udpSock_,
               &t->triggerMcAddr_, p4Ctx.at(2).id,
               sequence++, 'S', p4Ctx.at(2).askMaxPrice - 1,
               p4Ctx.at(2).size);

  sleep(1);
  efcArmP4(pEfcCtx, p4ArmVer++);
#endif
// ==============================================
#if 0
  while (keep_work) {
    sendAddOrder(AddOrder::Expanded,t->udpSock_,&t->triggerMcAddr_,p4Ctx.at(2).id,
		 sequence++,'B',p4Ctx.at(2).bidMinPrice + 1,p4Ctx.at(2).size);
  
    sleep(1);
    efcArmP4(pEfcCtx,, p4ArmVer++);
  }
#endif
  // ==============================================
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

  if (numTcpSess > MaxTcpTestSessions)
    on_error("numTcpSess %d > MaxTcpTestSessions %d",
             numTcpSess, MaxTcpTestSessions);
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
    createTcpServ(dev, t);
    tcpConnect(dev, t);
    bindUdpSock(t);
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
  EKA_LOG("--Test finished, ctrl-c to end---");
  //  keep_work = false;
  while (keep_work) {
    sleep(0);
  }
#endif

  for (auto t : testCase)
    for (auto i = 0; i < numTcpSess; i++)
      t->tcpServThr_[i].join();

  fflush(stdout);
  fflush(stderr);

  /* ============================================== */

  printf("Closing device\n");

  ekaDevClose(dev);
  sleep(1);

  return 0;
}
