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
#include "flowctrl.h"
#include "fpga.h"
#include "main.h"
#include "util.h"

int find_vacant_tcp_channel(const device_context_t * pDevExt)
{
    int i = 0;
    for (i = FIRST_TCP_CHANNEL; i < FIRST_TCP_CHANNEL + max_etlm_channels; i++)
    {
        if (!pDevExt->channel[i]->occupied)
        {
            return i;
        }
    }
    PRINTK_ERR("could not find a free TCP channel\n");
    return -1;
}

int find_vacant_udp_channel(const device_context_t * pDevExt)
{
    int i = 0;
    for (i = FIRST_UDP_CHANNEL; i < FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS; i++)
    {
        if (!pDevExt->channel[i]->occupied)
        {
            return i;
        }
    }

    PRINTK_ERR("could not find a free UDP channel\n");
    return -1;
}

// On some systems allocating smaller bucket memory (f.ex. 64 buckets) causes slower DMA performance.
// This is seen in the ul_dma_loop application by running it on channels 0-4 on each channel separately (-c 1):
// time taskset -c 2 ./ul_dma_loop -c 1 -s 2 -b 32 -i 131072 > /dev/null
// 256 buckets which allocates 512KB of bucket memory for each channel seems to fix this problem.
// It seems that kernel memory allocated below 1MB in physical
// memory has lower DMA performance than memory that is above 1MB.
// The root cause of this phenomenon is unknown.
#define X_FACTOR    (256 / SC_NO_BUCKETS)

/* allocate MAX_BUCKETS buckets of PCI dmable memory */
static int channel_alloc_buckets(fb_channel_t * channel, int new_size)
{
    const device_context_t * pDevExt = channel->pDevExt;
    int      bucket_no;
    void    *va;    // Buckets kernel address space virtual base address available in the driver
    uint64_t ba;    // Buckets physical memory base address to use for DMA transfers

    if (new_size <= SC_BUCKET_SIZE - 8)
    {
        new_size += 8; // Reserve space for Tx DMA zero acknowledgement after payload
    }
    else
    {
        new_size = SC_BUCKET_SIZE;
    }

    // Allocate GFP_ATOMIC memory:
    va = pci_alloc_consistent(pDevExt->pdev, X_FACTOR * MAX_BUCKETS * new_size, &ba);
    if (va == NULL)
    {
        PRINTK_ERR("ERROR: Can't allocate buckets\n");
        return -ENOMEM;
    }
    if (LOG(LOG_PCIE_MEMORY_ALLOCATIONS))
    {
        uint32_t memorySize = X_FACTOR * MAX_BUCKETS * new_size;
        PRINTK("Allocated  %u (0x%x) bytes channel %d buckets PCIe memory at virtual address %p, physical address 0x%llx\n",
            memorySize, memorySize, channel->dma_channel_no, va, ba);
    }

    channel->bucket_size = new_size;
    for (bucket_no = 0; bucket_no < MAX_BUCKETS; bucket_no++)
    {
        channel->bucket[bucket_no] = va + bucket_no * channel->bucket_size;
        channel->dmaBucket[bucket_no] = ba + bucket_no* channel->bucket_size;

        //PRINTK_C(LOG(LOG_MMAP), "channel %d, bucket %d: channel->bucket[bucket_no]=%p, channel->dmaBucket[bucket_no]=%p\n",
        //       channel->dma_channel_no, bucket_no, (void *)(channel->bucket[bucket_no]), (void *)(channel->dmaBucket[bucket_no]));
    }
    return 0;
}

static void channel_dealloc_buckets(fb_channel_t * channel)
{
    const device_context_t * pDevExt = channel->pDevExt;
    if (LOG(LOG_PCIE_MEMORY_ALLOCATIONS))
    {
        uint32_t memorySize = X_FACTOR * MAX_BUCKETS * channel->bucket_size;
        PRINTK("Deallocate %u (0x%x) bytes channel %d buckets PCIe memory at virtual address %p\n",
            memorySize, memorySize, channel->dma_channel_no, channel->bucket[0]);
    }
    pci_free_consistent(pDevExt->pdev, X_FACTOR * MAX_BUCKETS * channel->bucket_size,
        channel->bucket[0], channel->dmaBucket[0]);

    channel->bucket_size = 0;
    memset(channel->bucket, 0, sizeof(channel->bucket));
    memset(channel->dmaBucket, 0, sizeof(channel->dmaBucket));
    memset(channel->bucket_payload_length, 0, sizeof(channel->bucket_payload_length));
}

