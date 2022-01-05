#ifndef _EKA_BATSPITCH_MESSAGES_H
#define _EKA_BATSPITCH_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <string>

#include "eka_macros.h"
#include "EkaFhTypes.h"

#define EFH_PITCH_STRIKE_PRICE_SCALE 10
namespace Bats {

  enum class MsgId : uint8_t {
			      TIME = 0x20,
			      ADD_ORDER_LONG = 0x21,
			      ADD_ORDER_SHORT = 0x22,
			      ORDER_EXECUTED = 0x23,
			      ORDER_EXECUTED_AT_PRICE_SIZE = 0x24,
			      REDUCED_SIZE_LONG = 0x25,
			      REDUCED_SIZE_SHORT = 0x26,
			      ORDER_MODIFY_LONG = 0x27,
			      ORDER_MODIFY_SHORT = 0x28,
			      ORDER_DELETE = 0x29,
			      TRADE_LONG = 0x2A,
			      TRADE_SHORT = 0x2B,
			      TRADE_BREAK = 0x2C,
			      END_OF_SESSION = 0x2D,
			      SYMBOL_MAPPING = 0x2E,
			      ADD_ORDER_EXPANDED = 0x2F,
			      TRADE_EXPANDED = 0x30,
			      TRADING_STATUS = 0x31,
			      UNIT_CLEAR = 0x97,
			      TRANSACTION_BEGIN = 0xBC,
			      TRANSACTION_END = 0xBD,

			      LOGIN_REQUEST = 0x01,
			      LOGIN_RESPONSE = 0x02,
			      GAP_REQUEST = 0x03,
			      GAP_RESPONSE = 0x04,
			      SPIN_IMAGE_AVAILABLE = 0x80,
			      SNAPSHOT_REQUEST = 0x81,
			      SNAPSHOT_RESPONSE = 0x82,
			      DEFINITIONS_REQUEST = 0x84,
			      DEFINITIONS_RESPONSE = 0x85,
			      SPIN_FINISHED = 0x83,
			      DEFINITIONS_FINISHED = 0x86,

			      WIDTH_UPDATE = 0xD2,
			      AUCTION_UPDATE = 0x95,
			      OPTIONS_AUCTION_UPDATE = 0xD1,
			      AUCTION_SUMMARY = 0x96,
			      AUCTION_NOTIFICATION = 0xAD,
			      AUCTION_CANCEL = 0xAE,
			      AUCTION_TRADE  = 0xAF,
			      RETAIL_PRICE_IMPROVEMENT = 0x98,
			      SOQ_STRIKE_RANGE_UPDATE = 0x9D,
			      CONSTITUENT_SYMBOL_MAPPING = 0x9E,

