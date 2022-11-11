#ifndef _EKA_FH_NOM_MESSAGES_H
#define _EKA_FH_NOM_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "eka_macros.h"

//#define EFH_NOM_STRIKE_PRICE_SCALE 10
#define EFH_NOM_STRIKE_PRICE_SCALE 1
#define EFH_NOM_PRICE_SCALE 1

#define EKA_NOM_TS(x) (be64toh(*(uint64_t*)(x+1) & 0xffffffffffff0000))

class NomFeed {
 public:
  struct GenericHdr { // Dummy
    char	  type;            // 1
    uint16_t      trackingNum;     // 2
    char	  timeNano[6];     // 6
  } __attribute__((packed));

  static inline uint64_t getTs(const uint8_t* m) {
    uint64_t ts_tmp = 0;
    auto hdr = reinterpret_cast<const GenericHdr*>(m);
    memcpy((uint8_t*)&ts_tmp+2,hdr->timeNano,sizeof(hdr->timeNano));
    return be64toh(ts_tmp);
  }

 struct SystemEvent { // 'S'
   GenericHdr hdr;        // 9
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
    GenericHdr hdr;        // 9
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

 struct Directory { // 'R'
    GenericHdr hdr;        // 9
    uint32_t instrumentId; // 4
    char     symbol[6];    // 6
    uint8_t  expYear;      // 1
    uint8_t  expMonth;     // 1
    uint8_t  expDate;      // 1
    uint32_t price;        // 4 Explicit strike price
    char     optionType;   // 1 “C” = Call option “P” = Put option “N”= N/A
    uint8_t  source;       // 1
    char     underlying[13];//13
    char     closingType;  // 1 “N” = Normal Hours “L” = Late Hours
    char     tradable;     // 1 “Y” = Instrument is tradable “N” = Instrument is not tradable
    char     mpv;          // 1 “E” = penny Everywhere “S” = Scaled “P” = penny Pilot
  } __attribute__((packed));

  struct OptionOpen { // 'O'
    GenericHdr hdr;        // 9
    uint32_t instrumentId; // 4
    char     state;        // 1 Y = Open for auto execution
                           //   N = Closed for auto execution
  } __attribute__((packed));

 struct AddOrderShort { // 'a'
   GenericHdr hdr;        // 9
   uint64_t orderId;      // 8
   char     side;         // 1 “B” = Buy “S” = Sell 

   uint32_t instrumentId; // 4
   uint16_t price;        // 2 When converted to a decimal format,
   //   this price is in fixed point format with 3 whole 
   //   number places followed by 2 decimal digits.
   uint16_t volume;       // 2
 } __attribute__((packed));

  struct AddOrderLong { // 'A'
   GenericHdr hdr;        // 9
   uint64_t orderId;      // 8
   char     side;         // 1 “B” = Buy “S” = Sell 

   uint32_t instrumentId; // 4
    uint32_t price;       // 4
   uint32_t volume;       // 4
 } __attribute__((packed));
  
 struct AddQuoteShort { // 'j'
    GenericHdr hdr;        // 9
    uint64_t bidOrderId;   // 8
    uint64_t askOrderId;   // 8
    uint32_t instrumentId; // 4
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
    GenericHdr hdr;        // 9
    uint64_t bidOrderId;   // 8
    uint64_t askOrderId;   // 8
    uint32_t instrumentId; // 4
    uint32_t bidPrice;     // 4
    uint32_t bidSize;      // 4
    uint32_t askPrice;     // 4
    uint32_t askSize;      // 4
  } __attribute__((packed));

 struct OrderExecuted { // 'E'
    GenericHdr hdr;        // 9
    uint64_t orderId;      // 8
    uint32_t volume;       // 4 The total quantity executed
    uint32_t crossNumber;  // 4 Trade Group Id. Ties together all trades of
                           //   a given atomic transaction in the matching engine.
    uint32_t matchNumber;  // 4 Execution Id. Identifies the component of an execution.
                           //   Unique for a given day. The match number is also
                           //   referenced in the Trade Break Message.
  } __attribute__((packed));

