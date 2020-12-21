#ifndef _EKA_FH_NOM_GR_H_
#define _EKA_FH_NOM_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhFullBook.h"
#include "EkaFhSecurityState.h"

class EkaFhNomGr : public EkaFhNasdaqGr {
 public:
  virtual              ~EkaFhNomGr() {};

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

  static const uint   SCALE          = (const uint) 24;
  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurityState = EkaFhSecurityState<PriceT,SizeT>;
  using FhBook          = EkaFhFullBook<SCALE,SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

};

#endif
