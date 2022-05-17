#ifndef _EKA_FH_BX_PARSER_H_
#define _EKA_FH_BX_PARSER_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "eka_macros.h"

namespace Bx {
  struct GenericHdr { // Dummy
    char	  type;        // 1
    uint16_t      trackingNum; // 2 Integer Internal system tracking number
    uint64_t      ts;          // 8 Nanoseconds since midnight
  } __attribute__((packed));

  struct SystemEvent { // 'S'
    GenericHdr hdr;        // 11
    char     event;        // 1
                           //  “O” Start of Messages.
                           //      This is always the first message sent in any trading day.
                           //      After ~2:00am
                           //  “S” Start of System Hours.
                           //      This message indicates that the exchange is open and
                           //      ready to start accepting orders. 7:00am
                           //  “Q” Start of Opening Process. This message is intended
                           //      to indicate that the exchange has started its
                           //      opening auction process. 9:30:00am
                           //  “N” End of Normal Hours Processing -
                           //      This message is intended to indicate that the exchange
                           //      will no longer accept any new orders or changes to
                           //      existing orders for options that trade during
                           //      normal trading hours. 4:00:00pm
                           //  “L” End of Late Hours Processing - This message is intended
                           //      to indicate that the exchange will no longer accept any
                           //      new orders or changes to existing orders for options
                           //      that trade during extended hours. 4:15:00pm
                           //  “E” End of System Hours. This message indicates that the
                           //      exchange system is now closed. ~5:15pm
                           //  “C” End of Messages. This is always the last message sent
                           //      in any trading day. ~5:20pm

  } __attribute__((packed));
  
  struct TradingAction { // 'H'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    char     state;        // 1
                           //  "H” = Halt in effect
                           //  “B” = Buy Side Trading Suspended i.e. Buy orders are not executable)
                           //  ”S” = Sell Side Trading Suspended i.e. Sell orders are not executable)
                           //  ”I” = Pre Open
                           //  ”O” = Opening Auction
                           //  ”R” = Re-Opening
                           //  ”T” = Continuous Trading
                           //  ”X” = Closed
  } __attribute__((packed));
  
  struct AddOrderShort { // 'a'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t orderId;      // 8
    char     side;         // 1 “B” = Buy “S” = Sell “X” = Buy AON “Y” = Sell AON
    char     capacity;     // 1 Indicates Order Capacity.
                           //   Not supported on BX Options. Always set to ‘0’
    uint16_t price;        // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t volume;       // 2
    uint16_t rank;         // 2 Not supported on BX Options. Always set to ‘0’
  } __attribute__((packed));
  
  struct AddOrderLong { // 'A'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t orderId;      // 8
    char     side;         // 1 “B” = Buy “S” = Sell “X” = Buy AON “Y” = Sell AON
    char     capacity;     // 1 Indicates Order Capacity.
                           //   Not supported on BX Options. Always set to ‘0’
    uint32_t price;        // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t volume;       // 4
    uint16_t rank;         // 2 Not supported on BX Options. Always set to ‘0’
  } __attribute__((packed));

  struct AddQuoteShort { // 'j'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t bidOrderId;   // 8
    uint64_t askOrderId;   // 8
    uint16_t bidPrice;     // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t bidSize;      // 2
    uint16_t askPrice;     // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t askSize;      // 2
  } __attribute__((packed));

  struct AddQuoteLong { // 'J'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t bidOrderId;   // 8
    uint64_t askOrderId;   // 8
    uint32_t bidPrice;     // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t bidSize;      // 4
    uint32_t askPrice;     // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t askSize;      // 4
  } __attribute__((packed));

  struct OrderExecuted { // 'E'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint32_t strategyId;   // 4 Not supported on BX Options. Always set to ‘0’
    uint64_t orderId;      // 8
    uint32_t volume;       // 4 The total quantity executed
    uint32_t crossNumber;  // 4 Trade Group Id. Ties together all trades of
                           //   a given atomic transaction in the matching engine.
    uint32_t matchNumber;  // 4 Execution Id. Identifies the component of an execution.
                           //   Unique for a given day. The match number is also
                           //   referenced in the Trade Break Message.
  } __attribute__((packed));

