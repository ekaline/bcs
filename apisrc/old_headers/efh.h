/* Copyright (c) Ekaline, 2013-2016. All rights reserved.
 * PROVIDED "AS IS" WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED.
 * See LICENSE file for details.
 */

#ifndef _EFH_H_
#define _EFH_H_
/* #ifdef __cplusplus */
/* extern "C" { */
/* #endif */

#include <stdint.h>
  //#include "eka_data_structs.h"
#include "subscriptions.h"

  //extern eka_dev_t* dev;


typedef void* EFH;
struct eka_conf;
struct efh_message;

//EFH efh_open(struct eka_conf *conf);
  EFH efh_open(void* null_ptr); 

int efh_request_group_definitions(EFH null_ptr, uint8_t group);

  void efh_close(EFH null_ptr);

/* 
 * message pointer written by efh_get_message will be valid until the next call
 * to efh_get_message, at that point the memory allocated for it is freed 
 * access at your own discretion
 */
enum get_message_return_values {
    SUCCESS                     = 0,
    EMPTY_MESSAGE_QUEUE         = -1,
    ERROR_GETTING_DEFS          = -2,
    PACKET_IP_UNKNOWN           = -3,
    HARDWARE_UNRESPONSIVE       = -4,
    /* If HW exception occurs, the ptr to struct efh_message * will be assigned
     * with a uint64_t value instead, please add that value to the error report
     * when sending to us.
     * if parameter is struct efh_message **m, *m = exception_val
     */
    HARDWARE_EXCEPTION          = -5,
    SDMA_COUNTER_MISMATCH       = -7,
    SDMA_SIZE_CORRUPTION        = -8,
    SDMA_READ_COUNTER_CORRUPT   = -9,
    UNKOWN_STATUS_TYPE          = -10,
};

int efh_get_message(EFH null_ptr, struct efh_message** msg_ptr_ptr);
int efh_recv_start(EFH null_ptr);
int efh_recv_stop(EFH null_ptr);
int efh_recv_start_group(EFH null_ptr, int group);

#define EFH_VERSION_MAJOR 6
#define EFH_VERSION_MINOR 1
#define EFH_VERSION_COMPAT ((EFH_VERSION_MAJOR << 16) | EFH_VERSION_MINOR)

  struct eka_conf* eka_conf_create(); // Obsolete, returns NULL
  void eka_conf_destroy(void* conf); // Obsolete
typedef enum {CONF_SUCCESS=0, IGNORED=1, UNKNOWN_KEY=2, WRONG_VALUE=3, CONFLICTING_CONF=4} eka_add_conf_t;
  eka_add_conf_t eka_conf_add(void* null_ptr, const char *key, const char *value);

/* writes to stdout the set of Conf Keys tested during efh_open from efh_conf */
void efh_print_caps();

/* DEBUG */
//  uint64_t efh_get_card_clock(); // not implemented

//uint32_t check_subscription(uint64_t id);

/* returns  0   - success
 *          -1  - failed to allocate memory
 */
//typedef enum {SUBSCR_SUCCESS=0, REACHED_MAX_TOTAL_SUBSCRIPTIONS=1, NO_ROOM_IN_HASH_LINE=2} eka_subscr_status_t;
//eka_subscr_status_t efh_add_fh_subscription(EFH null_ptr, uint8_t group, uint64_t id);

/* #ifdef __cplusplus */

/* } // extern "C" */
/* #endif */
#endif //_EFH_H_
