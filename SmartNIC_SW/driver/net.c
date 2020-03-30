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
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>

#include "main.h"
#include "net.h"
#include "fpga.h"
#include "channel.h"

#define FB_BROADCAST_MAC_ADDR "\xff\xff\xff\xff\xff\xff"

#define ICMP_PROTOCOL 1
#define IGMP_PROTOCOL 2

static unsigned int frameIden = 0x3233;

static struct sk_buff *allocate_skb(const sc_net_interface_t * nif, int sz)
{
    struct net_device * dev = nif->netdevice;
    struct sk_buff * skb = netdev_alloc_skb_ip_align(dev, sz);
    skb->dev = dev;
    return skb;
}

const char * send_skb_reason(int rc)
{
    // Return codes from __dev_xmit_skb: http://elixir.free-electrons.com/linux/v4.13.9/source/net/core/dev.c#L3147
    // Also see http://elixir.free-electrons.com/linux/v4.13.9/source/include/linux/netdevice.h#L96

    switch (rc)
    {
        case NET_XMIT_SUCCESS:  return "NET_XMIT_SUCCESS";
        case NET_XMIT_DROP:     return "NET_XMIT_DROP: skb dropped";
        case NET_XMIT_CN:       return "NET_XMIT_CN: congestion notification";
#ifdef NET_XMIT_POLICED // This has been removed from Linux kernel versions 4.8 and up
        case NET_XMIT_POLICED:  return "NET_XMIT_POLICED: skb is shot by police";
#endif
        case NET_XMIT_MASK:     return "NET_XMIT_MASK: qdisc flags in net/sch_generic.h";
    }

    return " *** UNKNOWN send_skb reason *** ";
}

static void log_send_skb_errors(const sc_net_interface_t * nif, int rc, const char * callContext)
{
    // Only write an error if carrier is on
    const char * reason = send_skb_reason(rc);
    fb_channel_t * channel = nif->pDevExt->channel[nif->oob_channel_no];
    int mtu = nif->netdevice->mtu;
    const device_context_t * pDevExt = nif->pDevExt;

    PRINTK_ERR("send_skb from %s: dev_queue_xmit error code %d: %s, netif_queue_stopped=%d, tx_possible=%d, carrier=%d\n",
        callContext, rc, reason, netif_queue_stopped(nif->netdevice),
        channel_tx_possible(channel, mtu), nif->carrier_is_on);
}

static int send_skb(const sc_net_interface_t * nif, struct sk_buff * skb, unsigned int len, const char * callContext)
{
    int rc;

    skb_put(skb, len);
    skb->protocol = htons(ETH_P_IP);
    rc = dev_queue_xmit(skb);
    if (rc != NET_XMIT_SUCCESS)
    {
        if (nif->carrier_is_on)
        {
            if (LOG(LOG_NIC_TX))
            {
                log_send_skb_errors(nif, rc, callContext);
            }
        }
        else
        {
            // Carrier is off, try updating carrier status now because the normal timer callback might be 100ms away.
            device_context_t * pDevExt = nif->pDevExt;
            update_link_status(pDevExt);

            PRINTK_C(LOG(LOG_NIC_TX), "send_skb from %s: carrier was off, link status updated, carrier is now %s\n",
                callContext, nif->carrier_is_on ? "on" : "off");
        }
    }

    if (LOG(LOG_NIC))
    {
        const device_context_t * pDevExt = nif->pDevExt;
        PRINTK("send_skb called from %s: result %d\n", callContext, rc);
    }

    return rc;
}

/**
 *  Convert UDP multicast IP address to a MAC address.
 *
 *  https://networklessons.com/multicast/multicast-ip-address-to-mac-address-mapping/
 */
