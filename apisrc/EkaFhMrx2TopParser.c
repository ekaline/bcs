#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhParserCommon.h"
#include "EkaFhMrx2TopGr.h"
#include "EkaFhMrx2TopParser.h"

using namespace Mrx2Top;
using namespace EfhNasdaqCommon;

bool EkaFhMrx2TopGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,
			      uint64_t sequence,EkaFhMode op,
			      std::chrono::high_resolution_clock::time_point startTime) {
  auto genericHdr {reinterpret_cast<const GenericHdr *>(m)};
  auto enc = static_cast<MsgType>(genericHdr->type);

  if (op == EkaFhMode::DEFINITIONS && enc == MsgType::EndOfSnapshot) return true;
  if (op == EkaFhMode::DEFINITIONS && enc != MsgType::Directory) return false;
  if (op == EkaFhMode::SNAPSHOT    && enc == MsgType::Directory) return false;

  //  EKA_LOG("enc = %c",enc);

  FhSecurity* s = NULL;

  switch (enc) {    
    //--------------------------------------------------------------
  case MsgType::SystemEvent :
    // Do nothing
    return false;
    //--------------------------------------------------------------
  case MsgType::TradingAction :
    s = processTradingAction<FhSecurity,TradingAction>(m);
    break; 
    //--------------------------------------------------------------
  case MsgType::Directory : 
    if (op == EkaFhMode::SNAPSHOT) return false;
    processDefinition<Directory>(m,pEfhRunCtx);
    return false;
    //--------------------------------------------------------------
  case MsgType::EndOfSnapshot :
    if (op == EkaFhMode::DEFINITIONS) return true;
    this->seq_after_snapshot = processEndOfSnapshot<EndOfSnapshot>(m,op);
    return true;
    //--------------------------------------------------------------
  case MsgType::BestBidAndAskUpdateShort :
    s = processTwoSidesUpdate<FhSecurity,BestBidAndAskUpdateShort>(m);
    break;
    //--------------------------------------------------------------
  case MsgType::BestBidAndAskUpdateLong :
    s = processTwoSidesUpdate<FhSecurity,BestBidAndAskUpdateLong>(m);
    break;
    //--------------------------------------------------------------
  case MsgType::BestBidUpdateShort :
  case MsgType::BestAskUpdateShort :
    s = processOneSideUpdate<FhSecurity,BestBidOrAskUpdateShort>(m);
    break;
    //--------------------------------------------------------------
  case MsgType::BestBidUpdateLong :
  case MsgType::BestAskUpdateLong :
    s = processOneSideUpdate<FhSecurity,BestBidOrAskUpdateLong>(m);
    break;
    //--------------------------------------------------------------
  case MsgType::Trade :
    s = processTrade<FhSecurity,Trade>(m,sequence,pEfhRunCtx);
    return false;
    //--------------------------------------------------------------
  case MsgType::BrokenTrade :
    // Do nothing
    return false;
    //--------------------------------------------------------------
  case MsgType::ComplexDirectory :
    processComplexDefinition<ComplexDirectory,ComplexDefinitionLeg>(m,pEfhRunCtx);
    return false;
    //--------------------------------------------------------------
  case MsgType::ComplexBestBidAndAskUpdate :
    s = processComplexTwoSidesUpdate<FhSecurity,ComplexBestBidAndAskUpdate>(m);
    break;
    //--------------------------------------------------------------
  case MsgType::ComplexBestBidUpdate :
  case MsgType::ComplexBestAskUpdate :
    s = processComplexOneSideUpdate<FhSecurity,ComplexBestBidOrAskUpdate>(m);
    break;
    //--------------------------------------------------------------
  default: 
    EKA_WARN("UNEXPECTED Message type: enc=\'%c\'",(char)enc);
    //--------------------------------------------------------------
  }
  if (!s) return false;
  book->generateOnQuote (pEfhRunCtx,s,sequence,getTs(m),gapNum);

  return false;
}

template <class SecurityT, class Msg>
inline SecurityT*
EkaFhMrx2TopGr::processTradingAction(const unsigned char* m) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

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

