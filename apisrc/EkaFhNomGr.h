#ifndef _EKA_FH_NOM_GR_H_
#define _EKA_FH_NOM_GR_H_

#include "EkaFhNasdaqGr.h"

class EkaFhNomGr : public EkaFhNasdaqGr {
 public:
  virtual              ~EkaFhNomGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);
};

#endif
