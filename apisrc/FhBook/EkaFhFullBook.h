#ifndef _EKA_FH_FULL_BOOK_H_
#define _EKA_FH_FULL_BOOK_H_

#include "EkaFhBook.h"
#include "EkaFhFbSecurity.h"
#include "EkaFhPlevel.h"
#include "EkaFhOrder.h"

/* ####################################################### */
template <class OrderIdT>
inline uint32_t getOrderHashIdx(OrderIdT orderId, uint64_t MAX_ORDERS, uint ORDERS_HASH_MASK) {
  if ((orderId & ORDERS_HASH_MASK) > MAX_ORDERS) 
    on_error("orderId (%jx) & ORDERS_HASH_MASK (%x) (=%jx)  > MAX_ORDERS(%jx)",
	      
	      orderId,
	      ORDERS_HASH_MASK,
	      orderId & ORDERS_HASH_MASK,
	      MAX_ORDERS
	      );
  return orderId & ORDERS_HASH_MASK;
}
/* ####################################################### */

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  class EkaFhFullBook : 
  public EkaFhBook  {
 public:
  using FhSecurity   = EkaFhFbSecurity  <SecurityIdT, OrderIdT, PriceT, SizeT>;
  using FhPlevel     = EkaFhFbPlevel    <                       PriceT, SizeT>;
  using FhOrder      = EkaFhFbOrder     <             OrderIdT,         SizeT>;

 EkaFhFullBook(EkaDev* _dev, EkaLSI _grId, EkaSource   _exch) 
   : EkaFhBook (_dev,_grId,_exch) {}
 

  void            init() {
    EKA_LOG("OK");
    allocateResources();
  }
/* ####################################################### */

  EkaFhSecurity*  findSecurity(SecurityIdT secId) {
    uint32_t index =  secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES) on_error("index = %u >= SEC_HASH_LINES %u",index,SEC_HASH_LINES);
    if (sec[index] == NULL) return NULL;

    FhSecurity* sp = sec[index];

    while (sp != NULL) {
      if (sp->secId == secId) return sp;
      sp = sp->next;
    }

    return NULL;
  }
/* ####################################################### */

  EkaFhSecurity*  subscribeSecurity(SecurityIdT     secId,
				    EfhSecurityType type,
				    EfhSecUserData  userData,
				    uint64_t        opaqueAttrA,
				    uint64_t        opaqueAttrB) {
  FhSecurity* s = new FhSecurity(secId,type,userData,opaqueAttrA,opaqueAttrB);
  if (s == NULL) on_error("s == NULL, new FhSecurity failed");
  
  uint32_t index = secId & SEC_HASH_MASK;
  if (index >= SEC_HASH_LINES) 
    on_error("index = %u >= SEC_HASH_LINES %u",index,SEC_HASH_LINES);

  if (sec[index] == NULL) {
    sec[index] = s; // empty bucket
  } else {
    FhSecurity *sp = sec[index];
    while (sp->next != NULL) sp = sp->next;
    sp->next = s;
  }
  numSecurities++;
  return s;
}

/* ####################################################### */

  EkaFhPlevel*    addOrder(FhSecurity*     s,
			   OrderIdT        _orderId,
			   FhOrderType     _type, 
			   PriceT          _price, 
			   SizeT           _size, 
			   SideT           _side) {
  FhOrder* o = getNewOrder();
  o->reset();

  o->plevel = findOrAddPlevel(s,_price,_side);
  o->plevel->addSize(o->type,o->size);
  o->plevel->cnt++;
  addOrder2Hash (o);
  return o->plevel;
}

