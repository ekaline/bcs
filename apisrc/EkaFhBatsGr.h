#ifndef _EKA_FH_BATS_GR_H_
#define _EKA_FH_BATS_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhFullBook.h"

class EkaFhBatsGr : public EkaFhGroup{
 public:
  virtual               ~EkaFhBatsGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char*   m,
				 uint64_t         sequence,
				 EkaFhMode        op);

  int                   bookInit(EfhCtx*           pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      uint             msgInPkt, 
				      uint64_t         seq);

  void                 pushUdpPkt2Q(const uint8_t* pkt, 
				    uint           msgInPkt, 
				    uint64_t       sequence);


  int    closeSnapshotGap(EfhCtx*              pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t          startSeq,
			  uint64_t          endSeq);

  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhRunCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq);

  /* ##################################################################### */

  char                  sessionSubID[4] = {};  // for BATS Spin
  uint8_t               batsUnit = 0;


  static const uint   SCALE          = (const uint) 22;
  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurityState = EkaFhSecurityState<PriceT,SizeT>;
  using FhBook          = EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>;



};
#endif
