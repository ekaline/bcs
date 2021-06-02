#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <algorithm>

#include "EkaFhNomGr.h"
#include "EkaFhNomParser.h"
#include "EkaFhFullBook.h"

static void eka_print_nom_msg(FILE* md_file, const uint8_t* m, int gr, uint64_t sequence);
static inline uint64_t get_ts(const uint8_t* m);
std::string ts_ns2str(uint64_t ts);

/* ####################################################### */
inline SideT sideDecode(char _side) {
  switch (_side) {
  case 'B' :
    return SideT::BID;
  case 'S' :
    return SideT::ASK;
  default:
    on_error("Unexpected Side \'%c\'",_side);
  }
}


/* ####################################################### */

bool EkaFhNomGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op) {

#ifdef EKA_TIME_CHECK
  auto start = std::chrono::high_resolution_clock::now();  
#endif

  char enc =  (char)m[0];
  uint64_t msg_timestamp = get_ts(m);

  if (op == EkaFhMode::DEFINITIONS && enc == 'M') return true;
  if ((op == EkaFhMode::DEFINITIONS && enc != 'R') || (op == EkaFhMode::SNAPSHOT    && enc == 'R')) return false;

  FhSecurity* s = NULL;

  switch (enc) {    
  case 'R':  { //ITTO_TYPE_OPTION_DIRECTORY 
    struct itto_definition *message = (struct itto_definition *)m;

    EfhOptionDefinitionMsg msg{};
    msg.header.msgType        = EfhMsgType::kOptionDefinition;
    msg.header.group.source   = EkaSource::kNOM_ITTO;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) be32toh(message->option_id);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = 0;
    msg.securityType          = EfhSecurityType::kOption;
    msg.expiryDate            = (2000 + message->expiration_year) * 10000 + message->expiration_month * 100 + message->expiration_day;
    msg.contractSize          = 0;
    msg.strikePrice           = be32toh(message->strike_price) / EFH_NOM_STRIKE_PRICE_SCALE;
    msg.exchange              = EfhExchange::kNOM;
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    memcpy (&msg.underlying,message->underlying_symbol,std::min(sizeof(msg.underlying),sizeof(message->underlying_symbol)));
    memcpy (&msg.classSymbol,message->security_symbol,std::min(sizeof(msg.classSymbol),sizeof(message->security_symbol)));

    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
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
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) {
#ifdef FH_SUBSCRIBE_ALL
      s = book->subscribeSecurity(security_id,(EfhSecurityType)1,(EfhSecUserData)0,0,0);
#else
      return false;
#endif
    }
    OrderIdT order_id = be64toh(message->order_reference_delta);
    SizeT    size     = be16toh (message->size);
    PriceT   price    = be16toh (message->price) * 100 / EFH_PRICE_SCALE; // Short Price representation
    SideT    side     = sideDecode(message->side);

    book->setSecurityPrevState(s);
    book->addOrder(s,order_id,FhOrderType::CUSTOMER,price,size,side);
    break;
  }
    //--------------------------------------------------------------
  case 'A' : { //NOM_ADD_ORDER_LONG
    struct itto_add_order_long *message = (struct itto_add_order_long *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) {
#ifdef FH_SUBSCRIBE_ALL
      s = book->subscribeSecurity(security_id,(EfhSecurityType)1,(EfhSecUserData)0,0,0);
#else
      return false;
#endif
    }
    OrderIdT order_id = be64toh   (message->order_reference_delta);
    SizeT    size     = be32toh   (message->size);
    PriceT   price    = be32toh   (message->price) / EFH_PRICE_SCALE;
    SideT    side     = sideDecode(message->side);

    book->setSecurityPrevState(s);
    book->addOrder(s,order_id,FhOrderType::CUSTOMER,price,size,side);
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
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    book->setSecurityPrevState(s);
    s->trading_action = ITTO_NOM_TRADING_ACTION(message->trading_state);
    break;
  }
  case 'O': { //NOM_OPTION_OPEN 
    struct itto_option_open *message = (struct itto_option_open *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    book->setSecurityPrevState(s);
    if (s != NULL) s->option_open = (message->open_state == 'Y');
    if (s != NULL && message->open_state == 'N') s->trading_action = EfhTradeStatus::kClosed;
    break;
  }
    //--------------------------------------------------------------
  case 'J': {  //NOM_ADD_QUOTE_LONG
    struct itto_add_quote_long *message = (struct itto_add_quote_long *)m;

    SecurityIdT security_id  = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) {
#ifdef FH_SUBSCRIBE_ALL
      s = book->subscribeSecurity(security_id,(EfhSecurityType)1,(EfhSecUserData)0,0,0);
#else
      return false;
#endif
    }

    OrderIdT bid_order_id = be64toh(message->bid_reference_delta);
    PriceT   bid_price    = be32toh(message->bid_price) / EFH_PRICE_SCALE;
    SizeT    bid_size     = be32toh(message->bid_size);
    OrderIdT ask_order_id = be64toh(message->ask_reference_delta);
    PriceT   ask_price    = be32toh(message->ask_price) / EFH_PRICE_SCALE;
    SizeT    ask_size     = be32toh(message->ask_size);

    book->setSecurityPrevState(s);
    /* FhOrder* bid_o =  */
    book->addOrder(s,bid_order_id,FhOrderType::BD,bid_price,bid_size,SideT::BID);
    /* bid_o->plevel->print("NOM_ADD_QUOTE_LONG BID"); */

    /* FhOrder* ask_o =  */
    book->addOrder(s,ask_order_id,FhOrderType::BD,ask_price,ask_size,SideT::ASK);
    /* ask_o->plevel->print("NOM_ADD_QUOTE_LONG ASK"); */

    break;
  }
    //--------------------------------------------------------------
  case 'j': { //NOM_ADD_QUOTE_SHORT
    struct itto_add_quote_short *message = (struct itto_add_quote_short *)m;

    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) {
#ifdef FH_SUBSCRIBE_ALL
      s = book->subscribeSecurity(security_id,(EfhSecurityType)1,(EfhSecUserData)0,0,0);
#else
      return false;
#endif
    }
    OrderIdT bid_order_id =  be64toh(message->bid_reference_delta);
    PriceT   bid_price    =  be16toh(message->bid_price) * 100 / EFH_PRICE_SCALE;
    SizeT    bid_size     =  be16toh(message->bid_size);
    OrderIdT ask_order_id =  be64toh(message->ask_reference_delta);
    PriceT   ask_price    =  be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE;
    SizeT    ask_size     =  be16toh(message->ask_size);

    book->setSecurityPrevState(s);

    /* FhOrder* bid_o =  */
    book->addOrder(s,bid_order_id,FhOrderType::BD,bid_price,bid_size,SideT::BID);
    /* bid_o->plevel->print("NOM_ADD_QUOTE_SHORT BID"); */

    /* FhOrder* ask_o =  */
    book->addOrder(s,ask_order_id,FhOrderType::BD,ask_price,ask_size,SideT::ASK);    
    /* ask_o->plevel->print("NOM_ADD_QUOTE_SHORT ASK"); */

    break;
  }
    //--------------------------------------------------------------
  case 'E':  { //NOM_SINGLE_SIDE_EXEC
    struct itto_executed *message = (struct itto_executed *)m;
    OrderIdT order_id   = be64toh(message->order_reference_delta);
    SizeT    delta_size = be32toh(message->executed_contracts);

    FhOrder* o = book->findOrder(order_id);
    if (o == NULL) return false;

    FhPlevel* p = o->plevel;
    s = (FhSecurity*)p->s;

    book->setSecurityPrevState(s);

    if (o->size < delta_size) {
      on_error("o->size %u < delta_size %u",o->size,delta_size);
    } else if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o,delta_size);
      p->deductSize (o->type,delta_size);
    }
    break;
  }
    //--------------------------------------------------------------
  case 'C': { //NOM_SINGLE_SIDE_EXEC_PRICE
    struct itto_executed_price *message = (struct itto_executed_price *)m;
    OrderIdT order_id   = be64toh(message->order_reference_delta);
    SizeT  delta_size   = be32toh(message->size);

    FhOrder* o = book->findOrder(order_id);
    if (o == NULL) return false;

    FhPlevel* p = o->plevel;
    s = (FhSecurity*)p->s;

    book->setSecurityPrevState(s);

    if (o->size < delta_size) {
      on_error("o->size %u < delta_size %u",o->size,delta_size);
    } else if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o,delta_size);
      p->deductSize (o->type,delta_size);
    }
    break;
  }

  //--------------------------------------------------------------
  case 'X': { //NOM_ORDER_CANCEL
    struct itto_order_cancel *message = (struct itto_order_cancel *)m;
    OrderIdT order_id = be64toh(message->order_reference_delta);
    SizeT delta_size  = be32toh(message->cancelled_orders);
 
    FhOrder* o = book->findOrder(order_id);
    if (o == NULL)  return false;


    FhPlevel* p = o->plevel;
    s = (FhSecurity*)p->s;

    book->setSecurityPrevState(s);

    if (o->size < delta_size) {
      on_error("o->size %u < delta_size %u",o->size,delta_size);
    } else if (o->size == delta_size) {
      book->deleteOrder(o);
    } else {
      book->reduceOrderSize(o,delta_size);
      p->deductSize (o->type,delta_size);
    }

    break;
  }
