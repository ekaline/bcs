#ifndef _EKA_HSVF_BOX_MESSAGES_H
#define _EKA_HSVF_BOX_MESSAGES_H

#define EFH_HSV_BOX_STRIKE_PRICE_SCALE 1

typedef char EKA_HSVF_MSG_TYPE[2];

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
  char                sequence[9];
  EKA_HSVF_MSG_TYPE   MsgType; 
} __attribute__((packed));

const char HsvfSom = 0x2;
const char HsvfEom = 0x3;


struct OptionSummary { // "N "
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
};

struct OptionQuote { // "F "
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
};
#endif
