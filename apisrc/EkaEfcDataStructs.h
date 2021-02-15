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


/* struct EkaHwSecCtx { */
/*   uint8_t    lowerBytesOfSecId; */
/*   uint8_t    verNum; */
/*   uint8_t    size; */
/*   FixedPrice askMaxPrice; */
/*   FixedPrice bidMinPrice; */
/* } __attribute__ ((packed)); */

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

/* FPGA code: */
/* typedef struct packed { */
/* 	bit     [7:0]   SecID; */
/* 	bit     [7:0]   Version; */
/* 	bit     [7:0]   BidAskMaxSize; */
/* 	bit     [15:0]  AskMaxPrice; */
/* 	bit	[15:0]	BidMinPrice; */
/* } emc_sec_ctx_s; // you should have this struct in any case to configure SecCtx */

struct EkaHwSecCtx {
  FixedPrice bidMinPrice;
  FixedPrice askMaxPrice;
  uint8_t    size;
  uint8_t    verNum;
  uint8_t    lowerBytesOfSecId;
} __attribute__ ((packed));


/* FPGA code: */
/* typedef struct packed { */
/*   bit             SourceFeed; */
/*   bit             IsNotTradable; */
/*   bit             Side; */
/*   bit             isAON; */
/*   bit     [2:0]   CoreID; */
/*   bit             reserved; */
/* } sp4_order_bitparams_t; // !!! keep in byte */

typedef union  {
  uint8_t bits;
  struct  {
    uint8_t Reserved       : 1;
    uint8_t CoreID         : 3;
    uint8_t isAON          : 1;
    uint8_t Side           : 1;
    uint8_t IsNotTradable  : 1; 
    uint8_t SourceFeed     : 1;
  } __attribute__((packed)) bitmap; //must be in 1B resolution
} __attribute__((packed)) EfcFiredOrderBitmap;


/* FPGA code: */
/* typedef struct packed { */
/*   bit     [63:0]  Timestamp; */
/*   bit     [63:0]  Sequence; */
/*   bit     [7:0]   GroupID; */
/*   bit     [31:0]  SecurityID; */
/*   bit     [7:0]   Counter; */
/*   bit     [31:0]  Size; */
/*   bit     [31:0]  Price; */
/*   sp4_order_bitparams_t Bitparams; */
/* } sp4_order_t;  */

struct EfcFiredOrder {
  EfcFiredOrderBitmap   attr;
  uint32_t              price;
  uint32_t              size;
  uint8_t               counter;
  uint32_t              securityId;
  uint8_t               groupId;
  uint64_t              sequence;
  uint64_t              timestamp;
} __attribute__((packed));


/* FPGA code: */
/* typedef struct packed { */
/* 	emc_sec_ctx_s security_context_entry;  // Security Context */
/* 	bit [31:0]    security_context_addr; // Security Context in SecCtx Table */
/*         sp4_order_t   order_trigger_data; // MD used for trigger */
/* 	bit [6*8-1:0] padx8; */
/* } normalized_report_sh_t; */

struct EfcNormalizedFireReport {
  char                pad[6];          // = bit [6*8-1:0] padx8
  EfcFiredOrder       triggerOrder;    // = sp4_order_t   order_trigger_data 
  uint32_t            securityCtxAddr; // = bit [31:0]    security_context_addr
  EkaHwSecCtx         securityCtx;     // = emc_sec_ctx_s security_context_entry
} __attribute__((packed));

#endif

