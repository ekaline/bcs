#ifndef _EKA_FH_XDP_MESSAGES_H
#define _EKA_FH_XDP_MESSAGES_H

#define EFH_XDP_STRIKE_PRICE_SCALE 1

#define XDP_TOP_FEED_BASE static_cast<uint8_t>(32)

enum class EKA_XDP_MSG_TYPE : uint16_t {

  //enum class EKA_XDP_TOP_MSG : uint16_t {
  QUOTE = 401,
    TRADE = 407,
    TRADE_CANCEL = 409,
    TRADE_CORRECTION = 411,
    IMBALANCE = 413,
    CROSSING_RFQ = 415,
    BOLD_RFQ = 471,
    SUMMARY = 417,
    UNDERLYING_STATUS = 419,
    SERIES_STATUS = 421,
    REFRESH_QUOTE = 501,
    REFRESH_TRADE = 507,
    REFRESH_IMBALANCE = 509,
    //    };


    //enum class EKA_XDP_COMPLEX_MSG : uint16_t {
    COMPLEX_QUOTE = 423,
    COMPLEX_TRADE = 425,
    COMPLEX_CROSSING_RFQ = 429,
    COMPLEX_STATUS = 433,
    COMPLEX_REFRESH_QUOTE = 511,
    COMPLEX_REFRESH_TRADE = 513,
    //    };

    //enum class EKA_XDP_INDEX_MAPPING_MSG : uint16_t {
    UNDERLYING_INDEX_MAPPING = 435,
    SERIES_INDEX_MAPPING = 437,
    COMPLEX_SYMBOL_DEFINITION = 439,
    //    };

    //enum class EKA_XDP_CONTROL_MSG : uint16_t {
    STREAM_ID = 455,
    LOGIN_REQUEST = 456,
    LOGIN_RESPONSE = 457,
    HEARTBEAT = 12,
    TEST_REQUEST = 458,
    TEST_RESPONSE = 459,
    UNDERLYING_INDEX_MAPPING_REQUEST = 441,
    SERIES_INDEX_MAPPING_REQUEST = 443,
    COMPLEX_SYMBOL_DEFINITION_REQUEST = 445,
    REQUEST_RESPONSE = 447,
    LOGOUT_REQUEST = 460,
    SEQUENCE_NUMBER_RESET = 1,
    };


enum class EKA_XDP_DEEP_MSG : uint16_t {
  BUY = 403,
    SELL = 405,
    UNDERLYING_STATUS = 419,
    SERIES_STATUS = 421,
    REFRESH_BUY = 503,
    REFRESH_SELL = 505
    };

enum class EKA_XDP_DELIVERY_FLAG : uint8_t {
  Heartbeat = 1,
    OriginaAndRefresh = 2,
    RefreshOnly = 3,
    XdpFailover = 10,
    OrignalOnly = 11,
    SequenceReset = 12
};

struct XdpStreamId;

struct XdpTimeHdr {
  uint32_t    SourceTime;        // 4 4 Binary The time when this data was generated in the order book, in seconds since Jan 1, 1970 00:00:00 UTC. 
  uint32_t    SourceTimeNS;      // 8 4 Binary The nanosecond offset from the SourceTime. 
} __attribute__((packed));

struct XdpPktHdr {
  uint16_t             PktSize;           // 0 2 Binary Size of the message
  uint8_t              DeliveryFlag;      // 2 1 Binary Type of message
  uint8_t              NumberMsgs;        // 3 1
  uint32_t             SeqNum;            // 4 4
  XdpTimeHdr           time;              // 8 8
} __attribute__((packed));

#define EKA_XDP_PKT_TYPE(x)      (((XdpPktHdr*)x)->DeliveryFlag)
#define EKA_XDP_MSG_CNT(x)       (((XdpPktHdr*)x)->NumberMsgs)
#define EKA_XDP_SEQUENCE(x)      (((XdpPktHdr*)x)->SeqNum)
#define EKA_XDP_STREAM_ID_HDR(x) ((XdpStreamId*)((uint8_t*)x+sizeof(XdpPktHdr)))
#define EKA_XDP_STREAM_ID(x)     ((EKA_XDP_STREAM_ID_HDR(x))->StreamID)

struct XdpMsgHdr {
  uint16_t            MsgSize;           // 0 2 Binary Size of the message
  EKA_XDP_MSG_TYPE    MsgType;           // 2 2 Binary Type of message
} __attribute__((packed));

