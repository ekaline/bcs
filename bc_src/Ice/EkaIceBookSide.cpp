#include "EkaIceBookSide.h"
#include "EkaBookOrder.h"
#include "EkaBookPriceLevel.h"

/* ----------------------------------------------------- */
//inline Order* EkaIceBookSide::findOrder(OrderId orderId) {
Order* EkaIceBookSide::findOrder(OrderId orderId) {
    uint16_t hashIdx = calcOrderHash(orderId);
    for (Order* o = ordHashTable[hashIdx]; o != NULL; o = o->next) {
      if (o->getId() == orderId) return o;
    }
    return NULL;
  }

  /* ----------------------------------------------------- */
inline int EkaIceBookSide::addOrder2Hash(Order* o, OrderId _id) {      
    uint16_t hashIdx = calcOrderHash(_id);
    Order** o_ptr = &ordHashTable[hashIdx];
    for (; *o_ptr != NULL; o_ptr = &((*o_ptr)->next))
      if (unlikely((*o_ptr)->getId()== _id)) on_error("Adding existing order %ju",_id);
      
    o->next = ordHashTable[hashIdx];
    ordHashTable[hashIdx] = o;

    return 0;
  }
  /* ----------------------------------------------------- */

inline int EkaIceBookSide::addOrder(OrderId _id, Price price, uint32_t _size) {
    if (unlikely(++ordersInUse == ORDERS_FREE_LIST_SIZE)) on_error("Out of %u preallocated orders",ORDERS_FREE_LIST_SIZE);
    if (ordersInUse > maxOrdersInUse) maxOrdersInUse = ordersInUse;

    Order* o = getNewFreeOrder();

    addOrder2Hash(o,_id);
      
    PriceLevel* newP = findOrAddPlevel(price);
    if (unlikely(newP == NULL)) on_error("newP == NULL");

    o->setId(_id);
    o->setSize(_size);
    o->setPlevel(newP);

    //      EKA_LOG("Adding Order to %ju, o->getPlevel()->cnt = %u",price, o->getPlevel()->cnt);

    newP->addOrder(_size);

    return 0;
  }
  /* ----------------------------------------------------- */

inline int EkaIceBookSide::freeOrder(Order* o) {
    if (unlikely(freeOrders == NULL)) on_error("freeOrders = NULL");
    o->next = freeOrders;
    freeOrders = o;
    o->setPlevel(NULL);
    if (unlikely(ordersInUse == 0)) on_error("ordersInUse = 0");
    --ordersInUse;
    return 0;
  }

  /* ----------------------------------------------------- */

inline Order* EkaIceBookSide::getNewFreeOrder() {
    if (unlikely(freeOrders == NULL)) on_error("freeOrders == NULL, used Orders = %ju, Free Orders = %ju",countOrders(),countElems<Order>(freeOrders));
    Order* newO = freeOrders;
    freeOrders = newO->next;
    return newO;
  }
  /* ----------------------------------------------------- */

inline int EkaIceBookSide::freePLevel(PriceLevel* p) {
    if (unlikely(freePriceLevels == NULL)) on_error("freePriceLevels = NULL");
    p->next = freePriceLevels;
    freePriceLevels = p;
    if (unlikely(pLevelsInUse == 0)) on_error("Freeing Plevel when pLevelsInUse == 0");
    pLevelsInUse--;
    return 0;
  }
  /* ----------------------------------------------------- */

inline PriceLevel* EkaIceBookSide::getNewFreePlevel() {
    if (unlikely(freePriceLevels == NULL)) on_error("freePriceLevels = NULL");
    PriceLevel* newP = freePriceLevels;
    freePriceLevels = newP->next;
    newP->reset();
    pLevelsInUse++;
    if (pLevelsInUse > maxPLevelsInUse) maxPLevelsInUse = pLevelsInUse;
    return newP;
  }
  /* ----------------------------------------------------- */

inline PriceLevel* EkaIceBookSide::addPlevelAfterTob(Price _price) {
    PriceLevel* newP = getNewFreePlevel();
    if (newP == NULL) on_error("newP == NULL");
    newP->setPrice(_price);

    newP->setTop(true);
    newP->next = tob;
    newP->prev = NULL;
    if (tob != NULL) tob->prev = newP;
    if (tob != NULL) tob->setTop(false);
    tob = newP;

    return newP;
  }
  /* ----------------------------------------------------- */

