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

/* ----------------------------------------------------------------------- */
void hexDump (const char* desc, void *addr, int len);

inline uint hsvfTableLen(const uint8_t* msg) {
  if (memcmp(msg,"V ",2) == 0) return 17;
  if (memcmp(msg,"Z ",2) == 0) return 20;
  if (memcmp(msg,"C ",2) == 0) return 76;
  if (memcmp(msg,"CS",2) == 0) return 79;
  if (memcmp(msg,"D ",2) == 0) return 40;
  if (memcmp(msg,"F ",2) == 0) return 68;
  if (memcmp(msg,"FS",2) == 0) return 79;
  if (memcmp(msg,"GC",2) == 0) return 25;
  if (memcmp(msg,"GR",2) == 0) return 19;
  if (memcmp(msg,"GS",2) == 0) return 15;
  if (memcmp(msg,"I ",2) == 0) return 68;
  if (memcmp(msg,"IS",2) == 0) return 71;
  if (memcmp(msg,"J ",2) == 0) return 119;
  if (memcmp(msg,"L ",2) == 0) return 93;
  if (memcmp(msg,"N ",2) == 0) return 127;
  if (memcmp(msg,"NS",2) == 0) return 116;
  if (memcmp(msg,"Q ",2) == 0) return 12;
  if (memcmp(msg,"QS",2) == 0) return 12;
  if (memcmp(msg,"S ",2) == 0) return 18;
  if (memcmp(msg,"M ",2) == 0) return 84;
  if (memcmp(msg,"MS",2) == 0) return 94;
  if (memcmp(msg,"O ",2) == 0) return 80;
  if (memcmp(msg,"OS",2) == 0) return 91;
  if (memcmp(msg,"T ",2) == 0) return 47;
  if (memcmp(msg,"TS",2) == 0) return 57;

  if (memcmp(msg,"ER",2) == 0) return 95;
  if (memcmp(msg,"KI",2) == 0) return 11;
  if (memcmp(msg,"KO",2) == 0) return 11;
  if (memcmp(msg,"LI",2) == 0) return 51;
  if (memcmp(msg,"LO",2) == 0) return 11;
  if (memcmp(msg,"RB",2) == 0) return 11;
  if (memcmp(msg,"RE",2) == 0) return 11;
  if (memcmp(msg,"RT",2) == 0) return 31;
  if (memcmp(msg,"U ",2) == 0) return 18;
  if (memcmp(msg,"ER",2) == 0) return 95;

  return 0;
}

uint getHsvfMsgLen(const uint8_t* pkt, int bytes2run) {
  uint idx = 0;

  if (pkt[idx] != HsvfSom) {
    hexDump("Msg with no HsvfSom (0x2)",(void*)pkt,bytes2run);
    on_error("0x%x met while HsvfSom 0x%x is expected",pkt[idx],HsvfSom);
    return 0;
  }

  const uint8_t* msg = &pkt[10];
  uint tableLen = hsvfTableLen(msg);

  if (tableLen != 0) {
    if (pkt[tableLen+1] != HsvfEom) {
      hexDump("Hsvf Msg with wrong hsvfTableLen",pkt,bytes2run);
      on_error("tableLen of \'%c%c\' = %u, but last char = 0x%x",
	       (char)msg[0],(char)msg[1],tableLen,pkt[tableLen+1]);
    }
    return tableLen + 2;
  }


  do {
    idx++;
    if ((int)idx > bytes2run) {
      hexDump("Msg with no HsvfEom (0x3)",(void*)pkt,bytes2run);
      on_error("HsvfEom not met after %u characters",idx);
    }
  } while (pkt[idx] != HsvfEom);

  return idx + 1;
}

uint64_t getHsvfMsgSequence(const uint8_t* msg) {
  const HsvfMsgHdr* msgHdr = (const HsvfMsgHdr*)&msg[1];
  std::string seqString = std::string(msgHdr->sequence,sizeof(msgHdr->sequence));
  return std::stoul(seqString,nullptr,10);
}

uint trailingZeros(const uint8_t* p, uint maxChars) {
  uint idx = 0;
  while (p[idx] == 0x0 && idx < maxChars) {
    idx++; // skipping trailing '\0' chars
  }
  return idx;
}