//--------------------------------------------------------------
  case 'u': {  //NOM_SINGLE_SIDE_REPLACE_SHORT
    struct itto_message_replace_short *message = (struct itto_message_replace_short *)m;
    OrderIdT old_order_id = be64toh(message->original_reference_delta);
    OrderIdT new_order_id = be64toh(message->new_reference_delta); 
    PriceT   price        = be16toh(message->price) * 100 / EFH_PRICE_SCALE;
    SizeT    size         = be16toh(message->size);

    FhOrder* o            = book->findOrder(old_order_id);
    if (o == NULL) return false;

    s = (FhSecurity*)o->plevel->s;
    book->setSecurityPrevState(s);

    FhOrderType t    = o->type;
    SideT       side = o->plevel->side;

    book->deleteOrder(o);
    book->addOrder(s,new_order_id,t,price,size,side);

    break;
  }
//--------------------------------------------------------------
  case 'U': { //NOM_SINGLE_SIDE_REPLACE_LONG
    struct itto_message_replace_long *message = (struct itto_message_replace_long *)m;
    OrderIdT old_order_id = be64toh(message->original_reference_delta);
    OrderIdT new_order_id = be64toh(message->new_reference_delta); 
    PriceT price          = be32toh(message->price) / EFH_PRICE_SCALE;
    SizeT size            = be32toh(message->size);

    FhOrder* o            = book->findOrder(old_order_id);
    if (o == NULL)  return false;

    s = (FhSecurity*)o->plevel->s;
    book->setSecurityPrevState(s);

    FhOrderType t    = o->type;
    SideT       side = o->plevel->side;

    book->deleteOrder(o);
    book->addOrder(s,new_order_id,t,price,size,side);
    break;
  }
