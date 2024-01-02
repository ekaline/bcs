#ifndef _EHA_FH_NASDAQ_GR_H_
#define _EHA_FH_NASDAQ_GR_H_

#include "EkaFhGroup.h"

class EkaFhNasdaqGr : public EkaFhGroup {
 public:
  virtual             ~EkaFhNasdaqGr() {};

  bool   processUdpPkt(const EfhRunCtx* pEfhRunCtx,
		       const uint8_t*   pkt, 
		       uint             msgInPkt, 
		       uint64_t         seq);

  void   pushUdpPkt2Q(const uint8_t* pkt, 
		      uint           msgInPkt, 
		      uint64_t       sequence);

  int    closeSnapshotGap(EfhCtx*          pEfhCtx, 
			  const EfhRunCtx* pEfhRunCtx, 
			  uint64_t         startSeq,
			  uint64_t         endSeq);

  virtual int closeIncrementalGap(EfhCtx*          pEfhCtx, 
				  const EfhRunCtx* pEfhRunCtx, 
				  uint64_t         startSeq,
				  uint64_t         endSeq);
  
  /* ##################################################################### */

  // volatile
  char session_id[10] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};   // Mold Session Id
  bool          firstPkt       = true; // to get session_id
  static const int MoldLocalRetryAttempts = 5;
  uint64_t      firstSoupbinSeq = 0;
 private:


  /* ##################################################################### */
};

#endif
