#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhPhlxTopoParser.h"
#include "EkaFhPhlxTopoGr.h"

bool EkaFhPhlxTopoGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  char enc =  (char)m[0];

  if (op == EkaFhMode::DEFINITIONS && 
      ( enc == 'a' ||
	enc == 'b' ||
	enc == 'A' ||
	enc == 'B' ||
	enc == 'Q' ||
	enc == 'q' 
	)) return true; // ending the Definitions snapshot when getting first MD

  if (op == EkaFhMode::DEFINITIONS && enc != 'D') return false;
  if (op == EkaFhMode::SNAPSHOT    && enc == 'D') return false;

  uint64_t ts = gr_ts + be32toh(((topo_generic_hdr*) m)->time_nano);

  FhSecurity* s = NULL;

  switch (enc) {    
  case 'T':  { // topo_time_stamp
    struct topo_time_stamp  *message = (struct topo_time_stamp *)m;
    gr_ts = ((uint64_t) be32toh(message->time_seconds)) * SEC_TO_NANO; 
    return false;
  }
  case 'S':  { // topo_system_event
    //    struct topo_system_event  *message = (struct topo_system_event *)m;
    //    uint8_t  event_code = message->event_code;
    return false;
  }
  case 'D':  { // topo_option_directory
    struct topo_option_directory  *message = (struct topo_option_directory *)m;

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) be32toh(message->option_id);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = 0;
    msg.securityType          = EfhSecurityType::kOpt;
    msg.expiryDate            = (2000 + message->expiration_year) * 10000 + message->expiration_month * 100 + message->expiration_day;
    msg.contractSize          = 0;
    msg.strikePrice           = be32toh(message->strike_price) / EFH_PHLX_STRIKE_PRICE_SCALE;
    msg.exchange              = EfhExchange::kPHLX;
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    memcpy (&msg.underlying,message->underlying_symbol,std::min(sizeof(msg.underlying),sizeof(message->underlying_symbol)));
    memcpy (&msg.classSymbol,message->security_symbol,std::min(sizeof(msg.classSymbol),sizeof(message->security_symbol)));

    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }

  case 'q':    // topo_option_best_bid_and_ask_update_short
  case 'Q':  { // topo_option_best_bid_and_ask_update_long
    struct topo_option_best_bid_and_ask_update_long  *message_long  = (struct topo_option_best_bid_and_ask_update_long *)m;
    struct topo_option_best_bid_and_ask_update_short *message_short = (struct topo_option_best_bid_and_ask_update_short *)m;

    bool long_form = enc == 'Q';

    SecurityIdT security_id = long_form ? be32toh(message_long->option_id) : be32toh(message_short->option_id);
    s = book->find_security(security_id);
    if (s == NULL) return false;

    if (ts < s->bid_ts && ts < s->ask_ts) return false; // Back-in-time from Recovery

    if (ts >= s->bid_ts) {
      s->bid_size  = long_form ? be32toh(message_long->bid_size)  : (uint32_t) be16toh(message_short->bid_size);
      s->bid_price = long_form ? be32toh(message_long->bid_price) : (uint32_t) be16toh(message_short->bid_price) * 100;
      s->bid_ts = ts;
    }
    if (ts >= s->ask_ts) {
      s->ask_size  = long_form ? be32toh(message_long->ask_size)  : (uint32_t) be16toh(message_short->ask_size);
      s->ask_price = long_form ? be32toh(message_long->ask_price) : (uint32_t) be16toh(message_short->ask_price) * 100;
      s->ask_ts = ts;
    }
    break;

  }

  case 'a':
  case 'b':   // topo_option_best_bid_or_ask_update_short
  case 'A':
  case 'B': { // topo_option_best_bid_or_ask_update_long
    struct topo_option_best_bid_or_ask_update_long  *message_long  = (struct topo_option_best_bid_or_ask_update_long *)m;
    struct topo_option_best_bid_or_ask_update_short *message_short = (struct topo_option_best_bid_or_ask_update_short *)m;

    bool long_form = (enc == 'A') || (enc == 'B');

    SecurityIdT security_id = long_form ? be32toh(message_long->option_id) : be32toh(message_short->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    if (enc == 'B' || enc == 'b') {
      if (ts < s->bid_ts) return false; // Back-in-time from Recovery
      s->bid_size  = long_form ? be32toh(message_long->size)  : (uint32_t) be16toh(message_short->size);
      s->bid_price = long_form ? be32toh(message_long->price) : (uint32_t) be16toh(message_short->price) * 100;
      s->bid_ts = ts;
    } else {
      if (ts < s->ask_ts) return false; // Back-in-time from Recovery
      s->ask_size  = long_form ? be32toh(message_long->size)  : (uint32_t) be16toh(message_short->size);
      s->ask_price = long_form ? be32toh(message_long->price) : (uint32_t) be16toh(message_short->price) * 100;
      s->ask_ts = ts;
    }
    break;
  }

  case 'R': { // topo_option_trade_report
    struct topo_option_trade_report  *message = (struct topo_option_trade_report *)m;

    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    EfhTradeMsg msg = {};
    msg.header.msgType        = EfhMsgType::kTrade;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;
    msg.header.securityId     = (uint64_t) security_id;
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = ts;
    msg.header.gapNum         = gapNum;
    msg.price                 = be32toh(message->price);
    msg.size                  = be32toh(message->size);
    msg.tradeCond             = EKA_OPRA_TC_DECODE(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }

  case 'X': { // topo_option_broken_trade_report
    return false;
  }

  case 'H': { // topo_option_trading_action
    struct topo_option_trading_action  *message = (struct topo_option_trading_action *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    s->trading_action = message->current_trading_state == 'H' ? EfhTradeStatus::kHalted : 
      s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;

    break;
  }

  case 'O': { // topo_option_security_open_closed
    struct topo_option_security_open_closed  *message = (struct topo_option_security_open_closed *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    if (message->open_state ==  'Y') {
      s->option_open = true;
      s->trading_action = EfhTradeStatus::kNormal;
    } else {
      s->option_open = false;
      s->trading_action = EfhTradeStatus::kClosed;
    }

    break;
  }

  default: 
    EKA_WARN("WARNING: Unexpected message: %c",enc);
    return false;
  }
  if (s == NULL) on_error ("Trying to generate TOB update from s == NULL");
  if (op != EkaFhMode::SNAPSHOT)
    book->generateOnQuote (pEfhRunCtx, s, sequence, ts,gapNum);

  return false;
}

