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
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/delay.h>

#include "main.h"
#include "fpga.h"
#include "net.h"
#include "connect.h"
#include "channel.h"

const char * ConnState(fbConnectionState state)
{
    switch (state)
    {
        case FB_CON_CLOSED:     return "FB_CON_CLOSED";
        case FB_CON_CONNECTING: return "FB_CON_CONNECTING";
        case FB_CON_CONNECTED:  return "FB_CON_CONNECTED";
        case FB_CON_CLOSING:    return "FB_CON_CLOSING";
        case FB_CON_LISTENING:  return "FB_CON_LISTENING";
        default:                return "*** UNKNOWN ***";
    }
}

int fbDisconnect(fb_channel_t * channel)
{
    device_context_t * pDevExt = channel->pDevExt;
    unsigned tcp_conn = channel->dma_channel_no - FIRST_TCP_CHANNEL;

    int channelConnectionState = get_channel_connection_state(pDevExt, channel, nameof(fbDisconnect)"#1", true);
    if (channelConnectionState != FB_CON_CLOSED)
    {
        bool gracefullyDisconnected = true;

        // TODO FIXME: etlm_conn_close should not wait up 2 seconds for closed state. This should be handled in the FB_CON_CLOSING state.
        // TODO FIXME: that's why we cannot take the connectionLock above.

        // TODO FIXME: should we empty the Rx ring buffer here? TCP/IP state diagram implies that all window data
        // has to be read before state leaves ETLM_TCP_STATE_CLOSED_DATA_PENDING. Rx DMA should do this
        // automatically as long as there is free place in the packet ring buffer but what if the PRB is full?
        if (etlm_conn_close(pDevExt, tcp_conn) != 0)
        {
            etlm_abort_conn(pDevExt, tcp_conn);

            // TODO FIXME: should we do something else here if ETLM state is ETLM_TCP_STATE_FIN_WAIT2
            // TODO FIXME: i.e. for example we have an ill-behaving counterpart that refuses to close?

            set_channel_connection_state(pDevExt, channel, FB_CON_CLOSED, nameof(fbDisconnect)"#2", true);
            complete(&channel->connectCompletion);
        }
        else
        {
            // Set driver state to closing. Final internal FB_CON_CLOSED
            // state is achieved in timer callback fbProcessTCPChannel
            // which will detect when ETLM has reached ETLM_TCP_STATE_CLOSED state.
            set_channel_connection_state(pDevExt, channel, FB_CON_CLOSING, nameof(fbDisconnect)"#3", true);
        }

        if (gracefullyDisconnected)
        {
            PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION), "fbDisconnect on channel %u gracefully disconnected\n", tcp_conn);
        }
        else
        {
            PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION), "graceful ETLM close failed for TCP channel %u, channel was aborted!\n", tcp_conn);
        }
    }
    else
    {
        PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION), "ETLM channel %u already closed!\n", tcp_conn);
    }

    return 0;
}

static void Connect(fb_channel_t * channel, bool log)
{
    device_context_t * pDevExt = channel->pDevExt;
    sc_connection_info_t * conn = &channel->connectionInfo;
    unsigned tcp_conn = channel->dma_channel_no - FIRST_TCP_CHANNEL;

    memset(channel->recv, 0, RECV_DMA_SIZE);    // Clear receive PRB
    start_dma(pDevExt, channel->dma_channel_no, channel->prbSize);    // Start Rx DMA

    if (etlm_tcp_connect(pDevExt, tcp_conn, conn->local_port, conn->remote_port, conn->remote_ip, conn->remote_mac_address, conn->local_ip_address, conn->vlan_tag))
    {
        PRINTK_C(unlikely(log || LOG(LOG_ERROR | LOG_DEBUG)), "fbConnect: error connecting with tcp conn %u."
            " Aborting.\n", tcp_conn);

        set_channel_connection_state(pDevExt, channel, FB_CON_CLOSED, nameof(Connect)"#1", true);
        return;
    }

    set_channel_connection_state(pDevExt, channel, FB_CON_CONNECTING, nameof(Connect)"#2", true);

    PRINTK_C(unlikely(log || LOG(LOG_DEBUG)), ".. end Connect() with connection state=%s\n", ConnState(FB_CON_CONNECTING));
}

