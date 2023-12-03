#include "EkaBcEurProd.h"

#include "EkaEobiBook.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

#include "EkaEurStrategy.h"

EkaBcEurProd::EkaBcEurProd(
    EkaEurStrategy *strat, EkaBcSecHandle handle,
    const EkaBcEurProductInitParams *p) {
  strat_ = strat;

  handle_ = handle;

  secId_ = p->secId;
  maxBookSpread_ = p->maxBookSpread;
  midPoint_ = p->midPoint;
  priceDiv_ = p->priceDiv;
  step_ = p->step;
  isBook_ = p->isBook;
  eiPriceFlavor_ = p->eiPriceFlavor;
  fireActionIdx_ = p->fireActionIdx;

  maxAskSize_ = p->maxAskSize;
  maxBidSize_ = p->maxBidSize;

  hwMidPoint_ = midPoint_ / step_;

  if ((hwMidPoint_ * step_ != midPoint_) && isBook_)
    on_error("Product[%jd]: Misconfiguration: "
             "step_=%ju, "
             "midPoint_=%ju, "
             "hwMidPoint_=%u",
             handle_, step_, midPoint_, hwMidPoint_);

  EKA_LOG("Initialized Product[%jd]: "
          "secId_ = %jd, "
          "midPoint_ = %ju, "
          "step_ = %ju, "
          "hwMidPoint_ = %u, "
          "priceDiv_ = %ju, "
          "%s, ",
          handle_, secId_, midPoint_, step_, hwMidPoint_,
          priceDiv_, isBook_ ? "HAS BOOK" : "NO BOOK");

  book_ = new EkaEobiBook(strat_, this, step_, midPoint_);
  if (!book_)
    on_error("!book_");
}
/* --------------------------------------------------- */
/* --------------------------------------------------- */

