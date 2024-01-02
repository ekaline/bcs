#ifndef _EKA_FH_FB_SECURITY_H_
#define _EKA_FH_FB_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"
#include "EkaFhSecurity.h"
#include "EkaFhPlevel.h"

/* ####################################################### */

template <
class FhPlevel,
  class SecurityIdT, 
  class OrderIdT, 
  class PriceT, 
  class SizeT>
class alignas(64) EkaFhFbSecurity : public EkaFhSecurity {
 public:
  EkaFhFbSecurity() {}
  EkaFhFbSecurity(SecurityIdT     _secId,
		  EfhSecurityType _type,
		  EfhSecUserData  _userData,
		  uint64_t        _opaqueAttrA,
		  uint64_t        _opaqueAttrB) : 
    EkaFhSecurity(_type,_userData,_opaqueAttrA,_opaqueAttrB) {
    secId         = _secId;
    reset();
  }

  ~EkaFhFbSecurity() {};
  /* --------------------------------------------------------------- */

  inline bool crossedPrice() {
    if (option_open &&
	trading_action == EfhTradeStatus::kNormal &&
	bid &&
	ask &&
	bid->price >= ask->price)
      return true;
    return false;
  }

  inline uint64_t  getTopPrice(SideT side) {
    if (side == SideT::BID) return !bid ? 0 : (uint64_t) bid->price;
    if (side == SideT::ASK) return !ask ? 0 : (uint64_t) ask->price;
    return (uint64_t)-1;
  }
  inline int reset() {
    int cnt = 0;
    for (auto const& side : {bid, ask}) {
      auto p  = side;
      while (p) {
	auto n = p->next;
	p->reset();
	p = n;
	cnt++;
      }
    }
    numBidPlevels = 0;
    numAskPlevels = 0;
    bid           = NULL;
    ask           = NULL;
    return cnt;
  }
/* --------------------------------------------------------------- */

/*     uint32_t  getTopTotalSize(SideT side) {  */
/*       switch (side) { */
/*       case SideT::BID : */
/* 	if (bid == NULL) return 0; */
/* 	return bid->get_total_size(); */
/*       case SideT::ASK : */
/* 	if (ask == NULL) return 0; */
/* 	return ask->get_total_size(); */
/*       default: */
/* 	on_error("Unexpected Side %d",(int)side); */
/*       } */
/*       return 0; */
/*     } */
/* /\* --------------------------------------------------------------- *\/ */
/*   uint32_t  getTopTotalCustomerSize(SideT side) {  */
/*       switch (side) { */
/*       case SideT::BID : */
/* 	if (bid == NULL) return 0; */
/* 	return bid->get_total_customer_size(); */
/*       case SideT::ASK : */
/* 	if (ask == NULL) return 0; */
/* 	return ask->get_total_customer_size(); */
/*       default: */
/* 	on_error("Unexpected Side %d",(int)side); */
/*       } */
/*       return 0; */
/*     } */
/* --------------------------------------------------------------- */

    SecurityIdT      secId         = 0;
    
    uint             numBidPlevels = 0;
    uint             numAskPlevels = 0;
    
    FhPlevel*     bid           = NULL;
    FhPlevel*     ask           = NULL;
};
/* ####################################################### */

#endif
