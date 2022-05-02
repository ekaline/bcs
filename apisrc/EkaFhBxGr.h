#ifndef _EKA_FH_BX_GR_H_
#define _EKA_FH_BX_GR_H_

#include "EkaFhNomGr.h"

class EkaFhBxGr : public EkaFhNomGr {
public:
  ~EkaFhBxGr() {}
  bool parseMsg(const EfhRunCtx* pEfhRunCtx,
		const unsigned char* m,
		uint64_t sequence,
		EkaFhMode op,
		std::chrono::high_resolution_clock::time_point startTime={});


public:
  static const uint   SCALE          = (const uint) 24;
  static const uint   SEC_HASH_SCALE = 19;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhPlevel     = EkaFhPlevel      <                                                   PriceT, SizeT>;
  using FhSecurity   = EkaFhFbSecurity  <EkaFhPlevel<PriceT, SizeT>, SecurityIdT, OrderIdT, PriceT, SizeT>;
  using FhOrder      = EkaFhOrder       <EkaFhPlevel<PriceT, SizeT>,              OrderIdT,         SizeT>;

  using FhBook      = EkaFhFullBook<
    SCALE,SEC_HASH_SCALE,
    EkaFhFbSecurity  <EkaFhPlevel<PriceT, SizeT>,SecurityIdT, OrderIdT, PriceT, SizeT>,
    EkaFhPlevel      <PriceT, SizeT>,
    EkaFhOrder       <EkaFhPlevel<PriceT, SizeT>,OrderIdT,SizeT>,
    SecurityIdT, OrderIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

private:
  template <class SecurityT, class Msg>
  SecurityT* processTradingAction(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processAddOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processAddQuote(const unsigned char* m);
  
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
  SecurityT* processTrade(const unsigned char* m,
			  uint64_t sequence,uint64_t msgTs,
			  const EfhRunCtx* pEfhRunCtx);
  template <class Msg>
  void processDefinition(const unsigned char* m,
			 const EfhRunCtx* pEfhRunCtx);
  
  inline uint64_t processEndOfSnapshot(const unsigned char* m,
				       EkaFhMode op);
     
};
#endif
