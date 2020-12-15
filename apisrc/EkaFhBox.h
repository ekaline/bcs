#ifndef _EKA_FH_BOX_H_
#define _EKA_FH_BOX_H_

#include "EkaFh.h"

class EkaFhBoxGr;

class EkaFhBox : public EkaFh { // MiaxTom & PearlTom
 public:
  static const uint QSIZE = 1024 * 1024;

  EkaFhGroup* addGroup();

  EkaOpResult getDefinitions (EfhCtx*          pEfhCtx, 
			      const EfhRunCtx* pEfhRunCtx, 
			      EkaGroup*        group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId);

  virtual ~EkaFhBox() {};
 private:
  uint8_t*    getUdpPkt(EkaFhRunGroup* runGr, 
			uint16_t*      pktLen, 
			uint64_t*      sequence, 
			uint8_t*       gr_id);
  
};

#endif
