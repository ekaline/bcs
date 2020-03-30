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

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "main.h"
#include "igmp.h"
#include "net.h"
#include "fpga.h"

enum
{
    IGMP_STATE_NONMEMBER        = 0,
    IGMP_STATE_DELAYING_MEMBER  = 1,
    IGMP_STATE_IDLE_MEMBER      = 2
};

// Forward decleration
static int send_report(const igmp_info_t * group, const char * callContext);
static int send_leave(const igmp_info_t * group, const char * callContext);
static int set_flag(igmp_info_t *group);
static int clear_flag(igmp_info_t *group);
static int start_timer(igmp_info_t *group, unsigned int delay, const char * callContext);
static int reset_timer(igmp_info_t *group, unsigned int newTime);
static int stop_timer(igmp_info_t *group);
static uint32_t random_jiffies(uint32_t max_jiffies);
static int timer_expired(igmp_info_t * group, igmp_info_t * pSendGroup);

static igmp_info_t *insert_group(struct igmp_context *context, uint8_t lane, uint32_t group_address, int16_t vlanTag, uint32_t sourceVlanIpAddress);
static int insert_subscriber(sc_multicast_subscription_t *list, size_t listLength, uint8_t lane, uint32_t addr, uint32_t port, uint32_t channel, int16_t vlanTag, bool enableMulticastBypass);
static int find_subscriber(const sc_multicast_subscription_t *list, size_t listLength, uint8_t lane, uint32_t addr, uint16_t port, int16_t vlanTag);
static int delete_subscriber(sc_multicast_subscription_t *list, size_t listLength, uint8_t lane, uint32_t addr, uint32_t port, int16_t vlanTag);

// Host want to leave group
static int leave_this_group(igmp_info_t * pGroup, uint8_t lane, int16_t vlanTag, uint32_t group_address, igmp_info_t * pSendGroup);

static inline void lock_subscriber(struct igmp_context * pContext)
{
    spin_lock(&pContext->subscriberLock);
}

static inline void unlock_subscriber(struct igmp_context * pContext)
{
    spin_unlock(&pContext->subscriberLock);
}

static inline void lock_groups(struct igmp_context * pContext)
{
    spin_lock(&pContext->groupsLock);
}

static inline bool lock_groups_with_timeout(struct igmp_context * pContext, int64_t timeout)
{
    int64_t startTime = jiffies;
    bool locked;

    timeout = msecs_to_jiffies(timeout);

    do
    {
        locked = spin_trylock(&pContext->groupsLock) == 1;
    } while (!locked && jiffies - startTime < timeout);

    return locked;
}

static inline void unlock_groups(struct igmp_context * pContext)
{
    spin_unlock(&pContext->groupsLock);
}

const int64_t LockGroupsTimeoutMilliseconds = 100;

#if 0
/**
 *  This function initializes all UDP multicast FPGA data to zero.
 *  But this is currently done by FPGA reset so we don't need this.
 *  Future possibility to save FPGA gates by doing zero initialization
 *  in software.
 */
static void init_fpga(const char * from, struct igmp_context * context)
{
    size_t i;
    uint8_t macAddress[MAC_ADDR_LEN];

    memset(macAddress, 0, sizeof(macAddress));

    for (i = 0; i < number_of_elements(context->global_subscriber); ++i)
    {
        int lane;

        for (lane = 0; lane < SC_NO_NETWORK_INTERFACES; ++lane)
        {
            if (check_valid_udp_lane(context->pDevExt, lane))
            {
                uint16_t positionIndex;

                for (positionIndex = 0; positionIndex < SC_MAX_TCP_CHANNELS; ++positionIndex)
                {
                    setSourceMulticast(from, context->pDevExt, lane, positionIndex, macAddress, 0, 0);
                }
            }
        }
    }
}
#endif

static void update_fpga(const char * from, struct igmp_context * context)
{
    size_t i;

    for (i = 0; i < number_of_elements(context->global_subscriber); ++i)
    {
        uint8_t macAddress[MAC_ADDR_LEN];
        uint16_t ipPortNumber = 0;
        uint16_t channel = 0;

        sc_multicast_subscription_t * pSubscriber = &context->global_subscriber[i];
        if (pSubscriber->update_fpga)
        {
            uint8_t lane = pSubscriber->lane;
            int16_t vlanTag = pSubscriber->vlanTag;
            uint16_t positionIndex = pSubscriber->positionIndex;
            bool enableMulticastBypass = pSubscriber->enable_multicast_bypass;

            if (pSubscriber->group_address)
            {
                // Update FPGA with info in driver structures
                multicast_to_mac(pSubscriber->group_address, macAddress);
                ipPortNumber = pSubscriber->ip_port_number;
                channel = pSubscriber->channel;
            }
            else
            {
                // Delete/cancel filter from FPGA: lane and positionIndex identify which filter is deleted.
                memset(macAddress, 0, MAC_ADDR_LEN);
                ipPortNumber = 0;
                channel = 0;
            }

            setSourceMulticast(from, context->pDevExt, lane, positionIndex, macAddress, ipPortNumber, vlanTag, channel, enableMulticastBypass);

            pSubscriber->update_fpga = false;
        }
    }
}

