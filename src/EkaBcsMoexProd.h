#ifndef __EKA_BCS_MOEX_PROD_H__
#define __EKA_BCS_MOEX_PROD_H__

#include "eka_macros.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

class EkaMoexStrategy;
class EkaEobiBook;

class EkaBcsMoexProd {
public:
  EkaBcsMoexProd(EkaMoexStrategy *strat, EkaBcsSecHandle handle,
               const EkaBcsMoexProductInitParams *p);

  OpResult
  setDynamicParams(const EkaBcsProductDynamicParams *params);

  // OpResult
  // setJumpParams(const EkaBcEurJumpParams *params);

  // OpResult setReferenceJumpParams(
  //     EkaBcSecHandle fireProd,
  //     const EkaBcEurReferenceJumpParams *params);

private:
public:
  ExchSecurityId secId_ = 0;
  EkaBcsMoexFireSize maxBidSize_ = 0;
  EkaBcsMoexFireSize maxAskSize_ = 0;

  Price maxBookSpread_ = 0;
  Price midPoint_ = 0;
  uint64_t priceDiv_ =
      0; // for price normalization for prints only
  Price step_ = 0;
  bool isBook_ = 0;
  uint8_t eiPriceFlavor_ = 0;

  uint32_t hwMidPoint_ = 0;

  EkaBcsActionIdx fireActionIdx_ = -1;

  EkaEobiBook *book_ = nullptr;

  EkaBcsSecHandle handle_ = -1;

private:
  static const size_t MaxReferenceProds_ = 16;

  EkaMoexStrategy *strat_ = nullptr;

  // EkaBcEurJumpParams jumpParams_ = {};
  // EkaBcEurReferenceJumpParams
  //     refJumpParams_[MaxReferenceProds_] = {};

  int firstFireProd_ = -1;
  
};

#endif
