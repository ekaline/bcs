#ifndef _EKA_FH_MRX2_TOP_GR_H_
#define _EKA_FH_MRX2_TOP_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhTobBook.h"

class EkaFhMrx2TopGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhMrx2TopGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				const unsigned char* m,
				uint64_t sequence,
				EkaFhMode op,std::chrono::high_resolution_clock::time_point startTime={});

  int                  bookInit();

  int                  subscribeStaticSecurity(uint64_t        securityId, 
					       EfhSecurityType efhSecurityType,
					       EfhSecUserData  efhSecUserData,
					       uint64_t        opaqueAttrA,
					       uint64_t        opaqueAttrB) {
    if (book == NULL) on_error("%s:%u book == NULL",EKA_EXCH_DECODE(exch),id);
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }

  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,
      EkaFhTobSecurity<SecurityIdT, PriceT, SizeT>,
      EkaFhNoopQuotePostProc,
      SecurityIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

};

#endif
