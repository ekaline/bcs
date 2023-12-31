#ifndef _EKA_FH_PLR_PARSER_H_
#define _EKA_FH_PLR_PARSER_H_

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "eka_macros.h"
#include "EkaFhPlrGr.h"

// #define PLR_CERT 1


// https://www.nyse.com/publicdocs/nyse/data/PILLAR_TOP_Client_Specification.pdf

namespace Plr {

#if 0
NYSE ARCA Options 162 NYSE Arca TOP - BBO
NYSE ARCA Options 165 NYSE Arca TOP - Trades
NYSE ARCA Options 164 NYSE Arca TOP - Imbalances
NYSE AMERICAN Options 172 NYSE American TOP - BBO
NYSE AMERICAN Options 175 NYSE American TOP - Trades
NYSE AMERICAN Options 174 NYSE American TOP - Imbalances
#endif

  const uint8_t ARCA_BBO_ProductId = 162;
  const uint8_t ARCA_Complex_ProductId = 163;
  const uint8_t ARCA_Trades_ProductId = 165;
  const uint8_t ARCA_Auction_ProductId = 164;

  const uint8_t AMEX_BBO_ProductId = 172;
  const uint8_t AMEX_Complex_ProductId = 173;
  const uint8_t AMEX_Trades_ProductId = 175;
  const uint8_t AMEX_Auction_ProductId = 174;

  const size_t MaxVanillaDefinitions = 1'500'000;
  
  struct PktHdr {
    uint16_t pktSize;
    uint8_t  deliveryFlag;
    uint8_t  numMsgs;
    uint32_t seqNum;
    uint32_t seconds;
    uint32_t ns;
  } __attribute__((packed));

  enum class DeliveryFlag : uint8_t  {
				      Heartbeat = 1,
				      Failover = 10,
				      Original = 11,
				      SeqReset = 12,
	
				      SinglePktRetransmit = 13,
				      PartOfRetransmit = 15,
	
				      SinglePktRefresh = 17,
				      StartOfRefresh = 18,
				      PartOfRefresh = 19,
				      EndOfRefresh = 20,
	
				      MsgUnavail = 21
  };

  inline bool operator==(const DeliveryFlag lhs, const uint8_t rhs) noexcept {
    return ((uint8_t)lhs) == rhs;
  }
  inline bool operator==(const uint8_t lhs, const DeliveryFlag rhs) noexcept {
    return lhs == (uint8_t)rhs;
  }
  inline bool operator!=(const DeliveryFlag lhs, const uint8_t rhs) noexcept {
    return ((uint8_t)lhs) != rhs;
  }
  inline bool operator!=(const uint8_t lhs, const DeliveryFlag rhs) noexcept {
    return lhs != (uint8_t)rhs;
  }
  
  inline EfhOrderSide getSide(char side) {
    switch (side) {
    case 'B' : return EfhOrderSide::kBid;
    case 'S' : return EfhOrderSide::kAsk;
    default  : return EfhOrderSide::kErr;
    }
  }

  inline std::string deliveryFlag2str (uint8_t flag) {
    switch (static_cast<DeliveryFlag>(flag)) {
    case DeliveryFlag::Heartbeat :
      return std::string("Heartbeat");
    case DeliveryFlag::Failover :
      return std::string("Failover");
    case DeliveryFlag::Original :
      return std::string("Original");
    case DeliveryFlag::SeqReset :
      return std::string("SeqReset");
    case DeliveryFlag::SinglePktRetransmit :
      return std::string("SinglePktRetransmit");
    case DeliveryFlag::PartOfRetransmit :
      return std::string("PartOfRetransmit");
    case DeliveryFlag::SinglePktRefresh :
      return std::string("SinglePktRefresh");
    case DeliveryFlag::StartOfRefresh :
      return std::string("StartOfRefresh");
    case DeliveryFlag::PartOfRefresh :
      return std::string("PartOfRefresh");
    case DeliveryFlag::EndOfRefresh :
      return std::string("EndOfRefresh");
    case DeliveryFlag::MsgUnavail :
      return std::string("MsgUnavail");
    default:
      on_error("Unexpected DeliveryFlag %u",flag);
    }
  }