			      COMPLEX_INSTRUMENT_DEFINITION_EXPANDED = 0x9A
  };

#define EKA_PRINT_BATS_SYMBOL(x) ((std::string((x),6)).c_str())
#define EKA_PRINT_BATS_SYMBOL_EXP(x) ((std::string((x),8)).c_str())

#define EKA_BATS_PITCH_MSG_DECODE(x)					\
  x == MsgId::SYMBOL_MAPPING                 ? "SYMBOL_MAPPING" :	\
    x == MsgId::TIME                         ? "TIME" :			\
    x == MsgId::ADD_ORDER_LONG               ? "ADD_ORDER_LONG" :	\
    x == MsgId::ADD_ORDER_SHORT              ? "ADD_ORDER_SHORT" :	\
    x == MsgId::ORDER_EXECUTED               ? "ORDER_EXECUTED" :	\
    x == MsgId::ORDER_EXECUTED_AT_PRICE_SIZE ? "ORDER_EXECUTED_AT_PRICE_SIZE" : \
    x == MsgId::REDUCED_SIZE_LONG            ? "REDUCED_SIZE_LONG" :	\
    x == MsgId::REDUCED_SIZE_SHORT           ? "REDUCED_SIZE_SHORT" :	\
    x == MsgId::ORDER_MODIFY_LONG            ? "ORDER_MODIFY_LONG" :	\
    x == MsgId::ORDER_MODIFY_SHORT           ? "ORDER_MODIFY_SHORT" :	\
    x == MsgId::ORDER_DELETE                 ? "ORDER_DELETE" :		\
    x == MsgId::TRADE_LONG                   ? "TRADE_LONG" :		\
    x == MsgId::TRADE_SHORT                  ? "TRADE_SHORT" :		\
    x == MsgId::TRADE_EXPANDED               ? "TRADE_EXPANDED" :	\
    x == MsgId::TRADE_BREAK                  ? "TRADE_BREAK" :		\
    x == MsgId::ADD_ORDER_EXPANDED           ? "ADD_ORDER_EXPANDED" :	\
    x == MsgId::TRADING_STATUS               ? "TRADING_STATUS" :	\
    x == MsgId::WIDTH_UPDATE                 ? "WIDTH_UPDATE" :		\
    x == MsgId::AUCTION_UPDATE               ? "AUCTION_UPDATE" :	\
    x == MsgId::OPTIONS_AUCTION_UPDATE       ? "OPTIONS_AUCTION_UPDATE" : \
    x == MsgId::AUCTION_SUMMARY              ? "AUCTION_SUMMARY" :	\
    x == MsgId::AUCTION_NOTIFICATION         ? "AUCTION_NOTIFICATION" : \
    x == MsgId::AUCTION_CANCEL               ? "AUCTION_CANCEL" :	\
    x == MsgId::AUCTION_TRADE                ? "AUCTION_TRADE" :	\
    x == MsgId::UNIT_CLEAR                   ? "UNIT_CLEAR" :		\
    x == MsgId::RETAIL_PRICE_IMPROVEMENT     ? "RETAIL_PRICE_IMPROVEMENT" : \
    x == MsgId::SOQ_STRIKE_RANGE_UPDATE      ? "SOQ_STRIKE_RANGE_UPDATE" : \
    x == MsgId::CONSTITUENT_SYMBOL_MAPPING   ? "CONSTITUENT_SYMBOL_MAPPING" : \
    x == MsgId::UNIT_CLEAR                   ? "UNIT_CLEAR" :		\
    x == MsgId::TRANSACTION_BEGIN            ? "TRANSACTION_BEGIN" :	\
    x == MsgId::TRANSACTION_END              ? "TRANSACTION_END" :	\
    x == MsgId::COMPLEX_INSTRUMENT_DEFINITION_EXPANDED ? "COMPLEX_INSTRUMENT_DEFINITION_EXPANDED" :	\
    "NON_INTERESTING"

  inline bool operator==(const MsgId lhs, const uint8_t rhs) noexcept {
    return ((uint8_t)lhs) == rhs;
  }
  inline bool operator==(const uint8_t lhs, const MsgId rhs) noexcept {
    return lhs == (uint8_t)rhs;
  }
  inline bool operator!=(const MsgId lhs, const uint8_t rhs) noexcept {
    return ((uint8_t)lhs) != rhs;
  }
  inline bool operator!=(const uint8_t lhs, const MsgId rhs) noexcept {
    return lhs != (uint8_t)rhs;
  }

  /* ------------------------------------------------ */
  static inline EfhTradeStatus tradeAction(EfhTradeStatus prevTradeAction, char msgTradeStatus) {
    switch (msgTradeStatus) {
    case 'A' : // Accepting Orders for Queuing
      break; // To be confirmed!!!
    case 'Q' : // Quote-Only
      return EfhTradeStatus::kPreopen;
    case 'R' : // Opening-Rotation
      return EfhTradeStatus::kOpeningRotation;
    case 'H' : // Halted
    case 'S' : // Exchange Specific Suspension
      return EfhTradeStatus::kHalted;
    case 'T' : // Trading
      return EfhTradeStatus::kNormal;
    default:
      on_error("Unexpected trade status \'%c\'",msgTradeStatus);
    }
    return prevTradeAction;
  }
  /* ------------------------------------------------ */
  inline int checkPriceLengh(uint64_t price,int priceScale = 1) {
    if (((price / priceScale) & 0xFFFFFFFF00000000) != 0) return -1;
    return 0;
  }

  /* ------------------------------------------------ */
  inline FhOrderType addFlags2orderType(uint8_t flags) {
    switch (flags) {
      /* case 0x01 : // bit #0 */
      /*   return FhOrderType::BD; */
    case 0x08 : // bit #3
    case 0x09 : // bit #0 & #3
      return FhOrderType::BD_AON;
    default:
      return FhOrderType::OTHER;
    }
  }
  /* ------------------------------------------------ */
  inline FhOrderType addFlagsCustomerIndicator2orderType(uint8_t flags, char customerIndicator) {
    if (customerIndicator == 'C') {
      switch (flags) {
	/* case 0x01 : // bit #0 */
	/*   return FhOrderType::BD; */
      case 0x08 : // bit #3
      case 0x09 : // bit #0 & #3
	return FhOrderType::CUSTOMER_AON;
      default:
	return FhOrderType::CUSTOMER;
      }
    } else {
      switch (flags) {
	/* case 0x01 : // bit #0 */
	/*   return FhOrderType::BD; */
      case 0x08 : // bit #3
      case 0x09 : // bit #0 & #3
	return FhOrderType::BD_AON;
      default:
	return FhOrderType::BD;
      }
    }
  }

