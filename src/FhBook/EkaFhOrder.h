#ifndef _EKA_FH_ORDER_H_
#define _EKA_FH_ORDER_H_

#include "EkaFhTypes.h"
#include "EkaFhPlevel.h"

// ###############################################################
template <class FhPlevel, class OrderIdT, class SizeT>
class alignas(64) EkaFhOrder{
 public:
    EkaFhOrder() {
      type    = FhOrderType::UNINIT;
      orderId = 0;
      size    = 0;
      plevel  = NULL;
      next    = NULL;
    }

  void reset() {
    type    = FhOrderType::UNINIT;
    orderId = 0;
    size    = 0;
    plevel  = NULL;
    next    = NULL;
  }
//--------------------------------------------------------------
 public:
  FhOrderType       type    = FhOrderType::UNINIT;
  SizeT             size    = 0;
  OrderIdT          orderId = 0;
  FhPlevel*         plevel  = NULL;
  EkaFhOrder*       next    = NULL;
};

#endif
