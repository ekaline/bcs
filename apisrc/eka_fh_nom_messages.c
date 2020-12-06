#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <algorithm>

#include "eka_fh.h"
#include "eka_fh_nom_messages.h"

static void eka_print_nom_msg(FILE* md_file, uint8_t* m, int gr, uint64_t sequence);
static inline uint64_t get_ts(uint8_t* m);
std::string ts_ns2str(uint64_t ts);

bool FhNomGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  EKA_FH_ERR_CODE err = EKA_FH_RESULT__OK;
#ifdef EKA_TIME_CHECK
  auto start = std::chrono::high_resolution_clock::now();  
#endif

  char enc =  (char)m[0];
  uint64_t msg_timestamp = get_ts(m);

  fh_b_security_state prev_s;
  prev_s.reset();

  if (op == EkaFhMode::DEFINITIONS && enc == 'M') return true;
  if ((op == EkaFhMode::DEFINITIONS && enc != 'R') || (op == EkaFhMode::SNAPSHOT    && enc == 'R')) return false;

  fh_b_security* s = NULL;

  switch (enc) {    
  case 'R':  { //ITTO_TYPE_OPTION_DIRECTORY 
    struct itto_definition *message = (struct itto_definition *)m;

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = EkaSource::kNOM_ITTO;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) be32toh(message->option_id);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = 0;
    msg.securityType          = EfhSecurityType::kOpt;
    msg.expiryDate            = (2000 + message->expiration_year) * 10000 + message->expiration_month * 100 + message->expiration_day;
    msg.contractSize          = 0;
    msg.strikePrice           = be32toh(message->strike_price) / EFH_NOM_STRIKE_PRICE_SCALE;
    msg.exchange              = EfhExchange::kNOM;
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    memcpy (&msg.underlying,message->underlying_symbol,std::min(sizeof(msg.underlying),sizeof(message->underlying_symbol)));
    memcpy (&msg.classSymbol,message->security_symbol,std::min(sizeof(msg.classSymbol),sizeof(message->security_symbol)));

    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }
  case 'M': { // END OF SNAPSHOT
    struct itto_end_of_snapshot *message = (struct itto_end_of_snapshot*)m;
    char seq_num_str[21] = {};
    memcpy(seq_num_str, message->sequence_number, 20);
    seq_after_snapshot = (op == EkaFhMode::SNAPSHOT) ? strtoul(seq_num_str, NULL, 10) : 0;
    EKA_LOG("Glimpse snapshot_end_message (\'M\'): fh->gr[%u].seq_after_snapshot = %ju",id,seq_after_snapshot);
    return true;
  }
    //--------------------------------------------------------------
  case 'a':  {  //NOM_ADD_ORDER_SHORT
    struct itto_add_order_short *message = (struct itto_add_order_short *)m;
    uint32_t security_id = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((NomBook*)book)->subscribe_all) return false;
      s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);
    }
    uint64_t order_id = be64toh(message->order_reference_delta);
    uint32_t size =  (uint32_t) be16toh (message->size);
    uint32_t price = (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE; // Short Price representation
    char side = (char)message->side;

    prev_s.set(s);
    ((NomBook*)book)->add_order2book(s,order_id,fh_b_order::type_t::CUSTOMER,price,size,side);
    break;
  }
    //--------------------------------------------------------------
  case 'A' : { //NOM_ADD_ORDER_LONG
    struct itto_add_order_long *message = (struct itto_add_order_long *)m;
    uint32_t security_id = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((NomBook*)book)->subscribe_all) return false;
      s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);
    }
    uint64_t order_id = be64toh(message->order_reference_delta);
    uint32_t size =  be32toh (message->size);
    uint32_t price = be32toh (message->price) / EFH_PRICE_SCALE;
    char side = (char)message->side;

    prev_s.set(s);
    ((NomBook*)book)->add_order2book(s,order_id,fh_b_order::type_t::CUSTOMER,price,size,side);
    break;
  }
    //--------------------------------------------------------------
  case 'S': { //NOM_SYSTEM_EVENT
    return false;
  }
    //--------------------------------------------------------------
  case 'L': { //NOM_BASE_REF -- do nothing        
    return false;
  }
    //--------------------------------------------------------------
  case 'H': { //NOM_TRADING_ACTION 
    struct itto_trading_action *message = (struct itto_trading_action *)m;
    uint32_t security_id = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((NomBook*)book)->subscribe_all) return false;
      s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);
      return false;
    }

    prev_s.set(s);
    s->trading_action = ITTO_NOM_TRADING_ACTION(message->trading_state);
    break;
  }
  case 'O': { //NOM_OPTION_OPEN 
    struct itto_option_open *message = (struct itto_option_open *)m;
    uint32_t security_id = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((NomBook*)book)->subscribe_all) return false;
      s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);
    }

    prev_s.set(s);
    if (s != NULL) s->option_open = (message->open_state == 'Y');
    if (s != NULL && message->open_state == 'N') s->trading_action = EfhTradeStatus::kClosed;
    break;
  }
    //--------------------------------------------------------------
  case 'J': {  //NOM_ADD_QUOTE_LONG
    struct itto_add_quote_long *message = (struct itto_add_quote_long *)m;

    uint32_t security_id  = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);

    if (s == NULL) {
      if (!((NomBook*)book)->subscribe_all) return false;
      s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);
    }
    uint64_t bid_order_id = be64toh(message->bid_reference_delta);
    uint32_t bid_price    = be32toh(message->bid_price) / EFH_PRICE_SCALE;
    uint32_t bid_size     = be32toh(message->bid_size);
    uint64_t ask_order_id = be64toh(message->ask_reference_delta);
    uint32_t ask_price    = be32toh(message->ask_price) / EFH_PRICE_SCALE;
    uint32_t ask_size     = be32toh(message->ask_size);

    prev_s.set(s);
    ((NomBook*)book)->add_order2book(s,bid_order_id,fh_b_order::type_t::BD,bid_price,bid_size,'B');
    ((NomBook*)book)->add_order2book(s,ask_order_id,fh_b_order::type_t::BD,ask_price,ask_size,'S');
    break;
  }
    //--------------------------------------------------------------
  case 'j': { //NOM_ADD_QUOTE_SHORT
    struct itto_add_quote_short *message = (struct itto_add_quote_short *)m;

    uint32_t security_id = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((NomBook*)book)->subscribe_all) return false;
      s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);
    }

    uint64_t bid_order_id =            be64toh(message->bid_reference_delta);
    uint32_t bid_price    = (uint32_t) be16toh(message->bid_price) * 100 / EFH_PRICE_SCALE;
    uint32_t bid_size     = (uint32_t) be16toh(message->bid_size);
    uint64_t ask_order_id =            be64toh(message->ask_reference_delta);
    uint32_t ask_price    = (uint32_t) be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE;
    uint32_t ask_size     = (uint32_t) be16toh(message->ask_size);

    prev_s.set(s);
    ((NomBook*)book)->add_order2book(s,bid_order_id,fh_b_order::type_t::BD,bid_price,bid_size,'B');
    ((NomBook*)book)->add_order2book(s,ask_order_id,fh_b_order::type_t::BD,ask_price,ask_size,'S');    
    break;
  }
    //--------------------------------------------------------------
  case 'E':  { //NOM_SINGLE_SIDE_EXEC
    struct itto_executed *message = (struct itto_executed *)m;
    uint64_t order_id   = be64toh(message->order_reference_delta);
    uint32_t delta_size = be32toh(message->executed_contracts);
    fh_b_order* o = ((NomBook*)book)->find_order(order_id);

    if (o == NULL) return false;

    if (o->plevel == NULL) on_error("NOM_SINGLE_SIDE_EXEC: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_SINGLE_SIDE_EXEC: o->plevel->s == NULL");
    s = o->plevel->s;

    prev_s.set(s);
    err = ((NomBook*)book)->change_order_size (o,delta_size,1);    
    break;
  }
    //--------------------------------------------------------------
  case 'C': { //NOM_SINGLE_SIDE_EXEC_PRICE
    struct itto_executed_price *message = (struct itto_executed_price *)m;
    uint64_t order_id   = be64toh(message->order_reference_delta);
    uint32_t delta_size = be32toh(message->size);

    fh_b_order* o = ((NomBook*)book)->find_order(order_id);

    if (o == NULL) return false;

    if (o->plevel == NULL) on_error("NOM_SINGLE_SIDE_EXEC_PRICE: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_SINGLE_SIDE_EXEC_PRICE: o->plevel->s == NULL");
    s = o->plevel->s;

    prev_s.set(s);
    err = ((NomBook*)book)->change_order_size (o,delta_size,1);    
    break;
  }

  //--------------------------------------------------------------
  case 'X': { //NOM_ORDER_CANCEL
    struct itto_order_cancel *message = (struct itto_order_cancel *)m;
    uint64_t order_id = be64toh(message->order_reference_delta);
    uint32_t delta_size = be32toh(message->cancelled_orders);
 
    fh_b_order* o = ((NomBook*)book)->find_order(order_id);
    if (o == NULL)  return false;
    if (o->plevel == NULL) on_error("NOM_ORDER_CANCEL: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_ORDER_CANCEL: o->plevel->s == NULL");
    s = o->plevel->s;

    prev_s.set(s);
    err = ((NomBook*)book)->change_order_size (o,delta_size,1);
    break;
  }
