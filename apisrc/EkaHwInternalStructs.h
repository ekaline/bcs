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
  struct epm_tcpcs_field_template_t field[16];
} __attribute__((packed));

struct epm_tcpcs_template_t {
  struct epm_tcpcs_half_template_t low;
  struct epm_tcpcs_half_template_t high;
} __attribute__((packed));

struct epm_actione_bitparams_t {
  uint8_t reserved7  : 1;
  uint8_t reserved6  : 1;
  uint8_t reserved5  : 1;
  uint8_t reserved4  : 1;
  uint8_t feedbck_en : 1; //should HW send packet feedback to libary
  uint8_t reserved2  : 1;
  uint8_t report_en  : 1; //should HW send user application report
  uint8_t israw      : 1; //should HW update HW_TCP_SEQ table
} __attribute__((packed)); //must be in 1B resolution

/* FPGA code: */
/* typedef struct packed { */
/*         bit [7:0]       action_type; // EPM_ACTION_TYPE_ */
/*         bit [15:0]      payload_size;   */
/*         bit [31:0]      tcp_cs;   */
/*         bit [63:0]      token; */
/*         bit [63:0]      user; */
/*         bit [63:0]      mask_post_local; */
/*         bit [63:0]      mask_post_strat; */
/*         bit [15:0]      next_action_index;   */
/*         bit [7:0]       target_session_id;   */
/*         bit [7:0]       target_core_id;   */
/*         bit [7:0]       target_prod_id;   */
/*         bit [31:0]      data_db_ptr; */
/*         bit [15:0]      template_db_ptr; */
/*         bit [7:0]       tcpcs_template_db_ptr; */
/*         epm_action_bp_t bit_params; */
/* } epm_action_t; */

/* parameter EPM_ACTION_TYPE_SRC_ACTION = 1; //take size and cs from action */
/* parameter EPM_ACTION_TYPE_SRC_DESC   = 2; //take size and cs from desc */

enum class TcpCsSizeSource : uint8_t {FROM_ACTION = 1, FROM_DESCR = 2};

struct epm_action_t {
  epm_actione_bitparams_t bit_params;
  uint8_t  tcpcs_template_db_ptr;  
  uint16_t template_db_ptr;
  uint32_t data_db_ptr;
  uint8_t  target_prod_id;  
  uint8_t  target_core_id;  
  uint8_t  target_session_id;  
  uint16_t next_action_index;
  uint64_t mask_post_strat;
  uint64_t mask_post_local;
  uint64_t user;
  uint64_t token;
  uint32_t tcpCSum;
  uint16_t payloadSize;
  TcpCsSizeSource   tcpCsSizeSource;  
} __attribute__((packed));


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
    uint16_t region   : 3;  
    uint16_t reserved : 2;
  } __attribute__((packed)) str;
} __attribute__((packed)) epm_trig_desc_t;

#define EPM_DATA_TEMPLATE_BASE_ADDR (0xc0000)
#define EPM_CSUM_TEMPLATE_BASE_ADDR (0x88000)
#define EPM_TRIGGER_DESC_ADDR       (0xf0230)


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

#endif
