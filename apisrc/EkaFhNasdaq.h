#ifndef _EKA_FH_NASDAQ_H_
#define _EKA_FH_NASDAQ_H_

#include "EkaFh.h"


class EkaFhNasdaq : public EkaFh { // base class for Nom, Gem, Ise, Phlx
  static const uint QSIZE = 1024 * 1024;
 public:
  virtual ~EkaFhNasdaq() {};

  virtual EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
				const EfhRunCtx* pEfhRunCtx, 
				uint8_t          runGrId);

  EkaOpResult         getDefinitions (EfhCtx*          pEfhCtx, 
				      const EfhRunCtx* pEfhRunCtx, 
				      const EkaGroup*  group);

 protected:
  const uint8_t* getUdpPkt(EkaFhRunGroup* runGr, 
			   uint*          msgInPkt, 
			   uint64_t*      sequence,
			   uint8_t*       gr_id);

};

#endif
