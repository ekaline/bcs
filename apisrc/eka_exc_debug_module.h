#ifndef _EXC_DEBUG_MODULE_H
#define _EXC_DEBUG_MODULE_H

#include <inttypes.h>

#include "eka_hw_conf.h"
#include "eka_macros.h"
#include "eka_nw_headers.h"

#define EXC_TEST_PKT_PREFIX_SIZE 6
#define EXC_TEST_PKT_FREE_TEXT_SIZE 64

class exc_debug_module {
 public:
  exc_debug_module(EkaDev* dev, const char* prefix);
  bool check_dummy_pkt(uint8_t* pkt, uint8_t core, uint8_t sess, uint size);
  bool check_rx(uint8_t* pkt, uint8_t core, uint8_t sess);
  bool check_tx(uint8_t* pkt, uint8_t core, uint8_t sess);

  uint64_t get_expected_dummy_pkt_cnt(uint8_t core, uint8_t sess);

 private:
  EkaDev* dev;
  char           prefix[EXC_TEST_PKT_PREFIX_SIZE];
  uint64_t       dummy_expected[EKA_MAX_CORES][EKA_MAX_TCP_SESSIONS_PER_CORE];
  uint64_t       rx_expected[EKA_MAX_CORES][EKA_MAX_TCP_SESSIONS_PER_CORE];
  uint64_t       tx_expected[EKA_MAX_CORES][EKA_MAX_TCP_SESSIONS_PER_CORE];
};

struct eka_tcp_test_pkt {
  char     prefix[EXC_TEST_PKT_PREFIX_SIZE];
  uint8_t  core;      // 1
  uint8_t  sess;      // 1
  uint64_t cnt;   // 8
  char     free_text[EXC_TEST_PKT_FREE_TEXT_SIZE];
};

#endif // _EXC_DEBUG_MODULE_H
