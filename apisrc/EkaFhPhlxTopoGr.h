#ifndef _EKA_FH_PHLX_TOPO_GR_H_
#define _EKA_FH_PHLX_TOPO_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhTobBook.h"

class EkaFhPhlxTopoGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhPhlxTopoGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);


  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,SecurityIdT, PriceT, SizeT>;

};

#endif
