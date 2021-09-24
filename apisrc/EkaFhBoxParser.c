#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <cctype>
#include <string>

#include "EkaFhParserCommon.h"
#include "EkaFhBoxGr.h"
#include "EkaFhBoxParser.h"

using namespace Hsvf;

/* ----------------------------------------------------------------------- */
template <class FhSecurity> 
inline int getStatus(FhSecurity* s, char statusMarker) {
  switch (statusMarker) {
  case 'Y' : // Pre-opening phase
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kPreopen;
    break;

  case 'O' : // Opening phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kOpeningRotation;
    break;

  case 'T' : // Opened for Trading
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kNormal;
    break;

  case 'F' : // Forbidden phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'H' : // Trading Halted
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'R' : // Reserved phase
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kPreopen;
    break;

  case 'S' : // Suspended phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'Z' : // Frozen
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'A' : // Surveillance Intervention phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'C' : // Closed
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kClosed;
    break;

  case 'B' : // Beginning of day inquiries
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kClosed;
    break;

  case ' ' : // Not Used
    break;

  default:
    on_error("unexpected statusMarker \'%c\'",statusMarker);
  }
  return 0;
}

/* ----------------------------------------------------------------------- */

/* static void eka_create_avt_definition (char* dst, const EfhOptionDefinitionMsg* msg) { */
/*   uint8_t y,m,d; */

/*   d = msg->expiryDate % 100; */
/*   m = ((msg->expiryDate - d) / 100) % 100; */
/*   y = msg->expiryDate / 10000 - 2000; */

/*   memcpy(dst,msg->underlying,6); */
/*   for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_'; */
/*   char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P'; */
/*   sprintf(dst+6,"%02u%02u%02u%c%08jd",y,m,d,call_put,msg->strikePrice / 10); */
/*   return; */
/* } */

/* ----------------------------------------------------------------------- */

