#include <gtest/gtest.h>
#include <iostream>

#include "TestEur.h"
/* --------------------------------------------- */
#if 1
static const TestTcpSess testDefaultTcpSess[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111},
    {2, "120.0.0.0", "10.0.0.12", 22222},
    {3, "130.0.0.0", "10.0.0.13", 22333}};

static const TestTcpSess tcp1_s[] = {
    {1, "110.0.0.0", "10.0.0.11", 22111}};
static const TestTcpSess tcp01_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000},
    {1, "110.0.0.0", "10.0.0.11", 22111}};

static const TestTcpParams tcp1 = {tcp1_s,
                                   std::size(tcp1_s)};
static const TestTcpParams tcp01 = {tcp01_s,
                                    std::size(tcp01_s)};

static const TestTcpSess tcp3_s[] = {
    {3, "130.0.0.0", "10.0.0.13", 22333}};
static const TestTcpParams tcp3 = {tcp3_s,
                                   std::size(tcp3_s)};

static const EkaBcMcGroupParams mc1[] = {
    {1, "224.0.0.1", 30301}};
static const EkaBcMcGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const EkaBcUdpMcParams core1_1mc = {mc1,
                                           std::size(mc1)};
static const EkaBcUdpMcParams two_cores_1mc = {
    mc01, std::size(mc01)};
#endif
/* --------------------------------------------- */

/* --------------------------------------------- */
#if 1
static const TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};
static const EkaBcMcGroupParams mc0[] = {0, "224.0.0.0",
                                         30300};
