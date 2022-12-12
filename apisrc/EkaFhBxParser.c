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

bool EkaFhBxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,
			 const unsigned char* m,
			 uint64_t sequence,
			 EkaFhMode op,
			 std::chrono::high_resolution_clock::time_point
			 startTime) {
  
  if (fh->print_parsed_messages) {
    printMsg<BxFeed>(parser_log,sequence,m);
    fflush(parser_log);
  }
  
  auto genericHdr {reinterpret_cast<const BxFeed::GenericHdr *>(m)};
  char enc = genericHdr->type;

  if (op == EkaFhMode::DEFINITIONS &&
      (enc != 'R') && (enc != 'M')) return false;
  
  FhSecurity* s = NULL;
  uint64_t msgTs = 0;
  
  switch (enc) {
    //--------------------------------------------------------------
  case 'H':  // TradingAction
    s = processTradingAction<FhSecurity,BxFeed::TradingAction>(m);
    break; 
    //--------------------------------------------------------------
  case 'a':  // AddOrderShort
    s = processAddOrder<FhSecurity,BxFeed::AddOrderShort>(m);
    break;
    //--------------------------------------------------------------
  case 'A':  // AddOrderLong
    s = processAddOrder<FhSecurity,BxFeed::AddOrderLong>(m);
    break;
    //--------------------------------------------------------------
  case 'j':  // AddQuoteShort
    s = processAddQuote<FhSecurity,BxFeed::AddQuoteShort>(m);
    break;
    //--------------------------------------------------------------
  case 'J':  // AddQuoteLong
    s = processAddQuote<FhSecurity,BxFeed::AddQuoteLong>(m);
    break;
    //--------------------------------------------------------------
  case 'E':  // OrderExecuted
    s = processOrderExecuted<FhSecurity,BxFeed::OrderExecuted,true>(m,sequence,
                                                                    msgTs,pEfhRunCtx);
    break;
    //--------------------------------------------------------------
  case 'C':  // OrderExecutedPrice
    s = processOrderExecuted<FhSecurity,BxFeed::OrderExecutedPrice,true>(m,sequence,
                                                                         msgTs,pEfhRunCtx);
    break;
    //--------------------------------------------------------------
  case 'X':  // OrderCancel
    s = processOrderExecuted<FhSecurity,BxFeed::OrderCancel,false>(m,sequence,
                                                                   msgTs,pEfhRunCtx);
    break;
    //--------------------------------------------------------------
  case 'u':  // ReplaceOrderShort
    s = processReplaceOrder<FhSecurity,BxFeed::ReplaceOrderShort>(m);
    break;
    //--------------------------------------------------------------
  case 'U':  // ReplaceOrderLong
    s = processReplaceOrder<FhSecurity,BxFeed::ReplaceOrderLong>(m);
    break;
    //--------------------------------------------------------------
  case 'D':  // SingleSideDelete
    s = processDeleteOrder<FhSecurity,BxFeed::SingleSideDelete>(m);
    break;        
    //--------------------------------------------------------------
  case 'G':  // SingleSideUpdate
    s = processSingleSideUpdate<FhSecurity,BxFeed::SingleSideUpdate>(m);
    break;
    //--------------------------------------------------------------
  case 'k':  // QuoteReplaceShort
    s = processReplaceQuote<FhSecurity,BxFeed::QuoteReplaceShort>(m);
    break;
    //--------------------------------------------------------------
  case 'K':  // QuoteReplaceLong
    s = processReplaceQuote<FhSecurity,BxFeed::QuoteReplaceLong>(m);
    break;
    //--------------------------------------------------------------
  case 'Y':  // QuoteDelete
    s = processDeleteQuote<FhSecurity,BxFeed::QuoteDelete>(m);
    break;
    //--------------------------------------------------------------
  case 'Q':  // Trade for BX
    msgTs = BxFeed::getTs(m);
    s = processTrade<FhSecurity,BxFeed::Trade>(m,sequence,
					       msgTs,pEfhRunCtx);
    break;
    //--------------------------------------------------------------
  case 'S':  // SystemEvent
    // Backed by instrument TradingAction message
    return false;
    //--------------------------------------------------------------
  case 'B':  // BrokenTrade
    // DO NOTHING
    return false;
    //--------------------------------------------------------------
  case 'I':  // NOII
    msgTs = BxFeed::getTs(m);
    processAuctionUpdate<FhSecurity,BxFeed::NOII>(m,sequence,
                                                  msgTs,
                                                  pEfhRunCtx);
    return false;
    //--------------------------------------------------------------
  case 'M':  // EndOfSnapshot
    EKA_LOG("%s:%u Glimpse EndOfSnapshot",EKA_EXCH_DECODE(exch),id);
    if (op == EkaFhMode::DEFINITIONS) return true;
    this->seq_after_snapshot = processEndOfSnapshot<BxFeed::EndOfSnapshot>(m,op);
    return true;
    //--------------------------------------------------------------
  case 'R':  // Directory
    if (op == EkaFhMode::SNAPSHOT) return false;
    processDefinition<BxFeed::Directory>(m,pEfhRunCtx);
    return false;
  default:
    on_error("UNEXPECTED Message type: enc=\'%c\', trackingNum=%d",enc,genericHdr->trackingNum);
  }
  if (!s) return false;

  msgTs = BxFeed::getTs(m);

  s->bid_ts = msgTs;
  s->ask_ts = msgTs;

  if (! book->isEqualState(s)) { 
    book->generateOnQuote (pEfhRunCtx, s,
			   sequence, msgTs,gapNum);
  }

  return false;
}
/* ##################################################################### */
template <class SecurityT, class Msg>
  inline SecurityT*
  EkaFhBxGr::processTradingAction(const unsigned char* m) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

  book->setSecurityPrevState(s);

  switch (reinterpret_cast<const Msg*>(m)->state) {
  case 'H' : // "H” = Halt in effect
  case 'B' : // “B” = Buy Side Trading Suspended 
  case 'S' : // ”S” = Sell Side Trading Suspended
    s->trading_action = EfhTradeStatus::kHalted;
    break;
  case 'I' : //  ”I” = Pre Open
    s->trading_action = EfhTradeStatus::kPreopen;
    break;
  case 'O' : //  ”O” = Opening Auction
    s->trading_action = EfhTradeStatus::kOpeningRotation;
    break;
  case 'R' : //  ”R” = Re-Opening
    s->trading_action = EfhTradeStatus::kPreopen;
    break;
  case 'T' : //  ”T” = Continuous Trading
    s->trading_action = EfhTradeStatus::kNormal;
    s->option_open    = true;
    break;
  case 'X' : //  ”X” = Closed
    s->trading_action = EfhTradeStatus::kClosed;
    s->option_open    = false;
    break;
  default:
    on_error("Unexpected TradingAction state \'%c\'",
	     reinterpret_cast<const Msg*>(m)->state);
  }
  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT*
  EkaFhBxGr::processAddOrder(const unsigned char* m) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) {
#ifdef FH_SUBSCRIBE_ALL
    s = book->subscribeSecurity(securityId,
				(EfhSecurityType)1,
				(EfhSecUserData)0,0,0);
#else  
    return NULL;
#endif
  }
  OrderIdT    orderId   = getOrderId  <Msg>(m);
  SizeT       size      = getSize     <Msg>(m);
  PriceT      price     = getPrice    <Msg>(m);
  SideT       side      = getSide     <Msg>(m);
  FhOrderType orderType = getOrderType<Msg>(m);
  book->setSecurityPrevState(s);
  book->addOrder(s,orderId,orderType,price,size,side);
  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT*
  EkaFhBxGr::processAddQuote(const unsigned char* m) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) {
#ifdef FH_SUBSCRIBE_ALL
    s = book->subscribeSecurity(securityId,
				(EfhSecurityType)1,
				(EfhSecUserData)0,0,0);
#else  
    return NULL;
#endif
  }
  OrderIdT bidOrderId   = getBidOrderId<Msg>(m);
  SizeT    bidSize      = getBidSize   <Msg>(m);
  PriceT   bidPrice     = getBidPrice  <Msg>(m);
  OrderIdT askOrderId   = getAskOrderId<Msg>(m);
  SizeT    askSize      = getAskSize   <Msg>(m);
  PriceT   askPrice     = getAskPrice  <Msg>(m);

  book->setSecurityPrevState(s);
  book->addOrder(s,bidOrderId,FhOrderType::BD,bidPrice,bidSize,SideT::BID);
  book->addOrder(s,askOrderId,FhOrderType::BD,askPrice,askSize,SideT::ASK);
  return s;
}
template <class SecurityT, class Msg, bool SendTrade>
  inline SecurityT*
  EkaFhBxGr::processOrderExecuted(const unsigned char* m,
                                  uint64_t sequence, uint64_t msgTs,
                                  const EfhRunCtx* pEfhRunCtx) {
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

  if constexpr(SendTrade) {
    EfhTradeMsg msg{};
    msg.header.msgType = EfhMsgType::kTrade;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) securityId;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = msgTs;
    msg.header.gapNum         = gapNum;

    msg.price       = o->plevel->price;
    msg.size        = deltaSize;
    msg.tradeStatus = s->trading_action;
    msg.tradeCond   = EfhTradeCond::kREG;

    pEfhRunCtx->onEfhTradeMsgCb(&msg,
                                s->efhUserData,
                                pEfhRunCtx->efhRunUserData);
  }
  
  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT*
  EkaFhBxGr::processDeleteOrder(const unsigned char* m) {
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
  inline SecurityT*
  EkaFhBxGr::processReplaceOrder(const unsigned char* m) {
  
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
  inline SecurityT*
  EkaFhBxGr::processSingleSideUpdate(const unsigned char* m) {
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
  inline SecurityT*
  EkaFhBxGr::processReplaceQuote(const unsigned char* m) {
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
  inline SecurityT*
  EkaFhBxGr::processDeleteQuote(const unsigned char* m) {
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
  inline SecurityT*
  EkaFhBxGr::processTrade(const unsigned char* m,
			  uint64_t sequence,
			  uint64_t msgTs,
			  const EfhRunCtx* pEfhRunCtx) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;
  
  SizeT       size      = getSize     <Msg>(m);
  PriceT      price     = getPrice    <Msg>(m);

  EfhTradeMsg msg{};
  msg.header.msgType = EfhMsgType::kTrade;
  msg.header.group.source   = exch;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0;
  msg.header.securityId     = (uint64_t) securityId;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = msgTs;
  msg.header.gapNum         = gapNum;

  msg.price       = price;
  msg.size        = size;
  msg.tradeStatus = s->trading_action;
  msg.tradeCond   = EfhTradeCond::kREG;

  pEfhRunCtx->onEfhTradeMsgCb(&msg,
                              s->efhUserData,
                              pEfhRunCtx->efhRunUserData);

  return NULL;
}

template <class SecurityT, class Msg>
  inline SecurityT*
  EkaFhBxGr::processAuctionUpdate(const unsigned char* m,
				  uint64_t sequence,
				  uint64_t msgTs,
				  const EfhRunCtx* pEfhRunCtx) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;
  
  EfhAuctionUpdateMsg msg{};
  msg.header.msgType        = EfhMsgType::kAuctionUpdate;
  msg.header.group.source   = exch;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0;
  msg.header.securityId     = securityId;
  msg.header.sequenceNumber = sequence;
  msg.header.timeStamp      = msgTs;
  msg.header.gapNum         = gapNum;

  msg.auctionId             = getAuctionId<Msg>(m);
  msg.auctionType           = getAuctionType<Msg>(m);

  msg.updateType            = EfhAuctionUpdateType::kNew;
  msg.side                  = getAuctionSide<Msg>(m);
  msg.capacity              = getAuctionOrderCapacity<Msg>(m);
  msg.quantity              = getImbalanceSize<Msg>(m) + getPairedSize<Msg>(m);
  msg.price                 = getAuctionPrice<Msg>(m);
  msg.endTimeNanos          = msgTs + getAuctionDurationNanos<Msg>(m);

  pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData,
				      pEfhRunCtx->efhRunUserData);
  
  return NULL;
}

template <class Msg>
inline uint64_t
EkaFhBxGr::processEndOfSnapshot(const unsigned char* m,
				EkaFhMode op) {
  auto msg {reinterpret_cast<const Msg*>(m)};
  char seqNumStr[sizeof(msg->nextLifeSequence)+1] = {};
  memcpy(seqNumStr, msg->nextLifeSequence, sizeof(msg->nextLifeSequence));
  uint64_t num = strtoul(seqNumStr, NULL, 10);
  EKA_LOG("%s:%u: Glimpse %s EndOfSnapshot: seq_after_snapshot = %ju (\'%s\')",
	  EKA_EXCH_DECODE(exch),id,
	  EkaFhMode2STR(op),
	  num,seqNumStr);
  return (op == EkaFhMode::SNAPSHOT) ? num : 0;
}

template <class Msg>
inline void EkaFhBxGr::processDefinition(const unsigned char* m,
					 const EfhRunCtx* pEfhRunCtx) {
  auto msg {reinterpret_cast<const Msg*>(m)};
  EfhOptionDefinitionMsg definitionMsg{};
  definitionMsg.header.msgType        = EfhMsgType::kOptionDefinition;
  definitionMsg.header.group.source   = exch;
  definitionMsg.header.group.localId  = id;
  definitionMsg.header.underlyingId   = 0;
  definitionMsg.header.securityId     = (uint64_t) getInstrumentId<Msg>(m);
  definitionMsg.header.sequenceNumber = 0;
  definitionMsg.header.timeStamp      = 0;
  definitionMsg.header.gapNum         = this->gapNum;

  definitionMsg.commonDef.securityType   = EfhSecurityType::kOption;
  definitionMsg.commonDef.exchange       = EfhExchange::kBX;
  definitionMsg.commonDef.underlyingType = EfhSecurityType::kStock;
  definitionMsg.commonDef.expiryDate     =
    (2000 + msg->expYear) * 10000 +
    msg->expMonth * 100 +
    msg->expDate;
  definitionMsg.commonDef.contractSize   = 0;

  definitionMsg.strikePrice           = getPrice<Msg>(m);
  definitionMsg.optionType            = decodeOptionType(msg->optionType);

  copySymbol(definitionMsg.commonDef.underlying,msg->underlying);
  copySymbol(definitionMsg.commonDef.classSymbol,msg->symbol);

  pEfhRunCtx->onEfhOptionDefinitionMsgCb(&definitionMsg,
					 (EfhSecUserData) 0,
					 pEfhRunCtx->efhRunUserData);
}
