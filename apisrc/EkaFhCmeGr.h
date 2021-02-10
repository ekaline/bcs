#ifndef _EKA_FH_CME_GR_H_
#define _EKA_FH_CME_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhCmeBook.h"
#include "EkaFhCmeSecurity.h"

#include "EkaFhPktQ.h"
#include "EkaFhPktQElem.h"

class EkaFhCmeGr : public EkaFhGroup {
 public:
  EkaFhCmeGr();
  virtual               ~EkaFhCmeGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
  				 const unsigned char* m,
  				 uint64_t sequence,
  				 EkaFhMode op) {
    return true;
  }

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

  bool                  processPkt(const EfhRunCtx* pEfhRunCtx,
				      const uint8_t*   pkt, 
				      int16_t          pktLen,
				      EkaFhMode        op);
  

  void                  pushPkt2Q(const uint8_t* pkt, 
				  uint16_t       pktSize,
				  uint64_t       sequence);
  
  int                   processPktFromQ(const EfhRunCtx* pEfhRunCtx);

  /* bool                  processUdpDefinitionsPkt(const EfhRunCtx* pEfhRunCtx, */
  /* 						 const uint8_t*   pkt,  */
  /* 						 int16_t          pktLen, */
  /* 						 EkaFhMode        op); */

  /* int                   closeIncrementalGap(EfhCtx*           pEfhCtx,  */
  /* 					   const EfhRunCtx*  pEfhRunCtx,  */
  /* 					   uint64_t          startSeq, */
  /* 					   uint64_t          endSeq); */



  /* ##################################################################### */

  static const uint   SEC_HASH_SCALE = 15;
  static const int    QELEM_SIZE     = 2048;
  static const int    MAX_ELEMS      = 1024 * 1024;

  using SecurityIdT = int32_t;
  using PriceT      = int64_t;
  using SizeT       = int32_t;

  using SequenceT   = uint32_t;
  using PriceLevetT = uint8_t;

  using FhSecurity  = EkaFhCmeSecurity  <
    PriceLevetT,
    SecurityIdT, 
    PriceT, 
    SizeT>;
  using FhBook      = EkaFhCmeBook      <
    SEC_HASH_SCALE,
    EkaFhCmeSecurity  <PriceLevetT,SecurityIdT, PriceT, SizeT>,
    SecurityIdT, 
    PriceT, 
    SizeT>;

  FhBook*   book       = NULL;

  using     PktElem    = EkaFhPktQElem<QELEM_SIZE>;
  using     PktQ       = EkaFhPktQ<QELEM_SIZE,MAX_ELEMS,PktElem>;
    
  PktQ*     pktQ       = NULL;
  int       processedDefinitionMessages = 0;

  volatile bool inGap  = false;
};
#endif