bool EkaFhBoxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence, EkaFhMode op,std::chrono::high_resolution_clock::time_point startTime) {
  FhSecurity* s = NULL;

  auto msgHdr {reinterpret_cast<const HsvfMsgHdr*>(m)};
  
  auto msgBody {m + sizeof(*msgHdr)};

  //  EKA_LOG("%s:%u: %ju \'%c%c\'",EKA_EXCH_DECODE(exch),id,seq,msgHdr->MsgType[0],msgHdr->MsgType[1]);
  //===================================================
  if (memcmp(msgHdr->MsgType,"F ",sizeof(msgHdr->MsgType)) == 0) { // OptionQuote
    if (op == EkaFhMode::DEFINITIONS) return true; // Dictionary is done

    auto boxMsg {reinterpret_cast<const HsvfOptionQuote*>(msgBody)};

    SecurityIdT security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);

    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    //    EKA_LOG("OptionQuote for %s",boxMsg->InstrumentDescription + '\0');

    s->bid_price     = getNumField<uint32_t>(boxMsg->BidPrice,sizeof(boxMsg->BidPrice)) * getFractionIndicator(boxMsg->BidPriceFractionIndicator);
    s->bid_size      = getNumField<uint32_t>(boxMsg->BidSize,sizeof(boxMsg->BidSize));
    s->bid_cust_size = getNumField<uint32_t>(boxMsg->PublicCustomerBidSize,sizeof(boxMsg->PublicCustomerBidSize));

    s->ask_price     = getNumField<uint32_t>(boxMsg->AskPrice,sizeof(boxMsg->AskPrice)) * getFractionIndicator(boxMsg->AskPriceFractionIndicator);
    s->ask_size      = getNumField<uint32_t>(boxMsg->AskSize,sizeof(boxMsg->AskSize));
    s->ask_cust_size = getNumField<uint32_t>(boxMsg->PublicCustomerAskSize,sizeof(boxMsg->PublicCustomerAskSize));

    getStatus<FhSecurity>(s,boxMsg->InstrumentStatusMarker);
    if (op != EkaFhMode::SNAPSHOT)
      book->generateOnQuote (pEfhRunCtx, s, sequence, gr_ts, gapNum);
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"C ",sizeof(msgHdr->MsgType)) == 0) { // Option Trade
    auto boxMsg {reinterpret_cast<const HsvfOptionTrade*>(msgBody)};
    SecurityIdT security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    EfhTradeMsg msg = {};
    msg.header.msgType        = EfhMsgType::kTrade;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = charSymbol2SecurityId(boxMsg->InstrumentDescription);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = gr_ts;
    msg.header.gapNum         = gapNum;

    msg.price = getNumField<uint32_t>(boxMsg->TradePrice,sizeof(boxMsg->TradePrice)) *
      getFractionIndicator(boxMsg->TradePriceFractionIndicator);
    msg.size = getNumField<uint32_t>(boxMsg->Volume,sizeof(boxMsg->Volume));
    msg.tradeStatus = s->trading_action;
    switch (boxMsg->PriceIndicatorMarker) {
    case ' ':
      // Actual transaction took place
      msg.tradeCond = EfhTradeCond::kREG;
      break;

    case 'C':
      // Trades performed at the end  of a PIP allocation phase -> SLAN
      // (Single Leg Auction Non ISO)
      msg.tradeCond = EfhTradeCond::kSLAN;
      break;

    case 'L':
      // Late trade (Transaction is  being reported late and is out of
      // sequence) -> OSEQ (same definition)
      msg.tradeCond = EfhTradeCond::kOSEQ;
      break;

    case 'O':
      // Trades performed during the opening -> OSHT (code is not part
      // of OPRA, we made it up to map to AVT's "outside hours trade."
      msg.tradeCond = EfhTradeCond::kOSHT;
      break;

    case 'W':
      // Trades resulting from the transmission of an ISO Inbound order -> ISOI
      msg.tradeCond = EfhTradeCond::kISOI;
      break;

    case 'X':
      // Trades performed when the market is closed or crossed -> REG
      // FIXME: this is wrong, but it is what the old feed handler did.
      msg.tradeCond = EfhTradeCond::kREG;
      break;

    case 'P':
      // Trade done on a Complex Order Instrument -> MLET (Multi Leg
      // auto-electronic trade)
      // FIXME: not sure if this is correct
      msg.tradeCond = EfhTradeCond::kMLET;
      break;

    case 'I':
      // Trade involving an implied order or Leg Trade of a Complex Order
      // instrument -> TESL (Stock Options auto-electronic trade against
      // against single leg(s))
      // FIXME: not sure if this is correct
      msg.tradeCond = EfhTradeCond::kTESL;
      break;

    case 'S': /* Reference price (volume field zero filled) */
    case 'V': /* Volume adjustment -- was this deprecated? */
    case 'G': /* Contingent Trade, price of the trade was not controlled against the NBBO. */
    default:
      msg.tradeCond = EfhTradeCond::kUnmapped;
      break;
    }

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"Z ",sizeof(msgHdr->MsgType)) == 0) { // SystemTimeStamp
    const char* timeStamp = ((HsvfSystemTimeStamp*)msgBody)->TimeStamp;
    this->localTimeComponents.tm_hour = getNumField<uint64_t>(&timeStamp[0],2);
    this->localTimeComponents.tm_min  = getNumField<uint64_t>(&timeStamp[2],2);
    this->localTimeComponents.tm_sec  = getNumField<uint64_t>(&timeStamp[4],2);
    const uint64_t millis             = getNumField<uint64_t>(&timeStamp[6],3);

    gr_ts = std::mktime(&this->localTimeComponents) * 1'000'000'000 + millis * 1'000'000;
  } else if (memcmp(msgHdr->MsgType,"U ",sizeof(msgHdr->MsgType)) == 0) { // EndOfTransmission
    EKA_LOG("%s:%u End Of Transmission",EKA_EXCH_DECODE(exch),id);
    return true;
  } else if (memcmp(msgHdr->MsgType,"J ",sizeof(msgHdr->MsgType)) == 0) { // OptionInstrumentKeys
    //    if (op == EkaFhMode::SNAPSHOT) return false;
    if (op != EkaFhMode::DEFINITIONS) return false;

    auto boxMsg {reinterpret_cast<const HsvfOptionInstrumentKeys*>(msgBody)};

    const char* symb = boxMsg->InstrumentDescription;

    EfhOptionDefinitionMsg msg{};
    msg.header.msgType        = EfhMsgType::kOptionDefinition;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = charSymbol2SecurityId(symb);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = gr_ts;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = 0;
    msg.commonDef.securityType   = EfhSecurityType::kOption;
    msg.commonDef.exchange       = EfhExchange::kBOX;
    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate     = (2000 + getYear(symb)) * 10000 + getMonth(symb,getOptionType(symb)) * 100 + getDay(symb);
    msg.commonDef.contractSize   = 0;

    msg.optionType            = getOptionType(symb);
    msg.strikePrice           = getNumField<uint32_t>(&symb[8],7) * getFractionIndicator(symb[15]) / EFH_HSV_BOX_STRIKE_PRICE_SCALE;

    copySymbol(msg.commonDef.underlying,boxMsg->UnderlyingSymbolRoot);

    // In HSVF, we're given the "underlying symbol root," i.e., it might
    // contain a contract adjustment, e.g., ABBV1 instead of ABBV. This
    // is not what we expect in our API (the contract adjustment should
    // only be present in classSymbol, not underlying), so remove it.
    for (char *s = std::end(msg.commonDef.underlying) - 1;
         s >= std::begin(msg.commonDef.underlying); --s) {
      if (std::isdigit(*s))
        *s = '\0';
    }

    {
      char *s = stpncpy(msg.commonDef.classSymbol,symb,6);
      *s-- = '\0';
      while (*s == ' ')
        *s-- = '\0';
    }

    memcpy(msg.commonDef.exchSecurityName,boxMsg->GroupInstrument,sizeof(boxMsg->GroupInstrument));
    memcpy(msg.commonDef.exchSecurityName + sizeof(boxMsg->GroupInstrument),boxMsg->Instrument,     sizeof(boxMsg->Instrument));

#ifdef TEST_PRINT_DICT
    char avtSecName[32] = {};
    eka_create_avt_definition(avtSecName,&msg);
    fprintf(stderr,"\'%s\', %s, 0x%016jx,%ju\n",
	    std::string(boxMsg->InstrumentDescription,20).c_str(),avtSecName,
	    msg.header.securityId,msg.header.securityId);
#endif
    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"N ",sizeof(msgHdr->MsgType)) == 0) { // OptionSummary

    //===================================================
  } else if (memcmp(msgHdr->MsgType,"M ",sizeof(msgHdr->MsgType)) == 0) { // HsvfRfqStart
    auto boxMsg {reinterpret_cast<const HsvfRfqStart*>(msgBody)};

    SecurityIdT security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);
    s = book->findSecurity(security_id);
    if (!s)
      return false;

    EfhAuctionUpdateMsg msg{};
    msg.header.msgType        = EfhMsgType::kAuctionUpdate;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = gr_ts;
    msg.header.gapNum         = gapNum;

    msg.auctionId             = getNumField<uint32_t>(boxMsg->RfqId,sizeof(boxMsg->RfqId));
    msg.updateType            = EfhAuctionUpdateType::kNew;
    msg.side                  = getSide(boxMsg->Side, /*flipSide*/ true);
    msg.capacity              = EfhOrderCapacity::kBrokerDealer;
    msg.quantity              = getNumField<uint32_t>(boxMsg->Size,sizeof(boxMsg->Size));
    msg.price                 = getNumField<uint32_t>(boxMsg->Price,sizeof(boxMsg->Price)) * getFractionIndicator(boxMsg->PriceFractionIndicator);
    msg.endTimeNanos          = getExpireNs(&localTimeComponents, boxMsg->ExpiryTime);

    switch (boxMsg->AuctionType) {
    case 'G': msg.auctionType = EfhAuctionType::kPriceImprovementPeriod; break;
    case 'B': msg.auctionType = EfhAuctionType::kSolicitation; break;
    case 'C': msg.auctionType = EfhAuctionType::kFacilitation; break;
    default:
      on_error("Unexpected AuctionType == \'%c\'",boxMsg->AuctionType);
    }

    pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData, pEfhRunCtx->efhRunUserData);
  } else if (memcmp(msgHdr->MsgType,"O ",sizeof(msgHdr->MsgType)) == 0) { // HsvfRfqInsert
    auto boxMsg {reinterpret_cast<const HsvfRfqInsert*>(msgBody)};

    SecurityIdT security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);
    s = book->findSecurity(security_id);
    if (!s)
      return false;

    EfhAuctionUpdateMsg msg{};
    msg.header.msgType        = EfhMsgType::kAuctionUpdate;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = gr_ts;
    msg.header.gapNum         = gapNum;

    msg.updateType            = EfhAuctionUpdateType::kNew;
    msg.side                  = getSide(boxMsg->OrderSide, /*flipSide*/ boxMsg->OrderType == 'E');
    msg.quantity              = getNumField<uint32_t>(boxMsg->Size,sizeof(boxMsg->Size));
    msg.price                 = getNumField<uint32_t>(boxMsg->LimitPrice,sizeof(boxMsg->LimitPrice)) * getFractionIndicator(boxMsg->LimitPriceFractionIndicator);
    msg.endTimeNanos          = getExpireNs(&localTimeComponents, boxMsg->EndOfExposition);

    if (boxMsg->OrderType == 'A') { // Initial
      msg.auctionId             = getNumField<uint32_t>(boxMsg->RfqId,sizeof(boxMsg->RfqId));
      msg.auctionType           = EfhAuctionType::kPriceImprovementPeriod;
    } else if (boxMsg->OrderType == 'P') { // Exposed
      msg.auctionId             = getNumField<uint32_t>(boxMsg->OrderSequence,sizeof(boxMsg->OrderSequence));
      msg.auctionType           = EfhAuctionType::kExposed;
    } else {
      on_error("Unexpected OrderType == \'%c\'",boxMsg->OrderType);
    }

    switch (boxMsg->ClearingType) {
    case '6': msg.capacity = EfhOrderCapacity::kCustomer; break;
    case '7': msg.capacity = EfhOrderCapacity::kBrokerDealer; break;
    case '8': msg.capacity = EfhOrderCapacity::kMarketMaker; break;
    case 'T': msg.capacity = EfhOrderCapacity::kProfessionalCustomer; break;
    case 'W': msg.capacity = EfhOrderCapacity::kBrokerDealerAsCustomer; break;
    case 'X': msg.capacity = EfhOrderCapacity::kAwayMarketMaker; break;
    default:
      on_error("Unexpected ClearingType == \'%c\'",boxMsg->ClearingType);
    }

    pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData, pEfhRunCtx->efhRunUserData);
  } else if (memcmp(msgHdr->MsgType,"T ",sizeof(msgHdr->MsgType)) == 0) { // HsvfRfqDelete
    auto boxMsg {reinterpret_cast<const HsvfRfqDelete*>(msgBody)};

    if (boxMsg->DeletionType != '3') return false; // We are only interested in the PIP end ('3' = Deletion of all orders)
    if (boxMsg->AuctionType  == 'F') return false; // This is an exposed order deletion

    SecurityIdT security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);
    s = book->findSecurity(security_id);
    if (!s)
      return false;

    EfhAuctionUpdateMsg msg{};
    msg.header.msgType        = EfhMsgType::kAuctionUpdate;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = gr_ts;
    msg.header.gapNum         = gapNum;

    msg.auctionId             = getNumField<uint32_t>(boxMsg->RfqId,sizeof(boxMsg->RfqId));
    msg.updateType            = EfhAuctionUpdateType::kDelete;
    msg.side                  = getSide(boxMsg->OrderSide, /*flipSide*/ true);

    msg.quantity              = 0;
    msg.price                 = 0;
    msg.endTimeNanos          = 0;

    switch (boxMsg->AuctionType) {
    case 'G': msg.auctionType = EfhAuctionType::kPriceImprovementPeriod; break;
    case 'B': msg.auctionType = EfhAuctionType::kSolicitation; break;
    case 'C': msg.auctionType = EfhAuctionType::kFacilitation; break;
    default:
      on_error("Unexpected AuctionType == \'%c\'",boxMsg->AuctionType);
    }

    pEfhRunCtx->onEfhAuctionUpdateMsgCb(&msg, (EfhSecUserData) s->efhUserData, pEfhRunCtx->efhRunUserData);
  }
 
  //===================================================

  return false;
}

