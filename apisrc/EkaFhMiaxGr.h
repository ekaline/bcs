#ifndef _EKA_FH_MIAX_GR_H_
#define _EKA_FH_MIAX_GR_H_

#include "EkaFhGroup.h"

class EkaFhMiaxGr : public EkaFhGroup{
 public:
  EkaFhMiaxGr();
  virtual               ~EkaFhMiaxGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);

  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      int16_t          pktLen);

  void                  pushUdpPkt2Q(const uint8_t* pkt, int16_t pktLen);

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
