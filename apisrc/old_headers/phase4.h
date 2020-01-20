#ifndef _PHASE4_H_
#define _PHASE4_H_
/* #ifdef __cplusplus */
/* extern "C" { */
/* #endif */
#include "subscriptions.h"
  //  typedef enum {SUBSCR_SUCCESS=0, REACHED_MAX_TOTAL_SUBSCRIPTIONS=1, NO_ROOM_IN_HASH_LINE=2} eka_subscr_status_t;
  uint64_t subscribe_with_ctx(uint64_t id); //calls eka_subscr_status_t add_subscription(, uint64_t id);

  void subscribe_done(void* null_ptr); // calls write_all_subscriptions(dev,0);

  int init_strategy(const struct global_params *params, void* null_ptr);

  void enable_strategy(uint8_t arm);

  typedef uint8_t ctx_write_channels;
  int set_full_ctx(uint64_t id, void* local_ctx, ctx_write_channels channel);

  int set_full_session_ctx(uint16_t global_session_idx, void *local_ctx);

  //  size_t p4_report_recv(uint8_t unused_core_id, void *buf, size_t size);
  size_t p4_report_recv(void *buf, size_t size);

  int set_group_session(void* null_ptr, uint8_t group, uint16_t sess_id);

/* #ifdef __cplusplus */
/* } // extern "C" */
/* #endif */
#endif //_PHASE4_H_

