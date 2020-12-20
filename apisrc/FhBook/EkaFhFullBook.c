#include "EkaFhFullBook.h"
#include "EkaFhGroup.h"

//----------------------------------------------------------
/* template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT> */
/*   EkaFhFullBook::EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>(EkaDev* _dev, EkaFhGroup* _gr, EkaSource   _exch) :  */
/*   EkaFhBook (_dev,_gr,_exch) { */
  
/* } */

/* ####################################################### */
template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhSecurity* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::findSecurity(SecurityIdT secId) {
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhSecurity* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::subscribeSecurity(SecurityIdT     secId,
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

/* template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT> */
/*   void EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT> */
/*   ::init() { */
/*   EKA_LOG("OK"); */
/* } */
/* ####################################################### */

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  void EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::allocateResources() {
  for (auto i = 0; i < MAX_ORDERS; i++) ord[i]=NULL;
  //----------------------------------------------------------
  EKA_LOG("%s:%u: preallocating %u free orders",EKA_EXCH_DECODE(exch),gr->id,MAX_ORDERS);
  FhOrder** o = &orderFreeHead;
  for (uint i = 0; i < MAX_ORDERS; i++) {
    *o = new FhOrder();
    if (*o == NULL) on_error("constructing new Order failed");
    o = &((*o)->next);
  }
  //----------------------------------------------------------
  EKA_LOG("%s:%u: preallocating %u free Plevels",EKA_EXCH_DECODE(exch),gr->id,MAX_PLEVELS);
  FhPlevel** p = &plevelFreeHead;
  for (uint i = 0; i < MAX_PLEVELS; i++) {
    *p = new FhPlevel();
    if (*p == NULL) on_error("constructing new Plevel failed");
    p = &((*p)->next);
  }
  //----------------------------------------------------------
}

/* ####################################################### */
template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhPlevel* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::getNewPlevel() {
  if (numPlevels++ == MAX_PLEVELS) 
    on_error("%s:%u: out of preallocated FhPlevels: numPlevels=%u MAX_PLEVELS=%ju",
	     EKA_EXCH_DECODE(exch),gr->id,numPlevels,MAX_PLEVELS);

  if (numPlevels > maxNumPlevels) maxNumPlevels = numPlevels;
  FhPlevel* p = plevelFreeHead;
  if (p == NULL) on_error("p == NULL");
  plevelFreeHead = plevelFreeHead->next;
  p->reset();
  return p;
}

/* ####################################################### */
template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhOrder* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::getNewOrder() {
  if (++numOrders == MAX_ORDERS) 
    on_error("%s:%u: out of preallocated FhOrders: numOrders=%u MAX_ORDERS=%ju",
	     EKA_EXCH_DECODE(exch),gr->id,numOrders,MAX_ORDERS);

  if (numOrders > maxNumOrders) maxNumOrders = numOrders;
  FhOrder* o = orderFreeHead;
  if (o == NULL) on_error("o == NULL, numOrders=%u",numOrders);
  orderFreeHead = orderFreeHead->next;
  return o;
}
/* ####################################################### */

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  void EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>::
  releasePlevel(FhPlevel* p) {
  if (p == NULL) on_error("p == NULL");
  if (numPlevels-- == 0) on_error("numPlevels == 0 before releasing new element for GR%u",gr->id);
  p->next = plevelFreeHead;
  plevelFreeHead = p;
  p->reset();
}
/* ####################################################### */
template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  void EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>::
  releaseOrder(FhOrder* o) {
  if (o == NULL) on_error("o == NULL");
  if (numOrders-- == 0) on_error("numOrders == 0 before releasing new element for GR%u",gr->id);
  o->next       = orderFreeHead;
  orderFreeHead = o;
  o->orderId    = 0;
  o->size       = 0;
  o->plevel     = NULL; 
}
/* ####################################################### */

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  int EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::deleteOrder (FhOrder* o) {
  if (o == NULL) on_error("o == NULL for GR%u",gr->id);
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhPlevel* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::addOrder (FhSecurity*     s,
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  int EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::modifyOrder (FhOrder*    o,
		 PriceT      price,
		 SizeT       size) {
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  void EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::deleteOrderFromHash(OrderIdT orderId) {
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  void EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::addOrder2Hash (FhOrder* o) {
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhPlevel* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::findOrAddPlevel(FhSecurity* s, PriceT _price, SideT _side) {
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhPlevel* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::addPlevelAfterTob(FhPlevel** pTob, PriceT _price, SideT _side) {
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

template <const uint SCALE, const uint SEC_HASH_SCALE,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  EkaFhPlevel* EkaFhFullBook<SCALE, SEC_HASH_SCALE,SecurityIdT, OrderIdT, PriceT, SizeT>
  ::addPlevelAfterP(FhPlevel* p, PriceT _price) {
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

