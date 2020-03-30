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

#include "main.h"
#include "arp.h"

void arp_context_construct(arp_context_t * pArpContext, const device_context_t * pDevExt)
{
    memset(pArpContext, 0, sizeof(*pArpContext));
    spin_lock_init(&pArpContext->spinlock);
    pArpContext->pDevExt = pDevExt;
    // Default ARP entry timeout is ?? seconds. See /proc/sys/net/ipv4/route/gc_timeout or /proc/sys/net/ipv4/neigh/default/gc_stale_time
    pArpContext->timeout_in_jiffies = 0; // Per default ARP entries will not time out
    pArpContext->arp_table_is_empty = true;
    pArpContext->arp_handled_by_user_space = false; // Per default user space handles ARP.
}

void arp_context_destruct(arp_context_t * pArpContext)
{
    memset(pArpContext, 0, sizeof(*pArpContext));
}

/**
 *  Try to lock the or return false if lock could not be obtained.
 *
 *  @param  pArpContext     Pointer to ARP context.
 *
 *  @return     True if lock was acquired, false otherwise.
 */
static inline bool arp_try_lock(arp_context_t * pArpContext)
{
    return spin_trylock(&pArpContext->spinlock) != 0;
}

/**
 *  Lock the ARP context.
 *
 *  @param  pArpContext     Pointer to ARP context.
 */
static inline void arp_lock(arp_context_t * pArpContext)
{
    spin_lock(&pArpContext->spinlock);
}

/**
 *  Unlock the ARP context.
 *
 *  @param  pArpContext     Pointer to ARP context.
 */
static inline void arp_unlock(arp_context_t * pArpContext)
{
    spin_unlock(&pArpContext->spinlock);
}

static int arp_find_mac_address_locked(arp_context_t * pArpContext, uint32_t ip_address, uint8_t mac_address[MAC_ADDR_LEN], bool lock)
{
    int i, startIndex;
    int oldestTimeStampIndex = 0;
    uint64_t oldestTimeStamp;

    if (lock)
    {
        arp_lock(pArpContext);
    }

    if (pArpContext->arp_table_is_empty)
    {
        if (lock)
        {
            arp_unlock(pArpContext);
        }
        return -1; // ARP table is empty so there is nothing to find.
    }

    oldestTimeStamp = pArpContext->arp_table[oldestTimeStampIndex].age_timestamp;
    startIndex = pArpContext->oldest_time_stamp_index;

    for (i = 0; i < number_of_elements(pArpContext->arp_table); ++i)
    {
        int index = (startIndex + i) % number_of_elements(pArpContext->arp_table);
        arp_entry_t * pArpEntry = &pArpContext->arp_table[index];

        if (pArpEntry->age_timestamp < oldestTimeStamp)
        {
            oldestTimeStamp = pArpEntry->age_timestamp;
            oldestTimeStampIndex = index;
        }
        if (pArpEntry->ip_address == ip_address)
        {
            pArpEntry->age_timestamp = jiffies; // Refresh timestamp making this the newest entry
            if (mac_address != NULL)
            {
                memcpy(mac_address, pArpEntry->mac_address, MAC_ADDR_LEN);
            }

            // We have now lost position of the oldest entry but next search starts from next entry:
            pArpContext->oldest_time_stamp_index = (oldestTimeStampIndex + 1) % number_of_elements(pArpContext->arp_table);

            if (lock)
            {
                arp_unlock(pArpContext);
            }

            return index;
        }
    }

    // No IP address found. However, remember the oldest entry seen.
    pArpContext->oldest_time_stamp_index = oldestTimeStampIndex;

    if (lock)
    {
        arp_unlock(pArpContext);
    }

    return -1;
}

int arp_find_mac_address(arp_context_t * pArpContext, uint32_t ip_address, uint8_t mac_address[MAC_ADDR_LEN])
{
    return arp_find_mac_address_locked(pArpContext, ip_address, mac_address, true);
}

