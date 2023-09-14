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
#include "TestP4.h"

#include "Efc.h"

volatile bool keep_work = true;
volatile bool serverSet = false;

/* --------------------------------------------- */

void TestEfcFixture::configureFpgaPorts() {
  EKA_LOG("Initializing %ju ports", tcpCtx_->nTcpSess_);
  for (auto i = 0; i < tcpCtx_->nTcpSess_; i++) {
    EKA_LOG("Initializing FPGA port %d to %s",
            tcpCtx_->tcpSess_[i]->coreId_,
            tcpCtx_->tcpSess_[i]->srcIp_);
    const EkaCoreInitCtx ekaCoreInitCtx = {
        .coreId = (EkaCoreId)tcpCtx_->tcpSess_[i]->coreId_,
        .attrs = {
            .host_ip =
                inet_addr(tcpCtx_->tcpSess_[i]->srcIp_),
            .netmask = inet_addr("255.255.255.0"),
            .gateway =
                inet_addr(tcpCtx_->tcpSess_[i]->dstIp_),
            .nexthop_mac =
                {}, // resolved by our internal ARP
            .src_mac_addr = {}, // taken from system config
            .dont_garp = 0}};
    ekaDevConfigurePort(g_ekaDev, &ekaCoreInitCtx);
  }
}

/* --------------------------------------------- */

void TestEfcFixture::SetUp() {

  keep_work = true;

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

  // TMP PATCH !!!
  // g_ekaLogFile = stdout;

  const EkaDevInitCtx ekaDevInitCtx = {.logContext =
                                           g_ekaLogFile};
  ekaDevInit(&dev_, &ekaDevInitCtx);
  ASSERT_NE(dev_, nullptr);

  return;
}
/* --------------------------------------------- */

void TestEfcFixture::TearDown() {
  // sleep(1);
  keep_work = false;

  EKA_LOG("\n"
          "===========================\n"
          "END OF %s TEST\n"
          "===========================\n",
          testName_);

  if (udpCtx_)
    delete udpCtx_;
  if (tcpCtx_)
    delete tcpCtx_;

  ASSERT_NE(dev_, nullptr);
  ekaDevClose(dev_);

  fclose(g_ekaLogFile);

  for (auto &p : fireReports_) {
    delete[] p->buf;
  }
  fireReports_.clear();

  for (auto &p : echoedPkts_) {
    delete[] p->buf;
  }
  echoedPkts_.clear();

  return;
}

/* --------------------------------------------- */

static void getFireReport(const void *p, size_t len,
                          void *ctx) {
  fflush(g_ekaLogFile);
  // EKA_LOG("Received Some Report");

  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(p)};
  if (containerHdr->type == EkaEventType::kExceptionEvent)
    return;

  auto tFixturePtr = static_cast<TestEfcFixture *>(ctx);
  if (!tFixturePtr)
    on_error("!tFixturePtr");

  tFixturePtr->nReceivedFireReports_++;
  EKA_LOG("Received FireReport: nReceivedFireReports=%d",
          (int)tFixturePtr->nReceivedFireReports_);

  auto fr = new TestEfcFixture::MemChunk;
  if (!fr)
    on_error("!fr");

  fr->buf = new uint8_t[len];
  if (!fr->buf)
    on_error("!fr->buf");
  fr->len = len;

  memcpy(fr->buf, p, len);
  tFixturePtr->fireReports_.push_back(fr);

  EKA_LOG("FireReport pushed: nReceivedFireReports_=%d",
          (int)tFixturePtr->nReceivedFireReports_);
  fflush(g_ekaLogFile);
  return;
}
/* --------------------------------------------- */
void TestEfcFixture::initNwCtxs(const TestCaseConfig *tc) {

  udpCtx_ = new TestUdpCtx(tc->mcParams);
  if (!udpCtx_)
    on_error("failed on new TestUdpCtx()");

  tcpCtx_ = new TestTcpCtx(tc->tcpParams);
  if (!tcpCtx_)
    on_error("failed on new TestTcpCtx()");
}

