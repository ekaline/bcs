#ifndef _EKA_FH_GEM_GR_H_
#define _EKA_FH_GEM_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhTobBook.h"

class EkaFhGemGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhGemGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);

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
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>,SecurityIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

};

#endif