  /* ------------------------------------------------ */

  inline uint32_t normalize_bats_symbol_char(char c) {
    if (c >= '0' && c <= '9') return (uint32_t)(0  + (c - '0')); // 0..9
    if (c >= 'A' && c <= 'Z') return (uint32_t)(10 + (c - 'A')); // 10..35
    if (c >= 'a' && c <= 'z') return (uint32_t)(36 + (c - 'a')); // 36..61
    on_error ("Unexpected symbol |%c|",c);
  }
  /* ------------------------------------------------ */

  inline uint32_t bats_symbol2optionid (const char* s, uint symbol_size) {
    uint64_t compacted_id = 0;
    if (s[0] != '0') on_error("%c%c%c%c%c%c doesnt have \"0\" prefix",s[0],s[1],s[2],s[3],s[4],s[5]);
    for (uint i = 1; i < symbol_size; i++) {
      //    printf ("s[%i] = %c, normalize_bats_symbol_char = %u = 0x%x\n",i,s[i], normalize_bats_symbol_char(s[i]), normalize_bats_symbol_char(s[i]));
      compacted_id |= normalize_bats_symbol_char(s[i]) << ((symbol_size - i - 1) * symbol_size);
    }
    if ((compacted_id & 0xffffffff00000000) != 0) on_error("%c%c%c%c%c%c encoding produced 0x%jx exceeding 32bit",s[0],s[1],s[2],s[3],s[4],s[5],compacted_id);
    return (uint32_t) (compacted_id & 0x00000000ffffffff);
  }

  /* ------------------------------------------------ */

  inline SideT sideDecode(char _side) {
    switch (_side) {
    case 'B' :
      return SideT::BID;
    case 'S' :
      return SideT::ASK;
    default:
      on_error("Unexpected Side \'%c\'",_side);
    }
  }
  /* ------------------------------------------------ */

  inline EfhOrderSide efhMsgSideDecode(char _side) {
    switch (_side) {
    case 'B' :
      return EfhOrderSide::kBid;
    case 'S' :
      return EfhOrderSide::kAsk;
    default:
      on_error("Unexpected Side \'%c\'",_side);
    }
  }
  /* ------------------------------------------------ */

  struct sequenced_unit_header { // 
    uint16_t	length;   // 2 -- length of entire block of messages including this header
    uint8_t       count;    // 1 -- number of messages to follow this header
    uint8_t       unit;     // 1 -- Unit to applies to messages included in this header
    uint32_t	sequence; // 4 -- Sequence of first message to follow this header
  } __attribute__((packed));

#define EKA_BATS_MSG_CNT(x)  ((((sequenced_unit_header*)x)->count))
#define EKA_BATS_SEQUENCE(x) ((((sequenced_unit_header*)x)->sequence))
#define EKA_BATS_UNIT(x)     ((((sequenced_unit_header*)x)->unit))

  struct DummyHeader { // presents at all messages
    uint8_t	length;   // 1
    uint8_t       type;     // 1
  }__attribute__((packed));

  struct GenericHeader { // presents at all messages excepting "Symbol Mapping"
    uint8_t	length;   // 1
    uint8_t       type;     // 1
    uint32_t	time;     // 4
  } __attribute__((packed));

  struct trading_status { // 0x31
    struct GenericHeader header;
    char          symbol[6];      // 6 right padded with spaces
    char          reserved1[2];   // 2
    char          trading_status; // 1
    /* 'H' - Halted */
    /* 'Q' - Quote-Only */
    /* 'R' - Opening Rotation */
    /* 'T' - Trading */
    char          reserved2;     // 1 

    char          gth_trading_status;   // 1 C1 Only !!!
                                        /* 'H' - Halted */
                                        /* 'Q' - Quote-Only */
                                        /* 'T' - Trading */  
    char          reserved3;   // 1
  } __attribute__((packed));

