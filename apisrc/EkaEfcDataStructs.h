#ifndef _EKA_EFC_DATA_STRUCTS_H_
#define _EKA_EFC_DATA_STRUCTS_H_

#define P4_CTX_PRICE_SCALE 100
enum {
    MAX_SEC_CTX                 = 768*1024,
    MAX_SESSION_CTX_PER_CORE    = 128,
};

struct global_params {
    uint8_t     enable_strategy;
    uint8_t     report_only;
    uint8_t     debug_always_fire_on_unsubscribed; // relevant only if debug_always_fire is enabled
    uint8_t     debug_always_fire; // if strategy_enable was on prior to fire,
                                   // it will not be disabled due to a fire
                                   // produced by always_fire.
                                   // to activate, write from CLI
                                   // "efh_tool write 4 0x0200000000f00f00 0xefa0beda 8"
    uint32_t    max_size;
    uint64_t    watchdog_timeout_sec;
};

// FixedPrice is uint16_t

struct EkaHwSecCtx {
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t    size;
  uint8_t    verNum;
  uint8_t    lowerBytesOfSecId;
} __attribute__ ((packed));

struct SqfShortQuoteBlockMsg {
  char     typeSub[2]; // = {'Q','Q'};
  uint32_t badge;
  uint64_t messageId;
  uint16_t quoteCount;  // = 1
  uint32_t optionId;
  uint32_t bidPrice;
  uint32_t bidSize;
  uint32_t askPrice;
  uint32_t askSize;
  char     reentry;
} __attribute__((packed));

#endif