  enum class MsgType : uint16_t {
				 // Control
				 SequenceNumberReset = 1,
				 TimeReference = 2,
				 SymbolIndexMapping = 3,
				 RetransmissionRequest = 10,
				 RequestResponse = 11,
				 HeartbeatResponse = 12,
				 SymbolIndexMappingRequest = 13,
				 RefreshRequest = 15,
				 MessageUnavailable = 31,
				 SymbolClear = 32,
				 SecurityStatus = 34,
				 RefreshHeader = 35,
				 OutrightSeriesIndexMapping = 50,
				 OptionsStatus = 51,

				 // Top
				 Quote = 340,
				 Trade = 320,
				 TradeCancel = 321,
				 TradeCorrection = 322,
				 Imbalance = 305,
				 SeriesRfq = 307,
				 SeriesSummary = 323,

				 // Complex
				 ComplexSeriesIndexMapping = 60
  };
  
  inline bool operator==(const MsgType lhs, const uint16_t rhs) noexcept {
    return ((uint16_t)lhs) == rhs;
  }
  inline bool operator==(const uint16_t lhs, const MsgType rhs) noexcept {
    return lhs == (uint16_t)rhs;
  }
  inline bool operator!=(const MsgType lhs, const uint16_t rhs) noexcept {
    return ((uint16_t)lhs) != rhs;
  }
  inline bool operator!=(const uint16_t lhs, const MsgType rhs) noexcept {
    return lhs != (uint16_t)rhs;
  }
  inline std::string msgType2str (uint16_t type) {
    switch (static_cast<MsgType>(type)) {
    case MsgType::SequenceNumberReset :
      return std::string("SequenceNumberReset");
    case MsgType::TimeReference :
      return std::string("TimeReference");
    case MsgType::SymbolIndexMapping :
      return std::string("SymbolIndexMapping");
    case MsgType::RetransmissionRequest :
      return std::string("RetransmissionRequest");
    case MsgType::RequestResponse :
      return std::string("RequestResponse");            
    case MsgType::SymbolIndexMappingRequest :
      return std::string("SymbolIndexMappingRequest");
    case MsgType::RefreshRequest :
      return std::string("RefreshRequest");
    case MsgType::MessageUnavailable :
      return std::string("MessageUnavailable");
    case MsgType::SymbolClear :
      return std::string("SymbolClear");            
    case MsgType::SecurityStatus :
      return std::string("SecurityStatus");            
    case MsgType::RefreshHeader :
      return std::string("RefreshHeader");            
    case MsgType::OutrightSeriesIndexMapping :
      return std::string("OutrightSeriesIndexMapping");            
    case MsgType::OptionsStatus :
      return std::string("OptionsStatus");            
    case MsgType::Quote :
      return std::string("Quote");            
    case MsgType::Trade :
      return std::string("Trade");            
    case MsgType::TradeCancel :
      return std::string("TradeCancel");
    case MsgType::TradeCorrection :
      return std::string("TradeCorrection");
    case MsgType::Imbalance :
      return std::string("Imbalance");
    case MsgType::SeriesRfq :
      return std::string("SeriesRfq");
    case MsgType::SeriesSummary :
      return std::string("SeriesSummary");
    case MsgType::ComplexSeriesIndexMapping :
      return std::string("ComplexSeriesIndexMapping");
    default:
      on_error("Unexpected MsgType %u",type);
    }

  }
  struct MsgHdr {
    uint16_t size;
    uint16_t type;
  } __attribute__((packed));

  struct SymbolIndexMapping { // 3
    MsgHdr   hdr;
    uint32_t SymbolIndex;
    char     Symbol[11]; // Null-terminated ASCII symbol in NYSE Symbology
    char     reserved;
    uint16_t MarketID; // ID of the Originating Market:
    // • 1 - NYSE Equities
    // • 3 – NYSE Arca Equities
    // • 4 – NYSE Arca Options
    // • 8 – NYSE American Options
    // • 9 - NYSE American Equities
    // • 10 - NYSE National Equities
    // • 11 - NYSE Chicago
    
