#ifndef _EKA_FH_MIAX_H_
#define _EKA_FH_MIAX_H_

#include "EkaFh.h"

class FhMiax : public EkaFh { // MiaxTom & PearlTom
 public:
  static const uint QSIZE = 1024 * 1024;
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  virtual ~FhMiax() {};
  uint8_t* getUdpPkt(FhRunGr* runGr, uint* msgInPkt, int16_t* pktLen, uint64_t* sequence,uint8_t* gr_id);
  bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhMiaxGr* gr, const uint8_t* pkt, int16_t pktLen);
  void pushUdpPkt2Q(FhMiaxGr* gr, const uint8_t* pkt, int16_t pktLen,int8_t gr_id);
 private:

};

#endif
