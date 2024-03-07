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

private:
  // static during Init()
  MoexSecurityId secBase_;
  MoexSecurityId secQuote_;
  EkaBcsActionIdx fireBaseNewIdx_ = -1;
  EkaBcsActionIdx fireQuoteReplaceIdx_ = -1;

  // dynamic prices
  EkaBcsMoexPrice markupBuy_ = -1;
  EkaBcsMoexPrice markupSell_ = -1;
  EkaBcsMoexPrice fixSpread_ = -1;
  EkaBcsMoexPrice tolerance_ = -1;
  EkaBcsMoexPrice negTolerance_ = -1;

  // dynamic sizes
  EkaBcsMoexSize quoteSize_ = -1;

  // dynamic times
  EkaBcsMoexTimeNs timeTolerance_ = -1;

  PairIdx idx_ = -1;
};

#endif
