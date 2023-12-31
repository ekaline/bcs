#ifndef MRX2TOP_MESSAGES_H
#define MRX2TOP_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "EkaFhNasdaqCommonParser.h"

using namespace EfhNasdaqCommon;
namespace Mrx2Top {

  enum class MsgType : char {
    SystemEvent                = 'S',
    Directory                = 'V',
    TradingAction            = 'H',
    BestBidAndAskUpdateShort = 'q',
    BestBidAndAskUpdateLong  = 'Q',
    BestBidUpdateShort       = 'b',
    BestBidUpdateLong        = 'B',
    BestAskUpdateShort       = 'a',
    BestAskUpdateLong        = 'A',
    Trade                    = 'T',
    BrokenTrade              = 'X',
    EndOfSnapshot            = 'M' ,

    ComplexDirectory           = 'N',
    ComplexBestBidAndAskUpdate = 'E',
    ComplexBestBidUpdate       = 'c',
    ComplexBestAskUpdate       = 'd',

    Auction                    = 'I',
    Order                      = 'O'

  };

  inline const char*
  getMsgTypeName(MsgType c) {
    switch (c) {
    case MsgType::SystemEvent :
      return "SystemEvent";
    case MsgType::Directory :
      return "Directory";
    case MsgType::TradingAction :
      return "TradingAction";
    case MsgType::BestBidAndAskUpdateShort :
      return "BestBidAndAskUpdateShort";
    case MsgType::BestBidAndAskUpdateLong :
      return "BestBidAndAskUpdateLong";
    case MsgType::BestBidUpdateShort :
      return "BestBidUpdateShort";
    case MsgType::BestBidUpdateLong :
      return "BestBidUpdateLong";
    case MsgType::BestAskUpdateShort :
      return "BestAskUpdateShort";
    case MsgType::BestAskUpdateLong :
      return "BestAskUpdateLong";
    case MsgType::Trade :
      return "Trade";
    case MsgType::BrokenTrade :
      return "BrokenTrade";
    case MsgType::EndOfSnapshot :
      return "EndOfSnapshot";

    case MsgType::ComplexDirectory :
      return "ComplexDirectory";
    case MsgType::ComplexBestBidAndAskUpdate :
      return "ComplexBestBidAndAskUpdate";
    case MsgType::ComplexBestBidUpdate :
      return "ComplexBestBidUpdate";
    case MsgType::ComplexBestAskUpdate :
      return "ComplexBestAskUpdate";

    case MsgType::Auction :
      return "Auction";

    default:
      return "Unknown";
    }
  }
  
  struct GenericHdr { // Dummy
    char	  type;        // 1
    uint16_t      trackingNum; // 2 Integer Internal system tracking number
    uint64_t      ts;          // 8 Nanoseconds since midnight
  } __attribute__((packed));

  static inline uint64_t getTs(const void* m) {
    auto hdr = reinterpret_cast<const GenericHdr*>(m);
    return be64toh(hdr->ts);
  }

    struct SystemEvent { // 'S'
    GenericHdr hdr;        // 11grep
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
                           //  "W" End of WCO Early closing. This message is intended to
                           //      indicate that the exchange will no
                           //      longer accept any new orders or changes to existing
                           //      Orders on last trading date of WCO options. 12:00 Noon 
  } __attribute__((packed));

  struct Directory { // 'V'
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


  struct BestBidAndAskUpdateShort { // 'q'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4


    char     quoteCondition; // <space> = regular quote/autox eligible
                             // “X” = Ask side not firm Bid side firm
                             // "Y" = Bid side not firm Ask side firm
    uint16_t bidMarketOrderSize;
    uint16_t bidPrice;
    uint16_t bidSize;
    uint16_t bidCustSize;
    uint16_t bidProCustSize;

    uint16_t askMarketOrderSize;
    uint16_t askPrice;
    uint16_t askSize;
    uint16_t askCustSize;
    uint16_t askProCustSize;

  } __attribute__((packed));

  struct BestBidAndAskUpdateLong { // 'Q'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4


    char     quoteCondition; // <space> = regular quote/autox eligible
                             // “X” = Ask side not firm Bid side firm
                             // "Y" = Bid side not firm Ask side firm
    uint32_t bidMarketOrderSize;
    uint32_t bidPrice;
    uint32_t bidSize;
    uint32_t bidCustSize;
    uint32_t bidProCustSize;

    uint32_t askMarketOrderSize;
    uint32_t askPrice;
    uint32_t askSize;
    uint32_t askCustSize;
    uint32_t askProCustSize;

  } __attribute__((packed));