int fbConnect(fb_channel_t * channel, const sc_connection_info_t *conn)
{
    device_context_t * pDevExt = channel->pDevExt;
    unsigned dma_channel_no = channel->dma_channel_no;
    unsigned tcp_conn = dma_channel_no - FIRST_TCP_CHANNEL;
    unsigned lane = pDevExt->tcp_lane;
    unsigned int tcp_state;
    bool log = LOG(LOG_ETLM_TCP_CONNECTION);

    PRINTK_C(unlikely(log), "fbConnect: dma_channel_no=%u %u\n", dma_channel_no, lane);

    if ((tcp_state = etlm_getTcpState(pDevExt, tcp_conn)) != ETLM_TCP_STATE_CLOSED)
    {
        PRINTK_ERR("fbConnect: conn %u not in closed state "
            "- the state is: 0x%x\n", tcp_conn, tcp_state);
        return -1;
    }
    PRINTK_C(unlikely(log), "fbConnect: ETLM state is %s\n", EtlmState(tcp_state));

    memcpy(&channel->connectionInfo, conn, sizeof(channel->connectionInfo));

    {
        uint32_t rx_win_start = tcp_conn * pDevExt->toe_rx_win_size;
        uint32_t tx_win_start = tcp_conn * pDevExt->toe_tx_win_size;

        PRINTK_C(unlikely(log), "fbConnect: tcp_conn=%d, rx_win_start=%u, pDev->toe_rx_win_size=%u, tx_win_start=%u, pDev->toe_tx_win_size=%u\n",
            tcp_conn, rx_win_start, pDevExt->toe_rx_win_size, tx_win_start, pDevExt->toe_tx_win_size);

        if (etlm_setup_rx_window(pDevExt, tcp_conn, rx_win_start, pDevExt->toe_rx_win_size))
        {
            PRINTK_ERR("fbConnect: etlm_setup_rx_window FAILED\n");
            return -2;
        }
        if (etlm_setup_tx_window(pDevExt, tcp_conn, tx_win_start, pDevExt->toe_tx_win_size))
        {
            PRINTK_ERR("fbConnect: etlm_setup_tx_window FAILED\n");
            return -3;
        }
    }

    /*
    // FIXME: this is how the local port used to be chosen, lets keep the same logic here
    if (!conn->local_port)
      setSrcPort(pDevExt, dma_channel_no, 0x3000+(dma_channel_no+SC_MAX_TCP_CHANNELS*dev->nif[phy_interface_no].local_port++));
    else
      setSrcPort(pDevExt, dma_channel_no, conn->local_port);
    */
    // FIXME verify which ports nums are allocated with 128 tcp conns
    // FIXME This formula can overflow a uint16_t and start using reserved port numbers (< 1024) is that a pb?
    // This formula allocates unique port numbers to each channel no matter what
    if (conn->local_port == 0)
    {
        // nif0: ports 0x3000, 0x3040, 0x3080, ... channel 0 connect 1
        // nif0: ports 0x3040, 0x3080, 0x30C0, ... channel 0 connect 2
        channel->connectionInfo.local_port = 0x3000 + (dma_channel_no + SC_MAX_TCP_CHANNELS * pDevExt->nif[lane].local_port++);
    }
    if (unlikely(log))
    {
        IPBUF(1);
        PRINTK("fbConnect: channel->connectionInfo.local_ip_address = %s and .local_port = %u\n",
            LogIP(1, channel->connectionInfo.local_ip_address), channel->connectionInfo.local_port);
    }

    stop_dma(pDevExt, dma_channel_no);

    if (MacAddressIsNotZero(conn->remote_mac_address))
    {
        const sc_net_interface_t * nif = &pDevExt->nif[lane];
        etlm_setSrcMac(pDevExt, nif->mac_addr);
        etlm_setSrcIp(pDevExt, get_local_ip(nif));
        Connect(channel, log);
    }
    else
    {
        PRINTK_ERR("fbConnect: MAC address is zero; a valid MAC address is required!\n");
        return -EINVAL;
    }

    return 0;
}

