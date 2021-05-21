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
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/hardirq.h>
#include <linux/inetdevice.h>

#include <linux/ip.h> // added by ekaline
#include <linux/tcp.h> // added by ekaline

#include "main.h"
#include "ndev.h"
#include "channel.h"
#include "fpga.h"
#include "oob_chan.h"

 // This file implements standard NIC using the Linux NAPI interface.

#define ETH_FRAME_HEADER_SZ 14

static int fb_set_mac_address(struct net_device *dev, void * pAddress);

struct fb_ndev {
    spinlock_t  lock;
    sc_net_interface_t *nif;
    struct napi_struct napi;
};

// Forward declarations
static const struct net_device_ops fbndev_ops;

// Utility function to log the list of currently requested mcast addresses on a device
static void dump_mc_list(struct net_device *dev)
{
    struct fb_ndev             *ndev = (struct fb_ndev*)netdev_priv(dev);
    const device_context_t * pDevExt = ndev->nif->pDevExt;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
    struct dev_addr_list * cur = dev->mc_list;

    PRINTK_DBG("List of multicast address on iface %s (oob_channel_no %d): %d entries\n", dev->name, ndev->nif->oob_channel_no, netdev_mc_count(dev));

    while (cur)
    {
        char msg[100];

        const u8 *mac = cur->da_addr;
        int length = snprintf(msg, sizeof(msg), "Mac addr: %02x:%02x:%02x:%02x:%02x:%02x, ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        length += snprintf(&msg[length], sizeof(msg) - length, " users: %3d", cur->da_users);
        PRINTK_DBG("%s\n", msg);
        cur = cur->next;
    }
#else
    struct netdev_hw_addr      *cur;

    PRINTK_DBG("List of multicast address on iface %s (oob_channel_no %d): %d entries\n", dev->name, ndev->nif->oob_channel_no, netdev_mc_count(dev));
    netdev_for_each_mc_addr(cur, dev)
    {
        char msg[100];

        const u8 *mac = cur->addr;
        int length = snprintf(msg, sizeof(msg), "Mac addr: %02x:%02x:%02x:%02x:%02x:%02x, ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        length += snprintf(&msg[length], sizeof(msg) - length, " type: %d (5 is mcast),  refs: %3d\n", cur->type, cur->refcount);
        PRINTK_DBG("%s", msg);
    }
#endif
}

/**
 *  NAPI polling function.
 *
 *  @brief  Instead of being interrupted by each incoming network packet, Linux calls
 *          this function to poll for incoming packets that the driver is pooling
 *          in the receive DMA buffer.
 */
static int fb_poll(struct napi_struct *napi, int budget)
{
    struct fb_ndev *ndev = netdev_priv(napi->dev);
    int work_done = 0;

    if (ndev->nif->pDevExt->reset_fpga)
    {
        if (LOG(LOG_FPGA_RESET))
        {
            const device_context_t * pDevExt = ndev->nif->pDevExt;
            PRINTK("FPGA RESET in progress in function %s\n", __func__);
        }

        return 0; // Don't do any NAPI work while FPGA reset is in progress.
    }

    work_done = fbProcessOobChannel(ndev->nif, budget);

    if (work_done < budget)
    {
        napi_complete(napi);

        start_single_oob_interrupt(ndev->nif);

        // Close the race condition where an interrupt could be lost, between napi_complete() and enable_interrupts()
        //for(i=0; i<FB_NO_NETWORK_LANES; i++)
        //{
        //    if (pDevExt->nif[i].netdevice)
        //        work_done += fbProcessOobChannel(&pDevExt->nif[i], budget);
        //}
    }

    // Returning a value higher than budget cause a warning in the kernel
    if (work_done > budget)
        work_done = budget;
    return work_done;
}

static void fb_init(struct net_device *dev)
{
    struct fb_ndev *ndev = netdev_priv(dev);

    ether_setup(dev);

    dev->netdev_ops = &fbndev_ops;
    dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_HIGHDMA;
    dev->flags |= IFF_MULTICAST;
    //dev->hw_features |= NETIF_F_SG;

    // 64 is weight, a measure of the importance of the interface.
    // Use weight of 64 for gigabit or faster networks.
    netif_napi_add(dev, &ndev->napi, &fb_poll, 64);

    spin_lock_init(&ndev->lock);
}

/**
 *  Setup and initialize a standad NIC interface for Linux.
 *
 *  Called from smartnic_probe (via smartnic_start_device) for every lane which has a standard NIC (OOB).
 */
int fb_setup_netdev(device_context_t * pDevExt, sc_net_interface_t * nif)
{
    struct net_device *dev;
    struct fb_ndev *ndev;
    char name[25];
    int lane = nif->network_interface;
    int rc = 0;

    if (nif && nif->netdevice)
    {
        int rc;
        // When reconfiguring the card, we must first tear down the interface
        PRINTK(" must cleanup first lane %d...\n", lane);
        rc = fb_cleanup_netdev(pDevExt, nif);
        if (rc)
        {
            PRINTK_ERR("Lane %d cleanup with rc %d\n", lane, rc);
            return rc;
        }
    }

    PRINTK_C(LOG(LOG_INIT), "Initialize smartnic %s(0x%02X) network interface %s%d on lane %u\n",
        FpgaName(pDevExt->rev.parsed.type), pDevExt->rev.parsed.type, interface_name, pDevExt->lane_base + nif->network_interface, nif->network_interface);

    snprintf(name, sizeof(name), "%s%d", interface_name, pDevExt->lane_base + lane);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
    dev = alloc_netdev(sizeof(struct fb_ndev), name, fb_init);
#else
    dev = alloc_netdev(sizeof(struct fb_ndev), name, NET_NAME_UNKNOWN, fb_init); // TODO: should we use NET_NAME_USER instead?
#endif
    if (dev == NULL)
    {
        PRINTK_ERR("alloc_netdev failed!");
        return -ENOMEM;
    }
    dev->mtu = SC_MAX_MTU_SIZE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
    dev->max_mtu = dev->mtu;
#endif

    ndev = netdev_priv(dev);
    ndev->nif = nif;

    // Let sc_net_interface_t have knowledge about this interface;
    nif->netdevice = dev;

    // TODO: why does not eth_mac_addr(...) or fb_set_mac_address(...) work here?
    // TODO: how about dev_set_mac_address?
    memcpy(dev->dev_addr, ndev->nif->mac_addr, ETH_ALEN);

    setSourceMac(pDevExt, lane, nif->mac_addr);
    if (pDevExt->tcp_lane_mask != 0 && lane == pDevExt->tcp_lane)
    {
        etlm_setSrcMac(pDevExt, nif->mac_addr);

        if (LOG(LOG_INIT | LOG_MAC))
        {
            MACBUF(0);
            PRINTK("MAC address of NIC on lane %u was set to %s\n", nif->network_interface, LogMAC(0, nif->mac_addr));
        }
    }

    // Set the sysfs physical device reference for the network logical device
    SET_NETDEV_DEV(dev, &pDevExt->pdev->dev);

    //PRINTK("register_netdev('%s')\n", name);
    rc = register_netdev(dev);
    if (rc != 0)
    {
        MACBUF(0);
        PRINTK_ERR("register_netdev failed for device '%s' MAC address %s failed with rc %d\n", name, LogMAC(0, nif->mac_addr), rc);
        return rc;
    }

    netif_carrier_off(dev);
    netif_dormant_off(dev);
    nif->carrier_is_on = false;

    sysfs_format_mac(name, ndev->nif->mac_addr, ETH_ALEN);
    PRINTK("   %s lane %d MAC: %s", dev->name, lane, name);

    return 0;
}

int fb_cleanup_netdev(const device_context_t * pDevExt, sc_net_interface_t *nif)
{
    if (nif->netdevice != NULL)
    {
        unregister_netdev(nif->netdevice);

        free_netdev(nif->netdevice);

        nif->netdevice = 0;
    }

    return 0;
}

void fb_update_netdev_linkstatus(device_context_t * pDevExt, uint64_t linkStatus)
{
    unsigned int i;
    for (i = 0; i < SC_NO_NETWORK_INTERFACES; i++)
    {
        struct net_device * pNetDevice = pDevExt->nif[i].netdevice;

        if (pNetDevice != NULL)
        {
            if (linkStatus & (1ULL << (8 * i)))
            {
                netif_carrier_on(pNetDevice);
                pDevExt->nif[i].carrier_is_on = true;
            }
            else
            {
                netif_carrier_off(pNetDevice);
                pDevExt->nif[i].carrier_is_on = false;
            }
        }
    }
}

/* this is called from interrupt context */
void fb_schedule_napi_dev(const device_context_t * pDevExt, int dev_no)
{
    if (pDevExt->nif[dev_no].netdevice != NULL)
    {
        struct fb_ndev *ndev = netdev_priv(pDevExt->nif[dev_no].netdevice);
        napi_schedule(&ndev->napi);
    }
}

/* this is called from interrupt context */
void fb_schedule_napi_alldev(const device_context_t * pDevExt)
{
    int i;

    for (i = 0; i < SC_NO_NETWORK_INTERFACES; i++)
    {
        if (pDevExt->nif[i].netdevice && (pDevExt->nif[i].netdevice->flags & IFF_UP))
        {
            struct fb_ndev *ndev = netdev_priv(pDevExt->nif[i].netdevice);
            napi_schedule(&ndev->napi);
            // In MSIX mode, each device processes its own OOB channel
        }
    }
}


/* Prints the IP level settings of the kernel network stack (ifconfig).
   This can be used if we decide to used the kernel for configuration, arp, ping, etc... */
static void dump_ifconfig_settings(sc_net_interface_t * nif)
{
    struct net_device *dev;
    struct in_device *in_dev;
    struct in_ifaddr **ifap = NULL;
    struct in_ifaddr *ifa = NULL;
    const device_context_t * pDevExt = nif->pDevExt;

    FUNCTION_ENTRY(LOG_NIC, "nif=%p", nif);

    dev = nif->netdevice;
    in_dev = __in_dev_get_rtnl(dev);

    if  (LOG(LOG_NIC_OPS))
    {
        IPBUF(1); IPBUF(2); IPBUF(3);

        PRINTK("dev %s ifap listing: mtu %d, DMA channel %u\n", dev->name, dev->mtu, nif->oob_channel_no);
        PRINTK("before: nif local IP=%s netmask=%s gateway=%s\n", LogIP(1, get_local_ip(nif)),
            LogIP(2, nif->netmask), LogIP(3, nif->gateway));
    }
    for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL; ifap = &ifa->ifa_next)
    {

      PRINTK("EKALINE DEBUG dump_ifconfig_settings: \n");
        bool log = LOGALL(LOG_NIC | LOG_DETAIL);

        char localbuf[20], maskbuf[20], broadcastbuf[20], gatewaybuf[20];
        util_format_ip(localbuf, sizeof(localbuf), ifa->ifa_local);
        util_format_ip(maskbuf, sizeof(maskbuf), ifa->ifa_mask);
        util_format_ip(broadcastbuf, sizeof(broadcastbuf), ifa->ifa_broadcast);
        util_format_ip(gatewaybuf, sizeof(gatewaybuf), ifa->ifa_address);
        PRINTK_C(!log, "%s ifap name %s, ip %s\n", dev->name, ifa->ifa_label, localbuf);
        PRINTK_C(log, "%s ifap name %s, ip %s, net mask %s, broadcast %s, gateway %s\n",
            dev->name, ifa->ifa_label, localbuf, maskbuf, broadcastbuf, gatewaybuf);

        if (nif->netmask == 0)
        {
            // ifa_mask seems to be out-of-date here, it contains the previous netmask? where is the up-to-date netmask?
            nif->netmask = ntohl(ifa->ifa_mask);
        }
        //if (nif->gateway == 0)
        //{
        //    nif->gateway = ntohl(ifa->ifa_address);
        //}

        if (get_local_ip(nif) == 0)
        {
            set_local_ip(nif, ntohl(ifa->ifa_local));
        }
    }

    //fixed by ekaline
    //    if (get_local_ip(nif) == 0) set_local_ip(nif, ntohl(ifa->ifa_local));

    if (LOG(LOG_NIC_OPS))
    {
        IPBUF(1); IPBUF(2); IPBUF(3);
        PRINTK("after: nif local IP=%s netmask=%s gateway=%s\n", LogIP(1, get_local_ip(nif)),
            LogIP(2, nif->netmask), LogIP(3, nif->gateway));
    }

    FUNCTION_EXIT(LOG_NIC, "");
}



/** **************************************************************************
 * @defgroup NicFunctions Standard NIC driver interface functions.
 * @{
 * @brief These functions are called when via standard OS interfaces.
 */


static int fb_open(struct net_device *dev)
{
    struct fb_ndev * ndev = (struct fb_ndev*)netdev_priv(dev);
    device_context_t * pDevExt = ndev->nif->pDevExt;
    fb_channel_t * pChannel = pDevExt->channel[ndev->nif->oob_channel_no];

    PRINTK_C(LOG(LOG_NIC_OPS), ">fb_open called on DMA channel %u\n", pChannel->dma_channel_no);

    if (pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);

        return -EBUSY; // Do not do anything else while FPGA reset is in progress.
    }

    //if (ndev->nif->oob_channel_no != 10)
    //    return -EINVAL;

    napi_enable(&ndev->napi);

    setBypass(pDevExt, ndev->nif->network_interface, 1);

    channel_reallocate_buckets(pChannel, dev->mtu + ETH_FRAME_HEADER_SZ);

    channel_reset_buckets(pChannel);

    dump_ifconfig_settings(ndev->nif);

    PRINTK_C(LOG(LOG_NIC_OPS), "<fb_open returning on DMA channel %u\n", pChannel->dma_channel_no);

    return 0;
}