static const char * GetStateName(uint8_t state)
{
    switch (state)
    {
        case IGMP_STATE_NONMEMBER:          return "IGMP_STATE_NONMEMBER";
        case IGMP_STATE_DELAYING_MEMBER:    return "IGMP_STATE_DELAYING_MEMBER";
        case IGMP_STATE_IDLE_MEMBER:        return "IGMP_STATE_IDLE_MEMBER";
    }

    return " *** UKNOWN IGMP STATE ***";
};

/** Change igmp state.
 *
 * @param group pointer to the igmp group
 * @param newstate the new state
 *
 * @return 0 on success
 */
static int change_state(igmp_info_t *group, uint8_t newstate)
{
    if (group->group_address == IGMP_ALL_HOSTS)
    {
        // Group 224.0.0.1 (All hosts) always idle state.
        IPBUF(1);
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK_C(LOG(LOG_IGMP), "Group address %s always idle\n", LogIP(1, IGMP_ALL_HOSTS));
    }
    else
    {
        group->state = newstate;

        if (LOG(LOG_IGMP))
        {
            IPBUF(1);
            const device_context_t * pDevExt = group->context->pDevExt;
            PRINTK("change_state of group %s (0x%x) to %s lane %u VLAN %d\n",
                LogIP(1, group->group_address), group->group_address, GetStateName(group->state), group->lane, group->vlanTag);
        }
    }

    return 0;
}

/**
 *  Send IGMP report or IGMP leave group message if applicable.
 *
 *  @param  pSendGroup      Pointer to a @ref igmp_info_t structure.
 *  @param  callContext     Context specifying where the call comes from.
 *
 *  @return 1 if a message was sent, 0 otherwise.
 */
static int SendGroup(const igmp_info_t * pSendGroup, const char * callContext)
{
    switch (pSendGroup->sendType)
    {
        case IGMP_SEND_REPORT:
            send_report(pSendGroup, callContext);
            return 1;

        case IGMP_SEND_LEAVE:
            send_leave(pSendGroup, callContext);
            return 1;

        case IGMP_SEND_NONE:
        default:
            return 0;
    }
}

/** Initialize the igmp handling
 *
 */
int init_igmp(device_context_t * pDevExt, sc_net_interface_t * nif)
{
    igmp_info_t *group;
    struct igmp_context * context = &pDevExt->igmpContext;
    memset(context, 0, sizeof(struct igmp_context));
    context->pDevExt = pDevExt;

    if (pDevExt->udp_lane_mask == 0)
    {
        return 0; // No UDP lanes in this FPGA, don't do anything!
    }

    spin_lock_init(&context->subscriberLock);
    spin_lock_init(&context->groupsLock);

    // Insert the all host group.
    // NB. Users that want to subscribe to all host group via VLAN must do it
    // explicitly themselves via the SC_IgmpJoin call.
    group = insert_group(context, nif->network_interface, IGMP_ALL_HOSTS, NO_VLAN_TAG, 0); // 224.0.0.1, local subnetwok
    group->state = IGMP_STATE_IDLE_MEMBER;

    insert_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), nif->network_interface, 0xe0000001, 0, 0, NO_VLAN_TAG, false);

    update_fpga("init_igmp", context);

    PRINTK_C(LOG(LOG_IGMP), "IGMP context initialized on lane %u\n", nif->network_interface);

    return 0;
}

