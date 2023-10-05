#ifndef __TEST_EFC_FIXTURE_H__
#define __TEST_EFC_FIXTURE_H__

#include <gtest/gtest.h>

#include "eka_macros.h"
#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <vector>

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

  virtual void runTest(const TestCaseConfig *tc);

  void sendPktToAll(const void *pkt, size_t pktLen,
                    bool expectedFire);

  void configureFpgaPorts();

  virtual void configureStrat(const TestCaseConfig *t){}

  virtual void generateMdDataPkts(const void *t){}

  virtual void sendData() {}

  virtual void checkAllCtxs() {}

  virtual void archiveSecCtxsMd() {}
  virtual void loadSecCtxsMd() {}

  std::pair<bool, bool>
  waitForResults(uint32_t nStratEvaluated_prev,
                 uint32_t nStratPassed_prev);

  void initNwCtxs(const TestCaseConfig *t);

  void printTcpCtx() { return tcpCtx_->printConf(); }
  void printUdpCtx() { return udpCtx_->printConf(); }

  virtual void
  checkAlgoCorrectness(const TestCaseConfig *tc) {}

  void getReportPtrs(const void *p, size_t len);

  void printTestConfig(const char *msg);

  virtual std::pair<uint32_t, bool> getArmVer();

protected:
  static const int MaxTcpTestSessions = 16;
  static const int MaxUdpTestSessions = 64;
  static const uint16_t numTcpSess = 1;
  static const int TcpDatagramOffset = 54;

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

  int nExpectedFires_ = -1;
  EfcArmVer armVer_ = 0;

  const EfcControllerState *ctrlState_ = nullptr;
  const EfcExceptionsReport *excptReport_ = nullptr;
  const SecCtx *secCtx_ = nullptr;
  const EfcMdReport *mdReport_ = nullptr;
  const uint8_t *firePkt_ = nullptr;
  const EpmFireReport *epmReport_ = nullptr;
  const EpmFastCancelReport *cmeFcReport_ = nullptr;
  const EpmQEDReport *qedReport_ = nullptr;

  bool testFailed_ = false;

  int ArmDisarmAddr_ = 0xf07d0; // overrided for P4

public:
  struct MemChunk {
    uint8_t *buf;
    size_t len;
  };

  std::vector<MemChunk *> fireReports_ = {};
  std::atomic<int> nReceivedFireReports_ = 0;

  std::vector<MemChunk *> echoedPkts_ = {};

  int nExpectedFireReports_ = 0;
};

/* --------------------------------------------- */

#endif
