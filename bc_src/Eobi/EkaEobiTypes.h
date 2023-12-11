#ifndef __EKA_EOBI_TYPES_H__
#define __EKA_EOBI_TYPES_H__

#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaBc.h"

/* ----------------------------------------------------- */

namespace EkaEobi {

using ExchSecurityId = EkaBcEurSecId;
using Price = EkaBcEurPrice;
using Size = EkaBcEurMdSize;

typedef uint16_t NormPrice;

typedef uint8_t HwFireSize;
typedef uint16_t HwMdSize;
typedef int64_t HwMdPrice;
typedef uint8_t HwBookSpread;
typedef uint16_t HwTimeDelta;

static const uint ENTRIES = 32 * 1024;

typedef enum {
  BID = 0,
  ASK = 1,
  IRRELEVANT = 3,
  ERROR = 101,
  BOTH = 200,
  NONE = 300
} SIDE;

struct MdOut {
  EkaDev *dev;
  uint8_t coreId;
  uint8_t pad1[3];
  uint32_t securityId;
  SIDE side;
  uint64_t price;
  uint32_t size;
  uint8_t priceLevel;
  uint8_t pad2[3];
  uint64_t sequence;
  uint32_t pktSeq;
  uint64_t transactTime;
  bool lastEvent;
  bool tobChanged;
  uint16_t msgNum;
  uint16_t sessNum;
  void *book;
};

typedef enum {
  Invalid = 0,
  PrintOnly,
  UpdateBookOnly,
  PrintAndUpdateBook,
  UpdateBookAndCheckIntegrity,
  PrintAndUpdateBookAndCheckIntegrity
} ProcessAction;

struct EobiTobParams {
  uint64_t last_transacttime; // 8
  uint64_t fireBetterPrice;   // 8
  uint64_t firePrice;         // 8
  uint64_t price;             // 8
  uint16_t normprice;         // 2
  uint32_t size;              // 4
  uint32_t msg_seq_num;       // 4
} __attribute__((packed));    // 42

struct EobiDepthEntryParams {
  uint64_t price;
  uint32_t size;
} __attribute__((packed));

typedef EobiDepthEntryParams
    depth_entry_params_array[HW_DEPTH_PRICES];

struct EobiDepthParams {
  EobiDepthEntryParams entry[HW_DEPTH_PRICES];
} __attribute__((packed));

struct EobiHwBookParams {
  EobiTobParams tob;
  EobiDepthParams depth;
} __attribute__((aligned(sizeof(uint64_t))))
__attribute__((packed));

enum class HwSideT : int { BID = 0, ASK = 1 };

#if 0
HW:
parameter ST_PRICE = 64;
parameter ST_SIZE  = 16;
parameter ST_TIME  = 8;

typedef struct packed {
        bit [7:0]  max_book_spread;
        bit [7:0]  buy_size;
        bit [7:0]  sell_size;
bit [15:0] action_index;
bit [7:0]  product_index; // pointer to prod, like arm and version
bit [63:0] secid;
} product_params_t; // 15B, keep under 32B for reports

parameter ATBEST_SETS     = 4;
parameter BETTERBEST_SETS = 5;

typedef struct packed {
bit [ST_PRICE-1:0] min_price_delta;
bit [ST_SIZE-1:0]  min_ticker_size;
bit [7:0]          max_post_size;
bit [ST_SIZE-1:0]  max_tob_size;
bit [ST_SIZE-1:0]  min_tob_size;
bit [7:0]          buy_size;
bit [7:0]          sell_size;
bit [7:0]          bit_params; // bit [7:0]  bit_params encoding
                               //     parameter BIT_ENABLED = 0;
                               //     parameter BIT_BOC     = 1;
} jump_param_set_t; // 1+1+8+2+1+2+2+1+1+1=20 (must be smaller than 32B for reports)

typedef struct packed {
jump_param_set_t [ATBEST_SETS-1:0]     at_best;
jump_param_set_t [BETTERBEST_SETS-1:0] better_best;
product_params_t                       product_params;
} strat_jump_conf_t;

JUMP BASE 0x50000
depth 16x256B
index == hash_handle

#endif

struct HwProdParams {
  int64_t secId;              // 8
  uint8_t prodHandle;         // 1
  uint16_t actionIdx;         // 2
  HwFireSize askSize;         // 1
  HwFireSize bidSize;         // 1
  HwBookSpread maxBookSpread; // 1
} __attribute__((packed));    // 14 Bytes

struct HwJumpParamsSet {
  uint8_t bitParams;       // 1 : 0x01 ENABLED, 0x02 BOC
  HwFireSize askSize;      // 1
  HwFireSize bidSize;      // 1
  HwMdSize minTobSize;     // 2
  HwMdSize maxTobSize;     // 2
  HwFireSize maxPostSize;  // 1
  HwMdSize minTickerSize;  // 2
  HwMdPrice minPriceDelta; // 8
} __attribute__((packed)); // 18 Bytes

struct HwJumpParams {
  HwProdParams prodParams;
  HwJumpParamsSet betterBest[EKA_JUMP_BETTERBEST_SETS];
  HwJumpParamsSet atBest[EKA_JUMP_ATBEST_SETS];
} __attribute__((aligned(sizeof(uint64_t))))
__attribute__((packed)); // 14 + 4 * 18 + 5 * 18 = 176

/* ----------------------------------------------------- */
#if 0
parameter RJUMP_ATBEST_SETS     = 4;
parameter RJUMP_BETTERBEST_SETS = 6;

typedef struct packed {
bit [ST_SIZE-1:0] tickersize_lots;
bit [ST_TIME-1:0] time_delta_us;
bit [ST_SIZE-1:0] max_tob_size;
bit [ST_SIZE-1:0] min_tob_size;
bit [ST_SIZE-1:0] max_opposit_tob_size;
bit [7:0]  min_spread;
bit [7:0]  buy_size;
bit [7:0]  sell_size;
bit [7:0]  bit_params;
} jump_onref_param_set_t; // 1+2+1+2+2+2+1+1+1+1=14 (must be smaller than 32B for reports)

typedef struct packed {
jump_onref_param_set_t [RJUMP_ATBEST_SETS-1:0]     at_best;
jump_onref_param_set_t [RJUMP_BETTERBEST_SETS-1:0] better_best;
product_params_t                                   product_params;
} strat_rjump_conf_t;

RJUMP_BASE 0x60000
depth 256x256B
index == hash_handle*16+0

#endif

#define BITPARAM_ENABLE  0x0
#define BITPARAM_BOC     0x1

struct HwReferenceJumpParamsSet {
  uint8_t bitParams;      // 1 : 0x01 ENABLED, 0x02 BOC
  HwFireSize askSize;     // 1
  HwFireSize bidSize;     // 1
  HwBookSpread minSpread; // 1

