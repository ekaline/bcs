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

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/mman.h>
#include <linux/version.h>
#include <linux/fs.h>

#include "channel.h"
#include "fpga.h"


volatile uint64_t reg_read_cnt = 0; // FIXME remove
volatile uint64_t last_cnt = 0; // FIXME remove

static uint64_t
get_dest_consumed_bytes_fast(fb_channel_t * pChannel)
{
    tx_context_t *tx = &pChannel->tx;
    return tx->last_known_consumed_value;
}

static uint64_t
get_dest_consumed_bytes_precise(fb_channel_t * pChannel)
{
    const device_context_t * pDevExt = pChannel->pDevExt;
    tx_context_t *tx = &pChannel->tx;
    uint32_t consumed_bytes = get_consumed_counter(pDevExt, pChannel->dma_channel_no);
    uint64_t cb64;
    uint32_t sentbytes_low32b = pChannel->tx.sent_bytes & 0xffffffff;

    __sync_fetch_and_add(&reg_read_cnt, 1);

    PRINTK_C(LOG(LOG_TX_REGISTER_WRAP_AROUND | LOG_DETAIL), " consumed_bytes = 0x%08X tx.sent_bytes = 0x%08llX\n",
        consumed_bytes, (pChannel->tx.sent_bytes & 0xFFFFFFFF));

    // Check if the sent_bytes 32bits counter have wrapped around
    if (likely(consumed_bytes <= sentbytes_low32b))
    {
        cb64 = (pChannel->tx.sent_bytes & 0xffffffff00000000);
        cb64 |= consumed_bytes;
    }
    else
    {
        PRINTK_C(LOG(LOG_TX_REGISTER_WRAP_AROUND), "dest_id %d: WARNING wrapping of consumed_bytes register!! cons32: %u sent32: %u\n",
            pChannel->dma_channel_no, consumed_bytes, sentbytes_low32b);

        cb64 = (pChannel->tx.sent_bytes & 0xffffffff00000000);
        cb64 -= 0x100000000;
        cb64 |= consumed_bytes;
    }

    tx->last_known_consumed_value = cb64;
    return cb64;
}

static uint64_t
make_packet_descriptor(fb_channel_t * pChannel, int bucket_no, int len)
{
    uint64_t  desc = pChannel->dmaBucket[bucket_no];
    desc |= ((uint64_t)len) << 40;  // 16bits length
    desc |= ((uint64_t)0) << 56;  // channel id (not used)
    desc |= ((uint64_t)1) << 63;  // 1=single packet
    return desc;
}

static int
check_for_consumed_fifo_entries(fb_channel_t * pChannel)
{
    tx_context_t *tx = &pChannel->tx;
    uint64_t consumed_bytes = get_dest_consumed_bytes_fast(pChannel);

    /*
        if (pChannel->reserved_fifo_entries_normal == 0)
        {
            const device_context_t * pDevExt = pChannel->pDevExt;
            PRINTK_ERR("DMA channel %d has 0 allocated fifo entries\n", pChannel->dma_channel_no);
            return 0;
        }
    */

    if (LOG(LOG_FLOW_CONTROL | LOG_DETAIL))
    {
        const device_context_t * pDevExt = pChannel->pDevExt;
        PRINTK("dest_id %d: check_for_consumed_fifo_entries: free entries %d,  marker %llu, consumed bytes %llu\n",
            pChannel->dma_channel_no, tx->free_fifo_entries_normal,
            tx->fifo_markers[tx->fifo_marker_head], consumed_bytes);
    }
    if (tx->fifo_markers[tx->fifo_marker_head] > consumed_bytes)
        consumed_bytes = get_dest_consumed_bytes_precise(pChannel);

    if (tx->fifo_markers[tx->fifo_marker_head] <= consumed_bytes)
    {
        //tx->fifo_markers[ tx->fifo_marker_head ] = 0;
        tx->fifo_marker_head = (tx->fifo_marker_head + 1) % pChannel->reserved_fifo_entries_normal;
        tx->free_fifo_entries_normal++;
        return 1;
    }
    return 0;
}

static int
fifo_has_room(fb_channel_t * pChannel)
{
    tx_context_t *tx = &pChannel->tx;
    if (likely(tx->free_fifo_entries_normal > 0))
        return 1;

    return check_for_consumed_fifo_entries(pChannel);
}

static int
get_dest_capacity(fb_channel_t * pChannel)
{
    const device_context_t * pDevExt = pChannel->pDevExt;

    switch (pChannel->type)
    {
        case SC_DMA_CHANNEL_TYPE_TCP:           return pDevExt->toe_tx_win_size;
        case SC_DMA_CHANNEL_TYPE_USER_LOGIC:    return 16 * 1024; // 16Kb
        case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:  return 16 * 1024; // 16Kb
    }

    PRINTK_ERR("get_dest_capacity* unexpected channel type: %d\n", pChannel->type);

    return 0;
}

