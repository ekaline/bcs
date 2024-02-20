#include <gtest/gtest.h>
#include <iostream>

#include "EkaBcs.h"
#include "TestEfcFixture.h"

#include "EkaMoexTestStructs.h"

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

/* -------------------------------------------- */
void printPayloadReport(uint8_t *p) {
  uint8_t *b = (uint8_t *)p;
  auto firePktHdr{reinterpret_cast<EfcBcReportHdr *>(b)};

  auto length = firePktHdr->size;

  printf("Length = %ju, Type = %s \n", length,
         EkaBcReportType2STR(firePktHdr->type));

  b += sizeof(EfcBcReportHdr);
  b += 54; // skip l2-l3 headers

  hexDump("Data (without headers)", b, length - 54);

  NewOrderSingleShortRequestT *o =
      (NewOrderSingleShortRequestT *)b;
  printf("Price = %ju\n", (uint64_t)o->Price);
  printf("OrderQty = %ju\n", (uint64_t)o->OrderQty);
}

/* -------------------------------------------- */
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
