#ifndef _EKA_FH_BOX_GR_H_
#define _EKA_FH_BOX_GR_H_

#include "EkaFhGroup.h"

class EkaHsvfTcp;

class EkaFhBoxGr : public EkaFhGroup{
 public:
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
				     uint           pktLen);


  int                  closeIncrementalGap(EfhCtx*           pEfhCtx, 
					   const EfhRunCtx*  pEfhRunCtx, 
					   uint64_t          startSeq,
					   uint64_t          endSeq);



  /* ##################################################################### */

  EkaHsvfTcp* hsvfTcp  = NULL;
  uint64_t    txSeqNum = 1;
  char        line[2]             = {};

};
#endif