    uint8_t  SystemID; // ID of the Originating matching engine server
    char     ExchangeCode; // For listed equity markets, the market where this symbol is listed:
    // ▪ A – NYSE American
    // ▪ L - LTSE
    // ▪ N – NYSE
    // ▪ P – NYSE Arca
    // ▪ Q – NASDAQ
    // ▪ V - IEX
    // ▪ Z – CBOE
    
    uint8_t  PriceScaleCode;

    char     SecurityType; // Type of Security used by Pillar-powered markets
    // ▪ A – ADR
    // ▪ C - COMMON STOCK
    // ▪ D – DEBENTURES
    // ▪ E – ETF
    // ▪ F – FOREIGN
    // ▪ H – US DEPOSITARY SHARES
    // ▪ I – UNITS
    // ▪ L – INDEX LINKED NOTES
    // ▪ M - MISC/LIQUID TRUST
    // ▪ O – ORDINARY SHARES
    // ▪ P - PREFERRED STOCK
    // ▪ R – RIGHTS
    // ▪ S - SHARES OF BENEFICIARY INTEREST
    // ▪ T – TEST
    // ▪ U – CLOSED END FUND
    // ▪ W – WARRANT
    
    
    uint16_t LotSize;
    uint32_t PrevClosePrice;
    uint32_t PrevCloseVolume;
    uint8_t  PriceResolution; // ▪ 0 - All Penny ▪ 1 - Penny/Nickel ▪ 5 - Nickel/Dime
    char     RoundLot; // Round Lots Accepted: ▪ Y – Yes ▪ N – No
    uint16_t MPV;
    uint16_t UnitOfTrade;
    uint16_t reserved2;
  } __attribute__((packed));

  struct OutrightSeriesIndexMapping { // 50
    MsgHdr   hdr;
    uint32_t SeriesIndex;
    uint8_t  SeriesType; // Identifies series type: • 0 - Standard • 1 - Flex • 2 - Flex percentage
    uint16_t MarketID; // Identifies originating market: • 4 (NYSE Arca Options) • 8 (NYSE American Options)
    uint8_t  SystemID; // ID of the Originating matching engine server
    char     OptionSymbolRoot[6];
    char     UnderlyingSymbol[11]; 
    uint32_t UnderlyingIndex;
    uint8_t  PriceScaleCode;
    uint16_t ContractMultiplier;
    char     MaturityDate[6]; // Option maturity date - YYMMDD
    uint8_t  PutOrCall; // Valid values: • 0 (Put) • 1 (Call)
    char     StrikePrice[10]; // Strike price. ASCII 0-9 with optional decimal point. EG:51.75, 123 (Option only)
    char     ClosingOnlyIndicator; // Valid values: • 0 - Standard Series • 1 - Closing Only Series
    uint8_t  reserved;
  } __attribute__((packed));


  struct SecurityStatus { // 34
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t SymbolIndex;
    uint32_t SymbolSeqNum;
    char     securityStatus; // The new status that this security is transitioning to.
    // The following are Halt Status Codes:
    //   ▪ 4 - Trading Halt
    //   ▪ 5 - Resume
    //   ▪ 6 - Suspend The following are Short Sale Restriction Codes (published for all symbols traded on this exchange):
    //   ▪ A – Short Sale Restriction Activated (Day 1)
    //   ▪ C – Short Sale Restriction Continued (Day 2)
    //   ▪ D - Short Sale Restriction Deactivated
    // Market Session values :
    //   ▪ P – Pre-opening
    //   ▪ B - Begin Accepting orders
    //   ▪ E – Early session
    //   ▪ O – Core session
    //   ▪ L – Late session
    //   ▪ X – Closed
    // If this security is not halted at the time of a session change, the Halt Condition field = ~.
    // If this security is halted on a session change, Halt Condition is non-~, and the security remains halted into the new session.
    // The following values are the Price Indication values :
    //   ▪ I – Halt Resume Price Indication
    //   ▪ G – Pre-Opening Price Indication

