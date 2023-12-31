#ifndef _EKA_FH_XDP_H_
#define _EKA_FH_XDP_H_

#include "EkaFh.h"

class EkaFhXdp : public EkaFh {
  static const uint QSIZE = 1 * 1024 * 1024;

 protected:
  EkaFhGroup*         addGroup();

 public:
  EkaOpResult getDefinitions (EfhCtx*          pEfhCtx, 
			      const EfhRunCtx* pEfhRunCtx, 
			      const EkaGroup*  group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId);

  virtual ~EkaFhXdp() {};
 private:
  const uint8_t* getUdpPkt(EkaFhRunGroup* runGr, 
			   uint*          msgInPkt, 
			   uint*          pktSize, 
			   uint64_t*      sequence,
			   uint8_t*       gr_id,
			   uint16_t*      streamId, 
			   uint8_t*       pktType);

};
#endif