int cleanup_igmp(struct igmp_context * context)
{
    size_t i;
    unsigned int numberOfMessagesSent = 0;

    if (context->pDevExt == NULL || context->pDevExt->udp_lane_mask == 0)
    {
        return 0; // No UDP lanes in this FPGA, don't do anything!
    }

    lock_subscriber(context);

    for (i = 0; i < number_of_elements(context->mgroups); ++i)
    {
        igmp_info_t sendGroup = { 0 };
        igmp_info_t * pGroup;

        lock_groups(context);

        pGroup = &context->mgroups[i];
        if (is_valid_udp_lane(context->pDevExt, pGroup->lane) && pGroup->group_address != 0)
        {
            leave_this_group(pGroup, pGroup->lane, pGroup->vlanTag, pGroup->group_address, &sendGroup);
        }

        unlock_groups(context);

        numberOfMessagesSent += SendGroup(&sendGroup, __func__);
    }

    unlock_subscriber(context);

    if (numberOfMessagesSent > 0)
    {
        // If any messages were sent then wait for a while for messages to get sent
        // from Linux network stack because NIC DMA will be stopped soon after
        // this function call terminates.
        int milliSecondDelay = numberOfMessagesSent > 10 ? 10 : numberOfMessagesSent;
        msleep(milliSecondDelay);
    }

    if (LOG(LOG_IGMP))
    {
        const device_context_t * pDevExt = context->pDevExt;
        PRINTK("IGMP context cleaned up\n");
    }

    return 0;
}

/** Lookup igmp groups in our register
 *
 *  @param group
 *
 *  @returns pointer to group if found else NULL.
 */
static igmp_info_t * find_group(struct igmp_context *context, uint8_t lane, int16_t vlanTag, uint32_t group_address)
{
    size_t i;

    for (i = 0; i < number_of_elements(context->mgroups); ++i)
    {
        igmp_info_t * pGroup = &context->mgroups[i];

        if (pGroup->group_address == group_address &&
            pGroup->lane == lane &&
            pGroup->vlanTag == vlanTag)
        {
            /* found */
            return pGroup;
        }
    }

    /* Not found */
    return NULL;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0) /* Timer API changed in kernel 4.15 */
static void igmp_timer(unsigned long arg)
{
    igmp_info_t * group = (igmp_info_t *)arg;
#else /* Linux kernel 4.15 or higher */
static void igmp_timer(struct timer_list * pTimerList)
{
    igmp_info_t * pGroup = from_timer(pGroup, pTimerList, timer);
    igmp_info_t * group = pGroup;
#endif
    struct igmp_context * pContext = group->context;
    if (pContext != NULL)
    {
        igmp_info_t sendGroup = { 0 };

        // Now we can lock the groups
        bool locked = lock_groups_with_timeout(pContext, LockGroupsTimeoutMilliseconds);
        if (!locked)
        {
            // Could not get lock within timeout, retry after 10 milliseconds
            reset_timer(group, msecs_to_jiffies(10));
            return;
        }

        if (group->context != NULL)
        {
            // Group still exists
            timer_expired(group, &sendGroup);
        }

        unlock_groups(pContext);

        SendGroup(&sendGroup, __func__);
    }
}

/** Insert multicast in igmp context.
 *
 *  @param context igmp context
 *  @param group Group to be inserted
 *
 *  @return Pointer to multicast group with
 *          new group inserted. Pointer may be NULL so check it!
 */
static igmp_info_t *insert_group(struct igmp_context *context, uint8_t lane, uint32_t group_address,
    int16_t vlanTag, uint32_t sourceVlanIpAddress)
{
    size_t i;

    // Find first spare slot
    for (i = 0; i < number_of_elements(context->mgroups); ++i)
    {
        igmp_info_t * pGroup = &context->mgroups[i];

        if (context->mgroups[i].group_address == 0)
        {
            if (LOG(LOG_IGMP))
            {
                IPBUF(1);
                const device_context_t * pDevExt = context->pDevExt;
                PRINTK("Setup IGMP timer of group #%lu %s (0x%x) lane %u VLAN %d : %lu bytes at %p\n",
                    i, LogIP(1, group_address), group_address,
                    lane, vlanTag, sizeof(struct timer_list), &pGroup->timer);
            }
            pGroup->context = context;
            pGroup->vlanTag = vlanTag;
            pGroup->sourceVlanIpAddress = sourceVlanIpAddress;
            pGroup->lane = lane;
            pGroup->sendType = IGMP_SEND_NONE;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0) /* Timer API changed in kernel 4.15 */
            setup_timer(&pGroup->timer, igmp_timer, (unsigned long)&context->mgroups[i]);
#else
            timer_setup(&pGroup->timer, igmp_timer, 0);
#endif
            // Group is now ready, mark it so
            pGroup->group_address = group_address;

            return pGroup;
        }
    }

    // Not able to find an empty slot
    return 0;
}

/** Host joins multicast group
 *
 *  @param nif Network interface
 *  @param g Group to join
 *
 *  @return int Zero if all went well
 */