/* --------------------------------------------- */

void TestEfcFixture::runTest(const TestCaseConfig *tc) {
  initNwCtxs(tc);
  printTestConfig("Running");

  configureFpgaPorts();

  EfcInitCtx initCtx = {.watchdog_timeout_sec = 1000000};
  EkaOpResult efcInitRc = efcInit(g_ekaDev, &initCtx);
  ASSERT_EQ(efcInitRc, EKA_OPRESULT__OK);

  tcpCtx_->connectAll();

  configureStrat(tc);

  EfcRunCtx runCtx = {.onEfcFireReportCb = getFireReport,
                      .cbCtx = this};
  efcRun(g_ekaDev, &runCtx);

  generateMdDataPkts(tc->mdInjectParams);
  sendData();

  disArmController_(g_ekaDev);

  EXPECT_EQ(nExpectedFires, fireReports_.size());
  EXPECT_EQ(echoedPkts_.size(), fireReports_.size());

  checkAlgoCorrectness(tc);
}
/* --------------------------------------------- */
void TestEfcFixture::getReportPtrs(const void *p,
                                   size_t len) {

  auto b = static_cast<const uint8_t *>(p);
  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(b)};
  b += sizeof(*containerHdr);

  for (uint i = 0; i < containerHdr->num_of_reports; i++) {
    auto reportHdr{
        reinterpret_cast<const EfcReportHdr *>(b)};
    b += sizeof(*reportHdr);
    switch (reportHdr->type) {
    case EfcReportType::kControllerState:
      ctrlState_ =
          reinterpret_cast<const EfcControllerState *>(b);
      // TEST_LOG("EfcReportType::kControllerState");
      break;

    case EfcReportType::kExceptionReport:
      excptReport_ =
          reinterpret_cast<const EfcExceptionsReport *>(b);
      // TEST_LOG("EfcReportType::kExceptionReport");
      break;

    case EfcReportType::kMdReport:
      mdReport_ = reinterpret_cast<const EfcMdReport *>(b);
      // TEST_LOG("EfcReportType::kMdReport");
      break;

    case EfcReportType::kSecurityCtx:
      secCtx_ = reinterpret_cast<const SecCtx *>(b);
      break;

    case EfcReportType::kFirePkt:
      firePkt_ = b;
      // TEST_LOG("EfcReportType::kFirePkt");
      break;

    case EfcReportType::kEpmReport:
      epmReport_ =
          reinterpret_cast<const EpmFireReport *>(b);
      // TEST_LOG("EfcReportType::kEpmReport");
      break;

    case EfcReportType::kFastCancelReport:
      cmeFcReport_ =
          reinterpret_cast<const EpmFastCancelReport *>(b);
      // TEST_LOG("EfcReportType::kFastCancelReport");
      break;

    case EfcReportType::kQEDReport:
      qedReport_ =
          reinterpret_cast<const EpmQEDReport *>(b);
      // TEST_LOG("EfcReportType::kQEDReport");
      break;

    default:
      on_error("Unexpected reportHdr->type %d",
               (int)reportHdr->type);
    }
    b += reportHdr->size;
  }
}

/* --------------------------------------------- */

static std::pair<uint32_t, uint32_t>
getP4stratStatistics(uint64_t fireStatisticsAddr) {
  uint64_t var_p4_cont_counter1 =
      eka_read(fireStatisticsAddr);

  auto nStrategyRuns = (var_p4_cont_counter1 >> 0) & MASK32;
  auto nStrategyPassed =
      (var_p4_cont_counter1 >> 32) & MASK32;

  return std::pair<uint32_t, uint32_t>{nStrategyRuns,
                                       nStrategyPassed};
}

