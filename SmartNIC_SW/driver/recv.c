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
#include <linux/completion.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "main.h"
#include "fpga.h"
#include "recv.h"

int
channel_get_filling_percent(fb_channel_t * pChannel)
{
    int filling_pct = 0;
    uint64_t last_packet_pa = pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)];

    if (last_packet_pa)
    {
        uint64_t offset_tail = pChannel->pCurrentPacket != NULL ? (uint64_t)pChannel->pCurrentPacket - (uint64_t)pChannel->recv : 0;
        uint64_t offset_head = last_packet_pa - pChannel->recvDma;
        uint64_t bytes;

        if (offset_tail <= offset_head)
            bytes = offset_head - offset_tail;
        else
            bytes = offset_head + (RECV_DMA_SIZE - offset_tail);

        filling_pct = (bytes * 100) / RECV_DMA_SIZE;
    }

    return filling_pct;
}

static const sc_packet *
FbGetRawNextPacket(const fb_channel_t * pChannel, const sc_packet * pCurrentPacket)
{
    const device_context_t * pDevExt = pChannel->pDevExt;

    if (unlikely(!pCurrentPacket || (pCurrentPacket->status & SC_PKT_STATUS_END_OF_BUFFER)))
    {
        PRINTK_C(LOGALL(LOG_RX | LOG_DETAIL), "%s: pCurrentPacket = 0x%llX, cur->status & SC_PKT_STATUS_END_OF_BUFFER) = %u\n", __func__, (uint64_t)pCurrentPacket, pCurrentPacket->status & SC_PKT_STATUS_END_OF_BUFFER);
        return (sc_packet*)(pChannel->recv);
    }
    else
    {
        const sc_packet * pNextPossiblePacket;

        if (unlikely(pCurrentPacket->len == 0))
        {
            static volatile int dumpCounts[SC_MAX_CHANNELS] = { 0 };

            int dumpCount = dumpCounts[pChannel->dma_channel_no];
            if (dumpCount <= 10)
            {
                const device_context_t * pDevExt = pChannel->pDevExt;
                uint64_t pldma_last = pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)];
                PRINTK_ERR("unexpected len 0 in packet on chan %i pChannel->lastPacketPa %p pldma_last %p ", pChannel->dma_channel_no, (void*)pChannel->lastPacketPa, (void*)pldma_last);
                PRINTK_ERR("           cur %p pChannel->recv %p ", pCurrentPacket, pChannel->recv);
                if (dumpCount == 0)
                {
                    dump_stack();
                }
                ++dumpCounts[pChannel->dma_channel_no];
            }
            return NULL;
        }

        // Returns the next 64 bytes-aligned ptr address. Could be replaced by:
        pNextPossiblePacket =(sc_packet*)((void*)pCurrentPacket + ((sizeof(sc_packet) - 2 + pCurrentPacket->len + 63U) & ~0x3f));

#if 0
        if (LOG(LOG_MMU))
        {
            static volatile const sc_packet * pPreviousPossiblePacket = NULL;
            if (pNextPossiblePacket != pPreviousPossiblePacket)
            {
                PRINTK_C(LOG(LOG_MMU), "%s: DMA channel %u next possible packet %p\n", __func__, pChannel->dma_channel_no, (void*)pNextPossiblePacket);
                pPreviousPossiblePacket = pNextPossiblePacket;
            }
        }
#endif

        return pNextPossiblePacket;
    }
}

/* returns NULL if no new packet has arrived after 'cur' on the channel */
const sc_packet * FbGetNextPacket(fb_channel_t * pChannel, const sc_packet* pCurrentPacket)
{
    const sc_packet * nextPacket;
    const device_context_t * pDevExt = pChannel->pDevExt;

    // Our cached lastPacketPa is not set - then update it
    if (unlikely(pChannel->lastPacketPa == SC_INVALID_PLDMA_ADDRESS)) // Only true for the first packet
    {
        pChannel->lastPacketPa = pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)]; // Initialize the cache pldma.
        //PRINTK_C(LOG(LOG_MMU | LOG_RX | LOG_DEBUG | LOG_EXTERNAL), "%s#%d: lastPacketPa = 0x%llX\n", __func__, __LINE__, pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)]);
        if (unlikely(pChannel->lastPacketPa == SC_INVALID_PLDMA_ADDRESS))
        {
            return NULL; // No packet yet.
        }
    }

    if (unlikely(pCurrentPacket == NULL)) // If lastPacketPa != SC_INVALID_PLDMA_ADDRESS and pCurrentPacket == NULL then the first packet is placed in the start of the buffer.
    {
        PRINTK_C(LOG(LOG_MMU | LOG_RX | LOG_DEBUG | LOG_EXTERNAL), "%s#%d: lastPacketPa = 0x%llX\n", __func__, __LINE__, pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)]);
        nextPacket = (sc_packet *)(pChannel->recv);
        return nextPacket;
    }

    // From here the variable nextPacket is expected location of next packet and should be check
    // for it is not above the current cache position the next pointer
    //PRINTK_C(LOG(LOG_MMU | LOG_RX | LOG_DEBUG | LOG_EXTERNAL), "%s#%d: lastPacketPa = 0x%llX\n", __func__, __LINE__, pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)]);
    nextPacket = FbGetRawNextPacket(pChannel, pCurrentPacket);

    if ((uint64_t)pCurrentPacket - (uint64_t)(pChannel->recv) + (uint64_t)pChannel->prbFpgaStartAddress == pChannel->lastPacketPa)
    {
        pChannel->lastPacketPa = pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)]; // Initialize the cache pldma.
        if ((uint64_t)pCurrentPacket - (uint64_t)(pChannel->recv) + (uint64_t)pChannel->prbFpgaStartAddress == pChannel->lastPacketPa) // recheck
        {
            return NULL;
        }
        //PRINTK_C(LOG(LOG_MMU | LOG_RX | LOG_DEBUG | LOG_EXTERNAL), "%s#%d: lastPacketPa = 0x%llX next packet %p\n", __func__, __LINE__, pChannel->pDevExt->plDmaVa->lastPacket[fb_phys_channel(pChannel->dma_channel_no)], (void *)nextPacket);
    }

    return nextPacket;
}