  struct OrderExecutedPrice { // 'C'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint32_t strategyId;   // 4 Not supported on BX Options. Always set to ‘0’
    uint64_t orderId;      // 8
    uint32_t crossNumber;  // 4 Trade Group Id. Ties together all trades of
                           //   a given atomic transaction in the matching engine.
    uint32_t matchNumber;  // 4 Execution Id. Identifies the component of an execution.
                           //   Unique for a given day. The match number is also
                           //   referenced in the Trade Break Message.
    char     printable;    // 1 “N” = non-printable “Y” = printable
    uint32_t price;        // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t volume;       // 4 The total quantity executed

  } __attribute__((packed));

  struct OrderCancel { // 'X'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t orderId;      // 8
    uint32_t volume;       // 4 The total quantity executed
  } __attribute__((packed));

  struct ReplaceOrderShort { // 'u'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t oldOrderId;   // 8
    uint64_t newOrderId;   // 8
    uint16_t price;        // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t volume;       // 2
  } __attribute__((packed));

  struct ReplaceOrderLong { // 'U'
    GenericHdr hdr;
    uint32_t instrumentId; // 4
    uint64_t oldOrderId;   // 8
    uint64_t newOrderId;   // 8
    uint32_t price;        // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t volume;       // 4
  } __attribute__((packed));

  struct SingleSideDelete{ // 'D'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t orderId;      // 8
  } __attribute__((packed));

  struct SingleSideUpdate { // 'G'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t orderId;      // 8
    char     reason;       // 1 ‘U’-USER, ‘R’-REPRICE, ‘S’-SUSPEND
    uint32_t price;        // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t volume;       // 4
  } __attribute__((packed));

  struct QuoteReplaceShort { // 'k'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t oldBidOrderId;// 8
    uint64_t newBidOrderId;// 8
    uint64_t oldAskOrderId;// 8
    uint64_t newAskOrderId;// 8
    uint16_t bidPrice;     // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t bidSize;      // 2
    uint16_t askPrice;     // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t askSize;      // 2
  } __attribute__((packed));

  struct QuoteReplaceLong { // 'K'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t oldBidOrderId;// 8
    uint64_t newBidOrderId;// 8
    uint64_t oldAskOrderId;// 8
    uint64_t newAskOrderId;// 8
    uint32_t bidPrice;     // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t bidSize;      // 4
    uint32_t askPrice;     // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t askSize;      // 4
  } __attribute__((packed));

  struct QuoteDelete { // 'Y'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint64_t bidOrderId;   // 8
    uint64_t askOrderId;   // 8
  } __attribute__((packed));
  
  struct Trade { // 'Q'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint32_t crossNumber;  // 4 Trade Group Id. Ties together all trades of
                           //   a given atomic transaction in the matching engine.
    uint32_t matchNumber;  // 4 Execution Id. Identifies the component of an execution.
                           //   Unique for a given day. The match number is also
                           //   referenced in the Trade Break Message.

    uint32_t strategyId;   // 4 Not supported on BX Options. Always set to ‘0’
    char     crossType;    // 1 “A”= All Auctions “P”= Price Improvement (PRISM) Auction “N”=None
    uint32_t price;        // 4 
    uint32_t volume;       // 4 The total quantity executed
    char     printable;    // 1 “N” = non-printable “Y” = printable
    char     tradeType;    // 1 “E” = Electronic Trade “M” = Manual Trade
  } __attribute__((packed));

  struct BrokenTrade { // 'B'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint32_t crossNumber;  // 4 Trade Group Id. Ties together all trades of
                           //   a given atomic transaction in the matching engine.
    uint32_t matchNumber;  // 4 Execution Id. Identifies the component of an execution.
                           //   Unique for a given day. The match number is also
                           //   referenced in the Trade Break Message.

  } __attribute__((packed));

