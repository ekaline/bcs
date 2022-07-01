#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhParserCommon.h"
#include "EkaFhMiaxParser.h"
#include "EkaFhMiaxGr.h"

using namespace Tom;

bool EkaFhMiaxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op,
				 std::chrono::high_resolution_clock::time_point startTime) {

  auto enc {static_cast<const TOM_MSG>(*m)};

  uint64_t ts = 0;

  if (enc != TOM_MSG::EndOfRefresh) ts = gr_ts + ((TomCommon*) m)->Timestamp;

  FhSecurity* s = NULL;

  switch (enc) {    
    //--------------------------------------------------------------
  case TOM_MSG::Time : {
    auto message {reinterpret_cast<const TomCommon*>(m)};
    gr_ts = message->Timestamp * SEC_TO_NANO; 
    return false;
  }
    //--------------------------------------------------------------
  case TOM_MSG::SeriesUpdate : {
    auto message {reinterpret_cast<const TomSeriesUpdate*>(m)};

    uint32_t y = (message->Expiration[0] - '0')*1000 + (message->Expiration[1] - '0')*100 + (message->Expiration[2] - '0')*10 + (message->Expiration[3] - '0');
    uint16_t m = (message->Expiration[4] - '0')*10   + (message->Expiration[5] - '0');
    uint8_t  d = (message->Expiration[6] - '0')*10   + (message->Expiration[7] - '0');

    EfhOptionDefinitionMsg msg{};
    msg.header.msgType        = EfhMsgType::kOptionDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;

    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) message->security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    msg.commonDef.securityType   = EfhSecurityType::kOption;
    msg.commonDef.exchange       = EKA_GRP_SRC2EXCH(exch); //EfhExchange::kMIAX;
    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate     = y * 10000 + m * 100 + d;
    msg.commonDef.contractSize   = 0;

    msg.strikePrice           = message->StrikePrice / EFH_STRIKE_PRICE_SCALE;
    msg.optionType            = message->CallOrPut == 'P' ?  EfhOptionType::kPut : EfhOptionType::kCall;

    copySymbol(msg.commonDef.underlying, message->UnderlyingSymbol);
    copySymbol(msg.commonDef.classSymbol,message->SecuritySymbol);

    const uint underlyingIdx  = addUnderlying(msg.commonDef.underlying);
    msg.commonDef.opaqueAttrB = (uint64_t)underlyingIdx;

    /* EKA_TRACE("UnderlyingSymbol = %s, SecuritySymbol = %s,  message->Expiration = %s = %u,  message->security_id = %ju", */
    /* 	      (std::string(message->UnderlyingSymbol,sizeof(message->UnderlyingSymbol))).c_str(), */
    /* 	      (std::string(message->SecuritySymbol,sizeof(message->SecuritySymbol))).c_str(), */
    /* 	      (std::string(message->Expiration,sizeof(message->Expiration))).c_str(), */
    /* 	      msg.expiryDate, */
    /* 	      msg.header.securityId */
    /* 	      ); */
	      

    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case TOM_MSG::SystemState : {
    auto message {reinterpret_cast<const TomSystemState*>(m)};

    if (message->SystemStatus == 'S') market_open = true;
    if (message->SystemStatus == 'C') market_open = false;
    return false;
  }
    //--------------------------------------------------------------
  case TOM_MSG::UnderlyingTradingStatus : {
    auto message {reinterpret_cast<const TomUnderlyingTradingStatus*>(m)};

    char name2print[16] = {};
    EfhSymbol underlyingSymbol;
    copySymbol(underlyingSymbol, message->underlying);
    int underlIdx = findUnderlying(underlyingSymbol);
    if (underlIdx < 0) {
      EKA_LOG("%s:%u \'%s\' 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x (size = %ju) is not found (size for strncmp = %ju:",
	      EKA_EXCH_DECODE(exch),id,name2print,
	      message->underlying[0],message->underlying[1],
	      message->underlying[2],message->underlying[3],
	      message->underlying[4],message->underlying[5],
	      message->underlying[6],message->underlying[7],
	      sizeof(message->underlying),
	      std::min(sizeof(message->underlying),sizeof(EfhSymbol))
	      );

      for (uint u = 0; u < underlyingNum; u++) {
	EKA_LOG("%3d: \'%s\' 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ",
		u,underlying[u]->name,
		underlying[u]->name[0],underlying[u]->name[1],underlying[u]->name[2],underlying[u]->name[3],
		underlying[u]->name[4],underlying[u]->name[5],underlying[u]->name[6],underlying[u]->name[7]
		);
      }
      on_error("Underlying \'%s\' is not found",name2print);
    }

    underlying[underlIdx]->tradeStatus = message->trading_status == 'H' ? EfhTradeStatus::kHalted : EfhTradeStatus::kNormal;

    //    EKA_LOG("UnderlyingTradingStatus of %s : \'%c\'", name2print,message->trading_status);
    return false;
  }
    //--------------------------------------------------------------
  case TOM_MSG::BestBidShort : 
  case TOM_MSG::BestAskShort : 
  case TOM_MSG::BestBidLong : 
  case TOM_MSG::BestAskLong : {
    auto message_long {reinterpret_cast <const TomBestBidOrOfferLong* >(m)};
    auto message_short {reinterpret_cast<const TomBestBidOrOfferShort*>(m)};

    bool long_form = (enc == TOM_MSG::BestBidLong) || (enc == TOM_MSG::BestAskLong);

    SecurityIdT security_id = long_form ? message_long->security_id : message_short->security_id;

    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    if (underlying[s->underlyingIdx] == NULL)
      on_error("underlying[%u] == NULL",s->underlyingIdx);

    if (enc == TOM_MSG::BestBidShort || enc == TOM_MSG::BestBidLong) {
      //      if (ts < s->bid_ts) return false; // Back-in-time from Recovery
      s->bid_size       = long_form ? message_long->size           : (uint32_t)  message_short->size;
      s->bid_cust_size  = long_form ? message_long->customer_size  : (uint32_t)  message_short->customer_size;
      s->bid_price      = long_form ? message_long->price          : (uint32_t) (message_short->price * 100);
      s->bid_ts = ts;
    } else {
      //      if (ts < s->ask_ts) return false; // Back-in-time from Recovery
      s->ask_size       = long_form ? message_long->size           : (uint32_t)  message_short->size;
      s->ask_cust_size  = long_form ? message_long->customer_size  : (uint32_t)  message_short->customer_size;
      s->ask_price      = long_form ? message_long->price          : (uint32_t) (message_short->price * 100);
      s->ask_ts = ts;
    }
    bool setHalted = long_form ? message_long->condition == 'T' : message_short->condition == 'T';
    //    s->trading_action = setHalted ? EfhTradeStatus::kHalted : EfhTradeStatus::kNormal;
    s->trading_action = setHalted ? EfhTradeStatus::kHalted : underlying[s->underlyingIdx]->tradeStatus;

    s->option_open = market_open;
    break;
  }
    //--------------------------------------------------------------

  case TOM_MSG::BestBidAskShort :   
  case TOM_MSG::BestBidAskLong :  { 
    auto message_long {reinterpret_cast <const TomBestBidAndOfferLong* >(m)};
    auto message_short {reinterpret_cast<const TomBestBidAndOfferShort*>(m)};

    bool long_form = enc == TOM_MSG::BestBidAskLong;

    SecurityIdT security_id = long_form ? message_long->security_id : message_short->security_id;
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    if (underlying[s->underlyingIdx] == NULL)
      on_error("underlying[%u] == NULL",s->underlyingIdx);

    //    if (ts < s->bid_ts && ts < s->ask_ts) return false; // Back-in-time from Recovery

    s->bid_size       = long_form ? message_long->bid_size             : (uint32_t)  message_short->bid_size;
    s->bid_cust_size  = long_form ? message_long->bid_customer_size    : (uint32_t)  message_short->bid_customer_size;
    s->bid_price      = long_form ? message_long->bid_price            : (uint32_t) (message_short->bid_price * 100);
    s->bid_ts         = ts;
    bool setHaltedBid = long_form ? message_long->bid_condition == 'T' : message_short->bid_condition == 'T';

    s->ask_size       = long_form ? message_long->ask_size             : (uint32_t)  message_short->ask_size;
    s->ask_cust_size  = long_form ? message_long->ask_customer_size    : (uint32_t)  message_short->ask_customer_size;
    s->ask_price      = long_form ? message_long->ask_price            : (uint32_t) (message_short->ask_price * 100);
    s->ask_ts         = ts;
    bool setHaltedAsk = long_form ? message_long->ask_condition == 'T' : message_short->ask_condition == 'T';

    s->trading_action = setHaltedBid || setHaltedAsk ? EfhTradeStatus::kHalted : underlying[s->underlyingIdx]->tradeStatus;
    s->option_open    = market_open;
    break;
  }
    //--------------------------------------------------------------
  case TOM_MSG::Trade: { 
    auto message {reinterpret_cast<const TomTrade*>(m)};

    SecurityIdT security_id = message->security_id;
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    EfhTradeMsg msg{};
    msg.header.msgType        = EfhMsgType::kTrade;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;
    msg.header.securityId     = (uint64_t) security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = ts;
    msg.header.gapNum         = gapNum;
    msg.price                 = message->price;
    msg.size                  = message->size;
    msg.tradeCond             = static_cast<EfhTradeCond>(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case TOM_MSG::TradeCancel : 
    return false;
    //--------------------------------------------------------------
  case TOM_MSG::EndOfRefresh : 
    EKA_LOG("%s:%u End Of Refresh of \'%c\' Request",EKA_EXCH_DECODE(exch),id,m[1]);
    return true;
    //--------------------------------------------------------------

  default: 
    on_error("Unexpected message: %c",(char)enc);
    return false;
  }
  if (op == EkaFhMode::DEFINITIONS) return false;

  if (s == NULL) on_error ("Trying to generate TOB update from s == NULL");
  book->generateOnQuote (pEfhRunCtx, s, {}, sequence, ts,gapNum);

  return false;
}