static int join_group(struct igmp_context *context, uint8_t lane, uint32_t group_address, int16_t vlanTag, uint32_t sourceVlanIpAddress, igmp_info_t * pSendGroup)
{
    igmp_info_t * group = find_group(context, lane, vlanTag, group_address);

    if (LOG(LOG_IGMP))
    {
        IPBUF(1); IPBUF(2);
        const device_context_t * pDevExt = context->pDevExt;
        PRINTK("join IGMP group %s (0x%x) lane %u VLAN %d (src addr: %s (0x%08x))", LogIP(1, group_address), group_address,
            lane, vlanTag, LogIP(2, sourceVlanIpAddress), sourceVlanIpAddress);
    }

    if (group == NULL)
    {
        /* not member */
        group = insert_group(context, lane, group_address, vlanTag, sourceVlanIpAddress);
        if (group == NULL) return -1;

        *pSendGroup = *group;
        pSendGroup->sendType = IGMP_SEND_REPORT;
        //send_report(group);
        set_flag(group);
        //start_timer(group, 10000, __func__);
        change_state(group, IGMP_STATE_DELAYING_MEMBER);

        //update_fpga(context);
    }

    group->use_count++;

    if (LOG(LOG_IGMP))
    {
        IPBUF(1);
        const device_context_t * pDevExt = context->pDevExt;
        PRINTK("joining group %s (0x%x) use count %d lane %u VLAN %d state %s\n",
            LogIP(1, group->group_address), group->group_address,
            group->use_count, group->lane, group->vlanTag, GetStateName(group->state));
    }

    return 0;
}

/** Leave multicast group
 *
 */
static int leave_group(struct igmp_context *context, uint8_t lane, int16_t vlanTag, uint32_t group_address, igmp_info_t * pSendGroup)
{
    igmp_info_t * group = find_group(context, lane, vlanTag, group_address);

    if (group == NULL) return -1;

    return leave_this_group(group, lane, vlanTag, group_address, pSendGroup);
}

static int leave_this_group(igmp_info_t * group, uint8_t lane, int16_t vlanTag, uint32_t group_address, igmp_info_t * pSendGroup)
{
    // If called from final cleanup_igmp then use_count can be zero but still need to clean-up the group (especially the timer!)
    if (group->use_count > 0)
    {
        group->use_count--;
    }

    if (LOG(LOG_IGMP))
    {
        IPBUF(1);
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK("Leave group %s(0x%x) use count  %d lane %u VLAN %d\n", LogIP(1, group->group_address), group->group_address,
            group->use_count, group->lane, group->vlanTag);
    }

    // Still in use
    if (group->use_count != 0) return 0;

    switch (group->state)
    {
        case IGMP_STATE_DELAYING_MEMBER:
            // delaying-member : leave_group do (stop_timer, send_leave if flag set) -> non-member
            stop_timer(group);
            // Fall through
        case IGMP_STATE_IDLE_MEMBER:
            // idle-member : leave_group do (send_leave if flag set) -> non-member
            if (group->flags)
            {
                *pSendGroup = *group;
                pSendGroup->sendType = IGMP_SEND_LEAVE;
                //send_leave(group);
            }
            del_timer_sync(&group->timer);
            if (LOG(LOG_IGMP))
            {
                IPBUF(1);
                const device_context_t * pDevExt = group->context->pDevExt;
                PRINTK("Deleted IGMP timer of group %s (0x%x) lane %u VLAN %d at %p\n",
                    LogIP(1, group->group_address), group->group_address,
                    group->lane, group->vlanTag, &group->timer);
            }
            memset(group, 0, sizeof(igmp_info_t));

            //update_fpga(group->context);

            break;
        case IGMP_STATE_NONMEMBER:
            break;
    }

    //update_fpga(group->context);

    return 0;
}

//  Call chain to here is:
//
//      -> fb_poll (Linux NAPI poll from kernel softirq in older kernels, from kernel task in newer kernels, not sure where the change happened)
//          -> fbProcessOobChannel
//              -> fbParseOobPacket
//                  -> fbHandleIpPacket
//                      -> report_received
//
bool report_received(struct igmp_context *context, uint8_t lane, int16_t vlanTag, uint32_t g)
{
    igmp_info_t * group;
    bool rc = true;

    if (!is_valid_udp_lane(context->pDevExt, lane))
    {
        return rc;
    }

    rc = lock_groups_with_timeout(context, LockGroupsTimeoutMilliseconds);
    if (!rc)
    {
        if (LOG(LOG_IGMP))
        {
            const device_context_t * pDevExt = context->pDevExt;
            PRINTK("IGMP report_received: could not lock groups within %llu milliseconds\n", LockGroupsTimeoutMilliseconds);
        }
        return false;
    }

    group = find_group(context, lane, vlanTag, g);
    if (group == NULL)
    {
        rc = true;
        goto exit;
    }

    switch (group->state)
    {
        case IGMP_STATE_DELAYING_MEMBER:
            stop_timer(group);
            clear_flag(group);
            change_state(group, IGMP_STATE_IDLE_MEMBER);
            break;
        default:
            break;
    }

exit:

    unlock_groups(context);

    return rc;
}

