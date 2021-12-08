#ifndef _EKA_FH_FULL_BOOK_H_
#define _EKA_FH_FULL_BOOK_H_

#include <string>

#include "EkaFhBook.h"
#include "EkaFhFbSecurity.h"
#include "EkaFhPlevel.h"
#include "EkaFhOrder.h"


/* ####################################################### */

template <const uint SCALE, const uint SEC_HASH_SCALE,class FhSecurity, class FhPlevel, class FhOrder,class SecurityIdT, class OrderIdT, class PriceT, class SizeT>
  class EkaFhFullBook : public EkaFhBook  {
 public:
  /* using FhSecurity   = EkaFhFbSecurity  <SecurityIdT, OrderIdT, PriceT, SizeT>; */
  /* using FhPlevel     = EkaFhFbPlevel    <                       PriceT, SizeT>; */
  /* using FhOrder      = EkaFhFbOrder     <             OrderIdT,         SizeT>; */

  /* ####################################################### */

 EkaFhFullBook(EkaDev* _dev, EkaLSI _grId, EkaSource   _exch) 
   : EkaFhBook (_dev,_grId,_exch) {}
  /* ####################################################### */

  int            init() {
    allocateResources();
    return 0;
  }
  /* ####################################################### */
  ~EkaFhFullBook() {
    EKA_LOG("Invalidating book before deleting");
    invalidate();
    //----------------------------------------------------------
    delete[] pLevelsPool;
    //----------------------------------------------------------
    auto o = orderFreeHead;
    while (o) {
      auto n = o->next;
      delete o;
      o = n;
    }
    //----------------------------------------------------------
    for (size_t hashLine = 0; hashLine < SEC_HASH_LINES; hashLine++) {
      auto s = sec[hashLine];
      while (s) {
	auto n = s->next;
	delete s;
	s = dynamic_cast<FhSecurity*>(n);
      }
    }
  }
  /* ####################################################### */

  void printPlevel(FhPlevel* p, const char* msg, std::FILE *file = stdout) {
    fprintf(file,"\t\t%s %s,%s,%ju,%u,BD size:%ju,Cust size: %ju\n",
	   msg,
	   side2str(p->side).c_str(),
	   p->top ? "TOB" : "NOT TOB",
	   (uint64_t)p->price,
	   p->cnt,
	   (uint64_t)p->bd_size,
	   (uint64_t)p->cust_size
	   );
    if (p->side != SideT::ASK && p->side != SideT::BID) on_error("bad side");
  }
  /* ####################################################### */
  void printSecurity(FhSecurity* s, std::FILE *file = stdout) {
    fprintf(file,"%ju :\n",(uint64_t)s->secId);
    fprintf(file,"\tBID:\n");
    for (FhPlevel* p = s->bid; p != NULL; p = p->next) {
      printPlevel(p, " ",file);
    }
    fprintf(file,"\tASK:\n");
    for (FhPlevel* p = s->ask; p != NULL; p = p->next) {
      printPlevel(p, " ",file);
    }
    fprintf(file,"###############\n");
  }

  /* ####################################################### */

  void printAll(std::FILE *file = stdout) {
    for (uint i = 0; i < SEC_HASH_LINES; i ++) {
      for (FhSecurity* s = (FhSecurity*)sec[i]; s != NULL; s = (FhSecurity*)s->next) {
	printSecurity(s,file);
      }
    }
  }
  /* ####################################################### */

  inline FhSecurity*  findSecurity(SecurityIdT secId) {
    uint32_t index =  secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES) on_error("index = %u >= SEC_HASH_LINES %ju",index,SEC_HASH_LINES);
    if (sec[index] == NULL) return NULL;

    FhSecurity* s = sec[index];

    while (s != NULL) {
      if (s->secId == secId) return s;
      s = (FhSecurity*)s->next;
    }

    return NULL;
  }
  /* ####################################################### */

  inline FhSecurity*  subscribeSecurity(SecurityIdT     secId,
				    EfhSecurityType type,
				    EfhSecUserData  userData,
				    uint64_t        opaqueAttrA,
				    uint64_t        opaqueAttrB) {
    FhSecurity* s = new FhSecurity(secId,type,userData,opaqueAttrA,opaqueAttrB);
    if (s == NULL) on_error("s == NULL, new FhSecurity failed");
  
    uint32_t index = secId & SEC_HASH_MASK;
    if (index >= SEC_HASH_LINES) 
      on_error("index = %u >= SEC_HASH_LINES %ju",index,SEC_HASH_LINES);

    if (sec[index] == NULL) {
      sec[index] = s; // empty bucket
    } else {
      FhSecurity* sp = sec[index];
      while (sp->next != NULL) sp = (FhSecurity*)sp->next;
      sp->next = s;
    }
    numSecurities++;
    return s;
  }

  /* ####################################################### */
  inline FhOrder*  findOrder(OrderIdT orderId) {
    uint32_t index = getOrderHashIdx(orderId);
    FhOrder* o = ord[index];
    while (o != NULL) {
      if (o->orderId == orderId) {
	if (o->plevel == NULL)
	  on_error("o->plevel == NULL for orderId %ju",(uint64_t)orderId);
	if (o->plevel->s == NULL)
	  on_error("o->plevel->s == NULL for  orderId %ju, side %s, price %ju",
		   (uint64_t)orderId,side2str(o->plevel->side).c_str(),(uint64_t)o->plevel->price);

	return o;
      }
      o = o->next;
    }
    return NULL;
  }

  /* ####################################################### */

  inline FhOrder* addOrder(FhSecurity*     s,
		       OrderIdT        _orderId,
		       FhOrderType     _type, 
		       PriceT          _price, 
		       SizeT           _size, 
		       SideT           _side) {
    FhOrder* o = getNewOrder();

    o->orderId = _orderId;
    o->type    = _type;
    o->size    = _size;
    o->next    = NULL;

    o->plevel = findOrAddPlevel(s,_price,_side);
    o->plevel->s = s;
    o->plevel->addSize(o->type,o->size);
    o->plevel->cnt++;
    if (o->plevel->side != _side) on_error("o->plevel->side != _side");
    addOrder2Hash (o);
    return o;
  }

  /* ####################################################### */

  inline int modifyOrder(FhOrder* o, PriceT price,SizeT size) {
    FhPlevel* p = o->plevel;
    if (p->price == price) {
      if (p->deductSize(o->type,o->size) < 0) 
	test_error("deductSize failed for orderId %ju, price %ju, size %ju",
		   (uint64_t)o->orderId,(uint64_t)price,(uint64_t)size);
      if (p->addSize(o->type,size) < 0)
	test_error("addSize failed for orderId %ju, price %ju, size %ju",
		   (uint64_t)o->orderId,(uint64_t)price,(uint64_t)size);
      o->size = size;
    } else {
      FhSecurity* s = (FhSecurity*)p->s;
      SideT    side = p->side;
      if (p->deductSize(o->type,o->size) < 0)
	test_error("deductSize failed for orderId %ju, price %ju, size %ju",
		   (uint64_t)o->orderId,(uint64_t)price,(uint64_t)size);
      p->cnt--;
      if (p->isEmpty()) 
	if (deletePlevel(p) < 0)
	  test_error("deletePlevel failed for orderId %ju, price %ju, size %ju",
		   (uint64_t)o->orderId,(uint64_t)price,(uint64_t)size);
      o->plevel = findOrAddPlevel(s,price,side);
      o->plevel->s = s;
      if (o->plevel->addSize(o->type,size) < 0)
	test_error("addSize failed for orderId %ju, price %ju, size %ju",
		   (uint64_t)o->orderId,(uint64_t)price,(uint64_t)size);
      o->size   = size;
      o->plevel->cnt++;
    }
    return 0;
  }
  /* ####################################################### */
  inline int deleteOrder(FhOrder* o) {
    if (o == NULL) test_error("o == NULL for GR%u",grId);
    FhPlevel* p = o->plevel;
    if (p == NULL) on_error("p == NULL");
  
    /* TEST_LOG("%ju: price = %u, plevel->cnt = %u", */
    /* 	     o->orderId, p->price,p->cnt); */

    p->deductSize(o->type,o->size);
    p->cnt--;

    //    if (sizeof(o->orderId) != 8) on_warning("sizeof(o->orderId) = %ju",(uint64_t) sizeof(o->orderId));

    /* TEST_LOG("%ju: price = %u, plevel->cnt = %u, %s", */
    /* 	     o->orderId, p->price,p->cnt, p->isEmpty() ? "EMPTY" : "NOT EMPTY"); */

   if (p->isEmpty()) deletePlevel(p);

    deleteOrderFromHash (o->orderId);
    releaseOrder(o);
    return 0;
  }
  /* ####################################################### */
  inline int reduceOrderSize(FhOrder* o, SizeT deltaSize) {
    if (o->size < deltaSize) test_error("o->size %d < deltaSize %d",(int)o->size, (int)deltaSize);
   
    o->size -= deltaSize;
    return o->size;
  }
  /* ####################################################### */
  inline int invalidate() {
    int secCnt = 0;
    int ordCnt = 0;

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    EKA_LOG("%s:%u: invalidating %d Securities, %d Plevels, %d Orders",
	    EKA_EXCH_DECODE(exch),grId,
	    numSecurities,numPlevels,numOrders);
    
    for (size_t hashLine = 0; hashLine < SEC_HASH_LINES; hashLine++) {
      auto s = sec[hashLine];      
      while (s) {
	auto n = s->next;
	s->reset();
	secCnt++;
	s = dynamic_cast<FhSecurity*>(n);
      }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    EKA_LOG("%s:%u: invalidating %d numOrders",
	    EKA_EXCH_DECODE(exch),grId,numOrders);
    for (size_t hasLine = 0; hasLine < ORDERS_HASH_LINES; hasLine++) {
      auto o = ord[hasLine];

      while (o) {
	auto n = o->next;
	releaseOrder(o);
	o = n;
	ordCnt++;
      }
      ord[hasLine] = NULL;
    }
    
    EKA_LOG("%s:%u: invalidated %d Securities, ALL Plevels, %d Orders)",
	    EKA_EXCH_DECODE(exch),grId,
	    secCnt,ordCnt
	    );
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    EKA_LOG("%s:%u: invalidating %d numPlevels",
	    EKA_EXCH_DECODE(exch),grId,numPlevels);
    carvePlevels();

    EKA_LOG("%s:%u: book is invalidated",
	    EKA_EXCH_DECODE(exch),grId);
    return 0; 
  }
  /* ####################################################### */
  inline int generateOnQuote (const EfhRunCtx* pEfhRunCtx, 
		       FhSecurity* s, 
		       uint64_t sequence, 
		       uint64_t timestamp,
		       uint gapNum,
		       std::chrono::high_resolution_clock::time_point startTime={}) {
    if (s == NULL) test_error("s == NULL");

    FhPlevel* topBid = s->bid;
    FhPlevel* topAsk = s->ask;

    EfhQuoteMsg msg = {};
    msg.header.msgType        = EfhMsgType::kQuote;
    msg.header.group.source   = exch;
    msg.header.group.localId  = grId;
    msg.header.securityId     = s->secId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = timestamp;

#ifdef EFH_TIME_CHECK_PERIOD
    if (EFH_TIME_CHECK_PERIOD && sequence % EFH_TIME_CHECK_PERIOD == 0) {
      auto now = std::chrono::high_resolution_clock::now();
      msg.header.deltaNs        = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime).count();
      fprintf(dev->deltaTimeLogFile,"%ju,%jd\n",sequence,msg.header.deltaNs);
    }
#endif
    
    msg.header.gapNum         = gapNum;
    msg.tradeStatus           = s->trading_action == EfhTradeStatus::kHalted ? EfhTradeStatus::kHalted :
      s->option_open ? s->trading_action : EfhTradeStatus::kClosed;

    msg.bidSide.price           = s->numBidPlevels == 0 ? 0 : topBid->price;
    msg.bidSide.size            = s->numBidPlevels == 0 ? 0 : topBid->get_total_size();
    msg.bidSide.customerSize    = s->numBidPlevels == 0 ? 0 : topBid->get_total_customer_size();
    msg.bidSide.customerAoNSize = s->numBidPlevels == 0 ? 0 : topBid->cust_aon_size;
    msg.bidSide.bdAoNSize       = s->numBidPlevels == 0 ? 0 : topBid->bd_aon_size;
    msg.bidSide.aoNSize         = s->numBidPlevels == 0 ? 0 : topBid->get_total_aon_size();

    msg.askSide.price           = s->numAskPlevels == 0 ? 0 : topAsk->price;
    msg.askSide.size            = s->numAskPlevels == 0 ? 0 : topAsk->get_total_size();
    msg.askSide.customerSize    = s->numAskPlevels == 0 ? 0 : topAsk->get_total_customer_size();
    msg.askSide.customerAoNSize = s->numAskPlevels == 0 ? 0 : topAsk->cust_aon_size;
    msg.askSide.bdAoNSize       = s->numAskPlevels == 0 ? 0 : topAsk->bd_aon_size;
    msg.askSide.aoNSize         = s->numAskPlevels == 0 ? 0 : topAsk->get_total_aon_size();

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
  /* ####################################################### */

  inline int setSecurityPrevState(FhSecurity* s) {
    prevTradingAction = s->trading_action;
    prevOptionOpen    = s->option_open;

    FhPlevel* topBid = s->bid;
    FhPlevel* topAsk = s->ask;

    prevBestBidPrice     = topBid == NULL ? 0 : topBid->price;
    prevBestBidTotalSize = topBid == NULL ? 0 : topBid->get_total_size();
    prevBestAskPrice     = topAsk == NULL ? 0 : topAsk->price;
    prevBestAskTotalSize = topAsk == NULL ? 0 : topAsk->get_total_size();
    return 0;
  }

  /* ####################################################### */
  inline bool isEqualState(FhSecurity* s) {
    FhPlevel* topBid = s->bid;
    FhPlevel* topAsk = s->ask;

    PriceT newBestBidPrice     = topBid == NULL ? 0 : topBid->price;
    SizeT  newBestBidTotalSize = topBid == NULL ? 0 : topBid->get_total_size();
    PriceT newBestAskPrice     = topAsk == NULL ? 0 : topAsk->price;
    SizeT  newBestAskTotalSize = topAsk == NULL ? 0 : topAsk->get_total_size();

    if (prevTradingAction    != s->trading_action    ||
	prevOptionOpen       != s->option_open       ||
	prevBestBidPrice     != newBestBidPrice      ||
	prevBestBidTotalSize != newBestBidTotalSize  ||
	prevBestAskPrice     != newBestAskPrice      ||
	prevBestAskTotalSize != newBestAskTotalSize )
      return false;

    return true;
  }
  /* ####################################################### */
 private:
  /* ####################################################### */  
  inline uint32_t getOrderHashIdx(OrderIdT orderId) const {
    uint32_t idx = orderId & ORDERS_HASH_MASK;
    if ( idx > ORDERS_HASH_LINES) 
      on_error("orderId (%jx) & ORDERS_HASH_MASK (%jx) (=%x)  > ORDERS_HASH_LINES(%jx)",	      
	       (uint64_t)orderId,
	       ORDERS_HASH_MASK,
	       idx,
	       ORDERS_HASH_LINES
	       );
    return idx;
  }  
  /* ####################################################### */
  inline FhPlevel*    getNewPlevel() {
    if (numPlevels++ == MAX_PLEVELS) 
      on_error("%s:%u: out of preallocated FhPlevels: numPlevels=%u MAX_PLEVELS=%ju",
	       EKA_EXCH_DECODE(exch),grId,numPlevels,MAX_PLEVELS);

    if (numPlevels > maxNumPlevels) maxNumPlevels = numPlevels;
    freePlevels--;
    FhPlevel* p = plevelFreeHead;
    if (p == NULL) on_error("p == NULL");
    plevelFreeHead = plevelFreeHead->next;
    p->reset();
    return p;
  }
  /* ####################################################### */
  inline FhOrder*     getNewOrder() {
    if (++numOrders == MAX_ORDERS) 
      on_error("%s:%u: out of preallocated FhOrders: numOrders=%u MAX_ORDERS=%ju",
	       EKA_EXCH_DECODE(exch),grId,numOrders,MAX_ORDERS);

    if (numOrders > maxNumOrders) maxNumOrders = numOrders;
    FhOrder* o = orderFreeHead;
    if (o == NULL) on_error("o == NULL, numOrders=%u",numOrders);
    orderFreeHead = orderFreeHead->next;
    freeOrders --;
    return o;
  }
  /* ####################################################### */
  inline int releasePlevel(FhPlevel* p) {
    if (p == NULL) test_error("p == NULL");
    if (numPlevels-- == 0) test_error("numPlevels == 0 before releasing new element for GR%u",grId);
    freePlevels++;
    p->next = plevelFreeHead;
    plevelFreeHead = p;
    p->reset();
    return 0;
  }

  /* ####################################################### */
  inline int deletePlevel(FhPlevel* p) {
    /* TEST_LOG("%s %u %s %u cust_size=%u, bd_size=%u, p->next=%p", */
    /* 	     side2str(p->side).c_str(), */
    /* 	     p->price, */
    /* 	     p->top ? "TOB" : "NOT TOB", */
    /* 	     p->cnt, */
    /* 	     p->cust_size, */
    /* 	     p->bd_size, */
    /* 	     p->next); */

    FhSecurity* s = (FhSecurity*)p->s;
    if (s == NULL) test_error("p->security == NULL");
    switch (p->side) {
    case SideT::BID :
      if (p->top) {
	if (s->bid  != p)    test_error("s->bid  != p    for Top Plevel");
	if (p->prev != NULL) test_error("p->prev != NULL for Top Plevel");
	s->bid = p->next;
	if (p->next != NULL) {
	  p->next->top = true;
	  p->next->prev = NULL;
	}
      } else {
	p->prev->next = p->next;
	if (p->next != NULL) {
	  p->next->prev = p->prev;
	}
      }
      if (s->numBidPlevels-- == 0) test_error("s->numBidPlevels == 0");
      break;
    case SideT::ASK :
      if (p->top) {
	if (s->ask  != p)    
	  test_error("s->ask  != p for Top Plevel %s %ju",
		     side2str(p->side).c_str(),(uint64_t)p->price);
	if (p->prev != NULL) 
	  test_error("p->prev != NULL for Top Plevel %s %ju",
		     side2str(p->side).c_str(),(uint64_t)p->price);
	s->ask = p->next;
	if (p->next != NULL) {
	  p->next->top = true;
	  p->next->prev = NULL;
	}
      } else {
	p->prev->next = p->next;
	if (p->next != NULL) {
	  p->next->prev = p->prev;
	}
      }
      if (s->numAskPlevels-- == 0) test_error("s->numAskPlevels == 0");
      break;
    default:
      test_error("Unexpected p->side %d",(int)p->side);
    }
    releasePlevel(p);
    return 0;
  }

  /* ####################################################### */
  inline int releaseOrder(FhOrder* o) {
    if (o == NULL) test_error("o == NULL");
    if (numOrders-- == 0) test_error("numOrders == 0 before releasing new element for GR%u",grId);
    o->next       = orderFreeHead;
    orderFreeHead = o;
    o->orderId    = 0;
    o->size       = 0;
    o->plevel     = NULL;
    freeOrders++;
    return 0;
  }
  /* ####################################################### */
  inline int deleteOrderFromHash(OrderIdT orderId) {
    uint32_t index = getOrderHashIdx(orderId);
    if (ord[index] == NULL) test_error("ord[%u] == NULL",index);

    if ((ord[index]->orderId == orderId) && (ord[index]->next == NULL)) {
      ord[index] = NULL;
    } else if ((ord[index]->orderId == orderId) && (ord[index]->next != NULL)) {
      ord[index] = ord[index]->next;
    } else {
      FhOrder* c = ord[index];
      while (c->next != NULL) {
	if (c->next->orderId == orderId) {
	  c->next = c->next->next;
	  return 0;
	}
	c = c->next;
      }
    }
    return 0;
  }
  /* ####################################################### */
  inline int addOrder2Hash (FhOrder* o) {
    if (o == NULL) test_error("o==NULL");

    uint64_t orderId = o->orderId;

    uint32_t index = getOrderHashIdx(orderId);
    FhOrder** curr = &(ord[index]);
    o->next = NULL;

    while (*curr != NULL) {
      if ((*curr)->orderId == orderId) 
	test_error("adding existing orderId %ju at index %u",orderId,index);
      curr = &((*curr)->next);
    }
    *curr = o;
  
    return 0;
  }
  /* ####################################################### */
  inline FhPlevel* findOrAddPlevel (FhSecurity* s,   
				PriceT     _price, 
				SideT      _side) {
    FhPlevel** pTob = NULL;
    switch (_side) {
    case SideT::BID :
      pTob = (FhPlevel**)&s->bid;
      break;
    case SideT::ASK :
      pTob = (FhPlevel**)&s->ask;
      break;
    default:
      on_error("Unexpected side = %d",(int)_side);
    }

    if (*pTob == NULL || (*pTob)->worsePriceThan(_price))
      return addPlevelAfterTob(s, _price, _side);

    for (FhPlevel* p = *pTob; p != NULL; p = p->next) {
      if (p->price == _price) return p;
      if (p->next == NULL || p->next->worsePriceThan(_price))
	return addPlevelAfterP(s, p,_price);
    }
    on_error("place to insert Plevel not found");
    return NULL;
  }
  /* ####################################################### */
  inline FhPlevel* addPlevelAfterTob(FhSecurity* s,
			      PriceT     _price, 
			      SideT      _side) {
    FhPlevel* newP = getNewPlevel();
    if (newP == NULL) on_error("newP == NULL");
    newP->price = _price;
    newP->side  = _side;

    newP->top = true;
    newP->prev = NULL;

    switch (newP->side) {
    case SideT::BID :
      newP->next = s->bid;
      s->bid     = newP;
      s->numBidPlevels++;
      break;
    case SideT::ASK :
      newP->next = s->ask;
      s->ask     = newP;
      s->numAskPlevels++;
      break;
    default:
      on_error("Unexpected side = %d",(int)newP->side);
    }

    if (newP->next != NULL) {
      newP->next->prev = newP;
      newP->next->top  = false;
    }
    //    printPlevel(newP,"addPlevelAfterTob:");
    return newP;
  }
  /* ####################################################### */
  inline FhPlevel* addPlevelAfterP  (FhSecurity* s,
				     FhPlevel* p,     
				     PriceT _price) {
    FhPlevel* newP = getNewPlevel();
    if (newP == NULL) on_error("newP == NULL");
    newP->price =_price;
    newP->side  = p->side;

    newP->top  = false;
    newP->next = p->next;
    newP->prev = p;
    if (newP->next != NULL) newP->next->prev = newP;
    p->next = newP;
    switch (newP->side) {
    case SideT::BID :
      s->numBidPlevels++;
      break;
    case SideT::ASK :
      s->numAskPlevels++;
      break;
    default:
      on_error("Unexpected side = %d",(int)newP->side);
    }
    //    printPlevel(newP,"addPlevelAfterP:");

    return newP;
  }
  /* ####################################################### */
  int  carvePlevels() {
    EKA_LOG("%s:%u: carving Plevels array",EKA_EXCH_DECODE(exch),grId);
    auto p = pLevelsPool;
    for (size_t i = 0; i < MAX_PLEVELS; i++) {
      p->reset();
      if (i != MAX_PLEVELS - 1)
	p->next = p + 1;
      p++;
    }
    plevelFreeHead = pLevelsPool;
    numPlevels  = 0;
    freePlevels = MAX_PLEVELS; 
    return 0;
  }
  /* ####################################################### */
  int  allocateResources() {
    for (uint i = 0; i < ORDERS_HASH_LINES; i++)
      ord[i]=NULL;
    //----------------------------------------------------------
    EKA_LOG("%s:%u: preallocating %ju free orders",EKA_EXCH_DECODE(exch),grId,MAX_ORDERS);
    FhOrder** o = (FhOrder**)&orderFreeHead;
    for (uint i = 0; i < MAX_ORDERS; i++) {
      *o = new FhOrder();
      if (*o == NULL) on_error("constructing new Order failed");
      o = (FhOrder**)&(*o)->next;
    }
    freeOrders = MAX_ORDERS;
    //----------------------------------------------------------
    EKA_LOG("%s:%u: preallocating %ju free Plevels",EKA_EXCH_DECODE(exch),grId,MAX_PLEVELS);
    pLevelsPool = new FhPlevel[MAX_PLEVELS];
    if (! pLevelsPool)
      on_error("failed to allocate %ju MAX_PLEVELS",MAX_PLEVELS);
    carvePlevels();
    //----------------------------------------------------------
    EKA_LOG("%s:%u: completed",EKA_EXCH_DECODE(exch),grId);

    return 0;
  }

  /* ####################################################### */

  //----------------------------------------------------------

 public:
  FhOrder*       orderFreeHead = NULL; // head pointer to free list of preallocated FhOrder elements
  int            numOrders     = 0;    // counter of currently used orders (for statistics only)
  int            freeOrders    = MAX_ORDERS;   // counter of currently free orders (for sanity check only)
  int            maxNumOrders  = 0;    // highest value of numOrders ever reached (for statistics only)

  FhPlevel*      plevelFreeHead = NULL; // head pointer to free list of preallocated FhPlevel elements
  int            numPlevels     = 0;    // counter of currently used plevels (for statistics only)
  int            freePlevels    = MAX_PLEVELS;   // counter of currently free plevels (for sanity check only)
  int            maxNumPlevels  = 0;    // highest value of numPlevels ever reached (for statistics only)

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

  static const uint64_t ORDERS_HASH_SCALE = SCALE - 3;
  static const uint64_t ORDERS_HASH_MASK = ((1 << ORDERS_HASH_SCALE) - 1);
  static const uint64_t ORDERS_HASH_LINES = 0x1 << ORDERS_HASH_SCALE;

  static const uint64_t SEC_HASH_LINES = 0x1 << SEC_HASH_SCALE;
  static const uint64_t SEC_HASH_MASK  = (0x1 << SEC_HASH_SCALE) - 1;

  FhSecurity*  sec[SEC_HASH_LINES] = {}; // array of pointers to Securities

  FhOrder*     ord[ORDERS_HASH_LINES] = {};

  FhPlevel*    pLevelsPool = NULL;

  //----------------------------------------------------------

  EfhTradeStatus prevTradingAction = EfhTradeStatus::kUninit;
  bool           prevOptionOpen    = false;

  PriceT	prevBestBidPrice      = 0;
  SizeT  	prevBestBidTotalSize  = 0;

  PriceT	prevBestAskPrice      = 0;
  SizeT  	prevBestAskTotalSize  = 0;

  int           numSecurities = 0;
};

#endif
