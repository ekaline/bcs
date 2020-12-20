#ifndef _EKA_FH_PLEVEL_H_
#define _EKA_FH_PLEVEL_H_

#include "EkaFhTypes.h"

class EkaFhPlevel {
 protected:
  EkaFhPlevel() {
    security = NULL;
    top      = false;
    cnt      = 0;
  }
  //----------------------------------------------------------

 public:
  EkaFhSecurity*  security = NULL;
  bool            top      = false;
  uint            cnt      = 0;
  
};

// ##########################################################
template <class PriceT, class SizeT>
  class EkaFhFbPlevel : public EkaFhPlevel {
 public:
  //----------------------------------------------------------
  inline bool isEmpty() {
    return cnt == 1;
  }
  //----------------------------------------------------------

  bool worsePriceThan(PriceT _price) {
    switch (side) {
    case SideT::BID : return (price < _price);
    case SideT::ASK : return (price > _price);
    default: on_error("Unexpected side %d",(int)side);
    }
    return false;
  }

 private:
  //----------------------------------------------------------
  inline SizeT* getSizePtr(FhOrderType t) {
    switch (t) {
    case FhOrderType::CUSTOMER :
      return &cust_size;
    case FhOrderType::BD :
      return &bd_size;
    case FhOrderType::CUSTOMER_AON :
      return &cust_aon_size;
    case FhOrderType::BD_AON :
      return &bd_aon_size;
    case FhOrderType::OTHER :
      return &other_size;
    default:
      on_error("Unexpected Order Type %d",(int)t);
    }
  }
  //----------------------------------------------------------
  inline SizeT addSize (FhOrderType t, SizeT deltaSize) {
    SizeT* pLevelSize = getSizePtr(t);
    *pLevelSize += deltaSize;
    return *pLevelSize;
  }
  //----------------------------------------------------------
  inline SizeT deductSize (FhOrderType t, SizeT deltaSize) {
    SizeT* pLevelSize = getSizePtr(t);
    if (*pLevelSize < deltaSize) 
      on_error("pLevelSize %d < deltaSize %d",(int)*pLevelSize, (int)deltaSize);
    *pLevelSize -= deltaSize;
    return *pLevelSize;
  }
  //----------------------------------------------------------

 public:

  PriceT          price    = 0;
  //  SizeT           size     = 0;
  SideT           side     = SideT::UNINIT;

  SizeT	          cust_size     = 0;
  SizeT	          cust_aon_size = 0;
  SizeT	          bd_size       = 0;
  SizeT	          bd_aon_size   = 0;
  SizeT	          other_size    = 0;

};
#endif
