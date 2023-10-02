#ifndef __TEST_CME_FC_H__
#define __TEST_CME_FC_H__

#include "TestEfcFixture.h"

struct DummyIlinkMsg {
  uint8_t preamble[29];
  uint32_t appSeq;
  uint8_t trailing[];
} __attribute__((packed));

struct TestCmeFcMd {
  const uint8_t *preloadedPkt;
  size_t pktLen;
  uint32_t appSeq;
  bool expectedFire;
};

class TestCmeFc : public TestEfcFixture {
protected:
  void configureStrat(const TestCaseConfig *t) override;
  void generateMdDataPkts(const void *t) override;
  void sendData() override;
  void
  checkAlgoCorrectness(const TestCaseConfig *tc) override;

protected:
  std::vector<TestCmeFcMd> insertedMd_ = {};
};

/* --------------------------------------------- */

#endif