inline uint64_t charSymbol2SecurityId(const char* charSymbol) {
  uint64_t hashRes = 0;
  uint shiftBits = 0;

  uint64_t fieldSize = 0;
  uint64_t fieldMask = 0;

  uint64_t f = 0;

  /* // 5 letters * 5 bits = 25 bits */
  /* fieldSize = 5; */
  /* fieldMask = (0x1 << fieldSize) - 1; */
  /* for (int i = 0; i < 5; i++) { */
  /*   uint64_t nameLetter = (charSymbol[i] - 'A') & fieldMask; */
  /*   f = nameLetter << shiftBits; */
  /*   hashRes |=  f; */
  /*   // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes); */
  /*   shiftBits += fieldSize; */
  /* }  */
  /* // shiftBits = 25; */

  // 5 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  uint64_t month = (charSymbol[6] - 'A') & fieldMask;
  f = month << shiftBits;
  hashRes |= f;
  // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 30;

  // 9,999,999 = 24 bits
  fieldSize = 24;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string priceStr = std::string(&charSymbol[8],7);
  uint64_t price = std::stoul(priceStr,nullptr,10) & fieldMask;
  f = price << shiftBits;
  hashRes |= f;
  // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 54;

  // 'A' .. 'G' = 3 bits
  fieldSize = 3;
  fieldMask = (0x1 << fieldSize) - 1;
  uint64_t priceFractionIndicator = (charSymbol[15] - 'A') & fieldMask;
  f = priceFractionIndicator << shiftBits;
  hashRes |= f;
  // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 57;

  // year offset = 2 bits
  fieldSize = 2;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string yearStr = std::string(&charSymbol[16],2);
  uint64_t year = (std::stoi(yearStr,nullptr,10) - 20) & fieldMask;
  f = year << shiftBits;
  hashRes |= f;
  // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 59;

  // 5 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string dayStr = std::string(&charSymbol[18],2);
  uint64_t day = std::stoi(dayStr,nullptr,10) & fieldMask;
  f = day << shiftBits;
  hashRes |= f;
  // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 64;


  // 5 letters * 5 bits = 25 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  for (int i = 0; i < 5; i++) {
    uint64_t nameLetter = (charSymbol[i] - 'A') & fieldMask;
    f = nameLetter << shiftBits;
    hashRes |=  f;
    // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
    shiftBits += fieldSize;
  } 
  // shiftBits = 25;

  return hashRes;
}

/* ----------------------------------------------------------------------- */

template <class T> inline T getNumField(char* f, size_t fSize) {
  std::string fieldString = std::string(f,fSize);
  return static_cast<T>(std::stoul(fieldString,nullptr,10));
}

/* ----------------------------------------------------------------------- */
EfhOptionType getOptionType(char* c) {
  if (c[6] >= 'A' && c[6] <= 'L') return EfhOptionType::kCall;
  if (c[6] >= 'M' && c[6] <= 'X') return EfhOptionType::kPut;
  on_error("Unexpected Month Code \'%c\'",c[6]);
}

/* ----------------------------------------------------------------------- */
uint64_t getMonth(char* c, EfhOptionType optType) {
  if (optType == EfhOptionType::kCall) return c[6] - 'A' + 1;
  return c[6] - 'M' + 1;
}
/* ----------------------------------------------------------------------- */
uint64_t getYear(char* c) {
  return getNumField<uint64_t>(c + 16,2);
}
/* ----------------------------------------------------------------------- */
uint64_t getDay(char* c) {
  return getNumField<uint64_t>(c + 18,2);
}
/* ----------------------------------------------------------------------- */
inline uint32_t getFractionIndicator(char FI) {
  switch (FI) {
  case '0' : return 1e0;
  case '1' : return 1e1;
  case '2' : return 1e2;
  case '3' : return 1e3;
  case '4' : return 1e4;
  case '5' : return 1e5;
  case '6' : return 1e6;
  case '7' : return 1e7;
  case '8' : return 1e8;
  case '9' : return 1e9;
  default:
    on_error("Unexpected FractionIndicator \'%c\'",FI);
  }

}
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
  sprintf(dst+6,"%02u%02u%02u%c%08u",y,m,d,call_put,msg->strikePrice / 10);
  return;
}

/* ----------------------------------------------------------------------- */

bool EkaFhBoxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,const unsigned char* m,uint64_t sequence, EkaFhMode op) {
  uint pos = 0;

  FhSecurity* s = NULL;

  HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&m[pos];

  uint8_t* msgBody = (uint8_t*)msgHdr + sizeof(HsvfMsgHdr);

  //  EKA_LOG("%s:%u: %ju \'%c%c\'",EKA_EXCH_DECODE(exch),id,seq,msgHdr->MsgType[0],msgHdr->MsgType[1]);
  //===================================================
  if (memcmp(msgHdr->MsgType,"J ",sizeof(msgHdr->MsgType)) == 0) { // OptionInstrumentKeys
    //    if (op == EkaFhMode::SNAPSHOT) return false;
    if (op != EkaFhMode::DEFINITIONS) return false;

    HsvfOptionInstrumentKeys* boxMsg = (HsvfOptionInstrumentKeys*)msgBody;
    
    char* symb = boxMsg->InstrumentDescription;

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
  } else if (memcmp(msgHdr->MsgType,"F ",sizeof(msgHdr->MsgType)) == 0) { // OptionQuote
    if (op == EkaFhMode::DEFINITIONS) return true; // Dictionary is done

    HsvfOptionQuote* boxMsg = (HsvfOptionQuote*)msgBody;

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
  } else {
  };
  //===================================================

  return false;
}

