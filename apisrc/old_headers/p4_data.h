#ifndef _P4_DATA_H_
#define _P4_DATA_H_

#define BAD_HANDLE  0
#define P4_CTX_PRICE_SCALE 100
#include <stdint.h>

enum {
    MAX_SEC_CTX                 = 768*1024,
    MAX_SESSION_CTX_PER_CORE    = 128,
};

#define MIN_SIZE_TO_ONLY_FIRE_ONCE 10

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

/* watchdog timeout is set in seconds
 * 0  - watchdog is set to default 1 sec timeout
 * 0< - internal counter activated, any access to phase4 data resets counter.
 */

/* struct max_sizes { */
/*     uint8_t bid_max_size : 4; */
/*     uint8_t ask_max_size : 4; */
/* } __attribute__ ((packed)); */

/* struct sec_ctx { */
/*     uint16_t            bid_min_price; */
/*     uint16_t            ask_max_price; */
/*     uint8_t             size; */
/*     uint8_t             ver_num; */

/*     uint8_t             lower_bytes_of_sec_id; */
/* } __attribute__ ((packed)); */

/* struct common_session_ctx { */
/*     uint64_t    cl_ord_id; */
/*     uint8_t     next_session_ctx; */
/* } __attribute__ ((packed)); */

/* Fire Reason BIT on numbers
 * FIRE_REASON_ALWAYS_FIRE  0
 * FIRE_REASON_BID_PASS     1
 * FIRE_REASON_ASK_PASS     2
 * FIRE_REASON_EXCEPTION    4 // added on June 20, 2018
 * FIRE_SRC_FEED_IDX        6 
 *      value 0 = source primary feed update
 *      value 1 = source secondary feed update
 * FIRE_REASON_ARM_STATUS   7
 */
/* Unarm Reason BIT on numbers
 * UNARM_STRATEGY_PASS      0
 * UNARM_RX_DISABLE         1
 * UNARM_WD_EXPIRED         2
 * UNARM_CTX_COLLISION      3
 * UNARM_GAP_OPENED         4
 * UNARM_CRC_ERROR          5
 * UNARM_USER_COMMAND       6
 */
struct normalized_report {
    uint64_t    timestamp;
    uint64_t    sec_id;
    uint64_t    price;
    uint64_t    size;
    uint8_t     side;
    uint8_t     fire_reason;
    uint8_t     unarm_reason;
    uint8_t     num_fires;
    uint64_t    md_timestamp;
    uint64_t    md_sequence;
    uint64_t    md_price;
    uint64_t    md_size;
    uint8_t     md_group_id;
    uint8_t     md_core_id;
} __attribute__ ((packed));

#endif /*_P4_DATA_H_*/