int fbListen(fb_channel_t * channel, sc_listen_info_t *listen_info)
{
    device_context_t * pDevExt = channel->pDevExt;
    unsigned dma_channel_no = channel->dma_channel_no;
    unsigned tcp_conn = dma_channel_no - FIRST_TCP_CHANNEL;
    unsigned lane = pDevExt->tcp_lane;
    unsigned int tcp_state;

    PRINTK_C(LOG(LOG_LISTEN), "fbListen: dma_channel_no=%u\n", dma_channel_no);

    if ((tcp_state = etlm_getTcpState(pDevExt, tcp_conn)) != ETLM_TCP_STATE_CLOSED)
    {
        PRINTK_ERR("fbListen: tcp conn %u not in closed state "
            "- the state is: 0x%x\n", tcp_conn, tcp_state);
        return -1;
    }

    {
        uint32_t rx_win_start = tcp_conn * pDevExt->toe_rx_win_size;
        uint32_t tx_win_start = tcp_conn * pDevExt->toe_tx_win_size;
        if (etlm_setup_rx_window(pDevExt, tcp_conn, rx_win_start, pDevExt->toe_rx_win_size)
            || etlm_setup_tx_window(pDevExt, tcp_conn, tx_win_start, pDevExt->toe_tx_win_size))
            return -1;
    }

    PRINTK_C(LOG(LOG_LISTEN), "nif local IP = %08X\n", get_local_ip(&pDevExt->nif[lane]));

    stop_dma(pDevExt, dma_channel_no);
    memset(channel->recv, 0, RECV_DMA_SIZE);
    start_dma(pDevExt, dma_channel_no, channel->prbSize);

    if (etlm_tcp_listen(pDevExt, tcp_conn, listen_info->local_port, listen_info->local_ip_addr, listen_info->vlan_tag))
    {
        IPBUF(1);

        PRINTK_ERR("fbListen: error listening on tcp conn %u. Listen local IP %s port %u VLAN %d. Aborting.\n",
            tcp_conn, LogIP(1, listen_info->local_ip_addr), listen_info->local_port, listen_info->vlan_tag);
        return -1;
    }

    set_channel_connection_state(pDevExt, channel, FB_CON_LISTENING, nameof(fbListen), true);

    PRINTK_C(LOG(LOG_LISTEN), "initiate server listen mode\n");

    return 0;
}

/**
 *  This function is called from the main timer interrupt "sc_timer_callback"
 *  to poll any changes in the following areas:
 *
 *  - Poll for changes in the TOE state
 */
