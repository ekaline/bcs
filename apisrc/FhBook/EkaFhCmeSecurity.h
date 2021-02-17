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
  class SizeT>
  class EkaFhCmeSecurity : public EkaFhSecurity {
 public:
 EkaFhCmeSecurity(SecurityIdT     _secId,
		  EfhSecurityType _type,
		  EfhSecUserData  _userData,
		  uint64_t        _opaqueAttrA,
		  uint64_t        _opaqueAttrB) : 
    EkaFhSecurity(_type,_userData,_opaqueAttrA,_opaqueAttrB) {
      secId         = _secId;
      numBidPlevels = 0;
      numAskPlevels = 0;
      bid           = new EkaFhCmeBookSide<PriceLevetT,PriceT,SizeT>(SideT::BID);
      ask           = new EkaFhCmeBookSide<PriceLevetT,PriceT,SizeT>(SideT::ASK);
    }

    /* --------------------------------------------------------------- */
    inline bool newPlevel(SideT         side,
			 PriceLevetT   pLevelIdx,
			 PriceT        price,
			 SizeT         size) {
      switch (side) {
      case SideT::BID :
	if (bid == NULL) on_error("bid == NULL");
	return bid->newPlevel(pLevelIdx,price,size);
      case SideT::ASK :
	if (ask == NULL) on_error("ask == NULL");
	return ask->newPlevel(pLevelIdx,price,size);
      default:
	on_error("Unexpected side %d",(int)side);
      }
    }
    /* --------------------------------------------------------------- */
    inline bool changePlevel(SideT       side,
			    PriceLevetT pLevelIdx,
			    PriceT      price,
			    SizeT       size) {
      switch (side) {
      case SideT::BID :
	if (bid == NULL) on_error("bid == NULL");
	return bid->changePlevel(pLevelIdx,price,size);
      case SideT::ASK :
	if (ask == NULL) on_error("ask == NULL");
	return ask->changePlevel(pLevelIdx,price,size);
      default:
	on_error("Unexpected side %d",(int)side);
      }
    }
    /* --------------------------------------------------------------- */
    inline bool deletePlevel(SideT       side,
			    PriceLevetT pLevelIdx) {
      switch (side) {
      case SideT::BID :
	if (bid == NULL) on_error("bid == NULL");
	return bid->deletePlevel(pLevelIdx);
      case SideT::ASK :
	if (ask == NULL) on_error("ask == NULL");
	return ask->deletePlevel(pLevelIdx);
      default:
	on_error("Unexpected side %d",(int)side);
      }
    }
    /* --------------------------------------------------------------- */
    using BookSide  = EkaFhCmeBookSide  <PriceLevetT,PriceT,SizeT>;

    SecurityIdT      secId         = 0;
    EfhTradeStatus   tradeStatus   = EfhTradeStatus::kNormal;
    PriceLevetT      numBidPlevels = 0;
    PriceLevetT      numAskPlevels = 0;
    
    BookSide*        bid           = NULL;
    BookSide*        ask           = NULL;
};
/* ####################################################### */

#endif
