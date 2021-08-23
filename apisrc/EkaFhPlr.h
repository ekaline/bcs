#ifndef _EKA_FH_PLR_H_
#define _EKA_FH_PLR_H_

#include "EkaFh.h"

class EkaFhPlrGr;

class EkaFhPlr : public EkaFh {
  static const uint QSIZE = 1024 * 1024;
 protected:
  EkaFhGroup* addGroup();


 public:
  EkaOpResult getDefinitions (EfhCtx*          pEfhCtx, 
			      const EfhRunCtx* pEfhRunCtx, 
			      EkaGroup*        group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId);


  void        pushUdpPkt2Q(EkaFhPlrGr*   gr, 
			   const uint8_t* pkt, 
			   uint           msgInPkt, 
			   uint64_t       sequence,
			   uint8_t        gr_id);


  virtual ~EkaFhPlr() {};
 private:

  const uint8_t*   getUdpPkt(EkaFhRunGroup* runGr, 
			     uint*          msgInPkt, 
			     uint32_t*      sequence,
			     uint8_t*       gr_id,
			     uint8_t*       pktType);

};
#endif