int fbProcessTCPChannel(device_context_t * pDevExt, unsigned int dma_channel_no)
{
    fb_channel_t * channel = pDevExt->channel[dma_channel_no];
    unsigned tcp_conn = dma_channel_no - FIRST_TCP_CHANNEL;
    unsigned tcpState;
    int previousConnectionState, channelConnectionState;
    bool log = LOG(LOG_ETLM_TCP_CONNECTION);

    if (pDevExt->tcp_lane_mask == 0)
    {
        return -1;
    }

    tcpState = etlm_getTcpState(pDevExt, tcp_conn);

    spin_lock(&channel->connectionStateSpinLock);
    channelConnectionState = get_channel_connection_state(pDevExt, channel, NULL, false);
    if (channelConnectionState != FB_CON_CLOSED)
    {
        channelConnectionState = get_channel_connection_state(pDevExt, channel, nameof(fbProcessTCPChannel)"#1", false);
    }

    switch (channelConnectionState)
    {
        case FB_CON_CONNECTING:
            switch (tcpState)
            {
                // In case the other end accepts the connection and closes it very early, we will not see the 'established' state (poll too slow)
                // So from our viewpoint, the connection will go into 'close wait' directly.
                case ETLM_TCP_STATE_CLOSE_WAIT:
                case ETLM_TCP_STATE_ESTABLISHED:
                    channel->initialSeqNumber = etlm_getTcpConnSndUna(pDevExt, tcp_conn);
                    // At this point the SYN packet is the only one that has been sent and acknowledged, so we need the -1
                    channel->initialSeqNumber -= 1;
                    previousConnectionState = set_channel_connection_state(pDevExt, channel, FB_CON_CONNECTED, NULL, false);
                    spin_unlock(&channel->connectionStateSpinLock);

                    if (unlikely(log))
                    {
                        PRINTK("fbProcessTCPChannel: FB_CON_CONNECTING, channel %d, tcpState = %s\n", dma_channel_no, EtlmState(tcpState));
                        PRINTK("fbProcessTCPChannel: channel %d connection state=%s ; tcpState=%s\n",
                            dma_channel_no, ConnState(channelConnectionState), EtlmState(tcpState));
                    }
                    set_channel_connection_state_msg(pDevExt, channel, previousConnectionState, FB_CON_CONNECTED, nameof(fbProcessTCPChannel)"#2");

                    complete(&channel->connectCompletion); // Signal waiters that connection has been established

                    break;
                case ETLM_TCP_STATE_CLOSED:
                    previousConnectionState = set_channel_connection_state(pDevExt, channel, FB_CON_CLOSED, NULL, false);
                    spin_unlock(&channel->connectionStateSpinLock);

                    if (unlikely(log))
                    {
                        PRINTK("fbProcessTCPChannel: FB_CON_CONNECTING, channel %d, tcpState = %s\n", dma_channel_no, EtlmState(tcpState));
                        PRINTK("fbProcessTCPChannel: channel %d connection state=%s ; tcpState=%s\n",
                            dma_channel_no, ConnState(channelConnectionState), EtlmState(tcpState));
                    }
                    set_channel_connection_state_msg(pDevExt, channel, previousConnectionState, FB_CON_CLOSED, nameof(fbProcessTCPChannel)"#3");

                    complete(&channel->connectCompletion); // Signal waiters that connection has been closed

                    break;
                default:
                    spin_unlock(&channel->connectionStateSpinLock);
                    PRINTK_C(unlikely(log), "fbProcessTCPChannel: FB_CON_CONNECTING, channel %d, tcpState = %s\n", dma_channel_no, EtlmState(tcpState));
                    break;
            }
            break;

        case FB_CON_CLOSING:
            switch (tcpState)
            {
                case ETLM_TCP_STATE_CLOSED:
                    previousConnectionState = set_channel_connection_state(pDevExt, channel, FB_CON_CLOSED, NULL, false);
                    spin_unlock(&channel->connectionStateSpinLock);

                    if (unlikely(log))
                    {
                        PRINTK("fbProcessTCPChannel: FB_CON_CLOSING, channel %d, tcpState = %s\n", dma_channel_no, EtlmState(tcpState));
                        PRINTK("fbProcessTCPChannel: channel %d connection state=%s ; tcpState=%s\n",
                            dma_channel_no, ConnState(channelConnectionState), EtlmState(tcpState));
                    }
                    set_channel_connection_state_msg(pDevExt, channel, previousConnectionState, FB_CON_CLOSED, nameof(fbProcessTCPChannel)"#4");

                    complete(&channel->connectCompletion);

                    break;
                default:
                    spin_unlock(&channel->connectionStateSpinLock);
                    PRINTK_C(unlikely(log), "fbProcessTCPChannel: FB_CON_CLOSING, channel %d, tcpState = %s\n", dma_channel_no, EtlmState(tcpState));
                    break;
            }
            break;

        case FB_CON_LISTENING:
            switch (tcpState)
            {
                // In case the other end closes the connection very early, we will not see the 'established' state (poll too slow)
                // So from our viewpoint, the connection will go into 'close wait' directly.
                case ETLM_TCP_STATE_CLOSE_WAIT:
                case ETLM_TCP_STATE_ESTABLISHED:
                    previousConnectionState = set_channel_connection_state(pDevExt, channel, FB_CON_CONNECTED, NULL, false);
                    spin_unlock(&channel->connectionStateSpinLock);

                    if (unlikely(log))
                    {
                        PRINTK("fbProcessTCPChannel: FB_CON_LISTENING, channel %d tcp state: %s\n", dma_channel_no, EtlmState(tcpState));
                        PRINTK("fbProcessTCPChannel:FB_CON_LISTENING, channel %d connection state=FB_CON_LISTENING ; tcpState=%s\n",
                            dma_channel_no, EtlmState(tcpState));
                    }
                    set_channel_connection_state_msg(pDevExt, channel, previousConnectionState, FB_CON_CONNECTED, nameof(fbProcessTCPChannel)"#5");

                    complete(&channel->connectCompletion);

                    break;
                case ETLM_TCP_STATE_CLOSED:
                    previousConnectionState = set_channel_connection_state(pDevExt, channel, FB_CON_CLOSED, NULL, false);
                    spin_unlock(&channel->connectionStateSpinLock);

                    if (unlikely(log))
                    {
                        PRINTK("fbProcessTCPChannel: FB_CON_LISTENING, channel %d tcp state: %s\n", dma_channel_no, EtlmState(tcpState));
                        PRINTK("fbProcessTCPChannel: FB_CON_LISTENING, channel %d connection state=FB_CON_LISTENING ; tcpState=%s\n",
                            dma_channel_no, EtlmState(tcpState));
                    }
                    set_channel_connection_state_msg(pDevExt, channel, previousConnectionState, FB_CON_CLOSED, nameof(fbProcessTCPChannel)"#6");

                    complete(&channel->connectCompletion);

                    break;
                default:
                    spin_unlock(&channel->connectionStateSpinLock);
                    PRINTK_C(unlikely(log), "fbProcessTCPChannel: FB_CON_LISTENING, channel %d tcp state: %s\n", dma_channel_no, EtlmState(tcpState));
                    break;
            }
            break;

        default:
            spin_unlock(&channel->connectionStateSpinLock);
            break;
    }

    return 0;
}

