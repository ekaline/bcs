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

bool EkaFhPlrGr::parseMsg(const EfhRunCtx* pEfhRunCtx,
			  const unsigned char*   pMsg,
			  uint64_t         sequence,
			  EkaFhMode        op,
			  const std::chrono::high_resolution_clock::time_point startTime) {

  auto msgHdr {reinterpret_cast<const MsgHdr*>(pMsg)};

  switch (static_cast<MsgType>(msgHdr->type)) {
  case MsgType::OutrightSeriesIndexMapping : {
    auto m {reinterpret_cast<const OutrightSeriesIndexMapping*>(msgHdr)};
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
    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate    =
      (2000 + (m->MaturityDate[0] - '0') * 10 + (m->MaturityDate[1] - '0')) * 10000 + 
      (       (m->MaturityDate[2] - '0') * 10 +  m->MaturityDate[3] - '0')  * 100   +
      (        m->MaturityDate[4] - '0') * 10 +  m->MaturityDate[5] - '0';
    msg.commonDef.contractSize   = m->ContractMultiplier;
      
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
                 "precision of %d", msg.commonDef.classSymbol,
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
     case MsgType::SymbolClear : {
#ifdef PLR_CERT
      printf (YEL "MsgType::SymbolClear\n" RESET);
#endif      

  }
    break; 
  case MsgType::TimeReference : {
#ifdef PLR_CERT
      printf (YEL "MsgType::TimeReference\n" RESET);
#endif      
    auto m {reinterpret_cast<const SourceTimeReference*>(msgHdr)};
    seconds = m->sourceTimeSec;
    gr_ts = m->sourceTimeSec * SEC_TO_NANO;
  }
    break;
  case MsgType::SecurityStatus : 
#ifdef PLR_CERT
//      printf (YEL "MsgType::SecurityStatus\n" RESET);
#endif  
#if 0
      auto m {reinterpret_cast<const SecurityStatus*>(msgHdr)};
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
      
  case MsgType::OptionsStatus : {
      auto m {reinterpret_cast<const OptionsStatus*>(msgHdr)};
      FhSecurity* s = book->findSecurity(m->seriesIndex);
      if (s == NULL) return false;
      switch (m->seriesStatus) {
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
      case 'O' : // Core session
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
      
  case MsgType::Quote : {
    auto m {reinterpret_cast<const Quote*>(msgHdr)};
    FhSecurity* s = book->findSecurity(m->seriesIndex);
    if (s == NULL) return false;

    s->bid_price     = m->bidPrice;
    s->bid_size      = m->bidVolume;
    s->bid_cust_size = m->bidCustomerVolume;

    s->ask_price     = m->askPrice;
    s->ask_size      = m->askVolume;
    s->ask_cust_size = m->askCustomerVolume;

    s->trading_action = quoteCondition(m->quoteCondition);
    
    book->generateOnQuote (pEfhRunCtx, s, sequence,
			   gr_ts + m->sourceTimeNs, gapNum);    
  }
    break;
    
  case MsgType::Trade : {
    auto m {reinterpret_cast<const Trade*>(msgHdr)};
    FhSecurity* s = book->findSecurity(m->seriesIndex);
    if (s == NULL) return false;
    const EfhTradeMsg msg = {
	{ EfhMsgType::kTrade,
	  {exch,(EkaLSI)id}, // group
	  0,  // underlyingId
	  (uint64_t) m->seriesIndex,
	  sequence,
	  m->sourceTimeSec * static_cast<uint64_t>(SEC_TO_NANO) + m->sourceTimeNs,
	  gapNum },
	m->price,
	m->volume,
	s->trading_action,
	EfhTradeCond::kREG
    };
    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
  }
    break;
    
  default:
    break;
  }
  
  return false;
}
