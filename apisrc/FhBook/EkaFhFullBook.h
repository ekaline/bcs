#ifndef _EKA_FH_FULL_BOOK_H_
#define _EKA_FH_FULL_BOOK_H_

#include "EkaFhBook.h"

class EkaFhFullBook : 
public EkaFhBook <class FhSecurityT, class SecurityIdT, class OrderIdT, class PriceT, class SizeT>  {
 public:
  using FhPriceLevel = EkaFhPriceLevel<FhSecurityT,SecurityIdT,OrderIdT,PriceT,SizeT>;

  int             initPlevelsAndOrders();
  FhPriceLevel*   addOrder();
  int             replaceOrder();
  int             modifyOrder();
  int             deleteOrder();

  int             invalidate();

  //----------------------------------------------------------

  FhOrder*        orderFreeHead = NULL; // head pointer to free list of preallocated FhOrder elements
  uint            numOrders     = 0;    // counter of currently used orders (for statistics only)
  uint            maxNumOrders  = 0;    // highest value of numOrders ever reached (for statistics only)

  FhPlevel*       plevelFreeHead = NULL; // head pointer to free list of preallocated FhPlevel elements
  uint            numPlevels     = 0;    // counter of currently used plevels (for statistics only)
  uint            maxNumPlevels  = 0;    // highest value of numPlevels ever reached (for statistics only)

 private:
  static const uint     BOOK_SCALE  = 22; //
  // for SCALE = 22 (BATS): 
  //         MAX_ORDERS       = 1M
  //         MAX_PLEVELS      = 0.5 M
  //         ORDERS_HASH_MASK = 0x7FFF

  // for SCALE = 24 (NOM): 
  //         MAX_ORDERS       = 8M
  //         MAX_PLEVELS      = 4 M
  //         ORDERS_HASH_MASK = 0xFFFF

  const uint64_t MAX_ORDERS       = (1 << SCALE);            
  const uint64_t MAX_PLEVELS      = (1 << (SCALE - 1));      
  const uint64_t ORDERS_HASH_MASK = ((1 << (SCALE - 3)) - 1);

  FhOrder*       ord[MAX_ORDERS] = {};
};

#endif