bool query_received(struct igmp_context * context, uint8_t lane, int16_t vlanTag, uint32_t g, uint32_t maxRespCode)
{
    bool rc = true;
    size_t i;
    igmp_info_t * group = NULL;
    uint32_t max_resp_time = 0;

    if (!is_valid_udp_lane(context->pDevExt, lane))
    {
        return rc;
    }

    rc = lock_groups_with_timeout(context, LockGroupsTimeoutMilliseconds);
    if (!rc)
    {
        if (LOG(LOG_IGMP))
        {
            const device_context_t * pDevExt = context->pDevExt;
            PRINTK("IGMP report_received: could not lock groups within %llu milliseconds\n", LockGroupsTimeoutMilliseconds);
        }
        return false;
    }

    // Ugly code their try to take care of both specify query g!=0 and general query g==0

    // If g specify query
    if (g)
        group = find_group(context, lane, vlanTag, g);

    // Else loop over all groups - note the break at the end of the loop so specific query don't iterate
    for (i = 0; i < number_of_elements(context->mgroups); ++i)
    {

        if (!g)     // But only if it not a specify query
        {
            group = &context->mgroups[i];
            if (group->group_address == 0) continue; // If no entry on i'th pos the confinue
        }

        if (group == NULL)
        {
            rc = true; // Return false if no group was found
            goto exit;
        }

        max_resp_time = random_jiffies(msecs_to_jiffies(maxRespCode * 100));

        switch (group->state)
        {
            case IGMP_STATE_DELAYING_MEMBER:
                //delaying-member : query_received do (reset_timer if max_resp_time < current_time) -> delaying-member
                // if (max_resp_time < current_time)
                if (jiffies + msecs_to_jiffies(maxRespCode * 100) < group->timer.expires)
                    reset_timer(group, max_resp_time);
                break;
            case IGMP_STATE_IDLE_MEMBER:
                start_timer(group, max_resp_time, __func__);
                change_state(group, IGMP_STATE_DELAYING_MEMBER);
        }

        // If specify query don't iterate in the loop
        if (g) break;
    }

exit:

    unlock_groups(context);

    return rc;
}

static int timer_expired(igmp_info_t * group, igmp_info_t * pSendGroup)
{
    //delaying-member : timer_expired do (send_report, set_flag) -> idle-member
    if (!group) return -1;

    // PRINTK("%s\n", __func__, group->context->pDevExt);

    switch (group->state)
    {
        case IGMP_STATE_DELAYING_MEMBER:
            *pSendGroup = *group;
            pSendGroup->sendType = IGMP_SEND_REPORT;
            //send_report(group);
            set_flag(group);
            change_state(group, IGMP_STATE_IDLE_MEMBER);
            break;
    }

    return 0;
}

/* ++++++++ ACTIONS ++++++++++ */

static const char * XmitErrorString(int rc)
{
    switch (rc)
    {
        case NET_XMIT_SUCCESS:  return "NET_XMIT_SUCCESS";
        case NET_XMIT_DROP:     return "NET_XMIT_DROP";
        case NET_XMIT_CN:       return "NET_XMIT_CN";
#ifdef NET_XMIT_POLICED // This error code was removed in Linux kernel 4.8
        case NET_XMIT_POLICED:  return "NET_XMIT_POLICED";
#endif
        default:                return "*** UNKNOWN XMIT ERROR ***";
    }
}

static int send_report(const igmp_info_t * group, const char * callContext)
{
    int rc;
    struct igmp_context *context = group->context;
    const device_context_t * pDevExt = context->pDevExt;
    const sc_net_interface_t * nif = &pDevExt->nif[group->lane];
    uint32_t srcIp = group->vlanTag == NO_VLAN_TAG ? get_local_ip(nif) : group->sourceVlanIpAddress;
    IPBUF(1);

    PRINTK_C(LOG(LOG_IGMP), "%s from %s for group %s(0x%x) lane %u VLAN %d, srcIp = 0x%x\n", __func__, callContext,
        LogIP(1, group->group_address), group->group_address, group->lane, group->vlanTag,srcIp);

    assert(nif->lane == group->lane);

    rc = sendIgmpReport(nif, srcIp, group->group_address, group->vlanTag);
    if (rc != NET_XMIT_SUCCESS)
    {
        PRINTK_ERR_C(LOGANY(LOG_ERROR), "%s from %s failed with rc %d (%s) for group %s(0x%x) lane %u VLAN %d, srcIp = 0x%x\n",
            __func__, callContext, rc, XmitErrorString(rc), LogIP(1, group->group_address), group->group_address,
		     group->lane, group->vlanTag,srcIp);
    }

    return rc;
}

