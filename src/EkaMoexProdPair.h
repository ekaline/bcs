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

  OpResult downloadStaticParams();

private:
  MoexSecurityId secA_;
  MoexSecurityId secB_;

  EkaBcsActionIdx fireActionIdx_ = -1;
  PairIdx idx_ = -1;
};

#endif
