#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <assert.h>
#include <string>

#include "eka_fh.h"
#include "eka_hsvf_box_messages.h"
#include "eka_data_structs.h"
#include "EkaDev.h"
#include "Efh.h"

/* ----------------------------------------------------------------------- */

inline uint64_t charSymbol2SecurityId(const char* charSymbol) {
  uint64_t hashRes = 0;
  uint shiftBits = 0;

  uint64_t fieldSize = 0;
  uint64_t fieldMask = 0;

  uint64_t f = 0;

  // 5 letters * 5 bits = 25 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  for (int i = 0; i < 5; i++) {
    uint64_t nameLetter = (charSymbol[i] - 'A') & fieldMask;
    f = nameLetter << shiftBits;
    hashRes |=  f;
    TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
    shiftBits += fieldSize;
  } 
  // shiftBits = 25;

  // 5 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  uint64_t month = (charSymbol[6] - 'A') & fieldMask;
  f = month << shiftBits;
  hashRes |= f;
  TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 30;

  // 9,999,999 = 24 bits
  fieldSize = 24;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string priceStr = std::string(&charSymbol[8],7);
  uint64_t price = std::stoul(priceStr,nullptr,10) & fieldMask;
  f = price << shiftBits;
  hashRes |= f;
  TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 54;

  // 'A' .. 'G' = 3 bits
  fieldSize = 3;
  fieldMask = (0x1 << fieldSize) - 1;
  uint64_t priceFractionIndicator = (charSymbol[15] - 'A') & fieldMask;
  f = priceFractionIndicator << shiftBits;
  hashRes |= f;
  TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 57;

  // year offset = 2 bits
  fieldSize = 2;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string yearStr = std::string(&charSymbol[16],2);
  uint64_t year = (std::stoi(yearStr,nullptr,10) - 20) & fieldMask;
  f = year << shiftBits;
  hashRes |= f;
  TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 59;

  // 5 bits
  fieldSize = 5;
  std::string dayStr = std::string(&charSymbol[18],2);
  uint64_t day = std::stoi(dayStr,nullptr,10) & fieldMask;
  f = day << shiftBits;
  hashRes |= f;
  TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
  shiftBits += fieldSize;
  // shiftBits = 64;

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
  if (optType == EfhOptionType::kCall) return c[6] - 'A';
  return c[6] - 'M';
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
  case '0' : return 1;
  case '1' : return 10;
  case '2' : return 100;
  case '3' : return 1000;
  case '4' : return 10000;
  case '5' : return 100000;
  case '6' : return 1000000;
  case '7' : return 10000000;
  case '8' : return 100000000;
  case '9' : return 1000000000;
  default:
    on_error("Unexpected FractionIndicator \'%c\'",FI);
  }

}
/* ----------------------------------------------------------------------- */
inline int getStatus(fh_b_security64* s, char statusMarker) {
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

bool FhBoxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint *msgLen,EkaFhMode op) {
  uint pos = 0;

  fh_b_security64* s = NULL;

  HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&m[pos];

  uint64_t seq = getNumField<uint64_t>(msgHdr->sequence,sizeof(msgHdr->sequence));

  uint8_t* msgBody = (uint8_t*)msgHdr + sizeof(HsvfMsgHdr);

  //  EKA_LOG("%s:%u: %ju \'%c%c\'",EKA_EXCH_DECODE(exch),id,seq,msgHdr->MsgType[0],msgHdr->MsgType[1]);
  //===================================================
  if (memcmp(msgHdr->MsgType,"J ",sizeof(msgHdr->MsgType)) == 0) { // OptionInstrumentKeys
    *msgLen = sizeof(HsvfMsgHdr) + sizeof(EfhDefinitionMsg);
    HsvfOptionInstrumentKeys* boxMsg = (HsvfOptionInstrumentKeys*)msgBody;
    
    char* symb = boxMsg->InstrumentDescription;

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = charSymbol2SecurityId(symb);
    msg.header.sequenceNumber = seq;
    msg.header.timeStamp      = 0;
    msg.header.gapNum         = gapNum;

    //    msg.secondaryGroup        = 0;
    msg.securityType          = EfhSecurityType::kOpt;
    msg.optionType            = getOptionType(symb);
    msg.expiryDate            = (2000 + getYear(symb)) * 10000 + getMonth(symb,msg.optionType) * 100 + getDay(symb);
    msg.contractSize          = 0;
    msg.strikePrice           = getNumField<uint32_t>(&symb[8],7) / EFH_HSV_BOX_STRIKE_PRICE_SCALE;
    msg.exchange              = EfhExchange::kBOX;

    memcpy (&msg.classSymbol,boxMsg->UnderlyingSymbolRoot,std::min(sizeof(msg.classSymbol),sizeof(boxMsg->UnderlyingSymbolRoot)));
    memcpy (&msg.underlying,&symb[0],6);


    pEfhRunCtx->onEfhDefinitionMsgCb(&msg, (EfhSecUserData) 0, pEfhRunCtx->efhRunUserData);
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"N ",sizeof(msgHdr->MsgType)) == 0) { // OptionSummary
    *msgLen = sizeof(HsvfMsgHdr) + sizeof(HsvfOptionSummary);

    //===================================================
  } else if (memcmp(msgHdr->MsgType,"F ",sizeof(msgHdr->MsgType)) == 0) { // OptionQuote
    *msgLen = sizeof(HsvfMsgHdr) + sizeof(HsvfOptionQuote);

    if (op == EkaFhMode::DEFINITIONS) return true; // Dictionary is done

    HsvfOptionQuote* boxMsg = (HsvfOptionQuote*)msgBody;

    uint64_t security_id = charSymbol2SecurityId(boxMsg->InstrumentDescription);

    s = ((TobBook*)book)->find_security64(security_id);
    if (s == NULL && !((TobBook*)book)->subscribe_all) return false;
    if (s == NULL && book->subscribe_all) 
      s = book->subscribe_security64(security_id,0,0);

    if (s == NULL) on_error("s == NULL");
    s->bid_price     = getNumField<uint32_t>(boxMsg->BidPrice,sizeof(boxMsg->BidPrice)) * getFractionIndicator(boxMsg->BidPriceFractionIndicator);
    s->bid_size      = getNumField<uint32_t>(boxMsg->BidSize,sizeof(boxMsg->BidSize));
    s->bid_cust_size = getNumField<uint32_t>(boxMsg->PublicCustomerBidSize,sizeof(boxMsg->PublicCustomerBidSize));

    s->ask_price     = getNumField<uint32_t>(boxMsg->AskPrice,sizeof(boxMsg->AskPrice)) * getFractionIndicator(boxMsg->AskPriceFractionIndicator);
    s->ask_size      = getNumField<uint32_t>(boxMsg->AskSize,sizeof(boxMsg->AskSize));
    s->ask_cust_size = getNumField<uint32_t>(boxMsg->PublicCustomerAskSize,sizeof(boxMsg->PublicCustomerAskSize));

    getStatus(s,boxMsg->InstrumentStatusMarker);
    book->generateOnQuote64 (pEfhRunCtx, s, seq, gr_ts, gapNum);

    //===================================================
  } else if (memcmp(msgHdr->MsgType,"Z ",sizeof(msgHdr->MsgType)) == 0) { // SystemTimeStamp
    *msgLen = sizeof(HsvfMsgHdr) + sizeof(HsvfSystemTimeStamp);
    char* timeStamp = ((HsvfSystemTimeStamp*)msgBody)->TimeStamp;
    uint64_t hour = getNumField<uint64_t>(&timeStamp[0],2);
    uint64_t min  = getNumField<uint64_t>(&timeStamp[2],2);
    uint64_t sec  = getNumField<uint64_t>(&timeStamp[4],2);
    uint64_t ms   = getNumField<uint64_t>(&timeStamp[6],3);
    
    gr_ts = ((hour * 3600 + min * 60 + sec) * 1000 + ms) * 1000000;
  } else {
    *msgLen = sizeof(HsvfMsgHdr);
  };
  //===================================================

  return false;
}