  struct NOII { // 'I'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    uint32_t auctionId;    // 4 
    char     auctionType;  // 1 “O” = Opening
                           //   “R” = Reopening
                           //   “P” = Price Improvement (PRISM) Auction
                           //   “I” = Order Exposure
                           //   “B” = Block Auction
    uint32_t pairedQuantity;
    // 4 The total amount that are eligible to be matched
    //   at the Current Reference Price.
    //   0 for Price Improvement (PRISM) Auction.
    char     imbalanceDirection;
    // 1 Indicates the market side of the imbalance
    //   “B” = buy imbalance “S” = sell imbalance
    uint32_t imbalancePrice;
    // 4 The imbalance price. 0 for Price Improvement (PRISM) Auction.
    uint32_t imbalanceVolume;
    // 4
    char     customerFirmIndicator;
    // 1 For Order Exposure (Auction Type=I) only.
    //   Indicates the order capacity:
    //   “C” = Customer
    //   “F” = Firm/ Joint Back Office (JBO)
    //   “M” = On-floor Market Maker
    //   “P” = Professional Customer
    //   “B” = Broker Dealer/ Non Registered Market Maker
    uint32_t bestBidPrice; // 4 Best Bid Price or zero (0) if the book is crossed.
                           //   Not supported on BX Options. Always set to ‘0’
    uint32_t bestBidQuantity;
    // 4 Bid volume at top of book, or zero (0) if the book is crossed.
    //   Not supported on BX Options. Always set to ‘0’
    uint32_t bestAskPrice; // 4 Best Ask Price or zero (0) if the book is crossed.
                           //   Not supported on BX Options. Always set to ‘0’
    uint32_t bestAskQuantity;
    // 4 Ask volume at top of book, or zero (0) if the book is crossed.
    //   Not supported on BX Options. Always set to ‘0’
  } __attribute__((packed));

  struct EndOfSnapshot { // 'M'
    char          type;             // 1
    char          nextLifeSequence[20];    
  } __attribute__((packed));


  struct Directory { // 'R'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    char     symbol[6];    // 6
    uint8_t  expYear;      // 1
    uint8_t  expMonth;     // 1
    uint8_t  expDate;      // 1
    uint32_t price;        // 4 Explicit strike price
    char     optionType;   // 1 “C” = Call option “P” = Put option “N”= N/A
    char     underlying[13];//13
    char     closingType;  // 1 “N” = Normal Hours “L” = Late Hours
    char     tradable;     // 1 “Y” = Instrument is tradable “N” = Instrument is not tradable
    char     mpv;          // 1 “E” = penny Everywhere “S” = Scaled “P” = penny Pilot
    char     isin[12];     // 12 Not supported on BX Options. Always set to ‘0’
    uint16_t tickSizeTable;// 2 Not supported on BX Options. Always set to ‘0’
    char     priceNotation;// 1  Not supported on BX Options. Always set to ‘0’
    char     volumeNotation;// 1  Not supported on BX Options. Always set to ‘0’
    uint16_t financialProd;// 2 Not supported on BX Options. Always set to ‘0’
    char     marketSegment;// 1  Not supported on BX Options. Always set to ‘0’
    char     currency[3];  // 3  Not supported on BX Options. Always set to ‘0’
    char     mic[4];       // 4  Not supported on BX Options. Always set to ‘0’
    char     longName[16]; // 16  Not supported on BX Options. Always set to ‘0’    
  } __attribute__((packed));