/* --------------------------------------------- */
std::pair<bool, bool> TestEfcFixture::waitForResults(
    uint32_t nStratEvaluated_prev,
    uint32_t nStratPassed_prev, int nExpectedFireReports_) {
  bool stratEvaluated = false;
  bool stratPassed = false;
  while (keep_work && !stratEvaluated) {
    auto [nStratEvaluated_post, nStratPassed_post] =
        getP4stratStatistics(FireStatisticsAddr_);

    stratEvaluated =
        (nStratEvaluated_post != nStratEvaluated_prev);
    stratPassed = (nStratPassed_post != nStratPassed_prev);
    if (!stratEvaluated) {
      EKA_LOG("[nStratEvaluated_prev %u, "
              "nStratPassed_prev %u] !="
              "[nStratEvaluated_post %u, "
              "nStratPassed_post %u]",
              nStratEvaluated_prev, nStratPassed_prev,
              nStratEvaluated_post, nStratPassed_post);
    }

    if (stratPassed) {
      while (nExpectedFireReports_ !=
             nReceivedFireReports_.load()) {
#if 0
        EKA_LOG("Waiting for FireReport: "
                "nExpectedFireReports_ = %d, "
                "nReceivedFireReports_ = %d",
                nExpectedFireReports_,
                nReceivedFireReports_.load());
#endif
        std::this_thread::yield();
      }
    }
    std::this_thread::yield();
  } // waiting loop

  EKA_LOG("Waiting completed");
  return std::pair<bool, bool>(stratEvaluated, stratPassed);
}
/* --------------------------------------------- */

EfcArmVer TestEfcFixture::sendPktToAll(const void *pkt,
                                       size_t pktLen,
                                       EfcArmVer armVer,
                                       bool expectedFire) {

  auto nFiresPerUdp = tcpCtx_->nTcpSess_;

  if (!armController_)
    on_error("!armController_");

  auto nextArmVer = armVer;

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    for (auto i = 0; i < udpCtx_->nMcCons_[coreId]; i++) {
      auto udpCon = udpCtx_->udpConn_[coreId][i];

      if (!udpCon)
        on_error("!udpConn_[%d][%d]", coreId, i);

      armController_(g_ekaDev, nextArmVer++);

      char udpConParamsStr[128] = {};
      udpCon->printMcConnParams(udpConParamsStr);

      auto [nStratEvaluated_prev, nStratPassed_prev] =
          getP4stratStatistics(FireStatisticsAddr_);
      // ==============================================
      EKA_LOG("Sending UPD MD to %s", udpConParamsStr);
      udpCon->sendUdpPkt(pkt, pktLen);
      // ==============================================
      auto [stratEvaluated, stratPassed] =
          waitForResults(nStratEvaluated_prev,
                         nStratPassed_prev, nExpectedFires);

      if (stratPassed) {
        for (auto i = 0; i < tcpCtx_->nTcpSess_; i++) {
          int len = 0;
          char rxBuf[8192] = {};
          while (len <= 0) {
            len = excRecv(g_ekaDev,
                          tcpCtx_->tcpSess_[i]->hCon_,
                          rxBuf, sizeof(rxBuf), 0);
          }
          char bufStr[10000] = {};
          hexDump2str("Echoed TCP pkt", rxBuf, len, bufStr,
                      sizeof(bufStr));
          EKA_LOG("TCP Sess %d: %s", i, bufStr);

          auto ep = new TestEfcFixture::MemChunk;
          if (!ep)
            on_error("!ep");

          ep->buf = new uint8_t[len];
          if (!ep->buf)
            on_error("!ep->buf");
          ep->len = len;

          memcpy(ep->buf, rxBuf, len);
          echoedPkts_.push_back(ep);
        }
      }

    } // iteration per MC grp
  }   // iteration per Core

  return nextArmVer;
}

/* ############################################# */

/* ############################################# */
void TestEfcFixture::printTestConfig(const char *msg) {
  EKA_LOG("\n%s: \'%s\' ", msg, testName_);

  printTcpCtx();
  printUdpCtx();
}
/* ############################################# */
