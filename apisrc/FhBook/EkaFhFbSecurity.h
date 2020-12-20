#ifndef _EKA_FH_FB_SECURITY_H_
#define _EKA_FH_FB_SECURITY_H_

#include "EkaFhSecurity.h"

class EkaFhPlevel;

template <class SecurityIdT, 
  class OrderIdT, 
  class PriceT, 
  class SizeT>
  class EkaFhFbSecurity : public EkaFhSecurity {
 public:
    EkaFhFbSecurity() {}
    
    SecurityIdT      secId         = 0;
    
    uint             numBidPlevels = 0;
    uint             numAskPlevels = 0;
    
    EkaFhPlevel*     bid           = NULL;
    EkaFhPlevel*     ask           = NULL;
};

#endif
