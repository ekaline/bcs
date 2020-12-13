#ifndef _EKA_FH_PHLX_ORD_H_
#define _EKA_FH_PHLX_ORD_H_

#include "EkaNasdaq.h"

class FhPhlxOrd : public FhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
public:
  EkaOpResult initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhRunGr* runGr);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );

  uint8_t* getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id);
  bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence);
  void pushUdpPkt2Q(FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence,int8_t gr_id);

  virtual ~FhPhlxOrd() {};
private:

};
#endif
