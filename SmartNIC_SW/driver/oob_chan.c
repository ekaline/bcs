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
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#include "main.h"
#include "net.h"
#include "oob_chan.h"
#include "recv.h"
#include "fpga.h"
#include "ndev.h"
#include "connect.h"
#include "igmp.h"

#include <linux/ip.h> // added by ekaline
#include <linux/tcp.h> // added by ekaline

void FbDbgDumpMem(device_context_t * pDevExt, const uint8_t *buf, unsigned int len);

#if 0
static void fbDbgDumpArpHeader(const FbArpHeader *arpHeader, int minor)
{
    PRINTK("hwType             0x%04x\n", minor, htons(arpHeader->hwType));
    PRINTK("protocolType       0x%04x\n", minor, htons(arpHeader->protocolType));
    PRINTK("hwAddrLen          %u\n", minor, (unsigned int)arpHeader->hwAddrLen);
    PRINTK("protocolAddrLen    %u\n", minor, (unsigned int)arpHeader->protocolAddrLen);
    PRINTK("operation          0x%04x (%s)\n", minor, htons(arpHeader->operation), htons(arpHeader->operation) == 1 ? "Request" : "Reply");
#if 0
    FbDbgDumpMacAddr("senderHwAddr       ", arpHeader->senderHwAddr, minor);
    FbDbgDumpIpAddr("senderProtocolAddr ", arpHeader->senderProtocolAddr, minor);
    FbDbgDumpMacAddr("targetHwAddr       ", arpHeader->targetHwAddr, minor);
    FbDbgDumpIpAddr("targetProtocolAddr ", arpHeader->targetProtocolAddr, minor);
#endif
}
#endif

static bool fbHandleIpPacket(sc_net_interface_t *nif, const sc_packet *pkt)
{
    bool rc = true;
    const uint8_t *payload = sc_get_raw_payload(pkt);
    uint16_t len = sc_get_raw_payload_length(pkt);
    device_context_t * pDevExt = nif->pDevExt;
    const FbEtherFrameHeader *frameHeader = (FbEtherFrameHeader*)payload;
    int sizeOfEtherFrameHeader = GetSizeOfEthernetFrameHeader(frameHeader);
    const FbIpHeader *ipHeader = (FbIpHeader*)&payload[sizeOfEtherFrameHeader];
    unsigned int totalIpPacketLen = 0;
    unsigned int ipHeaderLength = 0;

    // Do we have a valid ip address?
    if (!get_local_ip(nif))
    {
        //PRINTK("Local IP address not set yet %d\n", nif->oob_channel_no);
        return true;
    }

    //PRINTK("Recv oob %d\n", nif->oob_channel_no);

    len -= sizeOfEtherFrameHeader;
    if (len < sizeof(FbIpHeader)) { PRINTK("Payload too small for IP header\n"); goto unknownPacket; }
    ipHeaderLength = ipHeader->headerLength * sizeof(uint32_t);
    if (len < ipHeaderLength) { PRINTK("Payload too small for IP header with options\n"); goto unknownPacket; }
    //if (ipHeader->headerLength * sizeof(uint32_t) != sizeof(FbIpHeader)) { PRINTK("Wrong IP header length\n", pDevExt->minor); goto unknownPacket; }
    if (FbCalcIpChecksum((uint8_t*)ipHeader, ipHeaderLength)) { PRINTK("IP header checksum failed\n"); goto unknownPacket; }
    totalIpPacketLen = htons(ipHeader->totalLength);
    //PRINTK("IP: Length %d, protocol: %d \n", totalIpPacketLen, ipHeader->protocol);
    if (len < totalIpPacketLen) { PRINTK("Payload too small for IP packet\n"); goto unknownPacket; }
    if (ipHeader->version != 4) { PRINTK("Unknown IP version: 0x%02x\n", (unsigned int)ipHeader->version); goto unknownPacket; }
    switch (ipHeader->protocol)
    {
        case 0x01: // ICMP; driver ping not supported anymore, use Linux ping instead.
            break;

        case 0x02: // IGMP
            {
                const FbIgmpHeader * igmpHeader = (FbIgmpHeader*)&payload[sizeOfEtherFrameHeader + ipHeaderLength];
                int16_t vlanTag = GetVLanTag(frameHeader);

                if (FbCalcIpChecksum((uint8_t*)igmpHeader, totalIpPacketLen - ipHeaderLength))
                {
                    PRINTK("IGMP checksum failed\n");
                    goto unknownPacket;
                }
                if (!nif->flags & FB_NIF_HANDLE_IGMP)
                {
                    PRINTK_C(LOG(LOG_IGMP), "Igmp not enabled for lane %u\n", nif->network_interface);
                    goto unknownPacket;
                }

                // See IGMP type numbers at https://www.iana.org/assignments/igmp-type-numbers/igmp-type-numbers.xhtml
                switch (igmpHeader->type)
                {
                    case 0x11: // Query v2 (Membership Query)
                        rc = query_received(&pDevExt->igmpContext, nif->network_interface, vlanTag, ntohl(igmpHeader->group), igmpHeader->maxRespCode);
                        return rc;

                    case 0x16: // Report v2 (Membership Report IGMPv2)
                    case 0x22: // Report v3 (Membership Report IGMPv3, header start is compatible with IGMPv2 )
                        rc = report_received(&pDevExt->igmpContext, nif->network_interface, vlanTag, ntohl(igmpHeader->group));
                        return rc;

                    case 0x17: // IGMPv2 Leave Group
                    default:
                        PRINTK_C(LOG(LOG_IGMP | LOG_DETAIL), "Unhandled IGMP packet of type %d (0x%x)\n", igmpHeader->type, igmpHeader->type);
                        break;
                }
            }
            break;

        case 0x06: // TCP
        case 0x11: // UDP
            break;

        default:
            PRINTK_ERR_C(LOG(LOG_NIC), "Unknown IP protocol: 0x%02x\n", (unsigned int)ipHeader->protocol); goto unknownPacket;
            break;
    }

unknownPacket:

    //FbDbgDumpMem(pDevExt, payload, len);
    return rc;
}

