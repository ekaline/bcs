#ifndef _EKA_FH_NASDAQ_H_
#define _EKA_FH_NASDAQ_H_

#include "EkaFh.h"


class FhNasdaq : public EkaFh { // base class for Nom, Gem, Ise, Phlx
  static const uint QSIZE = 1024 * 1024;
 public:
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );

  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  virtual ~FhNasdaq() {};
  virtual uint8_t* getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id);
  virtual bool processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence);
  virtual void pushUdpPkt2Q(FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence,int8_t gr_id);
 private:

};

#endif