//--------------------------------------------------------------
  case 'u': {  //NOM_SINGLE_SIDE_REPLACE_SHORT
    struct itto_message_replace_short *message = (struct itto_message_replace_short *)m;
    uint64_t old_order_id = be64toh(message->original_reference_delta);
    uint64_t new_order_id = be64toh(message->new_reference_delta); 
    uint32_t price    = (uint32_t) be16toh(message->price) * 100 / EFH_PRICE_SCALE;
    uint32_t size     = (uint32_t) be16toh(message->size);

    fh_b_order* o = ((NomBook*)book)->find_order(old_order_id);
    if (o == NULL) return false;
    if (o->plevel == NULL) on_error("NOM_SINGLE_SIDE_REPLACE_SHORT: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_SINGLE_SIDE_REPLACE_SHORT: o->plevel->s == NULL");
    s = o->plevel->s;

    prev_s.set(s);
    err = ((NomBook*)book)->replace_order(o,new_order_id,price,size);
    break;
  }
//--------------------------------------------------------------
  case 'U': { //NOM_SINGLE_SIDE_REPLACE_LONG
    struct itto_message_replace_long *message = (struct itto_message_replace_long *)m;
    uint64_t old_order_id = be64toh(message->original_reference_delta);
    uint64_t new_order_id = be64toh(message->new_reference_delta); 
    uint32_t price    = be32toh(message->price) / EFH_PRICE_SCALE;
    uint32_t size     = be32toh(message->size);
    fh_b_order* o = ((NomBook*)book)->find_order(old_order_id);
    if (o == NULL)  return false;

    if (o->plevel == NULL) on_error("NOM_SINGLE_SIDE_REPLACE_LONG: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_SINGLE_SIDE_REPLACE_LONG: o->plevel->s == NULL");
    s = o->plevel->s;

    prev_s.set(s);
    err = ((NomBook*)book)->replace_order(o,new_order_id,price,size);
    break;
  }
