#ifndef _EKA_FH_PHLX_ORD_GR_H_
#define _EKA_FH_PHLX_ORD_GR_H_

#include "EkaFhNasdaqGr.h"

class EkaFhPhlxOrdGr : public EkaFhNasdaqGr{
 public:
  virtual              ~EkaFhPhlxOrdGr() {};
  
  bool                 parseMsg(const EfhRunCtx* pEfhRunCtx,
				unsigned char* m,
				uint64_t sequence,
				EkaFhMode op);

  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);
  
  
  bool                 processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				     const uint8_t*   pkt, 
				     uint             msgInPkt, 
				     uint64_t         seq);

  void                 pushUdpPkt2Q(const uint8_t* pkt, 
				    uint           msgInPkt, 
				    uint64_t       sequence);

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