  HwMdSize maxOppositeTobSize; // 2
  HwMdSize minTobSize;         // 2
  HwMdSize maxTobSize;         // 2

  HwTimeDelta timeDelta;   // 2
  HwMdSize tickerSizeLots; // 2
} __attribute__((packed)); // 14 Bytes

struct HwReferenceJumpParams {
  char pad[6];
  HwProdParams prodParams;
  HwReferenceJumpParamsSet
      betterBest[EKA_RJUMP_BETTERBEST_SETS];
  HwReferenceJumpParamsSet atBest[EKA_RJUMP_ATBEST_SETS];
} __attribute__((aligned(sizeof(uint64_t))))
__attribute__((packed)); // 14 + 4 * 14 + 6 * 14 = 154, roundup 160

/* ----------------------------------------------------- */

inline HwMdPrice normalizePriceDelta(EkaBcEurPrice price) {
  if (price == static_cast<EkaBcEurPrice>(-1))
    return static_cast<HwMdPrice>(-1);
  if (price / 100000000 > 42)
    on_error("price delta parameter must be less than "
             "42*100000000 (not %ju)",
             price);
  return static_cast<HwMdPrice>(price);
}

/* ----------------------------------------------------- */
inline HwTimeDelta
normalizeTimeDelta(EkaBcEurTimeNs timeDelta) {
  if (timeDelta == static_cast<EkaBcEurTimeNs>(-1))
    return static_cast<HwTimeDelta>(-1);

  if (timeDelta % 1000)
    on_error("time delta must have whole number of "
             "microseconds, (not %ju nanoseconds)",
             timeDelta);
  if (timeDelta > 64000000)
    on_error("time delta is limited to 64ms "
             "(not %ju ns)",
             timeDelta);

  return static_cast<HwTimeDelta>(timeDelta / 1000);
}

inline uint8_t normalizeHandle(EkaBcSecHandle h) {
  if (h < 0 || h > 15)
    on_error("Bad prodHandle %jd", h);
  return static_cast<uint8_t>(h);
}

inline uint16_t normalizeActionIdx(EkaBcActionIdx idx) {
  if (idx < 0 || idx > 2048)
    on_error("Bad actionIdx %d", idx);
  return static_cast<uint16_t>(idx);
}

inline HwFireSize normalizeFireSize(EkaBcEurFireSize size) {
  if (size == static_cast<EkaBcEurFireSize>(-1))
    return static_cast<HwFireSize>(-1);
  
  if (size % 10000 || size > 254 * 10000)
    on_error("Bad size %u", size);
  return static_cast<HwFireSize>(size / 10000);
}

inline HwMdSize normalizeMdSize(EkaBcEurMdSize size) {
  if (size == static_cast<EkaBcEurMdSize>(-1))
    return static_cast<HwMdSize>(-1);

  if (size % 10000)
    on_error("Bad size %u", size);
  return static_cast<HwMdSize>(size / 10000);
}

inline HwBookSpread
normalizePriceSpread(EkaBcEurPrice maxBookSpread) {
  if (maxBookSpread > 255)
    on_error("Bad maxBookSpread %jd", maxBookSpread);
  return static_cast<HwBookSpread>(maxBookSpread);
}

} // namespace EkaEobi
#endif
