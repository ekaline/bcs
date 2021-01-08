#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhGemGr.h"
#include "EkaFhGemParser.h"

static void eka_print_gem_msg(FILE* md_file, uint8_t* m, int gr, uint64_t sequence,uint64_t ts);
std::string ts_ns2str(uint64_t ts);

bool EkaFhGemGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op) {
  //  uint64_t ts = get_ts(m);
  uint64_t ts = EKA_GEM_TS(m);

  char enc =  (char)m[0];

  if (fh->print_parsed_messages) eka_print_gem_msg(parser_log,(uint8_t*)m,id,sequence,ts);

  if (op == EkaFhMode::DEFINITIONS && enc == 'M') return true;
  if (op == EkaFhMode::DEFINITIONS && enc != 'D') return false;
  if (op == EkaFhMode::SNAPSHOT    && enc == 'D') return false;

  //  EKA_LOG("enc = %c",enc);

  FhSecurity* s = NULL;

  switch (enc) {    
  case 'D':  { //GEM_TQF_TYPE_OPTION_DIRECTORY

    struct tqf_option_directory *message = (struct tqf_option_directory *)m;
 
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
    msg.expiryDate            = (message->expiration_year + 2000) * 10000 + message->expiration_month * 100 + message->expiration_day;
    msg.contractSize          = 0;
    msg.strikePrice           = be64toh(message->strike_price) / EFH_GEMX_STRIKE_PRICE_SCALE;
    msg.exchange              = EKA_GRP_SRC2EXCH(exch);
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    memcpy (&msg.underlying,message->underlying_symbol,std::min(sizeof(msg.underlying),sizeof(message->underlying_symbol)));
    memcpy (&msg.classSymbol,message->security_symbol,std::min(sizeof(msg.classSymbol),sizeof(message->security_symbol)));

    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;

  }
  case 'M': { // END OF SNAPSHOT
    struct tqf_snapshot_end *message = (struct tqf_snapshot_end *)m;
    char seq_num_str[21] = {};
    memcpy(seq_num_str, message->sequence_number, 20);
    seq_after_snapshot = strtoul(seq_num_str, NULL, 10);
    EKA_LOG("Glimpse snapshot_end_message (\'M\'): fh->gr[%u].seq_after_snapshot = %ju\n",id,seq_after_snapshot);
    return true;
  }

  case 'Q':    // tqf_best_bid_and_ask_update_long
  case 'q':  { // tqf_best_bid_and_ask_update_short 
    struct tqf_best_bid_and_ask_update_long  *message_long  = (struct tqf_best_bid_and_ask_update_long *)m;
    struct tqf_best_bid_and_ask_update_short *message_short = (struct tqf_best_bid_and_ask_update_short *)m;
    bool long_form = (enc == 'Q');

    SecurityIdT security_id = long_form ? be32toh(message_long->option_id) : be32toh(message_short->option_id);

    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    if (ts < s->bid_ts && ts < s->ask_ts) return false; // Back-in-time from Recovery

    if (ts >= s->bid_ts) {
      s->bid_size       = long_form ? be32toh(message_long->bid_size)          : (uint32_t) be16toh(message_short->bid_size);
      s->bid_cust_size  = long_form ? be32toh(message_long->bid_cust_size)     : (uint32_t) be16toh(message_short->bid_cust_size);
      s->bid_bd_size    = long_form ? be32toh(message_long->bid_pro_cust_size) : (uint32_t) be16toh(message_short->bid_pro_cust_size);

      s->bid_price   = long_form ? be32toh(message_long->bid_price)      : (uint32_t) be16toh(message_short->bid_price) * 100;
      s->bid_ts = ts;
    }
    if (ts >= s->ask_ts) {
      s->ask_size       = long_form ? be32toh(message_long->ask_size)          : (uint32_t) be16toh(message_short->ask_size);
      s->ask_cust_size  = long_form ? be32toh(message_long->ask_cust_size)     : (uint32_t) be16toh(message_short->ask_cust_size);
      s->ask_bd_size    = long_form ? be32toh(message_long->ask_pro_cust_size) : (uint32_t) be16toh(message_short->ask_pro_cust_size);

      s->ask_price   = long_form ? be32toh(message_long->ask_price)      : (uint32_t) be16toh(message_short->ask_price) * 100;
      s->ask_ts = ts;
    }
    break;

  }

  case 'A':
  case 'B':   // tqf_best_bid_or_ask_update_long
  case 'a':
  case 'b': { // tqf_best_bid_or_ask_update_short
    //    EKA_LOG("Im here");

    struct tqf_best_bid_or_ask_update_long  *message_long  = (struct tqf_best_bid_or_ask_update_long *)m;
    struct tqf_best_bid_or_ask_update_short *message_short = (struct tqf_best_bid_or_ask_update_short *)m;

    bool long_form = (enc == 'A') || (enc == 'B');

    SecurityIdT security_id = long_form ? be32toh(message_long->option_id) : be32toh(message_short->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    if (enc == 'B' || enc == 'b') {
      if (ts < s->bid_ts) return false; // Back-in-time from Recovery
      s->bid_size       = long_form ? be32toh(message_long->size)          : (uint32_t) be16toh(message_short->size);
      s->bid_cust_size  = long_form ? be32toh(message_long->cust_size)     : (uint32_t) be16toh(message_short->cust_size);
      s->bid_bd_size    = long_form ? be32toh(message_long->pro_cust_size) : (uint32_t) be16toh(message_short->pro_cust_size);


      s->bid_price   = long_form ? be32toh(message_long->price)      : (uint32_t) be16toh(message_short->price) * 100;
      s->bid_ts = ts;
    } else {
      if (ts < s->ask_ts) return false; // Back-in-time from Recovery
      s->ask_size       = long_form ? be32toh(message_long->size)          : (uint32_t) be16toh(message_short->size);
      s->ask_cust_size  = long_form ? be32toh(message_long->cust_size)     : (uint32_t) be16toh(message_short->cust_size);
      s->ask_bd_size    = long_form ? be32toh(message_long->pro_cust_size) : (uint32_t) be16toh(message_short->pro_cust_size);

      s->ask_price   = long_form ? be32toh(message_long->price)      : (uint32_t) be16toh(message_short->price) * 100;
      s->ask_ts = ts;
    }
    break;
  }

  case 'T': { // tqf_ticker
    struct tqf_ticker  *message = (struct tqf_ticker *)m;

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
    msg.price                 = be32toh(message->last_price);
    msg.size                  = be32toh(message->size);
    msg.tradeCond             = EKA_OPRA_TC_DECODE(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }

  case 'H': { // tqf_trading_action
    struct tqf_trading_action  *message = (struct tqf_trading_action *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    s->trading_action = message->current_trading_state == 'H' ? EfhTradeStatus::kHalted : 
      s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;

    break;
  }

  case 'O': { // tqf_security_open_close
    struct tqf_security_open_close  *message = (struct tqf_security_open_close *)m;
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
    return false;
  }
  if (s == NULL) on_error ("Trying to generate TOB update from s == NULL");
  book->generateOnQuote (pEfhRunCtx, s, sequence, ts, gapNum);

  return false;
}
 /* ##################################################################### */


/* static inline uint64_t get_ts(uint8_t* m) { */
/*   uint64_t ts_tmp = 0; */
/*   memcpy((uint8_t*)&ts_tmp+2,m+1,6); */
/*   return be64toh(ts_tmp); */
/* } */
 /* ##################################################################### */

static void eka_print_gem_msg(FILE* md_file, uint8_t* m, int gr, uint64_t sequence,uint64_t ts) {
  char enc = (char)m[0];
  fprintf(md_file,"%s,%ju,%c,",ts_ns2str(ts).c_str(),sequence,enc);

  switch (enc) {
  case 'Q':    // tqf_best_bid_and_ask_update_long
  case 'q':  { // tqf_best_bid_and_ask_update_short 
    tqf_best_bid_and_ask_update_long  *message_long  = (tqf_best_bid_and_ask_update_long *)m;
    tqf_best_bid_and_ask_update_short *message_short = (tqf_best_bid_and_ask_update_short *)m;
    bool long_form = (enc == 'Q');
    fprintf(md_file,"%u, %u,%u,%u,%u, %u,%u,%u,%u",
	    long_form ? be32toh(message_long->option_id)          :            be32toh(message_short->option_id),

	    long_form ? be32toh(message_long->bid_price)          : (uint32_t) be16toh(message_short->bid_price) * 100,
	    long_form ? be32toh(message_long->bid_size)           : (uint32_t) be16toh(message_short->bid_size),
	    long_form ? be32toh(message_long->bid_cust_size)      : (uint32_t) be16toh(message_short->bid_cust_size),
	    long_form ? be32toh(message_long->bid_pro_cust_size)  : (uint32_t) be16toh(message_short->bid_pro_cust_size),

	    long_form ? be32toh(message_long->ask_price)          : (uint32_t) be16toh(message_short->ask_price) * 100,
	    long_form ? be32toh(message_long->ask_size)           : (uint32_t) be16toh(message_short->ask_size),
	    long_form ? be32toh(message_long->ask_cust_size)      : (uint32_t) be16toh(message_short->ask_cust_size),
	    long_form ? be32toh(message_long->ask_pro_cust_size)  : (uint32_t) be16toh(message_short->ask_pro_cust_size)
	    );
    break;
  }

  case 'A':
  case 'B':   // tqf_best_bid_or_ask_update_long
  case 'a':
  case 'b': { // tqf_best_bid_or_ask_update_short
    //    EKA_LOG("Im here");

    tqf_best_bid_or_ask_update_long  *message_long  = (tqf_best_bid_or_ask_update_long *)m;
    tqf_best_bid_or_ask_update_short *message_short = (tqf_best_bid_or_ask_update_short *)m;

    bool long_form = (enc == 'A') || (enc == 'B');
    fprintf(md_file,"%u, %u,%u,%u,%u",
	    long_form ? be32toh(message_long->option_id)      :            be32toh(message_short->option_id),

	    long_form ? be32toh(message_long->price)          : (uint32_t) be16toh(message_short->price) * 100,
	    long_form ? be32toh(message_long->size)           : (uint32_t) be16toh(message_short->size),
	    long_form ? be32toh(message_long->cust_size)      : (uint32_t) be16toh(message_short->cust_size),
	    long_form ? be32toh(message_long->pro_cust_size)  : (uint32_t) be16toh(message_short->pro_cust_size)
	    );
    break;
  }

  case 'T': { // tqf_ticker
    tqf_ticker  *message = (tqf_ticker *)m;
    fprintf(md_file,"%u,%u,%u,%u,%c",
	    be32toh(message->option_id),
	    be32toh(message->last_price),
	    be32toh(message->size),
	    be32toh(message->volume),
	    message->trade_condition
	    );
    break;
  }

  case 'H': { // tqf_trading_action
    tqf_trading_action  *message = (tqf_trading_action *)m;
    fprintf(md_file,"%u,%c",
	    be32toh(message->option_id),
	    message->current_trading_state
	    );
    break;
  }

  case 'O': { // tqf_security_open_close
    tqf_security_open_close  *message = (tqf_security_open_close *)m;
    fprintf(md_file,"%u,%c",
	    be32toh(message->option_id),
	    message->open_state
	    );
    break;
  }

  default: 
    break;
    
  }
  fprintf(md_file,"\n");
  return;
}
