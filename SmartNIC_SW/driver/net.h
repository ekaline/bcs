#ifndef NET_H
#define NET_H

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

#if 0
#define htons(A)    ((((uint16_t)(A) & 0xff00) >> 8) | \
                    (((uint16_t)(A) & 0x00ff) << 8 ))
#define htonl(A)    ((((uint32_t)(A) & 0xff000000) >> 24) | \
                    (((uint32_t)(A) & 0x00ff0000) >> 8)  | \
                    (((uint32_t)(A) & 0x0000ff00) << 8)  | \
                    (((uint32_t)(A) & 0x000000ff) << 24))
#endif

typedef enum
{
    ETHER_TYPE_IPV4     = 0x0800,
    ETHER_TYPE_ARP      = 0x0806,
    ETHER_TYPE_VLAN     = 0x8100,
    ETHER_TYPE_IPV6     = 0x86DD,
    ETHER_TYPE_LLDP     = 0x88CC,
} ether_type_t;

#define FB_ARP_REQUEST htons(0x0001)
#define FB_ARP_REPLY htons(0x0002)
#define MAC_ADDR_LEN 6
#define MIN_FRAME_SIZE 0x40
#define FB_ARP_RETRY_COUNT 5
#define FB_ARP_TIMEOUT 10 // Timer runs @ 100ms

#define FB_NIF_HANDLE_IGMP 0x0001U      // Internet Group Management Protocol (to establish multicast group memberships)
#define FB_NIF_HANDLE_ARP  0x0002U      // Address Resolution Protocol

typedef uint8_t FbMacAddr[MAC_ADDR_LEN];
#include "eka.h" // added by ekaline

/**
 *  Describes local net properties per lane (sometimes also called physical port).
 */
typedef struct
{
    uint32_t local_ip;                      /**< Local IP address for this net interface */
    uint16_t local_port;                    /**< Local IP port for this net interface */
    uint16_t network_interface;             /**< Network interface used by this net interface */
    uint32_t netmask;                       /**< Net mask of this net interface. */
    uint32_t gateway;                       /**< IP gateway for this net interface */
    uint8_t  mac_addr[MAC_ADDR_LEN];        /**< MAC address of this net interface. */
    uint32_t flags;                         /**< Used internally to signal FB_NIF_HANDLE_ARP and/or FB_NIF_HANDLE_IGMP */
    unsigned int oob_channel_no;
    device_context_t * pDevExt;            /**< Pointer to owner device context */
    struct net_device * netdevice;          /**< Linux net device, see http://elixir.free-electrons.com/linux/v4.14.15/source/include/linux/netdevice.h#L1420 */
    bool carrier_is_on;                     /**< Link status (carrier in old ethernet nomenclature) is on or off. */
    uint16_t number_of_not_processed_packets;

  eka_nif_state_t *eka_private_data; // added by ekaline

} sc_net_interface_t;

#pragma pack(push, 1)

typedef struct
{
    FbMacAddr       destinationMacAddr;
    FbMacAddr       sourceMacAddr;
    uint16_t        etherType;

    // Optional VLAN fields
    uint16_t        pcp_dei_vid;        ///< PCP 3 bits, DEI 1 bit, VID (VLAN tag) 12 bits
    uint16_t        actualEtherType;    ///< Actual payload ether type when above "etherType" field is equal to ETHER_TYPE_VLAN
} FbEtherFrameHeader;

inline static int GetSizeOfEthernetFrameHeader(const FbEtherFrameHeader * pEthernetFrameHeader)
{
    if (pEthernetFrameHeader->etherType == ETHER_TYPE_VLAN)
    {
        return 18;
    }
    return 14;
}

inline static uint16_t GetEtherTypeOfEthernetFrame(const FbEtherFrameHeader * pEthernetFrameHeader)
{
    if (pEthernetFrameHeader->etherType == ETHER_TYPE_VLAN)
    {
        return pEthernetFrameHeader->actualEtherType;
    }
    return pEthernetFrameHeader->etherType;
}