#define EKA_XDP_MSG_LEN(x)       (((XdpMsgHdr*)x)->MsgSize)


/* OUTRIGHT QUOTE MESSAGE – MSG TYPE 401 */
/* REFRESH OUTRIGHT QUOTE MESSAGE – MSG TYPE 501 */

struct XdpQuote { // Valid for both REFRESH and OUTRIGHT QUOTE MESSAGES – MSG TYPE 401 and 501
  XdpMsgHdr   hdr;               // 4 bytes
  XdpTimeHdr  time;              // 8 bytes
  uint32_t    SeriesIndex;       // 12 4 Binary Numerical representation of the outright options symbol. 
  uint32_t    SymbolSeqNum;      // 16 4 Binary Sequence number of messages for the Outright options symbol. 
  int32_t     AskPrice;          // 20 4 Signed Binary Best Ask price. Should be used with the Price Scale Code from the symbol index mapping message. 
  int32_t     BidPrice;          // 24 4 Signed Binary Best Bid price. Should be used with the Price Scale Code from the symbol index mapping message 
  uint16_t    AskVolume;         // 28 2 Binary Total quantity available at the Ask price. 
  uint16_t    BidVolume;         // 30 2 Binary Total quantity available at the Bid price. 
  uint16_t    AskCustomerVolume; // 32 2 Binary *Total quantity of customer orders available at the Ask price. 
  uint16_t    BidCustomerVolume; // 34 2 Binary *Total quantity of customer orders available at the Bid price. 
  char        QuoteCondition;    // 36 1 ASCII  1 (Regular Trading)  2 (Rotation)  3 (Trading Halted)  4 (Pre-open) 
  uint8_t     Reserved1;         // 37 1 Binary Filler 
  uint16_t    Reserved;          // 38 2 Binary Filler
} __attribute__((packed));

struct XdpTrade { // 
  XdpMsgHdr   hdr;               // 4 bytes
  XdpTimeHdr  time;              // 8 bytes
  uint32_t    SeriesIndex;       // 12 4 Binary Numerical representation of the outright options symbol. 
  uint32_t    SymbolSeqNum;      // 16 4 Binary Sequence number of messages for the Outright options symbol. 
  uint32_t    TradeID;           // 20 4 Binary Unique Trade execution ID. 
  uint32_t    Price;             // 24 4 Signed Binary Price of the trade. Use the Price Scale Code from the symbol index mapping message.symbol index mapping message 
  uint32_t    Volume;            // 28 4 Binary Volume of the trade in number of contracts 
  char        TradeCond1;        // 32 1 ASCII  Blank = regular trade  I = Late report  R = Floor trade  S = SO sweep trade 
  char        TradeCond2;        // 33 1 ASCII Complex indicator:  P = Complex trade with equity trade  L = Complex trade 
  uint16_t    Reserved;          // 34 2 Binary Filler
} __attribute__((packed));


struct XdpUnderlyingStatus { // MSG TYPE 419
  XdpMsgHdr   hdr;               // 4 bytes
  XdpTimeHdr  time;              // 8 bytes
  uint32_t    UnderlyingIndex;   // 12 4 Binary The unique ID of the symbol in the Series Index message 
  uint32_t    UnderlyingSeqNum;  // 16 4 Binary Sequence number of messages for the underlying symbol. 
  char        SecurityStatus;    // 20 1 ASCII  S Halt  U Unhalt  O Open indication  X Close indication 
  char        HaltCondition;     // 21 1 ASCII Not applicable 
  uint16_t    Reserved;          // 22 2 Binary Filler
} __attribute__((packed));

struct XdpSeriesStatus { // MSG TYPE 421
  XdpMsgHdr   hdr;               // 4 bytes
  XdpTimeHdr  time;              // 8 bytes
  uint32_t    SeriesIndex;       // 12 4 Binary The unique ID of the symbol in the Series Index message 
  uint32_t    SymbolSeqNum;      // 16 4 Binary Sequence number of messages for the underlying symbol. 
  char        SecurityStatus;    // 20 1 ASCII  S Halt  U Unhalt  O Open indication  X Close indication 
  char        HaltCondition;     // 21 1 ASCII Not applicable 
  uint16_t    Reserved;          // 22 2 Binary Filler
} __attribute__((packed));


