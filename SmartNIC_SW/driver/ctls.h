#ifndef CTLS_H
#define CTLS_H

/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 *or without modification, are permitted provided that the
 *following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *copyright notice, this list of conditions and the
 *following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the
 *above copyright notice, this list of conditions and the
 *following disclaimer in the documentation and/or other
 *materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products
 *derived from this software without specific prior written
 *permission.
 *
 * 4. This software may only be redistributed and used in
 *connection with a Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/**
 * @file ctls.h
 *
 * @brief Shared data structures.
 */

/// @cond PRIVATE_CODE
// These data structure and defintions are internal and are
// therefore not documented.

#ifdef __cplusplus
extern "C" {
#endif

//

#ifdef SC_CTLS_H_INCLUDED_FROM__DRIVER
// ctls.h included in the driver
#include <linux/types.h>
#include <asm/page_types.h> // Definition of PAGE_SIZE
#include <linux/time.h>
#else
// ctls.h included in user mode code
#include <stdint.h>
#endif

#ifndef nameof
#define nameof(x) #x
#endif

#ifndef PAGE_SIZE // Define PAGE_SIZE explicitly if not
                  // already defined in a system header.
#define PAGE_SIZE 4096 // Size of memory pages
#endif

#include "../driver/regs.h"
// #include PRODUCT_COMMON_H commented by ekaline
#include "smartnic_common.h"

#define SC_INVALID_PLDMA_ADDRESS                           \
  0 /**< Invalid PL DMA address */

// Make sure the compiler does not add its own padding to
// our data structures
#pragma pack(push, 1)

// FIXME SC_NO_NETWORK_INTERFACES should not be a constant
// but a field of device_context_t
#define SC_NO_NETWORK_INTERFACES 8

/**
 * @addtogroup PacketTransmission
 * @{
 */
/**
 * @{
 * @name Transmit bucket definitions.
 */

#define SC_MAX_TX_DMA_FIFO_ENTRIES                         \
  512 /**< Maximum number of entries in the Tx DMA FIFO.   \
       */
#define SC_NO_BUCKETS                                      \
  64 /**< Number of available transmit buckets per         \
        channel. */

/// @} Transmit bucket definitions.

#define FB_FIFO_ENTRIES_PER_CHANNEL                        \
  4 ///< // Number of Tx FIFO entries to reserve for
    ///< channels used inside the driver (512 / 128)
#define SC_BUCKET_SIZE                                     \
  (PAGE_SIZE /                                             \
   2) ///< Maximum size of a transmit bucket (2048 bytes).
#define SC_MAX_MTU_SIZE                                    \
  (SC_BUCKET_SIZE -                                        \
   8) /**< Maximum MTU size is bucket size - 8 for single  \
         packet send acknowledgement. */

// !!! Important Please change DMA_CHANNEL_CANARY when
// updating the DMA channels layout!!!
#define SC_MAX_ULOGIC_CHANNELS 8  // Rx/Tx
#define SC_MAX_OOB_CHANNELS 8     // Standard NIC (Rx/Tx)
#define SC_MAX_UNUSED_CHANNELS 8  /**< not used */
#define SC_MAX_MONITOR_CHANNELS 8 /**< Rx only */
#define SC_MAX_UDP_CHANNELS                                \
  32 /**< Maximum number of UDP channels, Rx only, 64      \
        feeds (multicast subscribtions) */
#define SC_MAX_TCP_CHANNELS                                \
  64 /**< Maximum number of ETLM TCP channels, , Rx and Tx \
        supported */

#define SC_MAX_CHANNELS                                    \
  128 /**< Maximum number of DMA channels. */

#define FIRST_UL_CHANNEL (0) // ULOGIC:  channels 0-7
#define FIRST_OOB_CHANNEL                                  \
  (FIRST_UL_CHANNEL +                                      \
   SC_MAX_ULOGIC_CHANNELS) // OOB:     channels 8-15
#define DMA_CHANNEL_CANARY                                 \
  (SC_MAX_ULOGIC_CHANNELS + SC_MAX_OOB_CHANNELS +          \
   SC_MAX_UNUSED_CHANNELS + SC_MAX_MON_CHANNELS +          \
   SC_MAX_UDP_CHANNELS + SC_MAX_TCP_CHANNELS)

// !!! Important Please change DMA_CHANNEL_CANARY when
// updating the DMA channels layout!!!
#define FIRST_UNUSED_CHANNEL                               \
  (FIRST_OOB_CHANNEL +                                     \
   SC_MAX_OOB_CHANNELS) // unused:  channels 16-23
#define FIRST_MONITOR_CHANNEL                              \
  (FIRST_UNUSED_CHANNEL +                                  \
   SC_MAX_UNUSED_CHANNELS) // MON:     channels 24-31
#define FIRST_UDP_CHANNEL                                  \
  (FIRST_MONITOR_CHANNEL +                                 \
   SC_MAX_MONITOR_CHANNELS) // UDP:     channels 32-63
#define FIRST_TCP_CHANNEL                                  \
  (FIRST_UDP_CHANNEL +                                     \
   SC_MAX_UDP_CHANNELS) // TCP:     channels 64-127

// FIXME StatusDma beeing broken. we will get the TCP status
// through IOCTl instead (slower)
#define SC_GET_TCP_STATUS_BY_IOCTL ///< Use IO Control call
                                   ///< to get TCP status

// ulogic + oob + unused + mon + udp + tcp (in that order!)

#define SC_MAX_NUMBER_OF_NETWORK_INTERFACES                \
  8 /**< Maximum number of network interfaces that this    \
       API can handle. */

/* PL DMA groups */
#define SC_NUMBER_OF_PLDMA_GROUPS 8
#define SC_NUMBER_OF_DMA_CHANNELS_PER_PLDMA_GROUP          \
  (SC_MAX_CHANNELS / SC_NUMBER_OF_PLDMA_GROUPS)

// Mapping regions to use in mmap
// BAR 0:
#define BAR0_REGS_OFFSET                                   \
  0x0 // BAR 0 REGS:  [0x000000, 0x100000)
#define BAR0_REGS_SIZE 0x100000
/* DMAs: */
#define PL_DMA_OFFSET                                      \
  (BAR0_REGS_OFFSET +                                      \
   BAR0_REGS_SIZE) // PL DMA:      [0x100000, 0x101000)
                   // PL_DMA_OFFSET     = 0x100000
#define PL_DMA_SIZE                                        \
  PAGE_SIZE //                                              PL_DMA_SIZE       = 0x1000 (4096)
#define STATUS_DMA_OFFSET                                  \
  (PL_DMA_OFFSET +                                         \
   PL_DMA_SIZE) // STATUS DMA:  [0x101000, 0x102000)
                // STATUS_DMA_OFFSET = 0x101000
#define STATUS_DMA_SIZE                                    \
  PAGE_SIZE //                                              STATUS_DMA_SIZE   = 0x1000 (4096)
/* PCIe memory: */
#define BUCKETS_OFFSET                                     \
  (STATUS_DMA_OFFSET +                                     \
   STATUS_DMA_SIZE) // BUCKETS:     [0x102000, 0x122000)
                    // BUCKETS_OFFSET    = 0x102000
#define BUCKETS_SIZE                                       \
  (SC_BUCKET_SIZE *                                        \
   SC_NO_BUCKETS) //                                              BUCKETS_SIZE      = 0x20000
#define RECV_DMA_OFFSET                                    \
  (BUCKETS_OFFSET +                                        \
   BUCKETS_SIZE) // RECV DMA 0:  [0x122000, 0x522000)
                 // RECV_DMA_OFFSET   = 0x122000 PRBs:
                 // [0x122000, 10122000) PacketMover PRBs:
                 // [0x122000, 20122000) SmartNIC

// This is normally the maximum size supported by Linux
// pci_alloc_consistent call. To use bigger sizes one has to
// use huge pages or add an MMU to the FPGA or possibly try
// to allocate as much continuous physical memory as
// possible at boot/load time. None of these strategies are
// currently implemented.
#define RECV_DMA_SIZE                                      \
  (4 * 1024 *                                              \
   1024) ///< Size of Rx DMA receive packet ring buffer
         ///< (PRB).  RECV_DMA_SIZE     = 0x400000
#define RECV_DMA_TOTAL_SIZE                                \
  (RECV_DMA_SIZE *                                         \
   SC_MAX_CHANNELS) // 0x10000000 (256 MB) for PacketMover;
                    // 0x20000000 (512 MB) for SmartNIC

#define MMAP_FULL_SIZE                                     \
  (RECV_DMA_OFFSET +                                       \
   RECV_DMA_SIZE) // Total mapped memory size: 0x522000
#define REGS_FULL_SIZE                                     \
  (BAR0_REGS_SIZE + PL_DMA_SIZE +                          \
   STATUS_DMA_SIZE) // Total mapped registers
                    // size: 0x102000

#define EXTERNAL_DMA_DEMO_MEMORY_SIZE                      \
  (4 * 1024 * 1024) // 0x400000
#define EXTERNAL_DMA_DEMO_MEMORY_OFFSET                    \
  (RECV_DMA_OFFSET +                                       \
   RECV_DMA_TOTAL_SIZE) // Externel DMA: [0x522000,
                        // 0x922000)

// BAR 2:
#define BAR2_REGS_OFFSET                                   \
  0x0 // BAR 2 REGS:        [0x000000, 0x100000)
#define BAR2_REGS_SIZE 0x100000
// fixed by ekaline
/* #define EKA_WC_REGION_SIZE      0x10000 */
/* #define BAR2_REGS_SIZE          (0x100000 -
 * EKA_WC_REGION_SIZE) */

// Offset in bytes to bucket 'n' from start of mapped area
#define BUCKET_OFFSET(n)                                   \
  (BUCKETS_OFFSET + ((n) * SC_BUCKET_SIZE))

#define MAX_FIFO_ENTRIES                                   \
  512 ///< Size of the TxDMA send register FIFO in the FPGA.

#define FIND_FREE_DMA_CHANNEL -1
#define NO_VLAN_TAG -1
#define SC_MAGIC_SIGNAL_INT 0xDEADBABE

/* mmap/sc_mmap offsets */
enum { SC_MMAP_ALLOCATE_PRB_OFFSET = 0x70 };

/**
 *  FPGA revision data.
 */
typedef union {
  uint64_t raw_rev; ///< Raw 64 bit revision data
  struct {
    uint8_t build; ///< Build number
    uint8_t sub;   ///< Sub-revision number
    uint8_t minor; ///< Minor revision number
    uint8_t major; ///< Major revision number
    uint8_t type;  ///< FPGA image type number
    uint8_t model; ///< FPGA model
    uint16_t
        pldma : 1; ///< Bit 0 (bit 48 of raw_rev): set if
                   ///< packet DMA uses pointer list or
                   ///< reset if linked list is used. This
                   ///< bit is always set in the FPGA.
    uint16_t
        extended_register : 1; ///< Bit 1 (bit 49 of
                               ///< raw_rev): set when
                               ///< extended register mode
                               ///< is available. In this
                               ///< mode there are multiple
                               ///< sub registers. Always
                               ///< set in the FPGA.
    uint16_t reserved1 : 1;    /**< Bit 50 */
    bool bulk_supported : 1;   /**< Bit 51: bulk receive
                                  supported */
    uint16_t reserved2 : 1;    /**< Bit 52 */
    bool mmu : 1;              /**< Bit 53: MMU */
    uint16_t reserved3 : 11; /**< 11 bits or bits 54 - 63 of
                                raw_rev */
  } parsed;                  ///< Parsed revision number

} fpga_revision_t;

/// Transmit bucket data structure
typedef struct {
  uint8_t data[SC_BUCKET_SIZE]; ///< Data to be transmitted
} sc_bucket;

/// @} PacketTransmission

/**
 * @addtogroup PacketReceptionRaw
 * @{
 */

/** Incoming packet data structure.
 *
 * @note @ref sc_packet.offset_ip "Offsets" are number of
 * double words.
 *                  TODO: use union instead
 *
 *  NB. Packets are aligned at 64 byte boundary in PRB.
 */
typedef struct __SC_Packet__ {
  uint16_t len;        ///< Length of payload
  uint16_t status;     ///< Packet status.
  uint32_t ts_seconds; ///< Timestamp, whole seconds part.
  uint32_t ts_nanoseconds; ///< Timestamp, hardcoded as
                           ///< nanoseconds.
  uint8_t offset_ip;       ///< Offset to IP part of packet.
  uint8_t
      offset_layer_4; ///< Offset to layer 4 part of packet.
  uint8_t
      offset_layer_5; ///< Offset to layer 5 part of packet.
  uint8_t padding2;   ///< Not used
  uint16_t layer_5_len; ///< Length of TCP or UDP payload.
                        ///< For other packet types, this
                        ///< field is undefined.

#if defined(STRICT) && (STRICT == 1) && defined(__GNUC__)
#pragma GCC diagnostic push
// For pkt_payload[] ignore warning: ISO C++ forbids
// zero-size array 'pkt_payload' [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
  uint8_t pkt_payload[]; /**< Packet payload including layer
                            2 and upwards. */
#if defined(STRICT) && (STRICT == 1) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
} sc_packet;

/** Get pointer to TCP payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint8_t* Pointer to payload
 */
static inline const uint8_t *
sc_get_tcp_payload(const sc_packet *pkt) {
  return pkt->pkt_payload + pkt->offset_layer_5 * 4 -
         2; // TODO: Remove " + pkt->offset_layer_5 * 4"?
            // offset_layer_5 should always be 0
}

/** Get pointer to UDP payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint8_t* Pointer to payload
 */
static inline const uint8_t *
sc_get_udp_payload(const sc_packet *pkt) {
  return pkt->pkt_payload + pkt->offset_layer_5 * 4 - 2;
}

/** Get length of TCP payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint16_t Length of payload
 */
static inline uint16_t
sc_get_tcp_payload_length(const sc_packet *pkt) {
  return pkt->len;
}

/** Get length of UDP payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint16_t Length of payload
 */
static inline uint16_t
sc_get_udp_payload_length(const sc_packet *pkt) {
  return pkt->layer_5_len;
}

/** Get pointer to raw payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint8_t* Pointer to payload
 */
static inline const uint8_t *
sc_get_raw_payload(const sc_packet *pkt) {
  return pkt->pkt_payload;
}

/** Get pointer to user logic payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint8_t* Pointer to payload
 */
static inline const uint8_t *
sc_get_ulogic_payload(const sc_packet *pkt) {
  return pkt->pkt_payload - 2;
}

/** Get length of raw payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint16_t Length of payload
 */
static inline uint16_t
sc_get_raw_payload_length(const sc_packet *pkt) {
  return SC_CAST(uint16_t, pkt->len - 2);
}

/** Get length of user logic payload
 *
 *  @param pkt Pointer to packet
 *
 *  @return uint16_t Length of payload
 */
static inline uint16_t
sc_get_ulogic_payload_length(const sc_packet *pkt) {
  return SC_CAST(uint16_t,
                 pkt->len); /* Packet length field is
                               payload length. */
}

/**
 * @{
 * @name Packet status flags.
 * @brief Bit mask definitions for the @ref
 * sc_packet_t.status field.
 */
#define SC_PKT_STATUS_END_OF_BUFFER                        \
  (1U << 12) ///< Packet is at end of buffer - next packet
             ///< will be at start.

/// @}

/// @} PacketReceptionRaw

/**
 * @addtogroup SystemInfo
 * @{
 */

// se paa
// /home/ele/projects/capture_linux/fbcapture_common/fbinternalstatistics.h

/**
 *  Statistics for a single network interface. This struct
 * is a copy from capture because capture uses the same
 * statistics module.
 */
typedef struct {
  uint32_t okFrames;    ///< Number of OK frames (includes
  uint32_t errorFrames; ///< Rx: number of corrupted frames;
                        ///< Tx: number of frames sent to
                        ///< the MAC with an instruction to
                        ///< invalidate the packet
  uint32_t
      size_1_63; ///< Frames with length less than 64 bytes
  uint32_t
      size_64; ///< Frames with length equal to 64 bytes
  uint32_t size_65_127;  ///< Frames with length between 65
                         ///< and 127 bytes
  uint32_t size_128_255; ///< Frames with length between 128
                         ///< and 255 bytes
  uint32_t size_256_511; ///< Frames with length between 256
                         ///< and 511 bytes
  uint32_t size_512_1023;  ///< Frames with length between
                           ///< 512 and 1023 bytes
  uint32_t size_1024_1518; ///< Frames with length between
                           ///< 1024 and 1518 bytes
  uint32_t size_1519_2047;
  uint32_t size_2048_4095;
  uint32_t size_4096_8191;
  uint32_t size_8192_9022;
  uint32_t size_above_9022;
  uint64_t okBytes;    ///< Number of OK bytes
  uint64_t errorBytes; ///< Number of error bytes
  uint32_t broadcast;
  uint32_t multicast;
  uint32_t
      runts; ///< length < 64 and FCS errors (not available)
  uint32_t
      jabbers; ///< FCS errors, length >= configurable value
               ///< between [128-10000] (not available)
  uint32_t oversize;
  uint32_t trunc;
  uint32_t discarded_undersize;
  uint32_t
      filter_drops; /**< This is actually overflow_frames
                       and works only for NIC/OOB */
  uint32_t overflow_frames;
} sc_stat_nif_t; // Size 27 DW = 108 bytes

#define SC_STAT_CHANNELS_PER_GROUP                         \
  8 /**< Statistics lanes in a group */

/**
 *  Lane Rx and Tx statistics.
 */
typedef struct {
  sc_stat_nif_t
      rx_nifs[SC_STAT_CHANNELS_PER_GROUP]; /**< Rx data */
  uint32_t reserved1[40]; /**< Reserved for future use:
                             0x104 - 0xdd + 1 = 40 DW */
  sc_stat_nif_t
      tx_nifs[SC_STAT_CHANNELS_PER_GROUP]; /**< Tx data */
  /**< Reserved for future use: 0x204 - 0x1dd + 1 = 40 DW */
  uint32_t reserved2[40]; /**< Reserved for future use */
} sc_stat_lane_group_8_t; // Size 512 DW = 2048 bytes

/**
 *  Rx/Tx statistics DMA packet payload.
 */
typedef struct {
  // Size include timestamp 5 DW
  uint32_t timestamp_seconds; /**< Timestamp seconds */
  uint32_t
      timestamp_nanoseconds; /**< Timestamp nanoseconds */

  sc_stat_lane_group_8_t
      nif_groups[2]; /**< The first 8 lanes are in
                        lane_groups[0], the next 8 lanes are
                        in lane_groups[1] */

} sc_stat_packet_payload_t;

/**
 *  System information about all devices.
 */
typedef struct {
  enum {
    SC_SYSTEM_INFO_NONE,
    SC_SYSTEM_INFO_GET,
    SC_SYSTEM_INFO_GET_NIF
  } cmd; /**< What system information is requested. */
         // union // See
  // https://rkrishnan.org/posts/2013-11-13-kernel-abi-breakage.html
  // for explanation for not using unions in ioctl structs!
  //{
  uint16_t
      number_of_cards_found; /**< Total number of cards
                                found not including cards
                                with test FPGA images */
  uint16_t
      number_of_test_fpga_cards_found; /**< Total number of
                                          cards found with
                                          test FPGA images
                                        */
  struct {
    uint8_t nif;
    char nif_name[100];
    uint8_t mac_address[SC_MAC_ADDRESS_LENGTH];
    uint32_t ip_address;
    uint32_t ip_mask;
    uint32_t max_speed;
  } nif_info;
  //};
} sc_system_info_t;

/**
 *  FPGA register information.
 */
typedef struct {
  uint16_t pcie_bar;
  uint16_t register_id;
  uint64_t offset_in_pcie_bar;
  uint64_t physical_address;
  void *kernel_virtual_address;
  uint8_t is_readable;
  uint8_t is_writable;
} sc_register_info_t;

typedef struct {
  uint64_t length;
  uint64_t kernel_virtual_address;
  uint64_t user_space_virtual_address;
  uint64_t physical_address;
  uint64_t fpga_start_address;

} sc_memory_t;

/**
 *  Information related to external DMA settings.
 */
typedef struct {
  enum {
    SC_DMA_INFO_NONE = 0,
    SC_DMA_INFO_GET = 1,
    SC_RX_DMA_START = 2,
    SC_RX_DMA_STOP = 3,
    SC_SET_PL_DMA_GROUP_OPTIONS = 4,
    SC_GET_REGISTER_INFO = 5,
    SC_GET_CHANNEL_INFO = 6,
    SC_DMA_INFO_START_PL_DMA = 7,
    SC_DMA_INFO_STOP_PL_DMA = 8
  } cmd;
  int16_t dma_channel;
  uint16_t group;

  /* Tx DMA settings */
  uint64_t bar0_physical_address;
  void *bar0_kernel_virtual_address;
  sc_register_info_t user_logic_tx_send_register;

  /* Rx DMA settings */
  uint64_t prb_physical_start_address;
  uint64_t prb_length;
  uint8_t dma_mode;
  uint8_t bulk_enable;
  int8_t bulk_flush_size;
  uint8_t bulk_flush_timeout;
  uint32_t bulk_flush_packet_count;

  /* Rx PL DMA group settings */
  uint32_t pl_dma_flush_time_in_ns;
  uint32_t pl_dma_flush_packet_count;
  uint64_t pl_dma_group_physical_address;
  uint64_t pl_dma_group_offset;

  // union // See
  // https://rkrishnan.org/posts/2013-11-13-kernel-abi-breakage.html
  // for explanation for not using unions in ioctl structs!
  //{
  sc_register_info_t register_info; /* Register info */
  sc_memory_t prb_memory_info;
  //};

} sc_dma_info_t;

typedef struct {
  enum {
    SC_MEMORY_NONE = 0,
    SC_MEMORY_ALLOC = 1,
    SC_MEMORY_FREE = 2
  } cmd;
  uint64_t length;
  uint64_t kerneL_virtual_address;
  uint64_t physical_address;

} sc_memory_alloc_free_t;

typedef struct {
  enum reference_count_which_t {
    SC_REFERENCE_COUNT_USER_LOGIC_DEMO = 0,
    SC_REFERENCE_COUNT_MAX = 1
  } which;
  enum {
    SC_REFERENCE_COUNT_INCREMENT = 1,
    SC_REFERENCE_COUNT_DECREMENT = 2
  } what;
  int64_t reference_count;

} sc_reference_count_t;

/// @} SystemInfo

#define SC_STAT_MAX_NUMBER_OF_NETWORK_INTERFACES 2

/**
 * @addtogroup TCPConnections
 * @{
 */

/**
 *  Connection info needed to establish a TCP connection via
 * an ioctl call.
 */
typedef struct {
  uint32_t
      remote_ip; ///< Remote IP address in host byte order
  uint16_t remote_port; ///< Remope IP port number.
  uint32_t
      local_ip_address; ///< Optional per connection local
                        ///< IP address in host byte order.
  uint16_t
      local_port; ///< Optional local port number used by
                  ///< connection, either selected by user
                  ///< or automatically selected.
  uint8_t remote_mac_address
      [MAC_ADDR_LEN]; ///< Optional remote MAC address.
                      ///< Resolved with ARP if not
                      ///< available.
  int16_t vlan_tag;   ///< VLAN tag 1-4094 or -1 if VLAN is
                    ///< not used. (VLAN tags 0 and 4095 are
                    ///< reserved!)
  int32_t timeout; ///< Connection established wait timeout
                   ///< in milliseconds
} sc_connection_info_t;

typedef struct {
  uint32_t
      timeout; ///< Disconnect wait timeout in milliseconds
} sc_disconnect_info_t;

typedef struct {
  uint8_t cancel; /**< 0 for listen, 1 for unlisten or
                     cancel pending listen. */
  uint32_t local_ip_addr; ///< Local IP address in host byte
                          ///< order
  uint16_t local_port;    ///< Local listen port
  uint32_t timeout;
  int16_t vlan_tag; ///< VLAN tag 1-4094 or -1 if VLAN is
                    ///< not used. (VLAN tags 0 and 4095 are
                    ///< reserved!)
  uint32_t remote_ip_address; /**< Remote IP address if
                                 connected. */
  uint16_t
      remote_ip_port; /**< Remote IP port if connected. */
  uint8_t remote_mac_address[MAC_ADDR_LEN]; /**< Remote MAC
                                               address if
                                               connected. */
} sc_listen_info_t;

/**
 *  ARP related driver ioctl interface.
 */
typedef struct {
  enum {
    ARP_COMMAND_SEND, /**< Send an ARP request out. */
    ARP_COMMAND_POLL, /**< Poll ARP table for a resolved MAC
                         address. */
    ARP_COMMAND_DUMP, /**< Dump the contents of ARP table
                         into syslog. */
    ARP_COMMAND_DUMP_ALL /**< Dump the total contents of ARP
                            table into syslog. */

  } command;          /**< ARP commands */
  uint32_t remote_ip; /**< Remote IP address in host byte
                         order */
  uint32_t timeout;
  uint8_t remote_mac_address[MAC_ADDR_LEN]; /**< Remote MAC
                                               address. */
  int16_t vlanTag;            /**< VLAN to use */
  uint16_t arp_table_index;   /**< Index of registered entry
                                 in the ARP table. */
  uint32_t arp_entry_timeout; /**< ARP entry timeout in
                                 milliseconds. Value of zero
                                 disables timeouts. */
  uint8_t lane;               /**< Network lane of TOE */
} sc_arp_t;

/**
 *  Miscellaneous ETLM specific data transfer.
 */
typedef struct {
  enum operation {
    ETLM_GET_WRITE_BYTES_SEQ_OFFSET =
        1, ///< Get channel specific write bytes offset
           ///< (register EtlmTxFifoCtrlSndSeqOff 16.6.12)
    ETLM_GET_TCB_SND_UNA =
        2, ///< Get channel specific acknowledged byte count
    ETLM_GET_SIZES =
        3 ///< Get miscellaneous ETLM capacities and sizes

  } operation;
  uint16_t channel_number;
  union // See
        // https://rkrishnan.org/posts/2013-11-13-kernel-abi-breakage.html
        // for explanation for not using unions in ioctl
        // structs!
  {
    uint32_t write_bytes_sequence_offset;
    uint32_t tcb_snd_una;
    struct {
      uint32_t number_of_connections_supported;
      uint32_t tx_packet_size;
      uint32_t rx_packet_size;
      uint32_t tx_total_window_size;
      uint32_t tx_window_size;
      uint32_t rx_total_window_size;
      uint32_t rx_window_size;
      bool vlan_supported;

    } capabilities;
  } u;

} sc_etlm_data_t;

/// @} TCPConnections

typedef struct {
  uint32_t network_interface;
  uint32_t remote_ip;
  uint16_t sequence;
  uint8_t remote_mac_addr[MAC_ADDR_LEN];
  uint32_t pingtime;
} sc_ping_info_t;

#define IGMP_JOIN 0x1
#define IGMP_LEAVE 0x2

typedef struct {
  uint32_t action;
  uint32_t group_address; // Multicast group address in host
                          // byte order!
  uint16_t ip_port_number; // Multicast group port number
  uint8_t lane;
  int16_t vlan_tag;
  uint8_t enable_multicast_bypass;
  uint32_t source_vlan_ip_address;
} sc_igmp_info_t;

typedef struct {
  pid_t pid;  /**< Process id of the process to which the
                 interrupt signal is delivered. */
  int signal; /**< Signal number to use when signalling user
                 space application. */
  uint8_t mask; /**< Bit mask specifying which interrupts to
                   enable. */
  uint8_t previous_mask; /**< Value of previous interrupt
                            bit mask. */
  void *pContext; /**< Optional user specified context to be
                     passed to the user space application
                     signal handler when a user logic
                     interrupt occurs. */
} sc_user_logic_interrupts;

/**
 *  User logic interrupts result information that is passed
 * to the user space application signal handler.
 */
typedef struct {
#ifdef SC_CTLS_H_INCLUDED_FROM__DRIVER
  struct timespec64
      timestamp; /**< Timestamp when the signal was sent. */
#else
  struct timespec timestamp; /**< Timestamp
when the signal was sent. */
#endif
  void *pContext; /**< Optional user specified context to be
                     passed to the user space application
                     signal handler when a user logic
                     interrupt occurs. */
  uint8_t minor; /**< Minor number (0-15) of the device that
                    the interrupt came from. */
  uint8_t channel; /**< User logic channel that the
                      interrupt came from. */
} sc_user_logic_interrupt_results;

typedef SC_ChannelType sc_channel_type_t;

typedef struct {
  uint16_t type;
  int16_t dma_channel_no;
  uint32_t network_interface;
  uint8_t enableDma;
  uint64_t prb_size;
} sc_alloc_info_t;

typedef struct {
  uint32_t network_interface; ///< Network interface
  uint32_t local_ip_addr; ///< Local IP address in host byte
                          ///< order
  uint32_t netmask;
  uint32_t gateway;
  uint8_t mac_addr[MAC_ADDR_LEN];
} sc_local_addr_t;

// Pointer list DMA for Rx
typedef struct {
  volatile uint64_t lastPacket[SC_MAX_CHANNELS];
} sc_pldma_t;

/**
 *  Filling scale of Rx DMA ring buffers and statistics
 * counters. This defines the unit of @ref sc_filling_t
 * filling values.
 */
typedef enum {
  SC_FILLING_SCALE_1_KB =
      0, /**< Raw fillings are in 1 KB units */
  SC_FILLING_SCALE_16_KB =
      1, /**< Raw fillings are in 16 KB units */
  SC_FILLING_SCALE_256_KB =
      2, /**< Raw fillings are in 256 KB units */
  SC_FILLING_SCALE_4_MB =
      3 /**< Raw fillings are in 4 MB units */
} sc_filling_scale_t;

/**
 *  Timestamp mode in statistics and Rx packets.
 */
typedef enum {
  SC_TIME_MODE_SECONDS_MICROSECONDS =
      0, /**< PCAP timestamp format in seconds and
            microseconds. */
  SC_TIME_MODE_SECONDS_NANOSECONDS =
      1, /**< PCAP timestamp format in seconds and
            nanoseconds. */
  SC_TIME_MODE_10_NANOSECOND_COUNT =
      2, /**< Count of 10 nanosecond units. */
  SC_TIME_MODE_100_NANOSECOND_COUNT =
      3, /**< Count of 100 nanosecond units. */
  SC_TIME_MODE_SECONDS_400_PICOSECONDS =
      4 /**< Seconds and count of 400 nanosecond units
           (quants, FPGA internal format). */
} sc_time_mode_t;
/**
 *  Rx DMA ring buffer filling levels. The scale or unit of
 * the values is defined by @ref sc_filling_scale_t. The
 * scale is currently hard coded as 0
 * (SC_FILLING_SCALE_1_KB) in @ref start_dma in @ref fpga.c.
 */
typedef struct {
  uint16_t filling; /**< Current filling level of Rx DMA
                       ring buffer. */
  uint16_t peak_filling; /**< Peak filling level of Rx DMA
                            ring buffer. */
} sc_filling_t;

typedef struct {
  uint8_t link : 1;
  uint8_t lost_link : 1;
  uint8_t sfp_present : 1;
  uint8_t sfp_removed : 1;
  uint8_t reserved : 4;
} sc_input_stat_t;

/**
 *  Status DMA registers mapped at the start of the mapped
 * Status DMA memory.
 */
typedef struct {
  volatile sc_input_stat_t input_stat[4];
  volatile uint8_t pps_missing : 1;
  volatile uint8_t pps_present : 1;
  volatile uint8_t fifo_overflow : 1;
  volatile uint8_t stat_overflow : 1;
  volatile uint8_t reserved_0 : 4;
  volatile uint32_t reserved_1 : 24;
  volatile uint32_t packet_dma_overflow;
  volatile uint32_t reserved_2;
  volatile uint32_t fifo_filling : 8;
  volatile uint32_t reserved_3 : 24;
  volatile sc_input_stat_t input_stat_ext[4];
  volatile uint32_t tcp_connection_status;
  volatile uint32_t reserved_4;
  volatile sc_filling_t dma_filling[SC_MAX_CHANNELS];
} sc_status_dma_t;

typedef struct {
  uint16_t minTemp;
  uint16_t curTemp;
  uint16_t maxTemp;
} sc_temperature_t;

typedef struct {
  enum {
    SC_MISC_NONE,
    SC_MISC_GET_FPGA_VERSION,
    SC_MISC_GET_BOARD_SERIAL,
    SC_MISC_GET_PORT_MASKS,
    SC_MISC_GET_DRIVER_LOG_MASK,
    SC_MISC_SET_DRIVER_LOG_MASK,
    SC_MISC_GET_PCI_DEVICE_NAME,
    SC_MISC_GET_INTERFACE_NAME,
    SC_MISC_SET_ARP_MODE,
    SC_MISC_SIGNAL_TEST
  } command;
  uint64_t value1;
  uint64_t value2;
  char name[100];
  uint8_t mac_addresses[SC_MAX_NUMBER_OF_NETWORK_INTERFACES]
                       [MAC_ADDR_LEN];
  uint16_t number_of_dma_channels;
  uint16_t number_of_tcp_channels;
  uint16_t number_of_nifs;
  uint8_t is_toe_connected_to_user_logic;
  uint8_t statistics_enabled;
  void *pContext;
  uint8_t mmu_is_enabled;

} sc_misc_t;

typedef struct {
  int16_t normal;
  int16_t priority;
} sc_fifo_t;

typedef struct {
  void *data;   // User addressable pointer
  uint64_t dma; // DMA address (usually physical address)
  uint64_t sz;  // size of the buffer
} sc_dmabuf_t;

/**
 *  @defgroup ACLFunctions ACL specific data passed between
 * user and kernel space.
 *  @{
 */

/**
 *  Data structure to pass data to and from user space in a
 * ioctl call.
 */
typedef struct {
  uint32_t ip; ///< IP address white/black list (filter)
  uint16_t
      port_min; ///< UDP/TCP port lower limit (inclusive)
  uint16_t
      port_max;    ///< UDP/TCP port upper limit (inclusive)
  int index;       ///< FW Table index
  SC_AclId acl_id; ///< Identify ACL Tx or Rx list
  int network_interface; ///< Network interface on which to
                         ///< setup the acl
  enum {
    ACL_CMD_EDIT = 0,
    ACL_CMD_RESET = 1,
    ACL_CMD_GET = 2,
    ACL_CMD_ENABLE = 3,
    ACL_CMD_DISABLE = 4,
    ACL_CMD_IS_SUPPORTED = 5
  } cmd; ///< The command requested (ADD acl, REMOVE acl,
         ///< ...)
  uint32_t ip_mask; ///< IP mask white/black list (filter)
  int acl_id_index; ///< ACL Tx or Rx table index
  uint16_t
      acl_nif_mask; ///< ACL network interfaces enabled mask
} sc_acl_config_t;

/// @} ACLFunctions

// These types are used in driver code with different names:
typedef SC_Version sc_sw_version_t;

#pragma pack(pop)

#define SC_IOCTL_IOC_MAGIC 'k'

#define SC_IOCTL_LINK_STATUS                               \
  _IOR(SC_IOCTL_IOC_MAGIC, 1, uint64_t)
#define SC_IOCTL_ALLOC_CHANNEL                             \
  _IOWR(SC_IOCTL_IOC_MAGIC, 2, sc_alloc_info_t)
#define SC_IOCTL_DEALLOC_CHANNEL _IO(SC_IOCTL_IOC_MAGIC, 3)
#define SC_IOCTL_GET_BUCKET_HWADDR                         \
  _IOWR(SC_IOCTL_IOC_MAGIC, 7, uint64_t)
#define SC_IOCTL_SET_LOCAL_ADDR                            \
  _IOW(SC_IOCTL_IOC_MAGIC, 9, sc_local_addr_t)
#define SC_IOCTL_GET_LOCAL_ADDR                            \
  _IOWR(SC_IOCTL_IOC_MAGIC, 10, sc_local_addr_t)
#define SC_IOCTL_GET_NO_BUCKETS                            \
  _IOR(SC_IOCTL_IOC_MAGIC, 11, uint32_t)
#define SC_IOCTL_SET_READMARK                              \
  _IOW(SC_IOCTL_IOC_MAGIC, 12, uint32_t)
#define SC_IOCTL_GET_RECV_HWADDR                           \
  _IOR(SC_IOCTL_IOC_MAGIC, 13, uint64_t)
#define SC_IOCTL_EMPTY                                     \
  _IOWR(SC_IOCTL_IOC_MAGIC, 14, uint64_t)
#define SC_IOCTL_GET_TCP_STATUS                            \
  _IOWR(SC_IOCTL_IOC_MAGIC, 18, uint32_t)
#define SC_IOCTL_READ_TEMPERATURE                          \
  _IOR(SC_IOCTL_IOC_MAGIC, 19, sc_temperature_t)
#define SC_IOCTL_MISC                                      \
  _IOWR(SC_IOCTL_IOC_MAGIC, 20U, sc_misc_t)
#define SC_IOCTL_GET_STAT                                  \
  _IOR(SC_IOCTL_IOC_MAGIC, 21, sc_stat_packet_payload_t)
#define SC_IOCTL_FPGA_RESET _IO(SC_IOCTL_IOC_MAGIC, 23)
#define SC_IOCTL_TIMERMODE                                 \
  _IOW(SC_IOCTL_IOC_MAGIC, 24, uint32_t)
#define SC_IOCTL_USERSPACE_RX _IO(SC_IOCTL_IOC_MAGIC, 25)
#define SC_IOCTL_USERSPACE_TX _IO(SC_IOCTL_IOC_MAGIC, 26)
#define SC_IOCTL_RESERVE_FIFO                              \
  _IOW(SC_IOCTL_IOC_MAGIC, 27, sc_fifo_t)
// #define SC_IOCTL_ALLOC_DMABUF _IOWR(SC_IOCTL_IOC_MAGIC,
// 28, sc_dmabuf_t)         // For allocating dynamic Tx DMA
// buffers (bucket buffers) #define SC_IOCTL_DEALLOC_DMABUF
// _IOW(SC_IOCTL_IOC_MAGIC, 29, void*)
#define SC_IOCTL_ACL_CMD                                   \
  _IOWR(SC_IOCTL_IOC_MAGIC, 30, sc_acl_config_t)
#define SC_IOCTL_GET_DRIVER_VERSION                        \
  _IOR(SC_IOCTL_IOC_MAGIC, 31, sc_sw_version_t)
#define SC_IOCTL_GET_SYSTEM_INFO                           \
  _IOR(SC_IOCTL_IOC_MAGIC, 33, sc_system_info_t)
#define SC_IOCTL_DMA_INFO                                  \
  _IOWR(SC_IOCTL_IOC_MAGIC, 34, sc_dma_info_t)
#define SC_IOCTL_MEMORY_ALLOC_FREE                         \
  _IOWR(SC_IOCTL_IOC_MAGIC, 35, sc_memory_alloc_free_t)
#define SC_IOCTL_REFERENCE_COUNT                           \
  _IOWR(SC_IOCTL_IOC_MAGIC, 36, sc_reference_count_t)

#define SC_IOCTL_CONNECT                                   \
  _IOWR(SC_IOCTL_IOC_MAGIC, 4, sc_connection_info_t)
#define SC_IOCTL_DISCONNECT                                \
  _IOW(SC_IOCTL_IOC_MAGIC, 5, sc_disconnect_info_t)
#define SC_IOCTL_ABORT_ETLM_CHANNEL                        \
  _IO(SC_IOCTL_IOC_MAGIC, 6)
#define SC_IOCTL_ARP _IOWR(SC_IOCTL_IOC_MAGIC, 8, sc_arp_t)
#define SC_IOCTL_LISTEN                                    \
  _IOW(SC_IOCTL_IOC_MAGIC, 15, sc_listen_info_t)
#define SC_IOCTL_IGMPINFO                                  \
  _IOW(SC_IOCTL_IOC_MAGIC, 16, sc_igmp_info_t)
#define SC_IOCTL_ENABLE_USER_LOGIC_INTERRUPTS              \
  _IOW(SC_IOCTL_IOC_MAGIC, 17, sc_user_logic_interrupts)
#define SC_IOCTL_LAST_ACK_NO                               \
  _IOR(SC_IOCTL_IOC_MAGIC, 22, uint32_t)
#define SC_IOCTL_ETLM_DATA                                 \
  _IOWR(SC_IOCTL_IOC_MAGIC, 32, sc_etlm_data_t)

// added by ekaline
#define SC_IOCTL_EKALINE_DATA                              \
  _IOWR(SC_IOCTL_IOC_MAGIC, 39, eka_ioctl_t)
#define SMARTNIC_EKALINE_DATA SC_IOCTL_EKALINE_DATA

/// @endcond

#ifdef __cplusplus
}
#endif

#endif /* CTLS_H */
