#include <thread>
#include <assert.h>

#include "EkaFh.h"
#include "eka_fh_book.h"

#include "EkaFhGroup.h"

inline uint32_t eka_get_order_hash_idx (uint64_t order_id, uint64_t MAX_ORDERS, uint ORDERS_HASH_MASK) {
  if ((order_id & ORDERS_HASH_MASK) > MAX_ORDERS) 
    on_error("order_id (%jx) & ORDERS_HASH_MASK (%x) (=%jx)  > MAX_ORDERS(%jx)",
	      
	      order_id,
	      ORDERS_HASH_MASK,
	      order_id & ORDERS_HASH_MASK,
	      MAX_ORDERS
	      );
  return order_id & ORDERS_HASH_MASK;
}

int fh_b_order::deduct_size(uint32_t delta_size) {
  if (size < delta_size) 
    on_error("For order id %ju size %u < delta_size %u",order_id,size,delta_size);

  size -= delta_size;
  plevel->deduct_order_size(delta_size,type);
  return 0;
}

/* void fh_book::init_securities_data_struct () { */
/*   EKA_LOG("Initializing Securities for FH GR:%u",id); */
/*   for (int i=0;i<EKA_FH_SEC_HASH_LINES;i++) sec[i]=NULL; */
/*   return; */
/* } */

void FullBook::init_plevels_and_orders () {
  //  EKA_LOG("Initializing PLevels and Orders for FH GR:%u",id);

  for (uint i=0;i<getMaxOrders();i++) ord[i]=NULL;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  EKA_LOG("preallocating %u free orders for GR%u",MAX_ORDERS,id);
  fh_b_order** o = &free_o_head;
  for (uint i = 0; i < getMaxOrders(); i++) {
    *o = new fh_b_order();
    if (*o == NULL) on_error("constructing new Order failed");
    o = &((*o)->next);
  }

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  EKA_LOG("preallocating %u free Plevels for GR%u",MAX_PLEVELS,id);
  fh_b_plevel** p = &free_p_head;
  for (uint i = 0; i < getMaxPlevels();i++) {
    *p = new fh_b_plevel();
    if (*p == NULL) on_error("constructing new Plevel failed");
    p = &((*p)->next);
  }
  return;
}

void FullBook::invalidate() {
  EKA_LOG("Invalidating FH %s:%u",EKA_EXCH_DECODE(exch),id);
  /* if (! full_book) { */
  /*   EKA_LOG("No PLeveles ans Orders to invalidate for %s:%u",EKA_EXCH_DECODE(exch),id); */
  /*   return; */
  /* } */
  uint32_t released_used_plevels_cnt = 0;
  uint32_t released_used_orders_cnt = 0;
  uint32_t released_free_plevels_cnt = 0;
  uint32_t released_free_orders_cnt = 0;

  //-------------------------------------------------------------------------
  uint32_t s_cntr=0;
  for (uint i = 0; i < EKA_FH_SEC_HASH_LINES; i++) {
    if (sec[i] == NULL) continue;
    fh_b_security *sp = sec[i];
    while (sp != NULL) {
      sp->buy = NULL;
      sp->sell = NULL;
      sp->num_of_buy_plevels = 0;
      sp->num_of_sell_plevels = 0;
      sp = sp->next;
      s_cntr ++;
    }
  }
  //  EKA_LOG("invalidated Sec.buy/sell pointers for %u securities",s_cntr);

  //-------------------------------------------------------------------------
  for (uint i=0;i<getMaxOrders();i++) {
    fh_b_order* o = ord[i];
    while (o != NULL) {
      fh_b_order* n = o->next;
      if (o->plevel == NULL) on_error("invalidating order %ju pointing to NULL Plevel",o->order_id);
      if (--o->plevel->cnt == 0) {
	free(o->plevel);
	released_used_plevels_cnt++;
      }
      free(o);
      released_used_orders_cnt++;
      o = n;
    }
    ord[i]=NULL;
  }
  fh_b_order* o = free_o_head;
  while (o != NULL) {
      fh_b_order* n = o->next;
      free(o);
      released_free_orders_cnt++;
      o = n;
  }
  free_o_head = NULL;
  //-------------------------------------------------------------------------

  fh_b_plevel* p = free_p_head;
  while (p != NULL) {
      fh_b_plevel* n = p->next;
      free(p);
      released_free_plevels_cnt++;
      p = n;
  }
  EKA_LOG("released %u used Orders + %u free Orders = %u total (MAX_ORDERS= %ju),  %ju used Plevelss + %u free Plevels = %u total (MAX_PLEVELS=%ju)",
	  released_used_orders_cnt,released_free_orders_cnt,released_used_orders_cnt+released_free_orders_cnt,getMaxOrders(),
	  released_used_plevels_cnt,released_free_plevels_cnt,released_used_plevels_cnt+released_free_plevels_cnt,getMaxPlevels());
  if (released_used_orders_cnt+released_free_orders_cnt != getMaxOrders()) 
    on_error("released_used_orders_cnt+released_free_orders_cnt %u != MAX_ORDERS %ju",
	      released_used_orders_cnt+released_free_orders_cnt,getMaxOrders());

  if (released_used_plevels_cnt+released_free_plevels_cnt != getMaxPlevels())
    on_error("released_used_plevels_cnt+released_free_plevels_cnt %u != MAX_PLEVELS %ju",
  	      released_used_plevels_cnt+released_free_plevels_cnt,getMaxPlevels());

  free_p_head = NULL;

  return;
}