template <class Msg>
inline void EkaFhMrx2TopGr::processDefinition(const unsigned char* m,
					 const EfhRunCtx* pEfhRunCtx) {
  auto msg {reinterpret_cast<const Msg *>(m)};

  EfhOptionDefinitionMsg definitionMsg{};
  definitionMsg.header.msgType        = EfhMsgType::kOptionDefinition;
  definitionMsg.header.group.source   = exch;
  definitionMsg.header.group.localId  = id;
  definitionMsg.header.underlyingId   = 0;
  definitionMsg.header.securityId     = getInstrumentId<Msg>(m);
  definitionMsg.header.sequenceNumber = 0;
  definitionMsg.header.timeStamp      = 0;
  definitionMsg.header.gapNum         = this->gapNum;

  definitionMsg.commonDef.securityType   = EfhSecurityType::kOption;
  definitionMsg.commonDef.exchange       = EKA_GRP_SRC2EXCH(exch);
  definitionMsg.commonDef.underlyingType = EfhSecurityType::kStock;
  definitionMsg.commonDef.expiryDate     =
    (msg->expYear + 2000) * 10000 +
    msg->expMonth * 100 +
    msg->expDate;
  definitionMsg.commonDef.contractSize   = 0;

  definitionMsg.strikePrice           = getPrice<Msg,VanillaPriceT>(m);
  definitionMsg.optionType            = decodeOptionType(msg->optionType);

  copySymbol(definitionMsg.commonDef.underlying,msg->underlying);
  copySymbol(definitionMsg.commonDef.classSymbol,msg->symbol);

  pEfhRunCtx->onEfhOptionDefinitionMsgCb(&definitionMsg,
					 (EfhSecUserData) 0,
					 pEfhRunCtx->efhRunUserData);
  return;
}

