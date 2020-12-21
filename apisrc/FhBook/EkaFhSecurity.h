#ifndef _EKA_FH_FB_SECURITY_H_
#define _EKA_FH_FB_SECURITY_H_

#include "EkaFhTypes.h"
#include "Efh.h"

class FhPlevel;

/* ####################################################### */

class EkaFhSecurity {
 protected:
  EkaFhSecurity() {}
 public:
  uint64_t         efhUserData    = 0; // to be returned per TOB update
  EfhSecurityType  type           = EfhSecurityType::kOpt;
  EkaFhSecurity*   next           = NULL;
  EfhTradeStatus   trading_action = EfhTradeStatus::kUninit;
  bool             option_open    = false;

  uint             underlyingIdx  = -1;

  uint64_t         bid_ts         = 0;
  uint64_t         ask_ts         = 0;

};

#endif
