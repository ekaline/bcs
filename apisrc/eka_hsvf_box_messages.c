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
#include "eka_dev.h"
#include "Efh.h"

/* ----------------------------------------------------------------------- */

inline uint32_t charSymbol2SecurityId(const char* charSymbol) {
  uint64_t hashRes = 0;
  uint shiftBits = 0;

  uint64_t fieldSize = 0;
  uint64_t fieldMask = 0;

  // 5 letters * 5 bits = 25 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  for (int i = 0; i < 5; i++) {
    uint64_t nameLetter = (charSymbol[i] - 'A') & fieldMask;
    hashRes |=  nameLetter << shiftBits;
    shiftBits += fieldSize;
  } 
  // shiftBits = 25;

  // 5 bits
  fieldSize = 5;
  fieldMask = (0x1 << fieldSize) - 1;
  char month = (charSymbol[6] - 'A') & fieldMask;
  hashRes |= month << shiftBits;
  shiftBits += fieldSize;
  // shiftBits = 30;

  // 9,999,999 = 24 bits
  fieldSize = 24;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string priceStr = std::string(&charSymbol[8],7);
  uint64_t price = std::stoul(priceStr,nullptr,10) & fieldMask;
  hashRes |= price << shiftBits;
  shiftBits += fieldSize;
  // shiftBits = 54;

  // 'A' .. 'G' = 3 bits
  fieldSize = 3;
  fieldMask = (0x1 << fieldSize) - 1;
  char priceFractionIndicator = (charSymbol[15] - 'A') & fieldMask;
  hashRes |= priceFractionIndicator << shiftBits;
  shiftBits += fieldSize;
  // shiftBits = 57;

  // year offset = 2 bits
  fieldSize = 2;
  fieldMask = (0x1 << fieldSize) - 1;
  std::string yearStr = std::string(&charSymbol[16],2);
  uint64_t year = (std::stoi(yearStr,nullptr,10) - 20) & fieldMask;
  hashRes |= year << shiftBits;
  shiftBits += fieldSize;
  // shiftBits = 59;

  // 5 bits
  std::string dayStr = std::string(&charSymbol[18],2);
  int day = std::stoi(dayStr,nullptr,10) & fieldMask;
  hashRes |= day << shiftBits;
  shiftBits += fieldSize;
  // shiftBits = 64;

  uint32_t hashPartA = (hashRes >> 0 ) & 0xFFFFF;
  uint32_t hashPartB = (hashRes >> 20) & 0xFFFFF;
  uint32_t hashPartC = (hashRes >> 40) & 0xFFFFF;
  uint32_t hashPartD = (hashRes >> 60) & 0xFFFFF;
 //    printf ("charSymbol = %s, price = %s  = %u, month = %c, hash = %x\n",charSymbol,priceStr.c_str(), price, month,hashVal);
  return hashPartA ^ hashPartB ^ hashPartC ^ hashPartD;
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

bool FhBoxGr::parseMsg(const EfhRunCtx* pEfhRunCtx,unsigned char* m,uint64_t sequence,EkaFhMode op) {
  uint pos = 0;
  if (m[pos] != HsvfSom) on_error("\'%c\' met while HsvfSom is expected",m[pos]);
  pos++;

  HsvfMsgHdr* msgHdr = (HsvfMsgHdr*)&m[pos];

  uint64_t seq = getNumField<uint64_t>(msgHdr->sequence,sizeof(msgHdr->sequence));

  uint8_t* msgBody = (uint8_t*)msgHdr + sizeof(HsvfMsgHdr);

  //===================================================
  if (memcmp(msgHdr->MsgType,"J ",sizeof(msgHdr->MsgType)) == 0) { // OptionInstrumentKeys
    HsvfOptionInstrumentKeys* boxMsg = (HsvfOptionInstrumentKeys*)msgBody;
    
    char* symb = boxMsg->InstrumentDescription;

    EfhDefinitionMsg msg = {};
    msg.header.msgType        = EfhMsgType::kDefinition;
    msg.header.group.source   = EkaSource::kBOX_HSVF;
    msg.header.group.localId  = id;
    msg.header.underlyingId   = 0;
    msg.header.securityId     = (uint64_t) charSymbol2SecurityId(symb);
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
    
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"F ",sizeof(msgHdr->MsgType)) == 0) { // OptionQuote
    
    //===================================================
  } else if (memcmp(msgHdr->MsgType,"Z ",sizeof(msgHdr->MsgType)) == 0) { // SystemTimeStamp
    
  }
  //===================================================

  return false;
}