inline static int16_t GetVLanTag(const FbEtherFrameHeader * pEthernetFrameHeader)
{
    if (pEthernetFrameHeader->etherType == ETHER_TYPE_VLAN)
    {
        return pEthernetFrameHeader->pcp_dei_vid & 0x0FFF;
    }
    return NO_VLAN_TAG;
}

/**
 *  ARP header.
 *
 *  See https://en.wikipedia.org/wiki/Address_Resolution_Protocol
 */
typedef struct
{
    uint16_t    hwType;                 // Hardware type (HTYPE)
    uint16_t    protocolType;           // Protocol type (PTYPE)
    uint8_t     hwAddrLen;              // Hardware address length (HLEN)
    uint8_t     protocolAddrLen;        // Protocol address length (PLEN)
    uint16_t    operation;              // Operation (OPER)
    FbMacAddr   senderHwAddr;           // Sender hardware address (SHA)
    uint32_t    senderProtocolAddr;     // Sender protocol address (SPA)
    FbMacAddr   targetHwAddr;           // Target hardware address (THA)
    uint32_t    targetProtocolAddr;     // Target protocol address (TPA)
} FbArpHeader;

typedef struct
{
    uint16_t  headerLength                    : 4;
    uint16_t  version                         : 4;
    uint16_t  explicitCongestionNotification  : 2;
    uint16_t  differentiatedServicesCodePoint : 6;
    uint16_t  totalLength;
    uint16_t  identification;
    uint16_t  w2;
    uint8_t   ttl;
    uint8_t   protocol;
    uint16_t  headerChecksum;
    uint32_t  srcIpAddr;
    uint32_t  dstIpAddr;
    uint32_t  options[];
} FbIpHeader;

typedef struct
{
    uint8_t   type;
    uint8_t   code;
    uint16_t  checksum;
    uint16_t  identifier;
    uint16_t  sequenceNo;
    uint8_t   payload[];
} FbIcmpHeader;

/**
 *  IGMPv2 header.
 */
typedef struct
{
    uint8_t   type;
    uint8_t   maxRespCode;  /* This field specifies the maximum time (in 1/10 second) allowed before sending a responding report.
                            If the number is below 128, the value is used directly. If the value is 128 or more,
                            it is interpreted as an exponent and mantissa. */
    uint16_t  checksum;
    uint32_t  group;
} FbIgmpHeader;

#pragma pack(pop)

const char * send_skb_reason(int rc);

int sendIgmpLeave(const sc_net_interface_t *nif, uint32_t srcIp, uint32_t group, int16_t vlanTag);
int sendIgmpReport(const sc_net_interface_t *nif, uint32_t srcIp, uint32_t dstGroup, int16_t vlanTag);
int sendArpRequest(const sc_net_interface_t *nif, uint32_t destinationIp, int16_t vlanTag);

void FbDbgDumpMacAddr(const device_context_t * pDevExt, const char *header, const FbMacAddr macAddr);
void FbDbgDumpIpAddr(const device_context_t * pDevExt, const char *header, uint32_t ipAddr);
uint16_t FbCalcIpChecksum(const uint8_t *buf, uint32_t len);

void multicast_to_mac(uint32_t addr, char mac[MAC_ADDR_LEN]);

/* Compute the IP to resolve given the destination IP and the interface.
 * If the destination IP is inside the local network, the MAC address to
 * retrieve is the dest IP.
 * Otherwise, it is the MAC address of the gateway to the outside of
 * the local network */
static inline uint32_t
compute_arp_ip(uint32_t ip, uint32_t local_ip_address, uint32_t gateway, uint32_t netmask)
{
    uint32_t arp_ip;

//printk("compute_arp_ip: ip = 0x%08x, local_ip_address = 0x%08x, gateway = 0x%08x, netmask = 0x%08x\n", ip, local_ip_address, gateway, netmask);
    if ((local_ip_address & netmask) == (ip & netmask))
    {
        arp_ip = ip;
    }
    else
    {
        arp_ip = gateway;
    }

    return arp_ip;
}

#endif /* NET_H */
