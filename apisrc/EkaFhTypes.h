#ifndef _EKA_FH_TYPES_H_
#define _EKA_FH_TYPES_H_

#include <inttypes.h>

enum class SideT : int {
  UNINIT = 0,
    BID = 1,
    ASK = 2,
    OTHER = 3
    };

enum class FhOrderType : int {
  UNINIT = 0,
    CUSTOMER, 
    BD, 
    CUSTOMER_AON,
    BD_AON,
    OTHER
    };

#endif
