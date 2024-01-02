#ifndef _EKA_FH_PHLX_TOPO_GR_H_
#define _EKA_FH_PHLX_TOPO_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhTobBook.h"

class EkaFhPhlxTopoGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhPhlxTopoGr() {};

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


  int closeIncrementalGap(EfhCtx*          pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t         startSeq,
			  uint64_t         endSeq);
  
  //-----------------------------------------------------------------

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