struct XdpIndexMapping { // MSG TYPE 435
  XdpMsgHdr   hdr;                  // 4 bytes
  uint32_t    UnderlyingIndex;      //  4 4 Binary The unique ID of this underlying symbol in this market. 
                                    // This ID cannot be used to cross reference a security between markets. 
  char        UnderlyingSymbol[11]; //  8 11 ASCII Full symbol in NYSE Symbology. 
  uint8_t     ChannelID;            //  19 1 Binary Multicast channel ID of the symbols being provided. 
  uint16_t    MarketID;             //  20 2 Binary Identifies originating market: 
                                    //   1 (NYSE Cash) 
                                    //   2 (Europe Cash) 
                                    //   3 (NYSE Arca Cash) 
                                    //   4 (NYSE/Arca Options) 
                                    //   5 (NYSE/Arca Bonds) 
                                    //   6 (ArcaEdge) 
                                    //   7 (LIFFE) 
                                    //   8 (NYSE Amex Options) 
                                    //   9 (NYSE MKT Cash) 
  uint8_t     SystemID;             //  22 1 Binary ID of the Originating System 
  uint8_t     ExchangeCode;         //  23 1 ASCII Exchanges where it is listed:  N (NYSE)  P (NYSE Arca)  Q (NASDAQ)  A (NYSE MKT) 
  uint8_t     PriceScaleCode;       //  24 1 Binary Price Scale Code for price conversion of the symbol. See Price Formats. 
  char        SecurityType;         //  25 1 ASCII Type of Security: 
                                    //   A (ADR) 
                                    //   C (COMMON STOCK) 
                                    //   D (DEBENTURES) 
                                    //   E (ETF)
                                    //   F (FOREIGN) 
                                    //   I (UNITS) 
                                    //   M (MISC/LIQUID TRUST) 
                                    //   P (PREFERRED STOCK) 
                                    //   R (RIGHTS) 
                                    //   S (SHARES OF BENEFICIARY INTEREST) 
                                    //   T (TEST) 
                                    //   U (UNITS) 
                                    //   W (WARRANT) 
  uint8_t     PriceResolution;      //  26 1 Binary  0 (All Penny)  1 (Penny/Nickel)  5 (Nickel/Dime) 
  uint8_t     Reserved;             //  27 1 Binary Filler
} __attribute__((packed));

struct XdpSeriesMapping { // MSG TYPE 437
  XdpMsgHdr   hdr;                  // 4 bytes
  uint32_t    SeriesIndex;          // 4 4 Binary The unique ID of the symbol in the Series Index message 
  uint8_t     ChannelID;            // 8 1 Binary Multicast channel ID of the symbols being provided. 
  uint8_t     Reserved1;            // 9 1 Binary Filler
  uint16_t    MarketID;             // 10 2 Binary Identifies originating market: 
                                    //   1 (NYSE Cash) 
                                    //   2 (Europe Cash) 
                                    //   3 (NYSE Arca Cash) 
                                    //   4 (NYSE/Arca Options) 
                                    //   5 (NYSE/Arca Bonds) 
                                    //   6 (ArcaEdge) 
                                    //   7 (LIFFE) 
                                    //   8 (NYSE Amex Options) 
                                    //   9 (NYSE MKT Cash) 
  uint8_t     SystemID;             //  12 1 Binary ID of the Originating System 
  uint8_t     Reserved2;            // 13 1 Binary Filler
  uint16_t    StreamID;             // 14 2 Binary Identifies Stream on which this symbol will be updated 
  uint32_t    UnderlyingIndex;      // 16 4 Binary Underlying Stock Mapping Index 
  uint16_t    ContractMultiplier;   // 20 2 Binary Contract quantity 
  char        MaturityDate[6];      //  22 6 ASCII YY MM DD 
  uint8_t     PutOrCall;            //  28 1 Binary  0 (Put)  1 (Call) 
  char        StrikePrice[10];      //  29 10 ASCII Strike price. ASCII 0-9 with optional decimal point. EG: 51.75, 123 
  uint8_t     PriceScaleCode;       //  39 1 Binary Decimal places on price
  char        UnderlyingSymbol[11]; //  40 11 ASCII Full underlying symbol in NYSE Symbology 
  char        OptionSymbolRoot[5];  //  51 5 ASCII OCC root of option symbol 
  uint32_t    GroupID;              //  56 4 Binary Used by Market Makers. Predefined group of series within a given underlying symbol.
} __attribute__((packed));

struct XdpDefAttrA {
  uint16_t    StreamID;
  uint8_t     ChannelID;
  uint8_t     PriceScaleCode;
  uint32_t    GroupID;
};

union XdpAuxAttrA {
  uint64_t opaqueField;
  XdpDefAttrA attr;
};

