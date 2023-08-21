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

volatile bool keep_work = true;
volatile bool serverSet = false;

/* --------------------------------------------- */

void TestEfcFixture::SetUp() {
  keep_work = true;

  const EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&dev_, &ekaDevInitCtx);
  ASSERT_NE(dev_, nullptr);
  return;
}
/* --------------------------------------------- */

void TestEfcFixture::TearDown() {
  keep_work = false;
  ASSERT_NE(dev_, nullptr);
  ekaDevClose(dev_);
  return;
}
/* --------------------------------------------- */

void TestEfcFixture::createTestCase(
    TestScenarioConfig &testScenario) {}
/* --------------------------------------------- */

void TestEfcFixture::runTest(const TestCase *testCase) {}

/* --------------------------------------------- */

void TestEfcFixture::getFireReport(const void *p,
                                   size_t len, void *ctx) {
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

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++)
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
  return nextArmVer;
}

/* ############################################# */

void TestCmeFc::configure(TestCase *t) {
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

bool TestCmeFc::run(TestCase *t) {
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
  TEST_LOG("\n"
           "===========================\n"
           "END OT CmeFc TEST\n"
           "===========================\n");
  return true;
}
