#ifndef _EKA_DATA_STRUCTS_H
#define _EKA_DATA_STRUCTS_H

/* #include "lwip/sockets.h" */
/* #include "lwip/prot/ethernet.h" */
/* #include "lwip/prot/ip4.h" */
/* #include "lwip/prot/tcp.h" */

#include "eka_fh_book.h"
#include "EkaCtxs.h"
#include "Eka.h"
//#include "eka_user_channel.h"

//#include "eka_exc.h"


#define EKA_WC


typedef enum {EFC_CONF=0, EFH_CONF=1} eka_conf_type_t;
typedef uint64_t uint64_t;
typedef enum {SUBSCR_SUCCESS=0, DUPLICATE_SEC_ID=-1, REACHED_MAX_TOTAL_SUBSCRIPTIONS=-2, NO_ROOM_IN_HASH_LINE=-3} eka_subscr_status_t;

#define EFH_PRICE_SCALE 1
#define EFH_STRIKE_PRICE_SCALE 1


#define P4_CTX_PRICE_SCALE 100
enum {
    MAX_SEC_CTX                 = 768*1024,
    MAX_SESSION_CTX_PER_CORE    = 128,
};

#define MIN_SIZE_TO_ONLY_FIRE_ONCE 10

typedef enum {CONF_SUCCESS=0, IGNORED=1, UNKNOWN_KEY=2, WRONG_VALUE=3, CONFLICTING_CONF=4} eka_add_conf_t;

enum efh_order_side {EFH_ORDER_BID = 1,EFH_ORDER_ASK = -1};

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

//class eka_fh;

struct sec_ctx {
    uint16_t            bid_min_price;
    uint16_t            ask_max_price;
    uint8_t             size;
    uint8_t             ver_num;

    uint8_t             lower_bytes_of_sec_id;
} __attribute__ ((packed));

struct common_session_ctx {
    uint64_t    cl_ord_id;
    uint8_t     next_session_ctx;
} __attribute__ ((packed));


typedef struct __attribute__((packed)) {
  uint8_t  type; 
  uint8_t  subtype; // must be == 0xFF
  uint8_t  core_id; 
  uint16_t length; 
} dma_report_t;

typedef struct __attribute__((packed)) {
  uint8_t fired_cores;
  uint8_t core_id;
  uint8_t group_id;
  uint64_t md_size;
  uint64_t md_price;
  uint64_t md_sequence;
  uint64_t md_timestamp;
  uint8_t fire_counter;
  uint8_t last_unarm_reason;
  uint8_t fire_reason;
  uint8_t side;
  uint64_t ei_size;
  uint64_t ei_price;
  uint64_t security_id;
  uint64_t timestamp;
} normalized_report_t;

typedef struct __attribute__((packed)) {
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
} normalized_report_compat_t; //copy from compat p4_data.h

/* struct global_params { */
/*   uint8_t     report_only; */
/*   uint8_t     no_report_on_exception; */
/*   uint8_t     debug_always_fire; // if strategy_enable was on prior to fire, */
/*   // it will not be disabled due to a fire */
/*   // produced by always_fire. */
/*   // to activate, write from CLI */
/*   // "efh_tool write 4 0x0200000000f00f00 0xefa0beda 8" */
/*   uint32_t    max_size; */
/*   uint64_t    watchdog_timeout_sec; */
/* }; */

struct report_max_sizes {
  uint8_t ask_max : 4;
  uint8_t bid_max : 4;
}__attribute__ ((packed));

typedef struct __attribute__((packed)) {
  uint8_t lower_bytes_of_sec_id;
  uint8_t ver_num;
  uint8_t size;
  uint16_t ask_max_price;
  uint16_t bid_min_price;
} secctx_report_t;

typedef struct __attribute__((packed)) {
  uint16_t            bid_min_price;
  uint16_t            ask_max_price;
  uint8_t             size;
  uint8_t             ver_num;
  uint8_t             lower_bytes_of_sec_id;
} secctx_report_compat_t;

typedef struct __attribute__((packed)) {
  uint32_t equote_mpid_sqf_badge;
  uint8_t next_session;
  uint64_t clid;
} sessionctx_report_t;

typedef struct __attribute__((packed)) {
  uint64_t    cl_ord_id;
  uint8_t     next_session_ctx;
} sessionctx_report_compat_t;

typedef struct __attribute__((packed)){
  dma_report_t dma_report;
  normalized_report_t normalized_report;
  secctx_report_t secctx_report;
  sessionctx_report_t sessionctx_report;
} fire_report_t;

struct __attribute__((packed)) session_fire_app_ctx {
  uint64_t clid;
  uint8_t next_session;
  uint32_t equote_mpid_sqf_badge;
};

typedef struct session_fire_app_ctx session_fire_app_ctx_t;

typedef struct sec_ctx sec_ctx_t;

