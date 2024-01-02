#ifndef _EKA_FH_MIAX_PARSER_H_
#define _EKA_FH_MIAX_PARSER_H_
#include <stdint.h>
#include <unistd.h>

// #define EFH_MIAX_STRIKE_PRICE_SCALE 10
#define EFH_MIAX_STRIKE_PRICE_SCALE 1
namespace Tom {

const size_t MaxVanillaDefinitions = 1'500'000;

struct sesm_header {
  uint16_t length;
  char type;
} __attribute__((packed));

struct sesm_unsequenced {
  uint8_t type;
  uint64_t sequence;
} __attribute__((packed));

struct sesm_login_req {
  struct sesm_header header;
  char version[5];
  char username[5];
  char computer_id[8];
  char app_protocol[8];
  uint8_t session;
  uint64_t sequence;
} __attribute__((packed));

struct sesm_retransmit_req { // 'A'
  struct sesm_header header;
  uint64_t start;
  uint64_t end;
} __attribute__((packed));

struct sesm_logout_req {
  struct sesm_header header;
  char reason;
  char text[];
} __attribute__((packed));

struct sesm_login_response {
  struct sesm_header header;
  char status;
  uint8_t session;
  uint64_t sequence;
} __attribute__((packed));

struct miax_request {
  struct sesm_header header;
  uint8_t request_type;
  uint8_t refresh_type;
} __attribute__((packed));

struct sesm_goodbye {
  char reason;
  char text[1535];
} __attribute__((packed));

enum class EKA_SESM_TYPE : char {
  Sequenced = 'S',
  UnSequenced = 'U',
  Loginrequest = 'L',
  LoginResponse = 'R',
  SyncComplete = 'C',
  RetransmitRequest = 'R',
  LogoutRequest = 'X',
  GoodBye = 'G',
  EndOfSession = 'E',
  ServerHeartbeat = '0',
  ClientHeartbeat = '1',
  TestPacket = 'T'
};

enum class TOM_MSG : char {
  Time = '1',
  SeriesUpdate = 'P',
  SystemState = 'S',
  BestBidShort = 'B',
  BestPriorityBidShort = 'h',
  BestAskShort = 'O',
  BestPriorityAskShort = 'i',
  BestBidLong = 'W',
  BestPriorityBidLong = 'j',
  BestAskLong = 'A',
  BestPriorityAskLong = 'k',
  BestBidAskShort = 'd',
  BestBidAskLong = 'D',
  Trade = 'T',
  TradeCancel = 'X',
  UnderlyingTradingStatus = 'H',
  EndOfRefresh = 'E'
};

inline bool operator==(const TOM_MSG lhs,
                       const uint8_t rhs) noexcept {
  return ((uint8_t)lhs) == rhs;
}
inline bool operator==(const uint8_t lhs,
                       const TOM_MSG rhs) noexcept {
  return lhs == (uint8_t)rhs;
}
inline bool operator!=(const TOM_MSG lhs,
                       const uint8_t rhs) noexcept {
  return ((uint8_t)lhs) != rhs;
}
inline bool operator!=(const uint8_t lhs,
                       const TOM_MSG rhs) noexcept {
  return lhs != (uint8_t)rhs;
}

struct TomCommon {
  char Type;
  uint32_t Timestamp;
} __attribute__((packed));

struct TomSeriesUpdate {       // 'P'
  TomCommon CommHdr;           // 5
  uint32_t security_id;        // 4
  char UnderlyingSymbol[11];   // 11
  char SecuritySymbol[6];      // 6
  char Expiration[8];          // 8
  uint32_t StrikePrice;        // 4
  char CallOrPut;              // 'C' or 'P'     // 1
  char OpeningTime[8];         // 8
  char ClosingTime[8];         // 8
  char Restricted;             // 1
  char LongTerm;               // 1
  char Active;                 // 1
  char MbboIncrement;          // 1
  char LiquidityIncrement;     // 1
  char UnderlyingMarket;       // 1
  uint32_t PriorityQuoteWidth; // 4
  char Reserved[8];            // 8
} __attribute__((packed));

struct TomTime { // '1'
  TomCommon CommHdr;
} __attribute__((packed));

struct TomSystemState { // 'S'
  TomCommon CommHdr;    // 5
  char version[8];      // 8
  uint32_t SessId;      // 4
  char SystemStatus;    // 1
} __attribute__((packed));

struct TomBestBidOrOfferShort { // 'B' or 'O'
  TomCommon CommHdr;            // 5
  uint32_t security_id;         // 4
  uint16_t price;               // 2
  uint16_t size;                // 2
  uint16_t customer_size;       // 2
  char condition; // 'A','B','C','R' or 'T' (Halt)
} __attribute__((packed));

struct TomBestBidOrOfferLong { // 'W' or 'A'
  TomCommon CommHdr;           // 5
  uint32_t security_id;        // 4
  uint32_t price;              // 4
  uint32_t size;               // 4
  uint32_t customer_size;      // 4
  char condition; // 'A','B','C','R' or 'T' (Halt)
} __attribute__((packed));

struct TomBestBidAndOfferShort { // 'd'
  TomCommon CommHdr;             // 5
  uint32_t security_id;          // 4
  uint16_t bid_price;            // 2
  uint16_t bid_size;             // 2
  uint16_t bid_customer_size;    // 2
  char bid_condition; // 'A','B','C','R' or 'T' (Halt)
  uint16_t ask_price; // 2
  uint16_t ask_size;  // 2
  uint16_t ask_customer_size; // 2
  char ask_condition; // 'A','B','C','R' or 'T' (Halt)
} __attribute__((packed));

struct TomBestBidAndOfferLong { // 'D'
  TomCommon CommHdr;            // 5
  uint32_t security_id;         // 4
  uint32_t bid_price;           // 4
  uint32_t bid_size;            // 4
  uint32_t bid_customer_size;   // 4
  char bid_condition; // 'A','B','C','R' or 'T' (Halt)
  uint32_t ask_price; // 4
  uint32_t ask_size;  // 4
  uint32_t ask_customer_size; // 4
  char ask_condition; // 'A','B','C','R' or 'T' (Halt)
} __attribute__((packed));

struct TomTrade {             // 'T'
  TomCommon CommHdr;          // 5
  uint32_t security_id;       // 4
  uint32_t trade_id;          // 4
  uint8_t correction_num;     // 1
  uint32_t ref_trade_id;      // 4
  uint8_t ref_correction_num; // 1
  uint32_t price;             // 4
  uint32_t size;              // 4
  char trade_condition; // 'A','B','C','R' or 'T' (Halt)
} __attribute__((packed));

struct TomTradeCancel {   // 'X'
  TomCommon CommHdr;      // 5
  uint32_t security_id;   // 4
  uint32_t trade_id;      // 4
  uint8_t correction_num; // 1
  uint32_t trade_price;   // 4
  uint32_t trade_size;    // 4
  char trade_condition;   // 'A','B','C','R' or 'T' (Halt)
} __attribute__((packed));

struct TomUnderlyingTradingStatus { // 'H'
  TomCommon CommHdr;                // 5
  char underlying[11];              // 11
  char trading_status;              // 1 'H', 'R' or 'O'
  char event_reason;                // 1 'A' or 'M'
  uint32_t exp_time_seconds;        // 4
  uint32_t exp_time_nanos;          // 4
} __attribute__((packed));

struct TomEndOfRefresh { // 'E'
  char type;
  char request_type;
} __attribute__((packed));

#define EKA_MIAX_TRADE_COND(x)                             \
  x == ' '   ? EfhTradeCond::kREG                          \
  : x == 'I' ? EfhTradeCond::kISOI                         \
             : EfhTradeCond::kUnmapped

#define EKA_MIAX_TRADE_STAT(x)                             \
  (x == 'T' ? EfhTradeStatus::kNormal                      \
            : EfhTradeStatus::kHalted)

enum class MiaxMachType : uint8_t {
  Heartbeat = 0,
  StartOfSession = 1,
  EndOfSession = 2,
  AppData = 3
};

struct EkaMiaxMach {
  uint64_t sequence;
  uint16_t length;
  MiaxMachType type;
  uint8_t sess;
} __attribute__((packed));

#define EKA_GET_MACH_SEQ(x)                                \
  ((((Tom::EkaMiaxMach *)(x))->sequence))
#define EKA_GET_MACH_LEN(x)                                \
  ((((Tom::EkaMiaxMach *)(x))->length))
#define EKA_GET_MACH_TYPE(x)                               \
  ((((Tom::EkaMiaxMach *)(x))->type))
#define EKA_GET_MACH_SESS(x)                               \
  (((Tom::EkaMiaxMach *)(x))->sess)
#define EKA_IS_MACH_HEARTBEAT(x)                           \
  ((((Tom::EkaMiaxMach *)(x))->type ==                     \
    Tom::MiaxMachType::Heartbeat))
} // namespace Tom
#endif