/* Reallocate the buckets to a different size.
   Care must be taken to only reallocate buckeets that are not mmapped in userspace
 */
int channel_reallocate_buckets(fb_channel_t * channel, int new_size)
{
    if (channel->owner_tx != OWNER_KERNEL)
    {
        const device_context_t * pDevExt = channel->pDevExt;
        PRINTK_ERR("channel buckets are mapped into the user process and cannot just be reallocated\n");
        return -1;
    }

    channel_dealloc_buckets(channel);
    return channel_alloc_buckets(channel, new_size);
}

int init_channels(device_context_t * pDevExt, uint64_t prbSize)
{
    static const char *    channeltypes[] = { "TCP", "UL ", "OOB", "UDP", "MON", "N/A" };
    const char *    curname = 0;
    uint16_t        dma_channel_no;
    uint64_t        originalPrbSize = prbSize;

    for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS; dma_channel_no++)
    {
        fb_channel_t * pChannel;

        prbSize = originalPrbSize;

        pChannel = pDevExt->channel[dma_channel_no] = (fb_channel_t*)kmalloc(sizeof(fb_channel_t), GFP_KERNEL);
        if (pChannel == NULL)
        {
            PRINTK_ERR("ERROR: Can't allocate channel\n");
            return -ENOMEM;
        }
        PRINTK_C(LOG(LOG_MEMORY_ALLOCATIONS), "Allocated  %lu (0x%lx) bytes channel %d memory at %p\n",
            sizeof(fb_channel_t), sizeof(fb_channel_t), dma_channel_no, pChannel);

        memset(pChannel, 0, sizeof(fb_channel_t));
        pChannel->dma_channel_no = dma_channel_no;
        pChannel->pDevExt = pDevExt;
        pChannel->occupied = 0;
        pChannel->lastPacketPa = SC_INVALID_PLDMA_ADDRESS;
        init_completion(&pChannel->connectCompletion);
        //INIT_LIST_HEAD(&channel->dma_buffer_list);

        spin_lock_init(&pChannel->connectionStateSpinLock);

        channel_alloc_buckets(pChannel, SC_BUCKET_SIZE);

#ifndef DMA_CHANNEL_CANARY_UL8_OOB8_UNUSED8_MON8_UDP32_TCP64
    #error "init_channels: channel layout has changed, this code needs a review"
#endif
        // No need for a DMA_CHANNEL_CANARY check here
        if (dma_channel_no >= FIRST_TCP_CHANNEL && dma_channel_no < FIRST_TCP_CHANNEL + SC_MAX_TCP_CHANNELS)
        {
            curname = channeltypes[0];
            pChannel->type = SC_DMA_CHANNEL_TYPE_TCP;
        }
        else if (dma_channel_no >= FIRST_UL_CHANNEL && dma_channel_no < FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS)
        {
            curname = channeltypes[1];
            pChannel->type = SC_DMA_CHANNEL_TYPE_USER_LOGIC;
        }
        else if (dma_channel_no >= FIRST_OOB_CHANNEL && dma_channel_no < FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS)
        {
            curname = channeltypes[2];
            pChannel->type = SC_DMA_CHANNEL_TYPE_STANDARD_NIC;
        }
        else if (dma_channel_no >= FIRST_UDP_CHANNEL && dma_channel_no < FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS)
        {
            curname = channeltypes[3];
            pChannel->type = SC_DMA_CHANNEL_TYPE_UDP_MULTICAST;
        }
        else if (dma_channel_no >= FIRST_MONITOR_CHANNEL && dma_channel_no < FIRST_MONITOR_CHANNEL + SC_MAX_MONITOR_CHANNELS)
        {
            curname = channeltypes[4];
            pChannel->type = SC_DMA_CHANNEL_TYPE_MONITOR;
        }
        else
        {
            curname = channeltypes[5];
            pChannel->type = SC_DMA_CHANNEL_TYPE_NONE;
            // There are holes in the channel layout with unused channels so this case is acceptable
        }

        if (pChannel->type == SC_DMA_CHANNEL_TYPE_STANDARD_NIC)
        {
            pChannel->owner_rx = OWNER_KERNEL;
            pChannel->owner_tx = OWNER_KERNEL;
        }
        else
        {
            pChannel->owner_rx = OWNER_UNDECIDED;
            pChannel->owner_tx = OWNER_UNDECIDED;
        }

        if (pChannel->type == SC_DMA_CHANNEL_TYPE_STANDARD_NIC)
        {
            /* Only allocate 4 MB for NIC channels because we don't know how to combine more 4 MB blocks into one continuous virtual kernel memory */
            prbSize = RECV_DMA_SIZE;
        }
        if (pDevExt->pMMU != NULL && bufsize > 0)
        {
            pChannel->recv = mmu_allocate(pDevExt->pMMU, dma_channel_no, prbSize, &pChannel->recvDma, &pChannel->prbFpgaStartAddress);
        }
        else
        {
            prbSize = RECV_DMA_SIZE;
            pChannel->recv = pci_alloc_consistent(pDevExt->pdev, prbSize, &pChannel->recvDma);
            pChannel->prbFpgaStartAddress = pChannel->recvDma;
        }
        if (pChannel->recv == NULL)
        {
            PRINTK_ERR("ERROR: Can't allocate %llu byte receive buffer for DMA channel %d\n", prbSize, dma_channel_no);
            return -ENOMEM;
        }
        pChannel->prbSize = prbSize;
        PRINTK_C(LOG(LOG_PCIE_MEMORY_ALLOCATIONS), "Allocated  %llu (0x%llX, %llu MB) bytes channel Rx PCIe memory at va %p, pa 0x%llx, FPGA PRB start 0x%llX\n",
            prbSize, prbSize, prbSize /1024 / 1024, pChannel->recv, pChannel->recvDma, pChannel->prbFpgaStartAddress);

        PRINTK_C(LOGALL(LOG_INIT | LOG_DETAIL), "%s DMA channel: %d ring_buffer start addr: %llx dma_channel: %d\n",
            curname, dma_channel_no, pChannel->recvDma, fb_phys_channel(dma_channel_no));
    }
    return 0;
}

