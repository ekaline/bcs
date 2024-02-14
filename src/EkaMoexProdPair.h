#ifndef _EKA_MOEX_PROD_PAIR_H_
#define _EKA_MOEX_PROD_PAIR_H_

#include "EkaBcs.h"
#include "eka_hw_conf.h"

using namespace EkaBcs;

class EkaMoexProdPair {
public:
  EkaMoexProdPair(const ProdPairInitParams *params) {
    secA_ = params->secA;
    secB_ = params->secB;

    fireActionIdx_ = params->fireActionIdx;
  }

  OpResult
  setDynamicParams(const ProdPairDynamicParams *params) {
    return OPRESULT__OK;
  }

public:
  MoexSecurityId secA_;
  MoexSecurityId secB_;

  EkaBcsActionIdx fireActionIdx_ = -1;
};

#endif
