#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhParserCommon.h"
#include "EkaFhGemGr.h"
#include "EkaFhGemParser.h"

static void eka_print_gem_msg(FILE* md_file, const uint8_t* m, int gr, uint64_t sequence,uint64_t ts);
std::string ts_ns2str(uint64_t ts);

using namespace Gem;

bool EkaFhGemGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op,
				 std::chrono::high_resolution_clock::time_point startTime) {
  //  uint64_t ts = get_ts(m);
  uint64_t ts = EKA_GEM_TS(m);

  if (state == GrpState::NORMAL)
    checkTimeDiff(dev->deltaTimeLogFile,dev->midnightSystemClock,
		  ts,sequence);
    
  auto enc {static_cast<const char>(m[0])};

  if (fh->print_parsed_messages) eka_print_gem_msg(parser_log,(uint8_t*)m,id,sequence,ts);

  if (op == EkaFhMode::DEFINITIONS && enc == 'M') return true;
  if (op == EkaFhMode::DEFINITIONS && enc != 'D') return false;
  if (op == EkaFhMode::SNAPSHOT    && enc == 'D') return false;

  //  EKA_LOG("enc = %c",enc);

  FhSecurity* s = NULL;

  switch (enc) {    
  case 'D':  { //GEM_TQF_TYPE_OPTION_DIRECTORY

    auto message {reinterpret_cast<const option_directory *>(m)};

    EfhOptionDefinitionMsg msg{};
    msg.header.msgType        = EfhMsgType::kOptionDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = (EkaLSI)id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) be32toh(message->option_id);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    msg.commonDef.securityType   = EfhSecurityType::kOption;
    msg.commonDef.exchange       = EKA_GRP_SRC2EXCH(exch);
    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    msg.commonDef.expiryDate     = (message->expiration_year + 2000) * 10000 + message->expiration_month * 100 + message->expiration_day;
    msg.commonDef.contractSize   = 0;

    msg.strikePrice           = be64toh(message->strike_price) / EFH_GEMX_STRIKE_PRICE_SCALE;
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    copySymbol(msg.commonDef.underlying,message->underlying_symbol);
    copySymbol(msg.commonDef.classSymbol,message->security_symbol);

    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;

  }
  case 'M': { // END OF SNAPSHOT
    auto message {reinterpret_cast<const snapshot_end *>(m)};

    char seq_num_str[21] = {};
    memcpy(seq_num_str, message->sequence_number, 20);
    seq_after_snapshot = strtoul(seq_num_str, NULL, 10);
    EKA_LOG("Glimpse snapshot_end_message (\'M\'): fh->gr[%u].seq_after_snapshot = %ju\n",id,seq_after_snapshot);
    return true;
  }

  case 'Q':    // best_bid_and_ask_update_long
  case 'q':  { // best_bid_and_ask_update_short 
    auto message_long  {reinterpret_cast<const best_bid_and_ask_update_long  *>(m)};
    auto message_short {reinterpret_cast<const best_bid_and_ask_update_short *>(m)};

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
  case 'B':   // best_bid_or_ask_update_long
  case 'a':
  case 'b': { // best_bid_or_ask_update_short
    auto message_long  {reinterpret_cast<const best_bid_or_ask_update_long  *>(m)};
    auto message_short {reinterpret_cast<const best_bid_or_ask_update_short *>(m)};

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

  case 'T': { // ticker
    auto message {reinterpret_cast<const ticker *>(m)};

    SecurityIdT security_id = be32toh(message->option_id);
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
    msg.price                 = be32toh(message->last_price);
    msg.size                  = be32toh(message->size);
    msg.tradeCond             = static_cast<EfhTradeCond>(message->trade_condition);

    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }

  case 'H': { // trading_action
    auto message {reinterpret_cast<const trading_action *>(m)};

    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    s->trading_action = message->current_trading_state == 'H' ? EfhTradeStatus::kHalted : 
      s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;

    break;
  }

  case 'O': { // security_open_close
    auto message {reinterpret_cast<const security_open_close *>(m)};

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
  book->generateOnQuote (pEfhRunCtx, s, {}, sequence, ts, gapNum);

  return false;
}
 /* ##################################################################### */


/* static inline uint64_t get_ts(uint8_t* m) { */
/*   uint64_t ts_tmp = 0; */
/*   memcpy((uint8_t*)&ts_tmp+2,m+1,6); */
/*   return be64toh(ts_tmp); */
/* } */
 /* ##################################################################### */

static void eka_print_gem_msg(FILE* md_file, const uint8_t* m, int gr, uint64_t sequence,uint64_t ts) {
  auto enc {static_cast<const char>(m[0])};

  fprintf(md_file,"%s,%ju,%c,",ts_ns2str(ts).c_str(),sequence,enc);

  switch (enc) {
  case 'Q':    // best_bid_and_ask_update_long
  case 'q':  { // best_bid_and_ask_update_short 
    auto message_long  {reinterpret_cast<const best_bid_and_ask_update_long  *>(m)};
    auto message_short {reinterpret_cast<const best_bid_and_ask_update_short *>(m)};

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
  case 'B':   // best_bid_or_ask_update_long
  case 'a':
  case 'b': { // best_bid_or_ask_update_short
    auto message_long  {reinterpret_cast<const best_bid_or_ask_update_long  *>(m)};
    auto message_short {reinterpret_cast<const best_bid_or_ask_update_short *>(m)};

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

  case 'T': { // ticker
    auto message {reinterpret_cast<const ticker *>(m)};

    fprintf(md_file,"%u,%u,%u,%u,%c",
	    be32toh(message->option_id),
	    be32toh(message->last_price),
	    be32toh(message->size),
	    be32toh(message->volume),
	    message->trade_condition
	    );
    break;
  }

  case 'H': { // trading_action
    auto message {reinterpret_cast<const trading_action *>(m)};

    fprintf(md_file,"%u,%c",
	    be32toh(message->option_id),
	    message->current_trading_state
	    );
    break;
  }

  case 'O': { // security_open_close
    auto message {reinterpret_cast<const security_open_close *>(m)};

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
