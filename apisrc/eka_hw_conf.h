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
  
#define EKA_SUBSCR_TABLE_ROWS 32*1024
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

typedef enum {
  SN_NASDAQ   = 0x0,
  SN_MIAX     = 0x1,
  SN_PHLX     = 0x2,
  SN_GEMX     = 0x3,  
  SN_CBOE     = 0x4,  
} hw_feed_versions_t;

#endif
