#ifndef MRX2TOP_MESSAGES_H
#define MRX2TOP_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define EFH_GEMX_STRIKE_PRICE_SCALE 10000

#define EKA_GEM_TS(x) (be64toh(*(uint64_t*)(x-1) & 0xffffffffffff0000))

namespace Mrx2Top {

  class MsgType : char {
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
      EndOfSnapshot            = 'M' 
      };
  
  struct GenericHdr { // Dummy
    char	  type;        // 1
    uint16_t      trackingNum; // 2 Integer Internal system tracking number
    uint64_t      ts;          // 8 Nanoseconds since midnight
  } __attribute__((packed));

  static inline uint64_t getTs(const uint8_t* m) {
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
    uint16_t size;
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

  
  struct option_directory {
    char         message_type; // 'D'
    char         time_nano[6];
    uint32_t     option_id;
    char         security_symbol[6];
    uint8_t      expiration_year; 
    uint8_t      expiration_month;
    uint8_t      expiration_day;
    uint64_t     strike_price;
    char         option_type; // 'C' or 'P'
    uint8_t      source;
    char         underlying_symbol[13];
    char         trading_type;    // “E” = Equity “I” = Index “F” = ETF “C” = Currency
    uint16_t     contract_size;
    char         option_closing_type; // “N” = Normal Hours “L” = Late Hours
    char         tradable; // “Y” = Option is tradable “N” = Option is not tradable
    char         mpv; // “E” = penny Everywhere “S” = Scaled “P” = penny Pilot
    char         closing_only; // Closing position of the option: 
    // “Y” = Option is Closing Position Only. 
    // Only Market Maker origin orders can have open position 
    // “N” = Option is not Closing Position Only
  } __attribute__((packed));


  struct trading_action {
    char     message_type; // 'H'
    char     time_nano[6];
    uint32_t option_id;
    char     current_trading_state; // “H” = Halt in effect, “T” = Trading on the options system
  } __attribute__((packed));

  struct security_open_close {
    char     message_type; // 'O'
    char     time_nano[6];
    uint32_t option_id;
    char     open_state; // Y = Open for auto execution, N = Closed for auto execution
  } __attribute__((packed));

  struct opening_imbalance {
    char     message_type; // 'N'
    char     time_nano[6];
    uint32_t option_id;
    uint32_t paired_contracts;
    char     imbalance_direction; // “B” = buy imbalance “S” = sell imbalance
    uint32_t imbalance_price;
    uint32_t imbalance_volume;
  } __attribute__((packed));


} //namespece Mrx2top

#endif