inline PriceLevel* EkaIceBookSide::addPlevelAfterP(PriceLevel* p, Price _price) {
  PriceLevel* newP = getNewFreePlevel();
  if (newP == NULL) on_error("newP == NULL");
  newP->setPrice(_price);

  newP->setTop(false);
  newP->next = p->next;
  newP->prev = p;
  if (newP->next != NULL) newP->next->prev = newP;
  p->next = newP;

  return newP;
}
  /* ----------------------------------------------------- */

inline PriceLevel* EkaIceBookSide::findOrAddPlevel(Price _price) {
    if (tob == NULL || tob->worsePriceThan(_price))
      return addPlevelAfterTob(_price);

    PriceLevel* p = tob;
    for (p = tob; p != NULL; p = p->next) {
      if (p->getPrice() == _price) return p;
      if (p->next == NULL || p->next->worsePriceThan(_price)) break;
    }
    return addPlevelAfterP(p,_price);
  }
  /* ----------------------------------------------------- */

inline int EkaIceBookSide::deletePLevel(PriceLevel* p) {
    if (p->isTop()) {
      if (unlikely(tob != p)) on_error("tob != p");
      tob = p->next;
      if (tob != NULL) {
	tob->setTop(true);
	tob->prev = NULL;
      }
    } else {
      if (unlikely(tob == p)) on_error("tob == p");
      p->prev->next = p->next;
      if (p->next != NULL) p->next->prev = p->prev;
    }
    freePLevel(p);
    return 0;
  }

  /* ----------------------------------------------------- */

inline int EkaIceBookSide::removeOrderFromPLevel(PriceLevel* p, uint32_t _size) {
    p->removeOrder(_size);
    if (p->isEmpty()) deletePLevel(p);
    return 0;
  }
  /* ----------------------------------------------------- */
 
inline int EkaIceBookSide::removeOrderFromHash(Order* o2rm) {
    uint16_t hashIdx = calcOrderHash(o2rm->getId());

    Order** o_ptr = &ordHashTable[hashIdx];
    for (; *o_ptr != NULL; o_ptr = &((*o_ptr)->next)) {
      if (*o_ptr == o2rm) {
	(*o_ptr) = o2rm->next;
	return 0;
      }
    }
    on_error("Order %ju not found at Hash",o2rm->getId());
  }
  /* ----------------------------------------------------- */
 
//inline int EkaIceBookSide::addModifyOrder(OrderId _id, Price _price, Size _size, bool isAddOrder) {
int EkaIceBookSide::addModifyOrder(OrderId _id, Price _price, Size _size, bool isAddOrder) {
    Order* o = findOrder(_id);
    if (o == NULL) return addOrder(_id,_price,_size);

    PriceLevel* oldP = o->getPlevel();
    if (oldP->getPrice() == _price) {
      oldP->changeSize(o->getSize(),_size);
    } else {      
      removeOrderFromPLevel(oldP,o->getSize());	
      PriceLevel* newP = findOrAddPlevel(_price);
      newP->addOrder(_size);
      o->setPlevel(newP);
    }
    o->setSize(_size);

    return 0;
  }
  /* ----------------------------------------------------- */
 
//inline int EkaIceBookSide::deleteOrder(Order* o) {
int EkaIceBookSide::deleteOrder(Order* o) {
    if (unlikely(o->getPlevel() == NULL)) on_error("o->getPlevel() == NULL");

    removeOrderFromPLevel(o->getPlevel(),o->getSize());      
    removeOrderFromHash(o);
    freeOrder(o);
    return 0;
  }
  /* ----------------------------------------------------- */
inline PriceLevel* EkaIceBookSide::getPlevelByIdx(uint idx) {
    PriceLevel* p = tob;
    uint cnt = 0;
    while (p != NULL && cnt < idx) {
      p = p->next;
      cnt++;
    }
    return p;
  }
  /* ----------------------------------------------------- */
     
