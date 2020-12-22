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
  class EkaFhFbSecurity : public EkaFhSecurity {
 public:
    EkaFhFbSecurity(SecurityIdT     _secId,
		    EfhSecurityType _type,
		    EfhSecUserData  _userData,
		    uint64_t        _opaqueAttrA,
		    uint64_t        _opaqueAttrB) : 
    EkaFhSecurity(_type,_userData,_opaqueAttrA,_opaqueAttrB) {
      secId = _secId;
    }
/* /\* --------------------------------------------------------------- *\/ */
/*     uint64_t  getTopPrice(SideT side) {  */
/*       switch (side) { */
/*       case SideT::BID : */
/* 	if (bid == NULL) return 0; */
/* 	return bid->price; */
/*       case SideT::ASK : */
/* 	if (ask == NULL) return 0; */
/* 	return ask->price; */
/*       default: */
/* 	on_error("Unexpected Side %d",(int)side); */
/*       } */
/*     } */
/* /\* --------------------------------------------------------------- *\/ */

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