struct miax_sec_ctx {
    struct sec_ctx common;
} __attribute__((packed));

struct nasdaq_sec_ctx {
    struct sec_ctx  common;
} __attribute__((packed));

struct sqf_quote_sec_ctx {
    struct sec_ctx  common;
} __attribute__((packed));

/* up to 128 session_ctx are possible */
struct miax_session_ctx {
    struct common_session_ctx csc;
    union {
        uint32_t mpid_num;
        char mpid[4];
    } mpid;
} __attribute__((packed));

struct nasdaq_session_ctx {
    struct common_session_ctx   csc;
    uint32_t                    sqf_badge;
} __attribute__((packed));

struct equote {
    char message_type[2];
    uint32_t client_message_id;
    //char mpid[4];
    uint32_t mpid;
    uint32_t product_id;
    char action;
    char type;
    uint32_t event_id;
    uint32_t target_message_id;
    uint32_t price;
    uint32_t size;
    char side;
} __attribute__((packed));

struct sesm {
    uint16_t length;
    char type;
} __attribute__((packed));

struct equote_fire {
    struct sesm     ses;
    struct equote   eq;
} __attribute__((packed));

struct sqf {
    char type_sub[2];
    uint32_t badge;
    uint64_t message_id;
    uint16_t quote_count;
    uint32_t option_id1;
    uint32_t bid_price1;
    uint32_t bid_size1;
    uint32_t ask_price1;
    uint32_t ask_size1;
    char reentry1;
  //    uint32_t option_id2;
  //    uint32_t bid_price2;
  //    uint32_t bid_size2;
  //    uint32_t ask_price2;
  //    uint32_t ask_size2;
  //    char reentry2;
} __attribute__((packed));

struct boe_fire {
  uint16_t StartOfMessage;
  uint16_t MessageLength;
  uint8_t MessageType;
  uint8_t MatchingUnit;
  uint32_t SequenceNumber;
  char ClOrdID[20];
  uint8_t Side;
  uint32_t OrderQty;
  uint8_t NumberOfBitfields;
  uint8_t Bitfield0;
  uint8_t Bitfield1;
  //  uint8_t Bitfield2;
  //  uint8_t Bitfield3;
  //  uint8_t Bitfield4;
  //  uint8_t Bitfield5;
  //  uint32_t ClearingFirm;
  //  uint32_t ClearingAccount;
  uint64_t Price;
  //  uint8_t OrderType;
  uint8_t TIF;
  uint64_t Symbol;
  uint8_t Capacity;
  //  char Account[16];
  //  uint8_t OpenClose;
} __attribute__((packed));

struct soupbin {
    uint16_t length;
    char type;
} __attribute__((packed));

struct sqf_fire {
    struct soupbin  sb;
    struct sqf      sq;
    struct soupbin  sb1;
    struct sqf      sq1;
} __attribute__((packed));

typedef struct __attribute__((packed)) {
  uint16_t ip_cs;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port; 
  uint16_t tcpcs; //unused
} hw_session_nw_header_t;

  typedef enum {IDLE=0, GAP=1, RECOVERY=2, STREAMING_MD=3} eka_udp_state_t;


  typedef struct eka_tcp_sess_prams {
    /* struct sockaddr_in  src; */
    /* struct sockaddr_in  dst; */
    uint32_t           src_ip;
    uint16_t           src_port;

    uint32_t           dst_ip;
    uint16_t           dst_port;

    uint32_t           ip_preliminary_pseudo_csum;
    uint32_t           tcp_preliminary_pseudo_csum;

    uint8_t __attribute__ ((aligned(0x100)))  pktBuf[256];


    int sock_fd;
    struct pico_socket* pico_sock;
    uint16_t tcp_window;
    volatile uint32_t tcp_local_seq_num;
    volatile uint32_t tcp_remote_seq_num;

    volatile uint64_t fast_path_bytes;
    volatile uint64_t tx_driver_bytes;
    volatile uint64_t fast_path_dummy_bytes;
    volatile uint64_t fire_dummy_bytes;
    //    bool     send_seq2hw;
    session_fire_app_ctx_t app_ctx;
  } eka_tcp_sess_prams_t;

  typedef struct eka_udp_sess_prams {
    eka_udp_state_t     state;
    uint8_t             first_session_id;
    bool                disable_igmp;
    int                 sock_fd;
    /* struct sockaddr_in  mcast; */
    /* struct sockaddr_in  recovery; */
    uint32_t            mcast_ip;
    uint16_t            mcast_port;
    uint16_t            mcast_set;

    char                username[32];
    char                passwd[32];

    //    SN_ChannelId udpChannelId; // for HW UDP channel implementation
    //    const SN_Packet* pPreviousUdpPacket; // for HW UDP channel implementation
  } eka_udp_sess_prams_t;

  /* typedef struct user_channel_params { */
  /*   SN_ChannelId id; */
  /*   const SN_Packet* ptr; */
  /* } user_channel_params_t; */

  typedef struct service_thread_params {
    pthread_t               thread_id;
    volatile bool           active; 
  } service_thread_params_t;

  typedef struct eka_core_params {
    char ifname[10];
    bool connected;
    bool macsa_set_externally;
    bool macda_set_externally;
    bool src_ip_set_externally;
    uint8_t preconf_macsa[6];
    uint8_t macsa[6];
    uint8_t macda[6];
    uint32_t src_ip;
    //    struct sockaddr_in src_ip_addr; // same as previous field
    uint8_t tcp_sessions;
    uint8_t udp_sessions;
    eka_tcp_sess_prams_t tcp_sess[EKA_MAX_TCP_SESSIONS_PER_CORE];
    eka_udp_sess_prams_t udp_sess[EKA_MAX_UDP_SESSIONS_PER_CORE];
    //    struct EkaTcpDev     tcpDev;
    struct netif*  pLwipNetIf;
  } eka_core_params_t;
    
  typedef struct p4_fire_params {
    struct global_params external_params;
    uint8_t     auto_rearm; // for testing only
  } p4_fire_params_t;

  typedef struct hw_pararams {
    uint8_t enabled_cores;
    uint8_t feed_ver;
  } hw_pararams_t;

  typedef struct eka_subscr_line {
    uint16_t col[EKA_SUBSCR_TABLE_COLUMNS]; // keepint HASH of the corresponding Security
    uint8_t valid_cntr; // the columns are populated sequentially with no gaps/holes
    uint32_t prev_sum; // amount of securities in above lines
  } eka_subscr_line_t;

  typedef struct eka_subsr_params {
    uint32_t sec_cnt;
    eka_subscr_line_t line[EKA_SUBSCR_TABLE_ROWS];
    uint64_t sec_id[EKA_MAX_P4_SUBSCR]; // requested list
    uint32_t cnt; // requested subscr cnt
  } eka_subscr_params_t;

  typedef struct eka_ctx_thread_params {
    uint8_t bank;
  } eka_ctx_thread_params_t;