  struct time { // 0x20
    struct GenericHeader header;
  } __attribute__((packed));

  struct unit_clear { // 0x97
    struct GenericHeader header;
  } __attribute__((packed));

  struct transaction_begin { // 0xBC
    struct GenericHeader header;
  } __attribute__((packed));

  struct transaction_end { // 0xBD
    struct GenericHeader header;
  } __attribute__((packed));

  struct end_of_session { // 0x2D
    struct GenericHeader header;
  } __attribute__((packed));

  struct add_order_long { // 0x21
    struct GenericHeader header;
    uint64_t	order_id; // 8
    char		side;     // 1 'B' or 'S'
    uint32_t	size;     // 4
    char          symbol[6];// 6  right padded with spaces
    uint64_t	price;    // 8
    uint8_t	flags;    // 1
  } __attribute__((packed));

  struct add_order_short { // 0x22
    struct GenericHeader header;
    uint64_t	order_id; // 8
    char		side;     // 1 'B' or 'S'
    uint16_t	size;     // 2
    char          symbol[6];// 6  right padded with spaces
    uint16_t	price;    // 2
    uint8_t	flags;    // 1
  } __attribute__((packed));

  struct add_order_expanded { // 0x2F
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    char		side;               // 1 'B' or 'S'
    uint32_t	size;               // 4
    char          exp_symbol[8];      // 8  right padded with spaces
    uint64_t	price;              // 8
    uint8_t	flags;              // 1
    char          participant_id[4];  // 4
    char          customer_indicator; // 1 'N' - Non-Customer, 'C' - Customer
    char          client_id[4];       // 4
  } __attribute__((packed));

  struct order_executed { // 0x23
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    uint32_t	executed_size;      // 4
    uint64_t	execution_id;       // 8
    char  	trade_condition;    // 1
    /* ' ' (Space) - Normal Trade */
    /* 'O' - Opening trade */
    /* 'S' - Spread trade */
    /* 'A' - SPIM trade */
    /* 'I' - ISO trade */
    /* 'K' - Cabinet trade */
  } __attribute__((packed));

  struct order_executed_at_price_size { // 0x24
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    uint32_t	executed_size;      // 4
    uint32_t	remaining_size;     // 4 -- IGNORED IN THE BOOK LOGIC!!!
    uint64_t	execution_id;       // 8
    uint64_t	price;              // 8
    char  	trade_condition;    // 1
    /* ' ' (Space) - Normal Trade */
    /* 'O' - Opening trade */
    /* 'S' - Spread trade */
    /* 'A' - SPIM trade */
    /* 'I' - ISO trade */
    /* 'K' - Cabinet trade */
  } __attribute__((packed));

  struct reduced_size_long { // 0x25
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    uint32_t	canceled_size;      // 4
  } __attribute__((packed));

  struct reduced_size_short { // 0x26
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    uint16_t	canceled_size;      // 2
  } __attribute__((packed));

  struct order_modify_long { // 0x27
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    uint32_t	size;               // 4 (= new size)
    uint64_t	price;              // 8 (= new price)
    uint8_t  	flags;              // 1
    /* bit 0 - Display: '0' - not aggregated, '1' - aggregated */
    /*   bit 1 - Maintain Priority: '0' - Reset Priority, '1' - Maintain Priority */
    /*   bits 2-7 - Reserved */
  } __attribute__((packed));

  struct order_modify_short { // 0x28
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    uint16_t	size;               // 2 (= new size)
    uint16_t	price;              // 2 (= new price)
    uint8_t  	flags;              // 1
    /* bit 0 - Display: '0' - not aggregated, '1' - aggregated */
    /*   bit 1 - Maintain Priority: '0' - Reset Priority, '1' - Maintain Priority */
    /*   bits 2-7 - Reserved */
  } __attribute__((packed));

  struct order_delete { // 0x29
    struct GenericHeader header;
    uint64_t	order_id;           // 8
  } __attribute__((packed));

  struct TradeLong { // 0x2A
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    char		side;               // 1 Always 'B'
    uint32_t	size;               // 4
    char          symbol[6];          // 6  right padded with spaces
    uint64_t	price;              // 8
    uint64_t	execution_id;       // 8
    char  	trade_condition;    // 1
    /* ' ' (Space) - Normal Trade */
    /* 'O' - Opening trade */
    /* 'S' - Spread trade */
    /* 'A' - SPIM trade */
    /* 'I' - ISO trade */
    /* 'K' - Cabinet trade */
  } __attribute__((packed));