template <class Msg>
inline uint64_t
EkaFhMrx2TopGr::processEndOfSnapshot(const unsigned char* m,
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

template <class SecurityT, class Msg>
inline SecurityT*
EkaFhMrx2TopGr::processOneSideUpdate(const unsigned char* m) {
  //  auto msg {reinterpret_cast<const Msg*>(m)};
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

  if (getMrxSide<Msg>(m) == SideT::BID) {
    s->bid_size       = getSize        <Msg>(m);
    s->bid_cust_size  = getCustSize    <Msg>(m);
    s->bid_bd_size    = getProCustSize <Msg>(m);

    s->bid_price      = getPrice       <Msg,VanillaPriceT>(m);
    s->bid_ts         = getTs               (m);
  } else {
    s->ask_size       = getSize        <Msg>(m);
    s->ask_cust_size  = getCustSize    <Msg>(m);
    s->ask_bd_size    = getProCustSize <Msg>(m);

    s->ask_price      = getPrice       <Msg,VanillaPriceT>(m);
    s->ask_ts         = getTs               (m);
  }

  return s;
}

template <class SecurityT, class Msg>
inline SecurityT*
EkaFhMrx2TopGr::processTwoSidesUpdate(const unsigned char* m) {
  //  auto msg {reinterpret_cast<const Msg*>(m)};
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

  s->bid_size       = getBidSize        <Msg>(m);
  s->bid_cust_size  = getBidCustSize    <Msg>(m);
  s->bid_bd_size    = getBidProCustSize <Msg>(m); // To Be Fixed!!!

  s->bid_price      = getBidPrice       <Msg>(m);
  s->bid_ts         = getTs                  (m);

  s->ask_size       = getAskSize        <Msg>(m);
  s->ask_cust_size  = getAskCustSize    <Msg>(m);
  s->ask_bd_size    = getAskProCustSize <Msg>(m); // To Be Fixed!!!

  s->ask_price      = getAskPrice       <Msg>(m);
  s->ask_ts         = s->bid_ts;//getTs                  (m);


  return s;
}

template <class SecurityT, class Msg>
inline SecurityT*
EkaFhMrx2TopGr::processTrade(const unsigned char* m,
			  uint64_t sequence,
			     const EfhRunCtx* pEfhRunCtx) {
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;
  
  SizeT      size  = getSize <Msg>(m);
  BookPriceT price = getPrice<Msg,VanillaPriceT>(m);
  uint64_t   msgTs = getTs        (m); 
  const EfhTradeMsg msg = {
    {EfhMsgType::kTrade,
     {this->exch,(EkaLSI)this->id},
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

template <class Root, class Leg>
  inline void EkaFhMrx2TopGr::processComplexDefinition(const unsigned char* m,
						       const EfhRunCtx* pEfhRunCtx) {
  auto root {reinterpret_cast<const Root*>(m)};

  EfhComplexDefinitionMsg msg{};
  msg.header.msgType        = EfhMsgType::kComplexDefinition;
  msg.header.group.source   = exch;
  msg.header.group.localId  = id;
  msg.header.underlyingId   = 0;
  msg.header.securityId     = (uint64_t) getInstrumentId<Root>(m);
  msg.header.sequenceNumber = 0;
  msg.header.timeStamp      = 0;
  msg.header.gapNum         = this->gapNum;

  //    msg.secondaryGroup        = 0;
  msg.commonDef.securityType   = EfhSecurityType::kComplex;
  msg.commonDef.exchange       = EKA_GRP_SRC2EXCH(exch);
  msg.commonDef.underlyingType = EfhSecurityType::kStock;

  msg.commonDef.expiryDate     = 0; 
  msg.commonDef.expiryTime     = 0;
  msg.commonDef.contractSize   = 0;

  copySymbol(msg.commonDef.underlying,root->underlyingSymbol);
  copySymbol(msg.commonDef.classSymbol,root->underlyingSymbol);

  msg.numLegs = root->numOfLegs;

  const uint8_t* leg =  m + sizeof(*root);
  for (uint i = 0; i < root->numOfLegs; i++) {
    msg.legs[i].securityId = getInstrumentId<Leg>(leg);
    msg.legs[i].type       = getLegType<Leg>(leg);
    if (msg.legs[i].type == EfhSecurityType::kStock)
      msg.header.underlyingId = getInstrumentId<Leg>(leg);

    msg.legs[i].side       = getLegSide<Leg>(leg);
    msg.legs[i].ratio      = getRatio<Leg>(leg);
    msg.legs[i].price      = getLegStrikePrice<Leg,BookPriceT>(leg);

    leg += sizeof(Leg);
  }
  if (!pEfhRunCtx->onEfhComplexDefinitionMsgCb)
    on_error("!pEfhRunCtx->onEfhComplexDefinitionMsgCb");
  pEfhRunCtx->onEfhComplexDefinitionMsgCb(&msg, (EfhSecUserData) 0,
					  pEfhRunCtx->efhRunUserData);
  return;
}

template <class SecurityT, class Msg>
inline SecurityT*
EkaFhMrx2TopGr::processComplexTwoSidesUpdate(const unsigned char* m) {
  //  auto msg {reinterpret_cast<const Msg*>(m)};
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

  s->bid_size       = getBidSize        <Msg>(m);
  s->bid_cust_size  = getBidCustSize    <Msg>(m);
  s->bid_bd_size    = getBidProCustSize <Msg>(m); // To Be Fixed!!!

  s->bid_price      = getBidPrice       <Msg>(m);
  s->bid_ts         = getTs                  (m);

  s->ask_size       = getAskSize        <Msg>(m);
  s->ask_cust_size  = getAskCustSize    <Msg>(m);
  s->ask_bd_size    = getAskProCustSize <Msg>(m); // To Be Fixed!!!

  s->ask_price      = getAskPrice       <Msg>(m);
  s->ask_ts         = s->bid_ts;//getTs                  (m);


  return s;
}


template <class SecurityT, class Msg>
inline SecurityT*
EkaFhMrx2TopGr::processComplexOneSideUpdate(const unsigned char* m) {
  //  auto msg {reinterpret_cast<const Msg*>(m)};
  SecurityIdT securityId = getInstrumentId<Msg>(m);
  SecurityT* s = book->findSecurity(securityId);
  if (!s) return NULL;

  if (getMrxComplexSide<Msg>(m) == SideT::BID) {
    s->bid_size       = getSize        <Msg>(m);
    s->bid_bd_size    = getProCustSize <Msg>(m);

    s->bid_price      = getPrice       <Msg,ComplexPriceT>(m);
    s->bid_ts         = getTs               (m);
  } else {
    s->ask_size       = getSize        <Msg>(m);
    s->ask_cust_size  = getCustSize    <Msg>(m);
    s->ask_bd_size    = getProCustSize <Msg>(m);

    s->ask_price      = getPrice       <Msg,ComplexPriceT>(m);
    s->ask_ts         = getTs               (m);
  }

  return s;
}
