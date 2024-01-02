#ifndef _EKA_FH_TOB_SECURITY_H_
#define _EKA_FH_TOB_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"
#include "EkaFhSecurity.h"

/* ####################################################### */

template <class SecurityIdT,
  class PriceT,
  class SizeT>
class alignas(64) EkaFhTobSecurity : public EkaFhSecurity {
public:
  EkaFhTobSecurity() {}
  EkaFhTobSecurity(SecurityIdT     _secId,
		   EfhSecurityType _type,
		   EfhSecUserData  _userData,
		   uint64_t        _opaqueAttrA,
		   uint64_t        _opaqueAttrB) : 
    EkaFhSecurity(_type,_userData,_opaqueAttrA,_opaqueAttrB) {
    secId = _secId;
    reset();
  }

  virtual ~EkaFhTobSecurity() {};
  /* ####################################################### */
  inline int reset() {
    bid_size           = 0;
    bid_o_size         = 0;
    bid_cust_size      = 0;
    bid_pro_cust_size  = 0;
    bid_bd_size        = 0;

    bid_price          = 0;
  
    ask_size           = 0;
    ask_o_size         = 0;
    ask_cust_size      = 0;
    ask_pro_cust_size  = 0;
    ask_bd_size        = 0;

    ask_price          = 0;
    return 0;
  }

  /* ####################################################### */

  SecurityIdT secId             = 0;
  
  SizeT	     bid_size           = 0;
  SizeT	     bid_o_size         = 0;
  SizeT	     bid_cust_size      = 0;
  SizeT	     bid_pro_cust_size  = 0;
  SizeT	     bid_bd_size        = 0;

  PriceT     bid_price          = 0;
  
  SizeT	     ask_size           = 0;
  SizeT	     ask_o_size         = 0;
  SizeT	     ask_cust_size      = 0;
  SizeT	     ask_pro_cust_size  = 0;
  SizeT	     ask_bd_size        = 0;

  PriceT     ask_price          = 0;

};
#endif
