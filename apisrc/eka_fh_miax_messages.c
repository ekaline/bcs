#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "eka_fh_book.h"
#include "eka_data_structs.h"
#include "eka_fh_group.h"
#include "EkaDev.h"

#include "eka_fh_miax_messages.h"

bool FhMiaxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {

  EKA_MIAX_TOM_MSG enc =  (EKA_MIAX_TOM_MSG)m[0];

  uint64_t ts = 0;

  if (enc != EKA_MIAX_TOM_MSG::EndOfRefresh) ts = gr_ts + ((TomCommon*) m)->Timestamp;

  fh_b_security* tob_s = NULL;

  switch (enc) {    
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::Time : {
    //    gr_ts = (((TomCommon*) m)->Timestamp - 5 * 60 * 60) * SEC_TO_NANO; // shifted 5 hours for UTC
    gr_ts = ((TomCommon*) m)->Timestamp * SEC_TO_NANO; 
    return false;
  }
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::SeriesUpdate : {
    TomSeriesUpdate *message = (TomSeriesUpdate*) m;

    uint32_t y = (message->Expiration[0] - '0')*1000 + (message->Expiration[1] - '0')*100 + (message->Expiration[2] - '0')*10 + (message->Expiration[3] - '0');
    uint16_t m = (message->Expiration[4] - '0')*10   + (message->Expiration[5] - '0');
    uint8_t  d = (message->Expiration[6] - '0')*10   + (message->Expiration[7] - '0');

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;

    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) message->security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = {(EkaSource)0,EkaLSI(0)};
    msg.securityType          = EfhSecurityType::kOpt;
    msg.expiryDate            = y * 10000 + m * 100 + d;
    msg.contractSize          = 0;
    msg.strikePrice           = message->StrikePrice / EFH_STRIKE_PRICE_SCALE;
    msg.exchange              = EKA_GRP_SRC2EXCH(exch); //EfhExchange::kMIAX;
    msg.optionType            = message->CallOrPut == 'P' ?  EfhOptionType::kPut : EfhOptionType::kCall;

    memcpy (&msg.underlying, message->UnderlyingSymbol,std::min(sizeof(msg.underlying), sizeof(message->UnderlyingSymbol)));
    memcpy (&msg.classSymbol,message->SecuritySymbol,  std::min(sizeof(msg.classSymbol),sizeof(message->SecuritySymbol)));

    uint underlyingIdx = book->addUnderlying(msg.underlying, sizeof(msg.underlying));
    msg.opaqueAttrB = (uint64_t)underlyingIdx;

    /* EKA_TRACE("UnderlyingSymbol = %s, SecuritySymbol = %s,  message->Expiration = %s = %u,  message->security_id = %ju", */
    /* 	      (std::string(message->UnderlyingSymbol,sizeof(message->UnderlyingSymbol))).c_str(), */
    /* 	      (std::string(message->SecuritySymbol,sizeof(message->SecuritySymbol))).c_str(), */
    /* 	      (std::string(message->Expiration,sizeof(message->Expiration))).c_str(), */
    /* 	      msg.expiryDate, */
    /* 	      msg.header.securityId */
    /* 	      ); */
	      

    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::SystemState : 
    if (((TomSystemState*)m)->SystemStatus == 'S') market_open = true;
    if (((TomSystemState*)m)->SystemStatus == 'C') market_open = false;
    return false;
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::UnderlyingTradingStatus : {
    
    char name2print[16] = {};
    memcpy(name2print,((TomUnderlyingTradingStatus*)m)->underlying,sizeof(((TomUnderlyingTradingStatus*)m)->underlying));
    int underlIdx = book->findUnderlying(((TomUnderlyingTradingStatus*)m)->underlying,
					 std::min(sizeof(((TomUnderlyingTradingStatus*)m)->underlying),sizeof(EfhSymbol)));
    if (underlIdx < 0) {
      EKA_LOG("%s:%u \'%s\' 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x (size = %u) is not found (size for strncmp = %u:",
	      EKA_EXCH_DECODE(exch),id,name2print,
	      ((TomUnderlyingTradingStatus*)m)->underlying[0],((TomUnderlyingTradingStatus*)m)->underlying[1],
	      ((TomUnderlyingTradingStatus*)m)->underlying[2],((TomUnderlyingTradingStatus*)m)->underlying[3],
	      ((TomUnderlyingTradingStatus*)m)->underlying[4],((TomUnderlyingTradingStatus*)m)->underlying[5],
	      ((TomUnderlyingTradingStatus*)m)->underlying[6],((TomUnderlyingTradingStatus*)m)->underlying[7],
	      sizeof(((TomUnderlyingTradingStatus*)m)->underlying),
	      std::min(sizeof(((TomUnderlyingTradingStatus*)m)->underlying),sizeof(EfhSymbol))
	      );

      for (uint u = 0; u < book->underlyingNum; u++) {
	EKA_LOG("%3d: \'%s\' 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ",
		u,book->underlying[u]->name,
		book->underlying[u]->name[0],book->underlying[u]->name[1],book->underlying[u]->name[2],book->underlying[u]->name[3],
		book->underlying[u]->name[4],book->underlying[u]->name[5],book->underlying[u]->name[6],book->underlying[u]->name[7]
		);
      }
      on_error("Underlying \'%s\' is not found",name2print);
    }

    book->underlying[underlIdx]->tradeStatus = ((TomUnderlyingTradingStatus*)m)->trading_status == 'H' ? EfhTradeStatus::kHalted : EfhTradeStatus::kNormal;

    //    EKA_LOG("UnderlyingTradingStatus of %s : \'%c\'", name2print,((TomUnderlyingTradingStatus*)m)->trading_status);
    return false;
  }
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::BestBidShort : 
  case EKA_MIAX_TOM_MSG::BestAskShort : 
  case EKA_MIAX_TOM_MSG::BestBidLong : 
  case EKA_MIAX_TOM_MSG::BestAskLong : {
    TomBestBidOrOfferLong  *message_long  = (TomBestBidOrOfferLong  *)m;
    TomBestBidOrOfferShort *message_short = (TomBestBidOrOfferShort *)m;

    bool long_form = (enc == EKA_MIAX_TOM_MSG::BestBidLong) || (enc == EKA_MIAX_TOM_MSG::BestAskLong);

    uint32_t security_id = long_form ? message_long->security_id : message_short->security_id;

    fh_b_security* s = book->find_security(security_id);
    if (s == NULL && !book->subscribe_all) return false;
    if (s == NULL &&  book->subscribe_all) s = book->subscribe_security(security_id,0,0,0,0);

    if (s == NULL) on_error("s == NULL");    
    if (book->underlying[s->underlyingIdx] == NULL)
      on_error("underlying[%u] == NULL",s->underlyingIdx);

    if (enc == EKA_MIAX_TOM_MSG::BestBidShort || enc == EKA_MIAX_TOM_MSG::BestBidLong) {
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
    s->trading_action = setHalted ? EfhTradeStatus::kHalted : book->underlying[s->underlyingIdx]->tradeStatus;

    s->option_open = market_open;
    tob_s = s;
    break;
  }
    //--------------------------------------------------------------

  case EKA_MIAX_TOM_MSG::BestBidAskShort :   
  case EKA_MIAX_TOM_MSG::BestBidAskLong :  { 
    TomBestBidAndOfferLong  *message_long  = (TomBestBidAndOfferLong  *)m;
    TomBestBidAndOfferShort *message_short = (TomBestBidAndOfferShort *)m;

    bool long_form = enc == EKA_MIAX_TOM_MSG::BestBidAskLong;

    bool setHalted = false;

    uint32_t security_id = long_form ? message_long->security_id : message_short->security_id;
    fh_b_security* s = book->find_security(security_id);
    if (s == NULL && !book->subscribe_all) return false;
    if (s == NULL && book->subscribe_all) s = book->subscribe_security(security_id,0,0,0,0);

    if (s == NULL) on_error("s == NULL");    
    if (book->underlying[s->underlyingIdx] == NULL)
      on_error("underlying[%u] == NULL",s->underlyingIdx);

    //    if (ts < s->bid_ts && ts < s->ask_ts) return false; // Back-in-time from Recovery

    if (ts >= s->bid_ts) {
      s->bid_size       = long_form ? message_long->bid_size             : (uint32_t)  message_short->bid_size;
      s->bid_cust_size  = long_form ? message_long->bid_customer_size    : (uint32_t)  message_short->bid_customer_size;
      s->bid_price      = long_form ? message_long->bid_price            : (uint32_t) (message_short->bid_price * 100);
      s->bid_ts         = ts;
      setHalted         = long_form ? message_long->bid_condition == 'T' : message_short->bid_condition == 'T';
    }
    if (ts >= s->ask_ts) {
      s->ask_size       = long_form ? message_long->ask_size             : (uint32_t)  message_short->ask_size;
      s->ask_cust_size  = long_form ? message_long->ask_customer_size    : (uint32_t)  message_short->ask_customer_size;
      s->ask_price      = long_form ? message_long->ask_price            : (uint32_t) (message_short->ask_price * 100);
      s->ask_ts         = ts;
      setHalted         = long_form ? message_long->ask_condition == 'T' : message_short->ask_condition == 'T';
    }
    //    s->trading_action = setHalted ? EfhTradeStatus::kHalted : EfhTradeStatus::kNormal;
    s->trading_action = setHalted ? EfhTradeStatus::kHalted : book->underlying[s->underlyingIdx]->tradeStatus;
    s->option_open = market_open;
    tob_s = s;
    break;
  }
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::Trade: { 
    TomTrade *message = (TomTrade *)m;

    uint32_t security_id = message->security_id;
    fh_b_security* s = book->find_security(security_id);
    if (s == NULL && !book->subscribe_all) return false;
    if (s == NULL && book->subscribe_all) s = book->subscribe_security(security_id,0,0,0,0);

    EfhTradeMsg msg = {};
    msg.header.msgType        = EfhMsgType::kTrade;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;
    msg.header.securityId     = (uint64_t) security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = ts;
    msg.header.gapNum         = gapNum;
    msg.price                 = message->price;
    msg.size                  = message->size;
    msg.tradeCond             = EKA_OPRA_TC_DECODE(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::TradeCancel : 
    return false;
    //--------------------------------------------------------------
  case EKA_MIAX_TOM_MSG::EndOfRefresh : 
    EKA_LOG("%s:%u End Of Refresh of \'%c\' Request",EKA_EXCH_DECODE(exch),id,m[1]);
    return true;
    //--------------------------------------------------------------

  default: 
    on_error("Unexpected message: %c",(char)enc);
    return false;
  }
  if (op == EkaFhMode::DEFINITIONS) return false;

  if (tob_s == NULL) on_error ("Trying to generate TOB update from tob_s == NULL");
  book->generateOnQuote (pEfhRunCtx, tob_s, sequence, ts,gapNum);

  return false;
}

