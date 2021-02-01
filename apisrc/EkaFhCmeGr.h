#ifndef _EKA_FH_CME_GR_H_
#define _EKA_FH_CME_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhTobBook.h"

class EkaHsvfTcp;

class EkaFhCmeGr : public EkaFhGroup {
 public:
  virtual               ~EkaFhCmeGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
  				 const unsigned char* m,
  				 uint64_t sequence,
  				 EkaFhMode op) {
    return true;
  }

  int                   bookInit(EfhCtx* pEfhCtx,
  				 const EfhInitCtx* pEfhInitCtx) {
    return 0;
  }

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

  bool                  processPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      int16_t          pktLen,
				      EkaFhMode        op);
  
  /* bool                  processUdpDefinitionsPkt(const EfhRunCtx* pEfhRunCtx, */
  /* 						 const uint8_t*   pkt,  */
  /* 						 int16_t          pktLen, */
  /* 						 EkaFhMode        op); */
  
  /* void                  pushUdpPkt2Q(const uint8_t* pkt,  */
  /* 				     uint           pktLen); */

  /* int                   processFromQ(const EfhRunCtx* pEfhRunCtx); */


  /* int                   closeIncrementalGap(EfhCtx*           pEfhCtx,  */
  /* 					   const EfhRunCtx*  pEfhRunCtx,  */
  /* 					   uint64_t          startSeq, */
  /* 					   uint64_t          endSeq); */



  /* ##################################################################### */

  static const uint   SEC_HASH_SCALE = 17;

  using SecurityIdT = int32_t;
  using PriceT      = int64_t;
  using SizeT       = int32_t;

  using SequenceT   = uint32_t;
  using PriceLevetT = uint8_t;

  using FhSecurity  = EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>;
  using FhBook      = EkaFhTobBook<SEC_HASH_SCALE,EkaFhTobSecurity  <SecurityIdT, PriceT, SizeT>,SecurityIdT, PriceT, SizeT>;

  FhBook*   book = NULL;

  int       processedDefinitionMessages = 0;

};
#endif
