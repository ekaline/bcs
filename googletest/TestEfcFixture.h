#ifndef __TEST_EFC_FIXTURE_H__
#define __TEST_EFC_FIXTURE_H__

#include "eka_macros.h"

#include "TestCaseConfig.h"

class TestEfcFixture : public ::testing::Test {
protected:
  void SetUp() override;
  void TearDown() override;

  void createTestCase(TestScenarioConfig &testScenario);

  void runTest(const TestCaseConfig &tc);

  EfcArmVer sendPktToAll(const void *pkt, size_t pktLen,
                         EfcArmVer armVer, TestCase *t);

  void commonInit(TestCase *t);

  virtual void configure(TestCase *t){};
  virtual void run(TestCase *t){};

  void testPrologue();
  void testEpilogue();

protected:
  static const int MaxTcpTestSessions = 16;
  static const int MaxUdpTestSessions = 64;
  static const uint16_t numTcpSess = 1;

  char testName_[128] = {};
  char testLogFileName_[256] = {};

  EkaDev *dev_;

public:
  struct FireReport {
    uint8_t *buf;
    size_t len;
  };

  std::vector<FireReport *> fireReports = {};
  std::atomic<int> nReceivedFireReports = 0;
  int nExpectedFireReports = 0;
};

/* --------------------------------------------- */
class TestCmeFc : public TestEfcFixture {
protected:
  void configure(TestCase *t) override;
  void run(TestCase *t) override;
};

/* --------------------------------------------- */
class TestP4 : public TestEfcFixture {
protected:
  void configure(TestCase *t) override;
  void run(TestCase *t) override;
};
#endif