//--------------------------------------------------------------
  case 'D': { //NOM_SINGLE_SIDE_DELETE 
    struct itto_message_delete *message = (struct itto_message_delete *)m;
    OrderIdT order_id = be64toh(message->reference_delta);
    FhOrder* o        = book->findOrder(order_id);
    if (o == NULL) return false;

    s = (FhSecurity*)o->plevel->s;
    book->setSecurityPrevState(s);

    book->deleteOrder(o);
    break;
  }
//--------------------------------------------------------------
  case 'G': { //NOM_SINGLE_SIDE_UPDATE
    struct itto_message_update *message = (struct itto_message_update *)m;
    OrderIdT order_id = be64toh(message->reference_delta);
    PriceT price      = be32toh(message->price) / EFH_PRICE_SCALE;
    SizeT size        = be32toh(message->size);

    FhOrder* o        = book->findOrder(order_id);
    if (o == NULL)  return false;

    s = (FhSecurity*)o->plevel->s;
    book->setSecurityPrevState(s);
    if (book->modifyOrder (o,price,size) < 0) { // modify order for price and size
      EKA_WARN("modifyOrder failed at NOM_SINGLE_SIDE_UPDATE");
      return true;
    }
    break;
  }
//--------------------------------------------------------------
  case 'k':   {//NOM_QUOTE_REPLACE_SHORT
    struct itto_quote_replace_short *message = (struct itto_quote_replace_short *)m;
    
    OrderIdT old_bid_order_id   = be64toh(message->original_bid_delta);
    OrderIdT new_bid_order_id   = be64toh(message->new_bid_delta);
    PriceT   bid_price          = (uint32_t)be16toh(message->bid_price) * 100 / EFH_PRICE_SCALE;
    SizeT    bid_size           = (uint32_t)be16toh(message->bid_size);
    OrderIdT old_ask_order_id   = be64toh(message->original_ask_delta);
    OrderIdT new_ask_order_id   = be64toh(message->new_ask_delta);
    PriceT   ask_price          = (uint32_t)be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE;
    SizeT    ask_size           = (uint32_t)be16toh(message->ask_size);

    FhOrder* bid_o = book->findOrder(old_bid_order_id);
    FhOrder* ask_o = book->findOrder(old_ask_order_id);

    if (bid_o != NULL) {
      FhPlevel* bid_p = bid_o->plevel;
      s = (FhSecurity*)bid_p->s;
    }

    if (ask_o != NULL) {
      FhPlevel* ask_p = ask_o->plevel;
      s = (FhSecurity*)ask_p->s;
    }

    if (bid_o == NULL && ask_o == NULL) return false;

    if (s == NULL) on_error ("s = NULL");
    book->setSecurityPrevState(s);

    if (bid_o != NULL) {
      FhOrderType t = bid_o->type;
      book->deleteOrder(bid_o);
      book->addOrder(s,new_bid_order_id,t,bid_price,bid_size,SideT::BID);
    }
    if (ask_o != NULL) {
      FhOrderType t = ask_o->type;
      book->deleteOrder(ask_o);
      book->addOrder(s,new_ask_order_id,t,ask_price,ask_size,SideT::ASK);
    }

    break;
  }
