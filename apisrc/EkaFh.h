#ifndef _EKA_FH_H_
#define _EKA_FH_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string>
#include <regex>
#include <vector>

#include "eka_hw_conf.h"
#include "Efh.h"
#include "EkaDev.h"
#include "EkaCtxs.h"

#define EFH_PRICE_SCALE 1

enum class EkaFhAddConf {CONF_SUCCESS=0, IGNORED=1, UNKNOWN_KEY=2, WRONG_VALUE=3, CONFLICTING_CONF=4} ;
enum class EkaFhMode : uint8_t {UNINIT = 0, DEFINITIONS, SNAPSHOT, MCAST, RECOVERY};

#define EkaFhMode2STR(x) \
  x == EkaFhMode::UNINIT ? "UNINIT" : \
  x == EkaFhMode::DEFINITIONS ? "DEFINITIONS" : \
    x == EkaFhMode::SNAPSHOT ? "SNAPSHOT" : \
    x == EkaFhMode::RECOVERY ? "RECOVERY" : \
    x == EkaFhMode::MCAST ? "MCAST" : \
    "UNEXPECTED"


enum class EkaFhParseResult : int {End = 0, NotEnd, SocketError, ProtocolError};
#define EFH_STRIKE_PRICE_SCALE 1

class EkaFhGroup;
class EkaFhRunGroup;

/* ##################################################################### */
static inline bool isTradingHours(int startHour, int startMinute, int endHour, int endMinute) {
  time_t rawtime;
  time (&rawtime);
  struct tm * ct = localtime (&rawtime);
  if ((ct->tm_hour > startHour || (ct->tm_hour == startHour && ct->tm_min > startMinute)) &&
      (ct->tm_hour < endHour   || (ct->tm_hour == endHour   && ct->tm_min < endMinute  ))
      ) {
    return true;
  }
  return false;
}
/* ##################################################################### */
// Bitmask that tells us the product information carried on this group
enum ProductMask {
  PM_NoInfo         = 0,       // No information available
  PM_VanillaBook    = 1 << 0,  // Vanilla option book prices
  PM_VanillaTrades  = 1 << 1,  // Vanilla option trades
  PM_VanillaAuction = 1 << 2,  // Vanilla option RFQs
  PM_ComplexBook    = 1 << 3,  // Complex option book prices
  PM_ComplexTrades  = 1 << 4,  // Complex option trades
  PM_ComplexAuction = 1 << 5,  // Complex option RFQs
  PM_FutureBook     = 1 << 6,  // Future book prices
  PM_FutureTrades   = 1 << 7,  // Future trades
  PM_FutureAuction  = 1 << 8,  // Future RFQs (spreads)
  PM_VanillaALL = PM_VanillaBook | PM_VanillaTrades | PM_VanillaAuction,
  PM_ComplexALL = PM_ComplexBook | PM_ComplexTrades | PM_ComplexAuction,
  PM_FutureALL = PM_FutureBook | PM_FutureTrades | PM_FutureAuction
};

namespace {

struct ProductNameToMaskEntry {
  const char *name;
  int mask;
};

constexpr ProductNameToMaskEntry ProductNameToMaskMap[] = {
  {
    .name = "vanilla_book",
    .mask = PM_VanillaBook
  },

  {
    .name = "vanilla_trades",
    .mask = PM_VanillaTrades
  },

  {
    .name = "vanilla_auction",
    .mask = PM_VanillaAuction
  },

  {
    .name = "complex_book",
    .mask = PM_ComplexBook
  },

  {
    .name = "complex_trades",
    .mask = PM_ComplexTrades
  },

  {
    .name = "complex_auction",
    .mask = PM_ComplexAuction
  },

  {
    .name = "future_book",
    .mask = PM_FutureBook
  },

  {
    .name = "future_trades",
    .mask = PM_FutureTrades
  },

  {
    .name = "future_auction",
    .mask = PM_FutureAuction
  },
};

constexpr int NoSuchProduct = -1;

