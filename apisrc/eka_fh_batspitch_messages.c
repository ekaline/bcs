#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "eka_fh.h"
#include "eka_fh_batspitch_messages.h"

static void eka_print_batspitch_msg(FILE* md_file, uint8_t* m, int gr, uint64_t sequence,uint64_t ts);
std::string ts_ns2str(uint64_t ts);

static inline EfhTradeStatus tradeAction(EfhTradeStatus prevTradeAction, char msgTradeStatus) {
  switch (msgTradeStatus) {
  case 'A' : // Accepting Orders for Queuing
    break; // To be confirmed!!!
  case 'Q' : // Quote-Only
    return EfhTradeStatus::kPreopen;
  case 'R' : // Opening-Rotation
    return EfhTradeStatus::kOpeningRotation;
  case 'H' : // Halted
  case 'S' : // Exchange Specific Suspension
    return EfhTradeStatus::kHalted;
  case 'T' : // Trading
    return EfhTradeStatus::kNormal;
  default:
    on_error("Unexpected trade status \'%c\'",msgTradeStatus);
  }
  return prevTradeAction;
}

/* ------------------------------------------------ */
inline fh_b_order::type_t addFlags2orderType(uint8_t flags) {
  switch (flags) {
    /* case 0x01 : // bit #0 */
    /*   return fh_b_order::BD; */
  case 0x08 : // bit #3
  case 0x09 : // bit #0 & #3
    return fh_b_order::BD_AON;
  default:
    return fh_b_order::BD;
  }
}
/* ------------------------------------------------ */
inline fh_b_order::type_t addFlagsCustomerIndicator2orderType(uint8_t flags, char customerIndicator) {
  if (customerIndicator == 'C') {
    switch (flags) {
      /* case 0x01 : // bit #0 */
      /*   return fh_b_order::BD; */
    case 0x08 : // bit #3
    case 0x09 : // bit #0 & #3
      return fh_b_order::CUSTOMER_AON;
    default:
      return fh_b_order::CUSTOMER;
    }
  } else {
    switch (flags) {
      /* case 0x01 : // bit #0 */
      /*   return fh_b_order::BD; */
    case 0x08 : // bit #3
    case 0x09 : // bit #0 & #3
      return fh_b_order::BD_AON;
    default:
      return fh_b_order::BD;
    }
  }
}

/* ------------------------------------------------ */

inline uint32_t normalize_bats_symbol_char(char c) {
  if (c >= '0' && c <= '9') return (uint32_t)(0  + (c - '0')); // 0..9
  if (c >= 'A' && c <= 'Z') return (uint32_t)(10 + (c - 'A')); // 10..35
  if (c >= 'a' && c <= 'z') return (uint32_t)(36 + (c - 'a')); // 36..61
  on_error ("Unexpected symbol |%c|",c);
}
/* ------------------------------------------------ */

inline uint32_t bats_symbol2optionid (const char* s, uint symbol_size) {
  uint64_t compacted_id = 0;
  if (s[0] != '0') on_error("%c%c%c%c%c%c doesnt have \"0\" prefix",s[0],s[1],s[2],s[3],s[4],s[5]);
  for (uint i = 1; i < symbol_size; i++) {
    //    printf ("s[%i] = %c, normalize_bats_symbol_char = %u = 0x%x\n",i,s[i], normalize_bats_symbol_char(s[i]), normalize_bats_symbol_char(s[i]));
    compacted_id |= normalize_bats_symbol_char(s[i]) << ((symbol_size - i - 1) * symbol_size);
  }
  if ((compacted_id & 0xffffffff00000000) != 0) on_error("%c%c%c%c%c%c encoding produced 0x%jx exceeding 32bit",s[0],s[1],s[2],s[3],s[4],s[5],compacted_id);
  return (uint32_t) (compacted_id & 0x00000000ffffffff);
}
/* ------------------------------------------------ */

