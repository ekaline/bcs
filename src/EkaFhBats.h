#ifndef _EKA_FH_BATS_H_
#define _EKA_FH_BATS_H_

#include "EkaFh.h"

class EkaFhBatsGr;

class EkaFhBats : public EkaFh {
  static const uint QSIZE = 1024 * 1024;

  static const int MaxOpenSnapshotGaps = 1;
  static const int MaxOpenIncrGaps = 1;
  
 protected:
  EkaFhGroup* addGroup();


 public:
  EkaOpResult getDefinitions (EfhCtx*          pEfhCtx, 
			      const EfhRunCtx* pEfhRunCtx, 
			      const EkaGroup*  group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId);


  void        pushUdpPkt2Q(EkaFhBatsGr*   gr, 
			   const uint8_t* pkt, 
			   uint           msgInPkt, 
			   uint64_t       sequence,
			   uint8_t        gr_id);


  virtual ~EkaFhBats() {};
 private:
  uint8_t    getGrId(const uint8_t* pkt);

  const uint8_t*   getUdpPkt(EkaFhRunGroup* runGr, 
			     uint*          msgInPkt, 
			     uint64_t*      sequence,
			     uint8_t*       gr_id);

};
#endif

