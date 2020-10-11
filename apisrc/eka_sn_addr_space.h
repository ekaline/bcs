#ifndef _EKA_SN_ADDR_SPACE_H_
#define _EKA_SN_ADDR_SPACE_H_


enum addresses {
//General
    VERSION1 = 0xf0ff0, //feed id is (reg_val>>56)&0xF
    VERSION2 = 0xf0ff8, //cores # is (reg_val>>56)&0xFF
    ENABLE_PORT = 0xf0020,    // bit 0..7 - enable RX
                              // bit 16..23 - enable fire cores
    SW_STATISTICS = 0xf0770,
//P4
    P4_ARM_DISARM = 0xf07c8,
    P4_STRAT_CONF = 0xf07c0,
    P4_GLOBAL_MAX_SIZE = 0xf07e0,
    // force_fire - bit 63
    // auto_rearm - bit 62
    // report_only - bit 0
    P4_AUTO_REARM_SHIFT = 62,
    P4_FORCE_FIRE_SHIFT = 63,
    P4_WATCHDOG_CONF = 0xf0650,
    P4_FATAL_DEBUG = 0xf0f00,
    P4_MTHRD_CTX_BASE = 0x900000,
    P4_MTHRD_CTX_DESC = 0xf00660,
    //restrictions from nasdaq phase 4 apply
    P4_SESSION_CTX_BASE = 0x10000,
    P4_SESSION_CTX_CORE_OFFSET = 0x1000,
    P4_SESSION_CTX_DESC = 0x1f000,
    P4_SESSION_CTX_CORE_DESC_OFFSET = 0x100,
    P4_CTX_CHANNEL_BASE = 0x90000,
    P4_CTX_CHANNEL_SINGLE_SIZE = 8,
    P4_CTX_BUFFERS_PER_CHANNEL = 8,
    P4_CTX_CHANNEL_BUFF = P4_CTX_CHANNEL_SINGLE_SIZE*P4_CTX_BUFFERS_PER_CHANNEL,
    P4_CONFIRM_REG = 0xf0660,
    P4_CTX_CHANNELS_COUNT = 16,
// pricing Round down to dime for GEM
    ROUND_2B_TABLE_DEPTH = 65536,
    ROUND_2B_ADDR = 0xf0510,
    ROUND_2B_DATA = 0xf0518,
//TCP
    CORE_CONFIG_BASE = 0xe0000, //base address for all per-core configurations
    CORE_CONFIG_DELTA = 0x1000, //CORE_CONFIG_DELTA*core + CORE_CONFIG_BASE
    CORE_MACDA_OFFSET = 0x300,
    CORE_MACSA_OFFSET = 0x308,
    CORE_SRC_IP_OFFSET = 0x310,
    HW_SESSION_NETWORK_BASE = 0x40000,
    HW_SESSION_NETWORK_CORE_OFFSET = 0x1000,
    HW_SESSION_NETWORK_DESC_BASE = 0x4f000,
    HW_SESSION_NETWORK_DESC_OFFSET = 0x100,
    TCP_FAST_SEND_SP_BASE = 0x50000,
    TCP_FAST_SEND_DESC = 0xf0200,
    TCP_FAST_SEND_COUNTER = 0xf0208,
//FH
    FH_SUBS_HASH_BASE = 0xa0000,
    FH_SUBS_HASH_DESC = 0xf0668,

    FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL = 0xf0520, // global UDP traffic enable
    FH_ACL_MC_IP_ENABLE = 0xf0630,
    FH_ACL_MC_PORT_ENABLE = 0xf0638,
    //restricted to 1 bank & 1 thread.
    //
    //DESC structure for both CTX_DESC & HASH_DESC
    //defined in table_desc below
//GROUP SUB
    FH_GROUP_IP = 0xf0500,
    //described in ip_group union below
    FH_GROUP_PORT = 0xf0628,
    //described in port_group union below

    EpmHeapHwBaseAddr  = 0xd0000,
    HwCapabilitiesAddr = 0x8f000,

};

#endif
