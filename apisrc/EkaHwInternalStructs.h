/* SHURIK */

#ifndef _EKA_HW_INTERNAL_STRUCTS_H_
#define _EKA_HW_INTERNAL_STRUCTS_H_

#include "eka_macros.h"

enum {HW_BID=1, HW_ASK=0};

typedef struct __attribute__((packed)) {
  uint16_t ip_cs;
  uint32_t dst_ip;
  uint16_t src_port;
  uint16_t dst_port; 
  uint16_t tcpcs; //unused
} hw_session_nw_header_t;

union large_table_desc {
  uint64_t lt_desc;
  struct ltd {
    uint8_t src_bank;
    uint8_t src_thread;
    uint32_t target_idx : 24;
    uint8_t pad[2];
    uint8_t target_table; // bit[0]: buy/sell book, bit[7:1]: security index when subscribing
  }__attribute__((packed)) ltd;
}__attribute__((packed));


struct epm_tcpcs_field_template_t {
  uint16_t bitmap;
} __attribute__((packed));

struct epm_tcpcs_half_template_t {
  struct epm_tcpcs_field_template_t row[16];
} __attribute__((packed));

struct epm_tcpcs_template_t {
  struct epm_tcpcs_half_template_t low;
  struct epm_tcpcs_half_template_t high;
} __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));

/* FPGA code: */
/* typedef struct packed { */
/*         bit israw; */
/*         bit report_en; */
/*         bit action_valid; */
/*         bit dummy_en; */
/*         bit empty_report_en; */
/*         bit app_seq_inc; */
/*         bit reserved6; */
/*         bit reserved7; */
/* } epm_action_bp_t; //must be in 1B resolution */


typedef union  {
  uint8_t bits;
  struct  {
    uint8_t reserved7        : 1;
    uint8_t originatedFromHw : 1; //should be added to FastPathBytes
    uint8_t app_seq_inc      : 1;
    uint8_t empty_report_en  : 1;
    uint8_t feedbck_en       : 1; //should HW send packet feedback to libary
    uint8_t action_valid     : 1;
    uint8_t report_en        : 1; //should HW send user application report
    uint8_t israw            : 1; //should HW update HW_TCP_SEQ table
  } __attribute__((packed)) bitmap; //must be in 1B resolution
} __attribute__((packed)) EpmActionBitmap;

/* FPGA code: */
/* typedef struct packed { */
/* 	bit [7:0]       action_type; // EPM_ACTION_TYPE_ */
/*         bit [15:0]      payload_size;   */
/*         bit [31:0]      tcp_cs;   */
/*         bit [63:0]      token; */
/*         bit [63:0]      user; */
/*         bit [63:0]      enable_bitmap; */
/*         bit [63:0]      mask_post_local; */
/*         bit [63:0]      mask_post_strat; */
/*         bit [15:0]      next_action_index;   */
/*         bit [7:0]       target_session_id;   */
/*         bit [7:0]       target_core_id;   */
/*         bit [7:0]       target_prod_id;   */
/* 	bit [31:0]      data_db_ptr; */
/* 	bit [15:0]      template_db_ptr; */
/* 	bit [7:0]       tcpcs_template_db_ptr; */
/* 	epm_action_bp_t bit_params; */
/* } epm_action_t; */

/* parameter EPM_ACTION_TYPE_SRC_ACTION = 1; //take size and cs from action */
/* parameter EPM_ACTION_TYPE_SRC_DESC   = 2; //take size and cs from desc */

enum class TcpCsSizeSource : uint8_t {FROM_ACTION = 1, FROM_DESCR = 2};

struct epm_action_t {
  EpmActionBitmap bit_params;
  uint8_t  tcpcs_template_db_ptr;  
  uint16_t template_db_ptr;
  uint32_t data_db_ptr;
  uint8_t  target_prod_id;  
  uint8_t  target_core_id;  
  uint8_t  target_session_id;  
  uint16_t next_action_index;
  uint64_t mask_post_strat;
  uint64_t mask_post_local;
  uint64_t enable_bitmap;
  uint64_t user;
  uint64_t token;
  uint32_t tcpCSum;
  uint16_t payloadSize;
  TcpCsSizeSource   tcpCsSizeSource;  
} __attribute__((packed)) __attribute__ ((aligned(sizeof(uint64_t))));


