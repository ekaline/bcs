#include "EkaFhFullBook.h"

/* ####################################################### */
inline uint32_t getOrderHashIdx (OrderIdT orderId, uint64_t MAX_ORDERS, uint ORDERS_HASH_MASK) {
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
int EkaFhFullBook::init() {
  for (auto i = 0; i < MAX_ORDERS; i++) ord[i]=NULL;
  //----------------------------------------------------------
  EKA_LOG("%s:%u: preallocating %u free orders",EKA_EXCH_DECODE(exch),id,MAX_ORDERS);
  FhOrder** o = &orderFreeHead;
  for (uint i = 0; i < getMaxOrders(); i++) {
    *o = new FhOrder();
    if (*o == NULL) on_error("constructing new Order failed");
    o = &((*o)->next);
  }
  //----------------------------------------------------------
  EKA_LOG("%s:%u: preallocating %u free Plevels",EKA_EXCH_DECODE(exch),id,MAX_PLEVELS);
  FhPlevel** p = &plevelFreeHead;
  for (uint i = 0; i < getMaxPlevels();i++) {
    *p = new FhPlevel();
    if (*p == NULL) on_error("constructing new Plevel failed");
    p = &((*p)->next);
  }
  //----------------------------------------------------------
  return 0;
}

/* ####################################################### */
FhPlevel* EkaFhFullBook::getNewPlevel() {
  if (numPlevels++ == MAX_PLEVELS) 
    on_error("%s:%u: out of preallocated FhPlevels: numPlevels=%u MAX_PLEVELS=%ju",
	     EKA_EXCH_DECODE(exch),id,numPlevels,MAX_PLEVELS);

  if (numPlevels > maxNumPlevels) maxNumPlevels = numPlevels;
  FhPlevel* p = plevelFreeHead;
  if (p == NULL) on_error("p == NULL");
  plevelFreeHead = plevelFreeHead->next;
  p->reset();
  return p;
}

/* ####################################################### */
FhOrder* EkaFhFullBook::getNewOrder() {
  if (++numOrders == MAX_ORDERS) 
    on_error("%s:%u: out of preallocated FhOrders: numOrders=%u MAX_ORDERS=%ju",
	     EKA_EXCH_DECODE(exch),id,numOrders,MAX_ORDERS);

  if (numOrders > maxNumOrders) maxNumOrders = numOrders;
  FhOrder* o = orderFreeHead;
  if (o == NULL) on_error("o == NULL, numOrders=%u",numOrders);
  orderFreeHead = orderFreeHead->next;
  return o;
}
/* ####################################################### */

void EkaFhFullBook::releasePlevel(FhPlevel* p) {
  if (p == NULL) on_error("p == NULL");
  if (numPlevels-- == 0) on_error("numPlevels == 0 before releasing new element for GR%u",id);
  p->next = plevelFreeHead;
  plevelFreeHead = p;
  p->reset();
}
/* ####################################################### */
void EkaFhFullBook::releaseOrder(FhOrder* o) {
  if (o == NULL) on_error("o == NULL");
  if (numOrders-- == 0) on_error("numOrders == 0 before releasing new element for GR%u",id);
  o->next       = orderFreeHead;
  orderFreeHead = o;
  o->orderId   = 0;
  o->size       = 0;
  o->plevel     = NULL; 
}
/* ####################################################### */

int EkaFhFullBook::deleteOrder (FhOrder* o) {
  if (o == NULL) on_error("o == NULL for GR%u",id);
  FhPlevel* p = o->plevel;
  if (p == NULL) on_error("p == NULL");

  p->deleteMyOrder(o->size,o->type);
  if (p->isEmpty()) deletePlevel(p);

  deleteOrderFromHash (o->orderId);
  release_order_element(o);
  return err;
}

/* ####################################################### */

FhPlevel* EkaFhFullBook::addOrder (FhSecurity*     s,
				   OrderIdT        _orderId,
				   FhOrder::type_t _type, 
				   PriceT          _price, 
				   SizeT           _size, 
				   SideT           _side) {
#ifdef INTERNAL_DEBUG
  EKA_LOG("security_id=%d, orderId=%ju, price=%d, size=%d, side=%c",security_id, orderId,price,size,side);
#endif
  if (s == NULL) return NULL;
  FhOrder* o = getNewOrder();
  if (o == NULL) on_error("FhOrder* o = NULL");

  o->orderId = _orderId;
  o->type    = _type;
  o->size    = _size;
  o->next    = NULL;
  o->plevel  = NULL;
  
  switch (side) {
  case SideT::BID : 
  }
  if (side == 'B') o->plevel = find_and_update_buy_price_level4add(s,o->type,price,size);
  else o->plevel = find_and_update_sell_price_level4add(s,o->type,price,size);

  o->plevel = findOrAddPlevel(s,_price,_side);
  o->plevel->addMeOrder(o->type,o->size);
  addOrder2Hash (o);
  return o->plevel;
}

/* ####################################################### */

EKA_FH_ERR_CODE EkaFhFullBook::modify_order (FhOrder* o,uint32_t price,uint32_t size) {
  if (o == NULL) return EKA_FH_RESULT__OK;
  FhPlevel* new_p;

  //  FhPlevel* c = o->plevel;
  if (o->plevel == NULL) on_error("o->plevel == NULL");
  FhSecurity* s = o->plevel->s;

  char side = ((o->plevel->state == FhPlevel::STATE::BEST_BUY)  || 
	       (o->plevel->state == FhPlevel::STATE::BUY)) ? 'B' : 'S';
#ifdef INTERNAL_DEBUG
  EKA_LOG("o=%p, o->orderId=%ju,o->size=%d, price=%d,o->plevel = %p, o->plevel->price=%d, o->plevel->state=%u",
	  o,o->orderId,o->size,price,o->plevel,o->plevel->price,o->plevel->state);
#endif
  if (o->plevel->price == price) return  change_order_size (o,size,0);
  o->plevel->delete_order_from_plevel(o->size,o->type);
  if (o->plevel->is_empty()) delete_plevel(o->plevel);

  o->size = size;

  if (side == 'B') new_p = find_and_update_buy_price_level4add (s,o->type,price,size);
  else new_p = find_and_update_sell_price_level4add (s,o->type,price,size);
  o->plevel = new_p;

  return EKA_FH_RESULT__OK;
}

/* ####################################################### */
EKA_FH_ERR_CODE EkaFhFullBook::replace_order (FhOrder* o,uint64_t new_order_id,int32_t price,uint32_t size) {
  if (o == NULL) return EKA_FH_RESULT__OK;

#ifdef INTERNAL_DEBUG
  EKA_LOG("old_order_id=%ju, o->plevel->price=%u, o->plevel->cnt=%u, new_order_id=%ju, price=%u, o->plevel = %p, size=%u o->plevel->state=%s",
	  o->order_id,   o->plevel->price,    o->plevel->cnt,   new_order_id,      price,  o->plevel,       size,"XYU");
#endif

  modify_order(o,price,size);
  deleteOrderFromHash(o->order_id);
  o->order_id = new_order_id;
  o->size = size;
  add_order2hash(o);

  return EKA_FH_RESULT__OK;
}
/* ####################################################### */

EKA_FH_ERR_CODE EkaFhFullBook::change_order_size (FhOrder* o,uint32_t size,int incr) {
  if (o == NULL) on_error("o == NULL for GR%u",id);
#ifdef INTERNAL_DEBUG
  EKA_LOG("o->plevel->price=%d, size=%d, incr=%d,o->size=%d",o->plevel->price,size,incr,o->size);
#endif

  if (incr == 1) { // exec and cancel path
    if (o->size == size) {
      delete_order(o); 
    } else {
      o->deduct_size(size); 
    }
    return EKA_FH_RESULT__OK;
  } 
  // order modify path (= deducting old size then adding new size)
  o->plevel->delete_order_from_plevel(o->size,o->type);
  o->plevel->add_order(size,o->type);
  o->size = size;
  return EKA_FH_RESULT__OK;
}

/* ####################################################### */

void FullBook::deleteOrderFromHash(OrderIdT orderId) {
  uint32_t index = getOrderHashIdx(orderId,MAX_ORDERS,ORDERS_HASH_MASK);
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

void FullBook::addOrder2Hash (FhOrder* o) {
  if (o == NULL) on_error("o==NULL");
#ifdef INTERNAL_DEBUG
  EKA_LOG("o->orderId=%ju",o->orderId);
#endif
  uint64_t orderId = o->orderId;

  uint32_t index = getOrderHashIdx(orderId,MAX_ORDERS,ORDERS_HASH_MASK);
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

inline FhPlevel* EkaFhFullBook::findOrAddPlevel(FhSecurity* s, PriceT _price, SideT _side) {
  FhPlevel** pTob;
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
    return addPlevelAfterTob(pTob, _price, SideT _side);

  for (FhPlevel* p = *pTob; p != NULL; p = p->next) {
    if (p->price == _price) return p;
    if (p->next == NULL || p->next->worsePriceThan(_price))
      return addPlevelAfterP(p,_price);
  }
  on_error("place to insert Plevel not found");
  return NULL;
}

/* ####################################################### */

inline FhPlevel* EkaFhFullBook::addPlevelAfterTob(FhPlevel** pTob, PriceT _price, SideT _side) {
    FhPlevel* newP = getNewFreePlevel();
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

inline FhPlevel* EkaFhFullBook::addPlevelAfterP(FhPlevel* p, PriceT _price) {
  FhPlevel* newP = getNewFreePlevel();
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
