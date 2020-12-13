#ifndef _EKA_FH_BATS_GR_H_
#define _EKA_FH_BATS_GR_H_

#include "EkaFhGroup.h"

class EkaFhBatsGr : public EkaFhGroup{
 public:
  FhBatsGr();
  virtual               ~FhBatsGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);
  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

  char                  sessionSubID[4] = {};  // for BATS Spin
  uint8_t               batsUnit = -1;
};
#endif
