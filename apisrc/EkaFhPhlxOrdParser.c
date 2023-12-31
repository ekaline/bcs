#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>

#include "EkaFhParserCommon.h"
#include "EkaFhPhlxOrdGr.h"
#include "EkaFhPhlxOrdParser.h"

/* ######################################################### */
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
/* ######################################################### */

bool EkaFhPhlxOrdGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence,EkaFhMode op,
				 std::chrono::high_resolution_clock::time_point startTime) {
  char enc =  (char)m[0];

  if (op == EkaFhMode::DEFINITIONS && 
      ( enc == 'O' ||
	enc == 'X' )) return true; // ending the Definitions snapshot when getting first MD

  if (op == EkaFhMode::DEFINITIONS && enc != 'D') return false;
  if (op == EkaFhMode::SNAPSHOT    && enc == 'D') return false;

  uint64_t seconds  = (uint64_t) be32toh(((phlx_ord_generic_hdr*) m)->time_seconds);
  uint32_t nano_sec = be32toh(((phlx_ord_generic_hdr*) m)->time_nano);
  uint64_t ts = seconds * SEC_TO_NANO + nano_sec;

  FhSecurity* s = NULL;

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
    msg.commonDef.exchange       = EfhExchange::kPHLX;
    msg.commonDef.underlyingType = EfhSecurityType::kStock;
    // (message->expiration Denotes the explicit expiration date of the option.
    // Bits 0-6 = Year (0-99)
    // Bits 7-10 = Month (1-12)
    // Bits 11-15 = Day (1-31)
    // Bit 15 is least significant bit

    uint16_t expiration = be16toh(message->expiration);
    uint16_t year  = (expiration >> 9) & 0x007F;
    uint16_t month = (expiration >> 5) & 0x000F;
    uint16_t day   = (expiration     ) & 0x001F;
    msg.commonDef.expiryDate     = (2000 + year) * 10000 + month * 100 + day;
    msg.commonDef.contractSize   = 0;

    msg.strikePrice           = be32toh(message->strike_price) / EFH_PHLX_STRIKE_PRICE_SCALE;
    if (message->option_type != 'C' &&  message->option_type != 'P') 
      on_error("Unexpected message->option_type == \'%c\'",message->option_type);
    msg.optionType            = message->option_type == 'C' ?  EfhOptionType::kCall : EfhOptionType::kPut;

    copySymbol(msg.commonDef.underlying, message->underlying_symbol);
    copySymbol(msg.commonDef.classSymbol,message->security_symbol);

    pEfhRunCtx->onEfhOptionDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    return false;
  }

    /* -------------------------------------------------- */
  case 'O': { // phlx_ord_simple_order
    phlx_ord_simple_order* message = (phlx_ord_simple_order*)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    OrderIdT order_id = be64toh(message->order_id);
    SideT    side     = sideDecode(message->side);
    SizeT    size     = be32toh(message->exec_size);
    PriceT   price    = be32toh(message->price);
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

    FhOrder* o = book->findOrder(order_id);
    if (o != NULL) {
      if (s != o->plevel->s) on_error("Order's message security_id %u != book security_id %u",
				      security_id,((FhSecurity*)o->plevel->s)->secId);
    }

    book->setSecurityPrevState(s);
    if (order_status == 'F' || order_status == 'C') { // delete order
      if (size != 0) on_error("Unexpected combination: order_status \'%c\' && size %u",
			      order_status,size);
      if (o == NULL) return false;
      book->deleteOrder (o);
    } else {
      if (o == NULL) { // add order
	FhOrderType o_type = FhOrderType::OTHER;
	switch (message->customer_indicator) {
	case 'C' : //      “C” = Customer Order
	  o_type = message->all_or_none == 'Y' ? FhOrderType::CUSTOMER_AON : FhOrderType::CUSTOMER;
	  break;
	case 'F' : //      “F” = Firm Order
	case 'M' : //      “M” = On-floor Market Maker
	case 'B' : //      “B” = Broker Dealer Order
	case 'P' : //      “P” = Professional Order
	  o_type = message->all_or_none == 'Y' ? FhOrderType::BD_AON : FhOrderType::BD;
	break;
	case ' ' : //      “ ” = N/A (For Implied Order)
	  return false;
	}
	book->addOrder(s,order_id,o_type,price,size,side);
      } else { // modify order
	book->modifyOrder (o,price,size);
      }
    }
    break;

  }

    /* -------------------------------------------------- */
  case 'H': { // phlx_ord_option_trading_action
    phlx_ord_option_trading_action  *message = (phlx_ord_option_trading_action *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;
    book->setSecurityPrevState(s);
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
    break;
  }

    /* -------------------------------------------------- */
  case 'P': { // phlx_ord_security_open_closed
    phlx_ord_security_open_closed  *message = (phlx_ord_security_open_closed *)m;
    SecurityIdT security_id = be32toh(message->option_id);
    s = book->findSecurity(security_id);
    if (s == NULL) return false;
    book->setSecurityPrevState(s);

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
  if (s == NULL) on_error ("Trying to generate TOB update from s == NULL");
  if (op != EkaFhMode::SNAPSHOT) {
    if (! book->isEqualState(s))
      book->generateOnQuote (pEfhRunCtx, s, sequence, ts,gapNum);
  }
  return false;
}

