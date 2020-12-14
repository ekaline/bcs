#ifndef _EHA_FH_NASDAQ_GR_H_
#define _EHA_FH_NASDAQ_GR_H_

#include "EkaFhGroup.h"

class EkaFhNasdaqGr : public EkaFhGroup {
 public:
  virtual             ~EkaFhNasdaqGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);

  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);


  bool   processUdpPkt(const EfhRunCtx* pEfhRunCtx,
		       const uint8_t*   pkt, 
		       uint             msgInPkt, 
		       uint64_t         seq);

  void   pushUdpPkt2Q(const uint8_t* pkt, 
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

 private:


  /* ##################################################################### */
};

#endif