    char    HaltCondition; //
    // ▪ ~ - Security not delayed/halted
    // ▪ D - News released
    // ▪ I - Order imbalance
    // ▪ P - News pending
    // ▪ M – LULD pause
    // ▪ X - Equipment changeover
    // ▪ A - Additional Information Requested
    // ▪ C - Regulatory Concern
    // ▪ E - Merger Effective
    // ▪ F - ETF Component Prices Not Available
    // ▪ N - Corporate Action
    // ▪ O - New Security Offering
    // ▪ V - Intraday Indicative Value Not Available Market Wide Circuit Breakers:
    // ▪ 1 - Market Wide Circuit Breaker Halt Level 1
    // ▪ 2 - Market Wide Circuit Breaker Halt Level 2
    // ▪ 3 - Market Wide Circuit Breaker Halt Level 3
    
    
    uint32_t  reserved;
    uint32_t  Price1; // Default value is 0.
    // ▪ If securityStatus = A and this security is listed on this exchange,
    //   then this field is the SSR Triggering Trade Price
    // ▪ If securityStatus = G or I, then this field is the Indication Low Price.

    uint32_t  Price2; // Default value is 0.
    // ▪ If securityStatus = G or I, then this field is the Indication High Price.

    char      SSRtriggeringExchangeID; // This field is only populated when securityStatus = A
    //    and this security is listed on this exchange.
    //    Otherwise it is defaulted to 0x20.
    // 	 Valid values are:
    // 	 ▪ A – NYSE American
    // 	 ▪ B – NASDAQ OMX BX
    // 	 ▪ C – NYSE National
    // 	 ▪ D – FINRA
    // 	 ▪ I – ISE
    // 	 ▪ J – CBOE EDGA
    // 	 ▪ K – CBOE EDGX
    // 	 ▪ L - LTSE
    // 	 ▪ M – NYSE Chicago
    // 	 ▪ N – NYSE
    // 	 ▪ P – NYSE Arca
    // 	 ▪ Q – NASDAQ
    // 	 ▪ S – CTS
    // 	 ▪ T – NASDAQ OMX
    // 	 ▪ V – IEX
    // 	 ▪ W – CBSX
    // 	 ▪ X – NASDAQ OMX PSX
    // 	 ▪ Y – CBOE BYX
    // 	 ▪ Z – CBOE BZX
    // 	 ▪ H - MIAX
    // 	 ▪ U - MEMX
    uint32_t SSRtriggeringVolume;
    uint32_t Time; // Default value is 0.
    // Format : HHMMSSmmm (mmm = milliseconds)
    // ▪ If securityStatus = A and this security is listed on this exchange,
    // then this field is the SSR Trigger Time

    char     SSRstate; // The current SSR state, which this msg updates
    // if the Security Status field contains an SSR Code.
    // Valid values:
    //  ▪ ~ – No Short Sale Restriction in Effect
    //  ▪ E – Short Sale Restriction in Effect

    char     MarketState; // The current Market State, which this msg updates
    //   if the Security Status field contains a Market State Code.
    //   Valid values:
    //   ▪ P – Pre-opening
    //   ▪ E – Early session
    //   ▪ O – Core session
    //   ▪ L – Late session (Non-NYSE only)
    //   ▪ X -- Closed
    char     SessionState; // Unused. Defaulted to 0x00.
  } __attribute__((packed));

  struct RefreshHeader { // 35
    MsgHdr   hdr;
    uint16_t CurrentRefreshPkt;
    uint16_t TotalRefreshPkts;
    uint32_t LastSeqNum;
    uint32_t LastSymbolSeqNum;
  } __attribute__((packed));

  struct ShortRefreshHeader { // 35
    MsgHdr   hdr;
    uint16_t CurrentRefreshPkt;
    uint16_t TotalRefreshPkts;
  } __attribute__((packed));
    