//inline EkaIceBookSide::Price EkaIceBookSide::getEntryPrice(uint idx) {
Price EkaIceBookSide::getEntryPrice(uint idx) {
    PriceLevel* p = getPlevelByIdx(idx);
    if (p == NULL) return 0;
    return p->getPrice();
  }
  /* ----------------------------------------------------- */
     
//inline uint EkaIceBookSide::getEntrySize(uint idx) {
uint EkaIceBookSide::getEntrySize(uint idx) {
    PriceLevel* p = getPlevelByIdx(idx);
    if (p == NULL) return 0;
    return p->getSize();
  }
  /* ----------------------------------------------------- */

template <class T> uint64_t EkaIceBookSide::countElems(T* head) { 
    T* ptr = head;
    uint64_t cnt = 0;
    while (ptr != NULL) {
      cnt++;
      ptr = ptr->next;
    }
    return cnt;
  }
  /* ----------------------------------------------------- */
inline uint64_t EkaIceBookSide::countOrders() {
    uint64_t cnt = 0;
    for (uint i = 0; i < HASH_TABLE_SIZE; i++) {
      for (Order* o = ordHashTable[i]; o != NULL; o = o->next) {
	if (o->getPlevel() == NULL) on_error("%ju: o->getPlevel() == NULL, i = 0x%x",o->getId(),i);
	cnt++;
      }
    }
    return cnt;
  }
  /* ----------------------------------------------------- */

void EkaIceBookSide::printSide() { 
    printf("%s :\n", side == BID ? "BID" : "ASK");
    for (PriceLevel* p = tob; p != NULL; p = p->next) {
      printf ("\tprice: %ju,%u,%u (%c,%c) :\b\t",
	      p->getPrice(),p->getSize(),p->getCnt(),p->isTop() ? 'T' : 'N', p->getSide() == BID ? 'B' : 'A');
      for (uint i = 0; i < HASH_TABLE_SIZE; i++) {
	if (ordHashTable[i] == NULL) continue;
	for (Order* o = ordHashTable[i]; o != NULL; o = o->next) {
	  if (o->getPlevel() == p) {
	    printf("(%ju,%u),",o->getId(),o->getSize());
	  }
	}
      }
      printf("\n");
    }
  }

  /* ----------------------------------------------------- */
inline bool EkaIceBookSide::checkPlevels() {
    uint64_t usedPlevels    = countElems<PriceLevel>(tob);
    uint64_t unusedPlevels  = countElems<PriceLevel>(freePriceLevels);
    uint64_t totalPlevels   = usedPlevels + unusedPlevels;
    if (totalPlevels == PLEVELS_FREE_LIST_SIZE) return true;
    printf ("%s : Used Plevels=%ju, Free Plevels=%ju, total = %ju, allocated = %u : %s\n",
	    side == BID ? "BID" : "ASK",
	    usedPlevels,
	    unusedPlevels,
	    totalPlevels,
	    PLEVELS_FREE_LIST_SIZE,
	    totalPlevels == PLEVELS_FREE_LIST_SIZE ? "CORRECT" : "ERROR"
	    );
    return false;
  }

  /* ----------------------------------------------------- */

inline bool EkaIceBookSide::checkOrders() {
    uint64_t usedOrders    = countOrders();
    uint64_t unusedOrders  = countElems<Order>(freeOrders);
    uint64_t totalOrders   = usedOrders + unusedOrders;
    if (totalOrders == ORDERS_FREE_LIST_SIZE) return true;
    printf ("%s : Used Orders=%ju, Free Orders=%ju total = %ju, allocated = %u : %s\n",
	    side == BID ? "BID" : "ASK",
	    usedOrders,
	    unusedOrders,
	    totalOrders,
	    ORDERS_FREE_LIST_SIZE,
	    totalOrders == ORDERS_FREE_LIST_SIZE ? "CORRECT" : "ERROR"
	    );
    return false;
  }
  /* ----------------------------------------------------- */

