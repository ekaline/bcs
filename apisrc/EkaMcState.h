#ifndef _EKA_MC_STATE_H_
#define _EKA_MC_STATE_H_

#include "eka_macros.h"

typedef struct {
  uint32_t group_address;  /**< Multicast group address of
                              this subscription */
  uint16_t ip_port_number; /**< Multicast IP port number */
  uint16_t channel;        /**< UDP channel to use for this
                              subcscription */
  int16_t vlanTag;         /**< VLAN tag to use for this
                              subscription or -1 for no VLAN */
  uint8_t lane; /**< Lane to use for multicast. */
  uint8_t enable_multicast_bypass; /**< Enable multicast
                                      bypass. */
  uint16_t positionIndex; /**< Position index into lane
                             specific subscription */
  bool update_fpga; /**< True if FPGA should be updated,
                       false otherwise */
} sc_multicast_subscription_t;

struct EkaMcState {
  uint32_t ip;
  uint16_t port;
  int8_t coreId; /** lane */
  uint8_t pad;
  uint64_t pktCnt;
};

struct EkaHwMcState {
  uint32_t ip;
  uint16_t port;
  uint8_t coreId; /** lane */
  uint8_t pad;
  uint16_t positionIndex; /**< Position index into lane
                             specific subscription */
  uint16_t channel;       /**< UDP channel to use for this
                             subcscription */
};

#endif
