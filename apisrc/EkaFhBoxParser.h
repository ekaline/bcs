#ifndef _EKA_HSVF_BOX_MESSAGES_H
#define _EKA_HSVF_BOX_MESSAGES_H

#define EFH_HSV_BOX_STRIKE_PRICE_SCALE 1

typedef char EKA_HSVF_MSG_TYPE[2];

namespace Hsvf {

  class EkaHsvfMsgType {
  public:
    char val[2];
  };

  /* enum class EKA_HSVF_MSG : EKA_HSVF_MSG_TYPE { */
  /*   BeginningOfOptionsSummary = "Q ", */
  /*     GroupOpeningTime = "GC", */
  /*     GroupStatus = "GR", */
  /*     OptionQuote = "F ", */
  /*     OptionMarketDepth = "H ", */
  /*     OptionSummary = "N ", */
  /*     }; */

  inline bool operator==(const EkaHsvfMsgType &lhs, const EkaHsvfMsgType &rhs) {
    return lhs.val[0] == rhs.val[0] && lhs.val[1] == rhs.val[1];
  }

  inline bool operator==(const EkaHsvfMsgType &lhs, const char* rhs) {
    return lhs.val[0] == rhs[0] && lhs.val[1] == rhs[1];
  }

  struct HsvfMsgHdr {
    char        sequence[9];
    char        MsgType[2]; 
  } __attribute__((packed));

  const char HsvfSom = 0x2;
  const char HsvfEom = 0x3;

  struct HsvfLogin { // "LI"
    char       SoM; //  = HsvfSom;
    HsvfMsgHdr hdr;
    char       User[16];
    char       Pwd[16];
    char       TimeStamp[6]; // Format HHMMSS
    char       ProtocolVersion[2]; // HSVF Protocol version (C7)
    char       EoM; // = HsvfEom;
  };

  struct HsvfLoginAck { // "KI"
    char       SoM; //  = HsvfSom;
    HsvfMsgHdr hdr;
    char       EoM; // = HsvfEom;
  };


  struct HsvfError { // "ER"
    char       SoM; //  = HsvfSom;
    HsvfMsgHdr hdr;
    char       ErrorCode[4];
    char       ErrorMsg[80];
    char       EoM; // = HsvfEom;
  };

  struct HsvfRetransmissionRequest { // "RT"
    char       SoM; //  = HsvfSom;
    HsvfMsgHdr hdr;
    char       Line[2];
    char       Start[9];
    char       End[9];
    char       EoM; // = HsvfEom;
  };

  struct HsvfRetransmissionEnd { // "RE"
    char       SoM; //  = HsvfSom;
    HsvfMsgHdr hdr;
    char       EoM; // = HsvfEom;
  };



  struct HsvfEndOfTransmission { // "U "
    char       SoM; //  = HsvfSom;
    HsvfMsgHdr hdr;
    char       ExchangeID; // Q by default
    char       Time[6];
    char       EoM; // = HsvfEom;
  };

  struct HsvfCircuitAssurance { // "V "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char       Time[6];
    /* char       EoM; // = HsvfEom; */
  };

  struct HsvfOptionInstrumentKeys { // "J "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char StrikePriceCurrency[3];
    char MaximumNumberOfContractsPerOrder[6];
    char MinimumNumberOfContractsPerOrder[6];
    char MaximumThresholdPrice[6];
    char MaximumThresholdPriceFractionIndicator;
    char MinimumThresholdPrice[6];
    char MinimumThresholdPriceFractionIndicator;
    char TickIncrement[6];
    char TickIncrementFractionIndicator;
    char OptionType;
    char MarketFlowIndicator[2];
    char GroupInstrument[2];
    char Instrument[4];
    char InstrumentExternalCode[30];
    char OptionMarker[2];
    char UnderlyingSymbolRoot[10];
    /* char       EoM; // = HsvfEom; */
  };