  struct TradeShort { // 0x2B
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    char		side;               // 1 Always 'B'
    uint16_t	size;               // 2
    char          symbol[6];          // 6  right padded with spaces
    uint16_t	price;              // 2
    uint64_t	execution_id;       // 8
    char  	trade_condition;    // 1
    /* ' ' (Space) - Normal Trade */
    /* 'O' - Opening trade */
    /* 'S' - Spread trade */
    /* 'A' - SPIM trade */
    /* 'I' - ISO trade */
    /* 'K' - Cabinet trade */
  } __attribute__((packed));

  struct TradeExpanded { // 0x30
    struct GenericHeader header;
    uint64_t	order_id;           // 8
    char		side;               // 1 Always 'B'
    uint32_t	size;               // 4
    char          symbol[8];          // 8  right padded with spaces
    uint64_t	price;              // 8
    uint64_t	execution_id;       // 8
    char  	trade_condition;    // 1
    /* ' ' (Space) - Normal Trade */
    /* 'O' - Opening trade */
    /* 'S' - Spread trade */
    /* 'A' - SPIM trade */
    /* 'I' - ISO trade */
    /* 'K' - Cabinet trade */
  } __attribute__((packed));

  struct trade_break { // 0x2C
    struct GenericHeader header;
    uint64_t	execution_id;       // 8
  } __attribute__((packed));

  struct SymbolMapping { // 0x2E
    uint8_t	length;           // 1
    uint8_t       type;             // 1
    char          symbol[6];        // 6  right padded with spaces
    char          osi_symbol[21];   // 21
    char          symbol_condition; // 1  'N' - Normal, 'C' - Closing Only
    char          underlying[8];    // 8  right padded with spaces
  } __attribute__((packed));

  //-----------------------------------------------
  // RFQ messages
  //-----------------------------------------------

  struct AuctionNotification { // 0xAD
    GenericHeader header;
    char        symbol[6];
    uint64_t    auctionId;
    char        auctionType; // 'B' = Bats Auction Mechanism (BAM) (EDGX Only) or AIM (C1 Only)
                             // 'S' = Solicitation Auction Mechanism (C1 Only)
                             // 'T' = Step Up Mechanism (SUM)
                             // 'A' = SUM All or None
    char        side;  // 'B' or 'S'      
    uint64_t    price;
    uint32_t    contracts;
    char        customerIndicator; // 'N' = Non-Customer 'C' = Customer
    uint32_t    participantId;
    uint32_t    auctionEndOffset; // Nanosecond offset from last timestamp
    uint32_t    clientId;
  } __attribute__((packed));

  struct OptionsAuctionUpdate { // 0xD1
    GenericHeader header;
    char        symbol[8];
    char        auctionType;
    uint64_t    referencePrice;
    uint32_t    buyContracts;
    uint32_t    sellContracts;
    uint64_t    indicativePrice;
    uint64_t    auctionOnlyPrice;
    char        openningCondition;
    uint64_t    compositeMarketBidPrice;
    uint64_t    compositeMarketOfferPrice;
  } __attribute__((packed));

  struct AuctionCancel { // 0xAE
    GenericHeader header;
    uint64_t    auctionId;
  } __attribute__((packed));

  inline EfhOrderCapacity getRfqCapacity(char customerIndicator) {
    switch (customerIndicator) {
    case 'N' : return EfhOrderCapacity::kBrokerDealer;
    case 'C' : return EfhOrderCapacity::kCustomer;
    default  : on_error("Unexpected customerIndicator\'%c\'",customerIndicator);
    }
  }
  
  //-----------------------------------------------

  inline EfhSecurityType getComplexSecurityType(char type) {
    switch (type) {
    case 'O' : return EfhSecurityType::kOption;
    case 'E' : return EfhSecurityType::kStock;
    default  : on_error("Unexpected LegSecurityType \'%c\'",type);
    }
  }
  
  struct ComplexInstrumentDefinitionExpanded_leg {
    char     LegSymbol[8];
    uint32_t LegRatio;
    char     LegSecurityType; // 'O' = Leg is an Option instrument
                              //  E = Leg is an Equity instrument
  } __attribute__((packed));