unsigned arp_register_ip_address(arp_context_t * pArpContext, uint32_t ip_address)
{
    int index;
    arp_entry_t * pArpEntry;

    arp_lock(pArpContext);

    // Find next empty table position:
    index = arp_find_mac_address_locked(pArpContext, 0, NULL, false);
    if (index == -1)
    {
        // No empty entries in table; use the entry with the oldest timestamp.
        index = pArpContext->oldest_time_stamp_index;
    }
    pArpEntry = &pArpContext->arp_table[index];

    pArpEntry->ip_address = ip_address;
    memset(pArpEntry->mac_address, 0, MAC_ADDR_LEN);
    pArpEntry->age_timestamp = jiffies;
    pArpContext->arp_table_is_empty = false;

    arp_unlock(pArpContext);

    if (LOG(LOG_ARP))
    {
        IPBUF(0);
        const device_context_t * pDevExt = pArpContext->pDevExt;
        PRINTK("Registering for ARP IP %s at position %d\n", LogIP(0, ip_address), index);
    }

    return index;
}

void arp_insert_mac_address(arp_context_t * pArpContext, uint32_t ip_address, const uint8_t mac_address[MAC_ADDR_LEN])
{
    const int Retries = 10;
    int index, retries = Retries;

    while (retries-- > 0)
    {
        arp_entry_t * pArpEntry;
        uint8_t zero_mac_address[MAC_ADDR_LEN];

        index = arp_find_mac_address_locked(pArpContext, ip_address, zero_mac_address, false);
        if (index == -1)
        {
            // Nothing found use the oldest entry
            pArpEntry = &pArpContext->arp_table[pArpContext->oldest_time_stamp_index];
        }
        else
        {
            // Matching IP address found
            pArpEntry = &pArpContext->arp_table[index];

            if (MacAddressIsNotZero(zero_mac_address))
            {
                if (MacAddressesAreEqual(mac_address, zero_mac_address))
                {
                    if (LOG(LOG_ARP))
                    {
                        IPBUF(0); MACBUF(1); MACBUF(2);
                        const device_context_t * pDevExt = pArpContext->pDevExt;
                        PRINTK("arp_insert_mac_address: IP address %s new MAC %s already exists as MAC %s; not inserted.\n",
                            LogIP(0, ip_address), LogMAC(1, mac_address), LogMAC(2, zero_mac_address));
                    }
                    break;
                }
                if (LOG(LOG_ARP))
                {
                    IPBUF(0); MACBUF(1); MACBUF(2);
                    const device_context_t * pDevExt = pArpContext->pDevExt;
                    PRINTK("arp_insert_mac_address: IP address %s new MAC %s replaces existing different MAC %s\n",
                        LogIP(0, ip_address), LogMAC(1, mac_address), LogMAC(2, zero_mac_address));
                }
            }
        }

        arp_lock(pArpContext);

        if (pArpEntry->ip_address == 0 || pArpEntry->ip_address == ip_address)
        {
            /* Under lock the entry is still free or is an existing entry. */
            /* If not then we retry by finding a new index. */
            pArpEntry->ip_address = ip_address;
            memcpy(pArpEntry->mac_address, mac_address, MAC_ADDR_LEN);
            pArpEntry->age_timestamp = jiffies;
            pArpContext->arp_table_is_empty = false;

            arp_unlock(pArpContext);

            break;
        }

        arp_unlock(pArpContext);
    }

    if (retries <= 0)
    {
        IPBUF(0); MACBUF(1);
        const device_context_t * pDevExt = pArpContext->pDevExt;
        PRINTK_ERR("arp_insert_mac_address: failed to insert IP %s MAC address %s in %d retries\n",
            LogIP(0, ip_address), LogMAC(1, mac_address), Retries);
    }
    if (LOG(LOG_ARP))
    {
        IPBUF(0); MACBUF(1);
        const device_context_t * pDevExt = pArpContext->pDevExt;
        PRINTK("arp_insert_mac_address: inserted IP %s MAC %s at position %d; retries %d of max %d\n",
            LogIP(0, ip_address), LogMAC(1, mac_address), index, retries, Retries);
    }
}

