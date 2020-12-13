#ifndef _EKA_FH_PHLX_ORD_GR_H_
#define _EKA_FH_PHLX_ORD_GR_H_

#include "EkaFhNasdaqGr.h"

class EkaFhPhlxOrdGr : public EkaFhNasdaqGr{
  virtual              ~EkaFhPhlxOrdGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);
  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);

};

#endif
