#ifndef _EKA_FH_NOM_GR_H_
#define _EKA_FH_NOM_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhFullBook.h"

class EkaFhNomGr : public EkaFhNasdaqGr {
 public:
  virtual              ~EkaFhNomGr() {
    invalidateQ();
    invalidateBook();
  };

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				const unsigned char* m,
				uint64_t sequence,
				EkaFhMode op,std::chrono::high_resolution_clock::time_point startTime={});

  int                  bookInit();
  int                  invalidateQ();
  int                  invalidateBook();

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

};

#endif