static int fb_close(struct net_device *dev)
{
    struct fb_ndev * ndev = (struct fb_ndev*)netdev_priv(dev);
    device_context_t * pDevExt = ndev->nif->pDevExt;

    PRINTK_C(LOG(LOG_NIC_OPS), ">fb_close called on DMA channel %u\n", ndev->nif->oob_channel_no);

    if (pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);

        return -EBUSY; // Do not do anything else while FPGA reset is in progress.
    }

    netif_stop_queue(dev);

    setBypass(pDevExt, ndev->nif->network_interface, 0);

    napi_disable(&ndev->napi);

    PRINTK_C(LOG(LOG_NIC_OPS), "<fb_close returning on DMA channel %u\n", ndev->nif->oob_channel_no);

    return 0;
}

static int fb_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct fb_ndev * ndev = (struct fb_ndev*)netdev_priv(dev);
    device_context_t * pDevExt = ndev->nif->pDevExt;
    const unsigned int dma_channel_no = ndev->nif->oob_channel_no;
    fb_channel_t * pChannel = pDevExt->channel[dma_channel_no];
    int bucket;
    int len = skb->len;
    uint8_t * bucketPayload;
    unsigned long flags;
    int rc = NETDEV_TX_OK;

    PRINTK_C(LOGALL(LOG_NIC_OPS | LOG_TX | LOG_DETAIL), ">fb_start_xmit called on channel %u, len %d, data_len %d\n",
        ndev->nif->oob_channel_no, len, skb->data_len);

    if (pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);

        // Do not try to transmit while FPGA reset is in progress.
        rc = NETDEV_TX_OK; // Should not ever return NETDEV_TX_BUSY so we lose the packet!
        goto fb_start_xmit_exit;
    }

    if (len > pChannel->bucket_size)
    {
        PRINTK_ERR_C(LOG(LOG_DEBUG), "skb too big for bucket; %d bytes (bucket is %d bytes). Packet dropped\n",
            len, pChannel->bucket_size);

        dev->stats.tx_dropped++;
        dev_kfree_skb_any(skb);
        rc = NETDEV_TX_OK;
        goto fb_start_xmit_exit;
    }

    spin_lock_irqsave(&ndev->lock, flags);

    bucket = channel_get_free_bucket(pChannel, len);
    if (unlikely(bucket < 0))
    {
        PRINTK_ERR("No buckets available on channel %u, bucket %d len %d\n", dma_channel_no, bucket, len);
        DUMP_BUCKETS(pChannel);
        spin_unlock_irqrestore(&ndev->lock, flags);
        dev_kfree_skb_any(skb);
        rc = NETDEV_TX_OK; // Should not ever return NETDEV_TX_BUSY so we lose the packet!
        goto fb_start_xmit_exit;
    }

    bucketPayload = pChannel->bucket[bucket]->data;

    if (pad_frames != 0)
    {
        // Pad packet length
        if (len < MIN_FRAME_SIZE) // Ethernet packet minimum size is 60
        {
            memset(bucketPayload + len, 0, MIN_FRAME_SIZE - len);
            len = MIN_FRAME_SIZE;
        }
    }

    //DUMP_PACKET("fb_start_xmit: ", bucketPayload, len);

    skb_copy_and_csum_dev(skb, bucketPayload);
    //-------------------------------------------------------------------        