static const EkaBcUdpMcParams core0_1mc = {mc0,
                                           std::size(mc0)};
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestEur, Eur_basic) {
  EkaBCOpResult rc;

  /////////////// General
  uint8_t activeJumpAtBestSet = 2;
  uint8_t activeJumpBetterBestSet = 4;
  EkaBcEurMdSize sizeMultiplier = 10000;
  uint8_t AggressorSide = ENUM_AGGRESSOR_SIDE_BUY;
  /////////////// General

  /////////////// TOB
  EkaBcEurMdSize rawTobBidSize = 5;
  EkaBcEurPrice tobBidPrice = 50000;

  EkaBcEurMdSize rawTobAskSize = 8;
  EkaBcEurPrice tobAskPrice = 50006;
  /////////////// TOB

  /////////////// Trade
  EkaBcEurMdSize rawTradeSize = 2;
  EkaBcEurPrice tradePrice = 60000;
  /////////////// Trade

  EkaBcEurMdSize tobBidSize =
      rawTobBidSize * sizeMultiplier;
  EkaBcEurMdSize tobAskSize =
      rawTobAskSize * sizeMultiplier;
  EkaBcEurMdSize tradeSize = rawTradeSize * sizeMultiplier;

  mcParams_ = &core0_1mc;
  tcpParams_ = &tcp3;

  prodList_[0] = static_cast<EkaBcEurSecId>(123);
  prodList_[1] = static_cast<EkaBcEurSecId>(456);
  prodList_[2] = static_cast<EkaBcEurSecId>(789);
  nProds_ = 3;
  // Inititializes everything
  initEur();

  auto h = ekaBcGetSecHandle(dev_, prodList_[0]);
  ASSERT_NE(h, -1);

  auto eurHwAction = ekaBcAllocateNewAction(
      dev_, EkaBcActionType::EurFire);
  ASSERT_NE(eurHwAction, -1);

  rc = ekaBcSetActionTcpSock(
      dev_, eurHwAction, tcpCtx_->tcpSess_[0]->excSock_);
  ASSERT_EQ(rc, EKABC_OPRESULT__OK);

  EkaBcEurProductInitParams prodParams = {};
  prodParams.fireActionIdx = eurHwAction;
  prodParams.secId = prodList_[0];
  prodParams.step = 1;
  prodParams.isBook = 1;
  prodParams.maxBidSize = sizeMultiplier;     // TBD
  prodParams.maxAskSize = sizeMultiplier * 2; // TBD
  prodParams.maxBookSpread = tobAskPrice - tobBidPrice;
  prodParams.midPoint =
      (tobAskPrice - tobBidPrice) / 2 + tobBidPrice;

  rc = ekaBcInitEurProd(dev_, h, &prodParams);
  ASSERT_EQ(rc, EKABC_OPRESULT__OK);

  const char EurFireMsg[] = "EurFireMsg with Dummy "
                            "payload";
  rc = ekaBcSetActionPayload(dev_, eurHwAction, &EurFireMsg,
                             strlen(EurFireMsg));
  ASSERT_EQ(rc, EKABC_OPRESULT__OK);

  EkaBcEurJumpParams jumpParams = {};
  jumpParams.atBest[activeJumpAtBestSet].max_tob_size =
      (tobBidSize > tobAskSize) ? tobBidSize : tobAskSize;
  jumpParams.atBest[activeJumpAtBestSet].min_tob_size =
      (tobBidSize > tobAskSize) ? tobAskSize : tobBidSize;
  jumpParams.atBest[activeJumpAtBestSet].max_post_size =
      tobBidSize - tradeSize; // TBD assume BUY ticker
  jumpParams.atBest[activeJumpAtBestSet].min_ticker_size =
      tradeSize;
  jumpParams.atBest[activeJumpAtBestSet].min_price_delta =
      tradePrice - tobBidPrice; // TBD assume BUY ticker
  jumpParams.atBest[activeJumpAtBestSet].buy_size =
      sizeMultiplier;
  jumpParams.atBest[activeJumpAtBestSet].sell_size =
      sizeMultiplier * 2;
  jumpParams.atBest[activeJumpAtBestSet].strat_en = 0;
  jumpParams.atBest[activeJumpAtBestSet].boc = 1;

  rc = ekaBcEurSetJumpParams(dev_, h, &jumpParams);
  ASSERT_EQ(rc, EKABC_OPRESULT__OK);

  EkaBcArmVer armVer = 0;

  rc = ekaBcArmEur(dev_, h, true /* armBid */,
                   true /* armAsk */, armVer);
  ASSERT_EQ(rc, EKABC_OPRESULT__OK);

  EkaBcRunCtx runCtx = {.onReportCb = getFireReport,
                        .cbCtx = this};
  ekaBcEurRun(dev_, &runCtx);
  sleep(1);

  EobiAddOrderPkt addOrderBidPkt = {};
  addOrderBidPkt.pktHdr.MessageHeader.TemplateID =
      TID_PACKETHEADER;
  addOrderBidPkt.pktHdr.MessageHeader.BodyLen =
      sizeof(addOrderBidPkt.pktHdr);
  addOrderBidPkt.pktHdr.TransactTime = 0; // TBD
  addOrderBidPkt.orderAddMsg.MessageHeader.BodyLen =
      sizeof(addOrderBidPkt.orderAddMsg);
  addOrderBidPkt.orderAddMsg.MessageHeader.TemplateID =
      TID_ORDER_ADD;
  addOrderBidPkt.orderAddMsg.RequestTime = 0; // TBD
  addOrderBidPkt.orderAddMsg.SecurityID = prodList_[0];
  addOrderBidPkt.orderAddMsg.OrderDetails.DisplayQty =
      tobBidSize;
  addOrderBidPkt.orderAddMsg.OrderDetails.Side =
      ENUM_SIDE_BUY;
  addOrderBidPkt.orderAddMsg.OrderDetails.Price =
      tobBidPrice;

  EobiAddOrderPkt addOrderAskPkt = {};
  addOrderAskPkt.pktHdr.MessageHeader.TemplateID =
      TID_PACKETHEADER;
  addOrderAskPkt.pktHdr.MessageHeader.BodyLen =
      sizeof(addOrderAskPkt.pktHdr);
  addOrderAskPkt.pktHdr.TransactTime = 0; // TBD
  addOrderAskPkt.orderAddMsg.MessageHeader.BodyLen =
      sizeof(addOrderAskPkt.orderAddMsg);
  addOrderAskPkt.orderAddMsg.MessageHeader.TemplateID =
      TID_ORDER_ADD;
  addOrderAskPkt.orderAddMsg.RequestTime = 0; // TBD
  addOrderAskPkt.orderAddMsg.SecurityID = prodList_[0];
  addOrderAskPkt.orderAddMsg.OrderDetails.DisplayQty =
      tobAskSize;
  addOrderAskPkt.orderAddMsg.OrderDetails.Side =
      ENUM_SIDE_SELL;
  addOrderAskPkt.orderAddMsg.OrderDetails.Price =
      tobAskPrice;

  EobiExecSumPkt execSumPkt = {};
  execSumPkt.pktHdr.MessageHeader.TemplateID =
      TID_PACKETHEADER;
  execSumPkt.pktHdr.MessageHeader.BodyLen =
      sizeof(addOrderAskPkt.pktHdr);
  execSumPkt.pktHdr.TransactTime = 0; // TBD
  execSumPkt.execSumMsg.MessageHeader.BodyLen =
      sizeof(execSumPkt.execSumMsg);
  execSumPkt.execSumMsg.MessageHeader.TemplateID =
      TID_EXECUTION_SUMMARY;
  execSumPkt.execSumMsg.SecurityID = prodList_[0];
  execSumPkt.execSumMsg.RequestTime = 0; // TBD
  execSumPkt.execSumMsg.LastQty = tradeSize;
  execSumPkt.execSumMsg.AggressorSide = AggressorSide;
  execSumPkt.execSumMsg.LastPx = tradePrice;

  sendPktToAll(&addOrderBidPkt, sizeof(addOrderBidPkt),
               false);
  sendPktToAll(&addOrderAskPkt, sizeof(addOrderAskPkt),
               false);
  sendPktToAll(&execSumPkt, sizeof(execSumPkt), true);
  sleep(1);

#ifndef _VERILOG_SIM
  ekaBcCloseDev(dev_);
#endif
}
#endif
/* --------------------------------------------- */

/* --------------------------------------------- */
int main(int argc, char **argv) {
  std::cout << "Main of " << argv[0] << "\n";
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