static void fbHandleArpPacket(sc_net_interface_t *nif, const uint8_t *payload, uint16_t len, int16_t vlanTag)
{
    const FbArpHeader * arpHeader = (FbArpHeader*)payload;
    device_context_t * pDevExt = nif->pDevExt;
    bool log = LOG(LOG_ARP);

    //FbDbgDumpMem(pDevExt, payload, len);

    if (len < sizeof(FbArpHeader))
    {
        PRINTK("Payload too small for arp header\n");
        return;
    }

    // Validate arp packet before trying to answer
    if (arpHeader->hwType != htons(0x0001) ||
        arpHeader->protocolType != htons(ETHER_TYPE_IPV4) ||
        arpHeader->hwAddrLen != 6 ||
        arpHeader->protocolAddrLen != 4 ||
        (arpHeader->operation != FB_ARP_REQUEST && arpHeader->operation != FB_ARP_REPLY))
    {
        PRINTK_C(unlikely(log || LOG(LOG_ERROR)), "Found invalid fields in ARP header. Ignoring packet\n");
        return;
    }

    // Do we have a valid ip address?
    if (get_local_ip(nif) == 0)
    {
        //PRINTK_C(unlikely(log || LOG(LOG_ERROR)), "ARP: Local IP address not set yet for lane %u\n", nif->lane);
        return;
    }

    /*if (LOG(log))
    {
        MACBUF(1); MACBUF(2); IPBUF(3); IPBUF(4);
        PRINTK("fbHandleArpPacket#5: phy=%u, nif->flags=%u==FB_NIF_HANDLE_ARP=%u\n"
                                ", arpHeader->operation=%u==FB_ARP_REPLY=%u\n"
                                ", arpHeader->targetHwAddr=%s==nif->mac_addr=%s\n"
                                ", arpHeader->targetProtocolAddr=%s==get_local_ip(nif)=%s\n",
               nif->physical_interface, nif->flags, FB_NIF_HANDLE_ARP,
               arpHeader->operation, FB_ARP_REPLY,
               LogMAC(1, arpHeader->targetHwAddr), LogMAC(2, nif->mac_addr),
               LogIP(3, htonl(arpHeader->targetProtocolAddr)), LogIP(4, get_local_ip(nif)));
    }*/
    // Is this a reply to our request from connect?
    if ((nif->flags & FB_NIF_HANDLE_ARP) &&
        arpHeader->operation == FB_ARP_REPLY &&
        memcmp(&arpHeader->targetHwAddr, &nif->mac_addr, MAC_ADDR_LEN) == 0 &&
        htonl(arpHeader->targetProtocolAddr) == get_local_ip(nif))
    {
        if (unlikely(log))
        {
            IPBUF(1); MACBUF(2);
            PRINTK("fbHandleArpPacket(): Found ARP reply for our request from connect, remote IP=%s, remote MAC=%s\n",
                LogIP(1, htonl(arpHeader->targetProtocolAddr)), LogMAC(2, arpHeader->senderHwAddr));
        }

        if (!pDevExt->arp_context.arp_handled_by_user_space)
        {
            // ARP reply is handled in driver
            arp_insert_mac_address(&pDevExt->arp_context, ntohl(arpHeader->senderProtocolAddr), arpHeader->senderHwAddr);
        }

        return;
    }
}