  struct BestBidOrAskUpdateShort { // 'b' = bid side, 'a' = ask side
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4


    char     quoteCondition; // <space> = regular quote/autox eligible
                             // “X” = Ask side not firm Bid side firm
                             // "Y" = Bid side not firm Ask side firm
    uint16_t marketOrderSize;
    uint16_t price;
    uint16_t volume; // size
    uint16_t custSize;
    uint16_t proCustSize;
  } __attribute__((packed));
  
  struct BestBidOrAskUpdateLong { // 'B' = bid side, 'A' = ask side
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4


    char     quoteCondition; // <space> = regular quote/autox eligible
                             // “X” = Ask side not firm Bid side firm
                             // "Y" = Bid side not firm Ask side firm
    uint32_t marketOrderSize;
    uint32_t price;
    uint32_t volume;//size;
    uint32_t custSize;
    uint32_t proCustSize;
  } __attribute__((packed));

  struct Trade { // 'T'
    GenericHdr hdr;           // 11
    uint32_t instrumentId;    // 4
    uint32_t crossId;         // 4

    char     tradeCondition;  // 1 Opra based
    uint32_t price;           // 4
    uint32_t volume;          // 4
  } __attribute__((packed));


  struct BrokenTrade { // 'X'
    GenericHdr hdr;          // 11
    uint32_t instrumentId;   // 4
    uint32_t originalCrossId;// 4
    uint32_t originalPrice;  // 4
    uint32_t originalVolume; // 4
  } __attribute__((packed));
  // ----------------------------------------


  struct EndOfSnapshot { // 'M'
    char          type;             // 1
    char          nextLifeSequence[20];    
  } __attribute__((packed));

  
  struct ComplexDirectory { //'N'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    char     type;         // 1
                           //    “V” = Vertical Spread
                           //    “T” = Time Spread
                           //    “D” = Diagonal Spread
                           //    “S” = Straddle
                           //    “G” = Strangle
                           //    “C” = Combo
                           //    “R” = Risk Reversal
                           //    “A” = Ratio Spread
                           //    “B” = Box Spread
                           //    “F” = Butterfly Spread
                           //    “U” = Custom
    char underlyingSymbol[13]; // 13
    uint8_t   numOfLegs;   // 1
  } __attribute__((packed));

  struct ComplexDefinitionLeg { // part of ComplexDefinition
    uint32_t instrumentId;  // 4
    char securitySymbol[6]; // 6
    uint8_t  expYear;       // 1
    uint8_t  expMonth;      // 1
    uint8_t  expDay;        // 1
    uint32_t strikePrice;   // 4
    char     optionType;    // 1  “C” = Call, “P” = Put,
                            //    Blank (“ “) for Stock Leg.
    char     side;          // 1  “B” = Buy, “S” = Sell
    uint32_t ratio;         // 4
  } __attribute__((packed));
  
  struct ComplexBestBidAndAskUpdate { //'E'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4
    char     quoteCondition; // <space> = regular quote/autox eligible

    uint32_t bidMarketOrderSize;
    int32_t  bidPrice;
    uint32_t bidSize;
    uint32_t bidCustSize;
    uint32_t bidProCustSize;
    uint32_t bidDnttSize;
    uint32_t bidDnttMarketSize;

    uint32_t askMarketOrderSize;
    int32_t  askPrice;
    uint32_t askSize;
    uint32_t askCustSize;
    uint32_t askProCustSize;
    uint32_t askDnttSize;
    uint32_t askDnttMarketSize;

  } __attribute__((packed));

  struct ComplexBestBidOrAskUpdate { // 'c' = bid side, 'd' = ask side
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4


    char     quoteCondition; // <space> = regular quote/autox eligible

    uint32_t marketOrderSize;
    int32_t  price;
    uint32_t volume;//size;
    uint32_t custSize;
    uint32_t proCustSize;
    uint32_t dnttSize;
    uint32_t dnttMarketSize;
  } __attribute__((packed));

  struct Order { // 'O'
    GenericHdr hdr;        // 11
    uint32_t instrumentId; // 4


    char placeHolder[61-11-4];

  } __attribute__((packed));


  struct Auction { // 'I'
    GenericHdr hdr;           // 11
    uint32_t instrumentId;    // 4
    uint32_t auctionId;       // 4 
    char     auctionType;     // 1
                              //    'B’ = Block Auction
                              //    ‘O’ = Opening
                              //    ‘R’ = Reopening
                              //    ‘I’ = Order Exposure
                              //    ‘P’ = Price Improvement (PIM) Auction
                              //    ‘C’ = Facilitation
                              //    ‘S’ = Solicitation
    