static int send_leave(const igmp_info_t * group, const char * callContext)
{
    int rc;
    const device_context_t * pDevExt = group->context->pDevExt;
    const sc_net_interface_t * nif = &pDevExt->nif[group->lane];
    uint32_t srcIp = group->vlanTag == NO_VLAN_TAG ? get_local_ip(nif) : group->sourceVlanIpAddress;

    assert(nif->lane == group->lane);

    rc = sendIgmpLeave(nif, srcIp, group->group_address, group->vlanTag);
    if (rc != NET_XMIT_SUCCESS)
    {
        PRINTK_ERR_C(LOGALL(LOG_IGMP | LOG_ERROR), "%s from %s failed with rc %d for group %x lane %u VLAN %d\n",
            __func__, callContext, rc, group->group_address, group->lane, group->vlanTag);
    }

    {
        IPBUF(1);
        PRINTK_C(LOG(LOG_IGMP), "%s from %s for group %s(0x%x) lane %u VLAN %d\n", __func__, callContext,
            LogIP(1, group->group_address), group->group_address, group->lane, group->vlanTag);
    }

    return rc;
}

static int set_flag(igmp_info_t * group)
{
    if (LOG(LOG_IGMP))
    {
        IPBUF(1);
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK("send_flags for group %s (0x%x) lane %u VLAN %d\n", LogIP(1, group->group_address), group->group_address,
            group->lane, group->vlanTag);
    }

    group->flags = 1;
    return 0;
}

static int clear_flag(igmp_info_t * group)
{
    if (LOG(LOG_IGMP))
    {
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK("clear_flags for group %x lane %u VLAN %d\n", group->group_address, group->lane, group->vlanTag);
    }

    group->flags = 0;
    return 0;
}

static int start_timer(igmp_info_t * group, unsigned int delay, const char * callContext)
{
    if (LOG(LOG_IGMP))
    {
        IPBUF(1);
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK("%s from %s for group %s (0x%x) delay %d lane %u VLAN %d\n", __func__,
            callContext, LogIP(1, group->group_address), group->group_address, delay, group->lane, group->vlanTag);
    }

    mod_timer(&group->timer, jiffies + delay);
    return 0;
}

static int reset_timer(igmp_info_t *group, uint32_t newTimeInJiffies)
{
    if (LOG(LOG_IGMP))
    {
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK("%s for group %x lane %u VLAN %d new time %u ms\n", __func__,
            group->group_address, group->lane, group->vlanTag, jiffies_to_msecs(newTimeInJiffies));
    }

    mod_timer(&group->timer, jiffies + newTimeInJiffies);
    return 0;
}

static int stop_timer(igmp_info_t *group)
{
    if (LOG(LOG_IGMP))
    {
        const device_context_t * pDevExt = group->context->pDevExt;
        PRINTK("stop_timer for group %x lane %u VLAN %d\n", group->group_address, group->lane, group->vlanTag);
    }

    del_timer_sync(&group->timer);
    return 0;
}

/* State machine

   non-member : join_group  do (send_report, set_flag, start_timer)  -> delaying-member

   delaying-member : leave_group do (stop_timer, send_leave if flag set) -> non-member

   delaying-member : report_recive do (stop_timer, clear_flag) -> idle-member

   delaying-member : timer_expired do (send_report, set_flag) -> idle-member

   delaying-member : query_received do (reset_timer if max_resp_time < current_time) -> delaying-member

   idle-member : leave_group do (send_leave if flag set) -> non-member

   idle-member : query_receive do (start_timer) -> delaying-member

 */

static uint32_t random_jiffies(uint32_t max_jiffies)
{
    uint32_t v;
    get_random_bytes(&v, sizeof(v));
    return v % max_jiffies;
}

static int find_subscriber(const sc_multicast_subscription_t * list, size_t listLength, uint8_t lane, uint32_t group_address, uint16_t ip_port_number, int16_t vlanTag)
{
    size_t i;

    for (i = 0; i < listLength; ++i)
    {
        const sc_multicast_subscription_t * pSubscriber = &list[i];

        // Ip address match and port is either wildcard or actual port number
        if (pSubscriber->group_address == group_address &&
            (pSubscriber->ip_port_number == 0 || pSubscriber->ip_port_number == ip_port_number) &&
            pSubscriber->vlanTag == vlanTag &&
            pSubscriber->lane == lane)
        {
            return i;
        }
    }

    // Not found
    return -1;
}