#include "ekaline_tx.c" // added by ekaline
    if (! eka_drop_me)
    //-------------------------------------------------------------------      
    channel_send_bucket(pChannel, bucket, len);

    dev->stats.tx_bytes += len;
    dev->stats.tx_packets++;

    // Make sure that it is possible to send one more packet
    {
        int count = 10;
        while ((!channel_tx_possible(pChannel, dev->mtu)) && count--)
        {
            __asm__ __volatile__("pause");
        }

        if (unlikely(count == 0))
        {
            // Flow control means we cannot send.
            // The strategy to check flow control AFTER channel_send_bucket
            // seems to be to minimize the number of calls to netif_stop_queue.
            // If netif_stop_queue is called then netif_wake_queue is first called
            // in the timer callback which runs every 100 ms.
            // TODO: seems like we could get better NIC perfomance in that case 
            // by calling netif_wake_queue much earlier than after 0-100 ms (min to max, 50 ms expected value).
            PRINTK("no more Tx slots available on DMA channel %d\n", dma_channel_no);
            netif_stop_queue(dev);
            // Stopping the queue is enough, returning NETDEV_TX_BUSY is a hard error and MUST NOT HAPPEN.
            // see http://lxr.free-electrons.com/source/Documentation/networking/driver.txt
        }
    }

    spin_unlock_irqrestore(&ndev->lock, flags);
    dev_kfree_skb_any(skb);

