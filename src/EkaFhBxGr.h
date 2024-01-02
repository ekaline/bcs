#ifndef _EKA_FH_BX_GR_H_
#define _EKA_FH_BX_GR_H_


#include "EkaFhNasdaqGr.h"
#include "EkaFhFullBook.h"
#include "EkaFhBxParser.h"

class EkaFhBxGr : public EkaFhNasdaqGr {
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
    EkaFhNoopQuotePostProc,
    SecurityIdT, OrderIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

  public:
  virtual ~EkaFhBxGr() {
    if (book) {
      delete book;
      EKA_DEBUG("%s:%u Book is deleted",
		EKA_EXCH_DECODE(exch),id);
    }
  };

  bool parseMsg(const EfhRunCtx* pEfhRunCtx,
		const unsigned char* m,
		uint64_t sequence,
		EkaFhMode op,
		std::chrono::high_resolution_clock::time_point
		startTime={});

  int bookInit();
  int invalidateQ();
  int invalidateBook();
  void printBookState();

  int subscribeStaticSecurity(uint64_t securityId, 
			      EfhSecurityType efhSecurityType,
			      EfhSecUserData  efhSecUserData,
			      uint64_t        opaqueAttrA,
			      uint64_t        opaqueAttrB) {
    if (!book)
      on_error("%s:%u !book",EKA_EXCH_DECODE(exch),id);
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }

  void print_q_state();
  
private:
  template <class SecurityT, class Msg>
  SecurityT* processTradingAction(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processAddOrder(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processAddQuote(const unsigned char* m);
  
  template <class SecurityT, class Msg>
  SecurityT* processDeleteOrder(const unsigned char* m);

  template <class SecurityT, class Msg, bool SendTrade>
  SecurityT* processOrderExecuted(const unsigned char* m,
                                  uint64_t sequence,uint64_t msgTs,
                                  const EfhRunCtx* pEfhRunCtx);

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

  template <class SecurityT, class Msg>
  SecurityT* processAuctionUpdate(const unsigned char* m,
                                  uint64_t sequence,uint64_t msgTs,
                                  const EfhRunCtx* pEfhRunCtx);
  template <class Msg>
  void processDefinition(const unsigned char* m,
                         const EfhRunCtx* pEfhRunCtx);
  
  template <class Msg>
  uint64_t processEndOfSnapshot(const unsigned char* m,
                                EkaFhMode op);
     
};
#endif