EkaBCOpResult EkaBcEurProd::setJumpParams(
    const EkaBcEurJumpParams *params) {
  memcpy(&jumpParams_, params, sizeof(*params));
  HwJumpParams p = {};
  /* -------------------------------------------------- */

  p.prodParams.secId = secId_;
  p.prodParams.prodHandle = normalizeHandle(handle_);
  p.prodParams.actionIdx =
      normalizeActionIdx(fireActionIdx_);
  p.prodParams.askSize = normalizeFireSize(maxAskSize_);
  p.prodParams.bidSize = normalizeFireSize(maxBidSize_);
  p.prodParams.maxBookSpread =
      normalizePriceSpread(maxBookSpread_);
  /* -------------------------------------------------- */

  for (auto i = 0; i < EKA_JUMP_BETTERBEST_SETS; i++) {
    auto s = &jumpParams_.betterBest[i];
    auto d = &p.betterBest[i];

    d->bitParams =
        (s->strat_en ? 0x01 : 0) | (s->boc ? 0x02 : 0);
    d->askSize = normalizeFireSize(s->sell_size);
    d->bidSize = normalizeFireSize(s->buy_size);
    d->minTobSize = normalizeMdSize(s->min_tob_size);
    d->maxTobSize = normalizeMdSize(s->max_tob_size);
    d->maxPostSize = normalizeFireSize(s->max_post_size);
    d->minTickerSize = normalizeMdSize(s->min_ticker_size);
    d->minPriceDelta =
        normalizePriceDelta(s->min_price_delta);
  }

  /* -------------------------------------------------- */

  for (auto i = 0; i < EKA_JUMP_ATBEST_SETS; i++) {
    auto s = &jumpParams_.atBest[i];
    auto d = &p.atBest[i];

    d->bitParams =
        (s->strat_en ? 0x01 : 0) | (s->boc ? 0x02 : 0);
    d->askSize = normalizeFireSize(s->sell_size);
    d->bidSize = normalizeFireSize(s->buy_size);
    d->minTobSize = normalizeMdSize(s->min_tob_size);
    d->maxTobSize = normalizeMdSize(s->max_tob_size);
    d->maxPostSize = normalizeFireSize(s->max_post_size);
    d->minTickerSize = normalizeMdSize(s->min_ticker_size);
    d->minPriceDelta =
        normalizePriceDelta(s->min_price_delta);
  }
  /* -------------------------------------------------- */

  static const size_t EXPECTED_HW_SIZE = 176;
  if (sizeof(p) != EXPECTED_HW_SIZE)
    on_error("sizeof(p) %ju != EXPECTED_HW_SIZE %ju",
             sizeof(p), EXPECTED_HW_SIZE);

  auto nWords = roundUp8(sizeof(p)) / 8;
  auto sPtr = reinterpret_cast<const uint64_t *>(&p);
  uint dst = 0x50000 + handle_ * 256;

  for (auto i = 0; i < nWords; i++)
    eka_write(dst + 8 * i, *(sPtr++));
  return EKABC_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaBCOpResult EkaBcEurProd::setReferenceJumpParams(
    EkaBcSecHandle fireProdHandle,
    const EkaBcEurReferenceJumpParams *params) {
  memcpy(&refJumpParams_[fireProdHandle], params,
         sizeof(*params));


  auto fireProd = strat_->getProd(fireProdHandle);
  if (!fireProd)
    on_error("!fireProd");
  
  HwReferenceJumpParams p = {};

  
  /* -------------------------------------------------- */

  p.prodParams.secId = fireProd->secId_;
  p.prodParams.prodHandle = normalizeHandle(fireProd->handle_);
  p.prodParams.actionIdx =
      normalizeActionIdx(fireProd->fireActionIdx_);
  p.prodParams.askSize = normalizeFireSize(fireProd->maxAskSize_);
  p.prodParams.bidSize = normalizeFireSize(fireProd->maxBidSize_);
  p.prodParams.maxBookSpread =
      normalizePriceSpread(fireProd->maxBookSpread_);
  /* -------------------------------------------------- */

  for (auto i = 0; i < EKA_RJUMP_BETTERBEST_SETS; i++) {
    auto s = &refJumpParams_[fireProdHandle].betterBest[i];
    auto d = &p.betterBest[i];

    d->bitParams =
        (s->strat_en ? 0x01 : 0) | (s->boc ? 0x02 : 0);

    d->askSize = normalizeFireSize(s->sell_size);
    d->bidSize = normalizeFireSize(s->buy_size);

    d->minSpread = normalizePriceSpread(s->min_spread);

    d->maxOppositeTobSize =
        normalizeMdSize(s->max_opposit_tob_size);
    d->minTobSize = normalizeMdSize(s->min_tob_size);
    d->maxTobSize = normalizeMdSize(s->max_tob_size);

    d->timeDelta = normalizeTimeDelta(s->time_delta_ns);
    d->tickerSizeLots = normalizeMdSize(s->tickersize_lots);
  }
  /* -------------------------------------------------- */

  for (auto i = 0; i < EKA_RJUMP_ATBEST_SETS; i++) {
    auto s = &refJumpParams_[fireProdHandle].atBest[i];
    auto d = &p.atBest[i];

    d->bitParams =
        (s->strat_en ? 0x01 : 0) | (s->boc ? 0x02 : 0);

    d->askSize = normalizeFireSize(s->sell_size);
    d->bidSize = normalizeFireSize(s->buy_size);

    d->minSpread = normalizePriceSpread(s->min_spread);

    d->maxOppositeTobSize =
        normalizeMdSize(s->max_opposit_tob_size);
    d->minTobSize = normalizeMdSize(s->min_tob_size);
    d->maxTobSize = normalizeMdSize(s->max_tob_size);

    d->timeDelta = normalizeTimeDelta(s->time_delta_ns);
    d->tickerSizeLots = normalizeMdSize(s->tickersize_lots);
  }

  /* -------------------------------------------------- */

  
  /* static const size_t EXPECTED_HW_SIZE = 154; */
  /* if (sizeof(p) != EXPECTED_HW_SIZE) */
  /*   on_error("sizeof(p) %ju != EXPECTED_HW_SIZE %ju", */
  /*            sizeof(p), EXPECTED_HW_SIZE); */

  auto nWords = roundUp8(sizeof(p)) / 8;
  auto sPtr = reinterpret_cast<const uint64_t *>(&p);
  uint dst = 0x60000 + handle_ * 16 * 256;

  for (auto i = 0; i < nWords; i++)
    eka_write(dst + 8 * i, *(sPtr++));

  /* -------------------------------------------------- */
  //configuring the first ref target, for fast book fetch
  EKA_LOG("Setting Reference: trigger prod=%d, ref prod=%d",
	  handle_, fireProdHandle);
  
  return EKABC_OPRESULT__OK;
}
/* --------------------------------------------------- */