  struct ComplexInstrumentDefinitionExpanded_root {
    GenericHeader header;
    char ComplexInstrumentId[6];
    char ComplexInstrumentUnderlying[8];
    char ComplexInstrumentType[4]; // 4 character field; each field describes a characteristic.
                                   // Character 1: Complex Option Type
                                   //  'O' = All legs are options
                                   //  'E' = One leg is an equity leg
                                   // Characters 2-4: Reserved
    uint8_t LegCount;
    
  } __attribute__((packed));


  //-----------------------------------------------
  // SPIN and GRP messages
  //-----------------------------------------------

  struct LoginRequest { // 0x01 (same for GRP and Spin)
    struct sequenced_unit_header hdr;
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    char          session_sub_id[4];  // 4
    char          username[4];        // 4
    char          filler[2];          // 2
    char          password[10];       // 10
  } __attribute__((packed));

  struct LoginResponse { // 0x02 (same for GRP and Spin)
    struct sequenced_unit_header hdr;
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    char          status;             // 1
    /* 'A' - Login Accepted */
    /* 'N' - Not authorized (Invalid Username/Password) */
    /* 'B' - Session in use */
    /* 'S' - Invalid Session */
  } __attribute__((packed));

  struct GapRequest { // 0x03
    struct sequenced_unit_header hdr;
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    uint8_t       unit;               // 1
    uint32_t      sequence;           // 4
    uint16_t      count;              // 2
  } __attribute__((packed));

  struct GapResponse { // 0x04
    //  struct sequenced_unit_header hdr;
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    uint8_t       unit;               // 1
    uint32_t      sequence;           // 4
    uint16_t      count;              // 2
    char          status;             // 1
    /* 'A' - Accepted */
    /* 'O' - Out of range (ahead of sequence or too far behind) */
    /* 'D' - daily gap request allocation exhausted */
    /* 'M' - Minute gap request allocation exhausted */
    /* 'S' - Second gap request allocation exhausted */
    /* 'C' - Count request limit for one gap request is exceeded */
    /* 'I' - Invalid Unit specified in request */
    /* 'U' - Unit is currently unavailable */
  } __attribute__((packed));

  struct SpinImageAvailable { // 0x80
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    uint32_t      sequence;           // 4
  } __attribute__((packed));

  struct SpinResponse { // 0x82 for snapshot
    // 0x85 for defintions
    //  struct sequenced_unit_header hdr;
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    uint32_t      sequence;           // 4 Sequence number from spin_image_available or '0' for Definitions
    uint32_t      count;              // 4 Number of "Add Order" or Definitions messages which will be contained in this spin
    char          status;             // 1
    /* 'A' - Accepted */
    /* 'O' - Out of range  */
    /* 'S' - Spin already in progress */
  } __attribute__((packed));

  struct SpinFinished { // 0x83
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    uint32_t      sequence;           // 4 Sequence number from spin_request
  } __attribute__((packed));

  struct SpinRequest { // 0x84 for instrument_definition
    // 0x81 for snapshot
    struct sequenced_unit_header hdr;
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
    uint32_t      sequence;           // 4 Must be '0' for instrument_definition, from spin_image_available for snapshot
  } __attribute__((packed));

  struct instrument_definition_finished { // 0x86
    uint8_t	length;             // 1
    uint8_t       type;               // 1 
  } __attribute__((packed));


  /* ------------------------------------------------ */
  // converting 6 char CBOE symbol to uint64_t
  inline uint64_t symbol2secId(const char* s) {
    return be64toh(*(uint64_t*)(s - 2)) & 0x0000ffffffffffff;
  }
  inline uint64_t expSymbol2secId(const char* s) {
    if (s[6] != ' ' || s[7] != ' ')
      on_error("ADD_ORDER_EXPANDED message with \'%c%c%c%c%c%c%c%c\' symbol (longer than 6 chars) not supported",
	       s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7]);
    return be64toh(*(uint64_t*)(s - 2)) & 0x0000ffffffffffff;
  }

  inline EfhOrderSide getSide(char side) {
    switch (side) {
    case 'B' : return EfhOrderSide::kBid;
    case 'S' : return EfhOrderSide::kAsk;
    default  : on_error("Unexpected side \'%c\'",side);
    }
  }
} // namespace Bats

#define EKA_BATS_TRADE_COND(x)		 \
  x == ' ' ? EfhTradeCond::kREG :	 \
    x == 'I' ? EfhTradeCond::kISOI :	 \
    EfhTradeCond::kUnmapped


#define EKA_BATS_TRADE_STAT(x)   (x == 'T' ? EfhTradeStatus::kNormal : EfhTradeStatus::kHalted)

#endif
