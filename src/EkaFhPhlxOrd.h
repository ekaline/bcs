#ifndef _EKA_FH_PHLX_ORD_H_
#define _EKA_FH_PHLX_ORD_H_

#include "EkaFhNasdaq.h"

class EkaFhPhlxOrd : public EkaFhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
 public:
  EkaFhGroup*         addGroup();

  EkaOpResult  runGroups(EfhCtx*          pEfhCtx, 
			 const EfhRunCtx* pEfhRunCtx, 
			 uint8_t          runGrId);

  virtual ~EkaFhPhlxOrd() {};
 private:
  const uint8_t*    getUdpPkt(EkaFhRunGroup* runGr, 
			      uint*          msgInPkt, 
			      uint64_t*      sequence,
			      uint8_t*       gr_id);

};
#endif