/* FPGA code: */
/* typedef struct packed { */
/* 	bit [1:0]  reserved; */
/*         bit [2:0]  action_region;   */
/*         bit [10:0] payload_size;   */
/*         bit [15:0] action_index;   */
/*         bit [31:0] tcp_cs;   */
/* } epm_trig_desc_t; */

#define EpmTrigPayloadMask 0x3FF


typedef union {
  uint64_t desc;
  struct  {
    uint32_t tcp_cs;  
    uint16_t action_index;
    uint16_t size     : 11;  
    uint16_t region   : 5;  
  } __attribute__((packed)) str;
} __attribute__((packed)) epm_trig_desc_t;

#define EPM_DATA_TEMPLATE_BASE_ADDR (0xc0000)
#define EPM_CSUM_TEMPLATE_BASE_ADDR (0x88000)
#define EPM_TRIGGER_DESC_ADDR       (0xf0230)


/* FPGA code: */
/* typedef struct packed { */
/*         bit [7:0]  islocal;             ///< True -> called from epmRaiseTrigger */
/*         bit [63:0] user;              ///< Opaque value copied from EpmAction */
/*         bit [15:0] postStratEnable;   ///< Strategy-level enable bits after fire */
/* 	bit [15:0] preStratEnable;    ///< Strategy-level enable bits before fire */
/* 	bit [15:0] postLocalEnable;   ///< Action-level enable bits after firing */
/* 	bit [15:0] preLocalEnable;    ///< Action-level enable bits before fire */
/*         bit [7:0]  error;             ///< Error code for SendError */
/*         bit [7:0]  action;            ///< What device did in response to trigger */
/* 	bit [7:0]  strategyid; */
/* 	bit [15:0] triggeractionid; */
/* 	bit [15:0] actionactionid; */
/* 	bit [63:0] token; */
/* } epm_report_t; */


struct hw_epm_report_t {
  uint64_t  token;
  uint16_t  actionId;
  uint16_t  triggerActionId;
  uint8_t   strategyId;
  uint8_t   action;
  uint8_t   error;
  uint16_t  preLocalEnable;
  uint16_t  postLocalEnable;
  uint16_t  preStratEnable;
  uint16_t  postStratEnable;
  uint64_t  user;
  uint8_t   islocal;
} __attribute__((packed));

struct hw_status_exception_report_t {
  uint32_t global_vector;
  uint32_t core_vector[4];
} __attribute__((packed));

struct hw_status_arm_report_t {
  uint8_t  arm_state;
  uint32_t arm_expected_version;
} __attribute__((packed));

struct hw_epm_status_report_t {
  hw_status_arm_report_t       arm_report;
  hw_status_exception_report_t exception_report;
  uint8_t                      b32_padding[7];
  hw_epm_report_t              epm;
} __attribute__((packed));

struct hw_epm_news_report_t {
  uint16_t        strategy_index;
  uint8_t         strategy_region;
  uint64_t        token;
  uint8_t         b32_padding[21];
  hw_epm_report_t epm;
} __attribute__((packed));

struct hw_epm_fast_sweep_report_t {
  uint8_t         last_msg_id;                     
  uint8_t         first_msg_id;                     
  uint16_t        last_msg_num;
  uint16_t        locate_id;
  uint16_t        udp_payload_size;
  uint8_t         b32_padding[24];
  hw_epm_report_t epm;
} __attribute__((packed));

struct hw_epm_fast_cancel_report_t {
  uint8_t         num_in_group;
  uint16_t        header_size;
  uint32_t        sequence_number;
  uint8_t         b32_padding[25];
  hw_epm_report_t epm;
} __attribute__((packed));

struct hw_epm_sw_trigger_report_t {
  uint8_t         b32_padding[32];
  hw_epm_report_t epm;
} __attribute__((packed));

/* parameter EKA_ACTIONRESULT_Unknown          = 0;  ///< Zero initialization yields an invalid value */
/* parameter EKA_ACTIONRESULT_Sent             = 1;  ///< All action payloads sent successfully */
/* parameter EKA_ACTIONRESULT_InvalidToken     = 2;  ///< Trigger security token did not match action token */
/* parameter EKA_ACTIONRESULT_InvalidStrategy  = 3;  ///< Strategy id was not valid */
/* parameter EKA_ACTIONRESULT_InvalidAction    = 4;  ///< Action id was not valid */
/* parameter EKA_ACTIONRESULT_DisabledAction   = 5;  ///< Did not fire because an enable bit was not set */
/* parameter EKA_ACTIONRESULT_SendError        = 6;  ///< Send error occured (e.g., TCP session closed) */
/* parameter EKA_ACTIONRESULT_HWPeriodicStatus = 255; ///< HW generated periodic status */

