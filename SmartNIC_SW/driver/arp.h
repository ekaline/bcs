#ifndef __arp_h__
#define __arp_h__

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

#if !defined(MAX_ARP_ENTRIES) || SC_IS_EMPTY(MAX_ARP_ENTRIES)
    #undef MAX_ARP_ENTRIES
    #define MAX_ARP_ENTRIES         SC_MAX_TCP_CHANNELS /* Per default one ARP entry per TOE channel. */
#endif

/**
 *  A single ARP entry in the ARP table.
 */
typedef struct
{
    uint32_t    ip_address;                         /**< Remote IP address in host byte order. */
    uint8_t     mac_address[MAC_ADDR_LEN];          /**< Remote MAC address. */
    uint64_t    age_timestamp;                      /**< Entry age timestamp or zero for free entry. */

} arp_entry_t;

/**
 *  ARP information context.
 */
typedef struct
{
    const device_context_t *   pDevExt;                    /**< Pointer to the device context. */
    spinlock_t                  spinlock;                   /**< Spinlock protecteding access to @ref arp_context_t. */
    int                         oldest_time_stamp_index;    /**< Index of entry with oldest timestamp in the table. */
    uint64_t                    oldest_timestamp;           /**< Oldest timestamp in ARP tbale. Only used by @ref arp_dump function. */
    uint64_t                    timeout_in_jiffies;         /**< Timeout in jiffies when an entry is zeroes. */
    arp_entry_t                 arp_table[MAX_ARP_ENTRIES]; /**< Table of ARP entries. */
    bool                        arp_table_is_empty;         /**< ARP table is empty if true, ARP table contains entries if false. */
    bool                        arp_handled_by_user_space;  /**< True if ARP is handled in user space API library, false if ARP is handled in driver. */

} arp_context_t;

/**
 *  Initialize ARP context.
 *
 *  @param  pArpContext     Pointer to ARP context.
 *  @param  pDevExt         Pointer to device context.
 */
void arp_context_construct(arp_context_t * pArpContext, const device_context_t * pDevExt);

/**
 *  Clean up ARP context.
 *
 *  @param  pArpContext     Pointer to ARP context.
 */
void arp_context_destruct(arp_context_t * pArpContext);

/**
 *  Find a MAC address for a given IP address.
 *
 *  @param  pArpContext     Pointer to ARP context.
 *  @param  ip_address      IP address in host byte order to search for.
 *  @param  mac_address     Pointer to the corresponding MAC address if IP is found, undefined contents otherwise.
 *                          MAC address is zeroed for a registered IP address that waits for ARP reply or
 *                          is the MAC address of a resolved remote host.
 *
 *  @return     Returns a positive index of found entry or -1 if not found.
 */
int arp_find_mac_address(arp_context_t * pArpContext, uint32_t ip_address, uint8_t mac_address[MAC_ADDR_LEN]);

/**
 *  Register an IP address in the ARP table that is waiting for an ARP reply.
 *
 *  @param  pArpContext     Pointer to ARP context.
 *  @param  ip_address      IP address in host byte order waiting ARP reply.
 *
 *  @return                 The index of the registered entry in the ARP table.
 */
unsigned arp_register_ip_address(arp_context_t * pArpContext, uint32_t ip_address);

/**
 *  Insert a new IP address and MAC address pair into the table.
 *
 *  @param  pArpContext     Pointer to ARP context.
 *  @param  ip_address      IP address in host byte order to insert.
 *  @param  mac_address     A nonzero MAC address to insert.
 */
void arp_insert_mac_address(arp_context_t * pArpContext, uint32_t ip_address, const uint8_t mac_address[MAC_ADDR_LEN]);

/**
 *  Process timeouts of ARP entries.
 *
 *  @param  pArpContext     Pointer to ARP context.
 */
void arp_timeout(arp_context_t * pArpContext);

/**
 *  Dump contents of ARP context to dmesg/syslog.
 *
 *  @param  pArpContext     Pointer to ARP context.
 *  @param  all             Dump whole table if true, only active entries if false.
 */
void arp_dump(arp_context_t * pArpContext, bool all);

#endif // __arp_h__