static int insert_subscriber(sc_multicast_subscription_t *list, size_t listLength, uint8_t lane,
    uint32_t group_address, uint32_t ip_port_number, uint32_t channel, int16_t vlanTag, bool enableMulticastBypass)
{
    size_t i;

    for (i = 0; i < listLength; ++i)
    {
        sc_multicast_subscription_t * pSubscriber = &list[i];

        // Ip address match and port is either wildcard or actual port number
        if (pSubscriber->group_address == 0)
        {
            pSubscriber->group_address = group_address;
            pSubscriber->ip_port_number = ip_port_number;
            pSubscriber->channel = channel;
            pSubscriber->vlanTag = vlanTag;
            pSubscriber->lane = lane;
            pSubscriber->positionIndex = i;
            pSubscriber->enable_multicast_bypass = enableMulticastBypass;
            pSubscriber->update_fpga = true;
            return i;
        }
    }

    // Not found
    return -1;
}

static int delete_subscriber(sc_multicast_subscription_t *list, size_t listLength, uint8_t lane, uint32_t group_address, uint32_t ip_port_number, int16_t vlanTag)
{
    size_t i;

    for (i = 0; i < listLength; ++i)
    {
        sc_multicast_subscription_t * pSubscriber = &list[i];

        if (pSubscriber->group_address == group_address && pSubscriber->ip_port_number == ip_port_number &&
            pSubscriber->vlanTag == vlanTag && pSubscriber->lane == lane)
        {
            pSubscriber->group_address = 0;
            pSubscriber->ip_port_number = 0;
            pSubscriber->channel = 0;
            pSubscriber->vlanTag = NO_VLAN_TAG;
            pSubscriber->lane = NO_NIF;
            pSubscriber->positionIndex = 0;
            pSubscriber->enable_multicast_bypass = 0;
            pSubscriber->update_fpga = true;
            return i;
        }
    }

    // not found
    return -1;
}

int handle_igmp_ioctl(struct igmp_context *context, fb_channel_t * channel, sc_igmp_info_t * pValue)
{
    igmp_info_t sendGroup = { 0 };
    const device_context_t * pDevExt = context->pDevExt;

    if (!is_valid_udp_lane(pDevExt, pValue->lane))
    {
        PRINTK_ERR("Igmp: %d is not an UDP lane!\n", pValue->lane);
        return -EINVAL;
    }

    if (pValue->vlan_tag > 4094 || (pValue->vlan_tag <= 0 && pValue->vlan_tag != NO_VLAN_TAG))
    {
        PRINTK_ERR("Igmp: %d is not a valid VLAN tag!\n", pValue->vlan_tag);
        return -EINVAL;
    }

    switch (pValue->action)
    {
        case IGMP_JOIN:
	  if (LOG(LOG_IGMP))
            {
                IPBUF(1);
                PRINTK("Igmp join called with group %s (0x%08x) IP port %u lane %u VLAN %d\n",
                    LogIP(1, pValue->group_address), pValue->group_address, pValue->ip_port_number, pValue->lane, pValue->vlan_tag);
            }

            // Check is it already on global list?
            lock_subscriber(context);

            if (find_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), pValue->lane,
                pValue->group_address, pValue->ip_port_number, pValue->vlan_tag) != -1)
            {
                unlock_subscriber(context);
                if (LOG(LOG_ERROR | LOG_IGMP))
                {
                    PRINTK_ERR("Already subscribed to group 0x%08x at port %u VLAN %d\n", pValue->group_address, pValue->ip_port_number,
                        pValue->vlan_tag);
                }
                return -EINVAL;
            }
            // Not in global list, insert to both global subscribers and channel subscribers
            insert_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), pValue->lane,
                pValue->group_address, pValue->ip_port_number, channel->dma_channel_no, pValue->vlan_tag, pValue->enable_multicast_bypass);
            insert_subscriber(channel->subscriber, number_of_elements(channel->subscriber), pValue->lane,
                pValue->group_address, pValue->ip_port_number, channel->dma_channel_no, pValue->vlan_tag, pValue->enable_multicast_bypass);

            update_fpga("handle_igmp_ioctl", context);

            unlock_subscriber(context);

            lock_groups(context);
            join_group(context, pValue->lane, pValue->group_address, pValue->vlan_tag, pValue->source_vlan_ip_address, &sendGroup);
            unlock_groups(context);

            break;

        case IGMP_LEAVE:
            if (LOG(LOG_IGMP))
            {
                IPBUF(1);
                PRINTK("Igmp leave called with group %s(0x%08x) IP port %u lane %u VLAN %d\n",
                    LogIP(1, pValue->group_address), pValue->group_address, pValue->ip_port_number, pValue->lane, pValue->vlan_tag);
            }

            // Check is it already on global list?
            lock_subscriber(context);

            if (find_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), pValue->lane, pValue->group_address, pValue->ip_port_number, pValue->vlan_tag) == -1)
            {
                unlock_subscriber(context);
                if (LOG(LOG_ERROR | LOG_IGMP))
                {
                    PRINTK_ERR("Cannot leave, not global subscribed to group 0x%08x at port %u VLAN %d\n", pValue->group_address, pValue->ip_port_number, pValue->vlan_tag);
                }
                return -EINVAL;
            }
            if (find_subscriber(channel->subscriber, number_of_elements(channel->subscriber), pValue->lane, pValue->group_address, pValue->ip_port_number, pValue->vlan_tag) == -1)
            {
                unlock_subscriber(context);
                if (LOG(LOG_ERROR | LOG_IGMP))
                {
                    PRINTK_ERR("Cannot leave, not subscribed to group 0x%08x at port %u VLAN %d\n", pValue->group_address, pValue->ip_port_number, pValue->vlan_tag);
                }
                return -EINVAL;
            }
            // Found in global subscribers and channel subscribers, delete from both
            delete_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), pValue->lane, pValue->group_address, pValue->ip_port_number, pValue->vlan_tag);
            delete_subscriber(channel->subscriber, number_of_elements(channel->subscriber), pValue->lane, pValue->group_address, pValue->ip_port_number, pValue->vlan_tag);

            update_fpga("handle_igmp_ioctl", context);

            unlock_subscriber(context);

            lock_groups(context);
            leave_group(context, pValue->lane, pValue->vlan_tag, pValue->group_address, &sendGroup);
            unlock_groups(context);

            break;

        default:
            return -EINVAL;
    }

    SendGroup(&sendGroup, __func__);

    return 0;
}

