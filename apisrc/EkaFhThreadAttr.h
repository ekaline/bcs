#ifndef _EKA_FH_THREAD_ATTR_H_
#define _EKA_FH_THREAD_ATTR_H_

class EkaFhThreadAttr {
 public:
  EkaFhThreadAttr(EfhCtx* _pEfhCtx, const EfhRunCtx* _pEfhRunCtx, uint _runGrId, FhGroup* _gr, uint64_t _start, uint64_t _end, EkaFhMode _op) {
  pEfhCtx    = _efhCtx;
  pEfhRunCtx = (EfhRunCtx*)_efhRunCtx;
  gr         = _gr;
  startSeq   = _start;
  endSeq     = _end;
  runGrId    = _runGrId;
  op         = _op;
  }

  EfhCtx*     pEfhCtx;
  EfhRunCtx*  pEfhRunCtx;
  uint        runGrId;
  FhGroup*    gr;
  uint64_t    startSeq;
  uint64_t    endSeq;
  EkaFhMode   op;
};

#endif
