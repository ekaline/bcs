#ifndef _EKA_FH_PLEVEL_H_
#define _EKA_FH_PLEVEL_H_

#include "EkaFhTypes.h"

/* ####################################################### */
inline std::string side2str(SideT side) {
  switch (side) {
  case SideT::BID : return std::string("BID");
  case SideT::ASK : return std::string("ASK");
  default: 
    return std::string("UNEXPECTED SIDE: ") + std::to_string((int)side);
  }
}
// ##########################################################

template <class PriceT, class SizeT>
  class alignas(64) EkaFhPlevel {
 public:
  EkaFhPlevel() {
    reset();
  }
  //----------------------------------------------------------
  inline bool isEmpty() {
    return cnt == 0;
  }
  //----------------------------------------------------------

  inline bool worsePriceThan(PriceT _price) {
    switch (side) {
    case SideT::BID : return (price < _price);
    case SideT::ASK : return (price > _price);
    default: on_error("Unexpected side %d",(int)side);
    }
    return false;
  }

  //----------------------------------------------------------
  inline int addSize (FhOrderType t, SizeT deltaSize) {
    SizeT* pLevelSize = getSizePtr(t);
    *pLevelSize += deltaSize;
    return *pLevelSize;
  }
  //----------------------------------------------------------
  inline int deductSize (FhOrderType t, SizeT deltaSize) {
    SizeT* pLevelSize = getSizePtr(t);
    if (*pLevelSize < deltaSize) {
      print("pLevelSize error");
      test_error("%s pLevelSize %d < deltaSize %d",
		 side2str(side).c_str(),(int)*pLevelSize, (int)deltaSize);
    }
    *pLevelSize -= deltaSize;
    return *pLevelSize;
  }

  //----------------------------------------------------------
  inline uint32_t get_total_size() {
    return cust_size + bd_size + cust_aon_size + bd_aon_size + other_size;
  }
  //----------------------------------------------------------

  inline uint32_t get_total_customer_size() {
    return cust_size + cust_aon_size;
  }
  //----------------------------------------------------------

  inline uint32_t get_total_aon_size() {
    return cust_aon_size + bd_aon_size;
  }
  //----------------------------------------------------------
  int print (const char* msg) {
    TEST_LOG("%32s: %s, price %6jd, cnt = %u,CUSTOMER = %3ju, BD = %3ju, CUSTOMER_AON = %3ju, BD_AON = %3ju, OTHER = %3ju",
	     msg,  
	     side2str(side).c_str(),
	     (int64_t)price,
	     cnt,
	     (uint64_t)cust_size,
	     (uint64_t)bd_size,
	     (uint64_t)cust_aon_size,
	     (uint64_t)bd_aon_size,
	     (uint64_t)other_size);
    return 0;
  }

  //----------------------------------------------------------
  inline void reset() {
    s        = NULL;
    top      = false;
    cnt      = 0;
    side     = SideT::UNINIT;
  
    price         = 0;

    cust_size     = 0;
    cust_aon_size = 0;
    bd_size       = 0;
    bd_aon_size   = 0;
    other_size    = 0;
  }
  //----------------------------------------------------------

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
 public:
  class EkaFhSecurity*     s             = NULL;
  bool            top           = false;
  uint            cnt           = 0;
  SideT           side          = SideT::UNINIT;
  
  EkaFhPlevel*    prev          = NULL;
  EkaFhPlevel*    next          = NULL;

  PriceT          price         = 0;

  SizeT	          cust_size     = 0;
  SizeT	          cust_aon_size = 0;
  SizeT	          bd_size       = 0;
  SizeT	          bd_aon_size   = 0;
  SizeT	          other_size    = 0;

};
// ##########################################################

#endif
