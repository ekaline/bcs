#ifndef _EKA_FH_SECURITY_H_
#define _EKA_FH_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"

class EkaFhSecurity {
 protected:
  EkaFhSecurity() {}
 public:
  EfhSecurityType  type           = EfhSecurityType::kOpt;
  EkaFhSecurity*   next           = NULL;
  EfhTradeStatus   trading_action = EfhTradeStatus::kUninit;
  bool		   option_open    = false;

  uint             underlyingIdx  = -1;
};
#endif
