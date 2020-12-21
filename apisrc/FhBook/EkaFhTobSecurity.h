#ifndef _EKA_FH_TOB_SECURITY_H_
#define _EKA_FH_TOB_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"
#include "EkaFhSecurity.h"

/* ####################################################### */

template <class SecurityIdT, 
  class PriceT, 
  class SizeT>
  class EkaFhTobSecurity : public EkaFhSecurity {
 public:
    EkaFhTobSecurity() {}
    SecurityIdT      secId              = 0;
    
    SizeT	     bid_size           = 0;
    SizeT	     bid_o_size         = 0;
    SizeT	     bid_cust_size      = 0;
    SizeT	     bid_pro_cust_size  = 0;
    SizeT	     bid_bd_size        = 0;

    PriceT	     bid_price          = 0;
  
    SizeT	     ask_size           = 0;
    SizeT	     ask_o_size         = 0;
    SizeT	     ask_cust_size      = 0;
    SizeT	     ask_pro_cust_size  = 0;
    SizeT	     ask_bd_size        = 0;

    PriceT	     ask_price          = 0;

};
/* ####################################################### */

#endif
