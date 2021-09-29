#ifndef _EKA_FH_CME_H_
#define _EKA_FH_CME_H_

#include "EkaFh.h"

class EkaFhCmeGr;

class EkaFhCme : public EkaFh { 
 public:
  static const uint QSIZE = 1024 * 1024;

  EkaFhGroup* addGroup();

  EkaOpResult getDefinitions (EfhCtx*          pEfhCtx, 
			      const EfhRunCtx* pEfhRunCtx, 
			      const EkaGroup*  group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId);

  virtual ~EkaFhCme() {};
 private:
  
  EkaOpResult initGroups(EfhCtx* pEfhCtx, 
			 const EfhRunCtx* pEfhRunCtx, 
			 EkaFhRunGroup* runGr);

  const uint8_t*    getUdpPkt(EkaFhRunGroup* runGr, 
			      int16_t*      pktLen, 
			      uint64_t*      sequence, 
			      uint8_t*       gr_id);
  
};

#endif
