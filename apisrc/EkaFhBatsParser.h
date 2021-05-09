#ifndef _EKA_BATSPITCH_MESSAGES_H
#define _EKA_BATSPITCH_MESSAGES_H
#include <stdint.h>
#include <unistd.h>
#include <string>


#define EFH_PITCH_STRIKE_PRICE_SCALE 10

enum class EKA_BATS_PITCH_MSG : uint8_t {
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
    DEFINITIONS_FINISHED = 0x86
    };

#define EKA_PRINT_BATS_SYMBOL(x) ((std::string((x),6)).c_str())
#define EKA_PRINT_BATS_SYMBOL_EXP(x) ((std::string((x),8)).c_str())

#define EKA_BATS_PITCH_MSG_DECODE(x)					\
  x == EKA_BATS_PITCH_MSG::SYMBOL_MAPPING                 ? "SYMBOL_MAPPING" : \
    x == EKA_BATS_PITCH_MSG::ADD_ORDER_LONG               ? "ADD_ORDER_LONG" : \
    x == EKA_BATS_PITCH_MSG::ADD_ORDER_SHORT              ? "ADD_ORDER_SHORT" :	\
    x == EKA_BATS_PITCH_MSG::ORDER_EXECUTED               ? "ORDER_EXECUTED" : \
    x == EKA_BATS_PITCH_MSG::ORDER_EXECUTED_AT_PRICE_SIZE ? "ORDER_EXECUTED_AT_PRICE_SIZE" : \
    x == EKA_BATS_PITCH_MSG::REDUCED_SIZE_LONG            ? "REDUCED_SIZE_LONG" : \
    x == EKA_BATS_PITCH_MSG::REDUCED_SIZE_SHORT           ? "REDUCED_SIZE_SHORT" : \
    x == EKA_BATS_PITCH_MSG::ORDER_MODIFY_LONG            ? "ORDER_MODIFY_LONG" : \
    x == EKA_BATS_PITCH_MSG::ORDER_MODIFY_SHORT           ? "ORDER_MODIFY_SHORT" : \
    x == EKA_BATS_PITCH_MSG::ORDER_DELETE                 ? "ORDER_DELETE" : \
    x == EKA_BATS_PITCH_MSG::TRADE_LONG                   ? "TRADE_LONG" : \
    x == EKA_BATS_PITCH_MSG::TRADE_SHORT                  ? "TRADE_SHORT" : \
    x == EKA_BATS_PITCH_MSG::TRADE_BREAK                  ? "TRADE_BREAK" : \
    x == EKA_BATS_PITCH_MSG::ADD_ORDER_EXPANDED           ? "ADD_ORDER_EXPANDED" : \
    x == EKA_BATS_PITCH_MSG::TRADING_STATUS               ? "TRADING_STATUS" : \
    "NON_INTERESTING"

enum class EKA_BATS_PITCH_CONSTANTS : uint {
  SYMBOL_SIZE = 6
};

inline bool operator==(const EKA_BATS_PITCH_MSG lhs, const uint8_t rhs) noexcept {
  return ((uint8_t)lhs) == rhs;
}
inline bool operator==(const uint8_t lhs, const EKA_BATS_PITCH_MSG rhs) noexcept {
  return lhs == (uint8_t)rhs;
}
inline bool operator!=(const EKA_BATS_PITCH_MSG lhs, const uint8_t rhs) noexcept {
  return ((uint8_t)lhs) != rhs;
}
inline bool operator!=(const uint8_t lhs, const EKA_BATS_PITCH_MSG rhs) noexcept {
  return lhs != (uint8_t)rhs;
}
/* inline bool operator==(const EKA_SPIN_GRP_MSG lhs, const uint8_t rhs) noexcept { */
/*   return ((uint8_t)lhs) == rhs; */
/* } */
/* inline bool operator==(const uint8_t lhs, const EKA_SPIN_GRP_MSG rhs) noexcept { */
/*   return lhs == (uint8_t)rhs; */
/* } */
/* inline bool operator!=(const EKA_SPIN_GRP_MSG lhs, const uint8_t rhs) noexcept { */
/*   return ((uint8_t)lhs) != rhs; */
/* } */
/* inline bool operator!=(const uint8_t lhs, const EKA_SPIN_GRP_MSG rhs) noexcept { */
/*   return lhs != (uint8_t)rhs; */
/* } */

