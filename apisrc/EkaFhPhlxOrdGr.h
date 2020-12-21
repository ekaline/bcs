#ifndef _EKA_FH_PHLX_ORD_GR_H_
#define _EKA_FH_PHLX_ORD_GR_H_

#include "EkaFhNasdaqGr.h"
#include "EkaFhFullBook.h"

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

  int    closeIncrementalGap(EfhCtx*           pEfhCtx, 
			     const EfhRunCtx* pEfhRunCtx, 
			     uint64_t          startSeq,
			     uint64_t          endSeq);


  static const uint   SCALE          = (const uint) 22;
  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint32_t;
  using OrderIdT    = uint32_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurityState = EkaFhSecurityState<PriceT,SizeT>;
  using FhBook          = EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>;

};

#endif
