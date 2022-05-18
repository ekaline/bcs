#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <algorithm>

#include "EkaFhParserCommon.h"
#include "EkaFhBxGr.h"
#include "EkaFhBxParser.h"
#include "EkaFhFullBook.h"

#include "EkaFhNasdaqCommonParser.h"

using namespace EfhNasdaqCommon;

template <class SecurityT, class Msg>
    inline SecurityT* EkaFhBxGr::processOrderExecuted(const unsigned char* m) {
    SecurityIdT securityId = getInstrumentId<Msg>(m);
    SecurityT* s = book->findSecurity(securityId);
    if (!s) return NULL;

    OrderIdT    orderId   = getOrderId  <Msg>(m);
    auto o = book->findOrder(orderId);
    if (!o) return NULL;

    SizeT  deltaSize   = getSize<Msg>(m);  
    auto p = o->plevel;

    book->setSecurityPrevState(s);

    if (o->size < deltaSize) {
	on_error("o->size %u < deltaSize %u",o->size,deltaSize);
    } else if (o->size == deltaSize) {
	book->deleteOrder(o);
    } else {
	book->reduceOrderSize(o,deltaSize);
	p->deductSize (o->type,deltaSize);
    }
  
    return s;
}

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processDeleteOrder(const unsigned char* m) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

  OrderIdT orderId   = getOrderId<Msg>(m);
  FhOrder* o = book->findOrder(orderId);
  if (!o) return NULL;

  book->setSecurityPrevState(s);

  book->deleteOrder(o);
  
  return s;
}

template <class SecurityT, class Msg>
    inline SecurityT* EkaFhBxGr::processReplaceOrder(const unsigned char* m) {
  
    SecurityIdT securityId = getInstrumentId<Msg>(m);
    SecurityT* s = book->findSecurity(securityId);
    if (!s) return NULL;

    OrderIdT oldOrderId   = getOldOrderId  <Msg>(m);
    auto o = book->findOrder(oldOrderId);
    if (!o) return NULL;

    FhOrderType t    = o->type;
    SideT       side = o->plevel->side;

    OrderIdT    newOrderId = getNewOrderId<Msg>(m);
    SizeT       size       = getSize      <Msg>(m);
    PriceT      price      = getPrice     <Msg>(m);

    book->setSecurityPrevState(s);

    book->deleteOrder(o);
    book->addOrder(s,newOrderId,t,price,size,side);

    return s;
}

template <class SecurityT, class Msg>
    inline SecurityT* EkaFhBxGr::processSingleSideUpdate(const unsigned char* m) {
    SecurityIdT securityId = getInstrumentId<Msg>(m);
    SecurityT* s = book->findSecurity(securityId);
    if (!s) return NULL;
  
    OrderIdT orderId   = getOrderId<Msg>(m);
    FhOrder* o         = book->findOrder(orderId);
    if (!o)  return NULL;

    SizeT       size  = getSize <Msg>(m);
    PriceT      price = getPrice<Msg>(m);

    book->setSecurityPrevState(s);
    book->modifyOrder(o,price,size);

    return s;
}

template <class SecurityT, class Msg>
    inline SecurityT* EkaFhBxGr::processReplaceQuote(const unsigned char* m) {
    SecurityIdT securityId = getInstrumentId<Msg>(m);
    SecurityT* s = book->findSecurity(securityId);
    if (!s) return NULL;
  
    OrderIdT oldBidOrderId   = getOldBidOrderId<Msg>(m);
    OrderIdT oldAskOrderId   = getOldAskOrderId<Msg>(m);
    FhOrder* bid_o = book->findOrder(oldBidOrderId);
    FhOrder* ask_o = book->findOrder(oldAskOrderId);

    if (!bid_o && !ask_o) return NULL;

    book->setSecurityPrevState(s);
    if (bid_o) {
	FhOrderType t          = bid_o->type;
	OrderIdT newBidOrderId = getNewBidOrderId<Msg>(m);
	SizeT    bidSize       = getBidSize   <Msg>(m);
	PriceT   bidPrice      = getBidPrice  <Msg>(m);

	book->deleteOrder(bid_o);
	book->addOrder(s,newBidOrderId,t,bidPrice,bidSize,SideT::BID);
    }
    if (ask_o) {
	FhOrderType t          = ask_o->type;
	OrderIdT newAskOrderId = getNewAskOrderId<Msg>(m);
	SizeT    askSize       = getAskSize   <Msg>(m);
	PriceT   askPrice      = getAskPrice  <Msg>(m);

	book->deleteOrder(ask_o);
	book->addOrder(s,newAskOrderId,t,askPrice,askSize,SideT::ASK);
    }

    return s;
}

template <class SecurityT, class Msg>
    inline SecurityT* EkaFhBxGr::processDeleteQuote(const unsigned char* m) {
    SecurityIdT securityId = getInstrumentId<Msg>(m);
    SecurityT* s = book->findSecurity(securityId);
    if (!s) return NULL;
  
    OrderIdT bidOrderId   = getBidOrderId<Msg>(m);
    OrderIdT askOrderId   = getAskOrderId<Msg>(m);

    FhOrder* bid_o = book->findOrder(bidOrderId);
    FhOrder* ask_o = book->findOrder(askOrderId);
  
    if (!bid_o && !ask_o) return NULL;

    book->setSecurityPrevState(s);

    if (bid_o) book->deleteOrder(bid_o);
    if (ask_o) book->deleteOrder(ask_o);
  
    return s;
}

template <class SecurityT, class Msg>
    inline SecurityT* EkaFhBxGr::processTradeQ(const unsigned char* m,
					      uint64_t sequence,
					      uint64_t msgTs,
					       const EfhRunCtx* pEfhRunCtx) {
  return processTrade<Msg>(m,sequence,msgTs,pEfhRunCtx);
}



