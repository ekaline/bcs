#ifndef CHANNEL_H
#define CHANNEL_H

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

#include "flowctrl.h"
#include "main.h"

int find_vacant_tcp_channel(const device_context_t * pDevExt);
int find_vacant_udp_channel(const device_context_t * pDevExt);
int init_channels(device_context_t * pDevExt, uint64_t prbSize);
void deinit_channels(device_context_t * pDevExt);

int channel_reallocate_buckets(fb_channel_t * channel, int min_size);
void channel_reset_buckets(fb_channel_t * channel);
bool is_bucket_free(fb_channel_t * pChannel, uint16_t bucket_index);
int  channel_get_free_bucket(fb_channel_t * channel, int size);
static inline int channel_tx_possible(fb_channel_t * channel, int size)
{
    return channel_has_room(channel, size);
}

int channel_allocate_dmabuf(fb_channel_t * channel, struct file *filep, sc_dmabuf_t *dmabuf);
int channel_free_dmabuf(fb_channel_t * channel, void *user_va);
int channel_free_all_dmabuf(fb_channel_t * channel);

int channel_release_tx_fifo_entries(fb_channel_t * channel);
int channel_reserve_tx_fifo_entries(fb_channel_t * channel, int normal, int priority);

int get_channel_connection_state(const device_context_t * pDevExt, fb_channel_t * pChannel, const char * context, bool lock);
void set_channel_connection_state_msg(const device_context_t * pDevExt, fb_channel_t * pChannel, int previousConnectionState, int newConnectionState, const char * context);
int set_channel_connection_state(const device_context_t * pDevExt, fb_channel_t * pChannel, int newConnectionState, const char * context, bool lock);

static inline int
channel_can_do_tx(fb_channel_t * channel)
{
    return (channel->type != SC_DMA_CHANNEL_TYPE_MONITOR) && (channel->type != SC_DMA_CHANNEL_TYPE_UDP_MULTICAST);
}

#endif /* CHANNEL_H */
