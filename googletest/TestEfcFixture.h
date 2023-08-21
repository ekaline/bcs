#ifndef __TEST_EFC_FIXTURE_H__
#define __TEST_EFC_FIXTURE_H__

#include "eka_macros.h"

#include "TestCaseConfig.h"

class TestEfcFixture : public ::testing::Test {
protected:
  void SetUp() override;
  void TearDown() override;

  void createTestCase(TestScenarioConfig &testScenario);

  void runTest(const TestCase *testCase);

  void getFireReport(const void *p, size_t len, void *ctx);

  EfcArmVer sendPktToAll(const void *pkt, size_t pktLen,
                         EfcArmVer armVer, TestCase *t);

  virtual void configure(TestCase *t){};
  virtual bool run(TestCase *t) { return false; };

protected:
  static const int MaxTcpTestSessions = 16;
  static const int MaxUdpTestSessions = 64;
  static const uint16_t numTcpSess = 1;

  /* --------------------------------------------- */

  EkaDev *dev_;

  struct FireReport {
    uint8_t *buf;
    size_t len;
  };

  std::vector<FireReport *> fireReports = {};
  std::atomic<int> nReceivedFireReports = 0;
  int nExpectedFireReports = 0;
};

class TestCmeFc : public TestEfcFixture {
protected:
  void configure(TestCase *t) override;
  bool run(TestCase *t) override;
};
#endif
