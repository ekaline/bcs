#ifndef _EKA_FH_BOX_GR_H_
#define _EKA_FH_BOX_GR_H_

#include "EkaFhGroup.h"

class EkaHsvfTcp;

class EkaFhBoxGr : public EkaFhGroup{
 public:
  EkaFhBoxGr();
  virtual               ~EkaFhBoxGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);

  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      int16_t          pktLen);
  

  void                  pushUdpPkt2Q(const uint8_t* pkt, 
				     uint           msgInPkt);


  int                  closeSnapshotGap(EfhCtx*              pEfhCtx, 
					const EfhInitCtx* pEfhRunCtx, 
					uint64_t          startSeq,
					uint64_t          endSeq);

  int                  closeIncrementalGap(EfhCtx*           pEfhCtx, 
					   const EfhInitCtx* pEfhRunCtx, 
					   uint64_t          startSeq,
					   uint64_t          endSeq);


  /* ##################################################################### */

  EkaHsvfTcp* hsvfTcp  = NULL;
  uint64_t    txSeqNum = 1;

};
#endif