void deinit_channels(device_context_t * pDevExt)
{
    uint16_t dma_channel_no;

    for (dma_channel_no = 0; dma_channel_no < SC_MAX_CHANNELS; dma_channel_no++)
    {
        fb_channel_t * channel = pDevExt->channel[dma_channel_no];

        if (channel->tx.fifo_markers)
        {
            PRINTK_C(LOG(LOG_MEMORY_ALLOCATIONS), "Deallocate DMA channel %d FIFO markers: at %p\n", dma_channel_no, channel->tx.fifo_markers);

            kfree(channel->tx.fifo_markers);
            channel->tx.fifo_markers = NULL;
        }

        channel_dealloc_buckets(channel);

        if (channel->recv != NULL)
        {
            PRINTK_C(LOG(LOG_PCIE_MEMORY_ALLOCATIONS), "Deallocate %llu (0x%llx) bytes DMA channel %d Rx DMA PCIe memory at virtual address %p\n",
                channel->prbSize, channel->prbSize, dma_channel_no, channel->recv);

            if (pDevExt->pMMU != NULL && bufsize > 0)
            {
                mmu_free(pDevExt->pMMU, dma_channel_no, channel->prbSize, channel->recv, channel->recvDma);
            }
            else
            {
                pci_free_consistent(pDevExt->pdev, channel->prbSize, channel->recv, channel->recvDma);
            }
            channel->prbSize = 0;
            channel->recv = NULL;
            channel->recvDma = 0;
        }

        PRINTK_C(LOG(LOG_MEMORY_ALLOCATIONS), "Deallocate DMA channel %d memory: at %p\n", dma_channel_no, channel);
        kfree(channel);
        pDevExt->channel[dma_channel_no] = NULL;
    }
}

