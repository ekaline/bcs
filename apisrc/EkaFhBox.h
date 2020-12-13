#ifndef _EKA_FH_BOX_H_
#define _EKA_FH_BOX_H_

#include "EkaFh.h"

class FhBox : public EkaFh { // MiaxTom & PearlTom
 public:
  static const uint QSIZE = 1024 * 1024;
  EkaOpResult getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group);
  EkaOpResult runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId );
  EkaOpResult initGroups(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhRunGr* runGr);

  uint8_t*    getUdpPkt(FhRunGr* runGr, uint16_t* pktLen, uint64_t* sequence, uint8_t* gr_id);

  bool        processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhBoxGr* gr, const uint8_t* pkt, int16_t pktLen);
  void        pushUdpPkt2Q(FhBoxGr* gr, const uint8_t* pkt, int16_t pktLen,int8_t gr_id);

  virtual ~FhBox() {};
 private:

};

#endif
