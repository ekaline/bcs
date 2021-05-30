#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <string>

#include "EkaFhBoxGr.h"
#include "EkaFhBoxParser.h"

using namespace Hsvf;

/* ----------------------------------------------------------------------- */
template <class FhSecurity> 
inline int getStatus(FhSecurity* s, char statusMarker) {
  switch (statusMarker) {
  case 'Y' : // Pre-opening phase
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kPreopen;
    break;

  case 'O' : // Opening phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kNormal;
    break;

  case 'T' : // Opened for Trading
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kNormal;
    break;

  case 'F' : // Forbidden phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'H' : // Trading Halted
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'R' : // Reserved phase
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kPreopen;
    break;

  case 'S' : // Suspended phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'Z' : // Frozen
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'A' : // Surveillance Intervention phase
    s->option_open    = true;
    s->trading_action = EfhTradeStatus::kHalted;
    break;

  case 'C' : // Closed
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kClosed;
    break;

  case 'B' : // Beginning of day inquiries
    s->option_open    = false;
    s->trading_action = EfhTradeStatus::kClosed;
    break;

  case ' ' : // Not Used
    break;

  default:
    on_error("unexpected statusMarker \'%c\'",statusMarker);
  }
  return 0;
}

/* ----------------------------------------------------------------------- */

static void eka_create_avt_definition (char* dst, const EfhDefinitionMsg* msg) {
  uint8_t y,m,d;

  d = msg->expiryDate % 100;
  m = ((msg->expiryDate - d) / 100) % 100;
  y = msg->expiryDate / 10000 - 2000;

  memcpy(dst,msg->underlying,6);
  for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_';
  char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P';
  sprintf(dst+6,"%02u%02u%02u%c%08jd",y,m,d,call_put,msg->strikePrice / 10);
  return;
}

/* ----------------------------------------------------------------------- */

bool EkaFhBoxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence, EkaFhMode op) {
  uint pos = 0;

  FhSecurity* s = NULL;

  const HsvfMsgHdr* msgHdr = (const HsvfMsgHdr*)&m[pos];

  const uint8_t* msgBody = (uint8_t*)msgHdr + sizeof(HsvfMsgHdr);

  //  EKA_LOG("%s:%u: %ju \'%c%c\'",EKA_EXCH_DECODE(exch),id,seq,msgHdr->MsgType[0],msgHdr->MsgType[1]);
  //===================================================
  if (memcmp(msgHdr->MsgType,"F ",sizeof(msgHdr->MsgType)) == 0) { // OptionQuote
    if (op == EkaFhMode::DEFINITIONS) return true; // Dictionary is done

    const HsvfOptionQuote* boxMsg = (const HsvfOptionQuote*)msgBody;

    SecurityIdT security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);

    s = book->findSecurity(security_id);
    if (s == NULL) return false;

    //    EKA_LOG("OptionQuote for %s",boxMsg->InstrumentDescription + '\0');

    s->bid_price     = getNumField<uint32_t>(boxMsg->BidPrice,sizeof(boxMsg->BidPrice)) * getFractionIndicator(boxMsg->BidPriceFractionIndicator);
    s->bid_size      = getNumField<uint32_t>(boxMsg->BidSize,sizeof(boxMsg->BidSize));
    s->bid_cust_size = getNumField<uint32_t>(boxMsg->PublicCustomerBidSize,sizeof(boxMsg->PublicCustomerBidSize));

    s->ask_price     = getNumField<uint32_t>(boxMsg->AskPrice,sizeof(boxMsg->AskPrice)) * getFractionIndicator(boxMsg->AskPriceFractionIndicator);
    s->ask_size      = getNumField<uint32_t>(boxMsg->AskSize,sizeof(boxMsg->AskSize));
    s->ask_cust_size = getNumField<uint32_t>(boxMsg->PublicCustomerAskSize,sizeof(boxMsg->PublicCustomerAskSize));

    getStatus<FhSecurity>(s,boxMsg->InstrumentStatusMarker);
    if (op != EkaFhMode::SNAPSHOT)
      book->generateOnQuote (pEfhRunCtx, s, sequence, gr_ts, gapNum);

    //===================================================
  } else if (memcmp(msgHdr->MsgType,"Z ",sizeof(msgHdr->MsgType)) == 0) { // SystemTimeStamp
    char* timeStamp = ((HsvfSystemTimeStamp*)msgBody)->TimeStamp;
    uint64_t hour = getNumField<uint64_t>(&timeStamp[0],2);
    uint64_t min  = getNumField<uint64_t>(&timeStamp[2],2);
    uint64_t sec  = getNumField<uint64_t>(&timeStamp[4],2);
    uint64_t ms   = getNumField<uint64_t>(&timeStamp[6],3);
    
    gr_ts = ((hour * 3600 + min * 60 + sec) * 1000 + ms) * 1000000;
  } else if (memcmp(msgHdr->MsgType,"U ",sizeof(msgHdr->MsgType)) == 0) { // EndOfTransmission
    EKA_LOG("%s:%u End Of Transmission",EKA_EXCH_DECODE(exch),id);
    return true;
  } else if (memcmp(msgHdr->MsgType,"J ",sizeof(msgHdr->MsgType)) == 0) { // OptionInstrumentKeys
    //    if (op == EkaFhMode::SNAPSHOT) return false;
    if (op != EkaFhMode::DEFINITIONS) return false;

    const HsvfOptionInstrumentKeys* boxMsg = (const HsvfOptionInstrumentKeys*)msgBody;
    
    const char* symb = boxMsg->InstrumentDescription;

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = charSymbol2SecurityId(symb);
    msg.header.sequenceNumber = sequence;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = 0;
    msg.securityType          = EfhSecurityType::kOpt;
    msg.optionType            = getOptionType(symb);
    msg.expiryDate            = (2000 + getYear(symb)) * 10000 + getMonth(symb,msg.optionType) * 100 + getDay(symb);
    msg.contractSize          = 0;
    msg.strikePrice           = getNumField<uint32_t>(&symb[8],7) * getFractionIndicator(symb[15]) / EFH_HSV_BOX_STRIKE_PRICE_SCALE;
    msg.exchange              = EfhExchange::kBOX;

    memcpy (&msg.underlying,boxMsg->UnderlyingSymbolRoot,std::min(sizeof(msg.underlying),sizeof(boxMsg->UnderlyingSymbolRoot)));
    memcpy (&msg.classSymbol,&symb[0],6);

#ifdef TEST_PRINT_DICT
    char avtSecName[32] = {};
    eka_create_avt_definition(avtSecName,&msg);
    fprintf(dev->testDict,"\'%s\', %s, 0x%016jx,%ju\n",
	    std::string(boxMsg->InstrumentDescription,20).c_str(),avtSecName,
	    msg.header.securityId,msg.header.securityId);
#endif
    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"N ",sizeof(msgHdr->MsgType)) == 0) { // OptionSummary

    //===================================================
  } 
 
  //===================================================

  return false;
}

