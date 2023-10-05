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
};

class TestCmeFc : public TestEfcFixture {
protected:
  void configureStrat(const TestCaseConfig *t) override;
  void sendData(const void *mdInjectParams) override;

  void
  checkAlgoCorrectness(const TestCaseConfig *tc) override;
};

/* --------------------------------------------- */

#endif