fb_start_xmit_exit:

    PRINTK_C(LOGALL(LOG_NIC_OPS | LOG_TX | LOG_DETAIL), "<fb_start_xmit returning on channel %u with return value %d\n", ndev->nif->oob_channel_no, rc);

    return rc;
}

static int fb_set_mac_address(struct net_device *dev, void * pAddress)
{
    struct fb_ndev *ndev = (struct fb_ndev*)netdev_priv(dev);
    device_context_t * pDevExt = ndev->nif->pDevExt;
    int rc = 0;
    int lane = ndev->nif->network_interface;

    PRINTK_C(LOGALL(LOG_NIC_OPS | LOG_MAC), ">fb_set_mac_address called on lane %u channel %u\n", lane, ndev->nif->oob_channel_no);

    if (ndev->nif->pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);

        rc = -EBUSY; // Do not do anything else while FPGA reset is in progress.
        goto fb_set_mac_address_exit;
    }

    rc = eth_mac_addr(dev, pAddress);
    if (rc)
    {
        goto fb_set_mac_address_exit;
    }

    memcpy(ndev->nif->mac_addr, dev->dev_addr, ETH_ALEN);
    setSourceMac(pDevExt, lane, ndev->nif->mac_addr);
    if (pDevExt->tcp_lane_mask != 0 && lane == pDevExt->tcp_lane)
    {
        etlm_setSrcMac(pDevExt, ndev->nif->mac_addr);
    }

