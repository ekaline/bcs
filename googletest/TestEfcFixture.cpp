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
  TEST_LOG("Im here");

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
void TestEfcFixture::testPrologue(
    const TestCaseConfig *tc) {
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
  g_ekaLogFile = stdout;

  const EkaDevInitCtx ekaDevInitCtx = {.logContext =
                                           g_ekaLogFile};
  ekaDevInit(&dev_, &ekaDevInitCtx);
  ASSERT_NE(dev_, nullptr);

  udpCtx_ = new TestUdpCtx(tc->mcParams);
  if (!udpCtx_)
    on_error("failed on new TestUdpCtx()");

  tcpCtx_ = new TestTcpCtx(tc->tcpParams);
  if (!tcpCtx_)
    on_error("failed on new TestTcpCtx()");
}

/* --------------------------------------------- */
void TestEfcFixture::testEpilogue() {
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
}
/* --------------------------------------------- */

void TestEfcFixture::runTest(const TestCaseConfig *tc) {
  testPrologue(tc);

  print("Running");

  commonInit();
  configure(tc);

  EfcRunCtx runCtx = {.onEfcFireReportCb = getFireReport,
                      .cbCtx = this};
  efcRun(g_ekaDev, &runCtx);

  sendData(tc->mdInjectParams);
  disArmController_(g_ekaDev);

  testEpilogue();
  delete udpCtx_;
  delete tcpCtx_;
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
                                       EfcArmVer armVer) {

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
      auto [strategyRuns_prev, strategyPassed_prev] =
          getP4stratStatistics(FireStatisticsAddr_);

      char udpConParamsStr[128] = {};
      udpCon->printMcConnParams(udpConParamsStr);
      TEST_LOG("Sending UPD MD to %s", udpConParamsStr);

      udpCon->sendUdpPkt(pkt, pktLen);
      nExpectedFireReports += nFiresPerUdp;
      bool fired = false;
      while (keep_work && !fired &&
             nExpectedFireReports !=
                 nReceivedFireReports.load()) {
        auto [strategyRuns_post, strategyPassed_post] =
            getP4stratStatistics(FireStatisticsAddr_);

        fired = (strategyRuns_post != strategyRuns_prev);
        if (!fired) {
          TEST_LOG("[strategyRuns_prev %u, "
                   "strategyPassed_prev %u] !="
                   "[strategyRuns_post %u, "
                   "strategyPassed_post %u]",
                   strategyRuns_prev, strategyPassed_prev,
                   strategyRuns_post, strategyPassed_post);
        }
        std::this_thread::yield();
      }
    }
  }

  return nextArmVer;
}

/* ############################################# */
void TestEfcFixture::commonInit() {
  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  configureFpgaPorts();
  tcpCtx_->connectAll();

  EfcInitCtx initCtx = {.watchdog_timeout_sec = 1000000};

  EkaOpResult efcInitRc = efcInit(dev, &initCtx);
  ASSERT_EQ(efcInitRc, EKA_OPRESULT__OK);
}
/* ############################################# */

/* ############################################# */
