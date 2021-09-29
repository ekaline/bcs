#ifndef _EKA_FH_CME_GR_H_
#define _EKA_FH_CME_GR_H_

#include "EkaFhGroup.h"
#include "EkaFhCmeBook.h"
#include "EkaFhCmeSecurity.h"

#include "EkaFhPktQ.h"
#include "EkaFhPktQElem.h"

namespace Cme {
  struct MaturityMonthYear_T;
}

class EkaFhCmeGr : public EkaFhGroup {
public:
  virtual               ~EkaFhCmeGr() {};

  bool                  parseMsg(const EfhRunCtx* pEfhRunCtx,
  				 const unsigned char* m,
  				 uint64_t sequence,
  				 EkaFhMode op,std::chrono::high_resolution_clock::time_point startTime={}) {
    return true;
  }

  int                   bookInit();

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
  int                   createPktQ();

  bool                  processPkt(const EfhRunCtx* pEfhRunCtx,
				   const uint8_t*   pkt, 
				   int16_t          pktLen,
				   EkaFhMode        op);
  

  void                  pushPkt2Q(const uint8_t* pkt, 
				  uint16_t       pktSize,
				  uint64_t       sequence);
  
  int                   processFromQ(const EfhRunCtx* pEfhRunCtx);

  int                   closeSnapshotGap(EfhCtx*           pEfhCtx, 
					 const EfhRunCtx* pEfhRunCtx, 
					 uint64_t          sequence);


  /* ##################################################################### */
public:
  
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

  bool      snapshotClosed = false;
  uint64_t  firstLifeSeq = 0;
  int       processedSnapshotMessages = 0;

  volatile bool inGap  = false;

  enum class DefinitionsCycleState : int {
					  Init = 0,
					  InProgress,
					  Done
  };
  
  int vanillaOptionsDefinitions = 0;  
  int complexOptionsDefinitions = 0;
  int futuresDefinitions        = 0;
  
  DefinitionsCycleState vanillaOptionsDefinitionsState = DefinitionsCycleState::Init;
  DefinitionsCycleState complexOptionsDefinitionsState = DefinitionsCycleState::Init;
  DefinitionsCycleState futuresDefinitionsState        = DefinitionsCycleState::Init;

private:
  void getCMEProductTradeTime(const Cme::MaturityMonthYear_T* maturity,
                              const char* symbol,
                              uint32_t* iso8601Date,
                              time_t* time);

  int process_QuoteRequest39(const EfhRunCtx* pEfhRunCtx,
			     const uint8_t*   msg,
			     uint64_t         pktTime,
			     SequenceT        pktSeq);
  int process_MDIncrementalRefreshBook46(const EfhRunCtx* pEfhRunCtx,
					 const uint8_t*   msg,
					 uint64_t         pktTime,
					 SequenceT        pktSeq);
  int process_MDIncrementalRefreshTradeSummary48(const EfhRunCtx* pEfhRunCtx,
						 const uint8_t*   msg,
						 uint64_t         pktTime,
						 SequenceT        pktSeq);
  int process_SnapshotFullRefresh52(const EfhRunCtx* pEfhRunCtx,
				    const uint8_t*   msg,
				    uint64_t         pktTime,
				    SequenceT        pktSeq);
  int process_MDInstrumentDefinitionFuture54(const EfhRunCtx* pEfhRunCtx,
					     const uint8_t*   msg,
					     uint64_t         pktTime,
					     SequenceT        pktSeq);

  int process_MDInstrumentDefinitionOption55(const EfhRunCtx* pEfhRunCtx,
					     const uint8_t*   msg,
					     uint64_t         pktTime,
					     SequenceT        pktSeq);
  int process_MDInstrumentDefinitionSpread56(const EfhRunCtx* pEfhRunCtx,
					     const uint8_t*   msg,
					     uint64_t         pktTime,
					     SequenceT        pktSeq);
        
};
#endif