fb_set_mac_address_exit:

    PRINTK_C(LOGALL(LOG_NIC_OPS | LOG_MAC), "<fb_set_mac_address returning on lane %u channel %u with return value %d\n", lane, ndev->nif->oob_channel_no, rc);

    return rc;
}

static const char * dump_flags(char * buffer, int buffer_size, unsigned int flags)
{
    unsigned int mask = 1;
    const char * separator = "";

    *buffer = '\0';
    while (flags != 0)
    {
        if (strlen(buffer) + 3 + 16 >= buffer_size)
        {
            strcat(buffer, "...");
            return buffer;
        }
        strcat(buffer, separator);
        separator = " | ";

        switch (flags & mask)
        {
            case 0:                 separator = ""; break;
            case IFF_UP:            strcat(buffer, "IFF_UP"); break;
            case IFF_BROADCAST:     strcat(buffer, "IFF_BROADCAST"); break;
            case IFF_DEBUG:         strcat(buffer, "IFF_DEBUG"); break;
            case IFF_LOOPBACK:      strcat(buffer, "IFF_LOOPBACK"); break;
            case IFF_POINTOPOINT:   strcat(buffer, "IFF_POINTOPOINT"); break;
            case IFF_NOTRAILERS:    strcat(buffer, "IFF_NOTRAILERS"); break;
            case IFF_RUNNING:       strcat(buffer, "IFF_RUNNING"); break;
            case IFF_NOARP:         strcat(buffer, "IFF_NOARP"); break;
            case IFF_PROMISC:       strcat(buffer, "IFF_PROMISC"); break;
            case IFF_ALLMULTI:      strcat(buffer, "IFF_ALLMULTI"); break;
            case IFF_MASTER:        strcat(buffer, "IFF_MASTER"); break;
            case IFF_SLAVE:         strcat(buffer, "IFF_SLAVE"); break;
            case IFF_MULTICAST:     strcat(buffer, "IFF_MULTICAST"); break;
            case IFF_PORTSEL:       strcat(buffer, "IFF_PORTSEL"); break;
            case IFF_AUTOMEDIA:     strcat(buffer, "IFF_AUTOMEDIA"); break;
            case IFF_DYNAMIC:       strcat(buffer, "IFF_DYNAMIC"); break;
            case IFF_LOWER_UP:      strcat(buffer, "IFF_LOWER_UP"); break;
            case IFF_DORMANT:       strcat(buffer, "IFF_DORMANT"); break;
            case IFF_ECHO:          strcat(buffer, "IFF_ECHO"); break;
            default:                strcat(buffer, "*** UNKNOWN ***"); break;
        }

        flags &= ~mask;
        mask <<= 1;
    }

    return buffer;
}