void channel_reset_buckets(fb_channel_t * channel)
{
    // NB: zeroing based on bucket_size, not on sizeof(channel->bucket[0])!
    memset(channel->bucket[0], 0, MAX_BUCKETS* channel->bucket_size);
    memset(channel->bucket_payload_length, 0, sizeof(channel->bucket_payload_length));

    channel->unused_buckets = MAX_BUCKETS;
    channel->next_bucket = 0;

    channel_flowctrl_reset(channel);
}

/**
 *  @brief  Check if a bucket is free for reuse.
 *
 *  Check if Tx DMA has zeroed 8 bytes right after the payload
 *  where 8 bytes of DEADBEEFDEADBEEF were filled when the bucket was sent.
 *  8 zero bytes after payload signal that Tx DMA has processed
 *  the bucket payload and the bucket can be reused.
 *
 *  Further flow control and allocation of resources should not
 *  be allowed to proceed before this function reports the bucket
 *  as freed.
 *
 *  @param  pChannel        Pointer to channel data structure.
 *  @param  bucket_index    Index of bucket to check.
 *
 *  @return             True if the bucket can be reused, false otherwise.
 */
bool is_bucket_free(fb_channel_t * pChannel, uint16_t bucket_index)
{
    volatile uint64_t * pZeroedBytes;
    uint64_t zeroedBytes;

    volatile uint16_t * pBucketPayloadLength = (volatile uint16_t *)&pChannel->bucket_payload_length[bucket_index];
    uint16_t payloadLength = *pBucketPayloadLength;
    if (payloadLength == 0)
    {
        return true; // This bucket is free
    }

    // Check for Tx DMA zeroed 8 bytes after payload:
    pZeroedBytes = (volatile uint64_t *)(&pChannel->bucket[bucket_index]->data[payloadLength]);
    zeroedBytes = *pZeroedBytes;
    if (zeroedBytes == 0)
    {
        // 8 bytes after payload zeroed, this bucket is free
        *pBucketPayloadLength = 0; // Mark it free
        return true;
    }
    // If not zero then it should be DEADBEEFDEADBEEF
    if (zeroedBytes != 0xDEADBEEFDEADBEEFLLU)
    {
        if (LOGALL(LOG_TX | LOG_FLOW_CONTROL | LOG_DETAIL))
        {
            static volatile int count = 0;
            int incrementedCount = __sync_fetch_and_add(&count, 1);
            if (incrementedCount < 10)
            {
                const device_context_t * pDevExt = pChannel->pDevExt;
                PRINTK_ERR("%d: channel %d bucket %u: prev = 0x%16llx zeroedBytes = 0x%16llx next = 0x%16llx\n",
                    incrementedCount, pChannel->dma_channel_no, bucket_index, *(pZeroedBytes - 1), zeroedBytes, *(pZeroedBytes + 1));
            }
        }
    }
    //assert(zeroedBytes == 0xDEADBEEFDEADBEEFLLU);

    return false;
}

int channel_get_free_bucket(fb_channel_t * pChannel, int size)
{
    while (true)
    {
        static int errorMessageCount = 0;
        uint64_t start = jiffies;
        uint64_t round = 0;

        while (!is_bucket_free(pChannel, pChannel->next_bucket))
        {
            // Not free, advance to next bucket which is the  most probable
            // candidate for having been zeroed by Tx DMA.
            pChannel->next_bucket++;
            pChannel->next_bucket %= SC_NO_BUCKETS;

            if (++round % SC_NO_BUCKETS == 0)
            {
                // Went a whole round around all buckets and no free buckets found
                uint64_t now = jiffies;
                if (now - start > msecs_to_jiffies(1000))
                {
                    if (errorMessageCount++ < 10)
                    {
                        const device_context_t * pDevExt = pChannel->pDevExt;
                        PRINTK_ERR("DMA channel %u no free buckets were found within 1 second. Is Tx DMA working?\n",
                            pChannel->dma_channel_no);
                    }
                    return -1;
                }

                // Retry one more round through bucket buffers
                round = 0;
            }
        }

        {
            // This bucket is free for (re)use
            int bucketIndex = pChannel->next_bucket;

            // Check channel flow control
            start = jiffies;
            while (!channel_has_room(pChannel, size))
            {
                uint64_t now = jiffies;
                if (now - start > msecs_to_jiffies(1000))
                {
                    if (errorMessageCount++ < 10)
                    {
                        const device_context_t * pDevExt = pChannel->pDevExt;
                        PRINTK_ERR("DMA channel %u had no room for Tx for 1 second. Is Tx DMA working?\n",
                            pChannel->dma_channel_no);
                    }
                    return -1;
                }
            }

            //pChannel->unused_buckets--; // FIXME not used, but make sure that more buckets than fifo entries are allocated
            // Next time around here start searching from the next bucket
            pChannel->next_bucket++;
            pChannel->next_bucket %= SC_NO_BUCKETS;

            errorMessageCount = 0;

            return bucketIndex;
        }
    }
}

