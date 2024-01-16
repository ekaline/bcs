#ifndef _EKA_H_
#define _EKA_H_

#include "ekaline_build_time.h"
#include "ekaline_ver_num.h"

#define EKA_SESSIONS_PER_NIF 32
#define EKA_WC_REGION_OFFS 0x8000ULL
#define EKA_WC_REGION_SIZE 0x8000ULL

#define EKA_RELEASE_STRING_LEN 256
#define EKA_BUILD_TIME_STRING_LEN 256

typedef struct {
  uint8_t active;
  uint8_t drop_next_real_tx_pckt;
  uint8_t send_seq2hw;
  uint16_t pkt_size_to_drop;
  uint32_t ip_saddr;
  uint32_t ip_daddr;
  uint16_t tcp_sport;
  uint16_t tcp_dport;
  uint32_t tcp_local_seq_num;
  uint32_t tcp_remote_seq_num;
  uint32_t pkt_tx_cntr;
  uint32_t pkt_rx_cntr;
  uint8_t macsa[6];
  uint8_t macda[6];
  uint32_t vlan_tag;
  uint16_t tcp_window;
} eka_session_t;

typedef struct { // driver's params per NIF
  /* char eka_version[64]; */
  /* char eka_release[256]; */
  /* uint8_t eka_debug; */
  /* uint8_t drop_igmp; */
  /* uint8_t drop_arp; */
  /* uint8_t drop_all_rx_udp; */
  /* uint64_t bar0_va; */
  /* uint64_t bar0_wc_va; */
  eka_session_t
      eka_session[EKA_SESSIONS_PER_NIF]; // valid only for
                                         // NIF == 0
} eka_nif_state_t;

typedef enum {
  EKA_VERSION = 1,
  EKA_DUMP = 2,
  EKA_SET = 3,
  EKA_DEBUG_ON = 4,
  EKA_DEBUG_OFF = 5,
  EKA_DROP_IGMP_ON = 6,
  EKA_DROP_IGMP_OFF = 7,
  EKA_DROP_ARP_ON = 8,
  EKA_DROP_ARP_OFF = 9,
  EKA_UDP_DROP_ON = 10,
  EKA_UDP_DROP_OFF = 11,
  EKA_GET_NIF_STATE = 12,
  EKA_IOREMAP_WC = 13,
  EKA_GET_IGMP_STATE = 14
} eka_ioctl_cmd_t;

typedef struct {
  uint64_t bar0_pa;
  uint64_t bar0_va;
  uint64_t bar0_wc_pa;
  uint64_t bar0_wc_va;
} eka_wcattr_t;

typedef struct { // struct used to transfer (in both
                 // directions) driver's params by IOCTL
  eka_ioctl_cmd_t cmd;
  /* uint8_t nif_num; */
  /* uint8_t session_num; */

  /* char eka_version[64]; */
  /* char eka_release[256]; */
  /* uint8_t eka_debug; */
  /* uint8_t drop_igmp; */
  /* uint8_t drop_arp; */
  /* uint8_t drop_all_rx_udp; */
  /* eka_session_t eka_session; */
  /* eka_wcattr_t wcattr; */

  uint64_t paramA;
  uint64_t paramB;
  uint64_t paramC;
  uint64_t paramD;
  eka_session_t eka_session;
} eka_ioctl_t;

typedef union table_desc {
  uint64_t desc;
  struct fields {
    uint8_t source_bank;
    uint8_t source_thread;
    uint32_t target_idx : 24;
    uint8_t pad[3];
  } __attribute__((packed)) td;
} __attribute__((packed)) table_desc_t;

#endif /* EKA_H */
