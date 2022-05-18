#ifndef _EKA_FH_BX_GR_H_
#define _EKA_FH_BX_GR_H_

#include "EkaFhNomGr.h"
#include "EkaFhBxParser.h"

class EkaFhBxGr : public EkaFhNomGr {
public:
  virtual ~EkaFhBxGr() {}

public:
  static const uint   SCALE          = (const uint) 24;
  static const uint   SEC_HASH_SCALE = 19;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhPlevel    = EkaFhPlevel<PriceT, SizeT>;
  using FhSecurity  = EkaFhFbSecurity<FhPlevel,SecurityIdT,OrderIdT,PriceT,SizeT>;
  using FhOrder     = EkaFhOrder<FhPlevel,OrderIdT,SizeT>;

  using FhBook      = EkaFhFullBook<
    SCALE,SEC_HASH_SCALE,
    FhSecurity,
    FhPlevel,
    FhOrder,
    SecurityIdT, OrderIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

  template <class SecurityT, class Msg>
  SecurityT* processDeleteOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processOrderExecuted(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processReplaceOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processSingleSideUpdate(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processReplaceQuote(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processDeleteQuote(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processTradeQ(const unsigned char* m,
			  uint64_t sequence,uint64_t msgTs,
			  const EfhRunCtx* pEfhRunCtx);

private:
  using Feed = Bx;
  
};
#endif