#if 0   // These are currently not used for anything
int channel_allocate_dmabuf(fb_channel_t * channel, struct file *filep, sc_dmabuf_t * dmabuf)
{
    device_context_t * pDevExt = channel->pDevExt;
    drv_dmabuf_t * drv_dma;

    drv_dma = (drv_dmabuf_t *)kzalloc(sizeof(drv_dmabuf_t), GFP_KERNEL);
    if (!drv_dma)
    {
        return -ENOMEM;
    }

    drv_dma->sz = dmabuf->sz;
    drv_dma->kern_va = pci_alloc_consistent(pDevExt->pdev, drv_dma->sz, &drv_dma->dma);
    if (!drv_dma->kern_va)
    {
        kfree(drv_dma);
        return -ENOMEM;
    }

    memset(drv_dma->kern_va, 0, drv_dma->sz);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
    down_write(&current->mm->mmap_sem);
    drv_dma->user_va = (void*)do_mmap(filep, 0, drv_dma->sz,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_LOCKED,
        drv_dma->dma);
    up_write(&current->mm->mmap_sem);
#else
    drv_dma->user_va = (void*)vm_mmap(filep, 0, drv_dma->sz,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_LOCKED,
        drv_dma->dma);
#endif

    if (!drv_dma->user_va)
    {
        PRINTK_ERR("Cannot map tx_area memory to user space\n");
        pci_free_consistent(pDevExt->pdev, drv_dma->sz, drv_dma->kern_va, drv_dma->dma);
        kfree(drv_dma);
        return -EFAULT;
    }

    list_add_tail(&drv_dma->dma_buffer_entry, &channel->dma_buffer_list);

    dmabuf->dma = drv_dma->dma;
    dmabuf->data = drv_dma->user_va;
    PRINTK("allocated dmabuf sz %u user_va=%p pa=0x%llx"
        drv_dma->sz, drv_dma->user_va, drv_dma->dma);

    return 0;
}


static int free_dmabuf(fb_channel_t * channel, drv_dmabuf_t * drv_dma)
{
    device_context_t * pDevExt = channel->pDevExt;

    PRINTK("freeing dmabuf sz %u user_va=%p pa=0x%llx"
        drv_dma->sz, drv_dma->user_va, drv_dma->dma);

    if (current && current->mm)
    {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
        down_write(&current->mm->mmap_sem);
        do_munmap(current->mm, (unsigned long)drv_dma->user_va, drv_dma->sz);
        up_write(&current->mm->mmap_sem);
#else
        vm_munmap((unsigned long)drv_dma->user_va, drv_dma->sz);
#endif
    }

    pci_free_consistent(pDevExt->pdev, drv_dma->sz, drv_dma->kern_va, drv_dma->dma);

    PRINTK("deallocated dmabuf user_va=%p pa=0x%llx"
        drv_dma->user_va, drv_dma->dma);
    return 0;
}

int channel_free_dmabuf(fb_channel_t * channel, void* user_va)
{
    drv_dmabuf_t *txa, *q;

    list_for_each_entry_safe(txa, q, &channel->dma_buffer_list, dma_buffer_entry)
    {
        if (txa->user_va == user_va)
        {
            free_dmabuf(channel, txa);
            list_del(&txa->dma_buffer_entry);
            kfree(txa);
            return 0;
        }
    }

    device_context_t * pDevExt = channel->pDevExt;
    PRINTK_ERR("channel_free_dmabuf dmabuf not found in channel list! (chan %d, user_va %p)\n",
        channel->dma_channel_no, user_va);
    WARN_ON(" channel_free_dmabuf dmabuf not found in channel list!");
    return -1;
}

