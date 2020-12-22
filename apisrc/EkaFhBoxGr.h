#ifndef _EKA_FH_BOX_GR_H_
#define _EKA_FH_BOX_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhTobBook.h"

class EkaHsvfTcp;

class EkaFhBoxGr : public EkaFhGroup{
 public:
  virtual               ~EkaFhBoxGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
				 unsigned char* m,
				 uint64_t sequence,
				 EkaFhMode op);

  int                   bookInit(EfhCtx* pEfhCtx,
				 const EfhInitCtx* pEfhInitCtx);

  int                  subscribeStaticSecurity(uint64_t        securityId, 
					       EfhSecurityType efhSecurityType,
					       EfhSecUserData  efhSecUserData,
					       uint64_t        opaqueAttrA,
					       uint64_t        opaqueAttrB) {
    if (book == NULL) on_error("%s:%u book == NULL",EKA_EXCH_DECODE(exch),id);
    book->subscribeSecurity(securityId, 
			    efhSecurityType,
			    efhSecUserData,
			    opaqueAttrA,
			    opaqueAttrB);
    return 0;
  }

  bool                  processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      int16_t          pktLen);
  

  void                  pushUdpPkt2Q(const uint8_t* pkt, 
				     uint           pktLen);

  int                   processFromQ(const EfhRunCtx* pEfhRunCtx);


  int                  closeIncrementalGap(EfhCtx*           pEfhCtx, 
					   const EfhRunCtx*  pEfhRunCtx, 
					   uint64_t          startSeq,
					   uint64_t          endSeq);



  /* ##################################################################### */

  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = uint64_t;
  using PriceT      = uint32_t;
  using SizeT       = uint32_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>,SecurityIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

  EkaHsvfTcp* hsvfTcp  = NULL;
  uint64_t    txSeqNum = 1;
  char        line[2]             = {};

};
#endif
