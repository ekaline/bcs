#ifndef _EKA_FH_NOM_GR_H_
#define _EKA_FH_NOM_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhFullBook.h"

class EkaFhNomGr : public EkaFhNasdaqGr {
 public:
  virtual              ~EkaFhNomGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);

  static const uint   SCALE          = (const uint) 24;
  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurityState = EkaFhSecurityState<PriceT,SizeT>;
  using FhBook          = EkaFhFullBook<SCALE,SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>;

};

#endif