void multicast_to_mac(uint32_t addr, char mac[MAC_ADDR_LEN])
{
    // Multicast prefix for layer 2:
    mac[0] = 0x01; // Multicast/broadcast bit
    mac[1] = 0x00;
    mac[2] = 0x5e;

    addr = htonl(addr & 0x7fffff); // Host to network byte order

    memcpy(&mac[3], (char *)(&addr) + 1, 3);
}

int createEtherFrameHeader(uint8_t *buffer, const FbMacAddr destinationMacAddr, const FbMacAddr sourceMacAddr, uint16_t etherType, int16_t vlanTag)
{
    FbEtherFrameHeader *frameHeader = (FbEtherFrameHeader *)buffer;

    memcpy(frameHeader->destinationMacAddr, destinationMacAddr, MAC_ADDR_LEN);
    memcpy(frameHeader->sourceMacAddr, sourceMacAddr, MAC_ADDR_LEN);
    if (vlanTag != NO_VLAN_TAG)
    {
        frameHeader->etherType = htons(ETHER_TYPE_VLAN);
        frameHeader->pcp_dei_vid = htons(vlanTag & 0xFFF); // PCP = 0 and DEI = 0
        frameHeader->actualEtherType = htons(etherType);
        return 14 + 4; // Ethernet frame header size grows 4 bytes due to VLAN fields.
    }
    else
    {
        frameHeader->etherType = htons(etherType);
        return 14;
    }
}

int createEtherFrameFooter(uint8_t *buffer, int frameLen)
{
    if (frameLen < MIN_FRAME_SIZE) // Frame length should always be at least 60
    {
        //uint8_t i;
        //for (i = 0; i < MIN_FRAME_SIZE - frameLen; ++i) *(buffer + frameLen + i) = i;
        memset(buffer + frameLen, 0, MIN_FRAME_SIZE - frameLen);
        return MIN_FRAME_SIZE - frameLen;
    }
    return 0;
}

int createIpHeader(char *buf, size_t len,
    uint16_t frameLen, uint8_t protocol,
    uint32_t srcIp, uint32_t destIp, uint8_t ttl)
{
    FbIpHeader *frame = (FbIpHeader *)buf;

    frame->headerLength = len / 4;
    frame->version = 4;
    frame->explicitCongestionNotification = 0;
    frame->differentiatedServicesCodePoint = 0;
    frame->totalLength = htons(frameLen);
    frame->identification = htons(frameIden++);
    frame->w2 = 0; //htons(0x02 << 9);
    frame->ttl = ttl;
    frame->protocol = protocol;
    frame->headerChecksum = 0;
    frame->srcIpAddr = htonl(srcIp);
    frame->dstIpAddr = htonl(destIp);

    return sizeof(FbIpHeader);
}

int createDummyPacket(uint8_t *buffer, size_t payloadLen, int16_t vlanTag)
{
    int offset = createEtherFrameHeader(buffer, FB_BROADCAST_MAC_ADDR, "\x00\x21\xB2\x01\x00\x00", ETHER_TYPE_IPV4, vlanTag);
    offset += payloadLen;
    offset += createEtherFrameFooter(buffer, offset);
    return offset;
}

static int createIpOption(uint8_t * buffer)
{
    *(uint32_t *)(buffer) = htonl(0x94040000); // Router Alert: Every router examines packet
    return 4;
}

static int createIgmpLeave(uint8_t *buffer, const FbMacAddr destinationMacAddr, const FbMacAddr sourceMacAddr, uint32_t srcIp, uint32_t group, int16_t vlanTag)
{
    int offset = createEtherFrameHeader(buffer, destinationMacAddr, sourceMacAddr, ETHER_TYPE_IPV4, vlanTag);
    FbIpHeader *ipHeader = (FbIpHeader *)&buffer[offset];
    FbIgmpHeader *igmpHeader = 0;
    offset += createIpHeader(&buffer[offset], 24, 24 + 14, IGMP_PROTOCOL, srcIp, group, 1);
    offset += createIpOption(&buffer[offset]);
    igmpHeader = (FbIgmpHeader*)&buffer[offset];
    igmpHeader->type = 0x17;
    igmpHeader->maxRespCode = 0;
    igmpHeader->checksum = 0;
    igmpHeader->group = htonl(group);
    igmpHeader->checksum = FbCalcIpChecksum((uint8_t*)igmpHeader, sizeof(FbIgmpHeader));
    offset += sizeof(FbIgmpHeader);
    ipHeader->totalLength = htons(offset);
    ipHeader->headerChecksum = FbCalcIpChecksum((uint8_t*)ipHeader, sizeof(FbIpHeader) + 4);
    return offset;
}