/* fh_book::~fh_book() { */

/* } */

fh_b_plevel* FullBook::get_new_plevel_element() {
  if (plevels_cnt++ == getMaxPlevels()) on_error("out of preallocated fh_b_plevel %ju elements for GR%u!!!",getMaxPlevels(),id);
  if (plevels_cnt > max_plevels_cnt) max_plevels_cnt = plevels_cnt;
  fh_b_plevel* p = free_p_head;
  if (p == NULL) on_error("p == NULL");
  free_p_head = free_p_head->next;
  p->reset();
  return p;
}

void FullBook::release_plevel_element(fh_b_plevel* p) {
  if (p == NULL) on_error("p == NULL");
  if (plevels_cnt-- == 0) on_error("plevels_cnt == 0 before releasing new element for GR%u",id);
  p->next = free_p_head;
  free_p_head = p;
  p->reset();
}

fh_b_order* FullBook::get_new_order_element() {
  if (++orders_cnt == getMaxOrders()) on_error("out of preallocated order_t elements for GR%u!!! orders_cnt=%u MAX_ORDERS=%ju",id,orders_cnt,getMaxOrders());
  if (orders_cnt > max_orders_cnt) max_orders_cnt = orders_cnt;
  fh_b_order* o = free_o_head;
  if (o == NULL) on_error("o == NULL, orders_cnt=%u",orders_cnt);
  free_o_head = free_o_head->next;
  return o;
}

void FullBook::release_order_element(fh_b_order* o) {
  if (o == NULL) on_error("o == NULL");
  if (orders_cnt-- == 0) on_error("orders_cnt == 0 before releasing new element for GR%u",id);
  o->next = free_o_head;
  free_o_head = o;
  o->order_id = 0;
  o->size = 0;
  o->plevel = NULL; 
}

EKA_FH_ERR_CODE FullBook::delete_order (fh_b_order* o) {
  if (o == NULL) on_error("o == NULL for GR%u",id);

#ifdef INTERNAL_DEBUG
  EKA_LOG("o->order_id=%ju,o->plevel=%p,o->size=%d,o->plevel->price=%d,o->plevel->cnt=%d,o->plevel->state=%u",
	  o->order_id,o->plevel,o->size,o->plevel->price,o->plevel->cnt,o->plevel->state);
#endif

  EKA_FH_ERR_CODE err = EKA_FH_RESULT__OK;
  o->plevel->delete_order_from_plevel(o->size,o->type);
  if (o->plevel->is_empty())  delete_plevel (o->plevel);
  delete_order_from_hash (o->order_id);
  release_order_element(o);
  return err;
}