//--------------------------------------------------------------
  case 'K': { //NOM_QUOTE_REPLACE_LONG
    struct itto_quote_replace_long *message = (struct itto_quote_replace_long *)m;
    
    OrderIdT old_bid_order_id   = be64toh(message->original_bid_delta);
    OrderIdT new_bid_order_id   = be64toh(message->new_bid_delta);
    PriceT   bid_price          = be32toh(message->bid_price) / EFH_PRICE_SCALE;
    SizeT    bid_size           = be32toh(message->bid_size);
    OrderIdT old_ask_order_id   = be64toh(message->original_ask_delta);
    OrderIdT new_ask_order_id   = be64toh(message->new_ask_delta);
    PriceT   ask_price          = be32toh(message->ask_price) / EFH_PRICE_SCALE;
    SizeT    ask_size           = be32toh(message->ask_size);

    FhOrder* bid_o = book->findOrder(old_bid_order_id);
    FhOrder* ask_o = book->findOrder(old_ask_order_id);

    if (bid_o != NULL) {
      FhPlevel* bid_p = bid_o->plevel;
      s = (FhSecurity*)bid_p->s;
    }

    if (ask_o != NULL) {
      FhPlevel* ask_p = ask_o->plevel;
      s = (FhSecurity*)ask_p->s;
    }

    if (bid_o == NULL && ask_o == NULL) return false;

    if (s == NULL) on_error ("s = NULL");
    book->setSecurityPrevState(s);

    if (bid_o != NULL) {
      FhOrderType t = bid_o->type;
      book->deleteOrder(bid_o);
      book->addOrder(s,new_bid_order_id,t,bid_price,bid_size,SideT::BID);
    }
    if (ask_o != NULL) {
      FhOrderType t = ask_o->type;
      book->deleteOrder(ask_o);
      book->addOrder(s,new_ask_order_id,t,ask_price,ask_size,SideT::ASK);
    }

    break;
  }