static bool fbParseOobPacket(sc_net_interface_t * nif, const sc_packet *oobPacket)
{
    bool rc = true;
    const device_context_t * pDevExt = nif->pDevExt;
    const FbEtherFrameHeader *frameHeader = (FbEtherFrameHeader*)sc_get_raw_payload(oobPacket);
    //    FbDbgDumpMem(pDevExt, (uint8_t*)oobPacket, oobPacket->len);
    int sizeOfEtherFrameHeader = GetSizeOfEthernetFrameHeader(frameHeader);
    if (sc_get_raw_payload_length(oobPacket) < sizeOfEtherFrameHeader)
    {
        PRINTK("fbParseOobPacket(): Skipping oob packet. Payload too small (%d), status %d\n", oobPacket->len, oobPacket->status);
        return true;
    }

    //PRINTK("fbParseOobPacket(): Ethertype 0x%04x\n", htons(frameHeader->etherType));
    switch (frameHeader->etherType)
    {
        case htons(ETHER_TYPE_IPV4):
            PRINTK_C(LOG(LOG_IPV4 | LOG_NIC), "fbParseOobPacket: .. found IPv4 packet\n");
            rc = fbHandleIpPacket(nif, oobPacket);
            break;
        case htons(ETHER_TYPE_ARP):
            PRINTK_C(LOG(LOG_ARP | LOG_NIC), "fbParseOobPacket: .. found ARP packet\n");
            {
                int16_t vlanTag = GetVLanTag(frameHeader);
                fbHandleArpPacket(nif, sc_get_raw_payload(oobPacket) + sizeOfEtherFrameHeader, sc_get_raw_payload_length(oobPacket) - sizeOfEtherFrameHeader, vlanTag);
            }
            break;
        case htons(ETHER_TYPE_IPV6):
            PRINTK_C(LOG(LOG_IPV6 | LOG_NIC), "fbParseOobPacket: .. found IPv6 packet\n");
            break;
        case htons(ETHER_TYPE_VLAN):
            PRINTK_C(LOG(LOG_VLAN | LOG_NIC), "fbParseOobPacket: .. found VLAN packet\n");
            break;
        case htons(ETHER_TYPE_LLDP):
            break;
        default:
            //PRINTK_ERR_C(LOGALL(LOG_UNKNOWN | LOG_NIC | LOG_DETAIL), "fbParseOobPacket: .. unknown ether type 0x%x\n", htons(frameHeader->etherType));
            break;
    }

    return rc;
}

