#ifndef __TEST_EUR_H__
#define __TEST_EUR_H__

#include "EkaBcs.h"
#include "TestEfcFixture.h"

struct DummyIlinkMsg {
  uint8_t preamble[29];
  uint32_t appSeq;
  uint8_t trailing[];
} __attribute__((packed));

struct EobiAddOrderPkt {
  PacketHeaderT pktHdr;
  OrderAddT orderAddMsg;
};

struct EobiExecSumPkt {
  PacketHeaderT pktHdr;
  ExecutionSummaryT execSumMsg;
};

class TestEur : public TestEfcFixture {
protected:
  void generateMdDataPkts(const void *t) override;
  void sendData() override;

protected:
  std::vector<int> insertedMd_ = {};
};

/* --------------------------------------------- */

#endif
