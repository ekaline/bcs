#ifndef __EKA_BC_EUR_PROD_H__
#define __EKA_BC_EUR_PROD_H__

#include "eka_macros.h"

#include "EkaBc.h"

class EkaEobiBook;

class EkaBcEurProd {
public:
  EkaBcEurProd(EkaBcSecHandle handle,
               const EkaBcEurProductInitParams *p);

  EkaBCOpResult
  setJumpParams(const EkaBcEurJumpParams *params);

  EkaBCOpResult setReferenceJumpParams(
      EkaBcSecHandle fireProd,
      const EkaBcEurReferenceJumpParams *params);

public:
  EkaBcSecId secId_ = 0;
  uint64_t maxBookSpread_ = 0;
  uint64_t midPoint_ = 0;
  uint64_t priceDiv_ =
      0; // for price normalization for prints only
  uint64_t step_ = 0;
  bool isBook_ = 0;
  uint8_t eiPriceFlavor_ = 0;

  uint32_t hwMidPoint_ = 0;

  EkaEobiBook *book_ = nullptr;
  EkaEobiParser *parser_ = nullptr;

private:
  static const size_t MaxReferenceProds_ = 16;

  EkaEurStrategy *strat_ = nullptr;

  EkaBcEurJumpParams jumpParams_ = {};
  EkaBcEurReferenceJumpParams
      refJumpParams_[MaxReferenceProds_] = {};
  EkaBcSecHandle handle_ = -1;
};

#endif