#if 0
int fbCheckForIgmp(sc_net_interface_t * nif, fb_packet *oobPacket)
{
    int rc = 0;
    const uint8_t *payload = fb_get_raw_payload(oobPacket);
    uint16_t len = fb_get_raw_payload_length(oobPacket);
    const FbEtherFrameHeader *frameHeader = (FbEtherFrameHeader*)payload;
    int sizeOfEtherFrameHeader = GetSizeOfEtherentFrameHeader(frameHeader);
    const FbIpHeader *ipHeader = (FbIpHeader*)&payload[sizeOfEtherFrameHeader];
    unsigned int totalIpPacketLen = 0;
    unsigned int ipHeaderLength = 0;

    if (frameHeader->etherType == htons(IPV4_TYPE) && ipHeader->protocol == 0x02)
    {
        device_context_t * pDevExt = nif->pDevExt;
        len -= sizeOfEtherFrameHeader;
        if (len < sizeof(FbIpHeader)) { PRINTK("Payload too small for IP header\n", pDevExt->minor); goto unknownPacket; }
        ipHeaderLength = ipHeader->headerLength * sizeof(uint32_t);
        if (len < ipHeaderLength) { PRINTK("Payload too small for IP header with options\n", pDevExt->minor); goto unknownPacket; }
        //if (ipHeader->headerLength * sizeof(uint32_t) != sizeof(FbIpHeader)) { PRINTK("Wrong IP header length\n", pDevExt->minor); goto unknownPacket; }
        if (FbCalcIpChecksum((uint8_t*)ipHeader, ipHeaderLength)) { PRINTK("IP header checksum failed\n", pDevExt->minor); goto unknownPacket; }
        totalIpPacketLen = htons(ipHeader->totalLength);
        //DbgPrint("IP packet total length: %d\n", totalIpPacketLen);
        //PRINTK("IP: Length %d, protocol: %d \n", totalIpPacketLen, ipHeader->protocol);
        if (len < totalIpPacketLen) { PRINTK("Payload too small for IP packet\n", pDevExt->minor); goto unknownPacket; }
        if (ipHeader->version != 4) { PRINTK("Unknown IP version: 0x%02x\n", (unsigned int)ipHeader->version); goto unknownPacket; }
        return 1;
    }

unknownPacket:
    return rc;
}
#endif

static inline int is_multicast(const sc_packet* pkt)
{
    const uint8_t* pl = sc_get_raw_payload(pkt);
    return pl[0] & 1;  // bit 1 of byte 0 the destination MAC addr says if it is a multicast mac address
}

