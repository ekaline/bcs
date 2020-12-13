#ifndef _EKA_FH_BOX_GR_H_
#define _EKA_FH_BOX_GR_H_

#include "EkaFhGroup.h"

class EkaHsvfTcp;

class EkaFhBoxGr : public EkaFhGroup{
 public:
  FhBoxGr();
  virtual               ~FhBoxGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);
  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

  EkaHsvfTcp* hsvfTcp  = NULL;
  uint64_t    txSeqNum = 1;

};
#endif