struct XdpDefAttrB {
  uint32_t    UnderlIdx;
  uint32_t    AbcGroupID;
};

union XdpAuxAttrB {
  uint64_t opaqueField;
  XdpDefAttrB attr;
};

struct XdpStreamId { // MSG TYPE 455
  XdpMsgHdr   hdr;                  // 4 bytes
  uint16_t    StreamID;             // 4 2 Binary Identifies Stream on which this symbol will be updated 
  uint16_t    Reserved;             // 6 2 Binary Filler
} __attribute__((packed));

struct XdpLoginRequest { // MSG TYPE 456
  XdpMsgHdr   hdr;                  // 4 bytes
  char        SourceID[10];         // 4 10 Name of the source requesting login. This field is up to 9 characters, null terminated
  char        Password[12];         // 14 12 Password of the requestor
  uint16_t    Reserved;             // 26 2 Binary Filler
} __attribute__((packed));

struct XdpLoginResponse { // MSG TYPE 457
  XdpMsgHdr   hdr;                  // 4 bytes
  char        ResponseCode;         //  A (Accepted)  B (Rejected)
  char        RejectCode;           //  blank (Accepted)  A (Not authorized)  M (Maximum server connections reached)  T (Timeout)
  uint16_t    Reserved;             // 6 2 Binary Filler
} __attribute__((packed));


struct XdpHeartbeat { // MSG TYPE 12
  XdpMsgHdr   hdr;                  // 4 bytes
  char        SourceID[10];         // 4 10 Name of the source requesting login. This field is up to 9 characters, null terminated
  uint16_t    Reserved;             // 26 2 Binary Filler
} __attribute__((packed));

struct XdpUnderlyingIndexMappingRequest { // MSG TYPE 441
  XdpMsgHdr   hdr;   
  uint32_t    UnderlyingIndex;      // 4 4 Binary The ID from the Underlying Index message of the requested symbol. 
                                    // To request a refresh for all symbols in the channel, set this field to 0. 
  char        SourceID[10];         // 8 10 ASCII Name of the requestor. 
  uint8_t     ChannelID;            // 18 1 Binary Multicast channel ID of the symbol(s) being requested. 
                                    // To request all underlying symbols in all channels, set this field to 0. 
  uint16_t    Reserved;             // 19 1 Binary Filler
} __attribute__((packed));

struct XdpSeriesIndexMappingRequest { // MSG TYPE 443
  XdpMsgHdr   hdr;   
  uint32_t    SeriesIndex;          // 4 4 Binary The ID from the Series Index message of the requested symbol. 
                                    // To request a refresh for all symbols in the channel, set this field to 0. 
  char        SourceID[10];         // 8 10 ASCII Name of the requestor. 
  uint8_t     ChannelID;            // 18 1 Binary Multicast channel ID of the symbol(s) being requested. 
                                    // To request all underlying symbols in all channels, set this field to 0. 
  uint16_t    Reserved;             // 19 1 Binary Filler
} __attribute__((packed));

struct XdpRequestResponse { // MSG TYPE 447
  XdpMsgHdr   hdr;   
  char        SourceID[10];         // 4 10 ASCII Name of the requestor. 
  uint8_t     ChannelID;            // 14 1 Binary Multicast channel ID of the symbol(s) being requested. 
                                    // To request all underlying symbols in all channels, set this field to 0. 
  uint8_t     Status;               // 15 1 Binary Outcome of the request. Valid values: 
                                    //  0 – Request was accepted 
                                    //  1 – Rejected due to an Invalid Source ID 
                                    //  4 – Rejected due to maximum number of refresh requests in a day 
                                    //  6 – Rejected due to an Invalid Channel ID 
                                    //  8 – Underlying download complete 
                                    //  9 – Series download complete 
                                    //  10 – Complex download complete
} __attribute__((packed));

struct XdpLogoutRequest { // MSG TYPE 460
  XdpMsgHdr   hdr; 
} __attribute__((packed));

struct XdpSequenceNumberReset { // MSG TYPE 1
  XdpMsgHdr   hdr;               // 4 bytes
  XdpTimeHdr  time;              // 8 bytes
  uint8_t     ProductID;         // 12 1 Binary This field is unused for Options. Ignore any content.
  uint8_t     ChannelID;         // 13 1 Binary Multicast channel ID of the symbol(s) being requested. 
  uint16_t    Reserved;          // 14 1 Binary Filler
} __attribute__((packed));

#endif
