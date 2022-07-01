#ifndef _EKA_FH_CME_SECURITY_H_
#define _EKA_FH_CME_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"
#include "EkaFhSecurity.h"
#include "EkaFhCmeBookSide.h"

/* ####################################################### */

template <
  class PriceLevetT,
  class SecurityIdT, 
  class PriceT, 
  class SizeT,
  class SequenceT>
class EkaFhCmeSecurity : public EkaFhSecurity {
public:
  EkaFhCmeSecurity(SecurityIdT     _secId,
		   EfhSecurityType _type,
		   EfhSecUserData  _userData,
		   uint64_t        _opaqueAttrA,
		   uint64_t        _opaqueAttrB) : 
    EkaFhSecurity(_type,_userData,_opaqueAttrA,_opaqueAttrB) {
    secId         = _secId;
    //    numBidPlevels = 0;
    //    numAskPlevels = 0;
    bid           = new EkaFhCmeBookSide<PriceLevetT,PriceT,SizeT>
      (SideT::BID,getActivePriceLevels());
    ask           = new EkaFhCmeBookSide<PriceLevetT,PriceT,SizeT>
      (SideT::ASK,getActivePriceLevels());
  }

  uint64_t getFinalPriceFactor() const noexcept { return this->opaqueAttrA; }
  uint64_t getActivePriceLevels() const noexcept { return this->opaqueAttrB; }

  /* --------------------------------------------------------------- */
  inline bool newPlevel(SideT   side,
			uint8_t pLevelIdx,
			PriceT  price,
			SizeT   size) {
    switch (side) {
    case SideT::BID :
      if (!bid) on_error("!bid");
      return bid->newPlvl(pLevelIdx,price,size);
    case SideT::ASK :
      if (!ask) on_error("!ask");
      return ask->newPlvl(pLevelIdx,price,size);
    default:
      on_error("Unexpected side %d",(int)side);
    }
  }
  /* --------------------------------------------------------------- */
  inline bool changePlevel(SideT   side,
			   uint8_t pLevelIdx,
			   PriceT  price,
			   SizeT   size) {
    switch (side) {
    case SideT::BID :
      if (!bid) on_error("!bid");
      return bid->changePlvl(pLevelIdx,price,size);
    case SideT::ASK :
      if (!ask) on_error("!ask");
      return ask->changePlvl(pLevelIdx,price,size);
    default:
      on_error("Unexpected side %d",(int)side);
    }
  }
  /* --------------------------------------------------------------- */
  inline bool deletePlevel(SideT   side,
			   uint8_t pLevelIdx) {
    switch (side) {
    case SideT::BID :
      if (!bid) on_error("!bid");
      return bid->deletePlvl(pLevelIdx);
    case SideT::ASK :
      if (!ask) on_error("!ask");
      return ask->deletePlvl(pLevelIdx);
    default:
      on_error("Unexpected side %d",(int)side);
    }
  }
  /* --------------------------------------------------------------- */
  inline void printSecurityBook() {
    for (auto const& testSide : {ask,bid})
      testSide->printSide();
  }
  /* --------------------------------------------------------------- */

  inline bool checkBookIntegrity() {
    for (auto const& testSide : {bid, ask}) {
      if (!testSide->checkIntegrity()) {
	// TEST_LOG("%s side checkIntegrity failed",
	// 	 testSide->getSide() == SideT::BID ? "BID" : "ASK");
	// testSide->printSide();
	return false;
      }
    }
    return true;
  }
  /* --------------------------------------------------------------- */

  inline int reset() {
    for (auto const& testSide : {bid, ask})
      testSide->resetSide();
    // ChannelReset4 shouldn't change tradeStatus 
    //    tradeStatus   = EfhTradeStatus::kClosed;
    lastMsgSeqNumProcessed = 0;
    return 0;
  }
  /* ####################################################### */

  using BookSide  = EkaFhCmeBookSide  <PriceLevetT,PriceT,SizeT>;

  SecurityIdT      secId         = 0;
  EfhTradeStatus   tradeStatus   = EfhTradeStatus::kNormal;
  // PriceLevetT      numBidPlevels = 0;
  // PriceLevetT      numAskPlevels = 0;
    
  BookSide*        bid           = NULL;
  BookSide*        ask           = NULL;

  SequenceT        lastMsgSeqNumProcessed = 0;
};

#endif
