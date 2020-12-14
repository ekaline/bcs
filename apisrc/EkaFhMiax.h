#ifndef _EKA_FH_MIAX_H_
#define _EKA_FH_MIAX_H_

#include "EkaFh.h"

class EkaFhMiax : public EkaFh { // MiaxTom & PearlTom
 public:
  static const uint QSIZE = 1024 * 1024;

  EkaOpResult getDefinitions (EfhCtx*          pEfhCtx, 
			      const EfhRunCtx* pEfhRunCtx, 
			      EkaGroup*        group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId );

  virtual     ~EkaFhMiax() {};

 private:
  uint8_t* getUdpPkt(EkaFhRunGroup* runGr, 
		     uint*          msgInPkt, 
		     int16_t*       pktLen, 
		     uint64_t*      sequence,
		     uint8_t*       gr_id);

};

#endif