class eka_sn_dev;
class eka_user_channel;
class exc_debug_module;

/* EkaDev { */
/*   //    SN_DeviceId               dev_id; */
/*   eka_sn_dev*               sn_dev; */
/*   hw_pararams_t             hw; */
/*   eka_core_params_t         core[EKA_MAX_CORES]; */
/*   //    user_channel_params_t     fire_report_ch; */
/*   //    user_channel_params_t     fast_path_ch; */

/*   eka_user_channel*         fr_ch; // new concept */
/*   eka_user_channel*         fp_ch; // new concept */
/*   eka_user_channel*         tcp_ch; // new concept */

/*   volatile bool             exc_active; */

/*   int                       lwip_sem; */

/*   service_thread_params_t   serv_thr; */
/*   eka_ctx_thread_params_t   thread[EKA_MAX_CTX_THREADS]; */
/*   p4_fire_params_t          fire_params; */
/*   eka_subscr_params_t       subscr; */
/*   //    volatile bool             continue_sending_igmps; */
/*   pthread_mutex_t           tcp_send_mutex; */
/*   pthread_mutex_t           tcp_socket_open_mutex; */

/*   struct EfcCtx*            pEfcCtx; */
/*   struct EfcRunCtx*         pEfcRunCtx; */
/*   struct EfhRunCtx*         pEfhRunCtx; */

/*   volatile bool             exc_inited; */
/*   volatile bool             lwip_inited; */
/*   //  volatile bool             ekaLwipPollThreadIsUp; */
/*   volatile bool             efc_run_threadIsUp; */
/*   volatile bool             efc_fire_report_threadIsUp; */

/*   uint8_t                   numFh; */
/*   eka_fh*                   fh[EKA_MAX_FEED_HANDLERS]; */
/*   EkaLogCallback            logCB; */
/*   void*                     logCtx; */
/*   exc_debug_module*         exc_debug; */
/* }; */

/* typedef EkaDev eka_dev_t;   */

union tcp_fast_send_desc {
    uint64_t desc;
    struct tcp_desc {
        uint16_t src_index;
        uint8_t length;
        uint8_t session;
        uint8_t core;
        uint16_t ip_checksum;
        uint8_t send_attr;
    } __attribute__((packed)) tcpd;
} __attribute__((packed));

enum dma_type {
    FPGA_FIRE = 1,
    FAST_PATH = 2,
};

union large_table_desc {
    uint64_t lt_desc;
    struct ltd {
        uint8_t src_bank;
        uint8_t src_thread;
        uint32_t target_idx : 24;
        uint8_t pad[3];
    }__attribute__((packed)) ltd;
}__attribute__((packed));


/* typedef struct fh_pthread_args { */
/*   eka_dev_t* dev; */
/*   uint8_t gr_id; */
/* } fh_pthread_args_t; */




#endif