void arp_timeout(arp_context_t * pArpContext)
{
    int i, startIndex, usedEntriesCount = 0, timedOutEntriesCount = 0;

    if (!arp_try_lock(pArpContext))
    {
        return; // Could not lock, let's try again next time.
    }

    if (pArpContext->arp_table_is_empty)
    {
        arp_unlock(pArpContext);
        return; // ARP table is already empty; nothing to do here!
    }

    startIndex = pArpContext->oldest_time_stamp_index;
    for (i = 0; i < number_of_elements(pArpContext->arp_table); ++i)
    {
        int index = (startIndex + i) % number_of_elements(pArpContext->arp_table);
        arp_entry_t * pArpEntry = &pArpContext->arp_table[index];

        if (pArpEntry->age_timestamp != 0)
        {
            ++usedEntriesCount;

            if (jiffies - pArpEntry->age_timestamp >= pArpContext->timeout_in_jiffies)
            {
                pArpEntry->ip_address = 0;
                memset(pArpEntry->mac_address, 0, MAC_ADDR_LEN);
                pArpEntry->age_timestamp = 0;

                /* New free entry available, use that in next call */
                pArpContext->oldest_time_stamp_index = index;

                ++timedOutEntriesCount;

                /* Only timeout one entry at a time to limit how long we keep the lock.
                   As sc_timer_callback calls this function 10 times every second it can take 
                   timeout_in_jiffies (default 60 seconds) + MAX_ARP_ENTRIES / 10 seconds
                   which equals 66 seconds with the default values to clear the whole ARP table. */
                break;
            }
        }
    }

    if (usedEntriesCount == 0)
    {
        pArpContext->arp_table_is_empty = true;
    }

    arp_unlock(pArpContext);

    if (LOG(LOG_ARP))
    {
        const device_context_t * pDevExt = pArpContext->pDevExt;
        if (usedEntriesCount == 0)
        {
            PRINTK("ARP table is now empty!\n");
        }
        if (timedOutEntriesCount > 0)
        {
            PRINTK("%d ARP entries timed out\n", timedOutEntriesCount);
        }
    }
}

void arp_dump(arp_context_t * pArpContext, bool all)
{
    int i;
    const device_context_t * pDevExt = pArpContext->pDevExt;
    uint64_t oldestTimeStamp = pArpContext->arp_table[0].age_timestamp;

    uint64_t timeoutInMilliseconds = pArpContext->timeout_in_jiffies / HZ;
    timeoutInMilliseconds *= 1000;
    PRINTK("--- ARP table %s, %s, oldest entry index is %d, timeout %llu jiffies (%llu milliseconds), HZ = %u:\n",
        pArpContext->arp_table_is_empty ? "is empty" : "is not empty",
        pArpContext->arp_handled_by_user_space ? "ARP is handled in API library" : "ARP is handled in driver",
        pArpContext->oldest_time_stamp_index, pArpContext->timeout_in_jiffies, timeoutInMilliseconds, HZ);
    for (i = 0; i < number_of_elements(pArpContext->arp_table); ++i)
    {
        arp_entry_t arpEntry;

        // Ensure consistent state of single entries only:
        arp_lock(pArpContext);
        arpEntry = pArpContext->arp_table[i];
        arp_unlock(pArpContext);

        if (all || arpEntry.age_timestamp != 0)
        {
            IPBUF(0); MACBUF(1);

            // Show timestamp relative to a minimum which might be somewhat out-of-date but the resulting timestamps
            // should be smaller numbers if arp_dump is called relatively often.
            PRINTK("    %3d: %s has MAC address %s; timestamp %llu jiffies\n", i, LogIP(0, arpEntry.ip_address),
                LogMAC(1, arpEntry.mac_address), arpEntry.age_timestamp/* - pArpContext->oldest_timestamp*/);
        }

        if (oldestTimeStamp < arpEntry.age_timestamp)
        {
            oldestTimeStamp = arpEntry.age_timestamp;
        }
    }

    pArpContext->oldest_timestamp = oldestTimeStamp;
}