int channel_free_all_dmabuf(fb_channel_t * channel)
{
    drv_dmabuf_t *txa, *q;

    list_for_each_entry_safe(txa, q, &channel->dma_buffer_list, dma_buffer_entry)
    {
        free_dmabuf(channel, txa);
        list_del(&txa->dma_buffer_entry);
        kfree(txa);
    }

    return 0;
}
#endif

int channel_reserve_tx_fifo_entries(fb_channel_t * channel, int normal, int priority)
{
    device_context_t * pDevExt = channel->pDevExt;

    WARN_ON(!mutex_is_locked(&pDevExt->channelListLock));

    // Validate the request
    if (normal > pDevExt->free_fifo_entries_normal
        || channel->reserved_fifo_entries_normal + normal < 0)
    {
        PRINTK_ERR("not enough normal fifo entries available (requested %d, avail %d)\n",
            normal, pDevExt->free_fifo_entries_normal);
        return -1;
    }

    if (priority > pDevExt->free_fifo_entries_priority
        || channel->reserved_fifo_entries_priority + priority < 0)
    {
        PRINTK_ERR("not enough priority fifo entries available (requested %d, avail %d)\n",
            priority, pDevExt->free_fifo_entries_priority);
        return -1;
    }

    channel->reserved_fifo_entries_normal += normal;
    channel->reserved_fifo_entries_priority += priority;

    channel->tx.free_fifo_entries_normal += normal;

    pDevExt->free_fifo_entries_normal -= normal;
    pDevExt->free_fifo_entries_priority -= priority;

    channel_flowctrl_reset(channel);

    PRINTK_C(LOG(LOG_CHANNEL | LOG_TX), "channel %d  new reserved normal: %u, priority: %u\n",
        channel->dma_channel_no, channel->reserved_fifo_entries_normal,
        channel->reserved_fifo_entries_priority);

    return 0;
}

int channel_release_tx_fifo_entries(fb_channel_t * channel)
{
    // Simply take advantage of abity to send negative values to the allocate function
    return channel_reserve_tx_fifo_entries(channel, -channel->reserved_fifo_entries_normal, -channel->reserved_fifo_entries_priority);
}

const char * ConnState(int state);

int get_channel_connection_state(const device_context_t * pDevExt, fb_channel_t * pChannel, const char * context, bool lock)
{
    int currentConnectionState;

    if (lock)
    {
        spin_lock(&pChannel->connectionStateSpinLock);
    }
    currentConnectionState = pChannel->connectionState;
    if (lock)
    {
        spin_unlock(&pChannel->connectionStateSpinLock);
    }

    if (context != NULL && LOG(LOG_ETLM_TCP_CONNECTION))
    {
        PRINTK("get_channel_connection_state from %s: channel %d as %s\n", context,
            pChannel->dma_channel_no, ConnState(currentConnectionState));
    }

    return currentConnectionState;
}

void set_channel_connection_state_msg(const device_context_t * pDevExt, fb_channel_t * pChannel, int previousConnectionState, int newConnectionState, const char * context)
{
    if (context != NULL && LOG(LOG_ETLM_TCP_CONNECTION))
    {
        PRINTK("set_channel_connection_state from %s: channel %d from %s to %s\n", context,
            pChannel->dma_channel_no, ConnState(previousConnectionState), ConnState(newConnectionState));
    }
}

int set_channel_connection_state(const device_context_t * pDevExt, fb_channel_t * pChannel, int newConnectionState, const char * context, bool lock)
{
    int previousConnectionState;

    if (lock)
    {
        spin_lock(&pChannel->connectionStateSpinLock);
    }
    if (pChannel->connectionState != newConnectionState)
    {
        previousConnectionState = pChannel->previousConnectionState = pChannel->connectionState;
        pChannel->connectionState = newConnectionState;
    }
    else
    {
        previousConnectionState = pChannel->previousConnectionState;
    }
    if (lock)
    {
        spin_unlock(&pChannel->connectionStateSpinLock);
    }

    if (context != NULL)
    {
        set_channel_connection_state_msg(pDevExt, pChannel, previousConnectionState, newConnectionState, context);
    }

    return previousConnectionState;
}