  struct OptionsStatus { // 51
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t seriesIndex;
    uint32_t SeriesSeqNum;
    char     seriesStatus; // The new status that this series is transitioning to.
    // The following are Halt Status Codes:
    // ▪ 4 - Trading Halt
    // ▪ 5 - Resume
    // ▪ 6 - Suspend
    // Market Session values :
    // ▪ P – Pre-opening
    // ▪ B - Begin Accepting orders
    // ▪ O – Core session
    // ▪ X – Closed
    // If this series is not halted at the time of a session change,
    // Halt Condition field = ~.
    // this series is halted on a session change,
    // Condition is non-~,
    // the series remains halted into the new session.

    char    MarketState;   // Market Session values : • P – Pre-opening • O – Core session • X – Closed
    char    HaltCondition; // ▪ ~ - series not delayed/halted ▪ h - Option series is halted
    
  } __attribute__((packed));

  
  struct SequenceNumberReset { // 1
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t SeriesIndex;
    uint8_t  ProductID;
    uint8_t  ChannelID;
  } __attribute__((packed));

  struct SourceTimeReference { // 2
    MsgHdr   hdr;
    uint32_t ID; // ID of the originating Matching Engine partition to which this message applies.
    // This usage will become standard across all products in future releases.
    uint32_t SymbolSeqNum;
    uint32_t sourceTimeSec; // Reserved for future use. Ignore any content.
    // This usage will become standard across all products in future releases.
  } __attribute__((packed));

  struct SymbolClear { // 32
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t SymbolIndex;
    uint32_t NextSourceSeqNum;
  } __attribute__((packed));
  
  struct Quote { // 340
    MsgHdr   hdr;
    uint32_t sourceTimeNs;
    uint32_t seriesIndex;
    uint32_t seriesSeqNum;
    int32_t  askPrice;
    uint32_t askVolume;
    int32_t  bidPrice;
    uint32_t bidVolume;
    char     quoteCondition; // ▪ '1' – Regular Trading
    // ▪ '2' – Rotation
    // ▪ '3' – Trading Halted
    char     reserved;
    uint32_t askCustomerVolume;
    uint32_t bidCustomerVolume;   
  } __attribute__((packed));

  
  struct Trade { // 320
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t seriesIndex;
    uint32_t seriesSeqNum;
      
    uint32_t tradeId;
    int32_t  price;
    uint32_t volume;
      
    char     tradeCond1;
    // Settlement related conditions. Valid values:
    // • “a” – outright series order/quote trading electronically with a outright series CUBE Exposed order or outright series CUBE Exposed order trading electronically with outright series
    // • “c” – trading of a outright series QCC order
    // • “e” – outright series floor trade
    // • “I” – all outright series electronic trades (excluding away market executions) that were not part of the following transactions:
    // o Outright series CUBE
    // o Outright series QCC
    // o Complex order trading electronically with the outright series orders/quotes
    // o ISO trade
    // • “S” – all outright series trades that are generated as part of an Intermarket Sweep order
    // • “D” = any of the above published after 90 seconds of occurring
    // • “f” = complex order trades that were not part of the following transactions
    //    o Complex CUBE
    //    o Complex QCC
    //    o Complex order trading electronically with the outright series orders/quotes
    // • “g” = complex order trading electronically with a complex CUBE Exposed order or complex CUBE Exposed order trading electronically with Complex CUBE Covered order
    // • “h” = trading of a complex QCC order
    // • “i” = complex order to complex order floor trade
    // • “j” = complex order trading electronically with the outright series orders/quotes
    // • “m” = complex order to outright series order floor trade
    // • “p” = complex order with stock to complex order with stock floor trade
    // • “s” (lowercase) = complex order with stock to outright series order floor trade
      
    char     tradeCond2;
    // Valid values:
    // • ‘’ -- N/A (space or 0x20)
    // • O -- Market Center Opening Trade
    // • 5 -- Reopening Trade
      
    char     reserved;
    char     tradeCond4;
    // Valid values:
    // • ‘’ -- N/A (space or 0x20)
    // • Q -- Official Open Price
  } __attribute__((packed));