static inline int is_broadcast(const sc_packet* pkt)
{
    const uint8_t BroascastAddr[MAC_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    const uint8_t* dmac = sc_get_raw_payload(pkt);
    return memcmp(dmac, BroascastAddr, MAC_ADDR_LEN) == 0;
}

/* Takes care of filtering the packets given to the kernel, according to their MAC addresses.
   returns 1 if packet is to be filtered out, 0 otherwise */
static int mac_filtering(sc_net_interface_t *nif, const sc_packet* pkt)
{
    int filter_out = 0; // packet should be passed on

    if (!(nif->netdevice->flags & IFF_PROMISC) && !(nif->netdevice->flags & IFF_ALLMULTI))
    {
        if (is_multicast(pkt) && !is_broadcast(pkt))
        {
            int match = 0;
            const uint8_t* dmac = sc_get_raw_payload(pkt); // first 6 bytes are destination MAC

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
            struct dev_addr_list * cur = nif->netdevice->mc_list;
            while (cur)
            {
                const u8 *mac = cur->da_addr;
                if (memcmp(dmac, mac, MAC_ADDR_LEN) == 0)
                {
                    match = 1;
                    break;
                }

                cur = cur->next;
            }
#else
            struct netdev_hw_addr *cur;
            netdev_for_each_mc_addr(cur, nif->netdevice)
            {
                const u8 *mac = cur->addr;
                if (memcmp(dmac, mac, MAC_ADDR_LEN) == 0)
                {
                    match = 1;
                    break;
                }
            }
#endif
            if (!match)
                filter_out = 1; // packet should be filtered out

            //PRINTK_C(LOG(LOG_DEBUG), "packet got %s (MAC %02x:%02x:%02x:%02x:%02x:%02x)\n", nif->filter_out ? "filtered":"passedon", dmac[0], dmac[1], dmac[2], dmac[3], dmac[4], dmac[5]);
        }
    }

    return filter_out;
}

/* This functionn can only be used in the context of the napi poll, because it uses 'netif_receive_skb'
   instead of 'netif_rx' to deliver packet upwards */
static void fbNicReceive(sc_net_interface_t *nif, const sc_packet *oobPacket)
{
    const device_context_t * pDevExt = nif->pDevExt;
    fb_channel_t * pChannel = pDevExt->channel[nif->oob_channel_no];
    unsigned int len;
    struct sk_buff *skb;
    struct net_device *dev = nif->netdevice;
    int ret;

    // FIXME should this count as dropped?
    if (mac_filtering(nif, oobPacket))
        return;

    len = sc_get_raw_payload_length(oobPacket);
    skb = netdev_alloc_skb_ip_align(dev, len);
    if (!skb)
    {
        dev->stats.rx_dropped++;
        return;
    }

    skb_copy_to_linear_data(skb, sc_get_raw_payload(oobPacket), len);

    skb_put(skb, len);
    skb->protocol = eth_type_trans(skb, dev);
    skb_checksum_none_assert(skb);

    dev->stats.rx_packets++;
    dev->stats.rx_bytes += len;

    ret = netif_receive_skb(skb);
    if (ret != NET_RX_SUCCESS)
    {
        int64_t failCount = __sync_fetch_and_add(&pChannel->linux_drop_cnt, 1);
        if (failCount < 10 || failCount % 1000 == 0)
        {
            PRINTK_ERR_C(LOGALL(LOG_NIC | LOG_RX | LOG_DETAIL), "%s: netif_receive_skb has failed %lld times\n", __func__, failCount);
        }
    }
}

static bool PrintPlDMA(const char * what, const device_context_t * pDevExt, fb_channel_t * pChannel, const sc_pldma_t * pPlDma, sc_pldma_t * pPreviousPlDma)
{
    size_t i;
    bool changed = false;

    for (i = 0; i < number_of_elements(pPlDma->lastPacket); ++i)
    {
        if (pPlDma->lastPacket[i] != pPreviousPlDma->lastPacket[i] && i == pChannel->dma_channel_no)
        {
            PRINTK("PLDMA %3lu DMA %u %s: previous = 0x%09llX current = 0x%09llX, PRB va 0x%llX pa 0x%09llX PRB DMA start address 0x%09llX\n",
                i, pChannel->dma_channel_no, what != NULL ? what : "", pPreviousPlDma->lastPacket[i], pPlDma->lastPacket[i],
                (uint64_t)pChannel->recv, pChannel->recvDma, pChannel->prbFpgaStartAddress);

            pPreviousPlDma->lastPacket[i] = pPlDma->lastPacket[i];
            changed = true;
        }
    }

    return changed;
}

int fbProcessOobChannel(sc_net_interface_t * nif, int quota)
{
    device_context_t * pDevExt = nif->pDevExt;
    fb_channel_t * pChannel = pDevExt->channel[nif->oob_channel_no];
    int packetCount;
    const sc_packet * pCurrentPacket;

    pCurrentPacket = FbGetNextPacket(pChannel, pChannel->pCurrentPacket);

    if (LOG(LOG_MMU))
    {
        PrintPlDMA("before FbGetNextPacket", pDevExt, pChannel, pDevExt->plDmaVa, &pDevExt->previousPlDmaVa);
    }

    if (LOG(LOG_RX | LOG_DETAIL))
    {
        PrintPlDMA(" after FbGetNextPacket", pDevExt, pChannel, pDevExt->plDmaVa, &pDevExt->previousPlDmaVa);
        if (pCurrentPacket != NULL)
        {
            hexdump(pDevExt, "pCurrentPacket", pCurrentPacket, 128);
        }
    }

    packetCount = 0;
    while (pCurrentPacket != NULL && packetCount < quota)
    {
        bool processed;

        PRINTK_C(LOGALL(LOG_NIC | LOG_RX | LOG_DETAIL), " --- channel %u: received packet of length %u\n",
                 pChannel->dma_channel_no, pCurrentPacket->len);

        if (nif->netdevice) // This is a standard NIC (OOB)
        {
//-----------------------------------------------------------------------------------------
#include "ekaline_rx.c" // added by ekaline 
	  if (! eka_drop_me) // added by ekaline (used to drop all UDP packets)
//-----------------------------------------------------------------------------------------    
            fbNicReceive(nif, pCurrentPacket); // Pass packet upwards into Linux network stack
        }

        // Give the driver also a chance to see the packet
        processed = fbParseOobPacket(nif, pCurrentPacket);
        if (!processed)
        {
            // For some reason the packet was not properly processed.
            if (++nif->number_of_not_processed_packets < 5)
            {
                // Do not process packets beyond this one until
                // we have retried.
                // Reschedule napi call to retry this same packet another time.
                fb_schedule_napi_dev(pDevExt, nif->network_interface);
                break;
            }
            else
            {
                nif->number_of_not_processed_packets = 0;
                PRINTK_ERR("Failed to process packet after retries!\n");

                // Give up, go to next packet!
                processed = true;
            }
        }

        if (processed)
        {
            packetCount++;

            pChannel->pCurrentPacket = pCurrentPacket;
            pCurrentPacket = FbGetNextPacket(pChannel, pCurrentPacket);
        }
    }


    //{ static volatile int count = 0; if (++count && packetCount > 0) PRINTK("%d packets received %d\n", packetCount, count); }

    // if something was read, advance the read mark
    if (packetCount > 0 && likely(pChannel->pCurrentPacket))
    {
        set_rx_dma_read_mark(pDevExt, nif->oob_channel_no, (uint8_t*)pChannel->pCurrentPacket);

    }

    if (packetCount != 0)
    {
        int64_t totalCount = __sync_fetch_and_add(&pChannel->packet_cnt, packetCount);
        PRINTK_C(LOGALL(LOG_NIC | LOG_RX | LOG_DETAIL), " --- channel %u: %d packets total %lld received\n",
                 pChannel->dma_channel_no, packetCount, totalCount);
    }
    return packetCount;
}

void FbDbgDumpMem(device_context_t * pDevExt, const uint8_t * buf, unsigned int len)
{
    const unsigned int n = (len + 7) / 8;
    unsigned int i = 0;

    for (; i < n; ++i)
    {
        PRINTK("%04x: %02x %02x %02x %02x %02x %02x %02x %02x\n", i * 8,
            (unsigned int)buf[i * 8 + 0],
            (unsigned int)buf[i * 8 + 1],
            (unsigned int)buf[i * 8 + 2],
            (unsigned int)buf[i * 8 + 3],
            (unsigned int)buf[i * 8 + 4],
            (unsigned int)buf[i * 8 + 5],
            (unsigned int)buf[i * 8 + 6],
            (unsigned int)buf[i * 8 + 7]);
    }
    buf = &buf[n * 8];

    switch (len % 8)
    {
        case 0: break;
        case 1: PRINTK("%04x: %02x\n",  n * 8, (unsigned int)buf[0]); break;
        case 2: PRINTK("%04x: %02x %02x\n", n * 8, (unsigned int)buf[0], (unsigned int)buf[1]); break;
        case 3: PRINTK("%04x: %02x %02x %02x\n", n * 8, (unsigned int)buf[0], (unsigned int)buf[1], (unsigned int)buf[2]); break;
        case 4: PRINTK("%04x: %02x %02x %02x %02x\n", n * 8, (unsigned int)buf[0], (unsigned int)buf[1], (unsigned int)buf[2], (unsigned int)buf[3]); break;
        case 5: PRINTK("%04x: %02x %02x %02x %02x %02x\n", n * 8, (unsigned int)buf[0], (unsigned int)buf[1], (unsigned int)buf[2], (unsigned int)buf[3], (unsigned int)buf[4]); break;
        case 6: PRINTK("%04x: %02x %02x %02x %02x %02x %02x\n", n * 8, (unsigned int)buf[0], (unsigned int)buf[1], (unsigned int)buf[2], (unsigned int)buf[3], (unsigned int)buf[4], (unsigned int)buf[5]); break;
        case 7: PRINTK("%04x: %02x %02x %02x %02x %02x %02x %02x\n", n * 8, (unsigned int)buf[0], (unsigned int)buf[1], (unsigned int)buf[2], (unsigned int)buf[3], (unsigned int)buf[4], (unsigned int)buf[5], (unsigned int)buf[6]); break;
    }
}
