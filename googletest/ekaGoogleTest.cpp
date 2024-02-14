#include <gtest/gtest.h>
#include <iostream>

#include "TestEur.h"
// Structure: MessageHeaderIn

using namespace EkaBcs;

typedef struct {
  uint32_t BodyLen;
  uint16_t TemplateID;
  char NetworkMsgID[8];
  char Pad2[2];
} MessageHeaderInCompT;

// Structure: RequestHeader

typedef struct {
  uint32_t MsgSeqNum;
  uint32_t SenderSubID;
} RequestHeaderCompT;

typedef struct {
  MessageHeaderInCompT MessageHeaderIn;
  RequestHeaderCompT RequestHeader;
  int64_t Price;
  int64_t OrderQty;
  uint64_t ClOrdID;
  uint64_t PartyIDClientID;
  uint64_t PartyIdInvestmentDecisionMaker;
  uint64_t ExecutingTrader;
  uint32_t SimpleSecurityID;
  uint32_t MatchInstCrossID;
  uint16_t EnrichmentRuleID;
  uint8_t Side;
  uint8_t ApplSeqIndicator;
  uint8_t PriceValidityCheckType;
  uint8_t ValueCheckTypeValue;
  uint8_t OrderAttributeLiquidityProvision;
  uint8_t TimeInForce;
  uint8_t ExecInst;
  uint8_t TradingCapacity;
  uint8_t OrderOrigination;
  uint8_t PartyIdInvestmentDecisionMakerQualifier;
  uint8_t ExecutingTraderQualifier;
  char ComplianceText[20];
  char Pad7[7];
} NewOrderSingleShortRequestT;

