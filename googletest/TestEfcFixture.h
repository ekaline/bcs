#ifndef __TEST_EFC_FIXTURE_H__
#define __TEST_EFC_FIXTURE_H__

#include <gtest/gtest.h>

#include "eka_macros.h"

#include "EkaFhTypes.h"
#include <Efc.h>
#include <Eka.h>
#include <Epm.h>
#include <Exc.h>

#include "TestTcpCtx.h"
#include "TestUdpCtx.h"

typedef EkaOpResult (*ArmControllerCb)(EkaDev *pEkaDev,
                                       EfcArmVer ver);
typedef EkaOpResult (*DisArmControllerCb)(EkaDev *pEkaDev);

class TestEfcFixture : public ::testing::Test {
protected:
  struct TestCaseConfig {
    const EfcUdpMcParams *mcParams;
    const TestTcpParams *tcpParams;
    const void *algoConfigParams;
    const void *mdInjectParams;
    bool loop = false; // endless loop
  };

  void SetUp() override;
  void TearDown() override;

  void runTest(const TestCaseConfig *tc);

  EfcArmVer sendPktToAll(const void *pkt, size_t pktLen,
                         EfcArmVer armVer);

  void configureFpgaPorts();

  virtual void configureStrat(const TestCaseConfig *t) = 0;
  virtual void sendData(const void *mdInjectParams) = 0;

  std::pair<bool, bool>
  waitForResults(uint32_t nStratEvaluated_prev,
                 uint32_t nStratPassed_prev,
                 int nExpectedFireReports);

  void initNwCtxs(const TestCaseConfig *t);

  void printTcpCtx() { return tcpCtx_->printConf(); }
  void printUdpCtx() { return udpCtx_->printConf(); }

  void checkFireReports(const TestCaseConfig *tc);
  void
  getReportPtrs(const void *p, size_t len,
                const EfcControllerState **ctrlState,
                const EfcExceptionsReport **excptReport,
                const SecCtx **secCtx,
                const EfcMdReport **mdReport,
                const uint8_t **firePkt,
                const EpmFireReport **epmReport,
                const EpmFastCancelReport **cmeFcReport,
                const EpmQEDReport **qedReport);

  void printTestConfig(const char *msg);

protected:
  static const int MaxTcpTestSessions = 16;
  static const int MaxUdpTestSessions = 64;
  static const uint16_t numTcpSess = 1;

  // P4: 0xf0340, Cme: 0xf0800, QED: 0xf0818
  uint32_t FireStatisticsAddr_ = 0;

  ArmControllerCb armController_;
  DisArmControllerCb disArmController_;

  char testName_[128] = {};
  char testLogFileName_[256] = {};

  EkaDev *dev_;

  TestUdpCtx *udpCtx_ = nullptr;
  TestTcpCtx *tcpCtx_ = nullptr;
  bool loop_ = false;

  int nExpectedFires = 0;

public:
  struct MemChunk {
    uint8_t *buf;
    size_t len;
  };

  std::vector<MemChunk *> fireReports = {};
  std::atomic<int> nReceivedFireReports = 0;

  std::vector<MemChunk *> echoedPkts = {};

  int nExpectedFireReports = 0;
};

/* --------------------------------------------- */

#endif