//--------------------------------------------------------------
  case 'Y': { //NOM_QUOTE_DELETE 
    struct itto_quote_delete *message = (struct itto_quote_delete *)m;
    OrderIdT bid_order_id = be64toh(message->bid_delta);
    OrderIdT ask_order_id = be64toh(message->ask_delta);

    FhOrder* bid_o = book->findOrder(bid_order_id);
    FhOrder* ask_o = book->findOrder(ask_order_id);

    if (bid_o != NULL) {
      FhPlevel* p = bid_o->plevel;
      s = (FhSecurity*)p->s;
    }
    if (ask_o != NULL) {
      FhPlevel* p = ask_o->plevel;
      s = (FhSecurity*)p->s;
    }

    if (bid_o == NULL && ask_o == NULL)  return false;
    if (s == NULL) on_error ("s = NULL");

    book->setSecurityPrevState(s);
    if (bid_o != NULL) {
      if (book->deleteOrder(bid_o) < 0) {
	EKA_WARN("NOM_QUOTE_DELETE failed for OrderId %ju",bid_order_id);
	return true;
      }
    }
    if (ask_o != NULL) {
      if (book->deleteOrder(ask_o) < 0) {
	eka_print_nom_msg(stderr,(uint8_t*)m,id,sequence); fflush(stderr);
	EKA_WARN("NOM_QUOTE_DELETE failed for OrderId %ju",ask_order_id);
	return true;
      }
    }
    break;
  }
