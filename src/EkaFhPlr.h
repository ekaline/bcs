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
			      const EkaGroup*        group);

  EkaOpResult runGroups(EfhCtx*          pEfhCtx, 
			const EfhRunCtx* pEfhRunCtx, 
			uint8_t          runGrId);


  void        pushUdpPkt2Q(EkaFhPlrGr*   gr, 
			   const uint8_t* pkt, 
			   uint           msgInPkt, 
			   uint64_t       sequence,
			   uint8_t        gr_id);

  EkaOpResult subscribeStaticSecurity(uint8_t groupNum, 
				      uint64_t securityId, 
				      EfhSecurityType efhSecurityType,
				      EfhSecUserData efhSecUserData,
				      uint64_t opaqueAttrA,
				      uint64_t opaqueAttrB);
  
  virtual ~EkaFhPlr() {};
 private:

  const uint8_t*   getUdpPkt(EkaFhRunGroup* runGr, 
			     uint*          msgInPkt, 
			     uint32_t*      sequence,
			     uint8_t*       gr_id,
			     uint8_t*       pktType);

  EkaFhPlrGr* findTradesGroup();

};
#endif

