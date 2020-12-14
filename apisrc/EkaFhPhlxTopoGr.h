#ifndef _EKA_FH_PHLX_TOPO_GR_H_
#define _EKA_FH_PHLX_TOPO_GR_H_

#include "EkaFhNasdaqGr.h"

class EkaFhPhlxTopoGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhPhlxTopoGr() {};

  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

  bool   processUdpPkt(const EfhRunCtx* pEfhRunCtx,
		       const uint8_t*   pkt, 
		       uint             msgInPkt, 
		       uint64_t         seq);

  int    closeSnapshotGap(EfhCtx*              pEfhCtx, 
			  const EfhInitCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq);

  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhInitCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq);

};

#endif
