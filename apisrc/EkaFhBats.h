#ifndef _EKA_FH_BATS_H_
#define _EKA_FH_BATS_H_

#include "EkaFh.h"

class FhBats : public EkaFh {
   static const uint QSIZE = 1024 * 1024;
public:
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  uint8_t     getGrId(const uint8_t* pkt);
  bool        processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhBatsGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence);
  void        pushUdpPkt2Q(FhBatsGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequenceu,int8_t gr_id);
  uint8_t*    getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id);

  virtual ~FhBats() {};
private:

};
#endif

