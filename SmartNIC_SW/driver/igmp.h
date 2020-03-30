#ifndef IGMP_H
#define IGMP_H

/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with a
 *  Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#define MAX_NO_MGROUPS 64

#define IGMP_MEMBERSHIP_QUERY     0x11
#define IGMP_V2_MEMBERSHIP_REPORT 0x16
#define IGMP_LEAVE_GROUP          0x17

/**
 *  Special multicast addresses (https://en.wikipedia.org/wiki/Multicast_address)
 */
#define IGMP_ALL_HOSTS      0xe0000001  ///< All hosts multicast group 224.0.0.1
#define IGMP_ALL_ROUTERS    0xe0000002  ///< All routers multicast group 224.0.0.2

#include "main.h"

/// Private data
typedef struct
{
    uint32_t group_address;         ///< Multicast group
    uint32_t last_report_seen;      ///< When de we last seen a report for this group
    uint8_t flags;                  ///< Flags used internal in the igmp state machine
    uint8_t state;                  ///< Current state
    uint8_t use_count;              ///< Use count
    struct timer_list timer;        ///< Timer
    struct igmp_context *context;   ///< IGMP context
    int16_t vlanTag;                ///< VLAN tag or -1 for no VLAN
    uint32_t sourceVlanIpAddress;   ///< Source (local) IP address to use when VLAN is in use or zero to use local IP address.
    uint8_t lane;                   ///< Lane used by this group
    enum
    {
        IGMP_SEND_NONE,
        IGMP_SEND_REPORT,
        IGMP_SEND_LEAVE
    } sendType;                     ///< send_report or send_leave operation that is executed outside of the @ref groupsLock
} igmp_info_t;

typedef struct
{
    uint32_t    group_address;              /**< Multicast group address of this subscription */
    uint16_t    ip_port_number;             /**< Multicast IP port number */
    uint16_t    channel;                    /**< UDP channel to use for this subcscription */
    int16_t     vlanTag;                    /**< VLAN tag to use for this subscription or -1 for no VLAN */
    uint8_t     lane;                       /**< Lane to use for multicast. */
    uint8_t     enable_multicast_bypass;    /**< Enable multicast bypass. */
    uint16_t    positionIndex;              /**< Position index into lane specific subscription */
    bool        update_fpga;                /**< True if FPGA should be updated, false otherwise */
} sc_multicast_subscription_t;

struct igmp_context
{
    device_context_t * pDevExt;                                /**< Back pointer to containing device context */
    igmp_info_t mgroups[SC_NO_NETWORK_INTERFACES * MAX_NO_MGROUPS];  /**< All groups */
    spinlock_t groupsLock;                                      /**< Protects access to the @ref mgroups above. */

    // Multicast subscription (dest addr, dest port) - the global list contains ALL multicast subscriptions from all UDP channels and network interfaces
    spinlock_t subscriberLock;                                  /**< Protects access to below global subscriber information. */
    sc_multicast_subscription_t global_subscriber[SC_NO_NETWORK_INTERFACES * FB_MAX_NO_SUBSCRIPTIONS];
};

// Public interface

int handle_igmp_ioctl(struct igmp_context *context, fb_channel_t * channel, sc_igmp_info_t *value);

int init_igmp(device_context_t * pDevExt, sc_net_interface_t *nif);

int cleanup_igmp(struct igmp_context * context);

// Called when we received a query
bool query_received(struct igmp_context *context, uint8_t lane, int16_t vlanTag, uint32_t g, uint32_t mrt);
// Called when we received a report
bool report_received(struct igmp_context *context, uint8_t lane, int16_t vlanTag, uint32_t g);

void igmp_cleanup_channel_subscriptions(struct igmp_context *context, fb_channel_t * channel);

#endif /* IGMP_H */