static int createIgmpReport(uint8_t *buffer, const FbMacAddr destinationMacAddr, const FbMacAddr sourceMacAddr, uint32_t srcIp, uint32_t dstGroup, int16_t vlanTag)
{
    int offset = createEtherFrameHeader(buffer, destinationMacAddr, sourceMacAddr, ETHER_TYPE_IPV4, vlanTag);
    FbIpHeader *ipHeader = (FbIpHeader *)(&buffer[offset]);
    FbIgmpHeader *igmpHeader = 0;
    offset += createIpHeader(&buffer[offset], 24, 24 + 14, IGMP_PROTOCOL, srcIp, dstGroup, 1);
    offset += createIpOption(&buffer[offset]);
    igmpHeader = (FbIgmpHeader*)&buffer[offset];
    igmpHeader->type = 0x16;
    igmpHeader->maxRespCode = 0;
    igmpHeader->checksum = 0;
    igmpHeader->group = htonl(dstGroup);
    igmpHeader->checksum = FbCalcIpChecksum((uint8_t*)igmpHeader, sizeof(FbIgmpHeader));
    offset += sizeof(FbIgmpHeader);
    ipHeader->totalLength = htons(offset);
    ipHeader->headerChecksum = 0;
    ipHeader->headerChecksum = FbCalcIpChecksum((uint8_t*)ipHeader, sizeof(FbIpHeader) + 4);
    return offset;
}

int sendIgmpLeave(const sc_net_interface_t * nif, uint32_t srcIp, uint32_t group, int16_t vlanTag)
{
  // fixed by Ekaline
#if 1
    int rc;
    int retry = 2;

    do
    {
        unsigned int len = 0;
        char dstMac[MAC_ADDR_LEN];
        struct sk_buff * skb = allocate_skb(nif, SC_BUCKET_SIZE);
        uint8_t * bucketPayload = (uint8_t *)skb->data;
        memset(bucketPayload, 0, MIN_FRAME_SIZE);
        multicast_to_mac(group, dstMac);
        len = createIgmpLeave(bucketPayload, dstMac, nif->mac_addr, srcIp, group, vlanTag);
        rc = send_skb(nif, skb, len, __func__);

    } while (rc != NET_XMIT_SUCCESS && --retry > 0);
#endif
    return NET_XMIT_SUCCESS;
    //    return rc;
}

int sendIgmpReport(const sc_net_interface_t * nif, uint32_t srcIp, uint32_t dstGroup, int16_t vlanTag)
{
  // fixed by Ekaline
#if 1
    int rc;
    int retry = 2;

    do
    {
        char dstMac[MAC_ADDR_LEN];
        unsigned int len = 0;
        struct sk_buff * skb = allocate_skb(nif, SC_BUCKET_SIZE);
        uint8_t * bucketPayload = (uint8_t *)skb->data;
        memset(bucketPayload, 0, MIN_FRAME_SIZE);
        multicast_to_mac(dstGroup, dstMac);
        len = createIgmpReport(bucketPayload, dstMac, nif->mac_addr, srcIp, dstGroup, vlanTag);
        rc = send_skb(nif, skb, len, __func__);

    } while (rc != NET_XMIT_SUCCESS && --retry > 0);
#endif
    return NET_XMIT_SUCCESS;
    //    return rc;
}

