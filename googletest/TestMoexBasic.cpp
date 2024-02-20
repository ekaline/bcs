#include <gtest/gtest.h>
#include <iostream>

#include "EkaBcs.h"
#include "TestEfcFixture.h"

#include "EkaMoexNetworkConfig.h"

void getExampleFireReport(const void *p, size_t len,
                          void *ctx);

class TestMoex : public TestEfcFixture {
protected:
  TestMoex() {}
  virtual ~TestMoex() {}
};

TEST_F(TestMoex, Basic) {
  mcParams_ = &core2_1mc;
  tcpParams_ = &tcp0;

  auto mcCon = new TestUdpConn(mcParams_->groups);
  ASSERT_NE(mcCon, nullptr);

  /* -------------------------------------------- */

  OpResult rc;

  EkaBcEurMdSize rawTradeSize = 2;
  EkaBcEurPrice tradePrice =
      60000; // to depth 2 (total size = 8+8)
  /////////////// Trade

  prodList_[0] = MoexSecurityId("EURRUB_TMS  ");
  prodList_[1] = MoexSecurityId("USDCNY_TOD  ");
  nProds_ = 2;
  // Inititializes everything
  initMoex();

  ProdPairInitParams prodPairInitParams;
  prodPairInitParams.secA = prodList_[0];
  prodPairInitParams.secB = prodList_[1];

  auto ret = initProdPair(0, &prodPairInitParams);

  EkaBcsRunCtx runCtx = {.onReportCb = getExampleFireReport,
                         .cbCtx = this};
  EkaBcsMoexRun(&runCtx);

  sleep(100);
#ifndef _VERILOG_SIM
  closeDev();
#endif
}