  struct TradeCorrection { // 322
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t seriesIndex;
    uint32_t seriesSeqNum;
      
    uint32_t originalTradeId;
    uint32_t tradeId;
    int32_t price;
    uint32_t size;
      
    char     tradeCond1;
    // Settlement related conditions. Valid values:
    // • “H” = Transaction is a late report of the opening trade, but is in the correct sequence; i.e., no other transactions have been reported for the particular option contract.
    // • “F” = Transaction is a late report of the opening trade and is out of sequence; i.e., other transactions have been reported for the particular option contract.
    // • “D” = Transaction is being reported late, but is in the correct sequence; i.e., no later transactions have been reported for the particular option contract.
    // • “B” = Transaction is being reported late and is out of sequence; i.e., later transactions have been reported for the particular option contract.
    char     reserved1;
    char     reserved2;
    char     reserved3;
  } __attribute__((packed));


  struct OptionsImbalance { // 305
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t seriesIndex;
    uint32_t seriesSeqNum;
      
    uint32_t reserved1;
    uint32_t PairedQty;
    uint32_t TotalImbalanceQty;
    uint32_t MarketImbalanceQty;
    
    uint16_t reserved2;
    
    char     AuctionType;   // • 'M' – Core Opening Auction
    // • 'H' – Reopening Auction (Halt resume)
    char     ImbalanceSide; // The side of the TotalImbalanceQty:
    // • 'B' – Buy side
    // • 'S' – Sell side
    // • 'Space' – No imbalance
    
    int32_t  ContinuousBookClearingPrice;
    int32_t  AuctionInterestClearingPrice;
    uint32_t reserved3;
    int32_t  IndicativeMatchPrice;
    int32_t  UpperCollar;
    int32_t  LowerCollar;
    
    uint8_t  AuctionStatus; // Indicates whether the auction will run
    // • 0 - Will run as usual
    // • 4 - Auction will not run because legal width quote does not exist
    // • 5 - Auction will not run because market maker quote is not received

  } __attribute__((packed));

  inline int64_t getPrice(const int64_t plrPrice, const EkaFhPlrGr::FhSecurity *sec) {
    // PriceScaleCode is stored in opaqueAttrA
    return priceToEfhScale(plrPrice, static_cast<uint8_t>(sec->opaqueAttrA));
  }
  
  inline EfhOrderCapacity getRfqCapacity(char capacity) {
    switch (capacity) {
    case ' ' : return EfhOrderCapacity::kUnknown;
    case '0' : return EfhOrderCapacity::kCustomer;
    case '1' : return EfhOrderCapacity::kAgency; // Firm
    case '2' : return EfhOrderCapacity::kBrokerDealer;
    case '3' : return EfhOrderCapacity::kMarketMaker;
    case '5' : return EfhOrderCapacity::kAwayMarketMaker;
    case '8' : return EfhOrderCapacity::kProfessionalCustomer;
    default  :
      on_error("Unexpected RFQ capacity \'%c\'",capacity);
    }
  }

  inline EfhAuctionType getAuctionType(char auctionType) {
    switch (auctionType) {
    case 'P' : return EfhAuctionType::kCube;
    case 'F' : return EfhAuctionType::kFacilitation;
    case 'S' : return EfhAuctionType::kSolicitation;
    case 'B' : return EfhAuctionType::kExposed; // Bold (Outright only)
    case 'C' : return EfhAuctionType::kComplexOrderAuction; // COA (Complex only)
    default  :
      on_error("Unexpected auctionType \'%c\'",auctionType);
    }
  }

  inline uint64_t getRfqRunTimeNanos(char auctionType) {
    switch (auctionType) {
    case 'P': // Price Improvement
    case 'F': // Facilitation
    case 'S': // Solicitation
    case 'B': // Bold (Outright only)
      return 500'000'000;
    case 'C': // COA (Complex only)
      return 100'000'000;
    default:
      on_error("Unexpected auctionType \'%c\'",auctionType);
    }
  }