  template <class T> inline uint32_t getInstrumentId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->instrumentId);
  }

  template <class T> inline uint32_t getOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->orderId);
  }
  template <class T> inline uint32_t getBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->bidOrderId);
  }
  template <class T> inline uint32_t getAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->askOrderId);
  }
  template <class T> inline uint32_t getOldOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->oldOrderId);
  }
  template <class T> inline uint32_t getNewOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->newOrderId);
  }
  template <class T> inline uint32_t getOldBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->oldBidOrderId);
  }
  template <class T> inline uint32_t getOldAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->oldAskOrderId);
  }
  template <class T> inline uint32_t getNewBidOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->newBidOrderId);
  }
  template <class T> inline uint32_t getNewAskOrderId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->newAskOrderId);
  }  
  inline uint32_t interpretVolume(uint32_t v) {
    return be32toh(v);
  }

  inline uint32_t interpretVolume(uint16_t v) {
    return be16toh(v);
  }
  
  template <class T> inline uint32_t getSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->volume);
  }
  template <class T> inline uint32_t getBidSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->bidSize);
  }
  template <class T> inline uint32_t getAskSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretVolume(msg->askSize);
  }
  
  inline uint32_t interpretPrice(uint32_t v) {
    return be32toh(v);
  }

  inline uint32_t interpretPrice(uint16_t v) {
    return be16toh(v) * 100;
  }
  
  template <class T> inline uint32_t getPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice(msg->price);
  }
  template <class T> inline uint32_t getBidPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice(msg->bidPrice);
  }
  template <class T> inline uint32_t getAskPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return interpretPrice(msg->askPrice);
  } 
  template <class T> inline SideT getSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->side) {
    case 'B' :
    case 'X' :
      return SideT::BID;
    case 'S' :
    case 'Y' :
      return SideT::ASK;
    default :
      on_error("Unexpected side \'%c\'",msg->side);
    }
  }

  template <class T>
  inline FhOrderType getOrderType(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->side) {
    case 'B' :
    case 'S' :
      return FhOrderType::CUSTOMER;
    case 'X' :
    case 'Y' :
      return FhOrderType::CUSTOMER_AON;
    default :
      on_error("Unexpected side \'%c\'",msg->side);
    }
  }

  template <class T>
  inline EfhAuctionUpdateType getAuctionUpdateType(const char* m) {
    switch (reinterpret_cast<const T*>(m)->auctionType) {
    case 'I' : // “I” = Order Exposure
      return EfhAuctionUpdateType::kNew; 
    case 'O' : // “O” = Opening
    case 'R' : // “R” = Reopening
    case 'P' : // “P” = Price Improvement (PRISM) Auction
    case 'B' : // “B” = Block Auction
      return EfhAuctionUpdateType::kUnknown;
    default :
      on_error("Unexpected auctionType \'%c\'",
	       reinterpret_cast<const T*>(m)->auctionType);
    }
  }

  template <class T>
  inline uint32_t getAuctionId(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->auctionId);
  }

  template <class T>
  inline SideT getAuctionSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->imbalanceDirection) {
    case 'B' :
      return SideT::BID;
    case 'S' :
      return SideT::ASK;
    default :
      on_error("Unexpected side \'%c\'",msg->side);
    }
  }

  template <class T> inline EfhOrderCapacity getAuctionOrderCapacity(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    switch (msg->customerFirmIndicator) {
    case 'C' : // “C” = Customer
      return EfhOrderCapacity::kCustomer;
    case 'F' : // “F” = Firm/ Joint Back Office (JBO)
      return EfhOrderCapacity::kAgency;
    case 'M' : // “M” = On-floor Market Maker
      return EfhOrderCapacity::kMarketMaker;
    case 'P' : // “P” = Professional Customer
      return EfhOrderCapacity::kProfessionalCustomer;
    case 'B' : // “B” = Broker Dealer/ Non Registered Market Maker
      return EfhOrderCapacity::kBrokerDealer;
    default :
      on_error("Unexpected customerFirmIndicator \'%c\'",msg->customerFirmIndicator);
    }
  }

 
  template <class T> inline uint32_t getAuctionPrice(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->imbalancePrice);
  }
  
  template <class T> inline uint32_t getAuctionSize(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    return be32toh(msg->imbalanceVolume);
  }  

  template <class T> inline size_t printGenericMsg(const uint8_t* m) {
    printf ("\n");
    return sizeof(T);
  }
  
  inline int printMsg(uint64_t sequence, const uint8_t* m) {
    auto genericHdr {reinterpret_cast<const GenericHdr*>(m)};
    char enc = genericHdr->type;
    uint64_t msgTs = be64toh(genericHdr->ts);

    printf("\t");
    printf("%s,",ts_ns2str(msgTs).c_str());
    printf ("%-8ju,",sequence);
    switch (enc) {
      //--------------------------------------------------------------
    case 'H':  // TradingAction
      return printGenericMsg<TradingAction>(m);
      //      return printTradingAction<TradingAction>(m);
    case 'a':  // AddOrderShort
      return printGenericMsg<AddOrderShort>(m);
      //      return printAddOrder<AddOrderShort>(m);
    case 'A':  // AddOrderLong
      return printGenericMsg<AddOrderLong>(m);
      //      return printAddOrder<AddOrderLong>(m);
    case 'j':  // AddQuoteShort
      return printGenericMsg<AddQuoteShort>(m);
      //      return printAddQuote<AddQuoteShort>(m);
    case 'J':  // AddQuoteLong
      return printGenericMsg<AddQuoteLong>(m);
      //      return printAddQuote<AddQuoteLong>(m);
    case 'E':  // OrderExecuted
      return printGenericMsg<OrderExecuted>(m);
      //      return printOrderExecuted<OrderExecuted>(m);
    case 'C':  // OrderExecutedPrice
      return printGenericMsg<OrderExecutedPrice>(m);
      //      return printOrderExecuted<OrderExecutedPrice>(m);
    case 'X':  // OrderCancel
      return printGenericMsg<OrderCancel>(m);
      //      return printOrderExecuted<OrderCancel>(m);
    case 'u':  // ReplaceOrderShort
      return printGenericMsg<ReplaceOrderShort>(m);
      //      return printReplaceOrder<ReplaceOrderShort>(m);
    case 'U':  // ReplaceOrderLong
      return printGenericMsg<ReplaceOrderLong>(m);
      //      return printReplaceOrder<ReplaceOrderLong>(m);
    case 'G':  // SingleSideUpdate
      return printGenericMsg<SingleSideUpdate>(m);
      //      return printSingleSideUpdate<SingleSideUpdate>(m);
    case 'k':  // QuoteReplaceShort
      return printGenericMsg<QuoteReplaceShort>(m);
      //      return printReplaceQuote<QuoteReplaceShort>(m);
    case 'K':  // QuoteReplaceLong
      return printGenericMsg<QuoteReplaceLong>(m);
      //      return printReplaceQuote<QuoteReplaceLong>(m);
    case 'Y':  // QuoteDelete
      return printGenericMsg<QuoteDelete>(m);
      //      return printDeleteQuote<QuoteDelete>(m);
    case 'Q':  // Trade
      return printGenericMsg<Trade>(m);
      //      return printTrade<Trade>(m);
    case 'S':  // SystemEvent
      return printGenericMsg<SystemEvent>(m);
    case 'B':  // BrokenTrade
      return printGenericMsg<BrokenTrade>(m);
    case 'I':  // NOII
      return printGenericMsg<NOII>(m);
    case 'M':  // EndOfSnapshot
      return printGenericMsg<EndOfSnapshot>(m);
    case 'R':  // Directory
      return printGenericMsg<Directory>(m);
    default: 
      on_error("UNEXPECTED Message type: enc=\'%c\'",enc);
    }
    return 0;
  }
      
  inline ssize_t printPkt(const uint8_t* pkt) {
    auto p {pkt};
    auto moldHdr {reinterpret_cast<const mold_hdr*>(p)};
    auto sequence = be64toh(moldHdr->sequence);
    auto msgCnt   = be16toh(moldHdr->message_cnt);
    p += sizeof(*moldHdr);
    for (auto i = 0; i < msgCnt; i++) {
      auto msgLenRaw = reinterpret_cast<const uint16_t*>(p);
      auto msgLen = be16toh(*msgLenRaw);
      p += sizeof(*msgLenRaw);
      auto parsedMsgLen = printMsg(sequence,p);
      if (parsedMsgLen != msgLen)
	on_error("parsedMsgLen %d != msgLen %d",parsedMsgLen,msgLen);
      sequence++;
      p += msgLen;
    }
    return p - pkt;
  }
} // namespace Bx
#endif
