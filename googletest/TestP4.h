#ifndef __TEST_P4_H__
#define __TEST_P4_H__

#include "EkaFhBatsParser.h"
#include "TestEfcFixture.h"

using namespace Bats;

/* --------------------------------------------- */

struct TestP4CboeSec {
  std::string strId;
  uint64_t binId;
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t size;
  bool valid;
  EfcSecCtxHandle handle;
};

enum class TestP4SecConfType : int {
  Predefined = 0,
  Random
};

struct TestP4SecConf {
  TestP4SecConfType type;
  double percentageValidSecs;
  const TestP4CboeSec *sec;
  size_t nSec;
};

struct TestP4Md {
  std::string secId;
  SideT side;
  FixedPrice price;
  uint16_t size;
  bool expectedFire;
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
  virtual void generateMdDataPkts(const void *t) override;

  virtual void sendData() override;

  virtual void
  checkAlgoCorrectness(const TestCaseConfig *tc);

  virtual void createSecList(const void *secConf);
  void setSecCtx(const TestP4CboeSec *secCtx, SecCtx *dst);

  virtual void initializeAllCtxs(const TestCaseConfig *tc);

  size_t createOrderExpanded(char *dst, const char *id,
                             SideT side, uint64_t price,
                             uint32_t size);

  uint64_t getBinSecId(std::string strId);
  /* --------------------------------------------- */

protected:
  static const size_t MaxTestSecurities = 1024 * 1024;
  size_t nSec_ = 0;
  uint64_t secList_[MaxTestSecurities] = {};

  uint32_t sequence_ = 100;

  std::vector<TestP4Md> insertedMd_ = {};

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
