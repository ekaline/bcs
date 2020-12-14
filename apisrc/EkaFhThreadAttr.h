#ifndef _EKA_FH_THREAD_ATTR_H_
#define _EKA_FH_THREAD_ATTR_H_

#include "EkaFh.h"

class EfhRunCtx;
class EfhCtx;
class EkaFhGroup;

class EkaFhThreadAttr {
 public:
  EkaFhThreadAttr(EfhCtx*          _pEfhCtx, 
		  const EfhRunCtx* _pEfhRunCtx, 
		  EkaFhGroup*      _gr, 
		  uint64_t         _start, 
		  uint64_t         _end, 
		  EkaFhMode        _op) {

  pEfhCtx    = _efhCtx;
  pEfhRunCtx = (EfhRunCtx*)_efhRunCtx;
  gr         = _gr;
  startSeq   = _start;
  endSeq     = _end;
  runGrId    = _runGrId;
  op         = _op;
  }

  EfhCtx*     pEfhCtx     = NULL;
  EfhRunCtx*  pEfhRunCtx  = NULL;
  EkaFhGroup* gr          = NULL;
  uint64_t    startSeq    = -1;
  uint64_t    endSeq      = -1;
  EkaFhMode   op          = -1;
};

#endif