//--------------------------------------------------------------
  case 'D': { //NOM_SINGLE_SIDE_DELETE 
    struct itto_message_delete *message = (struct itto_message_delete *)m;
    uint64_t order_id = be64toh(message->reference_delta);
    fh_b_order* o = ((NomBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    if (o->plevel == NULL) on_error("NOM_SINGLE_SIDE_DELETE: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_SINGLE_SIDE_DELETE: o->plevel->s == NULL");
    s = o->plevel->s;
    prev_s.set(s);
    err = ((NomBook*)book)->delete_order (o);
    break;
  }
//--------------------------------------------------------------
  case 'G': { //NOM_SINGLE_SIDE_UPDATE
    struct itto_message_update *message = (struct itto_message_update *)m;
    uint64_t order_id = be64toh(message->reference_delta);
    uint32_t price    = be32toh(message->price) / EFH_PRICE_SCALE;
    uint32_t size     = be32toh(message->size);

    fh_b_order* o = ((NomBook*)book)->find_order(order_id);
    if (o == NULL)  return false;
    if (o->plevel == NULL) on_error("NOM_SINGLE_SIDE_UPDATE: o->plevel == NULL");
    if (o->plevel->s == NULL) on_error("NOM_SINGLE_SIDE_UPDATE: o->plevel->s == NULL");
    s = o->plevel->s;

    prev_s.set(s);
    err = ((NomBook*)book)->modify_order (o,price,size); // modify New order for price and size
    break;
  }
//--------------------------------------------------------------
  case 'k':   {//NOM_QUOTE_REPLACE_SHORT
    struct itto_quote_replace_short *message = (struct itto_quote_replace_short *)m;
    
    uint64_t old_bid_order_id   = be64toh(message->original_bid_delta);
    uint64_t new_bid_order_id   = be64toh(message->new_bid_delta);
    uint32_t bid_price          = (uint32_t)be16toh(message->bid_price) * 100 / EFH_PRICE_SCALE;
    uint32_t bid_size           = (uint32_t)be16toh(message->bid_size);
    uint64_t old_ask_order_id   = be64toh(message->original_ask_delta);
    uint64_t new_ask_order_id   = be64toh(message->new_ask_delta);
    uint32_t ask_price          = (uint32_t)be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE;
    uint32_t ask_size           = (uint32_t)be16toh(message->ask_size);

    fh_b_order* bid_o = ((NomBook*)book)->find_order(old_bid_order_id);
    if (bid_o != NULL) {
      if (bid_o->plevel == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: bid_o->plevel == NULL");
      if (bid_o->plevel->s == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: bid_o->plevel->s == NULL");
      s = bid_o->plevel->s;
    }
    fh_b_order* ask_o = ((NomBook*)book)->find_order(old_ask_order_id);
    if (ask_o != NULL) {
      if (ask_o->plevel == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: ask_o->plevel == NULL");
      if (ask_o->plevel->s == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: ask_o->plevel->s == NULL");
      s = ask_o->plevel->s;
    }
    if (bid_o == NULL && ask_o == NULL) return false;
    if (s == NULL) on_error ("s = NULL");

    prev_s.set(s);
    if (bid_o != NULL) err = ((NomBook*)book)->replace_order( bid_o, new_bid_order_id, bid_price, bid_size);
    if (err != EKA_FH_RESULT__OK) break;
    if (ask_o != NULL) err = ((NomBook*)book)->replace_order( ask_o, new_ask_order_id, ask_price, ask_size);
    break;
  }
//--------------------------------------------------------------
  case 'K': { //NOM_QUOTE_REPLACE_LONG
    struct itto_quote_replace_long *message = (struct itto_quote_replace_long *)m;
    
    uint64_t old_bid_order_id   = be64toh(message->original_bid_delta);
    uint64_t new_bid_order_id   = be64toh(message->new_bid_delta);
    uint32_t bid_price          = be32toh(message->bid_price) / EFH_PRICE_SCALE;
    uint32_t bid_size           = be32toh(message->bid_size);
    uint64_t old_ask_order_id   = be64toh(message->original_ask_delta);
    uint64_t new_ask_order_id   = be64toh(message->new_ask_delta);
    uint32_t ask_price          = be32toh(message->ask_price) / EFH_PRICE_SCALE;
    uint32_t ask_size           = be32toh(message->ask_size);

    fh_b_order* bid_o = ((NomBook*)book)->find_order(old_bid_order_id);
    if (bid_o != NULL) {
      if (bid_o->plevel == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: bid_o->plevel == NULL");
      if (bid_o->plevel->s == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: bid_o->plevel->s == NULL");
      s = bid_o->plevel->s;
    }
    fh_b_order* ask_o = ((NomBook*)book)->find_order(old_ask_order_id);
    if (ask_o != NULL) {
      if (ask_o->plevel == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: ask_o->plevel == NULL");
      if (ask_o->plevel->s == NULL) on_error("NOM_QUOTE_REPLACE_SHORT: ask_o->plevel->s == NULL");
      s = ask_o->plevel->s;
    }
    if (bid_o == NULL && ask_o == NULL) return false;
    if (s == NULL) on_error ("s = NULL");

    prev_s.set(s);
    if (bid_o != NULL) err = ((NomBook*)book)->replace_order( bid_o, new_bid_order_id, bid_price, bid_size);
    if (err != EKA_FH_RESULT__OK) break;
    if (ask_o != NULL) err = ((NomBook*)book)->replace_order( ask_o, new_ask_order_id, ask_price, ask_size);
    break;
  }
//--------------------------------------------------------------
  case 'Y': { //NOM_QUOTE_DELETE 
    struct itto_quote_delete *message = (struct itto_quote_delete *)m;
    uint64_t bid_order_id = be64toh(message->bid_delta);
    uint64_t ask_order_id = be64toh(message->ask_delta);

    fh_b_order *bid_o = ((NomBook*)book)->find_order(bid_order_id);
    fh_b_order *ask_o = ((NomBook*)book)->find_order(ask_order_id);

    if (bid_o != NULL) {
      if (bid_o->plevel == NULL) on_error("NOM_QUOTE_DELETE: bid_o->plevel == NULL");
      if (bid_o->plevel->s == NULL) on_error("NOM_QUOTE_DELETE: bid_o->plevel->s == NULL");
      s = bid_o->plevel->s;
    }

    if (ask_o != NULL) {
      if (ask_o->plevel == NULL) on_error("NOM_QUOTE_DELETE: ask_o->plevel == NULL");
      if (ask_o->plevel->s == NULL) on_error("NOM_QUOTE_DELETE: ask_o->plevel->s == NULL");
      s = ask_o->plevel->s;
    }
    if (bid_o == NULL && ask_o == NULL)  return false;
    if (s == NULL) on_error ("s = NULL");

    prev_s.set(s);
    if (bid_o != NULL) err = ((NomBook*)book)->delete_order ( bid_o);
    if (err != EKA_FH_RESULT__OK) break;
    if (ask_o != NULL) err = ((NomBook*)book)->delete_order ( ask_o);
    break;
  }
//--------------------------------------------------------------
  case 'P': { //NOM_OPTIONS_TRADE
    struct itto_options_trade *message = (struct itto_options_trade *)m;
    uint32_t security_id = be32toh(message->option_id);
    s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL && !((NomBook*)book)->subscribe_all) return false;
    if (s == NULL && ((NomBook*)book)->subscribe_all) s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);

    const EfhTradeMsg msg = {
      { EfhMsgType::kTrade,
	{EkaSource::kNOM_ITTO,(EkaLSI)id}, // group

	0,  // underlyingId
	(uint64_t) security_id, 
	sequence,
	msg_timestamp,
	gapNum },
      be32toh(message->price) / EFH_PRICE_SCALE,
      be32toh(message->size),
      EfhTradeCond::kReg
    };
    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);

    return false;
  }
//--------------------------------------------------------------
  case 'Q': { //NOM_CROSS_TRADE
    return false;
  }
//--------------------------------------------------------------
  case 'B': { //NOM_BROKEN_EXEC
    return false;
  }
//--------------------------------------------------------------
  case 'I': { //NOM_NOII
    return false;
  }
  default: 
    on_error("UNEXPECTED Message type: enc=\'%c\'",enc);
  }

  if (s == NULL) on_error("Uninitialized Security ptr after message %c",enc);
  s->bid_ts = msg_timestamp;
  s->ask_ts = msg_timestamp;
  /* if(op == EkaFhMode::MCAST && msg_timestamp < s->timestamp) on_warning("WARNING: security %u: msg_timestamp (= %s) < s->timestamp (= %s)",s->security_id,ts_ns2str(msg_timestamp).c_str(),ts_ns2str(s->timestamp).c_str()); */

  if (err != EKA_FH_RESULT__OK) {
    eka_print_nom_msg(stderr,(uint8_t*)m,id,sequence);
    on_error ("Failed FH BOOK OPERATION");
  }

  if (fh->print_parsed_messages) eka_print_nom_msg(((NomBook*)book)->parser_log,(uint8_t*)m,id,sequence);

  if (prev_s.security_id == 0) on_error("Uninitialized prev_s (sequence = %ju, timestamp = %ju",sequence,msg_timestamp);
 /* ##################################################################### */

  if (prev_s.is_equal(s)) return false;
#ifdef EKA_TIME_CHECK
  auto finish = std::chrono::high_resolution_clock::now();
  uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
  if (duration_ns > 50000) EKA_WARN("WARNING: \'%c\' processing took %ju ns",enc, duration_ns);
#endif


  ((NomBook*)book)->generateOnQuote (pEfhRunCtx, s, sequence, msg_timestamp,gapNum);

  return false;
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

static inline uint64_t get_ts(uint8_t* m) {
  uint64_t ts_tmp = 0;
  memcpy((uint8_t*)&ts_tmp+2,m+3,6);
  return be64toh(ts_tmp);
}

static void eka_print_nom_msg(FILE* md_file, uint8_t* m, int gr, uint64_t sequence) {
  fprintf (md_file,"GR%d,%s,SN:%ju,%3s(%c),",gr,(ts_ns2str(get_ts(m))).c_str(),sequence,ITTO_NOM_MSG((char)m[0]),(char)m[0]);
  switch ((char)m[0]) {
  case 'R': //ITTO_TYPE_OPTION_DIRECTORY 
    break;
  case 'M': // END OF SNAPSHOT
    break;
  case 'a': { //NOM_ADD_ORDER_SHORT
    struct itto_add_order_short *message = (struct itto_add_order_short *)m;
    fprintf (md_file,"SID:%16u,OID:%16ju,%c,P:%8u,S:%8u",
	    be32toh (message->option_id),
	    be64toh (message->order_reference_delta),
	    (char)             (message->side),
	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'A' : { //NOM_ADD_ORDER_LONG
    struct itto_add_order_long *message = (struct itto_add_order_long *)m;
    fprintf (md_file,"SID:%16u,OID:%16ju,%c,P:%8u,S:%8u",
	    be32toh (message->option_id),
	    be64toh (message->order_reference_delta),
	    (char)             (message->side),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );
    break;
  }
  case 'S': //NOM_SYSTEM_EVENT
    break;
  case 'L': //NOM_BASE_REF -- do nothing        
    break;
  case 'H': { //NOM_TRADING_ACTION 
    struct itto_trading_action *message = (struct itto_trading_action *)m;
    fprintf (md_file,"SID:%16u,TS:%c",be32toh(message->option_id),message->trading_state);
    break;
  }
  case 'O': { //NOM_OPTION_OPEN 
    struct itto_option_open *message = (struct itto_option_open *)m;
    fprintf (md_file,"SID:%16u,OS:%c",be32toh(message->option_id),message->open_state);
    break;
  }
    //--------------------------------------------------------------
  case 'J': {  //NOM_ADD_QUOTE_LONG
    struct itto_add_quote_long *message = (struct itto_add_quote_long *)m;

    fprintf (md_file,"SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u",
	    be32toh (message->option_id),
	    be64toh (message->bid_reference_delta),
	    be32toh (message->bid_price) / EFH_PRICE_SCALE,
	    be32toh (message->bid_size),
	    be64toh (message->ask_reference_delta),
	    be32toh (message->ask_price) / EFH_PRICE_SCALE,
	    be32toh (message->ask_size)
	    );
    break;
  }
  case 'j': { //NOM_ADD_QUOTE_SHORT
    struct itto_add_quote_short *message = (struct itto_add_quote_short *)m;
    
    fprintf (md_file,"SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u",
	    be32toh (message->option_id),
	    be64toh (message->bid_reference_delta),
	    (uint32_t) be16toh (message->bid_price) / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->bid_size),
	    be64toh (message->ask_reference_delta),
	    (uint32_t) be16toh (message->ask_price) / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->ask_size)
	    );

    break;
  }

  case 'E':  { //NOM_SINGLE_SIDE_EXEC
    struct itto_executed *message = (struct itto_executed *)m;
    fprintf (md_file,"OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->executed_contracts));

    break;
  }
    //--------------------------------------------------------------
  case 'C': { //NOM_SINGLE_SIDE_EXEC_PRICE
    struct itto_executed_price *message = (struct itto_executed_price *)m;
    fprintf (md_file,"OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->size));

    break;
  }

    //--------------------------------------------------------------
  case 'X': { //NOM_ORDER_CANCEL
    struct itto_order_cancel *message = (struct itto_order_cancel *)m;
    fprintf (md_file,"OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->cancelled_orders));

    break;
  }
    //--------------------------------------------------------------
  case 'u': {  //NOM_SINGLE_SIDE_REPLACE_SHORT
    struct itto_message_replace_short *message = (struct itto_message_replace_short *)m;
    fprintf (md_file,"OOID:%16ju,NOID:%16ju,P:%8u,S:%8u",
	    be64toh (message->original_reference_delta),
	    be64toh (message->new_reference_delta),
	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'U': { //NOM_SINGLE_SIDE_REPLACE_LONG
    struct itto_message_replace_long *message = (struct itto_message_replace_long *)m;
    fprintf (md_file,"OOID:%16ju,NOID:%16ju,P:%8u,S:%8u",
	    be64toh (message->original_reference_delta),
	    be64toh (message->new_reference_delta),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'D': { //NOM_SINGLE_SIDE_DELETE 
    struct itto_message_delete *message = (struct itto_message_delete *)m;
    fprintf (md_file,"OID:%16ju",be64toh(message->reference_delta));

    break;
  }
    //--------------------------------------------------------------
  case 'G': { //NOM_SINGLE_SIDE_UPDATE
    struct itto_message_update *message = (struct itto_message_update *)m;
    fprintf (md_file,"OID:%16ju,P:%8u,S:%8u",
	    be64toh (message->reference_delta),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );

    break;
  }
    //--------------------------------------------------------------
  case 'k':   {//NOM_QUOTE_REPLACE_SHORT
    struct itto_quote_replace_short *message = (struct itto_quote_replace_short *)m;
    fprintf (md_file,"OBOID:%16ju,NBOID:%16ju,BP:%8u,BS:%8u,OAOID:%16ju,NAOID:%16ju,AP:%8u,AS:%8u",
	    be64toh (message->original_bid_delta),
	    be64toh(message->new_bid_delta),
	    (uint32_t) be16toh (message->bid_price) / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->bid_size),

	    be64toh(message->original_ask_delta),
	    be64toh(message->new_ask_delta),
	    (uint32_t) be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh(message->ask_size)
	    );

    break;
  }
    //--------------------------------------------------------------
  case 'K': { //NOM_QUOTE_REPLACE_LONG
    struct itto_quote_replace_long *message = (struct itto_quote_replace_long *)m;
    fprintf (md_file,"OBOID:%16ju,NBOID:%16ju,BP:%8u,BS:%8u,OAOID:%16ju,NAOID:%16ju,AP:%8u,AS:%8u",
	    be64toh (message->original_bid_delta),
	    be64toh (message->new_bid_delta),
	    be32toh (message->bid_price) * 100 / EFH_PRICE_SCALE,
	    be32toh (message->bid_size),

	    be64toh (message->original_ask_delta),
	    be64toh (message->new_ask_delta),
	    be32toh (message->ask_price) / EFH_PRICE_SCALE,
	    be32toh (message->ask_size)
	    );
    break;
  }
    //--------------------------------------------------------------

  case 'Y': { //NOM_QUOTE_DELETE 
    struct itto_quote_delete *message = (struct itto_quote_delete *)m;
    fprintf (md_file,"BOID:%16ju,AOID:%16ju",be64toh(message->bid_delta),be64toh(message->ask_delta));

    break;
  }
    //--------------------------------------------------------------

  case 'P': { //NOM_OPTIONS_TRADE
    struct itto_options_trade *message = (struct itto_options_trade *)m;
    fprintf (md_file,"SID:%16u",be32toh(message->option_id));
    break;
  }
    //--------------------------------------------------------------
  case 'Q': { //NOM_CROSS_TRADE
    break;
  }
    //--------------------------------------------------------------
  case 'B': { //NOM_BROKEN_EXEC
    break;
  }
    //--------------------------------------------------------------
  case 'I': { //NOM_NOII
    break;
  }
  default: 
    on_error("UNEXPECTED Message type: enc=\'%c\'",(char)m[0]);
  }
  fprintf (md_file,"\n");
  fflush(md_file);
  return;

}
