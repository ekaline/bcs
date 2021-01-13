#ifndef _EKA_HW_CONF_H_
#define _EKA_HW_CONF_H_

#include "eka_macros.h"

#define EKA_MAX_P4_SUBSCR 768*1024
//#define EKA_MAX_CTX_THREADS 16
#define EKA_BANKS_PER_CTX_THREAD 8
#define EKA_WORDS_PER_CTX_BANK 1

#define EKA_WATCHDOG_SEC_VAL 0x9EF21AA

#define EKA_P4_REPORT_ONLY_BIT        1ULL << 0
#define EKA_P4_NO_REPORT_ON_EXCEPTION 1ULL << 1
#define EKA_P4_ALWAYS_FIRE_UNSUBS_BIT 1ULL << 4
#define EKA_P4_ALWAYS_FIRE_BIT        1ULL << 63
#define EKA_P4_AUTO_REARM_BIT         1ULL << 62


#define EKA_MAX_CORES 8
#define EKA_MAX_TCP_SESSIONS_PER_CORE 32
#define EKA_MAX_UDP_SESSIONS_PER_CORE 64
  
/* #ifdef _VERILOG_SIM */
/* #define EKA_SUBSCR_TABLE_ROWS 64 */
/* #else */
#define EKA_SUBSCR_TABLE_ROWS 32*1024
/* #endif */

#define EKA_SUBSCR_TABLE_COLUMNS 42
#define EKA_MAX_TCP_PACKET_SIZE 256 // 256B - HW Buf
#define EKA_MAX_TCP_PAYLOAD_SIZE EKA_MAX_TCP_PACKET_SIZE-64 // 64B - NW headers


#define EKA_FH_SEC_HASH_LINES (1024 * 128) // 17 LS bits (using 0x1FFFF mask)
#define EKA_FH_SEC_HASH_MASK 0x1FFFF     // 17 LS bits to get the Hash table line

#define EKA_FH_GROUPS 64

#define ST_HASH_NUM_OF_BUCKETS 65536
#define EKA_FH_ORDERS_TO_ALLOCATE (8 * 1024 * 1024) // orders per MC group
#define EKA_FH_ORDERS_HASH_MASK 0x00000000000FFFFF // 20 Low Bits to get to 1M indexes
#define EKA_FH_PLEVELS_TO_ALLOCATE (4 * 1024 * 1024) // plevels per MC group

#define EKA_UCH_FASTPATH_ID 1 // User Channel ID for Fast Path and HW Fires
#define EKA_UCH_FIREREPORT_ID 0 // User Channel ID for Fast Path and HW Fires

#define HW_FEED_SHIFT_SIZE 56 // position of the HW_FEED field in the FPGA VERSION reg
#define HW_FEED_SHIFT_MASK  0xf // 4 LS bits encode the HW_FEED

#define EKA_ADDR_INTERRUPT_SHADOW_RO       0xf0790
#define EKA_ADDR_INTERRUPT_0_SHADOW_RO     0xe0150

#define EKA_FPGA_FREQUENCY (161.132828125)

// scratchpad 16bit 64kB address space
#define SCRATCHPAD_BASE (0x20000)
#define SCRATCHPAD_SIZE (64 * 1024)
#define SCRPAD_SW_VER (SCRATCHPAD_BASE)
#define SCRPAD_CORE_BASE (SCRPAD_SW_VER + 8)
#define SCRPAD_CORE_MC_IP_BASE (SCRPAD_CORE_BASE + (8 * EKA_MAX_CORES))

//#define SW_STATISTICS 0xf0770
// reset statistic counters
#define STAT_CLEAR    0xf0028

#define ADDR_OPENDEV_COUNTER     0xf0e10
#define ADDR_RT_COUNTER          0xf0e08
#define FPGA_RT_CNTR             0xf0e00
#define ADDR_INTERRUPT_SHADOW_RO 0xf0790

// global configurations
#define ADDR_VERSION_ID       0xf0ff0
#define ADDR_VERSION_HIGH_ID  0xf0ff8
#define ADDR_VERSION_HIGHTWO_ID 0xf0fe0

#define ADDR_NW_GENERAL_CONF  0xf0020
#define ADDR_SW_STATS_ZERO    0xf0770
#define ADDR_P4_GENERAL_CONF  0xf07c0
#define ADDR_P4_FIRE_MDCNT    0xf07f0
#define ADDR_P4_ARB_POINTS    0xf07f8
#define ADDR_FATAL_CONF       0xf0f00

#define ADDR_IPFOUND                 0xe0128
#define ADDR_IPFOUND1                0xe0148
#define ADDR_IPFOUND2                0xe0160
#define ADDR_IPFOUND3                0xe0168
#define ADDR_PARSER_STATS            0xe0130

#define ADDR_STATS_RX_PPS            0xe0008 //  Statistics: 4 msb - max, 4 lsb - curr
#define ADDR_STATS_RX_BPS_MAX        0xe0010 //  Statistics: bytes per second max
#define ADDR_STATS_RX_BPS_CURR       0xe0018 //  Statistics: bytes per second current
#define ADDR_STATS_RX_BYTES_TOT      0xe0020 //  Statistics: total bytes
#define ADDR_STATS_RX_PKTS_TOT       0xe0028 //  Statistics: total packets

#define EKA_CORRECT_SW_VER    0x1122330000000000
typedef enum {
  SN_NASDAQ   = 0x0,
  SN_MIAX     = 0x1,
  SN_PHLX     = 0x2,
  SN_GEMX     = 0x3,  
  SN_CBOE     = 0x4,  
} hw_feed_versions_t;

#endif