  inline EfhAuctionUpdateType getAuctionUpdateType (char auctionUpdateType) {
    switch (auctionUpdateType) {
    case 'O' : return EfhAuctionUpdateType::kNew;
    case 'Q' : return EfhAuctionUpdateType::kDelete;
    default  :
      on_error("Unexpected auctionUpdateType (\'RFQ Status\') \'%c\'",auctionUpdateType);
    }
  }

  struct Rfq { // 307
    MsgHdr   hdr;
    uint32_t sourceTimeSec;
    uint32_t sourceTimeNs;
    uint32_t seriesIndex;
    uint32_t seriesSeqNum;
      
    char     side; // Side of the RFQ
    // 'B' = Buy 'S' = Sell
    // 		     For Complex series, when a complex
    // 		     instrument is traded, the seller
    // 		     Sells the legs with Side = S and
    // 		     Buys the legs with Side = B
    // 		     The buyer
    // 		     Buys the legs with Side = S and
    // 		     Sells the legs with Side = B

    char     type;  // Order Type of CUBE/Bold/COA
    //                'P' = Price Improvement
    // 		      'F' = Facilitation
    // 		      'S' = Solicitation
    // 		      'B' = Bold (Outright only)
    //                'C' = COA (Complex only)

    char     capacity; // Customer or Firm capacity specified with the order.
    // Values include:
    //                   • (blank) = information not specified
    // 			 • 0 = Customer
    // 			 • 1 = Firm
    // 			 • 2 = Broker Dealer
    // 			 • 3 = Market Maker
    // 			 • 5 = Away Market Maker
    // 			 • 8 = Professional Customer
    // 			 Note: This field is used for BOLD only

    uint32_t totalQuantity;
    int32_t  workingPrice;
    uint32_t participant;
    uint64_t auctionId;
    char     rfqStatus; // 'O' - Start of RFQ Auction
    //                     'Q' - End of RFQ Auction
    //                     Note: This field is only used for CUBE/COA
  } __attribute__((packed));

  
  inline EfhSecurityType getComplexSecurityType(char type) {
    switch (type) {
    case 'O' : return EfhSecurityType::kOption;
    case 'E' : return EfhSecurityType::kStock;
    default  : on_error("Unexpected LegSecurityType \'%c\'",type);
    }
  }
  
  struct ComplexDefinitionLeg {
    uint32_t SymbolIndex; // This field will repeat for each leg:
                          // • Series index if Security type is Option
                          // • Underlying index if Security type is Equity
    
    uint16_t LegRatioQty; // Leg Ratio. This filed will repeat for each leg
                          // The maximum ratio between the smallest component
                          // leg quantity to the largest leg component
                          // quantity cannot exceed 3:1 or 1:3
    
    char     side;        // Leg side. This filed will repeat for each leg:
	                  // 'B' = Buy 'S' = Sell
    
    char     SecurityType;// Leg Security Type. This filed will repeat for each leg:
                          // • 'O' (Options Series leg)
                          // • 'E' (Equity stock leg)
  } __attribute__((packed));
  
  struct ComplexSeriesIndexMapping_root { // ComplexDefinition = 60
    MsgHdr   hdr;
    uint32_t seriesIndex;
    uint16_t MarketID; // ID of the Originating Market:
                       // • 4 – NYSE Arca Options
                       // • 8 – NYSE American Options
    
    uint8_t  SystemID; // ID of the Originating matching engine server
    
    uint16_t NoOfLegs; // Number of legs in complex symbol: 2..12
    
    //     ComplexDefinitionLeg leg[NoOfLegs];

  } __attribute__((packed));
  
  struct RequestResponse { // 11
    MsgHdr   hdr;
    uint32_t RequestSeqNum; // The sequence number of the request message sent by the client.
    // This can be used by the client to couple this response with
    // the original request message
    
    uint32_t BeginSeqNum;   // For Retrans Request responses, the beginning sequence number
    // of the requested retransmission range. For responses to
    // Refresh or Symbol Mapping Requests, 0.
    
