#include <arpa/inet.h>
#include <chrono>
#include <gtest/gtest.h>
#include <netinet/ether.h>
#include <optional>
#include <pthread.h>
#include <span>
#include <sstream>
#include <stdio.h>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <compat/Efc.h>
#include <compat/Eka.h>
#include <compat/Epm.h>
#include <compat/Exc.h>

#include "eka_macros.h"

#include "TestEfcFixture.h"

#include "Efc.h"

volatile bool keep_work = true;
volatile bool serverSet = false;

/* --------------------------------------------- */

static void configureFpgaPorts(EkaDev *dev, TestCase *t) {
  EKA_LOG("Initializing %ju ports", t->tcpCtx_->nTcpSess_);
  for (auto i = 0; i < t->tcpCtx_->nTcpSess_; i++) {
    EKA_LOG("Initializing FPGA port %d to %s",
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

void TestEfcFixture::SetUp() {
  keep_work = true;
#if 0
  const EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&dev_, &ekaDevInitCtx);
  ASSERT_NE(dev_, nullptr);

  const testing::TestInfo *const test_info =
      testing::UnitTest::GetInstance()->current_test_info();

  sprintf(testName_, "%s__%s", test_info->test_case_name(),
          test_info->name());
  sprintf(testLogFileName_, "%s/%s.log", "gtest_logs",
          testName_);
#endif
  return;
}
/* --------------------------------------------- */

void TestEfcFixture::TearDown() {
  keep_work = false;
#if 0
  EKA_LOG("\n"
          "===========================\n"
          "END OF %s TEST\n"
          "===========================\n",
          testName_);

  ASSERT_NE(dev_, nullptr);
  ekaDevClose(dev_);
#endif
  return;
}
/* --------------------------------------------- */

void TestEfcFixture::createTestCase(
    TestScenarioConfig &testScenario) {}

/* --------------------------------------------- */

static void getFireReport(const void *p, size_t len,
                          void *ctx) {
  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(p)};
  if (containerHdr->type == EkaEventType::kExceptionEvent)
    return;

  auto tFixturePtr = static_cast<TestEfcFixture *>(ctx);
  if (!tFixturePtr)
    on_error("!tFixturePtr");

  auto fr = new TestEfcFixture::FireReport;
  if (!fr)
    on_error("!fr");

  fr->buf = new uint8_t[len];
  if (!fr->buf)
    on_error("!fr->buf");
  fr->len = len;

  memcpy(fr->buf, p, len);
  tFixturePtr->fireReports.push_back(fr);
  tFixturePtr->nReceivedFireReports++;
  return;
}
/* --------------------------------------------- */
void TestEfcFixture::testPrologue() {
  const testing::TestInfo *const test_info =
      testing::UnitTest::GetInstance()->current_test_info();

  sprintf(testName_, "%s__%s", test_info->test_case_name(),
          test_info->name());
  sprintf(testLogFileName_, "%s/%s.log", "gtest_logs",
          testName_);

  if ((g_ekaLogFile = fopen(testLogFileName_, "w")) ==
      nullptr)
    on_error("Failed to open %s file for writing",
             testLogFileName_);

  const EkaDevInitCtx ekaDevInitCtx = {.logContext =
                                           g_ekaLogFile};
  ekaDevInit(&dev_, &ekaDevInitCtx);
  ASSERT_NE(dev_, nullptr);
}

/* --------------------------------------------- */
void TestEfcFixture::testEpilogue() {
  EKA_LOG("\n"
          "===========================\n"
          "END OF %s TEST\n"
          "===========================\n",
          testName_);

  ASSERT_NE(dev_, nullptr);
  ekaDevClose(dev_);

  fclose(g_ekaLogFile);
}
/* --------------------------------------------- */

void TestEfcFixture::runTest(const TestCaseConfig &tc) {
  testPrologue();

  auto t = new TestCase(tc);
  ASSERT_NE(t, nullptr);

  t->print("Running");

  commonInit(t);
  configure(t);

  EfcRunCtx runCtx = {};
  // runCtx.onEfcFireReportCb = efcPrintFireReport;
  runCtx.onEfcFireReportCb = getFireReport;
  runCtx.cbCtx = this;

  efcRun(g_ekaDev, &runCtx);

  run(t);
  delete t;
  testEpilogue();
}
/* --------------------------------------------- */

static std::pair<uint32_t, uint32_t>
getP4stratStatistics(uint64_t fireStatisticsAddr) {
  uint64_t var_p4_cont_counter1 =
      eka_read(fireStatisticsAddr);

  auto strategyRuns = (var_p4_cont_counter1 >> 0) & MASK32;
  auto strategyPassed =
      (var_p4_cont_counter1 >> 32) & MASK32;

  return std::pair<uint32_t, uint32_t>{strategyRuns,
                                       strategyPassed};
}

/* --------------------------------------------- */

EfcArmVer TestEfcFixture::sendPktToAll(const void *pkt,
                                       size_t pktLen,
                                       EfcArmVer armVer,
                                       TestCase *t) {

  auto armControllerCb = t->armController_;
  auto fireStatisticsAddr = t->FireStatisticsAddr_;
  auto nFiresPerUdp = t->tcpCtx_->nTcpSess_;

  if (!armControllerCb)
    on_error("!armControllerCb");

  auto nextArmVer = armVer;

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    for (auto i = 0; i < t->udpCtx_->nMcCons_[coreId];
         i++) {
      if (!t->udpCtx_->udpConn_[coreId][i])
        on_error("!udpConn_[%d][%d]", coreId, i);

      armControllerCb(g_ekaDev, nextArmVer++);
      auto [strategyRuns_prev, strategyPassed_prev] =
          getP4stratStatistics(fireStatisticsAddr);

      t->udpCtx_->udpConn_[coreId][i]->sendUdpPkt(pkt,
                                                  pktLen);
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
  }

  return nextArmVer;
}

/* ############################################# */
void TestEfcFixture::commonInit(TestCase *t) {
  ASSERT_NE(t, nullptr);

  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  if (!t)
    on_error("!t");

  configureFpgaPorts(dev, t);
  t->tcpCtx_->connectAll();

  EfcInitCtx initCtx = {.watchdog_timeout_sec = 1000000};

  EkaOpResult efcInitRc = efcInit(dev, &initCtx);
  ASSERT_EQ(efcInitRc, EKA_OPRESULT__OK);
}
/* ############################################# */

void TestCmeFc::configure(TestCase *t) {
  ASSERT_NE(t, nullptr);
  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  EKA_LOG("\n"
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

void TestP4::configure(TestCase *t) {
  ASSERT_NE(t, nullptr);
  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  EKA_LOG("\n"
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

    EKA_LOG(
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

void TestCmeFc::run(TestCase *t) {
  ASSERT_NE(t, nullptr);

  int cmeFcExpectedFires = 0;
  int TotalInjects = 4;
  EfcArmVer cmeFcArmVer = 0;

  uint64_t appSeq = 3000;
  efcSetSessionCntr(g_ekaDev,
                    t->tcpCtx_->tcpSess_[0]->hCon_, appSeq);

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

    cmeFcArmVer =
        sendPktToAll(pktBuf, pktLen, cmeFcArmVer, t);

    // usleep(300000);
  }
  // ==============================================

  return;
}

/* ############################################# */

void TestP4::selectAddOrderExpandedParams(
    const P4SecurityCtx *secCtx, SideT side,
    bool expectedFire) {}

/* ############################################# */

void TestP4::run(TestCase *t) {
  ASSERT_NE(t, nullptr);
  EKA_LOG("\n"
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

      p4ArmVer = sendPktToAll(pktBuf, pktLen, p4ArmVer, t);

      auto fReport = fireReports.back();
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

      p4ArmVer = sendPktToAll(pktBuf, pktLen, p4ArmVer, t);
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
}