static int createArpRequest(const sc_net_interface_t * nif, uint8_t *buffer, const FbMacAddr senderHwAddr, uint32_t senderProtocolAddr, uint32_t targetProtocolAddr, int16_t vlanTag)
{
    int offset = createEtherFrameHeader(buffer, FB_BROADCAST_MAC_ADDR, senderHwAddr, ETHER_TYPE_ARP, vlanTag);

    FbArpHeader *arpPacket = (FbArpHeader*)&buffer[offset];
    arpPacket->hwType = htons(0x0001); // Ethernet = 1
    arpPacket->protocolType = htons(ETHER_TYPE_IPV4);
    arpPacket->hwAddrLen = 6;
    arpPacket->protocolAddrLen = 4;
    arpPacket->operation = FB_ARP_REQUEST;
    memcpy(arpPacket->senderHwAddr, senderHwAddr, MAC_ADDR_LEN);
    arpPacket->senderProtocolAddr = htonl(senderProtocolAddr);
    memset(arpPacket->targetHwAddr, 0, MAC_ADDR_LEN);
    arpPacket->targetProtocolAddr = htonl(targetProtocolAddr);
    offset += sizeof(FbArpHeader);
    offset += createEtherFrameFooter(&buffer[offset], offset);

    if (LOG(LOG_ARP))
    {
        IPBUF(1); IPBUF(2);
        const device_context_t * pDevExt = nif->pDevExt;
        PRINTK("createArpRequest2: senderProtocolAddr=%s, targetProtocolAddr=%s, length=%d\n",
            LogIP(1, senderProtocolAddr), LogIP(2, targetProtocolAddr), offset);

        if (LOG(LOG_DUMP))
        {
            DUMP_PACKET(nif->pDevExt, "createArpRequest: ", buffer, offset);
        }
    }

    return offset;
}

int sendArpRequest(const sc_net_interface_t * nif, uint32_t destinationIp, int16_t vlanTag)
{
    int rc;
    int retry = 2;

    do
    {
        struct sk_buff * skb = allocate_skb(nif, SC_BUCKET_SIZE);
        uint8_t * bucketPayload = (uint8_t *)skb->data;
        const int len = createArpRequest(nif, bucketPayload, nif->mac_addr, get_local_ip(nif), destinationIp, vlanTag);
        rc = send_skb(nif, skb, len, "sendArpRequest");

    } while (rc != NET_XMIT_SUCCESS && --retry > 0);

    if (rc == NET_XMIT_SUCCESS && LOG(LOG_ARP))
    {
        const device_context_t * pDevExt = nif->pDevExt;
        PRINTK("ARP request sent on lane %d, send_skb rc=%d\n", nif->network_interface, rc);
    }

    return rc;
}

void FbDbgDumpMacAddr(const device_context_t * pDevExt, const char *header, const FbMacAddr macAddr)
{
    PRINTK("%s%02x:%02x:%02x:%02x:%02x:%02x\n", header, (unsigned int)macAddr[0], (unsigned int)macAddr[1], (unsigned int)macAddr[2], (unsigned int)macAddr[3], (unsigned int)macAddr[4], (unsigned int)macAddr[5]);
}

void FbDbgDumpIpAddr(const device_context_t * pDevExt, const char *header, uint32_t ipAddr)
{
    PRINTK("%s%u.%u.%u.%u\n", header, (ipAddr >> 0) & 0xff, (ipAddr >> 8) & 0xff, (ipAddr >> 16) & 0xff, (ipAddr >> 24) & 0xff);
}

uint16_t FbCalcIpChecksum(const uint8_t *buf, uint32_t len)
{
    register int32_t sum = 0;
    register uint16_t *p = (uint16_t*)buf;
    register uint32_t i = len;
    while (i > 1)
    {
        sum += *p++;
        i -= sizeof(uint16_t);
    }
    if (i)
        sum += *(uint8_t*)p;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}
