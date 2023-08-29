#ifndef __TEST_P4_H__
#define __TEST_P4_H__

#include "EkaFhBatsParser.h"
#include "TestEfcFixture.h"

using namespace Bats;

/* --------------------------------------------- */

struct TestP4CboeSec {
  const char *id;
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t size;
};

struct TestP4SecConf {
  const TestP4CboeSec *sec;
  size_t nSec;
};

struct TestP4Md {
  const char *secId;
  SideT side;
  FixedPrice price;
  uint16_t size;
};

struct CboePitchAddOrderShort {
  sequenced_unit_header hdr;
  add_order_short msg;
} __attribute__((packed));

struct CboePitchAddOrderLong {
  sequenced_unit_header hdr;
  add_order_long msg;
} __attribute__((packed));

struct CboePitchAddOrderExpanded {
  sequenced_unit_header hdr;
  add_order_expanded msg;
} __attribute__((packed));

static inline char cboeOppositeSide(SideT side) {
  return side == SideT::BID ? '2' : '1';
}
static inline char cboeSide(SideT side) {
  return side == SideT::BID ? 'B' : 'S';
}

/* --------------------------------------------- */

class TestP4 : public TestEfcFixture {
protected:
  void configureStrat(const TestCaseConfig *t) override;
  void sendData(const void *mdInjectParams) override;

private:
  void createSecList(const TestP4SecConf *secConf);
  void getSecCtx(const TestP4CboeSec *secCtx, SecCtx *dst);

  size_t createOrderExpanded(char *dst, const char *id,
                             SideT side, uint64_t price,
                             uint32_t size);
  /* --------------------------------------------- */

private:
  static const size_t MaxTestSecurities = 16;
  size_t nSec_ = 0;
  uint64_t secList_[MaxTestSecurities] = {};

  uint32_t sequence_ = 100;

protected:
  struct AddOrderParams {
    const char secId[8];
    SideT side;
    uint64_t price;
    uint32_t size;
  };
};

/* --------------------------------------------- */

#endif
