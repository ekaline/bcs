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
#endif
