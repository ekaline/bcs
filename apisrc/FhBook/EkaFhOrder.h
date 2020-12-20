#ifndef _EKA_FH_ORDER_H_
#define _EKA_FH_ORDER_H_

#include "EkaFhTypes.h"

class EkaFhOrder {
 protected:
  EkaFhOrder() {
    type    = FhOrderType::UNINIT;
    plevel  = NULL;
    next    = NULL;
  };
//--------------------------------------------------------------

 public:
  FhOrderType        type    = FhOrderType::UNINIT;
  class EkaFhPlevel* plevel  = NULL;
  class EkaFhOrder*  next    = NULL;
};


// ###############################################################
template <class OrderIdT, class SizeT>
  class EkaFhFbOrder : public EkaFhOrder {
 public:
    EkaFhFbOrder() {
      orderId = 0;
      size    = 0;
    }

  void reset() {
    orderId = 0;
    type    = FhOrderType::UNINIT;
    size    = 0;
    plevel  = NULL;
    next    = NULL;
  }
//--------------------------------------------------------------
 public:
  OrderIdT          orderId = 0;
  SizeT             size    = 0;
};

#endif
