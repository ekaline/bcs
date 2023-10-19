#ifndef __EKA_EOBI_TYPES_H__
#define __EKA_EOBI_TYPES_H__

#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaBc.h"

/* ----------------------------------------------------- */

namespace EkaEobi {

using ExchSecurityId = EkaBcEurSecId;
typedef int64_t Price;
typedef uint16_t NormPrice;
typedef int32_t Size;

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

struct EobiTobParams { // NOT used in eurex, in eurex
                       // different struct
  uint64_t last_transacttime;
  uint64_t fireBetterPrice;
  uint64_t firePrice;
  uint64_t price;
  uint16_t normprice;
  uint32_t size;
  uint32_t msg_seq_num;
} __attribute__((packed));

struct EobiDepthEntryParams {
  uint64_t price;
  uint32_t size;
} __attribute__((packed));

typedef EobiDepthEntryParams
    depth_entry_params_array[HW_DEPTH_PRICES];

struct EobiDepthParams {
  depth_entry_params_array entry;
} __attribute__((packed));

struct EobiHwBookParams {
  EobiTobParams tob;
  EobiDepthParams depth;
} __attribute__((packed));

/* ----------------------------------------------------- */

template <class OutT, class InpT>
inline OutT normalizePriceDelta(InpT price) {
  if (price == static_cast<InpT>(-1))
    return static_cast<OutT>(-1);
  if (price / 100000000 > 42)
    on_error("price delta parameter must be less than "
             "42*100000000 (not %ju)",
             price);
  return static_cast<OutT>(price);
}

/* ----------------------------------------------------- */
template <class OutT, class InpT>
inline OutT normalizeSize(InpT size) {
  if (size == static_cast<InpT>(-1))
    return static_cast<OutT>(-1);
  if (size % 10000)
    on_error("any size parameter must be multiple of "
             "10000 (not %u)",
             size);
  return static_cast<OutT>(size / 10000);
}

/* ----------------------------------------------------- */
template <class OutT, class InpT>
inline OutT normalizeTimeDelta(InpT timeDelta) {
  if (timeDelta == static_cast<InpT>(-1))
    return static_cast<OutT>(-1);

  if (timeDelta % 1000)
    on_error("time delta must have whole number of "
             "microseconds, (not %ju nanoseconds)",
             timeDelta);
  if (timeDelta > 64000000)
    on_error("time delta is limited to 64ms "
             "(not %ju ns)",
             timeDelta);

  return static_cast<OutT>(timeDelta / 1000);
}
} // namespace EkaEobi
#endif
