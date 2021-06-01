#ifndef _EKA_FH_SECURITY_H_
#define _EKA_FH_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"

class FhPlevel;

/* ####################################################### */

class EkaFhSecurity {
 protected:
  EkaFhSecurity(EfhSecurityType _type,
		EfhSecUserData  _userData,
		uint64_t        _opaqueAttrA,
		uint64_t        _opaqueAttrB) {
    type        = _type;
    efhUserData = _userData;
    opaqueAttrA = _opaqueAttrA;
    opaqueAttrB = _opaqueAttrB;

    underlyingIdx = (uint)opaqueAttrB;
}
/* --------------------------------------------------------------- */

 public:
  virtual uint64_t  getTopPrice(SideT side) { return 0;}
  virtual uint32_t  getTopTotalSize(SideT side) { return 0;}
  virtual uint32_t  getTopTotalCustomerSize(SideT side) { return 0;}

/* --------------------------------------------------------------- */

  uint64_t          efhUserData    = 0; // to be returned per TOB update
  EfhSecurityType   type           = EfhSecurityType::kOption;
  EkaFhSecurity*    next           = NULL;
  EfhTradeStatus    trading_action = EfhTradeStatus::kUninit;
  bool              option_open    = false;

  uint64_t          opaqueAttrA    = 0;
  uint64_t          opaqueAttrB    = 0;

  uint              underlyingIdx  = -1;

  uint64_t          bid_ts         = 0;
  uint64_t          ask_ts         = 0;

};

#endif