                              //    ‘S’=Start of Auction
                              //    ‘E’=End of Auction
                              //    ‘U’=Auction Update
                              //    When AuctionType = ”O”, “R”, or “I”
                              //    this AuctionEvent will always
                              //    be presented as “U”
    char     auctionEvent;    // 1
                              //    ‘S’=Start of Auction
                              //    ‘E’=End of Auction
                              //    ‘U’=Auction Update
                              //    When AuctionType = ”O”, “R”, or “I”
                              //    this AuctionEvent will always
                              //    be presented as “U”
    uint32_t volume;          // 4 quantity
    char     side;            // 1 'B', 'S'
    uint32_t price;           // 4
    uint32_t inbalanceVolume; // 4 Imbalance volume for opening auction.
                              //   Will be 0 for other auctions
    char     execFlag;        // 1 'N' = None; 'A' = AON
    char     orderCapacity;   // 1
                              //   ‘C’ = Customer Order
                              //   ‘F’ = Firm Order
                              //   ‘M’ = Nasdaq registered Market Maker Order
                              //   ‘B’ = Broker Dealer Order
                              //   ‘P' = Professional Order
                              //   ‘O’ = Other exchange registered
                              //         Market Maker Order
                              //   ' ' = when not applicable
                              //         for certain Auction Types
    
    char    ownerId[6];       // 6 Block Auction may provide Owner ID;
                              //   Spaces when not set
    char    giveUp[6];        // 6 Block Auction may provide GiveUp;
                              //   Spaces when not set
    char    cmta[6];          // 6 Block Auction may provide CMTA;
                              //   Spaces when not set
    
  } __attribute__((packed));

  template <class T>
  inline EfhAuctionType getAuctionType(const void* m) {
    auto auctionType = reinterpret_cast<const T*>(m)->auctionType;
    switch (auctionType) {
    case 'B' : // Block Auction
      return EfhAuctionType::kAllOrNone;
    case 'O' : // Opening
      return EfhAuctionType::kOpen;
    case 'R' : // Reopening
      return EfhAuctionType::kReopen;
    case 'I' : // Order Exposure
      return EfhAuctionType::kExposed;
    case 'P' : // Price Improvement (PIM) Auction
      return EfhAuctionType::kPrism;
    case 'C' : // Facilitation
      return EfhAuctionType::kFacilitation;
    case 'S' : // Solicitation
      return EfhAuctionType::kSolicitation;
    default:
      on_error("Unexpected auctionType \'%c\' (0x%x)",
	       auctionType,auctionType);
    }
  }

  template <class T>
  inline uint64_t getAuctionDurationNanos(const void* m) {
    auto auctionType = reinterpret_cast<const T*>(m)->auctionType;
    switch (auctionType) {
    case 'B' : // Block Auction
    case 'P' : // Price Improvement (PIM) Auction
    case 'C' : // Facilitation
    case 'S' : // Solicitation
      return 100'000'000;
    case 'O' : // Opening
    case 'R' : // Reopening
    case 'I' : // Order Exposure
      return 150'000'000;
    default:
      on_error("Unexpected auctionType \'%c\' (0x%x)",
               auctionType,auctionType);
    }
  }

  template <class T>
  inline EfhOrderSide getAuctionSide(const void* m) {
    auto side = reinterpret_cast <const T*>(m)->side;
    switch (side) {
    case 'B' :
      return EfhOrderSide::kBid;
    case 'S' :
      return EfhOrderSide::kAsk;
    case ' ' :
      return EfhOrderSide::kOther;
    default :
      on_error("Unexpected side \'%c\' (0x%x)",side,side);
    }
  }

  inline bool isUpdateOnlyAuction(const char auctionType) {
    switch (auctionType) {
    case 'O' : // Opening
    case 'R' : // Reopening
    case 'I' : // Order Exposure
      return true;
    default:
      return false;
    }
  }
  
  template <class T>
  inline EfhAuctionUpdateType getAuctionUpdateType(const void* m) {
    const auto msg = reinterpret_cast<const T*>(m);
    auto auctionEvent = msg->auctionEvent;
    switch (auctionEvent) {
    case 'S' : // Start of Auction
      return EfhAuctionUpdateType::kNew;
    case 'U' : // Auction Update
      if (isUpdateOnlyAuction(msg->auctionType)) {
        return EfhAuctionUpdateType::kNew;
      } else {
        return EfhAuctionUpdateType::kReplace;
      }
    case 'E' : // End of Auction
      return EfhAuctionUpdateType::kDelete;
    default:
      on_error("Unexpected auctionEvent \'%c\' (0x%x)",
	       auctionEvent,auctionEvent);
    }
  }

