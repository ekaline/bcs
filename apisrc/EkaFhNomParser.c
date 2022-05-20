#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <algorithm>

#include "EkaFhParserCommon.h"
#include "EkaFhNomGr.h"
#include "EkaFhNomParser.h"
#include "EkaFhFullBook.h"

#include "EkaFhNasdaqCommonParser.h"

using namespace EfhNasdaqCommon;

/* ####################################################### */

bool EkaFhNomGr::parseMsg(const EfhRunCtx* pEfhRunCtx,
			  const unsigned char* m,
			  uint64_t sequence,
			  EkaFhMode op,
			  std::chrono::high_resolution_clock::time_point
			  startTime) {

#ifdef EKA_NOM_LATENCY_CHECK
  auto start = std::chrono::high_resolution_clock::now();  
#endif

  if (fh->print_parsed_messages) {
    printMsg<NomFeed>(parser_log,sequence,m);
    fflush(parser_log);
  }
  
  auto genericHdr {reinterpret_cast<const NomFeed::GenericHdr *>(m)};
  char enc = genericHdr->type;

  if (op == EkaFhMode::DEFINITIONS &&
      (enc != 'R') && (enc != 'M')) return false;
  
  FhSecurity* s = NULL;
  uint64_t msgTs = 0;
  
  switch (enc) {
    //--------------------------------------------------------------
  case 'H':  // TradingAction
    s = processTradingAction<FhSecurity,NomFeed::TradingAction>(m);
    break; 
    //--------------------------------------------------------------
  case 'O':  // OptionOpen -- NOM only
    s = processOptionOpen<FhSecurity,NomFeed::OptionOpen>(m);
    break; 
    //--------------------------------------------------------------
  case 'a':  // AddOrderShort
    s = processAddOrder<FhSecurity,NomFeed::AddOrderShort>(m);
    break;
    //--------------------------------------------------------------
  case 'A':  // AddOrderLong
    s = processAddOrder<FhSecurity,NomFeed::AddOrderLong>(m);
    break;
    //--------------------------------------------------------------
  case 'j':  // AddQuoteShort
    s = processAddQuote<FhSecurity,NomFeed::AddQuoteShort>(m);
    break;
    //--------------------------------------------------------------
  case 'J':  // AddQuoteLong
    s = processAddQuote<FhSecurity,NomFeed::AddQuoteLong>(m);
    break;
    //--------------------------------------------------------------
  case 'E':  // OrderExecuted
    s = processOrderExecuted<FhSecurity,NomFeed::OrderExecuted>(m);
    break;
    //--------------------------------------------------------------
  case 'C':  // OrderExecutedPrice
    s = processOrderExecuted<FhSecurity,NomFeed::OrderExecutedPrice>(m);
    break;
    //--------------------------------------------------------------
  case 'X':  // OrderCancel
    s = processOrderExecuted<FhSecurity,NomFeed::OrderCancel>(m);
    break;
    //--------------------------------------------------------------
  case 'u':  // ReplaceOrderShort
    s = processReplaceOrder<FhSecurity,NomFeed::ReplaceOrderShort>(m);
    break;
    //--------------------------------------------------------------
  case 'U':  // ReplaceOrderLong
    s = processReplaceOrder<FhSecurity,NomFeed::ReplaceOrderLong>(m);
    break;
    //--------------------------------------------------------------
  case 'D':  // SingleSideDelete
    s = processDeleteOrder<FhSecurity,NomFeed::SingleSideDelete>(m);
    break;        
    //--------------------------------------------------------------
  case 'G':  // SingleSideUpdate
    s = processSingleSideUpdate<FhSecurity,NomFeed::SingleSideUpdate>(m);
    break;
    //--------------------------------------------------------------
  case 'k':  // QuoteReplaceShort
    s = processReplaceQuote<FhSecurity,NomFeed::QuoteReplaceShort>(m);
    break;
    //--------------------------------------------------------------
  case 'K':  // QuoteReplaceLong
    s = processReplaceQuote<FhSecurity,NomFeed::QuoteReplaceLong>(m);
    break;
    //--------------------------------------------------------------
  case 'Y':  // QuoteDelete
    s = processDeleteQuote<FhSecurity,NomFeed::QuoteDelete>(m);
    break;
    //--------------------------------------------------------------
  case 'Q':  // Cross Trade
    // DO NOTHING
    return false;
    //--------------------------------------------------------------
  case 'P':  // Trade for NOM
    msgTs = NomFeed::getTs(m);
    s = processTrade<FhSecurity,NomFeed::Trade>(m,sequence,
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
    // TO BE IMPLEMENTED!!!
    return false;
    //--------------------------------------------------------------
  case 'M':  // EndOfSnapshot
    EKA_LOG("%s:%u Glimpse EndOfSnapshot",EKA_EXCH_DECODE(exch),id);
    if (op == EkaFhMode::DEFINITIONS) return true;
    this->seq_after_snapshot = processEndOfSnapshot<NomFeed::EndOfSnapshot>(m,op);
    return true;
    //--------------------------------------------------------------
  case 'R':  // Directory
    if (op == EkaFhMode::SNAPSHOT) return false;
    processDefinition<NomFeed::Directory>(m,pEfhRunCtx);
    return false;
  default: 
    on_error("UNEXPECTED Message type: enc=\'%c\'",enc);
  }
  if (!s) return false;

  msgTs = NomFeed::getTs(m);

  s->bid_ts = msgTs;
  s->ask_ts = msgTs;

  if (! book->isEqualState(s)) {
#ifdef EKA_NOM_LATENCY_CHECK
    auto finish = std::chrono::high_resolution_clock::now();
    uint64_t duration_ns = (uint64_t) std::chrono::duration_cast
      <std::chrono::nanoseconds>(finish-start).count();
    //    if (duration_ns > 5000)
    //        EKA_WARN("WARNING: \'%c\' processing took %ju ns",
    //                 enc, duration_ns);
    fh->latencies.push_back(std::make_pair(enc,duration_ns));
#endif    
    book->generateOnQuote (pEfhRunCtx, s,
			   sequence, msgTs,gapNum);
  }

  /* ##################################################################### */

#if 0  
  if (s->crossedPrice()) {
    char hexBuf[16000]; // approximate max NOM message size
    if (std::FILE *const hexBufFile = fmemopen(hexBuf, sizeof hexBuf, "w")) {
      hexDump("Msg caused CROSS PRICE",m,getMsgLen(m),hexBufFile);
      book->printSecurity(s,hexBufFile);
      (void)std::fwrite("\0", 1, 1, hexBufFile);

      (void)std::fclose(hexBufFile);
    }
    
    EKA_WARN("WARNING PRICE CROSS: %s_%u at %s after \'%c\' : %s",
	     EKA_EXCH_DECODE(exch),id, EkaFhMode2STR(op),enc,
	     hexBuf);
  }
#endif

  return false;
}


template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processTradingAction(const unsigned char* m) {
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
    s->option_open    = false;
    break;
  case 'O' : //  ”O” = Opening Auction
    s->trading_action = EfhTradeStatus::kOpeningRotation;
    s->option_open    = true;
    break;
  case 'R' : //  ”R” = Re-Opening
    s->trading_action = EfhTradeStatus::kNormal;
    s->option_open    = true;
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
  inline SecurityT* EkaFhNomGr::processOptionOpen(const unsigned char* m) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;
  book->setSecurityPrevState(s);

  switch (reinterpret_cast<const Msg*>(m)->state) {
  case 'Y' :
    s->option_open = true;
    break;
  case 'N' :
    s->option_open = false;
    s->trading_action = EfhTradeStatus::kClosed;
    break;
  default :
    on_error("Unexpected OptionOpen state \'%c\'",
	     reinterpret_cast<const Msg*>(m)->state);
  }
  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processAddOrder(const unsigned char* m) {
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
  inline SecurityT* EkaFhNomGr::processAddQuote(const unsigned char* m) {
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

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processDeleteOrder(const unsigned char* m) {
  OrderIdT orderId   = getOrderId<Msg>(m);
  FhOrder* o = book->findOrder(orderId);
  if (!o) return NULL;

  SecurityT* s = (FhSecurity*)o->plevel->s;

  book->setSecurityPrevState(s);

  book->deleteOrder(o);
  
  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processReplaceOrder(const unsigned char* m) {

  OrderIdT oldOrderId   = getOldOrderId  <Msg>(m);
  auto o = book->findOrder(oldOrderId);
  if (!o) return NULL;

  SecurityT* s = (FhSecurity*)o->plevel->s;

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
    inline SecurityT* EkaFhNomGr::processOrderExecuted(const unsigned char* m) {

    OrderIdT    orderId   = getOrderId  <Msg>(m);
    auto o = book->findOrder(orderId);
    if (!o) return NULL;
    
    SecurityT* s = (FhSecurity*)o->plevel->s;

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
  inline SecurityT* EkaFhNomGr::processSingleSideUpdate(const unsigned char* m) {
  
  OrderIdT orderId   = getOrderId<Msg>(m);
  FhOrder* o         = book->findOrder(orderId);
  if (!o)  return NULL;

  SizeT       size  = getSize <Msg>(m);
  PriceT      price = getPrice<Msg>(m);
    
  SecurityT* s = (FhSecurity*)o->plevel->s;

  book->setSecurityPrevState(s);
  book->modifyOrder(o,price,size);

  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processReplaceQuote(const unsigned char* m) {

  OrderIdT oldBidOrderId   = getOldBidOrderId<Msg>(m);
  OrderIdT oldAskOrderId   = getOldAskOrderId<Msg>(m);
  FhOrder* bid_o = book->findOrder(oldBidOrderId);
  FhOrder* ask_o = book->findOrder(oldAskOrderId);

  if (!bid_o && !ask_o) return NULL;
  SecurityT* s = NULL;

  if (bid_o)
    s = (FhSecurity*)bid_o->plevel->s;
  if (ask_o)
    s = (FhSecurity*)ask_o->plevel->s;
  
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
  inline SecurityT* EkaFhNomGr::processDeleteQuote(const unsigned char* m) {
  SecurityT* s = NULL;

  OrderIdT bidOrderId   = getBidOrderId<Msg>(m);
  OrderIdT askOrderId   = getAskOrderId<Msg>(m);

  FhOrder* bid_o = book->findOrder(bidOrderId);
  FhOrder* ask_o = book->findOrder(askOrderId);
  if (bid_o) s = (FhSecurity*)bid_o->plevel->s;
  if (ask_o) s = (FhSecurity*)ask_o->plevel->s;
      
  if (!bid_o && !ask_o) return NULL;

  book->setSecurityPrevState(s);

  if (bid_o) book->deleteOrder(bid_o);
  if (ask_o) book->deleteOrder(ask_o);
  
  return s;
}

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processTrade(const unsigned char* m,
					     uint64_t sequence,
					     uint64_t msgTs,
					     const EfhRunCtx* pEfhRunCtx) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;
  
  SizeT       size      = getSize     <Msg>(m);
  PriceT      price     = getPrice    <Msg>(m);

  const EfhTradeMsg msg = {
			   {EfhMsgType::kTrade,
			    {this->exch,(EkaLSI)this->id}, // group
			    0,  // underlyingId
			    (uint64_t) securityId, 
			    sequence,
			    msgTs,
			    this->gapNum },
			   price,
			   size,
			   s->trading_action,
			   EfhTradeCond::kREG
  };
  pEfhRunCtx->onEfhTradeMsgCb(&msg,
			      s->efhUserData,
			      pEfhRunCtx->efhRunUserData);

  return NULL;
}

template <class SecurityT, class Msg>
  inline SecurityT* EkaFhNomGr::processAuctionUpdate(const unsigned char* m,
						     uint64_t sequence,
						     uint64_t msgTs,
						     const EfhRunCtx* pEfhRunCtx) {
  auto auctionUpdateType = getAuctionUpdateType<Msg>(m);
  if (auctionUpdateType == EfhAuctionUpdateType::kUnknown) return NULL;

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
  msg.header.timeStamp      = gr_ts;
  msg.header.gapNum         = gapNum;

  msg.auctionId             = getAuctionId<Msg>(m);

  msg.updateType            = auctionUpdateType;
  msg.side                  = getAuctionSide<Msg>(m);
  msg.capacity              = getAuctionOrderCapacity<Msg>(m);
  msg.quantity              = getAuctionSize<Msg>(m);
  msg.price                 = getAuctionPrice<Msg>(m);
  msg.endTimeNanos          = 0;

  pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData,
				      pEfhRunCtx->efhRunUserData);
  
  return NULL;
}

template <class Msg>
inline uint64_t EkaFhNomGr::processEndOfSnapshot(const unsigned char* m,
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
inline void EkaFhNomGr::processDefinition(const unsigned char* m,
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

  //    definitionMsg.secondaryGroup        = 0;
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


#if 0
static void print_sec_state(fh_b_security* s) {
  printf ("SecID: %u, num_of_buy_plevels: %u, num_of_sell_plevels: %u\n",s->security_id,s->num_of_buy_plevels,s->num_of_sell_plevels);
  printf ("BUY Side:\n");
  fh_b_plevel* p = s->buy;
  for (auto i = 0; i < s->num_of_buy_plevels; i++) {
    printf ("\t %d: price = %u, cnt = %u, size = %u, o_size = %u\n",i,p->price,p->cnt,p->size,p->o_size);
    p = p->next;
  }
  printf ("SELL Side:\n");
  p = s->sell;
  for (auto i = 0; i < s->num_of_sell_plevels; i++) {
    printf ("\t %d: price = %u, cnt = %u, size = %u, o_size = %u\n",i,p->price,p->cnt,p->size,p->o_size);
    p = p->next;
  }
  fflush(stdout);
}
#endif