static const char * dump_operstate(char * buffer, int buffer_size, unsigned int operstate)
{
    unsigned int mask = 1;
    const char * separator = "";

    *buffer = '\0';
    while (operstate != 0)
    {
        if (strlen(buffer) + 3 + 16 >= buffer_size)
        {
            strcat(buffer, "...");
            return buffer;
        }
        strcat(buffer, separator);
        separator = " | ";

        switch (operstate & mask)
        {
            case IF_OPER_UNKNOWN:           strcat(buffer, "IF_OPER_UNKNOWN"); break;
            case IF_OPER_NOTPRESENT:        strcat(buffer, "IF_OPER_NOTPRESENT"); break;
            case IF_OPER_DOWN:              strcat(buffer, "IF_OPER_DOWN"); break;
            case IF_OPER_LOWERLAYERDOWN:    strcat(buffer, "IF_OPER_LOWERLAYERDOWN"); break;
            case IF_OPER_TESTING:           strcat(buffer, "IF_OPER_TESTING"); break;
            case IF_OPER_DORMANT:           strcat(buffer, "IF_OPER_DORMANT"); break;
            case IF_OPER_UP:                strcat(buffer, "IF_OPER_UP"); break;
            default:                        strcat(buffer, "*** UNKNOWN ***"); break;
        }

        operstate &= ~mask;
        mask <<= 1;
    }

    return buffer;
}