  template <class T>
  inline SideT getMrxSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    auto type = msg->hdr.type;
    switch (type) {
    case 'B' :
    case 'b' :
      return SideT::BID;
    case 'A' :
    case 'a' :
      return SideT::ASK;
    default :
      on_error("Unexpected side \'%c\'",type);
    }
  }

  template <class T>
  inline EfhOrderCapacity getAuctionCapacity(const void* m) {
    auto orderCapacity = reinterpret_cast<const T*>(m)->orderCapacity;
    switch (orderCapacity) {
    case 'C' : //  Customer Order
      return EfhOrderCapacity::kCustomer;
    case 'P' : //  Professional Order
      return EfhOrderCapacity::kProfessionalCustomer;
    case 'B' : //  Broker Dealer Order
      return EfhOrderCapacity::kBrokerDealerAsCustomer;
    case 'F' : //  Firm Order
      return EfhOrderCapacity::kBrokerDealer;
    case 'M' : //  Nasdaq registered Market Maker Order
      return EfhOrderCapacity::kMarketMaker;
    case 'O' : //  Other exchange registered
      return EfhOrderCapacity::kAwayMarketMaker;
    case ' ' : //  not applicable for certain Auction Types
      return EfhOrderCapacity::kUnknown;
    default:
      on_error("Unexpected orderCapacity \'%c\' (0x%x)",
	       orderCapacity,orderCapacity);
    }
  }
  
  template <class T>
  inline SideT getMrxComplexSide(const uint8_t* m) {
    auto msg {reinterpret_cast <const T*>(m)};
    auto type = msg->hdr.type;
    switch (type) {
    case 'c' :
      return SideT::BID;
    case 'd' :
      return SideT::ASK;
    default :
      on_error("Unexpected side \'%c\'",type);
    }
  }
  
  template <class T>
  inline EfhSecurityType getLegType(const uint8_t* m) {
    auto legType {reinterpret_cast<const T*>(m)->optionType};
    switch (legType) {
    case ' ' :
      return EfhSecurityType::kStock;
    case 'C' :
    case 'P' :
      return EfhSecurityType::kOption;
    default :
      on_error("Unexpected Leg option type \'%c\'",legType);
    }
  }

  template <class T>
  inline EfhOrderSide getLegSide(const uint8_t* m) {
    switch (reinterpret_cast<const T*>(m)->side) {
    case 'B' : return EfhOrderSide::kBid;
    case 'S' : return EfhOrderSide::kAsk;
    default  :
      on_error("Unexpected Leg side \'%c\'",
	       reinterpret_cast<const T*>(m)->side);
    }
  }

  template <class MsgT>
  inline size_t printSystemEvent(const void* m, FILE* fd = stdout) {
    auto msg {reinterpret_cast <const MsgT*>(m)};
    fprintf (fd,"\'%c\'",msg->event);
    fprintf (fd,"\n");
    return sizeof(MsgT);
  }

  template <class MsgT>
  inline size_t printTradingAction(const void* m, FILE* fd = stdout) {
    auto msg {reinterpret_cast <const MsgT*>(m)};
    fprintf (fd,"\'%c\'",msg->state);
    fprintf (fd,"\n");
    return sizeof(MsgT);
  }

