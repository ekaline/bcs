#ifndef _EKA_FH_PLEVEL_H_
#define _EKA_FH_PLEVEL_H_

template <class FhSecurity, class PriceT, class SizeT, class SideT>
  class EkaFhPlevel {
 public:
  bool worsePriceThan(PriceT _price) {
    if (side == SideT::BID) return (price < _rice);
    return (price > _price);
  }

  FhSecurity*     security = NULL;
  PriceT          price    = 0;
  SizeT           size     = 0;
  SideT           side     = SideT::Invalid;
  bool            top      = false;
  uint            cnt      = 0;
  
};
#endif
