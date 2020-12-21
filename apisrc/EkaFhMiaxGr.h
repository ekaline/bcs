#ifndef _EKA_FH_MIAX_GR_H_
#define _EKA_FH_MIAX_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhTobBook.h"

class EkaFhMiaxGr : public EkaFhGroup{
 public:
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
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq);

  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhRunCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq);


  int                  bookInit(EfhCtx* pEfhCtx, 
				const EfhInitCtx* pEfhInitCtx);

  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,SecurityIdT, PriceT, SizeT>;

};
#endif
