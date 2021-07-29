#ifndef _EKA_HSVF_BOX_MESSAGES_H
#define _EKA_HSVF_BOX_MESSAGES_H

#include "eka_macros.h"
#include "Efh.h"

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

  struct HsvfOptionTrade { // "C "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char Volume[8];
    char TradePrice[6];
    char TradePriceFractionIndicator;
    char NetChangeSign;
    char NetChange[6];
    char NetChangeFractionIndicator;
    char Filler6[6];
    char Timestamp[6];
    char OpenInterest[7];
    char Filler;
    char PriceIndicatorMarker;
    /* char       EoM; // = HsvfEom; */
  };

  
  struct HsvfRFQ { // "D "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char RequestedSize[8];
  };

  struct HsvfRfqStart { // "M "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char RfqId[6]; // Improvement Phase Sequential Number
                   // Indicates the number of an Improvement Phase.
                   // Sequential number unique per Instrument and per
                   // trading day
    char Price[6];
    char PriceFractionIndicator;
    char Size[8];
    char Side;     // Indicates the dealer side of the Initial Order
                   // 'B' = Buy
                   // 'S' = Sell
    char ExpiryTime[8];     // HHMMSSCC
    char ExpiryDuration[4]; //     SSCC
    char MinimumQuantity[8];
    char Percentage[8];// Ex: 00040.00 stands for 40.00 %
    char AuctionType;  // 'G' = Regular PIP,'B' = Solicitation, 'C' = Facilitation
    char Filler;
  };

  struct HsvfRfqInsert { // "O "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char OrderSide;    // The "must be filled" side ("B" for Buy, "S" for Sell)
    char OrderType;    // Type of limit entered: 'A'=Initial, 'P'=Exposed
    char LimitPrice[6];
    char LimitPriceFractionIndicator;
    char Size[8];
    char OrderSequence[6];

    char RfqId[6]; // Improvement Phase Sequential Number
                   // Indicates the number of an Improvement Phase.
                   // Sequential number unique per Instrument and per
                   // trading day
    char ClearingType; // Indicates the account type for which an order
                       // was entered using the clearing house member's
                       // account typology.
                       // When “Type of Order” is equal to “A”,
                       // the Account Type is for the InitO
                       // (Auction initiator or dealer side).
                       //    6 =Public Customer
                       //    7 = Broker Dealer
                       //    8 = Market Maker
                       //    T = Professional Customer
                       //    W = Broker Dealer cleared as Customer
                       //    X = Away Market Maker
    char Filler;
    char EndOfExposition[8]; // HHMMSSCC - '0' filled for PIP messages
    char AuctionType;  // 'G' = Regular PIP,'F' = Exposed Order
    char FirmId[4];
    char CMTA[4];
  };


  struct HsvfRfqDelete { // "T "
    /* char       SoM; //  = HsvfSom; */
    /* HsvfMsgHdr hdr; */
    char ExchangeID;
    char InstrumentDescription[20];
    char DeletionType; // '1' = Deletion of a precise order
                       // '2' = Deletion of all previous orders
                       //       in the specified side
                       // '3' = Deletion of all orders
    char OrderSequence[6];
    char OrderSide;    // The "must be filled" side ("B" for Buy, "S" for Sell)
    char RfqId[6]; // Improvement Phase Sequential Number
                   // Not relevant when the message refers to an Exposed Order
                   // Sequential number unique per Instrument and per
                   // trading day
    char AuctionType;  // 'G' = Regular PIP,
                       // 'B' = Solicitation
                       // 'C' = Facilitation
                       // 'F' = Exposed Order
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

  
  // template <class T> inline T getNumField(const char* f, size_t fSize) {
  //   // std::string fieldString = std::string(f,fSize);
  //   // return static_cast<T>(std::stoul(fieldString,nullptr,10));

  //   static const size_t strLen = 17;
  //   if (fSize > strLen - 1)
  //     on_error("fSize %ju > strLen %ju - 1",fSize,strLen);

  //   char str[strLen];
  //   memcpy(str,f,fSize);
  //   str[fSize] = '\0';
  //   return static_cast<T>(atol(str));    
  // }

  // template <class T>
  // inline T getNumField(const char* s, size_t fSize) {
  //   T acc = 0;
  //   int mult = 1;
  //   for (auto i = fSize - 1; i >= 0; i--) {
  //     if (s[i] == ' ') return acc;
  //     if (s[i] < '0' || s[i] > '9')
  // 	on_error("unexpected char \'%c\'",s[i]);

  //     acc += (s[i] - '0') * mult;
  //     mult *= 10;
  //   }
  //   return acc;
  // }
  template <class T>
  inline T getNumField(const char* s, size_t fSize) {
    T acc = 0;
    for (size_t i = 0; i < fSize; i++) {
      //if (s[i] == '.') continue; // To be checked
      if (s[i] < '0' || s[i] > '9') return acc;
      int digit = s[i] - '0';
      acc = (10 * acc) + digit;
    }   
    return acc;
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
    auto msgHdr {reinterpret_cast<const HsvfMsgHdr*>(&msg[1])};
    // std::string seqString = std::string(msgHdr->sequence,sizeof(msgHdr->sequence));
    // return std::stoul(seqString,nullptr,10);

    return getNumField<uint64_t>(msgHdr->sequence,sizeof(msgHdr->sequence));
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
    // std::string priceStr = std::string(&charSymbol[8],7);
    // uint64_t price = std::stoul(priceStr,nullptr,10) & fieldMask;
    uint64_t price = getNumField<uint64_t>(&charSymbol[8],7);
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
    // std::string yearStr = std::string(&charSymbol[16],2);
    // uint64_t year = (std::stoi(yearStr,nullptr,10) - 20) & fieldMask;
    //  uint64_t year = getNumField<uint64_t>(&charSymbol[16],2) & fieldMask;
    auto year {getYear(charSymbol) & fieldMask};
    f = year << shiftBits;
    hashRes |= f;
    // TEST_LOG("shiftBits = %2u, f = 0x%016jx, hashRes = 0x%016jx", shiftBits, f, hashRes);
    shiftBits += fieldSize;
    // shiftBits = 59;

    // 5 bits
    fieldSize = 5;
    fieldMask = (0x1 << fieldSize) - 1;
    // std::string dayStr = std::string(&charSymbol[18],2);
    // uint64_t day = std::stoi(dayStr,nullptr,10) & fieldMask;
    //    uint64_t day = getNumField<uint64_t>(&charSymbol[18],2) & fieldMask;
    uint64_t day {getDay(charSymbol) & fieldMask};

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

  inline auto getSide(const char hsvfSide) {
    switch (hsvfSide) {
    case 'B' : return EfhOrderSide::kBid;
    case 'S' : return EfhOrderSide::kAsk;
    case ' ' : return EfhOrderSide::kOther; // Both
    default  : 
      on_error("Unexpected hsvfSide \'%c\'",hsvfSide);
    }
  }

  inline auto getExpireNs(const char* t) {
    // HHMMSSCC
    int hour {(t[0] - '0') * 10  + (t[1] - '0')};
    int min  {(t[2] - '0') * 10  + (t[3] - '0')};
    int sec  {(t[4] - '0') * 10  + (t[4] - '0')};
    int ms   {(t[6] - '0') * 100 + (t[7] - '0') * 10};

    uint64_t ns = hour * 60 * 60 * 1e9 +
      min * 60 * 1e9 +
      sec * 1e9 +
      ms  * 1e6;

    return ns;
  }
} // namespace Hsvf

#endif
