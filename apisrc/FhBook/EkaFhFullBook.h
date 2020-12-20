#ifndef _EKA_FH_FULL_BOOK_H_
#define _EKA_FH_FULL_BOOK_H_

#include "EkaFhBook.h"
#include "EkaFhFbSecurity.h"
#include "EkaFhPlevel.h"
#include "EkaFhOrder.h"

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  class EkaFhFullBook : 
  public EkaFhBook  {
 public:
  using FhSecurity   = EkaFhFbSecurity  <SecurityIdT, OrderIdT, PriceT, SizeT>;
  using FhPlevel     = EkaFhFbPlevel    <                       PriceT, SizeT>;
  using FhOrder      = EkaFhFbOrder     <             OrderIdT,         SizeT>;

 EkaFhFullBook(EkaDev* _dev, EkaFhGroup* _gr, EkaSource   _exch) 
   : EkaFhBook (_dev,_gr,_exch) {}
 

  int             init();

  EkaFhSecurity*     findSecurity(SecurityIdT secId);

  EkaFhSecurity*     subscribeSecurity(SecurityIdT     secId,
				       EfhSecurityType type,
				       EfhSecUserData  userData,
				       uint64_t        opaqueAttrA,
				       uint64_t        opaqueAttrB);
  EkaFhPlevel*    addOrder(FhSecurity*     s,
			   OrderIdT        _orderId,
			   FhOrderType     _type, 
			   PriceT          _price, 
			   SizeT           _size, 
			   SideT           _side);
  //    int             replaceOrder();
  int             modifyOrder(FhOrder* o, PriceT price,SizeT size);
  int             deleteOrder(FhOrder* o);
  SizeT           reduceOrderSize(FhOrder* o, SizeT deltaSize);
  int             invalidate();

 private:
  EkaFhPlevel*    getNewPlevel();
  EkaFhOrder*     getNewOrder();
  void            releasePlevel(FhPlevel* p);
  void            releaseOrder(FhOrder* o);
  void            deleteOrderFromHash(OrderIdT orderId);
  void            addOrder2Hash (FhOrder* o);
  EkaFhPlevel*    findOrAddPlevel  (FhSecurity* s,   PriceT _price, SideT _side);
  EkaFhPlevel*    addPlevelAfterTob(FhPlevel** pTob, PriceT _price, SideT _side);
  EkaFhPlevel*    addPlevelAfterP  (FhPlevel* p,     PriceT _price);
  //----------------------------------------------------------

 public:
  EkaFhOrder*     orderFreeHead = NULL; // head pointer to free list of preallocated FhOrder elements
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

  static const uint64_t MAX_ORDERS       = (1 << SCALE);            
  static const uint64_t MAX_PLEVELS      = (1 << (SCALE - 1));      
  static const uint64_t ORDERS_HASH_MASK = ((1 << (SCALE - 3)) - 1);

  static const uint64_t SEC_HASH_LINES = 0x1 << SEC_HASH_SCALE;
  static const uint64_t SEC_HASH_MASK  = (0x1 << SEC_HASH_SCALE) - 1;

  EkaFhSecurity*  sec[SEC_HASH_LINES] = {}; // array of pointers to Securities
  FhOrder*        ord[MAX_ORDERS] = {};
  };

#endif
