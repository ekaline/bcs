#include "EkaMoexProdPair.h"
#include "eka_hw_conf.h"

#include "EkaDev.h"
#include "EkaEpmAction.h"

extern EkaDev *g_ekaDev;

using namespace EkaBcs;

EkaMoexProdPair::EkaMoexProdPair(
    PairIdx idx, const ProdPairInitParams *params) {
  idx_ = idx;

  secBase_ = params->secBase;
  secQuote_ = params->secQuote;

  fireBaseNewIdx_ = params->fireBaseNewIdx;
  fireQuoteReplaceIdx_ = params->fireQuoteReplaceIdx;

  EKA_LOG("Created Pair[%d]: {\'%s\', \'%s\'}", idx,
          secBase_.getName().c_str(),
          secQuote_.getName().c_str());
}
/* --------------------------------------------------- */

OpResult EkaMoexProdPair::downloadParams() {

  struct HwSignedStruct {
    uint8_t sign;
    int64_t value; // TBD try unsigned
  } __attribute__((packed));

  struct HwSingpleProdStruct {
    char name[12];
  } __attribute__((packed));

  struct HwSingplePairStruct {
    HwSignedStruct time_tolerance;
    HwSignedStruct slippage;
    HwSignedStruct tolerance;
    HwSignedStruct fix_spread;
    HwSignedStruct markup_sell;
    HwSignedStruct markup_buy;
    HwSignedStruct quote_size;
    uint64_t token;
    uint16_t strategy_index_base_new;
    uint16_t strategy_index_quot_replace;
    uint8_t strategy_region;
    HwSingpleProdStruct quote;
    HwSingpleProdStruct base;
  } __attribute__((packed, aligned(8)));

  // TBD multiple pairs
  HwSingplePairStruct
      __attribute__((aligned(sizeof(uint64_t)))) hw = {};
  // HwSingplePairStruct hw = {};

  // secid
  secBase_.getSwapName(hw.base.name);
  secQuote_.getSwapName(hw.quote.name);

  hw.strategy_region = EkaEpmRegion::Regions::Efc;
  hw.strategy_index_quot_replace = fireQuoteReplaceIdx_;
  hw.strategy_index_base_new = fireBaseNewIdx_;
  hw.token = 0x1122334455667788; // TBD
  hw.quote_size.value = quoteSize_;
  hw.markup_buy.value = markupBuy_;
  hw.markup_sell.value = markupSell_;
  hw.tolerance.value = tolerance_;
  hw.slippage.value = slippage_;
  hw.time_tolerance.value = timeTolerance_;

  if (fixSpread_ > 0) {
    hw.fix_spread.value = fixSpread_;
    hw.fix_spread.sign  = 0x0; //plus
  } else {
    hw.fix_spread.value = -1*fixSpread_;
    hw.fix_spread.sign  = 0x1; //minus
  }

  EKA_LOG("Pair[%d]: "
          "markupBuy=%jd, "
          "markupSell=%jd, "
          "fixSpread=%jd (%d,%jd), "
          "tolerance=%jd, "
          "slippage=%jd, "
          "quoteSize=%jd, "
          "timeTolerance=%jd, ",
          idx_, markupBuy_, markupSell_,
	  fixSpread_, hw.fix_spread.sign, hw.fix_spread.value, 
          tolerance_, slippage_, quoteSize_,
          timeTolerance_);
  
  const uint32_t BaseDstAddr = 0x86000;

  // TBD multiple pairs
  copyBuf2Hw(g_ekaDev, BaseDstAddr, (uint64_t *)(&hw),
             sizeof(hw));

  return OPRESULT__OK;
}
/* ---------------------------------------------------
 */

OpResult EkaMoexProdPair::setDynamicParams(
    const ProdPairDynamicParams *params) {

  markupBuy_ = params->markupBuy;
  markupSell_ = params->markupSell;
  fixSpread_ = params->fixSpread;
  tolerance_ = params->tolerance;
  slippage_ = params->slippage;
  quoteSize_ = params->quoteSize;
  timeTolerance_ = params->timeTolerance;

  downloadParams();

  return OPRESULT__OK;
}
/* --------------------------------------------------- */
