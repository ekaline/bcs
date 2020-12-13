#ifndef _EKA_FH_PHLX_TOPO_GR_H_
#define _EKA_FH_PHLX_TOPO_GR_H_

#include "EkaFhNasdaqGr.h"

class EkaFhPhlxTopoGr : public EkaFhNasdaqGr{
  virtual              ~EkaFhPhlxTopoGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);
  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);

};

#endif