  struct HsvfOptionSummary { // "N "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char BidPrice[6];
    char BidPriceFractionIndicator;
    char BidSize[5];
    char AskPrice[6];
    char AskPriceFractionIndicator;
    char AskSize[5];
    char LastPrice[6];
    char LastPriceFractionIndicator;
    char OpenInterest[7];
    char Tick;
    char Volume[8];
    char NetChangeSign;
    char NetChange[6];
    char NetChangeFractionIndicator;
    char OpenPrice[6];
    char OpenPriceFractionIndicator;
    char HighPrice[6];
    char HighPriceFractionIndicator;
    char LowPrice[6];
    char LowPriceFractionIndicator;
    char OptionMarker[2];
    char UnderlyingSymbol[10];
    char ReferencePrice[6];
    char ReferencePriceFractionIndicator;
    /* char       EoM; // = HsvfEom; */
  };

  struct HsvfOptionQuote { // "F "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char BidPrice[6];
    char BidPriceFractionIndicator;
    char BidSize[5];
    char AskPrice[6];
    char AskPriceFractionIndicator;
    char AskSize[5];
    char Filler;
    char InstrumentStatusMarker;
    char PublicCustomerBidSize[5];
    char PublicCustomerAskSize[5];
    /* char       EoM; // = HsvfEom; */
  };


  struct HsvfSystemTimeStamp { // "Z "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char TimeStamp[9];
    /* char       EoM; // = HsvfEom; */
  };

  inline uint hsvfTableLen(const uint8_t* msg) {
    if (memcmp(msg,"F ",2) == 0) return 68;
    if (memcmp(msg,"V ",2) == 0) return 17;
    if (memcmp(msg,"Z ",2) == 0) return 20;
    if (memcmp(msg,"C ",2) == 0) return 76;
    if (memcmp(msg,"CS",2) == 0) return 79;
    if (memcmp(msg,"D ",2) == 0) return 40;
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

    return 0;
  }

  inline uint getMsgLen(const uint8_t* pkt, int bytes2run) {
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

  inline uint64_t getMsgSequence(const uint8_t* msg) {
    const HsvfMsgHdr* msgHdr = (const HsvfMsgHdr*)&msg[1];
    std::string seqString = std::string(msgHdr->sequence,sizeof(msgHdr->sequence));
    return std::stoul(seqString,nullptr,10);
  }

  inline bool isHeartbeat(const uint8_t* msg) {
    auto msgHdr {reinterpret_cast<const HsvfMsgHdr*>(&msg[1])};
    if (memcmp(msgHdr->MsgType,"V ",sizeof(msgHdr->MsgType)) == 0) return true;
    return false;
  }

  inline uint trailingZeros(const uint8_t* p, uint maxChars) {
    uint idx = 0;
    while (p[idx] == 0x0 && idx < maxChars) {
      idx++; // skipping trailing '\0' chars
    }
    if (idx > 2) {
      hexDump("Msg with unexpected trailing zeros",p,maxChars);
      on_error("unexpected %u trailing Zeros",idx);
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

  template <class T> inline T getNumField(const char* f, size_t fSize) {
    std::string fieldString = std::string(f,fSize);
    return static_cast<T>(std::stoul(fieldString,nullptr,10));
  }

  inline EfhOptionType getOptionType(const char* c) {
    if (c[6] >= 'A' && c[6] <= 'L') return EfhOptionType::kCall;
    if (c[6] >= 'M' && c[6] <= 'X') return EfhOptionType::kPut;
    on_error("Unexpected Month Code \'%c\'",c[6]);
  }

  inline uint64_t getMonth(const char* c, EfhOptionType optType) {
    if (optType == EfhOptionType::kCall) return c[6] - 'A' + 1;
    return c[6] - 'M' + 1;
  }

  inline uint64_t getYear(const char* c) {
    return getNumField<uint64_t>(c + 16,2);
  }

  inline uint64_t getDay(const char* c) {
    return getNumField<uint64_t>(c + 18,2);
  }

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
    
}

#endif