enum class HwEpmActionStatus : uint8_t {
  Unknown = 0,
    Sent             = 1,
    InvalidToken     = 2,
    InvalidStrategy  = 3,
    InvalidAction    = 4,
    DisabledAction   = 5,
    SendError        = 6,
    HWPeriodicStatus = 255
};

struct hw_epm_capabilities_t {
  uint16_t numof_actions;
  uint8_t tcpcs_numof_templates;
  uint16_t data_template_total_bytes;
  uint32_t heap_total_bytes;
  uint8_t max_threads;
} __attribute__((packed));

struct hw_core_capabilities_t {
  uint16_t tcp_sessions_percore;
  uint8_t bitmap_tcp_cores;
  uint8_t bitmap_md_cores;
} __attribute__((packed));

struct hw_capabilities_t {
  hw_core_capabilities_t core;
  hw_epm_capabilities_t epm;
} __attribute__((packed));

typedef struct __attribute__((packed)) {
  uint8_t  type; 
  uint8_t  subtype; // must be == 0xFF
  uint8_t  core_id; 
  uint16_t length; 
  uint8_t  paddingto32[32-5];
} dma_report_t;

struct EpmThreadState {
  bool     valid          = false;
  uint64_t threadWindBase = -1;
};


typedef struct __attribute__((packed)) {
  uint8_t reserved0     : 1;
  uint8_t reserved1     : 1;
  uint8_t reserved2     : 1;
  uint8_t reserved3     : 1;
  uint8_t reserved4     : 1;
  uint8_t reserved5     : 1;
  uint8_t dummy_en      : 1;
  uint8_t expect_report : 1;
} feedback_dma_bitparams_t;

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint16_t length;
  uint32_t index;
  /* feedback_dma_bitparams_t  bitparams; */
  EpmActionBitmap bitparams;
} feedback_dma_report_t;

typedef struct __attribute__((packed)) {
  uint8_t reserved0     : 1;
  uint8_t reserved1     : 1;
  uint8_t reserved2     : 1;
  uint8_t reserved3     : 1;
  uint8_t reserved4     : 1;
  uint8_t reserved5     : 1;
  uint8_t reserved6     : 1;
  uint8_t reserved7     : 1;
} report_dma_bitparams_t; //maybe other order?

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint16_t length;
  uint32_t feedbackDmaIndex;
  report_dma_bitparams_t  bitparams;
} report_dma_report_t;

typedef struct __attribute__((packed)) {
  uint8_t reserved0     : 1;
  uint8_t reserved1     : 1;
  uint8_t reserved2     : 1;
  uint8_t reserved3     : 1;
  uint8_t reserved4     : 1;
  uint8_t reserved5     : 1;
  uint8_t reserved6     : 1;
  uint8_t reserved7     : 1;
} tcprx_dma_bitparams_t; //maybe other order?

typedef struct __attribute__((packed)) {
  uint8_t  type;
  tcprx_dma_bitparams_t bitparams;
  char     pad[32 - 2];
} tcprx_dma_report_t;

/* typedef struct packed { */
/*         bit [7:0] strategy_region;   */
/*         bit [15:0] strategy_index;   */
/*         bit [63:0] token;   */
/* 	bit [15:0] max_header_size; */
/* 	bit [7:0] min_num_in_group; */
/* } sh_cancels_param_t; */

  struct EfcCmeFastCancelStrategyConf {
      uint8_t        minNoMDEntries;
      uint16_t       maxMsgSize;
      uint64_t       token;
      uint16_t       fireActionId;
      uint8_t        strategyId;
  } __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));


  struct EfcItchFastSweepStrategyConf {
      uint64_t       token;
      uint8_t        minMsgCount;
      uint16_t       minUDPSize;
      uint16_t       fireActionId;
      uint8_t        strategyId;
  } __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));


  struct EfcQEDStrategyConfSingle {
      uint32_t       padding : 15;
      uint32_t       enable  : 1;
      uint8_t        minNumLevel;
      uint16_t       dsID;
      uint16_t       fireActionId;
      uint8_t        strategyId;
      uint64_t       token;
  } __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));

  struct EfcQEDStrategyConf {
    EfcQEDStrategyConfSingle product[4];
  } __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));

#endif
