#include <stdio.h>
#include <utility>

#include "EkaFhPlrParser.h"
#include "EkaFhPlrGr.h"

#include "EkaFhParserCommon.h"

using namespace Plr;

inline EfhTradeStatus quoteCondition(const char c) {
  switch (c) {
  case '1' : return EfhTradeStatus::kNormal;
  case '2' : return EfhTradeStatus::kOpeningRotation;
  case '3' : return EfhTradeStatus::kHalted;
  default  : on_error("Unexpected Quote Condition \'%c\'",c);
  }
}

constexpr EfhTradeCond getTradeType(const char tradeCond) {
  switch (tradeCond) {
#define SWITCH_ENUM(NAME, VALUE) \
    case VALUE: return EfhTradeCond::EKA__DELAYED_CAT(k, NAME);
  EfhTradeCond_ENUM_ITER(SWITCH_ENUM)
#undef SWITCH_ENUM
  default: return EfhTradeCond::kUnmapped;
  }
}

constexpr EfhTradeCond getTradeType(const char tradeCond1, const char tradeCond2) {
  if (tradeCond1 == '\0' || tradeCond1 == ' ') {
    return getTradeType(tradeCond2);
  } else {
    return getTradeType(tradeCond1);
  }
}

bool EkaFhPlrGr::parseMsg(const EfhRunCtx* pEfhRunCtx,
			  const unsigned char*   pMsg,
			  uint64_t         sequence,
			  EkaFhMode        op,
			  const std::chrono::high_resolution_clock::time_point startTime) {

  auto msgHdr {reinterpret_cast<const MsgHdr*>(pMsg)};

  switch (static_cast<MsgType>(msgHdr->type)) {
  case MsgType::OutrightSeriesIndexMapping : {
    if (op != EkaFhMode::DEFINITIONS) break;
    
    auto m {reinterpret_cast<const OutrightSeriesIndexMapping*>(pMsg)};
    EfhOptionDefinitionMsg msg{};
    msg.header.msgType        = EfhMsgType::kOptionDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = m->UnderlyingIndex;
    msg.header.securityId     = m->SeriesIndex;
    msg.header.sequenceNumber = 0;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = 0;

    msg.commonDef.securityType   = EfhSecurityType::kOption;
    msg.commonDef.exchange       = EKA_GRP_SRC2EXCH(exch);
    msg.commonDef.isPassive      = isDefinitionPassive(EfhSecurityType::kOption);
    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate    =
      (2000 + (m->MaturityDate[0] - '0') * 10 + (m->MaturityDate[1] - '0')) * 10000 + 
      (       (m->MaturityDate[2] - '0') * 10 +  m->MaturityDate[3] - '0')  * 100   +
      (        m->MaturityDate[4] - '0') * 10 +  m->MaturityDate[5] - '0';
    msg.commonDef.contractSize   = m->ContractMultiplier;

    msg.commonDef.opaqueAttrA = m->PriceScaleCode;
    outrightPriceScaleCodes[m->SeriesIndex] = m->PriceScaleCode;

    msg.optionType            = m->PutOrCall ?  EfhOptionType::kCall : EfhOptionType::kPut;

    // Strike price is given to us as a null-terminted string which may have a
    // decimal point, e.g, `35.375`. In previous versions, strtof(3) was used,
    // but there can be precision problems.
    char *scanFraction;
    msg.strikePrice = strtol(m->StrikePrice, &scanFraction, 10) * EFH__PRICE_SCALE;
    if (*scanFraction == '.') {
      // A fractional price is present, we'll convert it manually.
      ++scanFraction; // Consume '.'
      const int64_t sign = msg.strikePrice >= 0 ? 1 : -1;

      for (int64_t residualMultipler = EFH__PRICE_SCALE / 10;
           std::isdigit(*scanFraction) && residualMultipler;
           ++scanFraction, residualMultipler /= 10) {
        msg.strikePrice += sign * (*scanFraction - '0') * residualMultipler;
      }

      if (std::isdigit(*scanFraction)) {
        EKA_WARN("price of `%s` expiry %d %s is `%s` which exceeds integral price "
                 "precision of %jd", msg.commonDef.classSymbol,
                 msg.commonDef.expiryDate,
                 msg.optionType == EfhOptionType::kCall ? "call" : "put",
                 m->StrikePrice, EFH__PRICE_SCALE);
        // The whole call will fail now that we have a single security
        // that cannot be faithfully represented.
	//        result = EKA_OPRESULT__ERR_STRIKE_PRICE_OVERFLOW;
      }
    }

    copySymbol(msg.commonDef.underlying, m->UnderlyingSymbol);
    copySymbol(msg.commonDef.classSymbol,m->OptionSymbolRoot);

    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
  }
    break;
    // #####################################################
  case MsgType::ComplexSeriesIndexMapping : { // ComplexDefinition
    auto root {reinterpret_cast<const ComplexSeriesIndexMapping_root*>(pMsg)};

    EfhComplexDefinitionMsg msg{};
    msg.header.msgType        = EfhMsgType::kComplexDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = root->seriesIndex;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    msg.commonDef.securityType   = EfhSecurityType::kComplex;
    msg.commonDef.exchange       = EKA_GRP_SRC2EXCH(exch);
    /* msg.commonDef.exchange       = root->MarketID == 4 ? EfhExchange::kPCX : */
    /*   root->MarketID == 8 ? EfhExchange::kAOE : EfhExchange::kUnknown; */

    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate     = 0; // FIXME: for completeness only, could be "today"
    msg.commonDef.expiryTime     = 0;
    msg.commonDef.contractSize   = 0;

    /* copySymbol(msg.commonDef.underlying, rootBlock->Asset); */
    /* copySymbol(msg.commonDef.classSymbol, rootBlock->SecurityGroup); */
    /* copySymbol(msg.commonDef.exchSecurityName, rootBlock->Symbol); */
    sprintf(msg.commonDef.exchSecurityName, "%u", root->seriesIndex);

    msg.numLegs = root->NoOfLegs;

    bool underlyingPriceScaleFound = false;

    /* EKA_LOG("%s:%u: ComplexDefinition:  securityId=%ju", */
    /* 	    EKA_EXCH_DECODE(exch),id,msg.header.securityId); */
    
    auto legs  {reinterpret_cast<const ComplexDefinitionLeg*>(pMsg + sizeof(*root))};
    for (uint i = 0; i < root->NoOfLegs; i++) {
      msg.legs[i].securityId = legs[i].SymbolIndex;
      msg.legs[i].type       = getComplexSecurityType(legs[i].SecurityType);
      if (msg.legs[i].type == EfhSecurityType::kStock)
	msg.header.underlyingId = legs[i].SymbolIndex;

      msg.legs[i].side       = getSide(legs[i].side);
      msg.legs[i].ratio      = legs[i].LegRatioQty;

      if (!underlyingPriceScaleFound) {
        const auto iter = outrightPriceScaleCodes.find(legs[i].SymbolIndex);
        if (iter != outrightPriceScaleCodes.end()) {
          msg.commonDef.opaqueAttrA = iter->second;
          underlyingPriceScaleFound = true;
        }
      }
    }

    if (!underlyingPriceScaleFound) {
      EKA_WARN("WARNING: %s:%d Could not find underlying price scale for complex def `%s`, defaulting to 4",
               EKA_EXCH_DECODE(exch),id,msg.commonDef.exchSecurityName);
      msg.commonDef.opaqueAttrA = 4;
    }

    if (pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL)
      on_error("pEfhRunCtx->onEfhComplexDefinitionMsgCb == NULL");
    pEfhRunCtx->onEfhComplexDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
  }
    break;
        // #####################################################
  case MsgType::MessageUnavailable : { // 31
    auto m {reinterpret_cast<const MessageUnavailable*>(pMsg)};
    EKA_WARN("WARNING: %s:%d MessageUnavailable at %s: %u..%u,"
	     "ProductID=%u,ChannelID=%u",
	     EKA_EXCH_DECODE(exch),id,EkaFhMode2STR(op),
	     m->BeginSeqNum,m->EndSeqNum,m->ProductID,m->ChannelID);
    
  }
    break; 
    // #####################################################
  case MsgType::RefreshHeader : { // 35
    if (op != EkaFhMode::SNAPSHOT)
      on_error("RefreshHeader met at %s",EkaFhMode2STR(op));
    
    auto m {reinterpret_cast<const RefreshHeader*>(pMsg)};
    if (msgHdr->size == sizeof(RefreshHeader)) { // Full (=1st) Refresh Header
	seq_after_snapshot = m->LastSeqNum + 1;
	/* EKA_LOG("Full (=1st) Refresh Header: seq_after_snapshot = %ju", */
	/* 	seq_after_snapshot); */
      }
  }
    break; 
    // #####################################################
  case MsgType::SymbolClear : { // 32
  }
    break; 
    // #####################################################
  case MsgType::TimeReference : { // 2
    auto m {reinterpret_cast<const SourceTimeReference*>(pMsg)};
    seconds = m->sourceTimeSec;
    gr_ts = m->sourceTimeSec * SEC_TO_NANO;
  }
    break;
    // #####################################################
  case MsgType::SecurityStatus : // 34 -- NOT USED!
#if 0
      auto m {reinterpret_cast<const SecurityStatus*>(pMsg)};
      FhSecurity* s = book->findSecurity(m->seriesIndex);
      if (s == NULL) return false;
      switch (m->securityStatus) {
      case '4' : // Trading Halt
      case '6' : // Suspend
	  s->trading_action = EfhTradeStatus::kHalted;
	  break;
      case '5' : // Resume
	  s->trading_action = EfhTradeStatus::kNormal;
	  s->option_open    = true;
	  break;
      case 'P' : // Pre-Open
	  s->trading_action = EfhTradeStatus::kPreopen;
	  break;	  
      case 'B' : // Begin Accepting orders
	  s->trading_action = EfhTradeStatus::kOpeningRotation;
	  break;
      case 'A' : // Short Sale Restriction Activated (Day 1)
      case 'C' : // Short Sale Restriction Continued (Day 2)
      case 'D' : // Short Sale Restriction Deactivated
	  break;
      case 'E' : // Early session
      case 'O' : // Core session
      case 'L' : // Late session
	  break;
      case 'X' : // Closed
	  s->trading_action = EfhTradeStatus::kClosed;
	  s->option_open    = false;
	  break;
      case 'I' : // Halt Resume Price Indication
      case 'G' : // Pre-Opening Price Indication
	  break;
      default : on_error("Unexpected seriesStatus \'%c\'",m->seriesStatus);
      }
      
#endif // 0
      break;
      
    // #####################################################
  case MsgType::OptionsStatus : { // 51
    auto m {reinterpret_cast<const OptionsStatus*>(pMsg)};
    FhSecurity* s = book->findSecurity(m->seriesIndex);
    if (s == NULL) return false;
    switch (m->seriesStatus) {
    case '4' : // Trading Halt
    case '6' : // Suspend
      s->trading_action = EfhTradeStatus::kHalted;
      break;
    case '5' : // Resume
      s->trading_action = EfhTradeStatus::kNormal;
      if (m->MarketState == 'O') // Core session
	s->option_open    = true;
      break;
    case 'P' : // Pre-Open
      s->trading_action = EfhTradeStatus::kPreopen;
      break;	  
    case 'B' : // Begin Accepting orders
      s->trading_action = EfhTradeStatus::kOpeningRotation;
      break;
    case 'O' : // Core session
      s->option_open    = true;
      s->trading_action = EfhTradeStatus::kNormal;
      break;
    case 'X' : // Closed
      s->trading_action = EfhTradeStatus::kClosed;
      s->option_open    = false;
      break;
    case 'I' : // Halt Resume Price Indication
    case 'G' : // Pre-Opening Price Indication
      break;
    default : on_error("Unexpected seriesStatus \'%c\'",m->seriesStatus);
    }
    book->generateOnQuote (pEfhRunCtx, s, sequence,
			   gr_ts + m->sourceTimeNs, gapNum);  
  }
    break;
      
    // #####################################################
  case MsgType::Quote : { // 340
    auto m {reinterpret_cast<const Quote*>(pMsg)};
    FhSecurity* s = book->findSecurity(m->seriesIndex);
    if (s == NULL) return false;

    s->bid_price     = getPrice(m->bidPrice, s);
    s->bid_size      = m->bidVolume;
    s->bid_cust_size = m->bidCustomerVolume;

    s->ask_price     = getPrice(m->askPrice, s);
    s->ask_size      = m->askVolume;
    s->ask_cust_size = m->askCustomerVolume;

    s->trading_action = quoteCondition(m->quoteCondition);

    book->generateOnQuote (pEfhRunCtx, s, sequence,
			   gr_ts + m->sourceTimeNs, gapNum);    
  }
    break;
    // #####################################################
    
  case MsgType::Trade : { // 320
    auto m {reinterpret_cast<const Trade*>(pMsg)};
    FhSecurity* s = book->findSecurity(m->seriesIndex);
    if (s == NULL) return false;

    EfhTradeMsg msg{};
    msg.header.msgType = EfhMsgType::kTrade;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) m->seriesIndex;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = m->sourceTimeSec * static_cast<uint64_t>(SEC_TO_NANO) + m->sourceTimeNs;
    msg.header.gapNum         = gapNum;

    msg.price       = getPrice(m->price, s);
    msg.size        = m->volume;
    msg.tradeStatus = s->trading_action;
    msg.tradeCond   = getTradeType(m->tradeCond1, m->tradeCond2);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
  }
    break;
     // #####################################################
    
  case MsgType::SeriesRfq : { // 307
    auto m {reinterpret_cast<const Rfq*>(pMsg)};
    FhSecurity* s = book->findSecurity(m->seriesIndex);
    if (s == NULL) return false;

    const EfhAuctionUpdateType updateType = getAuctionUpdateType(m->rfqStatus);
    const bool isDelete = updateType == EfhAuctionUpdateType::kDelete;

    EfhAuctionUpdateMsg msg{};
    msg.header.msgType        = EfhMsgType::kAuctionUpdate;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = m->sourceTimeSec * static_cast<uint64_t>(SEC_TO_NANO) + m->sourceTimeNs;
    msg.header.gapNum         = gapNum;

    numToStrBuf(msg.auctionId, m->auctionId);
    msg.auctionType       = getAuctionType(m->type);
    msg.updateType        = updateType;
    msg.securityType      = EfhSecurityType::kRfq;
    msg.header.securityId = (uint64_t) m->seriesIndex;
    msg.side              = getSide(m->side);
    msg.capacity          = getRfqCapacity(m->capacity);
    msg.price             = getPrice(m->workingPrice, s);
    if (s->type == EfhSecurityType::kComplex && m->side == 'S') {
      // Invert ask price to match our complex price conventions
      msg.price = -msg.price;
    }
    msg.quantity          = m->totalQuantity;
    if (m->participant) {
      // At time of writing all OCC member IDs are <1000, so this should never fail.
      // If we do get a massive ID somehow, it just leaves the field blank.
      tryNumToStrBuf(msg.firmId, m->participant);
    }
    msg.endTimeNanos      = isDelete ? 0 : msg.header.timeStamp + getRfqRunTimeNanos(m->type);

    if (pEfhRunCtx->onEfhAuctionUpdateMsgCb == NULL)
      on_error("pEfhRunCtx->onEfhAuctionUpdateMsgCb == NULL");
    pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData, pEfhRunCtx->efhRunUserData);
    
  }
    break;   
    // #####################################################
    
  default:
    break;
  }
  
  return false;
}
