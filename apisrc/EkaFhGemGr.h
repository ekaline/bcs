#ifndef _EKA_FH_GEM_GR_H_
#define _EKA_FH_GEM_GR_H_

#include "EkaFhNasdaqGr.h"

class EkaFhGemGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhGemGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

};

#endif