struct batspitch_sequenced_unit_header { // 
  uint16_t	length;   // 2 -- length of entire block of messages including this header
  uint8_t       count;    // 1 -- number of messages to follow this header
  uint8_t       unit;     // 1 -- Unit to applies to messages included in this header
  uint32_t	sequence; // 4 -- Sequence of first message to follow this header
} __attribute__((packed));

#define EKA_BATS_MSG_CNT(x)  ((((batspitch_sequenced_unit_header*)x)->count))
#define EKA_BATS_SEQUENCE(x) ((((batspitch_sequenced_unit_header*)x)->sequence))
#define EKA_BATS_UNIT(x)     ((((batspitch_sequenced_unit_header*)x)->unit))

struct batspitch_dummy_header { // presents at all messages
  uint8_t	length;   // 1
  uint8_t       type;     // 1
 }__attribute__((packed));

struct batspitch_generic_header { // presents at all messages excepting "Symbol Mapping"
  uint8_t	length;   // 1
  uint8_t       type;     // 1
  uint32_t	time;     // 4
} __attribute__((packed));

struct batspitch_trading_status { // 0x31
  struct batspitch_generic_header header;
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

struct batspitch_time { // 0x20
  struct batspitch_generic_header header;
} __attribute__((packed));

struct batspitch_unit_clear { // 0x97
  struct batspitch_generic_header header;
} __attribute__((packed));

struct batspitch_transaction_begin { // 0xBC
  struct batspitch_generic_header header;
} __attribute__((packed));

struct batspitch_transaction_end { // 0xBD
  struct batspitch_generic_header header;
} __attribute__((packed));

struct batspitch_end_of_session { // 0x2D
  struct batspitch_generic_header header;
} __attribute__((packed));

struct batspitch_add_order_long { // 0x21
  struct batspitch_generic_header header;
  uint64_t	order_id; // 8
  char		side;     // 1 'B' or 'S'
  uint32_t	size;     // 4
  char          symbol[6];// 6  right padded with spaces
  uint64_t	price;    // 8
  uint8_t	flags;    // 1
} __attribute__((packed));

struct batspitch_add_order_short { // 0x22
  struct batspitch_generic_header header;
  uint64_t	order_id; // 8
  char		side;     // 1 'B' or 'S'
  uint16_t	size;     // 2
  char          symbol[6];// 6  right padded with spaces
  uint16_t	price;    // 2
  uint8_t	flags;    // 1
} __attribute__((packed));

struct batspitch_add_order_expanded { // 0x2F
  struct batspitch_generic_header header;
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

struct batspitch_order_executed { // 0x23
  struct batspitch_generic_header header;
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

struct batspitch_order_executed_at_price_size { // 0x24
  struct batspitch_generic_header header;
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

struct batspitch_reduced_size_long { // 0x25
  struct batspitch_generic_header header;
  uint64_t	order_id;           // 8
  uint32_t	canceled_size;      // 4
} __attribute__((packed));

struct batspitch_reduced_size_short { // 0x26
  struct batspitch_generic_header header;
  uint64_t	order_id;           // 8
  uint16_t	canceled_size;      // 2
} __attribute__((packed));

struct batspitch_order_modify_long { // 0x27
  struct batspitch_generic_header header;
  uint64_t	order_id;           // 8
  uint32_t	size;               // 4 (= new size)
  uint64_t	price;              // 8 (= new price)
  uint8_t  	flags;              // 1
                                        /* bit 0 - Display: '0' - not aggregated, '1' - aggregated */
                                        /*   bit 1 - Maintain Priority: '0' - Reset Priority, '1' - Maintain Priority */
                                        /*   bits 2-7 - Reserved */
} __attribute__((packed));

struct batspitch_order_modify_short { // 0x28
  struct batspitch_generic_header header;
  uint64_t	order_id;           // 8
  uint16_t	size;               // 2 (= new size)
  uint16_t	price;              // 2 (= new price)
  uint8_t  	flags;              // 1
                                        /* bit 0 - Display: '0' - not aggregated, '1' - aggregated */
                                        /*   bit 1 - Maintain Priority: '0' - Reset Priority, '1' - Maintain Priority */
                                        /*   bits 2-7 - Reserved */
} __attribute__((packed));

struct batspitch_order_delete { // 0x29
  struct batspitch_generic_header header;
  uint64_t	order_id;           // 8
} __attribute__((packed));

struct batspitch_trade_long { // 0x2A
  struct batspitch_generic_header header;
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

struct batspitch_trade_short { // 0x2B
  struct batspitch_generic_header header;
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

struct batspitch_trade_expanded { // 0x30
  struct batspitch_generic_header header;
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

struct batspitch_trade_break { // 0x2C
  struct batspitch_generic_header header;
  uint64_t	execution_id;       // 8
} __attribute__((packed));

struct batspitch_symbol_mapping { // 0x2E
  uint8_t	length;           // 1
  uint8_t       type;             // 1
  char          symbol[6];        // 6  right padded with spaces
  char          osi_symbol[21];   // 21
  char          symbol_condition; // 1  'N' - Normal, 'C' - Closing Only
  char          underlying[8];    // 8  right padded with spaces
} __attribute__((packed));

//-----------------------------------------------
// SPIN and GRP messages
//-----------------------------------------------

struct batspitch_login_request { // 0x01 (same for GRP and Spin)
  struct batspitch_sequenced_unit_header hdr;
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  char          session_sub_id[4];  // 4
  char          username[4];        // 4
  char          filler[2];          // 2
  char          password[10];       // 10
} __attribute__((packed));

struct batspitch_login_response { // 0x02 (same for GRP and Spin)
  struct batspitch_sequenced_unit_header hdr;
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  char          status;             // 1
                                         /* 'A' - Login Accepted */
                                         /* 'N' - Not authorized (Invalid Username/Password) */
                                         /* 'B' - Session in use */
                                         /* 'S' - Invalid Session */
} __attribute__((packed));

struct batspitch_gap_request { // 0x03
  struct batspitch_sequenced_unit_header hdr;
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  uint8_t       unit;               // 1
  uint32_t      sequence;           // 4
  uint16_t      count;              // 2
} __attribute__((packed));

struct batspitch_gap_response { // 0x04
  //  struct batspitch_sequenced_unit_header hdr;
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

struct batspitch_spin_image_available { // 0x80
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  uint32_t      sequence;           // 4
} __attribute__((packed));

struct batspitch_spin_response { // 0x82 for snapshot
                                 // 0x85 for defintions
  //  struct batspitch_sequenced_unit_header hdr;
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  uint32_t      sequence;           // 4 Sequence number from batspitch_spin_image_available or '0' for Definitions
  uint32_t      count;              // 4 Number of "Add Order" or Definitions messages which will be contained in this spin
  char          status;             // 1
                                        /* 'A' - Accepted */
                                        /* 'O' - Out of range  */
                                        /* 'S' - Spin already in progress */
} __attribute__((packed));

struct batspitch_spin_finished { // 0x83
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  uint32_t      sequence;           // 4 Sequence number from batspitch_spin_request
} __attribute__((packed));

struct batspitch_spin_request { // 0x84 for instrument_definition
                                // 0x81 for snapshot
  struct batspitch_sequenced_unit_header hdr;
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
  uint32_t      sequence;           // 4 Must be '0' for instrument_definition, from batspitch_spin_image_available for snapshot
} __attribute__((packed));

struct batspitch_instrument_definition_finished { // 0x86
  uint8_t	length;             // 1
  uint8_t       type;               // 1 
} __attribute__((packed));

#define EKA_BATS_TRADE_COND(x)		 \
  x == ' ' ? EfhTradeCond::kReg :	 \
    x == 'S' ? EfhTradeCond::kSprd :	 \
    x == 'I' ? EfhTradeCond::kIsoi :	 \
    EfhTradeCond::kUnmapped


#define EKA_BATS_TRADE_STAT(x)   (x == 'T' ? EfhTradeStatus::kNormal : EfhTradeStatus::kHalted)

#endif