    uint32_t EndSeqNum;     // For Retrans Request responses, the ending sequence number
    // of the requested retransmission range. For responses to
    // Refresh or Symbol Mapping Requests, 0.
    char     SourceID[10];
    uint8_t  ProductID;
    uint8_t  ChannelID;
    char     Status;        // Valid values:
    // ▪ 0 – Message was accepted
    // ▪ 1 – Rejected due to an Invalid Source ID
    // ▪ 3 – Rejected due to maximum sequence range (see threshold limits)
    // ▪ 4 – Rejected due to maximum request in a day
    // ▪ 5 – Rejected due to maximum number of refresh requests in a day
    // ▪ 6 – Rejected. Request message SeqNum TTL (Time to live) is too old. 
    //       Use refresh to recover current state if necessary.
    // ▪ 7 – Rejected due to an Invalid Channel ID
    // ▪ 8 – Rejected due to an Invalid Product ID
    // ▪ 9 – Rejected due to:
    //       1) Invalid MsgType, or
    //       2) Mismatch between MsgType and MsgSize
  } __attribute__((packed));

  struct HeartbeatResponse { // 12
    MsgHdr   hdr;
    char     SourceID[10];
  } __attribute__((packed));

  struct RetransmissionRequest { // 10
    MsgHdr   hdr;
    uint32_t BeginSeqNum;   // The beginning sequence number of the unavailable range of messages.    
    uint32_t EndSeqNum;     // The ending sequence number of the unavailable range of messages.
    char     SourceID[10];
    uint8_t  ProductID;
    uint8_t  ChannelID;
  } __attribute__((packed));

  struct SymbolIndexMappingRequest { // 13
    MsgHdr   hdr;
    uint32_t SymbolIndex;
    char     SourceID[10];
    uint8_t  ProductID;
    uint8_t  ChannelID;
    uint8_t  RetransmitMethod = 0;
  } __attribute__((packed));
  
  struct RefreshRequest { // 15
    MsgHdr   hdr;
    uint32_t SymbolIndex;
    char     SourceID[10];
    uint8_t  ProductID;
    uint8_t  ChannelID;
  } __attribute__((packed));

  struct MessageUnavailable { // 31
    MsgHdr   hdr;
    uint32_t BeginSeqNum;   // The beginning sequence number of the unavailable range of messages.    
    uint32_t EndSeqNum;     // The ending sequence number of the unavailable range of messages.
    uint8_t  ProductID;
    uint8_t  ChannelID;
  } __attribute__((packed));

  // -------------------------------
  inline void printOptionsStatus(const void* pMsg) {
    auto m {reinterpret_cast<const OptionsStatus*>(pMsg)};
    printf ("\t%s:",msgType2str(m->hdr.type).c_str());
    printf ("%d,",m->seriesIndex);
    printf ("\'%c\',",m->seriesStatus);
    printf ("\'%c\',",m->MarketState);
    printf ("\'%c\',",m->HaltCondition);
    printf ("\n");
  }
  // -------------------------------
  inline uint32_t printPkt(const uint8_t* pkt) {
    auto p {pkt};
    auto pktHdr {reinterpret_cast<const PktHdr*>(p)};

    uint64_t ts = pktHdr->seconds * 1e9 + pktHdr->ns;
    printf("%u,",pktHdr->seqNum);
    printf("%s,",ts_ns2str(ts).c_str());
    printf("%s,",deliveryFlag2str(pktHdr->deliveryFlag).c_str());
    printf("PktSize:%u,",pktHdr->pktSize);
    printf("NumMsgs:%u,",pktHdr->numMsgs);
    printf("\n");
    
    p += sizeof(*pktHdr);

    for (auto i = 0; i < pktHdr->numMsgs; i++) {
      auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};

      switch (static_cast<MsgType>(msgHdr->type)) {
	case  MsgType::OptionsStatus : // 51
	  printOptionsStatus(msgHdr);
	  break;
      default:
	printf ("\t%s\n",msgType2str(msgHdr->type).c_str());
      }
      p += msgHdr->size;
    }
    return p - pkt;
  }
} // namespace Plr


#endif