/* --------------------------------------------- */
void printFireReport(uint8_t *p) {
  uint8_t *b = (uint8_t *)p;

  auto reportHdr = reinterpret_cast<EkaBcReportHdr *>(b);
  printf("reportHdr->idx = %d\n", reportHdr->idx);

  b += sizeof(*reportHdr);
  auto hwReport{
      reinterpret_cast<const EkaBcFireReport *>(b)};
  printf("\n---- Action Params ----\n");
  printf("currentActionIdx = %ju\n",
         (uint64_t)hwReport->currentActionIdx);
  printf("firstActionIdx = %ju\n",
         (uint64_t)hwReport->firstActionIdx);
  printf("\n---- Ticker Params ----\n");
  printf("localOrderCntr = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.ticker.localOrderCntr);
  printf(
      "appSeqNum = %ju\n",
      (uint64_t)hwReport->eurFireReport.ticker.appSeqNum);
  printf("transactTime = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.ticker.transactTime);
  printf(
      "requestTime = %ju\n",
      (uint64_t)hwReport->eurFireReport.ticker.requestTime);
  printf("aggressorSide = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.ticker.aggressorSide);
  printf("lastQty = %ju\n",
         (uint64_t)hwReport->eurFireReport.ticker.lastQty);
  printf(
      "lastPrice = %ju\n",
      (uint64_t)hwReport->eurFireReport.ticker.lastPrice);
  printf(
      "securityId = %ju\n",
      (uint64_t)hwReport->eurFireReport.ticker.securityId);
  printf("\n---- Product Params ----\n");
  printf("securityId = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.prodConf.securityId);
  printf("productIdx = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.prodConf.productIdx);
  printf(
      "actionIdx = %ju\n",
      (uint64_t)hwReport->eurFireReport.prodConf.actionIdx);
  printf(
      "askSize = %ju\n",
      (uint64_t)hwReport->eurFireReport.prodConf.askSize);
  printf(
      "bidSize = %ju\n",
      (uint64_t)hwReport->eurFireReport.prodConf.bidSize);
  printf("maxBookSpread = %ju\n",
         (uint64_t)hwReport->eurFireReport.prodConf
             .maxBookSpread);
  printf("\n---- Pass Params ----\n");
  printf("stratID = %ju\n",
         (uint64_t)hwReport->eurFireReport.controllerState
             .stratID);
  printf("stratSubID = %ju\n",
         (uint64_t)hwReport->eurFireReport.controllerState
             .stratSubID);
  switch (hwReport->eurFireReport.controllerState.stratID) {
  case EkaBcStratType::JUMP_ATBEST:
  case EkaBcStratType::JUMP_BETTERBEST:
    printf("\n---- Strategy Params Jump----\n");
    printf("bitParams = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.bitParams);
    printf("askSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.askSize);
    printf("bidSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.bidSize);
    printf("minTobSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.minTobSize);
    printf("maxTobSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.maxTobSize);
    printf("maxPostSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.maxPostSize);
    printf("minTickerSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.minTickerSize);
    printf("minPriceDelta = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .jumpConf.minPriceDelta);
    break;
  case EkaBcStratType::RJUMP_BETTERBEST:
  case EkaBcStratType::RJUMP_ATBEST:
    printf("\n---- Strategy Params RJump----\n");
    printf("bitParams = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.bitParams);
    printf("askSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.askSize);
    printf("bidSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.bidSize);
    printf("minSpread = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.minSpread);
    printf("maxOppositTobSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.maxOppositTobSize);
    printf("minTobSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.minTobSize);
    printf("maxTobSize = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.maxTobSize);
    printf("timeDeltaUs = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.timeDeltaUs);
    printf("tickerSizeLots = %ju\n",
           (uint64_t)hwReport->eurFireReport.stratConf
               .refJumpConf.tickerSizeLots);
    break;
  }
  printf("\n---- TOB State Bid----\n");
  printf("lastTransactTime = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.bid
             .lastTransactTime);
  printf("eiBetterPrice = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.bid
             .eiBetterPrice);
  printf("eiPrice = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.tobState.bid.eiPrice);
  printf(
      "price = %ju\n",
      (uint64_t)hwReport->eurFireReport.tobState.bid.price);
  printf("normPrice = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.bid
             .normPrice);
  printf(
      "size = %ju\n",
      (uint64_t)hwReport->eurFireReport.tobState.bid.size);
  printf("msgSeqNum = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.bid
             .msgSeqNum);
  printf("\n---- TOB State Ask----\n");
  printf("lastTransactTime = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.ask
             .lastTransactTime);
  printf("eiBetterPrice = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.ask
             .eiBetterPrice);
  printf("eiPrice = %ju\n",
         (uint64_t)
             hwReport->eurFireReport.tobState.ask.eiPrice);
  printf(
      "price = %ju\n",
      (uint64_t)hwReport->eurFireReport.tobState.ask.price);
  printf("normPrice = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.ask
             .normPrice);
  printf(
      "size = %ju\n",
      (uint64_t)hwReport->eurFireReport.tobState.ask.size);
  printf("msgSeqNum = %ju\n",
         (uint64_t)hwReport->eurFireReport.tobState.ask
             .msgSeqNum);
}

void printPayloadReport(uint8_t *p) {
  uint8_t *b = (uint8_t *)p;
  auto firePktHdr{reinterpret_cast<EfcBcReportHdr *>(b)};

  auto length = firePktHdr->size;

  printf("Length = %d, Type = %s \n", length,
         EkaBcReportType2STR(firePktHdr->type));

  b += sizeof(EfcBcReportHdr);
  b += 54; // skip l2-l3 headers

  hexDump("Data (without headers)", b, length - 54);

  NewOrderSingleShortRequestT *o =
      (NewOrderSingleShortRequestT *)b;
  printf("Price = %ju\n", (uint64_t)o->Price);
  printf("OrderQty = %ju\n", (uint64_t)o->OrderQty);
}

void getExampleFireReport(const void *p, size_t len,
                          void *ctx) {
  if (1) {
    uint8_t *b = (uint8_t *)p;
    auto containerHdr{
        reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};

    switch (containerHdr->eventType) {
    case EkaBcEventType::FireEvent:
      printf("Fire with %d reports...\n",
             containerHdr->nReports);
      // skip container header
      b += sizeof(*containerHdr);
      // print fire report
      printFireReport(b);
      // skip report hdr (of fire) and fire report
      b += sizeof(EkaBcReportHdr);
      b += sizeof(EkaBcFireReport);
      // print payload
      printPayloadReport(b);
      break;
    case EkaBcEventType::ExceptionEvent:
      //    printf ("Status...\n");
      break;
    case EkaBcEventType::EpmEvent:
      printf("SW Fire with %d reports...\n",
             containerHdr->nReports);
      // skip container header
      b += sizeof(*containerHdr);
      // skip report hdr (of fire) and swfire report
      b += sizeof(EkaBcReportHdr);
      b += sizeof(EkaBcSwReport);
      // print payload
      printPayloadReport(b);
      break;

    default:
      break;
    }
  }
}

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

static const McGroupParams mc01[] = {
    {0, "224.0.0.0", 30300}, {1, "224.0.0.1", 30301}};

static const UdpMcParams two_cores_1mc = {mc01,
                                          std::size(mc01)};

static const TestTcpSess tcp0_s[] = {
    {0, "100.0.0.0", "10.0.0.10", 22000}};
static const TestTcpParams tcp0 = {tcp0_s,
                                   std::size(tcp0_s)};

/* --------------------------------------------- */
static const McGroupParams mc0[] = {0, "224.0.10.100",
                                    30310};
static const UdpMcParams core0_1mc = {mc0, std::size(mc0)};
static const McGroupParams mc1[] = {1, "224.1.11.101",
                                    30311};
static const UdpMcParams core1_1mc = {mc1, std::size(mc1)};
// static const McGroupParams mc2[] = {2, "224.2.12.102",
//                                     30312}
static const McGroupParams mc2[] = {2, "239.195.1.16",
                                     16016};
static const UdpMcParams core2_1mc = {mc2, std::size(mc2)};
static const McGroupParams mc3[] = {3, "224.3.13.103",
                                    30313};
static const UdpMcParams core3_1mc = {mc3, std::size(mc3)};
#endif
/* --------------------------------------------- */
#if 1
TEST_F(TestEur, Eur_basic) {
  mcParams_ = &core2_1mc;
  tcpParams_ = &tcp0;

  auto mcCon = new TestUdpConn(mcParams_->groups);
  ASSERT_NE(mcCon, nullptr);

  /* -------------------------------------------- */

  OpResult rc;

  /////////////// General
  uint8_t activeJumpAtBestSet = 3;
  uint8_t activeJumpBetterBestSet = 4;
  uint8_t activeRJumpAtBestSet = 1;
  uint8_t activeRJumpBetterBestSet = 2;
  EkaBcEurMdSize sizeMultiplier = 10000;
  uint8_t AggressorSide = ENUM_AGGRESSOR_SIDE_BUY;
  /////////////// General

  /////////////// TOB
  EkaBcEurMdSize rawTobBidSize = 5;
  EkaBcEurPrice tobBidPrice = 59998;

  EkaBcEurMdSize rawTobAskSize = 8;
  EkaBcEurPrice tobAskPrice = 59999;
  /////////////// TOB

  /////////////// Trade
  EkaBcEurMdSize rawTradeSize = 2;
  EkaBcEurPrice tradePrice =
      60000; // to depth 2 (total size = 8+8)
  /////////////// Trade

  EkaBcEurMdSize tobBidSize =
      rawTobBidSize * sizeMultiplier;
  EkaBcEurMdSize tobAskSize =
      rawTobAskSize * sizeMultiplier;
  EkaBcEurMdSize tradeSize = rawTradeSize * sizeMultiplier;

  prodList_[0] = static_cast<EkaBcEurSecId>(1234567890);
  prodList_[1] = static_cast<EkaBcEurSecId>(456);
  prodList_[2] = static_cast<EkaBcEurSecId>(789);
  nProds_ = 3;
  // Inititializes everything
  initMoex();

  auto ref_index = 1;
  auto main_index = 0;

  auto h = ekaBcsGetSecHandle( prodList_[main_index]);
  auto r = ekaBcsGetSecHandle( prodList_[ref_index]);
  ASSERT_NE(h, -1);
  ASSERT_NE(r, -1);

#ifdef _TBD_
  
  //  printf ("Main Hanlde = %d, Reference Handle =
  //  %d",h,r);
  auto eurHwAction = ekaBcAllocateNewAction(
       EkaBcActionType::MoexFire);
  ASSERT_NE(eurHwAction, -1);


  rc = ekaBcSetActionTcpSock(
       eurHwAction, tcpCtx_->tcpSess_[0]->excSock_);
  ASSERT_EQ(rc, OPRESULT__OK);

  rc = ekaBcSetActionTcpSock(
       eurHwAction, tcpCtx_->tcpSess_[0]->excSock_);
  ASSERT_EQ(rc, OPRESULT__OK);

  // sw action
  auto eurSwAction = ekaBcAllocateNewAction(
       EkaBcActionType::MoexSwSend);
  ASSERT_NE(eurSwAction, -1);

  rc = ekaBcSetActionTcpSock(
       eurSwAction, tcpCtx_->tcpSess_[0]->excSock_);
  ASSERT_EQ(rc, OPRESULT__OK);

  const char EurSwFireMsg[] = "EurSwMessage wit 140"
                              "Byte payload XXXXXXX"
                              "12345678901234567890"
                              "12345678901234567890"
                              "12345678901234567890"
                              "12345678901234567890"
                              "12345678901234567890";

  //  ekaBcAppSend(dev_, eurSwAction, &EurSwFireMsg ,
  //  strlen(EurSwFireMsg)); ekaBcAppSend(dev_, eurSwAction,
  //  &EurSwFireMsg , strlen(EurSwFireMsg));
  //  ekaBcAppSend(dev_, eurSwAction, &EurSwFireMsg ,
  //  strlen(EurSwFireMsg));
  // sw action

  EkaBcEurProductInitParams prodParams = {};
  prodParams.fireActionIdx = eurHwAction;
  prodParams.secId = prodList_[main_index];
  prodParams.step = 1;
  prodParams.isBook = 1;
  prodParams.midPoint =
      (tobAskPrice - tobBidPrice) / 2 + tobBidPrice;

  rc = ekaBcInitEurProd( h, &prodParams);

  prodParams.secId = prodList_[ref_index];

  rc = ekaBcInitEurProd( r, &prodParams); // reference

  ASSERT_EQ(rc, OPRESULT__OK);

  EkaBcProductDynamicParams prodDynamicParams = {};
  prodDynamicParams.maxBidSize = sizeMultiplier;     // TBD
  prodDynamicParams.maxAskSize = sizeMultiplier * 2; // TBD
  prodDynamicParams.maxBookSpread =
      tobAskPrice - tobBidPrice + 1;

  rc = ekaBcSetEurProdDynamicParams( h,
                                    &prodDynamicParams);

  prodDynamicParams.maxBidSize = sizeMultiplier * 3; // TBD
  prodDynamicParams.maxAskSize = sizeMultiplier * 4; // TBD
  prodDynamicParams.maxBookSpread =
      tobAskPrice - tobBidPrice + 1;

  rc = ekaBcSetEurProdDynamicParams( r,
                                    &prodDynamicParams);
  ASSERT_EQ(rc, OPRESULT__OK);

  const char EurFireMsg[] = "EurFireMsg with 120 "
                            "Byte payload XXXXXXX"
                            "12345678901234567890"
                            "12345678901234567890"
                            "12345678901234567890"
                            "12345678901234567890";
  rc = ekaBcSetActionPayload( eurHwAction, &EurFireMsg,
                             strlen(EurFireMsg));
  ASSERT_EQ(rc, OPRESULT__OK);

  EkaBcEurJumpParams jumpParams = {};

  jumpParams.atBest[activeJumpAtBestSet].max_tob_size =
      (tobBidSize > tobAskSize) ? tobBidSize : tobAskSize;
  jumpParams.atBest[activeJumpAtBestSet].min_tob_size =
      (tobBidSize > tobAskSize) ? tobAskSize : tobBidSize;
  jumpParams.atBest[activeJumpAtBestSet].max_post_size =
      tobAskSize + tobAskSize -
      tradeSize; // TBD assume BUY ticker
  jumpParams.atBest[activeJumpAtBestSet].min_ticker_size =
      tradeSize;
  jumpParams.atBest[activeJumpAtBestSet].min_price_delta =
      tradePrice - tobBidPrice; // TBD assume BUY ticker
  jumpParams.atBest[activeJumpAtBestSet].buy_size =
      sizeMultiplier;
  jumpParams.atBest[activeJumpAtBestSet].sell_size =
      sizeMultiplier * 2;
  jumpParams.atBest[activeJumpAtBestSet].strat_en = 1;
  jumpParams.atBest[activeJumpAtBestSet].boc = 1;

  jumpParams.betterBest[activeJumpBetterBestSet]
      .max_tob_size =
      (tobBidSize > tobAskSize) ? tobBidSize : tobAskSize;
  jumpParams.betterBest[activeJumpBetterBestSet]
      .min_tob_size =
      (tobBidSize > tobAskSize) ? tobAskSize : tobBidSize;
  jumpParams.betterBest[activeJumpBetterBestSet]
      .max_post_size = tobAskSize + tobAskSize -
                       tradeSize; // TBD assume BUY ticker
  jumpParams.betterBest[activeJumpBetterBestSet]
      .min_ticker_size = tradeSize;
  jumpParams.betterBest[activeJumpBetterBestSet]
      .min_price_delta =
      tradePrice - tobBidPrice; // TBD assume BUY ticker
  jumpParams.betterBest[activeJumpBetterBestSet].buy_size =
      sizeMultiplier;
  jumpParams.betterBest[activeJumpBetterBestSet].sell_size =
      sizeMultiplier * 2;
  jumpParams.betterBest[activeJumpBetterBestSet].strat_en =
      1;
  jumpParams.betterBest[activeJumpBetterBestSet].boc = 0;

  //  rc = ekaBcEurSetJumpParams(dev_, h, &jumpParams);
  ASSERT_EQ(rc, OPRESULT__OK);

  EkaBcEurReferenceJumpParams rjumpParams = {};

  // rjumpParams.atBest[activeRJumpAtBestSet].max_tob_size =
  // (tobBidSize > tobAskSize) ? tobBidSize : tobAskSize;
  // rjumpParams.atBest[activeRJumpAtBestSet].min_tob_size =
  // (tobBidSize > tobAskSize) ? tobAskSize : tobBidSize;
  // rjumpParams.atBest[activeRJumpAtBestSet].max_opposit_tob_size
  // = sizeMultiplier; //TBD
  // rjumpParams.atBest[activeRJumpAtBestSet].time_delta_ns
  // = 0; //TBD
  // rjumpParams.atBest[activeRJumpAtBestSet].tickersize_lots
  // = sizeMultiplier; //TBD
  // rjumpParams.atBest[activeRJumpAtBestSet].buy_size =
  // sizeMultiplier;
  // rjumpParams.atBest[activeRJumpAtBestSet].sell_size =
  // sizeMultiplier * 2;
  // rjumpParams.atBest[activeRJumpAtBestSet].strat_en = 1;
  // rjumpParams.atBest[activeRJumpAtBestSet].boc = 1;
  // rjumpParams.atBest[activeRJumpAtBestSet].min_spread =
  // 1; //TBD

  rjumpParams.atBest[activeRJumpAtBestSet].max_tob_size =
      (EkaBcEurMdSize)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet].min_tob_size =
      (EkaBcEurMdSize)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet]
      .max_opposit_tob_size = (EkaBcEurMdSize)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet].time_delta_ns =
      (EkaBcEurTimeNs)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet].tickersize_lots =
      (EkaBcEurMdSize)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet].buy_size =
      (EkaBcEurFireSize)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet].sell_size =
      (EkaBcEurFireSize)(-1);
  rjumpParams.atBest[activeRJumpAtBestSet].strat_en = 1;
  rjumpParams.atBest[activeRJumpAtBestSet].boc = 1;
  rjumpParams.atBest[activeRJumpAtBestSet].min_spread =
      (uint8_t)(-1);

  // rjumpParams.betterBest[activeRJumpBetterBestSet].max_tob_size
  // =      (tobBidSize > tobAskSize) ? tobBidSize :
  // tobAskSize;
  // rjumpParams.betterBest[activeRJumpBetterBestSet].min_tob_size
  // =      (tobBidSize > tobAskSize) ? tobAskSize :
  // tobBidSize;
  // rjumpParams.betterBest[activeRJumpBetterBestSet].max_opposit_tob_size
  // = sizeMultiplier; //TBD
  // rjumpParams.betterBest[activeRJumpBetterBestSet].time_delta_ns
  // = 0; //TBD
  // rjumpParams.betterBest[activeRJumpBetterBestSet].tickersize_lots
  // = sizeMultiplier; //TBD
  // rjumpParams.betterBest[activeRJumpBetterBestSet].buy_size
  // =      sizeMultiplier;
  // rjumpParams.betterBest[activeRJumpBetterBestSet].sell_size
  // =      sizeMultiplier * 2;
  // rjumpParams.betterBest[activeRJumpBetterBestSet].strat_en
  // = 1;
  // rjumpParams.betterBest[activeRJumpBetterBestSet].boc =
  // 0;
  // rjumpParams.betterBest[activeRJumpBetterBestSet].min_spread
  // = 1; //TBD

  //  rc = ekaBcEurSetReferenceJumpParams(dev_, r, h,
  //                                      &rjumpParams);
  ASSERT_EQ(rc, OPRESULT__OK);

  EkaBcArmVer armVer = 0;

  rc = ekaBcArmEur( h, true /* armBid */,
                   true /* armAsk */, armVer++);
  ASSERT_EQ(rc, OPRESULT__OK);

#endif
  
  
  EkaBcsRunCtx runCtx = {.onReportCb = getExampleFireReport,
                        .cbCtx = this};
  EkaBcsMoexRun( &runCtx);


#ifdef _TBD_
    
  tcpCtx_->tcpSess_[0]->sendTestPkt();

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

  /*   sendPktToAll(&addOrderBidPkt, sizeof(addOrderBidPkt),
                 false); */
  mcCon->sendUdpPkt(&addOrderBidPkt,
                    sizeof(addOrderBidPkt));

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

  // tob
  /*   sendPktToAll(&addOrderAskPkt, sizeof(addOrderAskPkt),
                 false); */
  mcCon->sendUdpPkt(&addOrderAskPkt,
                    sizeof(addOrderAskPkt));

  addOrderAskPkt.orderAddMsg.OrderDetails.Price =
      tobAskPrice + 1;

  // depth 1
  /*   sendPktToAll(&addOrderAskPkt, sizeof(addOrderAskPkt),
                 false); */
  mcCon->sendUdpPkt(&addOrderAskPkt,
                    sizeof(addOrderAskPkt));

  // jump
  EobiExecSumPkt execSumJumpPkt = {};
  execSumJumpPkt.pktHdr.MessageHeader.TemplateID =
      TID_PACKETHEADER;
  execSumJumpPkt.pktHdr.MessageHeader.BodyLen =
      sizeof(addOrderAskPkt.pktHdr);
  execSumJumpPkt.pktHdr.TransactTime = 0; // TBD
  execSumJumpPkt.execSumMsg.MessageHeader.BodyLen =
      sizeof(execSumJumpPkt.execSumMsg);
  execSumJumpPkt.execSumMsg.MessageHeader.TemplateID =
      TID_EXECUTION_SUMMARY;
  execSumJumpPkt.execSumMsg.SecurityID =
      prodList_[main_index];
  execSumJumpPkt.execSumMsg.RequestTime = 0; // TBD
  execSumJumpPkt.execSumMsg.LastQty = tradeSize;
  execSumJumpPkt.execSumMsg.AggressorSide = AggressorSide;
  execSumJumpPkt.execSumMsg.LastPx = tradePrice;

  // rjump
  EobiExecSumPkt execSumRJumpPkt = {};
  execSumRJumpPkt.pktHdr.MessageHeader.TemplateID =
      TID_PACKETHEADER;
  execSumRJumpPkt.pktHdr.MessageHeader.BodyLen =
      sizeof(addOrderAskPkt.pktHdr);
  execSumRJumpPkt.pktHdr.TransactTime = 0; // TBD
  execSumRJumpPkt.execSumMsg.MessageHeader.BodyLen =
      sizeof(execSumRJumpPkt.execSumMsg);
  execSumRJumpPkt.execSumMsg.MessageHeader.TemplateID =
      TID_EXECUTION_SUMMARY;
  execSumRJumpPkt.execSumMsg.SecurityID =
      prodList_[ref_index];
  execSumRJumpPkt.execSumMsg.RequestTime = 0; // TBD
  execSumRJumpPkt.execSumMsg.LastQty = tradeSize;
  execSumRJumpPkt.execSumMsg.AggressorSide = AggressorSide;
  execSumRJumpPkt.execSumMsg.LastPx = tradePrice;

  /*   sendPktToAll(&execSumPkt, sizeof(execSumJumpPkt),
   * true);
   */
  ekaBcSetSessionCntr( tcpCtx_->tcpSess_[0]->excSock_,
                      5);

  //  mcCon->sendUdpPkt(&execSumJumpPkt,
  //  sizeof(execSumJumpPkt));
  mcCon->sendUdpPkt(&execSumRJumpPkt,
                    sizeof(execSumRJumpPkt));
  //  sleep(5);
  // rc = ekaBcArmEur(dev_, h, true /* armBid */,
  //                  true /* armAsk */, armVer++);
  // mcCon->sendUdpPkt(&execSumPkt, sizeof(execSumPkt));
  sleep(5);
  for (uint i = 0; i < 0; i++) {
    ekaBcAppSend( eurSwAction, &EurSwFireMsg,
                 strlen(EurSwFireMsg));
  }
  sleep(1);

#endif
  
#ifndef _VERILOG_SIM
  closeDev();
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
