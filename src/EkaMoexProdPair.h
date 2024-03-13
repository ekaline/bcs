#ifndef _EKA_MOEX_PROD_PAIR_H_
#define _EKA_MOEX_PROD_PAIR_H_

#include "EkaBcs.h"

using namespace EkaBcs;

class EkaMoexProdPair {
public:
  EkaMoexProdPair(PairIdx idx,
                  const ProdPairInitParams *params);

  OpResult
  setDynamicParams(const ProdPairDynamicParams *params);

  OpResult downloadParams();

  // static during Init()
  MoexSecurityId secBase_;
  MoexSecurityId secQuote_;

private:
  EkaActionIdx fireBaseNewIdx_ = -1;
  EkaActionIdx fireQuoteReplaceIdx_ = -1;

  // dynamic prices
  MoexPrice markupBuy_ = -1;
  MoexPrice markupSell_ = -1;
  MoexPrice fixSpread_ = -1;
  MoexPrice tolerance_ = -1;
  MoexPrice slippage_ = -1;

  // dynamic sizes
  MoexSize quoteSize_ = -1;

  // dynamic times
  MoexTimeNs timeTolerance_ = -1;

  PairIdx idx_ = -1;
};

#endif