  struct executed_price {  // 'C'
    char          type;             // 1
    uint16_t      tracking_num;     // 2
    char	        time_nano[6];     // 6
    uint64_t	order_reference_delta; // 8
    uint32_t	cross_number;     // 4
    uint32_t	match_number;     // 4
    char		printable;        // 1
    uint32_t	price;            // 4
    uint32_t	size;             // 4
  } __attribute__((packed));

   struct OrderExecutedPrice { // 'C'
    GenericHdr hdr;        // 9
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
    GenericHdr hdr;        // 9
    uint64_t orderId;      // 8
    uint32_t volume;       // 4 The total quantity executed
  } __attribute__((packed));

 struct ReplaceOrderShort { // 'u'
    GenericHdr hdr;        // 9
    uint64_t oldOrderId;   // 8
    uint64_t newOrderId;   // 8
    uint16_t price;        // 2 When converted to a decimal format,
                           //   this price is in fixed point format with 3 whole 
                           //   number places followed by 2 decimal digits.
    uint16_t volume;       // 2
  } __attribute__((packed));

  struct ReplaceOrderLong { // 'U'
    GenericHdr hdr;
    uint64_t oldOrderId;   // 8
    uint64_t newOrderId;   // 8
    uint32_t price;        // 4 When converted to a decimal format,
                           //   this price is in fixed point format with 6 whole 
                           //   number places followed by 4 decimal digits.
    uint32_t volume;       // 4
  } __attribute__((packed));

 struct SingleSideDelete{ // 'D'
    GenericHdr hdr;        // 11
    uint64_t orderId;      // 8
  } __attribute__((packed));

 struct SingleSideUpdate { // 'G'
   GenericHdr hdr;        // 9
   uint64_t orderId;      // 8
   char     reason;       // 1 ‘U’-USER, ‘R’-REPRICE, ‘S’-SUSPEND
   uint32_t price;        // 4 When converted to a decimal format,
   //   this price is in fixed point format with 6 whole 
   //   number places followed by 4 decimal digits.
   uint32_t volume;       // 4
 } __attribute__((packed));

  struct QuoteReplaceShort { // 'k'
    GenericHdr hdr;        // 9
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
    GenericHdr hdr;        // 9
    uint64_t bidOrderId;   // 8
    uint64_t askOrderId;   // 8
  } __attribute__((packed));

  struct OptionTrade { // 'P'
    GenericHdr hdr;        // 9
    uint8_t  buySell;      // 1
    uint32_t instrumentId; // 4
    uint32_t crossNumber;  // 4 
    uint32_t matchNumber;  // 4 
    uint32_t price;        // 4 
    uint32_t volume;       // 4 The total quantity executed

  } __attribute__((packed));

   struct CrossTrade { // 'Q'
    GenericHdr hdr;        // 9
    uint32_t instrumentId; // 4
    uint32_t crossNumber;  // 4
    uint32_t matchNumber;  // 4
    char     crossType;    // 1 “A”= All Auctions
                           //   “P”= Price Improvement (PRISM) Auction
    uint32_t price;        // 4 
    uint32_t volume;       // 4 The total quantity executed
  } __attribute__((packed));

  struct BrokenTrade { // 'B'
    GenericHdr hdr;        // 9
    uint32_t crossNumber;  // 4
    uint32_t matchNumber;  // 4
  } __attribute__((packed));

 struct NOII { // 'I'
   GenericHdr hdr;        // 9
   uint32_t auctionId;    // 4 
   char     auctionType;  // 1 “O” = Opening
   //   “R” = Reopening
   //   “P” = Price Improvement (PRISM) Auction
   //   “I” = Order Exposure