/* ####################################################### */

  int             modifyOrder(FhOrder* o, PriceT price,SizeT size) {
  FhPlevel* p = o->plevel;
  if (p->price == price) {
    p->deductSize(o->type,o->size);
    p->addSize   (o->type,   size);
  } else {
    FhSecurity* s = p->security;
    SideT    side = p->side;
    p->deductSize(o->type,o->size);
    p->cnt--;
    if (p->isEmpty()) deletePlevel(p);
    o->size   = size;
    o->plevel = findOrAddPlevel(s,price,side);
  }
  return 0;
}
/* ####################################################### */
  int             deleteOrder(FhOrder* o) {
  if (o == NULL) on_error("o == NULL for GR%u",grId);
  FhPlevel* p = o->plevel;
  if (p == NULL) on_error("p == NULL");
  
  p->deductSize(o->type,o->size);
  p->cnt--;
  if (p->isEmpty()) deletePlevel(p);

  deleteOrderFromHash (o->orderId);
  release_order_element(o);
  return 0;
}
/* ####################################################### */
  SizeT           reduceOrderSize(FhOrder* o, SizeT deltaSize) {
    if (o->size < deltaSize) 
      on_error("o->size %d < deltaSize %d",(int)o->size, (int)deltaSize);
    o->size -= deltaSize;
    return o->size;
  }
/* ####################################################### */
  int             invalidate() {
    return 0; // TBD
  }
/* ####################################################### */

 private:
/* ####################################################### */
  EkaFhPlevel*    getNewPlevel() {
  if (numPlevels++ == MAX_PLEVELS) 
    on_error("%s:%u: out of preallocated FhPlevels: numPlevels=%u MAX_PLEVELS=%ju",
	     EKA_EXCH_DECODE(exch),grId,numPlevels,MAX_PLEVELS);

  if (numPlevels > maxNumPlevels) maxNumPlevels = numPlevels;
  FhPlevel* p = plevelFreeHead;
  if (p == NULL) on_error("p == NULL");
  plevelFreeHead = plevelFreeHead->next;
  p->reset();
  return p;
}
/* ####################################################### */
  EkaFhOrder*     getNewOrder() {
  if (++numOrders == MAX_ORDERS) 
    on_error("%s:%u: out of preallocated FhOrders: numOrders=%u MAX_ORDERS=%ju",
	     EKA_EXCH_DECODE(exch),grId,numOrders,MAX_ORDERS);

  if (numOrders > maxNumOrders) maxNumOrders = numOrders;
  FhOrder* o = orderFreeHead;
  if (o == NULL) on_error("o == NULL, numOrders=%u",numOrders);
  orderFreeHead = orderFreeHead->next;
  return o;
}
/* ####################################################### */
  void            releasePlevel(FhPlevel* p) {
  if (p == NULL) on_error("p == NULL");
  if (numPlevels-- == 0) on_error("numPlevels == 0 before releasing new element for GR%u",grId);
  p->next = plevelFreeHead;
  plevelFreeHead = p;
  p->reset();
}
/* ####################################################### */
  void            releaseOrder(FhOrder* o) {
  if (o == NULL) on_error("o == NULL");
  if (numOrders-- == 0) on_error("numOrders == 0 before releasing new element for GR%u",grId);
  o->next       = orderFreeHead;
  orderFreeHead = o;
  o->orderId    = 0;
  o->size       = 0;
  o->plevel     = NULL; 
}
/* ####################################################### */
  void            deleteOrderFromHash(OrderIdT orderId) {
  uint32_t index = getOrderHashIdx<OrderIdT>(orderId,MAX_ORDERS,ORDERS_HASH_MASK);
  if (ord[index] == NULL) on_error("ord[%u] == NULL",index);

  if ((ord[index]->orderId == orderId) && (ord[index]->next == NULL)) {
    ord[index] = NULL;
  } else if ((ord[index]->orderId == orderId) && (ord[index]->next != NULL)) {
    ord[index] = ord[index]->next;
  } else {
    FhOrder* c = ord[index];
    while (c->next != NULL) {
      if (c->next->orderId == orderId) {
	c->next = c->next->next;
	return;
      }
      c = c->next;
    }
  }
}
/* ####################################################### */
  void            addOrder2Hash (FhOrder* o) {
  if (o == NULL) on_error("o==NULL");
#ifdef INTERNAL_DEBUG
  EKA_LOG("o->orderId=%ju",o->orderId);
#endif
  uint64_t orderId = o->orderId;

  uint32_t index = getOrderHashIdx<OrderIdT>(orderId,MAX_ORDERS,ORDERS_HASH_MASK);
  FhOrder** curr = &(ord[index]);
  o->next = NULL;

  while (*curr != NULL) {
    if ((*curr)->orderId == orderId) 
      on_error("adding existing orderId %ju at index %u",orderId,index);
    curr = &((*curr)->next);
  }
  *curr = o;
  
  return;
}
/* ####################################################### */
  EkaFhPlevel* findOrAddPlevel (FhSecurity* s,   
				PriceT     _price, 
				SideT      _side) {
  FhPlevel** pTob = NULL;
  switch (_side) {
  case SideT::BID :
    pTob = &s->bid;
    break;
  case SideT::ASK :
    pTob = &s->ask;
    break;
  default:
    on_error("Unexpected side = %d",(int)_side);
  }

  if (*pTob == NULL || (*pTob)->worsePriceThan(_price))
    return addPlevelAfterTob(pTob, _price, _side);

  for (FhPlevel* p = *pTob; p != NULL; p = p->next) {
    if (p->price == _price) return p;
    if (p->next == NULL || p->next->worsePriceThan(_price))
      return addPlevelAfterP(p,_price);
  }
  on_error("place to insert Plevel not found");
  return NULL;
}
/* ####################################################### */
  EkaFhPlevel* addPlevelAfterTob(FhPlevel** pTob, 
				 PriceT     _price, 
				 SideT      _side) {
    FhPlevel* newP = getNewPlevel();
    if (newP == NULL) on_error("newP == NULL");
    newP->price = _price;
    newP->side  = _side;

    FhPlevel* tob = *pTob;

    newP->top = true;
    newP->next = tob;
    newP->prev = NULL;
    if (tob != NULL) tob->prev = newP;
    if (tob != NULL) tob->top  = false;
    *pTob = newP;

    return newP;
  }
