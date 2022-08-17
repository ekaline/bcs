#ifndef _EKA_FH_MRX2_TOP_GR_H_
#define _EKA_FH_MRX2_TOP_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhTobBook.h"

class EkaFhMrx2TopGr : public EkaFhNasdaqGr{
 public:
  virtual ~EkaFhMrx2TopGr() {};

  bool parseMsg(const EfhRunCtx* pEfhRunCtx,
		const unsigned char* m,
		uint64_t sequence,
		EkaFhMode op,
		std::chrono::high_resolution_clock::time_point startTime={});

  int bookInit();

  int subscribeStaticSecurity(uint64_t        securityId, 
			      EfhSecurityType efhSecurityType,
			      EfhSecUserData  efhSecUserData,
			      uint64_t        opaqueAttrA,
			      uint64_t        opaqueAttrB) {
    if (!book)
      on_error("%s:%u book == NULL",EKA_EXCH_DECODE(exch),id);
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }

  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using BookPriceT  = int64_t;
  using SizeT       = uint32_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, BookPriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,
				   FhSecurity,
				   EkaFhNoopQuotePostProc,
				   SecurityIdT, BookPriceT, SizeT>;

  using VanillaPriceT      = uint32_t;
  using ComplexPriceT      = int32_t;

  
  FhBook*   book = NULL;

  
private:
  template <class Msg>
  void processDefinition(const unsigned char* m,
			 const EfhRunCtx* pEfhRunCtx);

  template <class Msg>
  uint64_t processEndOfSnapshot(const unsigned char* m,
				EkaFhMode op);
  template <class SecurityT, class Msg>
  SecurityT* processOneSideUpdate(const unsigned char* m);
  
  template <class SecurityT, class Msg>
  SecurityT* processTwoSidesUpdate(const unsigned char* m);
  
  template <class SecurityT, class Msg>
  SecurityT* processTrade(const unsigned char* m,
			   uint64_t sequence,
			   const EfhRunCtx* pEfhRunCtx);
  template <class SecurityT, class Msg>
  SecurityT* processTradingAction(const unsigned char* m);

  template <class Root, class Leg>
  inline void processComplexDefinition(const unsigned char* m,
				       const EfhRunCtx* pEfhRunCtx);
  
  template <class SecurityT, class Msg>
  SecurityT* processComplexTwoSidesUpdate(const unsigned char* m);

  template <class SecurityT, class Msg>
  SecurityT* processComplexOneSideUpdate(const unsigned char* m);
  
};

#endif