   uint32_t pairedQuantity;// 4
                           //    The total amount that are
                           //    eligible to be matched
                           //    at the Current Reference Price.
                           //    0 for Price Improvement (PRISM) Auction.
   char     imbalanceDirection;
                           // 1  Indicates the market side of the
                           //    imbalance “B” = buy imbalance
                           //              “S” = sell imbalance
   uint32_t instrumentId;  // 4
   uint32_t imbalancePrice;// 4 The imbalance price.
                           //   0 for Price Improvement (PRISM) Auction.
   uint32_t imbalanceVolume;// 4
   char     customerFirmIndicator;
   // 1 For Order Exposure (Auction Type=I) only.
   //   Indicates the order capacity:
   //   “C” = Customer
   //   “F” = Firm/ Joint Back Office (JBO)
   //   “M” = On-floor Market Maker
   //   “P” = Professional Customer
   //   “B” = Broker Dealer/ Non Registered Market Maker
   char     reserved[3];
 } __attribute__((packed));

 struct EndOfSnapshot { // 'M'
    char          type;             // 1
    char          nextLifeSequence[20];    
  } __attribute__((packed));


  inline size_t getMsgLen(const unsigned char* m) {
    return be16toh(*(uint16_t*)(m-2));
  }


  inline void printMsg(FILE* md_file, const uint8_t* m, int gr, uint64_t sequence, uint64_t ts) {
#if 0  
    fprintf (md_file,"GR%d,%s,%ju,\'%c\',",
	     gr,ts_ns2str(ts).c_str(),sequence,(char)m[0]);
    switch ((char)m[0]) {
    case 'a': { //NOM_ADD_ORDER_SHORT
      auto message {reinterpret_cast<const add_order_short *>(m)};

      fprintf (md_file,"SID:%16u,%c,P:%8u,S:%8u\n",
	       be32toh (message->option_id),
	       (char)             (message->side),
	       (uint32_t) be16toh (message->price) * 100 / EFH_NOM_PRICE_SCALE,
	       (uint32_t) be16toh (message->size)
	       );
      break;
    }
      //--------------------------------------------------------------
    case 'A' : { //NOM_ADD_ORDER_LONG
      fprintf (md_file,"GR%d,SN:%ju,",gr,sequence);
      auto message {reinterpret_cast<const add_order_long *>(m)};
    
      fprintf (md_file,"SID:%16u,%c,P:%8u,S:%8u\n",
	       be32toh (message->option_id),
	       (char)             (message->side),
	       be32toh (message->price) / EFH_NOM_PRICE_SCALE,
	       be32toh (message->size)
	       );
      break;
    }
      //--------------------------------------------------------------
    case 'J': {  //NOM_ADD_QUOTE_LONG
      auto message {reinterpret_cast<const add_quote_long *>(m)};

      fprintf (md_file,"SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u",
	       be32toh (message->option_id),
	       be64toh (message->bid_reference_delta),
	       be32toh (message->bid_price) / EFH_NOM_PRICE_SCALE,
	       be32toh (message->bid_size),
	       be64toh (message->ask_reference_delta),
	       be32toh (message->ask_price) / EFH_NOM_PRICE_SCALE,
	       be32toh (message->ask_size)
	       );
      break;
    }
    case 'j': { //NOM_ADD_QUOTE_SHORT
      auto message {reinterpret_cast<const add_quote_short *>(m)};

      fprintf (md_file,"SID:%16u,BOID:%16ju,BP:%8u,BS:%8u,AOID:%16ju,AP:%8u,AS:%8u",
	       be32toh (message->option_id),
	       be64toh (message->bid_reference_delta),
	       (uint32_t) be16toh (message->bid_price) / EFH_NOM_PRICE_SCALE,
	       (uint32_t) be16toh (message->bid_size),
	       be64toh (message->ask_reference_delta),
	       (uint32_t) be16toh (message->ask_price) / EFH_NOM_PRICE_SCALE,
	       (uint32_t) be16toh (message->ask_size)
	       );

      break;
    }     
    default:
      break; 
    }
    fprintf (md_file,"\n");
    fflush(md_file);
#endif    
    return;

  }
}; // Class Nom

#endif