// This is called by kernel when the device flags or mcast list changes.
// The IFF_ALLMULTI flag is not handled because the device is passing all mcast traffic when in 'bypass' mode
static void fb_set_rx_mode(struct net_device *dev)
{
    struct fb_ndev *ndev = (struct fb_ndev*)netdev_priv(dev);
    device_context_t * pDevExt = ndev->nif->pDevExt;
    unsigned int flags = dev->flags;

    if (LOG(LOG_NIC_OPS))
    {
        char buffer[100];
        PRINTK(">fb_set_rx_mode called on channel %u, flags = 0x%x (%s) operstate = %u (%s)\n",
            ndev->nif->oob_channel_no, flags, dump_flags(buffer, sizeof(buffer), flags),
            dev->operstate, dump_operstate(buffer, sizeof(buffer), dev->operstate));
    }

    if (pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);

        return; // Do not do anything else while FPGA reset is in progress.
    }

    if (flags & IFF_PROMISC)
    {
        if (LOG(LOG_DEBUG))
        {
            PRINTK_DBG("iface %s (oob_channel_no %d) going into promisc mode\n", dev->name, ndev->nif->oob_channel_no);
            dump_mc_list(dev);
        }
        setPromisc(pDevExt, ndev->nif->network_interface, 1);
    }
    else
    {
        if (LOG(LOG_DEBUG))
        {
            PRINTK_DBG("iface %s (oob_channel_no %d) going out of promisc mode\n", dev->name, ndev->nif->oob_channel_no);
            dump_mc_list(dev);
        }
        setPromisc(pDevExt, ndev->nif->network_interface, 0);
    }

    PRINTK_C(LOG(LOG_NIC_OPS), "<fb_set_rx_mode returning on channel %u\n", ndev->nif->oob_channel_no);
}

static int fb_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    struct fb_ndev * ndev = (struct fb_ndev*)netdev_priv(dev);
    const device_context_t * pDevExt = ndev->nif->pDevExt;

    PRINTK_C(LOG(LOG_NIC_OPS), ">fb_do_ioctl called on channel %u, cmd %s (0x%x)\n", ndev->nif->oob_channel_no, SocketFlags(cmd), cmd);

    if (pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);
    }

    if (cmd == SIOCSIFADDR)
    {
        struct fb_ndev * ndev = (struct fb_ndev*)netdev_priv(dev);
        const device_context_t * pDevExt = ndev->nif->pDevExt;
        IPBUF(1); IPBUF(2); IPBUF(3); IPBUF(4); IPBUF(5);
        PRINTK("SIOCSIFADDR: name=%s ifr_addr=%s ifr_dstaddr=%s ifr_broadaddr=%s ifr_netmask=%s ifr_hwaddr=%s\n",
            ifr->ifr_name, LogIPs(1, &ifr->ifr_addr), LogIPs(2, &ifr->ifr_dstaddr), LogIPs(3, &ifr->ifr_broadaddr),
            LogIPs(4, &ifr->ifr_netmask), LogIPs(5, &ifr->ifr_hwaddr));
    }

    if (LOG(LOG_NIC_OPS))
    {
        struct fb_ndev * ndev = (struct fb_ndev*)netdev_priv(dev);
        const device_context_t * pDevExt = ndev->nif->pDevExt;
        PRINTK("<fb_do_ioctl returning on channel %u with return value %d\n", ndev->nif->oob_channel_no, -ENOIOCTLCMD);
    }

    return -ENOIOCTLCMD;
}