  inline int lookupProductMask(const char *productName) {
    for (const auto [n, m] : ProductNameToMaskMap) {
      if (!strcmp(productName, n))
	return m;
    }
    return NoSuchProduct;
  }

  // returns only one name
  inline const char* lookupProductName(int productMask) {
    for (const auto [n, m] : ProductNameToMaskMap) {
      if (productMask & m)
	return n;
    }
    return "UnInitialized";
  }
} // End of anonymous namespace

class EkaFh {
 protected:
  EkaFh() {};

  virtual     EkaFhGroup* addGroup() = 0;

 public:
  virtual     ~EkaFh();

  int                     stop();

  int                     init(const EfhInitCtx* pEfhInitCtx, uint8_t numFh);
  int                     setId(EfhCtx* pEfhCtx, EkaSource exch, uint8_t numFh);

  int                     openGroups(EfhCtx*           pEfhCtx, 
				     const EfhInitCtx* pEfhInitCtx);

  virtual EkaOpResult     runGroups(EfhCtx*          pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint8_t          runGrId) = 0;

  virtual EkaOpResult     getDefinitions(EfhCtx*          pEfhCtx, 
					 const EfhRunCtx* pEfhRunCtx, 
					 const EkaGroup*  group) = 0;
  
  //  void                send_igmp(bool join_leave, volatile bool igmp_thread_active);
  EkaFhAddConf        conf_parse(const char *key, const char *value);
  virtual EkaOpResult subscribeStaticSecurity(uint8_t groupNum, 
						      uint64_t securityId, 
						      EfhSecurityType efhSecurityType,
						      EfhSecUserData efhSecUserData,
						      uint64_t opaqueAttrA,
						      uint64_t opaqueAttrB);

  EkaFhGroup*         nextGrToProcess(uint first, uint numGroups);

  int                 getTradeTime(const EfhDateComponents* dateComponents,
                                   uint32_t* iso8601Date,
                                   time_t* time);

  EkaOpResult         setTradeTimeCtx(void* ctx);

  //-----------------------------------------------------------------------------

 protected:
  EkaOpResult         initGroups(EfhCtx*           pEfhCtx, 
				 const EfhRunCtx*  pEfhRunCtx, 
				 EkaFhRunGroup*    runGr);
  
  virtual uint8_t     getGrId(const uint8_t* pkt);
  
  //-----------------------------------------------------------------------------

 public:
  enum class            GapType { SNAPSHOT=0, RETRANSMIT=1 };

  EfhFeedVer            feed_ver              = EfhFeedVer::kInvalid;
  EkaSource             exch                  = EkaSource::kInvalid;

  uint                  qsize                 = 8 * 1024 * 1024;

  volatile uint8_t      groups                = 0;
  bool                  full_book             = false;
  bool                  subscribe_all         = false;
  bool                  print_parsed_messages = false;

  char                  auth_user[10]         = {};
  char                  auth_passwd[12]       = {};
  bool                  auth_set              = false;

  EkaCoreId             c                     = -1;
  uint8_t               id                    = -1;

  uint8_t               lastServed            = 0; // Q for RR processing
  bool                  any_group_getting_snapshot = false;

  EkaFhGroup*           b_gr[EKA_FH_GROUPS]   = {};

  bool                  noTob                 = false;
  bool                  active                = false;

  volatile bool        terminated            = true;
#ifdef EKA_NOM_LATENCY_CHECK
    std::vector<std::pair<char,uint64_t>> latencies = {};
#endif
  
 protected:
  EkaDev*               dev                   = NULL;

  static const uint64_t TimeCheckRate = 1000;
  bool                  tradingHours = false;
  uint64_t              timeCheckCnt = 0;
  EfhGetTradeTimeFn     getTradeTimeCb;
  void*                 getTradeTimeCtx;
  bool                  pinPacketBuffer = false;
};

#endif