  template <class MsgT>
  inline size_t printDefinition(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};
    fprintf (fd,"\n");
    return sizeof(MsgT);
  }

  
  template <class MsgT>
  inline size_t printTwoSidesUpdate(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};
    fprintf (fd,"%u,",getInstrumentId<MsgT>(m));
    
    fprintf (fd,"%u,",getBidPrice<MsgT>(m));
    fprintf (fd,"%u,",getBidSize<MsgT>(m));
    fprintf (fd,"%u,",getBidCustSize<MsgT>(m));
    fprintf (fd,"%u,",getBidProCustSize<MsgT>(m));
    
    fprintf (fd,"%u,",getAskPrice<MsgT>(m));
    fprintf (fd,"%u,",getAskSize<MsgT>(m));
    fprintf (fd,"%u,",getAskCustSize<MsgT>(m));
    fprintf (fd,"%u,",getAskProCustSize<MsgT>(m));
          
    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
    
  template <class MsgT>
  inline size_t printOneSideUpdate(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};
    fprintf (fd,"%u,",getInstrumentId<MsgT>(m));
    
    fprintf (fd,"%u (0x%x),",getPrice<MsgT>(m),reinterpret_cast <const MsgT*>(m)->price);
    fprintf (fd,"%u,",getSize<MsgT>(m));
    fprintf (fd,"%u,",getCustSize<MsgT>(m));
    fprintf (fd,"%u,",getProCustSize<MsgT>(m));
    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
    
  template <class MsgT>
  inline size_t printTrade(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};

    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
      
  template <class MsgT, class LegT>
  inline size_t printComplexDefinition(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};

    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
      
  template <class MsgT>
  inline size_t printComplexTwoSidesUpdate(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};

    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
        
  template <class MsgT>
  inline size_t printComplexOneSideUpdate(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};

    fprintf (fd,"\n");
    return sizeof(MsgT);
  }

  template <class MsgT>
  inline size_t printAuction(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};

    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
  
  template <class MsgT>
  inline size_t printOrder(const void* m, FILE* fd = stdout) {
    //    auto msg {reinterpret_cast <const MsgT*>(m)};

    fprintf (fd,"\n");
    return sizeof(MsgT);
  }
  
  inline size_t
  printMsg(const void* m, uint64_t sequence = 0, FILE* fd = stdout) {
    //    using PriceT = uint32_t; // Price to display
    auto genericHdr {reinterpret_cast<const GenericHdr*>(m)};
    auto enc = static_cast<MsgType>(genericHdr->type);
    uint64_t msgTs = getTs(m);

    fprintf(fd,"\t");
    fprintf(fd,"%s,",ts_ns2str(msgTs).c_str());
    fprintf (fd,"%-8ju,",sequence);
    fprintf (fd,"\'%c\',",(char)enc);
    fprintf (fd,"%s,",getMsgTypeName(enc));
    switch (enc) {
      //--------------------------------------------------------------
    case MsgType::SystemEvent :
      return printSystemEvent<SystemEvent>(m,fd);
      //--------------------------------------------------------------
    case MsgType::TradingAction :
      return printTradingAction<TradingAction>(m,fd);
      //--------------------------------------------------------------
    case MsgType::Directory : 
      return printDefinition<Directory>(m,fd);
      //--------------------------------------------------------------
    case MsgType::EndOfSnapshot :
      return sizeof(EndOfSnapshot);
      //--------------------------------------------------------------
    case MsgType::BestBidAndAskUpdateShort :
      return printTwoSidesUpdate<BestBidAndAskUpdateShort>(m,fd);
      //--------------------------------------------------------------
    case MsgType::BestBidAndAskUpdateLong :
      return printTwoSidesUpdate<BestBidAndAskUpdateLong>(m,fd);
      //--------------------------------------------------------------
    case MsgType::BestBidUpdateShort :
    case MsgType::BestAskUpdateShort :
      return printOneSideUpdate<BestBidOrAskUpdateShort>(m,fd);
      //--------------------------------------------------------------
    case MsgType::BestBidUpdateLong :
    case MsgType::BestAskUpdateLong :
      return printOneSideUpdate<BestBidOrAskUpdateLong>(m,fd);
      //--------------------------------------------------------------
    case MsgType::Trade :
      return printTrade<Trade>(m,fd);
      //--------------------------------------------------------------
    case MsgType::BrokenTrade :
      return sizeof(BrokenTrade);
      //--------------------------------------------------------------
    case MsgType::ComplexDirectory :
      return printComplexDefinition<ComplexDirectory,ComplexDefinitionLeg>(m,fd);
      //--------------------------------------------------------------
    case MsgType::ComplexBestBidAndAskUpdate :
      return printComplexTwoSidesUpdate<ComplexBestBidAndAskUpdate>(m,fd);
      //--------------------------------------------------------------
    case MsgType::ComplexBestBidUpdate :
    case MsgType::ComplexBestAskUpdate :
      return printComplexOneSideUpdate<ComplexBestBidOrAskUpdate>(m,fd);
      //--------------------------------------------------------------
    case MsgType::Auction :
      return printAuction<Auction>(m,fd);
      //--------------------------------------------------------------
    case MsgType::Order :
      return printOrder<Order>(m,fd);
      //--------------------------------------------------------------
    default: 
      on_error("UNEXPECTED Message type: enc=\'%c\'",(char)enc);
      //--------------------------------------------------------------
    }
  }
} //namespace Mrx2top

#endif
