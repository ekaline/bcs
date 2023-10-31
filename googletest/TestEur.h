#ifndef __TEST_EUR_H__
#define __TEST_EUR_H__

#include "EkaBc.h"
#include "TestEfcFixture.h"

struct DummyIlinkMsg {
  uint8_t preamble[29];
  uint32_t appSeq;
  uint8_t trailing[];
} __attribute__((packed));

struct TestEurMd {
  const uint8_t *preloadedPkt;
  size_t pktLen;
  uint32_t appSeq;
  bool expectedFire;
};

class TestEur : public TestEfcFixture {
protected:
  void generateMdDataPkts(const void *t) override;
  void sendData() override;

protected:
  std::vector<TestEurMd> insertedMd_ = {};
};

/* --------------------------------------------- */

#endif