static uint64_t
dest_free_space_fast(fb_channel_t * pChannel)
{
    const uint64_t DestCapacity = get_dest_capacity(pChannel);
    uint64_t consumed_bytes = get_dest_consumed_bytes_fast(pChannel);
    uint64_t sent_bytes = pChannel->tx.sent_bytes;
    uint64_t free_space = DestCapacity - (sent_bytes - consumed_bytes);
    return free_space;
}
static uint64_t
dest_free_space_precise(fb_channel_t * pChannel)
{
    const uint64_t DestCapacity = get_dest_capacity(pChannel);
    uint64_t consumed_bytes = get_dest_consumed_bytes_precise(pChannel);
    uint64_t sent_bytes = pChannel->tx.sent_bytes;
    uint64_t free_space = DestCapacity - (sent_bytes - consumed_bytes);
    return free_space;
}

static int
dest_has_room(fb_channel_t * pChannel, int size)
{
    int size_round_up_32bytes = (size + 31) & ~((uint32_t)31);

    if (size_round_up_32bytes <= dest_free_space_fast(pChannel))
        return 1;

    if (size_round_up_32bytes <= dest_free_space_precise(pChannel))
        return 1;

    return 0;
}

void channel_flowctrl_reset(fb_channel_t * pChannel)
{
    device_context_t * pDevExt = pChannel->pDevExt;
    tx_context_t *tx = &pChannel->tx;

    // Some channels cannot do Tx
    if (!channel_can_do_tx(pChannel))
        return;

    //tx->sent_bytes = 0;
    tx->last_known_consumed_value = get_consumed_counter(pDevExt, pChannel->dma_channel_no);
    tx->sent_bytes = tx->last_known_consumed_value;
    //PRINTK("INITIAL TX CONSUMED : %llu\n", tx->sent_bytes);

    tx->free_fifo_entries_normal = pChannel->reserved_fifo_entries_normal;

    if (tx->fifo_markers)
    {
        kfree(tx->fifo_markers);
        tx->fifo_markers = NULL;
    }

    if (pChannel->reserved_fifo_entries_normal)
    {
        tx->fifo_markers = kmalloc(pChannel->reserved_fifo_entries_normal * sizeof(uint64_t), GFP_KERNEL);
        // FIXME missing error handling
        if (tx->fifo_markers)
        {
            memset(tx->fifo_markers, 0, pChannel->reserved_fifo_entries_normal * sizeof(uint64_t));
        }
    }

    tx->fifo_marker_tail = tx->fifo_marker_head = 0;
}

int  channel_has_room(fb_channel_t * pChannel, int size)
{
    WARN_ON(pChannel->owner_tx != OWNER_KERNEL);
    return fifo_has_room(pChannel) && dest_has_room(pChannel, size);
}

void channel_send_bucket(fb_channel_t * pChannel, int bucket_no, int size)
{
    tx_context_t *tx = &pChannel->tx;
    const device_context_t * pDevExt = pChannel->pDevExt;
    uint64_t  desc = make_packet_descriptor(pChannel, bucket_no, size);
    int round_up_32bytes = (size + 31) & ~((uint32_t)31);
    volatile uint64_t * pZeroedBytes = (volatile uint64_t *)(pChannel->bucket[bucket_no]->data + size);
    volatile uint64_t * pBucketStart = (volatile uint64_t *)(pChannel->bucket[bucket_no]->data);
    uint64_t bucketStartBytes;
    bool bucketStartBytesWereNotZero = false;

    // Write 8 bytes of DEADBEEFDEADBEEF right after payload so we detect Tx DMA nullified acknowledgement:
    *pZeroedBytes = 0xDEADBEEFDEADBEEFLLU;
    // Remember payload length of this bucket so we can later check
    // if the above bytes have been zeroed.
    pChannel->bucket_payload_length[bucket_no] = size;

    BUG_ON(pChannel->reserved_fifo_entries_normal == 0);

    //PRINTK("fifo_send_bucket: writing 0x%016llX to register %p (dest_id %d)\n", desc, reg, dest_id);
    tx->sent_bytes += (uint64_t)round_up_32bytes;
    assert(tx->free_fifo_entries_normal > 0);
    tx->free_fifo_entries_normal--;
    tx->fifo_markers[tx->fifo_marker_tail] = tx->sent_bytes;
    tx->fifo_marker_tail = (tx->fifo_marker_tail + 1) % pChannel->reserved_fifo_entries_normal;

    //PRINTK("SEND TX: %llu\n", pChannel->tx.sent_bytes);

    if (LOG(LOG_TX_ZERO_BYTES_ACK))
    {
        bucketStartBytes = *pBucketStart;
        bucketStartBytesWereNotZero = bucketStartBytes != 0;
    }

    send_descriptor(pDevExt, pChannel->dma_channel_no, desc);

    if (LOG(LOG_TX_ZERO_BYTES_ACK))
    {
        bucketStartBytes = *pBucketStart;
        if (bucketStartBytesWereNotZero && bucketStartBytes == 0)
        {
            static volatile uint64_t s_BucketStartBytesZeroedCount = 0;

            ++s_BucketStartBytesZeroedCount;
            PRINTK_ERR("Send check: pChannel %u bucket %d, beginning bytes zeroed: count %llu, bucket start *%p is 0x%llx\n",
                pChannel->dma_channel_no, bucket_no, s_BucketStartBytesZeroedCount, pBucketStart, bucketStartBytes);
        }
    }
}