EKA_FH_ERR_CODE FullBook::change_order_size (fh_b_order* o,uint32_t size,int incr) {
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

EKA_FH_ERR_CODE FullBook::delete_plevel(fh_b_plevel* p) {
  if (p == NULL) on_error("p == NULL for GR%u",id);
#ifdef INTERNAL_DEBUG
  EKA_LOG("p=%p, p->price=%d, p->size=%d, p->cnt=%d, p->state=%s",p, p->price,p->size,p->cnt,"XYU");
#endif

  if (p->state == fh_b_plevel::price_level_state_t::BEST_BUY) {
    ((fh_b_security*)p->prev)->buy = p->next;
    if (p->next != NULL) p->next->state = fh_b_plevel::price_level_state_t::BEST_BUY;
  } else if (p->state == fh_b_plevel::price_level_state_t::BEST_SELL) {
    ((fh_b_security*)p->prev)->sell = p->next;
    if (p->next != NULL) p->next->state = fh_b_plevel::price_level_state_t::BEST_SELL;
  } else {
    p->prev->next = p->next;
  }
  if (p->next != NULL) p->next->prev = p->prev;

  if (p->state == fh_b_plevel::price_level_state_t::BUY ||  p->state == fh_b_plevel::price_level_state_t::BEST_BUY) {
    p->s->num_of_buy_plevels --;
  } else if (p->state == fh_b_plevel::price_level_state_t::SELL ||  p->state == fh_b_plevel::price_level_state_t::BEST_SELL) {
    p->s->num_of_sell_plevels --;
  } else {
    on_error("Unexpeted p->state = %u",p->state);
  }
  release_plevel_element(p);

  return EKA_FH_RESULT__OK;
}

EKA_FH_ERR_CODE FullBook::replace_order (fh_b_order* o,uint64_t new_order_id,int32_t price,uint32_t size) {
  if (o == NULL) return EKA_FH_RESULT__OK;

#ifdef INTERNAL_DEBUG
  EKA_LOG("old_order_id=%ju, o->plevel->price=%u, o->plevel->cnt=%u, new_order_id=%ju, price=%u, o->plevel = %p, size=%u o->plevel->state=%s",
	  o->order_id,   o->plevel->price,    o->plevel->cnt,   new_order_id,      price,  o->plevel,       size,"XYU");
#endif

  modify_order(o,price,size);
  delete_order_from_hash(o->order_id);
  o->order_id = new_order_id;
  o->size = size;
  add_order2hash(o);

  return EKA_FH_RESULT__OK;
}

EKA_FH_ERR_CODE FullBook::modify_order (fh_b_order* o,uint32_t price,uint32_t size) {
  if (o == NULL) return EKA_FH_RESULT__OK;
  fh_b_plevel* new_p;

  //  fh_b_plevel* c = o->plevel;
  if (o->plevel == NULL) on_error("o->plevel == NULL");
  fh_b_security* s = o->plevel->s;

  char side = ((o->plevel->state == fh_b_plevel::price_level_state_t::BEST_BUY)  || 
	       (o->plevel->state == fh_b_plevel::price_level_state_t::BUY)) ? 'B' : 'S';
#ifdef INTERNAL_DEBUG
  EKA_LOG("o=%p, o->order_id=%ju,o->size=%d, price=%d,o->plevel = %p, o->plevel->price=%d, o->plevel->state=%u",
	  o,o->order_id,o->size,price,o->plevel,o->plevel->price,o->plevel->state);
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

fh_b_plevel* FullBook::add_order2book (fh_b_security* s,uint64_t order_id,fh_b_order::type_t t, uint32_t price,uint32_t size,char side) {
#ifdef INTERNAL_DEBUG
  EKA_LOG("security_id=%d, order_id=%ju, price=%d, size=%d, side=%c",security_id, order_id,price,size,side);
#endif
  if (s == NULL) return NULL;
  fh_b_order* o = get_new_order_element();
  if (o == NULL) on_error("fh_b_order* o = get_new_order_element(gr); FAILED !!!");

  o->order_id = order_id;
  o->type = t;
  o->size = size;
  o->next = NULL;
  o->plevel = NULL;
  
  if (side == 'B') o->plevel = find_and_update_buy_price_level4add(s,o->type,price,size);
  else o->plevel = find_and_update_sell_price_level4add(s,o->type,price,size);
  add_order2hash (o);
  return o->plevel;
}

fh_b_security* fh_book::find_security(uint32_t security_id) {
#ifdef EKA_TIME_CHECK
      auto start = std::chrono::high_resolution_clock::now();
      uint ll_hops = 0;
#endif
  uint32_t index =  security_id & EKA_FH_SEC_HASH_MASK;
  if (index >= EKA_FH_SEC_HASH_LINES) on_error("index = %u >= EKA_FH_SEC_HASH_LINES %u",index,EKA_FH_SEC_HASH_LINES);
  if (sec[index] == NULL) return NULL;
  fh_b_security* sp = sec[index];
  while (sp != NULL) {
    //    EKA_LOG("sp->security_id = %u",sp->security_id);
    if (sp->security_id == security_id) {
#ifdef EKA_TIME_CHECK
      ll_hops ++;
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
      if (duration_ns > 50000) EKA_WARN("WARNING: HIT processing took %ju ns with ll_hops = %u",duration_ns,ll_hops);
#endif
      return sp;
    }
    sp = sp->next;
  }
#ifdef EKA_TIME_CHECK
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
      if (duration_ns > 50000) EKA_WARN("WARNING: MISS processing took %ju ns with ll_hops = %u",duration_ns,ll_hops);
#endif
  if (subscribe_all) return  subscribe_security(security_id,0,0,0,0);   // internal test case
  return NULL;
}

fh_b_security64* fh_book::find_security64(uint64_t security_id) {
#ifdef EKA_TIME_CHECK
      auto start = std::chrono::high_resolution_clock::now();
      uint ll_hops = 0;
#endif
  uint32_t index =  security_id & EKA_FH_SEC_HASH_MASK;
  if (index >= EKA_FH_SEC_HASH_LINES) on_error("index = %u >= EKA_FH_SEC_HASH_LINES %u",index,EKA_FH_SEC_HASH_LINES);
  if (sec64[index] == NULL) return NULL;
  fh_b_security64* sp = sec64[index];
  while (sp != NULL) {
    //    EKA_LOG("sp->security_id = %u",sp->security_id);
    if (sp->security_id == security_id) {
#ifdef EKA_TIME_CHECK
      ll_hops ++;
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
      if (duration_ns > 50000) EKA_WARN("WARNING: HIT processing took %ju ns with ll_hops = %u",duration_ns,ll_hops);
#endif
      return sp;
    }
    sp = sp->next;
  }
#ifdef EKA_TIME_CHECK
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
      if (duration_ns > 50000) EKA_WARN("WARNING: MISS processing took %ju ns with ll_hops = %u",duration_ns,ll_hops);
#endif
  if (subscribe_all) return  subscribe_security64(security_id,0,0,0,0);   // internal test case
  return NULL;
}

fh_b_security::fh_b_security(uint32_t secid, uint8_t secType, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB) {
  security_id = secid;
  type = secType;
  efhUserData = userData;
  bid_ts = 0;
  ask_ts = 0;

  buy = NULL;
  sell = NULL;
  //  trading_action = '\0';
  trading_action = EfhTradeStatus::kUninit;
  option_open = false;
  next = NULL;
  num_of_buy_plevels = 0;
  num_of_sell_plevels = 0;

  bid_size = 0;
  bid_o_size = 0;
  bid_cust_size = 0;
  bid_pro_cust_size = 0;
  bid_bd_size = 0;
  bid_price = 0;

  ask_size = 0;
  ask_o_size = 0;
  ask_cust_size = 0;
  ask_pro_cust_size = 0;
  ask_bd_size = 0;
  ask_price = 0;

  underlyingIdx = opaqueAttrB;
  return;
}


fh_b_security64::fh_b_security64(uint64_t secid, uint8_t secType, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB) {
  security_id = secid;
  type = secType;
  efhUserData = userData;
  bid_ts = 0;
  ask_ts = 0;

  buy = NULL;
  sell = NULL;
  //  trading_action = '\0';
  trading_action = EfhTradeStatus::kUninit;
  option_open = false;
  next = NULL;
  num_of_buy_plevels = 0;
  num_of_sell_plevels = 0;

  bid_size = 0;
  bid_o_size = 0;
  bid_cust_size = 0;
  bid_pro_cust_size = 0;
  bid_bd_size = 0;
  bid_price = 0;

  ask_size = 0;
  ask_o_size = 0;
  ask_cust_size = 0;
  ask_pro_cust_size = 0;
  ask_bd_size = 0;
  ask_price = 0;

  underlyingIdx = opaqueAttrB;

  return;
}

fh_b_security*  fh_book::subscribe_security (uint32_t secid, uint8_t type, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB) {
/* #if 1 */
/*   EKA_LOG("GR%u: security_id=%d, subscribe_all=%u, total_securities=%u",id,secid,subscribe_all,total_securities); */
/* #endif */
  fh_b_security* s = new fh_b_security(secid,type,userData,opaqueAttrA,opaqueAttrB);
  if (s == NULL) on_error("constructor failed");

  uint32_t index = secid & EKA_FH_SEC_HASH_MASK;
  if (index >= EKA_FH_SEC_HASH_LINES) on_error("index = %u >= EKA_FH_SEC_HASH_LINES %u",index,EKA_FH_SEC_HASH_LINES);

  if (sec[index] == NULL) {
    sec[index] = s; // empty bucket
  } else {
    fh_b_security *sp = sec[index];
    while (sp->next != NULL) sp = sp->next;
    sp->next = s;
  }
  total_securities++;
  return s;
}

fh_b_security64*  fh_book::subscribe_security64 (uint64_t secid, uint8_t type, uint64_t userData,uint64_t opaqueAttrA,uint64_t opaqueAttrB) {
/* #if 1 */
/*   EKA_LOG("GR%u: security_id=%d, subscribe_all=%u, total_securities=%u",id,secid,subscribe_all,total_securities); */
/* #endif */
  fh_b_security64* s = new fh_b_security64(secid,type,userData,opaqueAttrA,opaqueAttrB);
  if (s == NULL) on_error("constructor failed");

  uint32_t index = secid & EKA_FH_SEC_HASH_MASK;
  if (index >= EKA_FH_SEC_HASH_LINES) on_error("index = %u >= EKA_FH_SEC_HASH_LINES %u",index,EKA_FH_SEC_HASH_LINES);

  if (sec64[index] == NULL) {
    sec64[index] = s; // empty bucket
  } else {
    fh_b_security64 *sp = sec64[index];
    while (sp->next != NULL) sp = sp->next;
    sp->next = s;
  }
  total_securities++;
  return s;
}

fh_b_order* FullBook::find_order(uint64_t order_id) {
#ifdef EKA_TIME_CHECK
  auto start1 = std::chrono::high_resolution_clock::now();
#endif
  uint32_t index = eka_get_order_hash_idx(order_id,getMaxOrders(),getOrdersHashMask());
  if (index >= getMaxOrders()) on_error("index = %u >= MAX_ORDERS %ju",index,getMaxOrders());
  /* uint32_t index = eka_get_order_hash_idx(order_id,8 * 1024 * 1024,getOrdersHashMask()); */
  /* if (index >= 8 * 1024 * 1024) on_error("index = %u >= MAX_ORDERS %ju",index,getMaxOrders()); */
#ifdef EKA_TIME_CHECK
      auto finish1 = std::chrono::high_resolution_clock::now();
      uint64_t duration1_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish1-start1).count();
      if (duration1_ns > 5000) EKA_WARN("WARNING: eka_get_order_hash_idx + getMaxOrders took %ju ns",duration1_ns);
#endif

  fh_b_order* ptr = ord[index];
#ifdef EKA_TIME_CHECK
  auto start = std::chrono::high_resolution_clock::now();
  uint ll_hops = 0;
#endif


  while (ptr != NULL) {
    if (ptr->order_id == order_id) {
#ifdef EKA_TIME_CHECK
      ll_hops ++;
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
      if (duration_ns > 5000) EKA_WARN("WARNING: HIT processing took %ju ns, ll_hops = %u",duration_ns, ll_hops);
#endif
      return ptr;
    }
    ptr = ptr->next;
  }
#ifdef EKA_TIME_CHECK
      auto finish = std::chrono::high_resolution_clock::now();
      uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
      if (duration_ns > 5000) EKA_WARN("WARNING: MISS processing took %ju ns",duration_ns);
#endif
  return NULL;
}

void FullBook::add_order2hash (fh_b_order* o) {
  if (o == NULL) on_error("o==NULL");
#ifdef INTERNAL_DEBUG
  EKA_LOG("o->order_id=%ju",o->order_id);
#endif
  uint64_t order_id = o->order_id;

  uint32_t index = eka_get_order_hash_idx(order_id,getMaxOrders(),getOrdersHashMask());
  fh_b_order** curr = &(ord[index]);
  o->next = NULL;

  while (*curr != NULL) {
    if ((*curr)->order_id == order_id) on_error("adding existing order_id %ju at index %u",order_id,index);
    curr = &((*curr)->next);
  }
  *curr = o;
  
  return;
}

void FullBook::delete_order_from_hash(uint64_t order_id) {
  uint32_t index = eka_get_order_hash_idx(order_id,getMaxOrders(),getOrdersHashMask());
  //  if (ord[index] == NULL) on_error("ord[index] == NULL");
  fh_b_order* c = ord[index];
#ifdef INTERNAL_DEBUG
  EKA_LOG("deleting order_id=%ju, ord[index]= %p, ord[index]->order_id = %ju, ord[index]->next=%p",
	   order_id, ord[index], ord[index] == NULL ? -1 : ord[index]->order_id, ord[index] == NULL ? NULL : ord[index]->next);
#endif
  if ((ord[index]->order_id == order_id) && (ord[index]->next == NULL)) {
    ord[index] = NULL;
  } else if ((ord[index]->order_id == order_id) && (ord[index]->next != NULL)) {
    ord[index] = ord[index]->next;
  } else {
    while (c->next != NULL) {
      if (c->next->order_id == order_id) {
#ifdef INTERNAL_DEBUG
	EKA_LOG("%s 2 : deleting order_id=%ju, o=%p, next o=%p", order_id,c,c->next->next);
#endif
	c->next = c->next->next;
	return;
      }
      c = c->next;
    }
  }
}

fh_b_plevel* FullBook::find_and_update_buy_price_level4add  (fh_b_security* s, fh_b_order::type_t o_type, uint32_t price, uint32_t size) {
  if (s == NULL) on_error("s == NULL for GR:%u",id);
#ifdef INTERNAL_DEBUG
  EKA_LOG("price=%d, size=%d, s=%p, s->buy=%p",price,size,s,s->buy);
#endif
  fh_b_plevel* new_p = NULL;
  if ((s->buy == NULL) || ((s->buy != NULL) && (price > s->buy->price))) {// price is the "only one" or better than the BEST in the list
    new_p = get_new_plevel_element();

    new_p->s = s;
    s->num_of_buy_plevels++;
    if (s->buy != NULL) {
      s->buy->state = fh_b_plevel::price_level_state_t::BUY;
      s->buy->prev = new_p;
    }
    new_p->price = price;
    new_p->add_order(size,o_type);

    new_p->state = fh_b_plevel::price_level_state_t::BEST_BUY;
    new_p->next = s->buy;
    new_p->prev = (fh_b_plevel*)s;
    s->buy = new_p;
  } else {
    fh_b_plevel* c = s->buy;
    while (c != NULL) {
      if (c->price == price) { // modifying existing Price Level
	c->add_order(size,o_type);
	return c;
      } 
      if (c->next == NULL) break; // end of list
      if (price > c->next->price) break; // price is better than the next one in the list, so new plevel hould be added after c, before c->next
      c = c->next;
    } 
    new_p = get_new_plevel_element();

    new_p->s = s;
    s->num_of_buy_plevels++;

    new_p->price = price;
    new_p->add_order(size,o_type);

    new_p->next = c->next;
    new_p->prev = c;
    if (c->next != NULL) c->next->prev = new_p;
    c->next = new_p;
    new_p->state = fh_b_plevel::price_level_state_t::BUY;
  }
  return new_p;
}

fh_b_plevel* FullBook::find_and_update_sell_price_level4add (fh_b_security* s, fh_b_order::type_t o_type, uint32_t price, uint32_t size) {
  if (s == NULL) on_error("s == NULL for GR:%u",id);
#ifdef INTERNAL_DEBUG
  EKA_LOG("price=%d, size=%d, s=%p, s->sell=%p",price,size,s,s->sell);
#endif
  fh_b_plevel* new_p = NULL;
  if ((s->sell == NULL) || ((s->sell != NULL) && (price < s->sell->price))) {// price is the "only one" or better than the BEST in the list
    new_p = get_new_plevel_element();

    new_p->s = s;
    s->num_of_sell_plevels++;

    if (s->sell != NULL) {
      s->sell->state = fh_b_plevel::price_level_state_t::SELL;
      s->sell->prev = new_p;
    }
    new_p->price = price;
    new_p->add_order(size,o_type);

    new_p->state = fh_b_plevel::price_level_state_t::BEST_SELL;
    new_p->next = s->sell;
    new_p->prev = (fh_b_plevel*)s;
    s->sell = new_p;
  } else {
    fh_b_plevel* c = s->sell;
    while (c != NULL) {
      if (c->price == price) { // modifying existing Price Level
	c->add_order(size,o_type);
	return c;
      }
      if (c->next == NULL) break; // end of list
      if (price < c->next->price) break; // price is better than the next one in the list, so new plevel hould be added after c, before c->next
      c = c->next;
    } 
    new_p = get_new_plevel_element();

    new_p->s = s;
    s->num_of_sell_plevels++;

    new_p->price = price;
    new_p->add_order(size,o_type);

    new_p->next = c->next;
    new_p->prev = c;
    if (c->next != NULL) c->next->prev = new_p;
    c->next = new_p;
    new_p->state = fh_b_plevel::price_level_state_t::SELL;
  }
  return new_p;
}


void fh_b_security_state::reset () {
  security_id = 0;
  num_of_buy_plevels = 0;
  num_of_sell_plevels = 0;
  best_buy_price = 0;
  best_sell_price = 0;
  best_buy_size = 0;
  best_sell_size = 0;
  best_buy_o_size = 0;
  best_sell_o_size = 0;
  efhUserData = 0;
  return;
}

void fh_b_security_state::set (fh_b_security* s) {
  security_id         = s->security_id;

  best_buy_price      = s->buy  != NULL ? s->buy->price                      : 0;
  best_sell_price     = s->sell != NULL ? s->sell->price                     : 0;
  best_buy_size       = s->buy  != NULL ? s->buy->get_total_size()           : 0;
  best_sell_size      = s->sell != NULL ? s->sell->get_total_size()          : 0;
  best_buy_o_size     = s->buy  != NULL ? s->buy->get_total_customer_size()  : 0;
  best_sell_o_size    = s->sell != NULL ? s->sell->get_total_customer_size() : 0;

  trading_action      = s->trading_action;
  option_open         = s->option_open;

  return;
}

bool fh_b_security_state::is_equal(fh_b_security* s) {

  if (security_id != s->security_id)  on_error("security_id(%u) != s->security_id(%u)",security_id,s->security_id);

  if ((trading_action   != s->trading_action)                                          ||
      (option_open      != s->option_open)                                             ||
      (best_buy_price   != (s->buy  != NULL ? s->buy->price                      : 0)) ||
      (best_sell_price  != (s->sell != NULL ? s->sell->price                     : 0)) ||
      (best_buy_size    != (s->buy  != NULL ? s->buy->get_total_size()           : 0)) ||
      (best_sell_size   != (s->sell != NULL ? s->sell->get_total_size()          : 0)) ||
      (best_buy_o_size  != (s->buy  != NULL ? s->buy->get_total_customer_size()  : 0)) ||
      (best_sell_o_size != (s->sell != NULL ? s->sell->get_total_customer_size() : 0))
      ) return false;
  return true;
}

 /* ##################################################################### */

fh_book::fh_book (EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,EkaFhGroup* gr_ptr) {
  dev = pEfhCtx->dev;

  gr = gr_ptr;

  id = gr->id;  
  exch = gr->exch;

  feed_ver = EFH_EXCH2FEED(exch);

  subscribe_all = pInitCtx->subscribe_all;
  total_securities = 0;
  for (auto i = 0; i < EKA_FH_SEC_HASH_LINES; i++) sec[i] = NULL;
}

FullBook::FullBook(EfhCtx* pEfhCtx, const EfhInitCtx* pInitCtx,EkaFhGroup* gr) : fh_book(pEfhCtx, pInitCtx,gr) {
  orders_cnt = 0;
  plevels_cnt = 0;
  max_orders_cnt = 0;
  max_plevels_cnt = 0;
};

int TobBook::init() {
  EKA_LOG("%s:%u : TOB book with preallocated: %u Securities Hash lines, no PLEVELS, no ORDERS",
	  EKA_EXCH_DECODE(exch),id,EKA_FH_SEC_HASH_LINES);
  return 0;
}

int NomBook::init() {
  init_plevels_and_orders();

  EKA_LOG("%s:%u : Full Book with preallocated: %u Securities Hash lines, %u Plevels, %u Orders ",
	  EKA_EXCH_DECODE(exch),id,EKA_FH_SEC_HASH_LINES, getMaxPlevels(), MAX_ORDERS);
  return 0;
}

int BatsBook::init() {
  init_plevels_and_orders();

  EKA_LOG("%s:%u : Full Book with preallocated: %u Securities Hash lines, %u Plevels, %u Orders ",
	  EKA_EXCH_DECODE(exch),id,EKA_FH_SEC_HASH_LINES, getMaxPlevels(), MAX_ORDERS);
  return 0;
}

 /* ##################################################################### */
void TobBook::sendTobImage (const EfhRunCtx* pEfhRunCtx) {
  for (uint i = 0; i < EKA_FH_SEC_HASH_LINES; i++) {
    if (sec[i] == NULL) continue;
    fh_b_security* s = sec[i];
    while (s != NULL) {
      TobBook::generateOnQuote(pEfhRunCtx,s,0,std::max(s->bid_ts,s->ask_ts),1);
      s = s->next;
    }
  }
  return;
}
 /* ##################################################################### */
void TobBook::sendTobImage64 (const EfhRunCtx* pEfhRunCtx) {
  for (uint i = 0; i < EKA_FH_SEC_HASH_LINES; i++) {
    if (sec64[i] == NULL) continue;
    fh_b_security64* s = sec64[i];
    while (s != NULL) {
      TobBook::generateOnQuote64(pEfhRunCtx,s,0,std::max(s->bid_ts,s->ask_ts),1);
      s = s->next;
    }
  }
  return;
}
 /* ##################################################################### */
int FullBook::generateOnQuote64 (const EfhRunCtx* pEfhRunCtx, fh_b_security64* s, uint64_t sequence, uint64_t timestamp,uint gapNum) {
  on_error ("I was not supposed to be here!!!");
  return 0; // place holder
}
int FullBook::generateOnQuote (const EfhRunCtx* pEfhRunCtx, fh_b_security* s, uint64_t sequence, uint64_t timestamp,uint gapNum) {
  assert (s != NULL);

  EfhQuoteMsg msg = {};
  msg.header.msgType        = EfhMsgType::kQuote;
  msg.header.group.source   = exch;
  msg.header.group.localId  = id;
  msg.header.securityId     = s->security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = timestamp;
  msg.header.queueSize      = gr->q->get_len();
  msg.header.gapNum         = gapNum;
  msg.tradeStatus           = s->trading_action == EfhTradeStatus::kHalted ? EfhTradeStatus::kHalted :
    s->option_open ? s->trading_action : EfhTradeStatus::kClosed;

  msg.bidSide.price           = s->num_of_buy_plevels == 0 ? 0 : s->buy->price;
  msg.bidSide.size            = s->num_of_buy_plevels == 0 ? 0 : s->buy->get_total_size();
  msg.bidSide.customerSize    = s->num_of_buy_plevels == 0 ? 0 : s->buy->get_total_customer_size();
  msg.bidSide.customerAoNSize = s->num_of_buy_plevels == 0 ? 0 : s->buy->cust_aon_size;
  msg.bidSide.bdAoNSize       = s->num_of_buy_plevels == 0 ? 0 : s->buy->bd_aon_size;
  msg.bidSide.aoNSize         = s->num_of_buy_plevels == 0 ? 0 : s->buy->get_total_aon_size();

  msg.askSide.price           = s->num_of_sell_plevels == 0 ? 0 : s->sell->price;
  msg.askSide.size            = s->num_of_sell_plevels == 0 ? 0 : s->sell->get_total_size();
  msg.askSide.customerSize    = s->num_of_sell_plevels == 0 ? 0 : s->sell->get_total_customer_size();
  msg.askSide.customerAoNSize = s->num_of_sell_plevels == 0 ? 0 : s->sell->cust_aon_size;
  msg.askSide.bdAoNSize       = s->num_of_sell_plevels == 0 ? 0 : s->sell->bd_aon_size;
  msg.askSide.aoNSize         = s->num_of_sell_plevels == 0 ? 0 : s->sell->get_total_aon_size();

  if (pEfhRunCtx->onEfhQuoteMsgCb == NULL) on_error("Uninitialized pEfhRunCtx->onEfhQuoteMsgCb");

#ifdef EKA_TIME_CHECK
  auto start = std::chrono::high_resolution_clock::now();
#endif

  pEfhRunCtx->onEfhQuoteMsgCb(&msg, (EfhSecUserData)s->efhUserData, pEfhRunCtx->efhRunUserData);

#ifdef EKA_TIME_CHECK
  auto finish = std::chrono::high_resolution_clock::now();
  uint duration_ms = (uint) std::chrono::duration_cast<std::chrono::milliseconds>(finish-start).count();
  if (duration_ms > 5) EKA_WARN("WARNING: onQuote Callback took %u ms",duration_ms);
#endif
  return 0;
}
 /* ##################################################################### */

int TobBook::generateOnQuote (const EfhRunCtx* pEfhRunCtx, fh_b_security* s, uint64_t sequence, uint64_t timestamp,uint gapNum) {
  assert (s != NULL);

  EfhQuoteMsg msg = {};
  msg.header.msgType        = EfhMsgType::kQuote;
  msg.header.group.source   = exch;
  msg.header.group.localId  = id;
  msg.header.securityId     = s->security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = timestamp;
  msg.header.queueSize      = gr->q == NULL ? 0 : gr->q->get_len();
  msg.header.gapNum         = gapNum;
  msg.tradeStatus           = s->trading_action == EfhTradeStatus::kHalted ? EfhTradeStatus::kHalted :
    s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;

  msg.bidSide.price         = s->bid_price;
  msg.bidSide.size          = s->bid_size;
  msg.bidSide.customerSize  = s->bid_cust_size;
  msg.bidSide.bdSize        = s->bid_bd_size;

  msg.askSide.price         = s->ask_price;
  msg.askSide.size          = s->ask_size;
  msg.askSide.customerSize  = s->ask_cust_size;
  msg.askSide.bdSize        = s->ask_bd_size;

  if (pEfhRunCtx->onEfhQuoteMsgCb == NULL) on_error("Uninitialized pEfhRunCtx->onEfhQuoteMsgCb");


#ifdef EKA_TIME_CHECK
  auto start = std::chrono::high_resolution_clock::now();
#endif
  pEfhRunCtx->onEfhQuoteMsgCb(&msg, (EfhSecUserData)s->efhUserData, pEfhRunCtx->efhRunUserData);

#ifdef EKA_TIME_CHECK
  auto finish = std::chrono::high_resolution_clock::now();
  uint duration_ms = (uint) std::chrono::duration_cast<std::chrono::milliseconds>(finish-start).count();
  if (duration_ms > 5) EKA_WARN("WARNING: onQuote Callback took %u ms",duration_ms);
#endif

  return 0;
}
 /* ##################################################################### */

int TobBook::generateOnQuote64 (const EfhRunCtx* pEfhRunCtx, fh_b_security64* s, uint64_t sequence, uint64_t timestamp,uint gapNum) {
  assert (s != NULL);

  EfhQuoteMsg msg = {};
  msg.header.msgType        = EfhMsgType::kQuote;
  msg.header.group.source   = exch;
  msg.header.group.localId  = id;
  msg.header.securityId     = s->security_id;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = timestamp;
  msg.header.queueSize      = gr->q == NULL ? 0 : gr->q->get_len();
  msg.header.gapNum         = gapNum;
  msg.tradeStatus           = s->trading_action == EfhTradeStatus::kHalted ? EfhTradeStatus::kHalted :
    s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;

  msg.bidSide.price         = s->bid_price;
  msg.bidSide.size          = s->bid_size;
  msg.bidSide.customerSize  = s->bid_cust_size;
  msg.bidSide.bdSize        = s->bid_bd_size;

  msg.askSide.price         = s->ask_price;
  msg.askSide.size          = s->ask_size;
  msg.askSide.customerSize  = s->ask_cust_size;
  msg.askSide.bdSize        = s->ask_bd_size;

  if (pEfhRunCtx->onEfhQuoteMsgCb == NULL) on_error("Uninitialized pEfhRunCtx->onEfhQuoteMsgCb");

  //  EKA_LOG("secId = %ju",msg.header.securityId);

#ifdef EKA_TIME_CHECK
  auto start = std::chrono::high_resolution_clock::now();
#endif
  pEfhRunCtx->onEfhQuoteMsgCb(&msg, (EfhSecUserData)s->efhUserData, pEfhRunCtx->efhRunUserData);

#ifdef EKA_TIME_CHECK
  auto finish = std::chrono::high_resolution_clock::now();
  uint duration_ms = (uint) std::chrono::duration_cast<std::chrono::milliseconds>(finish-start).count();
  if (duration_ms > 5) EKA_WARN("WARNING: onQuote Callback took %u ms",duration_ms);
#endif

  return 0;
}