/**
 *  Clean up any left scubscription for the channel.
 *
 *  @param  context     Pointer to a @ref igmp_context structure.
 *  @param  channel     Pointer to a @ref fb_channel_t structure.
 *
 *  @return             Number of IGMP messages sent out while cleaning up.
 */
void igmp_cleanup_channel_subscriptions(struct igmp_context *context, fb_channel_t * channel)
{
    unsigned int i = 0;
    unsigned int numberOfMessagesSent = 0;

    if (context->pDevExt == NULL)
    {
        return; // No device context, don't do anything!
    }
    if (context->pDevExt->udp_lane_mask == 0)
    {
        return; // No UDP lanes in this firmware, don't do anything!
    }

    lock_subscriber(context);

    for (i = 0; i < number_of_elements(channel->subscriber); ++i)
    {
        sc_multicast_subscription_t * pSubscriber = &channel->subscriber[i];

        if (is_valid_udp_lane(context->pDevExt, pSubscriber->lane) && pSubscriber->group_address)
        {
            igmp_info_t sendGroup = { 0 };
            sc_igmp_info_t value;

            value.group_address = pSubscriber->group_address;
            value.ip_port_number = pSubscriber->ip_port_number;
            value.vlan_tag = pSubscriber->vlanTag;
            value.lane = pSubscriber->lane;

            if (find_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), value.lane, value.group_address, value.ip_port_number, value.vlan_tag) == -1)
            {
                const device_context_t * pDevExt = context->pDevExt;
                PRINTK_ERR("Subscription entry not on global list but on channel list\n");
            }

            lock_groups(context);
            leave_group(context, value.lane, value.vlan_tag, value.group_address, &sendGroup);
            unlock_groups(context);

            numberOfMessagesSent += SendGroup(&sendGroup, __func__);

            delete_subscriber(context->global_subscriber, number_of_elements(context->global_subscriber), value.lane, value.group_address, value.ip_port_number, value.vlan_tag);
            pSubscriber->group_address = 0;
            pSubscriber->ip_port_number = 0;
            pSubscriber->vlanTag = NO_VLAN_TAG;
            // Do NOT change channel->subscriber[i].lane!
            // That is used in update_fpga(...) to cancel the subscription in the FPGA from the correct lane!
        }
    }
    update_fpga("igmp_cleanup_channel_subscriptions", context);

    unlock_subscriber(context);

    if (numberOfMessagesSent > 0)
    {
        // If any messages were sent then wait for a while for messages to get sent
        // from Linux network stack because NIC DMA will be stopped soon after
        // this function call terminates.
        int milliSecondDelay = numberOfMessagesSent > 10 ? 10 : numberOfMessagesSent;
        msleep(milliSecondDelay);
    }
}