inline bool EkaIceBookSide::checkPlevelsCorrectness() {
    bool res = true;
    for (PriceLevel* p = tob; p != NULL; p = p->next) {
      uint cnt = 0;
      uint size = 0;
      if (p == tob && !p->isTop()) on_error("p == tob && !p->isTop()");
      if (p != tob &&  p->isTop()) on_error("p != tob &&  p->isTop()");
      if (p->next != NULL && ! p->next->worsePriceThan(p->getPrice())) {
	EKA_LOG("%s: %ju is not worse than %ju",
		side == BID ? "BID" : "ASK",p->next->getPrice(),p->getPrice());
	res = false;
      }
      for (uint i = 0; i < HASH_TABLE_SIZE; i++) {
	if (ordHashTable[i] == NULL) continue;
	for (Order* o = ordHashTable[i]; o != NULL; o = o->next) {
	  if (o->getPlevel() == p) {
	    cnt++;
	    size += o->getSize();
	  }
	}
      }
      if (cnt  != p->getCnt()) {
	printf("price=%ju, cnt = %u  !=  p->cnt  = %u\n",p->getPrice(), cnt,  p->getCnt());
	res = false;
      }
      if (size != p->getSize()) {
	printf("price=%ju, size = %u != p->size = %u\n" ,p->getPrice(), size, p->getSize());
	res = false;
      }
    }
    return res;
  }
  /* ----------------------------------------------------- */
//inline bool EkaIceBookSide::checkIntegrity() { 
bool EkaIceBookSide::checkIntegrity() { 
    bool pLevelsCorrect = checkPlevels();
    bool ordersCorrect = checkOrders();
    bool pLevelSizesCorrect = checkPlevelsCorrectness();

    if (pLevelsCorrect && 
	ordersCorrect  &&
	pLevelSizesCorrect) return true;
    return false;

    /* static const uint MAX_COLLISIONS = 256; */
    /* uint collision[MAX_COLLISIONS] = {}; */
    /* for (uint i = 0; i < HASH_TABLE_SIZE; i++) { */
    /* 	if (ordHashTable[i] == NULL) continue; */
    /* 	uint cnt = 0; */
    /* 	for (Order* o = ordHashTable[i]; o != NULL; o = o->next) { */
    /* 	  cnt++; */
    /* 	} */
    /* 	if (cnt >= MAX_COLLISIONS) cnt = MAX_COLLISIONS - 1; */
    /* 	collision[cnt] ++; */
    /* } */
    /* printf ("Orders HASH collisions:\n"); */
    /* for (uint i = 0; i < MAX_COLLISIONS; i++) { */
    /* 	if (collision[i] == 0) continue; */
    /* 	printf ("%4u : %4u\n",i, collision[i]); */
    /* } */
  }
  /* ----------------------------------------------------- */

void EkaIceBookSide::printStats() {
    EKA_LOG("%s: maxOrdersInUse = %u, maxPLevelsInUse = %u",
	    side == BID ? "BID" : "ASK",maxOrdersInUse,maxPLevelsInUse);
  }
  /* ----------------------------------------------------- */

EkaIceBookSide::EkaIceBookSide(SIDE _side, EkaIceBook* _parent) {
  parent = _parent;

  ordersInUse     = 0;
  maxOrdersInUse  = 0;

  pLevelsInUse    = 0;
  maxPLevelsInUse = 0;

  tob             = NULL;
  freeOrders      = NULL;
  freePriceLevels = NULL;

  side = _side;

  EKA_LOG("Creating %s side",side == BID ? "BID" : "ASK");

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  EKA_LOG("Initializing Orders Hash of %u entries",HASH_TABLE_SIZE);
  for (uint i = 0; i < HASH_TABLE_SIZE; i++) ordHashTable[i] = NULL;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  EKA_LOG("Preallocating %u Orders",ORDERS_FREE_LIST_SIZE);
  Order** o = &freeOrders;
  for (uint i = 0; i < ORDERS_FREE_LIST_SIZE; i++) {
    *o = new Order();
    if (*o == NULL) on_error("constructing new Order failed");
    o = &((*o)->next);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  EKA_LOG("Preallocating %u PriceLevel",PLEVELS_FREE_LIST_SIZE);
  PriceLevel** p = &freePriceLevels;
  for (uint i = 0; i < PLEVELS_FREE_LIST_SIZE; i++) {
    *p = new PriceLevel(side);
    if (*p == NULL) on_error("constructing new PriceLevel failed");
    p = &((*p)->next);
  }
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
}