int fb_change_mtu(struct net_device *dev, int new_mtu)
{
    struct fb_ndev  * ndev = (struct fb_ndev*)netdev_priv(dev);
    const device_context_t * pDevExt = ndev->nif->pDevExt;
    int rc = 0;

    PRINTK_C(LOG(LOG_NIC_OPS), ">fb_change_mtu called on channel %u\n", ndev->nif->oob_channel_no);

    if (pDevExt->reset_fpga)
    {
        PRINTK_C(LOG(LOG_FPGA_RESET), "FPGA RESET in progress in function %s\n", __func__);
    }

    if (new_mtu > SC_MAX_MTU_SIZE)
    {
        PRINTK_ERR("New MTU size %d is greater than maximum supported MTU size %lu\n", new_mtu, SC_MAX_MTU_SIZE);
        rc = -1;
        goto fb_change_mtu_exit;
    }

    /* The buckets might be in use if the interface is up, so better insist on having it down */
    if (dev->flags & IFF_UP)
    {
        PRINTK_ERR("changing MTU on lane %u should be done when the interface is down\n", ndev->nif->network_interface);
        rc = -1;
        goto fb_change_mtu_exit;
    }

    {
        fb_channel_t * pChannel = pDevExt->channel[ndev->nif->oob_channel_no];

        // No need to reallocate buckets when reducing their size
        if (new_mtu > dev->mtu)
            channel_reallocate_buckets(pChannel, new_mtu + ETH_FRAME_HEADER_SZ);
    }

    PRINTK_DBG("mtu changing to %d\n", new_mtu);
    dev->mtu = new_mtu;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
    dev->max_mtu = dev->mtu;
#endif

fb_change_mtu_exit:

    if (LOG(LOG_NIC_OPS))
    {
        struct fb_ndev *ndev = (struct fb_ndev*)netdev_priv(dev);
        PRINTK("<fb_change_mtu returning on channel %u with return value %d\n", ndev->nif->oob_channel_no, rc);
    }

    return rc;

    //return eth_change_mtu(dev, new_mtu);
}

#if 0
int fb_set_config(struct net_device * pNetDevice, struct ifmap * pMap)
{
    struct fb_ndev  * pFbDevice = (struct fb_ndev *)netdev_priv(pNetDevice);
    const device_context_t * pDevExt = pFbDevice->nif->pDevExt;

    PRINTK_C(true/*LOG(LOG_NIC_OPS)*/, ">fb_set_config called on channel %u\n", pFbDevice->nif->oob_channel_no);


    return 0;
}
#endif

static const struct net_device_ops fbndev_ops =
{
    .ndo_open = fb_open,
    .ndo_stop = fb_close,
    .ndo_start_xmit = fb_start_xmit,
    .ndo_set_mac_address = fb_set_mac_address,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
    .ndo_set_rx_mode = fb_set_rx_mode,
#else
    .ndo_set_multicast_list = fb_set_rx_mode,
#endif
    .ndo_do_ioctl = fb_do_ioctl,
#if defined(RHEL_RELEASE_CODE) && defined(RHEL_RELEASE_VERSION)
    /* start of Ekaline fix*/
    #if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 0)
    /* keep ndo_change_mtu */
    /* end of Ekaline fix*/
    #elif RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 5)
        /* Centos 7.5 kernel version 3.10.0-862.11.6.el7.x86_64 has renamed member ndo_change_mtu!!! */
        #define ndo_change_mtu ndo_change_mtu_rh74
    #endif
#endif
    .ndo_change_mtu = fb_change_mtu,
    //.ndo_set_config = fb_set_config,
};

/// @} NicFunctions