bool FhBatsGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  EKA_BATS_PITCH_MSG enc =  (EKA_BATS_PITCH_MSG)m[1];
  //  EKA_LOG("%s:%u: 0x%02x",EKA_EXCH_DECODE(exch),id,enc);
  fh_b_security_state prev_s = {};
  prev_s.reset();

  uint64_t msg_timestamp = 0;
  fh_b_security* s = NULL;

  if (op == EkaFhMode::SNAPSHOT && enc == EKA_BATS_PITCH_MSG::SYMBOL_MAPPING) return false;
  switch (enc) {    
  case EKA_BATS_PITCH_MSG::ADD_ORDER_LONG:
  case EKA_BATS_PITCH_MSG::ADD_ORDER_SHORT:
  case EKA_BATS_PITCH_MSG::ADD_ORDER_EXPANDED:
  case EKA_BATS_PITCH_MSG::ORDER_EXECUTED:
  case EKA_BATS_PITCH_MSG::ORDER_EXECUTED_AT_PRICE_SIZE:
  case EKA_BATS_PITCH_MSG::REDUCED_SIZE_LONG:
  case EKA_BATS_PITCH_MSG::REDUCED_SIZE_SHORT:
  case EKA_BATS_PITCH_MSG::ORDER_MODIFY_LONG:
  case EKA_BATS_PITCH_MSG::ORDER_MODIFY_SHORT:
  case EKA_BATS_PITCH_MSG::ORDER_DELETE:
  case EKA_BATS_PITCH_MSG::TRADE_LONG:
  case EKA_BATS_PITCH_MSG::TRADE_SHORT:
  case EKA_BATS_PITCH_MSG::TRADING_STATUS:
    msg_timestamp = seconds + ((batspitch_generic_header *)m)->time; 
    break;
  default: {}
  }

  if (op != EkaFhMode::DEFINITIONS && fh->print_parsed_messages) eka_print_batspitch_msg(((BatsBook*)book)->parser_log,(uint8_t*)m,id,sequence,msg_timestamp);

  switch (enc) {    
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::DEFINITIONS_FINISHED : {
    EKA_LOG("%s:%u: DEFINITIONS_FINISHED",EKA_EXCH_DECODE(exch),id);
    if (op == EkaFhMode::DEFINITIONS) return true;
    on_error ("%s:%u DEFINITIONS_FINISHED accepted at non Definitions (%u) phase",EKA_EXCH_DECODE(exch),id,(uint)op);
  }
     //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::SPIN_FINISHED : {
    seq_after_snapshot = ((batspitch_spin_finished*)m)->sequence + 1;
    EKA_LOG("%s:%u: SPIN_FINISHED: seq_after_snapshot = %u",EKA_EXCH_DECODE(exch),id,seq_after_snapshot);
    return true;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::SYMBOL_MAPPING :  {
    batspitch_symbol_mapping* message = (batspitch_symbol_mapping*)m;

    char* osi = message->osi_symbol;

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = exch;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) bats_symbol2optionid(message->symbol,6);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    msg.securityType          = EfhSecurityType::kOpt;
    uint y = (osi[6] -'0') * 10 + (osi[7] -'0');
    uint m = (osi[8] -'0') * 10 + (osi[9] -'0');
    uint d = (osi[10]-'0') * 10 + (osi[11]-'0');

    msg.expiryDate            = (2000 + y) * 10000 + m * 100 + d;
    char strike_price_str[9] = {};
    memcpy(strike_price_str,&osi[13],8);
    strike_price_str[8] = '\0';

    //    msg.strikePrice           = strtoull(strike_price_str,NULL,10) / EFH_STRIKE_PRICE_SCALE;
    msg.strikePrice           = strtoull(strike_price_str,NULL,10) * EFH_PITCH_STRIKE_PRICE_SCALE; // per Ken's request
    msg.exchange              = EKA_GRP_SRC2EXCH(exch);

    msg.optionType            = osi[12] == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    memcpy (&msg.underlying,osi,6);
    memcpy (&msg.classSymbol,message->underlying,std::min(sizeof(msg.classSymbol),sizeof(message->underlying)));

    memcpy(&msg.opaqueAttrA,message->symbol,6);

    char osi2print[22] = {};
    memcpy(osi2print,osi,21);
    //    EKA_LOG("OSI: %s, Expiration = %u, strike: %ju",osi2print,msg.expiryDate,msg.strikePrice);
    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRANSACTION_BEGIN:  { 
    market_open = true;
    return false;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRANSACTION_END:  { 
    market_open = false;
    return false;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::END_OF_SESSION:  { 
    market_open = false;
    return true;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TIME:  { 
    seconds = ((batspitch_generic_header *)m)->time * SEC_TO_NANO;
    return false;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_SHORT:  { 
    batspitch_add_order_short *message = (batspitch_add_order_short *)m;
    uint32_t security_id =  bats_symbol2optionid(message->symbol,6);
    s = ((BatsBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((BatsBook*)book)->subscribe_all) return false;
      s = ((BatsBook*)book)->subscribe_security(security_id,0,0,0,0);
    }
    prev_s.set(s);

    uint64_t order_id = message->order_id;
    uint32_t size =  (uint32_t) message->size;
    uint32_t price = (uint32_t) message->price * 100 / EFH_PRICE_SCALE; // Short Price representation
    char side = (char)message->side;

    ((BatsBook*)book)->add_order2book(s,order_id,addFlags2orderType(message->flags),price,size,side);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_LONG:  { 
    batspitch_add_order_long *message = (batspitch_add_order_long *)m;
    uint32_t security_id =  bats_symbol2optionid(message->symbol,6);
    s = ((BatsBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((BatsBook*)book)->subscribe_all) return false;
      s = ((BatsBook*)book)->subscribe_security(security_id,0,0,0,0);
    }
    prev_s.set(s);

    uint64_t order_id = message->order_id;
    uint32_t size =  (uint32_t) message->size;
    uint32_t price = (uint32_t) ((message->price / EFH_PRICE_SCALE) & 0x00000000FFFFFFFF); // Long Price representation
    if (((message->price / EFH_PRICE_SCALE) & 0xFFFFFFFF00000000) != 0) on_error("Long price(%ju) exceeds 32bit",message->price);

    char side = (char)message->side;
    ((BatsBook*)book)->add_order2book(s,order_id,addFlags2orderType(message->flags),price,size,side);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_EXPANDED:  { 
    batspitch_add_order_expanded *message = (batspitch_add_order_expanded *)m;
    if (message->exp_symbol[6] != ' ' || message->exp_symbol[7] != ' ')
      on_error("ADD_ORDER_EXPANDED message with \'%c%c%c%c%c%c%c%c\' symbol (longer than 6 chars) not supported",
	       message->exp_symbol[0],message->exp_symbol[1],message->exp_symbol[2],message->exp_symbol[3],
	       message->exp_symbol[4],message->exp_symbol[5],message->exp_symbol[6],message->exp_symbol[7]);

    uint32_t security_id =  bats_symbol2optionid(message->exp_symbol,6);
    s = ((BatsBook*)book)->find_security(security_id);
    if (s == NULL) {
      if (!((BatsBook*)book)->subscribe_all) return false;
      s = ((BatsBook*)book)->subscribe_security(security_id,0,0,0,0);
    }
    prev_s.set(s);

    uint64_t order_id = message->order_id;
    uint32_t size =  (uint32_t) message->size;
    uint32_t price = (uint32_t) ((message->price / EFH_PRICE_SCALE) & 0x00000000FFFFFFFF); // Long Price representation
    if (((message->price / EFH_PRICE_SCALE) & 0xFFFFFFFF00000000) != 0) on_error("Long price(%ju) exceeds 32bit",message->price);

    char side = (char)message->side;
    ((BatsBook*)book)->add_order2book(s,order_id,addFlagsCustomerIndicator2orderType(message->flags,message->customer_indicator),price,size,side);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_EXECUTED: { 
    batspitch_order_executed *message = (batspitch_order_executed *)m;
    uint64_t order_id   = message->order_id;
    uint32_t delta_size = message->executed_size;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert(o->plevel != NULL);
    assert(o->plevel->s != NULL);
    s = o->plevel->s;
    prev_s.set(s);
    ((BatsBook*)book)->change_order_size (o,delta_size,1);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_EXECUTED_AT_PRICE_SIZE:  { 
    batspitch_order_executed_at_price_size *message = (batspitch_order_executed_at_price_size *)m;
    uint64_t order_id   = message->order_id;
    uint32_t delta_size = message->executed_size;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert(o->plevel != NULL);
    assert(o->plevel->s != NULL);
    s = o->plevel->s;
    prev_s.set(s);
    ((BatsBook*)book)->change_order_size (o,delta_size,1);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::REDUCED_SIZE_LONG:  { 
    batspitch_reduced_size_long *message = (batspitch_reduced_size_long *)m;
    uint64_t order_id   = message->order_id;
    uint32_t delta_size = message->canceled_size;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert(o->plevel != NULL);
    assert(o->plevel->s != NULL);
    s = o->plevel->s;
    prev_s.set(s);
    ((BatsBook*)book)->change_order_size (o,delta_size,1);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::REDUCED_SIZE_SHORT:  { 
    batspitch_reduced_size_short *message = (batspitch_reduced_size_short *)m;
    uint64_t order_id   = message->order_id;
    uint32_t delta_size = message->canceled_size;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert(o->plevel != NULL);
    assert(o->plevel->s != NULL);
    s = o->plevel->s;
    prev_s.set(s);
    ((BatsBook*)book)->change_order_size (o,delta_size,1);
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_MODIFY_SHORT:  { 
    batspitch_order_modify_short  *message = (batspitch_order_modify_short *)m;

    uint64_t order_id = message->order_id;
    uint32_t size =  (uint32_t) message->size;
    uint32_t price = (uint32_t) message->price * 100 / EFH_PRICE_SCALE; // Short Price representation

    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert (o->plevel != NULL);
    assert (o->plevel->s != NULL);
    s = o->plevel->s;

    prev_s.set(s);
    ((BatsBook*)book)->modify_order (o,price,size); // modify existing order for price and size */
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_MODIFY_LONG:  { 

    batspitch_order_modify_long  *message = (batspitch_order_modify_long *)m;

    uint64_t order_id = message->order_id;
    uint32_t size =  (uint32_t) message->size;
    uint32_t price = (uint32_t) ((message->price / EFH_PRICE_SCALE) & 0x00000000FFFFFFFF); // Long Price representation
    if (((message->price / EFH_PRICE_SCALE) & 0xFFFFFFFF00000000) != 0) on_error("Long price(%ju) exceeds 32bit",message->price);

    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert (o->plevel != NULL);
    assert (o->plevel->s != NULL);
    s = o->plevel->s;

    prev_s.set(s);
    ((BatsBook*)book)->modify_order (o,price,size); // modify existing order for price and size */
    break;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_DELETE:  { 
    batspitch_order_delete  *message = (batspitch_order_delete *)m;

    uint64_t order_id = message->order_id;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert (o->plevel != NULL);
    assert (o->plevel->s != NULL);
    s = o->plevel->s;

    prev_s.set(s);
    ((BatsBook*)book)->delete_order (o);
    break;
  }

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRADE_SHORT:  { 
    batspitch_trade_short *message = (batspitch_trade_short *)m;

    uint64_t order_id = message->order_id;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert (o->plevel != NULL);
    assert (o->plevel->s != NULL);
    uint32_t security_id =  bats_symbol2optionid(message->symbol,6);

    s = ((BatsBook*)book)->find_security(security_id);
    assert (s == o->plevel->s); // sanity check
    assert (security_id == s->security_id); // sanity check

    if (s == NULL && !((BatsBook*)book)->subscribe_all) return false;
    if (s == NULL && ((BatsBook*)book)->subscribe_all) s = ((BatsBook*)book)->subscribe_security(security_id,0,0,0,0);

    uint32_t price = (uint32_t) message->price * 100 / EFH_PRICE_SCALE; // Short Price representation
    uint32_t size =  (uint32_t) message->size;

    const EfhTradeMsg msg = {
      { EfhMsgType::kTrade,
	{exch,(EkaLSI)id}, // group
	0,  // underlyingId
	(uint64_t) security_id, 
	sequence,
	msg_timestamp,
	gapNum },
      price,
      size,
      EKA_BATS_TRADE_COND(message->trade_condition)
    };
    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRADE_LONG:  { 
    batspitch_trade_long *message = (batspitch_trade_long *)m;

    uint64_t order_id = message->order_id;
    fh_b_order* o = ((BatsBook*)book)->find_order(order_id);
    if (o == NULL) return false;
    assert (o->plevel != NULL);
    assert (o->plevel->s != NULL);
    uint32_t security_id =  bats_symbol2optionid(message->symbol,6);

    s = ((BatsBook*)book)->find_security(security_id);
    assert (s == o->plevel->s); // sanity check
    assert (security_id == s->security_id); // sanity check

    if (s == NULL && !((BatsBook*)book)->subscribe_all) return false;
    if (s == NULL && ((BatsBook*)book)->subscribe_all) s = ((BatsBook*)book)->subscribe_security(security_id,0,0,0,0);

    uint32_t price = (uint32_t) ((message->price / EFH_PRICE_SCALE) & 0x00000000FFFFFFFF); // Long Price representation
    if (((message->price / EFH_PRICE_SCALE) & 0xFFFFFFFF00000000) != 0) on_error("Long price(%ju) exceeds 32bit",message->price);

    uint32_t size =  (uint32_t) message->size;

    const EfhTradeMsg msg = {
      { EfhMsgType::kTrade,
	{exch,(EkaLSI)id}, // group
	0,  // underlyingId
	(uint64_t) security_id, 
	sequence,
	msg_timestamp,
	gapNum },
      price,
      size,
      EKA_BATS_TRADE_COND(message->trade_condition)
    };
    pEfhRunCtx->onEfhTradeMsgCb(&msg, s->efhUserData, pEfhRunCtx->efhRunUserData);
    return false;
  }
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRADING_STATUS:  { 
    batspitch_trading_status *message = (batspitch_trading_status *)m;
    uint32_t security_id =  bats_symbol2optionid(message->symbol,6);
    s = ((BatsBook*)book)->find_security(security_id);
    if (s == NULL) return false;
    prev_s.set(s);

    s->trading_action = tradeAction(s->trading_action,message->trading_status);
    break;
  }
    //--------------------------------------------------------------

  default:
    //    EKA_LOG("Ignored Message type: enc=\'0x%02x\'",enc);
    return false;
  }
  if (s == NULL) on_error("Uninitialized Security ptr after message 0x%x",(uint8_t)enc);
  //  s->option_open = market_open;
  s->option_open = true;
  if (prev_s.is_equal(s)) return false;

  ((BatsBook*)book)->generateOnQuote (pEfhRunCtx, s, sequence, msg_timestamp, gapNum);

  return false;
}


static void eka_print_batspitch_msg(FILE* md_file, uint8_t* m, int gr, uint64_t sequence,uint64_t ts) {
  EKA_BATS_PITCH_MSG enc = (EKA_BATS_PITCH_MSG)m[1];
  fprintf(md_file,"%s,%ju,%s(0x%x),",ts_ns2str(ts).c_str(),sequence,EKA_BATS_PITCH_MSG_DECODE(enc),(uint8_t)enc);

  switch (enc) {
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_LONG:
    fprintf(md_file,"%ju,%c,%u,%s (%u),%ju,0x%x\n",
	    ((batspitch_add_order_long*)m)->order_id,
	    ((batspitch_add_order_long*)m)->side,
	    ((batspitch_add_order_long*)m)->size,
	    EKA_PRINT_BATS_SYMBOL(((batspitch_add_order_long*)m)->symbol),
	    bats_symbol2optionid(((batspitch_add_order_long*)m)->symbol,6),
	    ((batspitch_add_order_long*)m)->price,
	    ((batspitch_add_order_long*)m)->flags
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ADD_ORDER_SHORT:
    fprintf(md_file,"%ju,%c,%u,%s (%u),%u,0x%x\n",
	    ((batspitch_add_order_short*)m)->order_id,
	    ((batspitch_add_order_short*)m)->side,
	    ((batspitch_add_order_short*)m)->size,
	    EKA_PRINT_BATS_SYMBOL(((batspitch_add_order_short*)m)->symbol),
	    bats_symbol2optionid(((batspitch_add_order_short*)m)->symbol,6),
	    ((batspitch_add_order_short*)m)->price,
	    ((batspitch_add_order_short*)m)->flags
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_EXECUTED:
    fprintf(md_file,"%ju,%u,%ju,%c\n",
	    ((batspitch_order_executed*)m)->order_id,
	    ((batspitch_order_executed*)m)->executed_size,
	    ((batspitch_order_executed*)m)->execution_id,
	    ((batspitch_order_executed*)m)->trade_condition
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_EXECUTED_AT_PRICE_SIZE:
    fprintf(md_file,"%ju,%u,%u,%ju,%ju,%c\n",
	    ((batspitch_order_executed_at_price_size*)m)->order_id,
	    ((batspitch_order_executed_at_price_size*)m)->executed_size,
	    ((batspitch_order_executed_at_price_size*)m)->remaining_size,
	    ((batspitch_order_executed_at_price_size*)m)->execution_id,
	    ((batspitch_order_executed_at_price_size*)m)->price,
	    ((batspitch_order_executed_at_price_size*)m)->trade_condition
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::REDUCED_SIZE_LONG:
    fprintf(md_file,"%ju,%u\n",
	    ((batspitch_reduced_size_long*)m)->order_id,
	    ((batspitch_reduced_size_long*)m)->canceled_size
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::REDUCED_SIZE_SHORT:
    fprintf(md_file,"%ju,%u\n",
	    ((batspitch_reduced_size_short*)m)->order_id,
	    ((batspitch_reduced_size_short*)m)->canceled_size
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_MODIFY_LONG:
    fprintf(md_file,"%ju,%u,%ju,0x%x\n",
	    ((batspitch_order_modify_long*)m)->order_id,
	    ((batspitch_order_modify_long*)m)->size,
	    ((batspitch_order_modify_long*)m)->price,
	    ((batspitch_order_modify_long*)m)->flags
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_MODIFY_SHORT:
    fprintf(md_file,"%ju,%u,%u,0x%x\n",
	    ((batspitch_order_modify_short*)m)->order_id,
	    ((batspitch_order_modify_short*)m)->size,
	    ((batspitch_order_modify_short*)m)->price,
	    ((batspitch_order_modify_short*)m)->flags
	    );
    break;
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::ORDER_DELETE:
    fprintf(md_file,"%ju\n",
	    ((batspitch_order_delete*)m)->order_id
	    );
    break;
    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRADE_LONG:
    fprintf(md_file,"%ju,%c,%u,%s (%u),%ju,%ju,%c\n",
	    ((batspitch_trade_long*)m)->order_id,
	    ((batspitch_trade_long*)m)->side,
	    ((batspitch_trade_long*)m)->size,
	    EKA_PRINT_BATS_SYMBOL(((batspitch_trade_long*)m)->symbol),
	    bats_symbol2optionid(((batspitch_trade_long*)m)->symbol,6),
	    ((batspitch_trade_long*)m)->price,
	    ((batspitch_trade_long*)m)->execution_id,
	    ((batspitch_trade_long*)m)->trade_condition
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRADE_SHORT:
    fprintf(md_file,"%ju,%c,%u,%s (%u),%u,%ju,%c\n",
	    ((batspitch_trade_short*)m)->order_id,
	    ((batspitch_trade_short*)m)->side,
	    ((batspitch_trade_short*)m)->size,
	    EKA_PRINT_BATS_SYMBOL(((batspitch_trade_short*)m)->symbol),
	    bats_symbol2optionid(((batspitch_trade_short*)m)->symbol,6),
	    ((batspitch_trade_short*)m)->price,
	    ((batspitch_trade_short*)m)->execution_id,
	    ((batspitch_trade_short*)m)->trade_condition
	    );
    break;

    //--------------------------------------------------------------
  case EKA_BATS_PITCH_MSG::TRADING_STATUS:
    fprintf(md_file,"%s (%u),%c,%c\n",
	    EKA_PRINT_BATS_SYMBOL(((batspitch_trading_status*)m)->symbol),
	    bats_symbol2optionid(((batspitch_trading_status*)m)->symbol,6),
	    ((batspitch_trading_status*)m)->trading_status,
	    ((batspitch_trading_status*)m)->reg_sho_action
	    );
    break;

    //--------------------------------------------------------------

  default:
    fprintf(md_file,"\n");
    break;
  }
}
