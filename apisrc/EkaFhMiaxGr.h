#ifndef _EKA_FH_MIAX_GR_H_
#define _EKA_FH_MIAX_GR_H_

#include "EkaFhGroup.h"

class EkaFhMiaxGr : public EkaFhGroup{
 public:
  FhMiaxGr();
  virtual               ~FhMiaxGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);
  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

};
#endif