/**
 * Utility function to copy data from return packet to buffer
 *
 * This function will copy data and update buffer pointer, buffer size, packet length, and data offset
 * so everything is ready for a subsequent new copy.
 *
 * @param returnPacket Return packet to copy data from. Len field is decreased by # of bytes copied.
 * @param dst Pointer to destination buffer. The pointer is moved by # of bytes copied.
 * @param dstBufLen Size of destination buffer. Decreased by # of bytes copied.
 * @param dataOffset Offset into current return packet. Updated after copy.
 * @return # of bytes copied.
 */
static unsigned int copy_data(const fb_channel_t * pChannel, const sc_packet *returnPacket, uint16_t *length, uint8_t **dst, unsigned int *dstBufLen, unsigned int *dataOffset)
{
    const uint16_t len = min((unsigned int)*length, *dstBufLen);
    if (!len)
    {
        return 0;
    }
    if (pChannel->type == SC_DMA_CHANNEL_TYPE_USER_LOGIC)
    {
        if (copy_to_user(*dst, &(sc_get_ulogic_payload(returnPacket)[*dataOffset]), len))
        {
            const device_context_t * pDevExt = pChannel->pDevExt;
            PRINTK_ERR("copy_to_user failed\n");
            return 0;
        }
    }
    else
    {
        if (copy_to_user(*dst, &(sc_get_tcp_payload(returnPacket)[*dataOffset]), len))
        {
            const device_context_t * pDevExt = pChannel->pDevExt;
            PRINTK_ERR("copy_to_user failed\n");
            return 0;
        }
    }
    *dst += len;
    *dstBufLen -= len;
    *length -= len;
    if (*length)
    {
        *dataOffset += len;
    }
    else
    {
        *dataOffset = 0;
    }
    return len;
}

/* Returns 1 if there is some data to read on the channel, 0 otherwise */
int channel_rx_data_available(fb_channel_t * pChannel)
{
    /* If userspace is reading the ring buffer, our pChannel->cur is likely invalid */
    WARN_ON(pChannel->owner_rx != OWNER_KERNEL);

    if (pChannel->pCurrentPacket != NULL && pChannel->curLength > 0)
        return 1;  // At min pChannel->curLength bytes available

    if (FbGetNextPacket(pChannel, pChannel->pCurrentPacket))
        return 1;

    return 0;
}

/* Read up to 'bufLen' bytes from the channel into buffer 'buf' */
int channel_read(fb_channel_t * pChannel, uint8_t *buf, unsigned int bufLen)
{
    const device_context_t * pDevExt = pChannel->pDevExt;
    unsigned int bytesCopied = 0;

    const sc_packet * pCurrentPacket = pChannel->pCurrentPacket;
    if (pCurrentPacket == NULL)
    {
        pChannel->pCurrentPacket = pCurrentPacket = FbGetNextPacket(pChannel, pCurrentPacket);
        if (pCurrentPacket == NULL)
        {
            return 0;
        }
        if (pChannel->type == SC_DMA_CHANNEL_TYPE_USER_LOGIC)
        {
            pChannel->curLength = sc_get_ulogic_payload_length(pCurrentPacket);
        }
        else
        {
            pChannel->curLength = sc_get_tcp_payload_length(pCurrentPacket);
        }
    }

    while (pCurrentPacket != NULL)
    {
        if (buf != NULL && bufLen > 0 && pChannel->curLength > 0)
        {
            bytesCopied += copy_data(pChannel, pCurrentPacket, &pChannel->curLength, &buf, &bufLen, &pChannel->dataOffset);
        }

        if (buf == NULL && bufLen == 0 && pChannel->curLength > 0) // TODO: what's this about???
        {
            return pChannel->curLength;
        }

        if (pChannel->curLength > 0)
        {
            // Data left in packet. Not enough room in buffer for more data. Exit here
            return bytesCopied;
        }

        pCurrentPacket = FbGetNextPacket(pChannel, pChannel->pCurrentPacket);
        if (pCurrentPacket != NULL)
        {
            set_rx_dma_read_mark(pDevExt, pChannel->dma_channel_no, (uint8_t*)pChannel->pCurrentPacket);
            pChannel->pCurrentPacket = pCurrentPacket;
            if (pChannel->type == SC_DMA_CHANNEL_TYPE_USER_LOGIC)
            {
                pChannel->curLength = sc_get_ulogic_payload_length(pCurrentPacket);
            }
            else
            {
                pChannel->curLength = sc_get_tcp_payload_length(pCurrentPacket);
            }
        }
    }

    return bytesCopied;
}