//--------------------------------------------------------------
  case 'P': { //NOM_OPTIONS_TRADE
    struct itto_options_trade *message = (struct itto_options_trade *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

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

  if (fh->print_parsed_messages) eka_print_nom_msg(parser_log,(uint8_t*)m,id,sequence);
  //  eka_print_nom_msg(stderr,(uint8_t*)m,id,sequence); fflush(stderr);
  //  book->printAll();
  //  book->printSecurity(s);

 /* ##################################################################### */

#ifdef EKA_TIME_CHECK
  auto finish = std::chrono::high_resolution_clock::now();
  uint64_t duration_ns = (uint64_t) std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();
  if (duration_ns > 50000) EKA_WARN("WARNING: \'%c\' processing took %ju ns",enc, duration_ns);
#endif

  if (! book->isEqualState(s))
    book->generateOnQuote (pEfhRunCtx, s, sequence, msg_timestamp,gapNum);

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

static inline uint64_t get_ts(const uint8_t* m) {
  uint64_t ts_tmp = 0;
  memcpy((uint8_t*)&ts_tmp+2,m+3,6);
  return be64toh(ts_tmp);
}

static void eka_print_nom_msg(FILE* md_file, const uint8_t* m, int gr, uint64_t sequence) {
  
  switch ((char)m[0]) {
  case 'a': { //NOM_ADD_ORDER_SHORT
    fprintf (md_file,"GR%d,SN:%ju,",gr,sequence);
    struct itto_add_order_short *message = (struct itto_add_order_short *)m;
    fprintf (md_file,"SID:%16u,%c,P:%8u,S:%8u\n",
	    be32toh (message->option_id),
	    (char)             (message->side),
	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE,
	    (uint32_t) be16toh (message->size)
	    );
    break;
  }
    //--------------------------------------------------------------
  case 'A' : { //NOM_ADD_ORDER_LONG
    fprintf (md_file,"GR%d,SN:%ju,",gr,sequence);
    struct itto_add_order_long *message = (struct itto_add_order_long *)m;
    fprintf (md_file,"SID:%16u,%c,P:%8u,S:%8u\n",
	    be32toh (message->option_id),
	    (char)             (message->side),
	    be32toh (message->price) / EFH_PRICE_SCALE,
	    be32toh (message->size)
	    );
    break;
  }
  default:
    break; 
  }
  fflush(md_file);
  return;

}


/* static void eka_print_nom_msg(FILE* md_file, const uint8_t* m, int gr, uint64_t sequence) { */
/*   fprintf (md_file,"GR%d,%s,SN:%ju,%3s(%c),",gr,(ts_ns2str(get_ts(m))).c_str(),sequence,ITTO_NOM_MSG((char)m[0]),(char)m[0]); */
/*   switch ((char)m[0]) { */
/*   case 'R': //ITTO_TYPE_OPTION_DIRECTORY  */
/*     break; */
/*   case 'M': // END OF SNAPSHOT */
/*     break; */
/*   case 'a': { //NOM_ADD_ORDER_SHORT */
/*     struct itto_add_order_short *message = (struct itto_add_order_short *)m; */
/*     fprintf (md_file,"SID:%16u,OID:%16ju,%c,P:%8u,S:%8u", */
/* 	    be32toh (message->option_id), */
/* 	    be64toh (message->order_reference_delta), */
/* 	    (char)             (message->side), */
/* 	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE, */
/* 	    (uint32_t) be16toh (message->size) */
/* 	    ); */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'A' : { //NOM_ADD_ORDER_LONG */
/*     struct itto_add_order_long *message = (struct itto_add_order_long *)m; */
/*     fprintf (md_file,"SID:%16u,OID:%16ju,%c,P:%8u,S:%8u", */
/* 	    be32toh (message->option_id), */
/* 	    be64toh (message->order_reference_delta), */
/* 	    (char)             (message->side), */
/* 	    be32toh (message->price) / EFH_PRICE_SCALE, */
/* 	    be32toh (message->size) */
/* 	    ); */
/*     break; */
/*   } */
/*   case 'S': //NOM_SYSTEM_EVENT */
/*     break; */
/*   case 'L': //NOM_BASE_REF -- do nothing         */
/*     break; */
/*   case 'H': { //NOM_TRADING_ACTION  */
/*     struct itto_trading_action *message = (struct itto_trading_action *)m; */
/*     fprintf (md_file,"SID:%16u,TS:%c",be32toh(message->option_id),message->trading_state); */
/*     break; */
/*   } */
/*   case 'O': { //NOM_OPTION_OPEN  */
/*     struct itto_option_open *message = (struct itto_option_open *)m; */
/*     fprintf (md_file,"SID:%16u,OS:%c",be32toh(message->option_id),message->open_state); */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'J': {  //NOM_ADD_QUOTE_LONG */
/*     struct itto_add_quote_long *message = (struct itto_add_quote_long *)m; */

/*     fprintf (md_file,"SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u", */
/* 	    be32toh (message->option_id), */
/* 	    be64toh (message->bid_reference_delta), */
/* 	    be32toh (message->bid_price) / EFH_PRICE_SCALE, */
/* 	    be32toh (message->bid_size), */
/* 	    be64toh (message->ask_reference_delta), */
/* 	    be32toh (message->ask_price) / EFH_PRICE_SCALE, */
/* 	    be32toh (message->ask_size) */
/* 	    ); */
/*     break; */
/*   } */
/*   case 'j': { //NOM_ADD_QUOTE_SHORT */
/*     struct itto_add_quote_short *message = (struct itto_add_quote_short *)m; */
    
/*     fprintf (md_file,"SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u", */
/* 	    be32toh (message->option_id), */
/* 	    be64toh (message->bid_reference_delta), */
/* 	    (uint32_t) be16toh (message->bid_price) / EFH_PRICE_SCALE, */
/* 	    (uint32_t) be16toh (message->bid_size), */
/* 	    be64toh (message->ask_reference_delta), */
/* 	    (uint32_t) be16toh (message->ask_price) / EFH_PRICE_SCALE, */
/* 	    (uint32_t) be16toh (message->ask_size) */
/* 	    ); */

/*     break; */
/*   } */

/*   case 'E':  { //NOM_SINGLE_SIDE_EXEC */
/*     struct itto_executed *message = (struct itto_executed *)m; */
/*     fprintf (md_file,"OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->executed_contracts)); */

/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'C': { //NOM_SINGLE_SIDE_EXEC_PRICE */
/*     struct itto_executed_price *message = (struct itto_executed_price *)m; */
/*     fprintf (md_file,"OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->size)); */

/*     break; */
/*   } */

/*     //-------------------------------------------------------------- */
/*   case 'X': { //NOM_ORDER_CANCEL */
/*     struct itto_order_cancel *message = (struct itto_order_cancel *)m; */
/*     fprintf (md_file,"OID:%16ju,S:%8u",be64toh(message->order_reference_delta),be32toh(message->cancelled_orders)); */

/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'u': {  //NOM_SINGLE_SIDE_REPLACE_SHORT */
/*     struct itto_message_replace_short *message = (struct itto_message_replace_short *)m; */
/*     fprintf (md_file,"OOID:%16ju,NOID:%16ju,P:%8u,S:%8u", */
/* 	    be64toh (message->original_reference_delta), */
/* 	    be64toh (message->new_reference_delta), */
/* 	    (uint32_t) be16toh (message->price) * 100 / EFH_PRICE_SCALE, */
/* 	    (uint32_t) be16toh (message->size) */
/* 	    ); */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'U': { //NOM_SINGLE_SIDE_REPLACE_LONG */
/*     struct itto_message_replace_long *message = (struct itto_message_replace_long *)m; */
/*     fprintf (md_file,"OOID:%16ju,NOID:%16ju,P:%8u,S:%8u", */
/* 	    be64toh (message->original_reference_delta), */
/* 	    be64toh (message->new_reference_delta), */
/* 	    be32toh (message->price) / EFH_PRICE_SCALE, */
/* 	    be32toh (message->size) */
/* 	    ); */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'D': { //NOM_SINGLE_SIDE_DELETE  */
/*     struct itto_message_delete *message = (struct itto_message_delete *)m; */
/*     fprintf (md_file,"OID:%16ju",be64toh(message->reference_delta)); */

/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'G': { //NOM_SINGLE_SIDE_UPDATE */
/*     struct itto_message_update *message = (struct itto_message_update *)m; */
/*     fprintf (md_file,"OID:%16ju,P:%8u,S:%8u", */
/* 	    be64toh (message->reference_delta), */
/* 	    be32toh (message->price) / EFH_PRICE_SCALE, */
/* 	    be32toh (message->size) */
/* 	    ); */

/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'k':   {//NOM_QUOTE_REPLACE_SHORT */
/*     struct itto_quote_replace_short *message = (struct itto_quote_replace_short *)m; */
/*     fprintf (md_file,"OBOID:%16ju,NBOID:%16ju,BP:%8u,BS:%8u,OAOID:%16ju,NAOID:%16ju,AP:%8u,AS:%8u", */
/* 	    be64toh (message->original_bid_delta), */
/* 	    be64toh(message->new_bid_delta), */
/* 	    (uint32_t) be16toh (message->bid_price) / EFH_PRICE_SCALE, */
/* 	    (uint32_t) be16toh (message->bid_size), */

/* 	    be64toh(message->original_ask_delta), */
/* 	    be64toh(message->new_ask_delta), */
/* 	    (uint32_t) be16toh(message->ask_price) * 100 / EFH_PRICE_SCALE, */
/* 	    (uint32_t) be16toh(message->ask_size) */
/* 	    ); */

/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'K': { //NOM_QUOTE_REPLACE_LONG */
/*     struct itto_quote_replace_long *message = (struct itto_quote_replace_long *)m; */
/*     fprintf (md_file,"OBOID:%16ju,NBOID:%16ju,BP:%8u,BS:%8u,OAOID:%16ju,NAOID:%16ju,AP:%8u,AS:%8u", */
/* 	    be64toh (message->original_bid_delta), */
/* 	    be64toh (message->new_bid_delta), */
/* 	    be32toh (message->bid_price) * 100 / EFH_PRICE_SCALE, */
/* 	    be32toh (message->bid_size), */

/* 	    be64toh (message->original_ask_delta), */
/* 	    be64toh (message->new_ask_delta), */
/* 	    be32toh (message->ask_price) / EFH_PRICE_SCALE, */
/* 	    be32toh (message->ask_size) */
/* 	    ); */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */

/*   case 'Y': { //NOM_QUOTE_DELETE  */
/*     struct itto_quote_delete *message = (struct itto_quote_delete *)m; */
/*     fprintf (md_file,"BOID:%16ju,AOID:%16ju",be64toh(message->bid_delta),be64toh(message->ask_delta)); */

/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */

/*   case 'P': { //NOM_OPTIONS_TRADE */
/*     struct itto_options_trade *message = (struct itto_options_trade *)m; */
/*     fprintf (md_file,"SID:%16u",be32toh(message->option_id)); */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'Q': { //NOM_CROSS_TRADE */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'B': { //NOM_BROKEN_EXEC */
/*     break; */
/*   } */
/*     //-------------------------------------------------------------- */
/*   case 'I': { //NOM_NOII */
/*     break; */
/*   } */
/*   default:  */
/*     on_error("UNEXPECTED Message type: enc=\'%c\'",(char)m[0]); */
/*   } */
/*   fprintf (md_file,"\n"); */
/*   fflush(md_file); */
/*   return; */

/* } */