/* ####################################################### */
  EkaFhPlevel*    addPlevelAfterP  (FhPlevel* p,     PriceT _price) {
  FhPlevel* newP = getNewPlevel();
  if (newP == NULL) on_error("newP == NULL");
  newP->setPrice(_price);
  newP->side = p->side;

  newP->top  = false;
  newP->next = p->next;
  newP->prev = p;
  if (newP->next != NULL) newP->next->prev = newP;
  p->next = newP;

  return newP;
}
/* ####################################################### */
  void            allocateResources() {
  for (uint i = 0; i < MAX_ORDERS; i++) ord[i]=NULL;
  //----------------------------------------------------------
  EKA_LOG("%s:%u: preallocating %u free orders",EKA_EXCH_DECODE(exch),grId,MAX_ORDERS);
  EkaFhOrder** o = (EkaFhOrder**)&orderFreeHead;
  for (uint i = 0; i < MAX_ORDERS; i++) {
    *o = new FhOrder();
    if (*o == NULL) on_error("constructing new Order failed");
    o = &(*o)->next;
  }
  //----------------------------------------------------------
  EKA_LOG("%s:%u: preallocating %u free Plevels",EKA_EXCH_DECODE(exch),grId,MAX_PLEVELS);
  EkaFhPlevel** p = (EkaFhPlevel**)&plevelFreeHead;
  for (uint i = 0; i < MAX_PLEVELS; i++) {
    *p = new FhPlevel();
    if (*p == NULL) on_error("constructing new Plevel failed");
    p = &(*p)->next;
  }
  //----------------------------------------------------------
}
/* ####################################################### */
  //----------------------------------------------------------

 public:
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

  static const uint64_t MAX_ORDERS       = (1 << SCALE);            
  static const uint64_t MAX_PLEVELS      = (1 << (SCALE - 1));      
  static const uint64_t ORDERS_HASH_MASK = ((1 << (SCALE - 3)) - 1);

  static const uint64_t SEC_HASH_LINES = 0x1 << SEC_HASH_SCALE;
  static const uint64_t SEC_HASH_MASK  = (0x1 << SEC_HASH_SCALE) - 1;

  EkaFhSecurity*  sec[SEC_HASH_LINES] = {}; // array of pointers to Securities
  FhOrder*        ord[MAX_ORDERS] = {};
  };

#endif
