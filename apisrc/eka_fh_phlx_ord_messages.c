#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "eka_fh.h"
#include "eka_fh_phlx_ord_messages.h"
#include "eka_data_structs.h"
#include "EkaDev.h"
#include "Efh.h"
#include "eka_fh_book.h"


bool FhPhlxOrdGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  char enc =  (char)m[0];

  if (op == EkaFhMode::DEFINITIONS && 
      ( enc == 'O' ||
	enc == 'X' )) return true; // ending the Definitions snapshot when getting first MD

  if (op == EkaFhMode::DEFINITIONS && enc != 'D') return false;
  if (op == EkaFhMode::SNAPSHOT    && enc == 'D') return false;

  uint64_t seconds  = (uint64_t) be32toh(((phlx_ord_generic_hdr*) m)->time_seconds);
  uint32_t nano_sec = be32toh(((phlx_ord_generic_hdr*) m)->time_nano);
  uint64_t ts = seconds * SEC_TO_NANO + nano_sec;

  fh_b_security* tob_s = NULL;
  fh_b_security_state prev_s;
  prev_s.reset();

  switch (enc) {    
    /* -------------------------------------------------- */
  case 'S':  { // phlx_ord_system_event
    //    phlx_ord_system_event  *message = (phlx_ord_system_event *)m;
    //    uint8_t  event_code = message->event_code;
    return false;
  }
    /* -------------------------------------------------- */
  case 'D':  { // phlx_ord_option_directory
    phlx_ord_option_directory  *message = (phlx_ord_option_directory *)m;

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
    // (message->expiration Denotes the explicit expiration date of the option.
    // Bits 0-6 = Year (0-99)
    // Bits 7-10 = Month (1-12)
    // Bits 11-15 = Day (1-31)
    // Bit 15 is least significant bit

    uint16_t expiration = be16toh(message->expiration);
    uint16_t year  = (expiration >> 9) & 0x007F;
    uint16_t month = (expiration >> 5) & 0x000F;
    uint16_t day   = (expiration     ) & 0x001F;
    msg.expiryDate            = (2000 + year) * 10000 + month * 100 + day;
    msg.contractSize          = 0;
    msg.strikePrice           = be32toh(message->strike_price) / EFH_PHLX_STRIKE_PRICE_SCALE;
    msg.exchange              = EfhExchange::kPHLX;
    if (message->option_type != 'C' &&  message->option_type != 'P') 
      on_error("Unexpected message->option_type == \'%c\'",message->option_type);
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    memcpy (&msg.underlying, message->underlying_symbol,std::min(sizeof(msg.underlying), sizeof(message->underlying_symbol)));
    memcpy (&msg.classSymbol,message->security_symbol,  std::min(sizeof(msg.classSymbol),sizeof(message->security_symbol)));

    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }

0x8b29 = 1000 1011 0010 1001

 1001 0100 110 10001

 100 1010 0 110 10001

    /* -------------------------------------------------- */
  case 'O': { // phlx_ord_simple_order
    phlx_ord_simple_order* message = (phlx_ord_simple_order*)m;
    uint32_t security_id = be32toh(message->option_id);
    fh_b_security* s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL && !((NomBook*)book)->subscribe_all) return false;
    if (s == NULL && ((NomBook*)book)->subscribe_all) s = ((NomBook*)book)->subscribe_security((uint32_t ) security_id & 0x00000000FFFFFFFF,0,0,0,0);

    uint64_t order_id = be64toh(message->order_id);
    char     side     = (char)message->side;
    uint32_t size     = be32toh(message->exec_size);
    uint32_t price    = be32toh(message->price);
    char     order_status = message->order_status;
    if (order_status != 'O' && order_status != 'F' && order_status != 'C')
      on_error("Unexepcted order_status \'%c\'",order_status);

    switch (message->market_qualifier) {
    case 'O' : // “O” = Opening Order
    case 'I' : // “I” = Implied Order 
    case ' ' : // “ ” = N/A (field is space char)
      break; 
    default:
      on_error("Unexpected market_qualifier \'%c\'",message->market_qualifier);
    }

    if (message->all_or_none != 'Y' && message->all_or_none != 'N')
      on_error("Unexpected message->all_or_none \'%c\'",message->all_or_none);

    fh_b_order* o = ((NomBook*)book)->find_order(order_id);
    if (o != NULL) {
      if (o->plevel == NULL) on_error("o->plevel == NULL");
      if (o->plevel->s == NULL) on_error("o->plevel->s == NULL");
      if (s != o->plevel->s) 
	on_error("Order's message security_id %u != book security_id %u",
		 security_id,o->plevel->s->security_id);
    }

    prev_s.set(s);
    if (order_status == 'F' || order_status == 'C') { // delete order
      if (size != 0) 
	on_error("Unexpected combination: order_status \'%c\' && size %u",
		 order_status,size);
      if (o == NULL) return false;
      ((NomBook*)book)->delete_order (o);
    } else {
      if (o == NULL) { // add order
	fh_b_order::type_t o_type = fh_b_order::type_t::OTHER;
	switch (message->customer_indicator) {
	case 'C' : //      “C” = Customer Order
	  o_type = message->all_or_none == 'Y' ? fh_b_order::type_t::CUSTOMER_AON : fh_b_order::type_t::CUSTOMER;
	  break;
	case 'F' : //      “F” = Firm Order
	case 'M' : //      “M” = On-floor Market Maker
	case 'B' : //      “B” = Broker Dealer Order
	case 'P' : //      “P” = Professional Order
	  o_type = message->all_or_none == 'Y' ? fh_b_order::type_t::BD_AON : fh_b_order::type_t::BD;
	break;
	case ' ' : //      “ ” = N/A (For Implied Order)
	  return false;
	}
	((NomBook*)book)->add_order2book(s,order_id,o_type,price,size,side);
      } else { // modify order
	((NomBook*)book)->modify_order (o,price,size);
      }
    }
    tob_s = s;
    break;

  }

    /* -------------------------------------------------- */
  case 'H': { // phlx_ord_option_trading_action
    phlx_ord_option_trading_action  *message = (phlx_ord_option_trading_action *)m;
    uint32_t security_id = be32toh(message->option_id);
    fh_b_security* s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) return false;
    prev_s.set(s);
    switch (message->current_trading_state) {
    case 'H' : 
      s->trading_action = EfhTradeStatus::kHalted;
      break;
    case 'T' :
      s->trading_action = s->option_open ? EfhTradeStatus::kNormal : EfhTradeStatus::kClosed;
      break;
    default:
      on_error("Unexpected current_trading_state \'%c\'",message->current_trading_state);
    }
    tob_s = s;
    break;
  }

    /* -------------------------------------------------- */
  case 'P': { // phlx_ord_security_open_closed
    phlx_ord_security_open_closed  *message = (phlx_ord_security_open_closed *)m;
    uint32_t security_id = be32toh(message->option_id);
    fh_b_security* s = ((NomBook*)book)->find_security(security_id);
    if (s == NULL) return false;
    prev_s.set(s);

    switch (message->open_state) {
    case 'Y' :
      s->option_open = true;
      s->trading_action = EfhTradeStatus::kNormal;
      break;
    case 'N' :
      s->option_open = false;
      s->trading_action = EfhTradeStatus::kClosed;
      break;
    default:
      on_error("Unexpected open_state \'%c\'",message->open_state);
    }
    tob_s = s;
    break;
  }
    /* -------------------------------------------------- */

  case 'R': // Complex Order Strategy
  case 'I': // Strategy Trading Action
  case 'Q': // Strategy Open/Closed
  case 'X': // Complex Order
  case 'A': // Auction Notification
  case 'C': // COLA Notification
    return false;
  default: 
    EKA_WARN("WARNING: Unexpected message: \'%c\'",enc);
    return false;
  }
  if (tob_s == NULL) on_error ("Trying to generate TOB update from tob_s == NULL");
  if (op != EkaFhMode::SNAPSHOT)
    ((NomBook*)book)->generateOnQuote (pEfhRunCtx, tob_s, sequence, ts,gapNum);

  return false;
}

