#ifndef MAIN_H
#define MAIN_H

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

#include <linux/timer.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/atomic.h>

typedef struct __device_context__ device_context_t;
typedef struct __channel_context__ fb_channel_t;

#include "ctls.h"
#include "util.h"
#include "net.h"
#include "acl.h"
#include "intr.h"
#ifdef SC_SMARTNIC_API
#include "arp.h"
#include "mmu.h"

// Per UDP multicast max subscriptions
#define FB_MAX_NO_SUBSCRIPTIONS 64

#include "igmp.h"
#endif /* SC_SMARTNIC_API */

#define MODULE_NAME     LOWER_PRODUCT

extern atomic_t s_NumberOfCards;
extern unsigned long bufsize;
int printMinusOne(const char * format, ...);
const char * basename(const char * fileName);

#define PRINTK(format, args...)         printk(KERN_INFO MODULE_NAME "%u: " format, pDevExt->minor, ##args)
#define PRINTK_WARN(format, args...)    printk(KERN_WARNING MODULE_NAME "%u: www in function %s at %s#%d: " format, pDevExt->minor, __func__, basename(__FILE__), __LINE__, ##args)
#define PRINTK_ERR(format, args...)     printk(KERN_ERR MODULE_NAME "%u: *** in function %s at %s#%d: " format, pDevExt->minor, __func__, basename(__FILE__), __LINE__, ##args)
#define PRINTK_DBG(format, args...)     printk(KERN_DEBUG MODULE_NAME "%u: dbg at %s#%d: " format, pDevExt->minor, basename(__FILE__), __LINE__, ##args)
#define PRINTK_                         printk

#define PRINTK_C(condition, format, args...)        if (condition) PRINTK(format, ##args)
#define PRINTK_WARN_C(condition, format, args...)   if (condition) PRINTK_WARN(format, ##args)
#define PRINTK_ERR_C(condition, format, args...)    if (condition) PRINTK_ERR(format, ##args)
#define PRINTK_DBG_C(condition, format, args...)    if (condition) PRINTK_DBG(format, ##args)

#ifdef DEBUG
    #define assert(logical_expression) \
    do \
    { \
        if (!(logical_expression)) \
        { \
            PRINTK_ERR("Assertion '%s' failed! File %s#%d, function %s\n", pDevExt->minor, #logical_expression, __FILE__, __LINE__, __func__); \
        } \
    } while (false)
#else
    #define assert(logical_expression)
#endif

#ifdef SC_SMARTNIC_API
#define DMA_CHANNEL_CANARY_UL8_OOB8_UNUSED8_MON8_UDP32_TCP64

#if SC_MAX_CHANNELS != (SC_MAX_ULOGIC_CHANNELS + SC_MAX_OOB_CHANNELS + SC_MAX_UNUSED_CHANNELS + SC_MAX_MONITOR_CHANNELS + SC_MAX_UDP_CHANNELS + SC_MAX_TCP_CHANNELS)
    #error "Constant SC_MAX_CHANNELS is outdated, the number of channels have changed. Please update its value to reflect the change."
#endif

#define SC_MAX_NO_ETLMS         1           /**< Maximum number of ETLMs supported by FPGA firmware. */
#endif /* SC_SMARTNIC_API */
#ifdef SC_PACKETMOVER_API
#if SC_MAX_CHANNELS != DMA_CHANNEL_CANARY
    #error "Constant SC_MAX_CHANNELS is outdated, the number of channels have changed. Please update its value to reflect the change."
#endif
#endif /* SC_PACKETMOVER_API */

// FIXME The new Tx dma does not have a bucket limitation, review where this is used
#define MAX_BUCKETS SC_NO_BUCKETS

#define STATISTICS_DMA_SIZE (128*1024)

#if SC_NO_BUCKETS < FB_FIFO_ENTRIES_PER_CHANNEL
    #error "The number of buckets per channel must be at least equal to the number of fifo entries reserved for that channel (for channels that can do tx in driver: OOB, UL, TCP) "
#endif

/**
 *  PCIe bus mapped memory locations defined in the FPGA.
 *  FPGA registers are mapped into BAR_0 and BAR_2
 *
 * They can be seen with the following command:
 *
 *  sudo lspci -d1c2c: -vvv
 *
 *  ...
 *  Region 0: Memory at fb700000 (64-bit, non-prefetchable) [size=1M]
 *  Region 2: Memory at fb600000 (64-bit, non-prefetchable) [size=1M]
 *  Region 4: Memory at fb800000 (64-bit, non-prefetchable) [size=8K]
 *  ...
 *
 *  Region 0 is currently used by all FPGA registers and memory mapped data.
 *  Region 2 is for User Logic register read/write.
 *  Region 4 is used by MSI exceptions.
 */

/**
 *  PCIe BARs (Base Address Registers).
 */
#define BAR_0 0     // This is "Region 0" seen above.
#define BAR_2 2     // This is "Region 2" seen above.

#define OWNER_UNDECIDED     0
#define OWNER_KERNEL        1
#define OWNER_USERSPACE     2


#pragma pack(push,1)

typedef struct
{
    volatile uint64_t next;                 /**< Pointer to next frame. @note This pointer points to a physical address that is <b>not</b> valid in user programs. */
    uint32_t status;                        /**< Status for packet */
    sc_stat_packet_payload_t    payload;
} fb_statistics_packet_t;

#pragma pack(pop)

#define NO_NIF         255                 /**< Value for a "no network interface" */

typedef struct
{
    struct list_head    dma_buffer_entry;   /**< field to keep those structs in a linked list */

    int                 sz;                 /**< Size of the allocated area */
    void *              user_va;            /**< User address to area */
    dma_addr_t          dma;                /**< Dma address to area */
    void *              kern_va;            /**< Kernel address to area */
} drv_dmabuf_t;

typedef struct
{
    uint64_t    sent_bytes;                     /**< The number of bytes sent on this channel */
    int         free_fifo_entries_normal;       /**< Number of fifo entries reserved for this channel */
    int         free_fifo_entries_normal_min;   /**< Minium number of fifo entries reserved for this channel */
    uint64_t *  fifo_markers;                   /**< Sent_bytes value marking when the FIFO has been processed */
    int         fifo_marker_tail;               /**< First entry of 'fifo_markers' in use */
    int         fifo_marker_head;               /**< Last entry of 'fifo_markers' in use */

#ifdef SC_SMARTNIC_API
    uint64_t    last_known_consumed_value;      /**< Keep a cache of the consumed register (optimization) */
#endif /* SC_SMARTNIC_API */
} tx_context_t;

/**
 *  Driver DMA channel context. One instance per DMA channel allocated and used.
 */
struct __channel_context__
{
    uint32_t dma_channel_no;             /**< DMA channel number 0-127. */
    int occupied;                        /**< Flag to indicate channel is occupied by other process */
    int type;

    device_context_t * pDevExt;         /**< Pointer back to the owning device */

    // Send dma buckets
    int         bucket_size;
    sc_bucket  *bucket[MAX_BUCKETS];                    /**< allocated bucket in kernel virtual memory address */
    dma_addr_t  dmaBucket[MAX_BUCKETS];                 /**< the physical memory address of the bucket used for DMA transfers over PCI (dma_addr_t is defined as unsigned long long) */
    uint16_t    bucket_payload_length[MAX_BUCKETS];     /**< Length of payload in the last successfully sent bucket. */
    volatile uint64_t * pZeroedBytes[MAX_BUCKETS];      /**< Pointer to successfully sent bucket zero acknowledgement. */
    uint64_t    zeroedBytesInitialValue[MAX_BUCKETS];   /**< Zeroed bytes initial value after bucket. */

    tx_context_t tx;                     /**< flow control-related variables */

    uint16_t reserved_fifo_entries_normal;      /**< Number of normal-prio fifo entries reserved for channel */
    uint16_t reserved_fifo_entries_priority;    /**< Number of high-prio fifo entries reserved for channel */

    // Receive DMA
    void *recv;                         /**< Start address of receive buffer (PRB) in kernel address space */
    dma_addr_t recvDma;                 /**< The physical address of the start of the receive buffer (PRB) used by DMA transfers */
    dma_addr_t prbFpgaStartAddress;     /**< PRB start address to write into the FPGA. If MMU is disabled then this is the same
                                             as recvDma, if MMU is enabled then this is the virtual address written into the MMU. */

    const sc_packet *pCurrentPacket;    /**< The current packet (used when channel is read by the fs read operation) */
    uint16_t curLength;
    unsigned int dataOffset;

    uint64_t lastPacketPa;              /**< Cache version of the hw address of the last received packet */

    int owner_rx;                       /**< Who handles reading the channel ring buffer (undecided, kernel, userspace) */
    int owner_tx;                       /**< Who handles writing on the channel (undecided, kernel, userspace) */

    wait_queue_head_t inq;
    wait_queue_head_t outq;

    // UL and OOB channels fields to keep track of the tx buckets in use (for driver use exclusively)
    uint16_t next_bucket;               /**< Next Tx bucket to use (least recently used) */
    int/*uint16_t*/ unused_buckets;            /**< Num of available buckets (from next_bucket and up) // ELE: not used, see channel.c line 202 FIXME */

    int64_t linux_drop_cnt;             /**< counts when network stack does not accept incoming packet */
    int64_t packet_cnt;

    //struct list_head  dma_buffer_list;      /**< Global list of all allocated dmabuf for this channel */

    sc_dma_info_t       dma_info;       /**< Miscellaneous Tx and Rx PL DMA related configuration information used by external PRB, PL DMA and buckets. */

    uint64_t            prbSize;        /**< Size of the PRB for this DMA channel. */

#ifdef SC_SMARTNIC_API

    // Multicast subscription (dest addr, dest port)  - per UDP channel
    sc_multicast_subscription_t subscriber[FB_MAX_NO_SUBSCRIPTIONS];

    sc_connection_info_t connectionInfo;

    // Connection state
    uint32_t connectionState;
    uint32_t previousConnectionState;
    spinlock_t connectionStateSpinLock;

    uint32_t initialSeqNumber;

    struct completion connectCompletion;

    bool  listen_pending;               /**< True if listen is pending, false otherwise. */
    bool  listen_cancelled;             /**< True if listen was cancelled, false otherwise. */
#endif /* SC_SMARTNIC_API */
};

#define MAX_INTR 16

/**
 *  Driver device context; one instance per card or PCIe device.
 */
struct __device_context__ // Do not use this name, it is only for forward definition of device_context_t
{

  // fixed by ekaline
  uint64_t ekaline_wc_addr;
  uint64_t eka_bar0_va;

  int      eka_debug;
  int      eka_drop_all_rx_udp;
  int      eka_drop_igmp;
  int      eka_drop_arp;

    int minor;                                  /**< Device minor number */
    fpga_revision_t rev;                        /**< Revision of the fpga image */
    bool                        groupPlDma;     /**< True is group PL DMA is suuported, false otherwise. */
    bool                        mmuSupport;     /**< True if FPGA MMU is present, false otherwise. */
    int panicOccured;                           /**< Flag to hinder that we parse the panic table multiple times */

    struct fasync_struct *fasync;               /**< something asynchronous??? */

    // Pldma
    sc_pldma_t *plDmaVa;                        /**< Virtual address of the pointer list dma */
    sc_pldma_t previousPlDmaVa;                        /**< Virtual address of the pointer list dma */
    sc_pldma_t * plDmaVaGroups[SC_NUMBER_OF_PLDMA_GROUPS];  /**< Virtual address of the pointer list dma */

    dma_addr_t plDmaPa;                         /**< Hardware address of the pointer list dma */
    dma_addr_t plDmaPaGroups[SC_NUMBER_OF_PLDMA_GROUPS];    /**< Hardware address of the group pointer list DMA */

    // Status dma
    sc_status_dma_t *statusDmaVa;               /**< Virtual address for the status dma */
    dma_addr_t statusDmaPa;                     /**< Hardware address for the status dma */

    // Statistics dma
    void * statisticsDmaVa;                     /**< Virtual address for the statitistics dma */
    dma_addr_t statisticsDmaPa;                 /**< Hardware address for the statitistics dma */
    fb_statistics_packet_t * curStatPacket;     /**< Pointer to last statistic packet */
    spinlock_t statSpinLock;

    spinlock_t registerLock;                    /**< Lock access to user logic registers */

    fb_channel_t * channel[SC_MAX_CHANNELS];    /**< Channels of the devices */

    void * bar0_va;                             /**< Kernel virtual address for the PCIe BAR 0 FPGA registers. */
    void * bar2_va;                             /**< Kernel virtual address for the PCIe BAR 2 FPGA user logic registers. */

    struct pci_dev * pdev;                      /**< Pointer to the pcie device */

    spinlock_t tcpEngineLock;                   /**< Spin lock to ensure uniform access to the tcpEngine */
    struct mutex  channelListLock;              /**< mutex lock to ensure uniform access to the channel list */

    sc_net_interface_t nif[SC_NO_NETWORK_INTERFACES];  /**< The network interfaces */

    struct igmp_context igmpContext;            /**< IGMP multicast context of the UDP interfaces */

    uint64_t ledState;                          /**< Led state (currently not used) */

    struct timer_list timer;                    /**< Timer to process channels */

    struct work_struct work;

    // Ping support
    struct mutex  pingLock;                     /**< mutex lock to ensure one ping at a time */
    sc_ping_info_t ping;
    uint32_t arp_ip;
    //struct completion pingReply;  // Driver ping not supported anymore, use Linux ping instead
    uint64_t pingStartTime;

    // Time adjust variables
    int64_t time_integral;
    int64_t time_lastDiff;
    int64_t time_deltaDiff;
    struct task_struct *time_adjust_thread;     /**< Thread for controlling software time adjust. Set to NULL when thread is not running */

    int num_irq;
    struct intr_setup_t  intrs[MAX_INTR];
    int64_t              previous_intr_cnt[MAX_INTR];

    uint32_t shadow_reg_tcb_cmd_conn_id;        /**< Shadows the selected connection Id, to save some register traffic */

    uint32_t toe_rx_win_size;                   /**< Allocated on board memory for TCP receive window (per conn) */
    uint32_t toe_tx_win_size;                   /**< Allocated on board memory for TCP transmit window (per conn) */

    uint16_t free_fifo_entries_normal;
    uint16_t free_fifo_entries_priority;

    uint16_t    lane_base;                      /**< System wide lane start index based on cumulative lane counts on previous cards */

    uint16_t    mac_lane_mask;
    uint16_t    oob_lane_mask;
    uint16_t    tcp_lane_mask;
    uint8_t     tcp_lane;                       /**< Lane of the one and only ETLM. Driver supports only one TCP engine */
    uint16_t    udp_lane_mask;

    uint16_t    qspf_lane_mask;
    uint16_t    monitor_lane_mask;
    uint16_t    evaluation_mask;

    bool        statistics_module_is_present;   /**< True if the Rx/Tx Ethernet frame statistics module is present in FPGA, false otherwise. */

    int         nb_netdev;                      /**< Number of lanes actually present in the FW */

    bool toeIsConnectedToUserLogic;             /**< True if TOE is connected to user logic, false otherwise */

    acl_context_t   acl_context;                /**< ACL related data */

    arp_context_t   arp_context;                /**< ARP related data */

    sc_filling_scale_t      filling_scale;      /**< Filling scale in statistics. */
    sc_time_mode_t          time_mode;          /**< Timestamp mode in RX packets and statistics. */

    bool        reset_fpga;                     /**< True if FPGA reset is in progress, false otherwise. */
    uint32_t    fpga_reset_count;               /**< Count of FPGA resets on this device */

    sc_user_logic_interrupts    user_logic_interrupts;  /**< Signal number to use for user logic interrupts. */

    mmu_context_t *         pMMU;               /**< Context data of MMU. */
    bool                    iommu;              /**< True if IOMMU is enabled, false otherwise. See https://en.wikipedia.org/wiki/Input%E2%80%93output_memory_management_unit */
};

/**
 *  Logging masks to enable logging in different categories.
 *  Combine the flags together with ORing (|) or addition (+) to achieve the desired logging level and detail.
 */
typedef enum
{
    LOG_NONE                                        = 0x0000000000000000,   /**< No logging enabled. */
    LOG_TCP                                         = 0x0000000000000001,   /**< Enable logging of TCP related activity. */
    LOG_ETLM                                        = 0x0000000000000002,   /**< Enable logging of ETLM related activity. */
    LOG_ARP                                         = 0x0000000000000004,   /**< Log ARP related activity. */
    LOG_IOCTL                                       = 0x0000000000000008,   /**< Log IOCTL related activity. */
    LOG_ICMP                                        = 0x0000000000000010,   /**< Log ICMP (ping) related activity. */
    LOG_IGMP                                        = 0x0000000000000020,   /**< Log IGMP () related activity. */
    LOG_NIC                                         = 0x0000000000000040,   /**< Log NIC (OOB) related activity. */
    LOG_IPV4                                        = 0x0000000000000080,   /**< Log IP version 4 related activity. */
    LOG_IPV6                                        = 0x0000000000000100,   /**< Log IP version 6 related activity. */
    LOG_REG                                         = 0x0000000000000200,   /**< Enable logging register operations. */
    LOG_ACL                                         = 0x0000000000000400,   /**< Enable logging of ACL related activity. */
    LOG_MSI_MSIX                                    = 0x0000000000000800,   /**< Log any activity related to MSI or MSIX. */
    LOG_UDP                                         = 0x0000000000001000,   /**< Log any activity related to UDP. */
    LOG_CHANNEL                                     = 0x0000000000002000,   /**< Log channel specific activity. */
    LOG_PL                                          = 0x0000000000004000,   /**< Log activity on PL (Rx pointer list). */
    LOG_LISTEN                                      = 0x0000000000008000,   /**< Log activity related to listen. */
    LOG_FILE                                        = 0x0000000000010000,   /**< Log activity related to files */
    LOG_OVERFLOW                                    = 0x0000000000020000,   /**< Log activity related to miscellaneous overflow situations. */
    LOG_PCIe                                        = 0x0000000000040000,   /**< Log activity related to the PCIe bus. */
    LOG_USER_LOGIC                                  = 0x0000000000080000,   /**< Log activity related to user logic. */
    LOG_MAC                                         = 0x0000000000100000,   /**< Log activity related MAC addresses. */
    LOG_IP_ADDRESS                                  = 0x0000000000200000,   /**< Log activity related to IP addresses, net masks, gateways, etc. */
    LOG_FPGA                                        = 0x0000000000400000,   /**< Log activity related to the FPGA. */
    LOG_FPGA_RESET                                  = 0x0000000000800000,   /**< Log activity related to FPGA reset. */
    LOG_STATISTICS                                  = 0x0000000001000000,   /**< Log activity related to packet statistics. */
    LOG_MMU                                         = 0x0000000002000000,   /**< Log activity related MMU. */

    LOG_EXTERNAL                                    = 0x000000200000000,    /**< Log external activities. */
    LOG_GROUP                                       = 0x000000400000000,    /**< Log group related activities. */
    LOG_ONCE_ONLY                                   = 0x000000800000000,    /**< Log once only. Needs to be set again to log one more time. */
    LOG_FLOW_CONTROL                                = 0x0000010000000000,   /**< Log activity releated to Tx DMA flow control. */
    LOG_VLAN                                        = 0x0000020000000000,   /**< Log VLAN related activity. */
    LOG_ENTRY                                       = 0x0000040000000000,   /**< Log entry into functions etc. */
    LOG_EXIT                                        = 0x0000080000000000,   /**< Log exit from functions etc. */
    LOG_OPS                                         = 0x0000100000000000,   /**< Log activity related interface operations */
    LOG_DETAIL                                      = 0x0000200000000000,   /**< Log details. */
    LOG_ACK                                         = 0x0000400000000000,   /**< Log activity related to acknowledging/succeeding something. */
    LOG_NAK                                         = 0x0000800000000000,   /**< Log activity related to not acknowledging/failing something. */
    LOG_WRITE                                       = 0x0001000000000000,   /**< Log activity related to writes. */
    LOG_READ                                        = 0x0002000000000000,   /**< Log activity related to reads. */
    LOG_CONNECTION                                  = 0x0004000000000000,   /**< Enable logging of connections/disconnects. */
    LOG_DMA                                         = 0x0008000000000000,   /**< Log any DMA related activity. */
    LOG_RX                                          = 0x0010000000000000,   /**< Log any activity related to Rx. */
    LOG_TX                                          = 0x0020000000000000,   /**< Log any activity related to Tx. */
    LOG_MEMORY                                      = 0x0040000000000000,   /**< Log acitivity related to memory access or allocation/deallocation */
    LOG_INTERRUPTS                                  = 0x0080000000000000,   /**< Log any activity related to interrupt processing. */
    LOG_INIT                                        = 0x0100000000000000,   /**< Log any activity related to initialization or cleanup. */
    LOG_DEBUG                                       = 0x0200000000000000,   /**< Log activities that are related to debugging (or replacement for old command line option debug). */
    LOG_UNKNOWN                                     = 0x0400000000000000,   /**< Log unknown, strange or unhandled activity. */
    LOG_DUMP                                        = 0x0800000000000000,   /**< Dump/disassemble something into clear text or into hex. */
    LOG_WARNING                                     = 0x1000000000000000,   /**< Log warnings. */
    LOG_ERROR                                       = 0x2000000000000000,   /**< Log errors. */

    // Special log flags to choose between LOGALL or LOGANY alternatives
    LOG_IF_ALL_OTHER_BITS_MATCH                     = 0xC000000000000000,   /**< Log only if all other bits match. */
    LOG_IF_ALL_BITS_MATCH                           = 0x8000000000000000,   /**< Log only if all bits match. */

    // Combined flags for convenience:

    LOG_ALL                                         = 0x7FFFFFFFFFFFFFFF,   /**< Log everything! */
    LOG_DETECT_COMMAND_LINE_OVERRIDE                = 0xFFFFFFFFFFFFFFFF,   /**< Used to detect command line override of logmask. */

    LOG_ALWAYS                                      = LOG_ERROR | LOG_WARNING | LOG_UNKNOWN,    /**< Always log any of these */

    LOG_MMAP                                        = LOG_INIT | LOG_MEMORY,                    /**< Log any activity related to memory mapping operations. */

    LOG_REG_W                                       = LOG_REG | LOG_WRITE,                      /**< Enable logging register operations. */
    LOG_REG_R                                       = LOG_REG | LOG_READ,                       /**< Enable logging register operations. */

    LOG_TCP_CONNECTION                              = LOG_TCP | LOG_CONNECTION,                 /**< Enable logging of TCP connections/disconnects. */
    LOG_TCP_LISTEN                                  = LOG_TCP | LOG_LISTEN,                     /**< Log activity related to TCP listen. */

    LOG_ETLM_TCP_LISTEN                             = LOG_ETLM | LOG_TCP_LISTEN,                /**< Enable logging of ETLM TCP listen. */
    LOG_ETLM_TCP_CONNECTION                         = LOG_ETLM | LOG_TCP_CONNECTION,            /**< Enable logging of ETLM TCP connections/disconnects. */
    LOG_ETLM_TCP_CONNECTION_REG_W                   = LOG_ETLM_TCP_CONNECTION | LOG_REG_W,      /**< Enable logging of ETLM TCP connection/disconnects related register writes. */

    LOG_PLDMA                                       = LOG_PL | LOG_DMA,                         /**< Log activity on PL DMA (Rx pointer list DMA). */
    LOG_GROUP_PLDMA                                 = LOG_GROUP | LOG_PL | LOG_DMA,             /**< Log activity on group PL DMA (Rx pointer list DMA). */
    LOG_EXTERNAL_PLDMA                              = LOG_EXTERNAL | LOG_PL | LOG_DMA,          /**< Log activity on external group PL DMA (Rx pointer list DMA). */
    LOG_TX_DMA                                      = LOG_TX | LOG_DMA,                         /**< Log Tx DMA related activity */

    LOG_TX_ZERO_BYTES_ACK                           = LOG_TX | LOG_INIT | LOG_ACK | LOG_ERROR,          /**< Tx DMA zero bytes acknowledgement */
    LOG_TX_REGISTER_WRAP_AROUND                     = LOG_TX | LOG_REG | LOG_WRITE | LOG_OVERFLOW,      /**< Log register wrap arounds */

    LOG_FILE_OPS                                    = LOG_FILE | LOG_OPS,                               /**< Log file operations */
    LOG_USER_LOGIC_FILE_OPS                         = LOG_FILE | LOG_OPS | LOG_USER_LOGIC,              /**< Log file operations */
    LOG_NIC_OPS                                     = LOG_NIC | LOG_OPS,                                /**< Log NIC operations */
    LOG_NIC_TX                                      = LOG_NIC | LOG_TX | LOG_IF_ALL_OTHER_BITS_MATCH,   /**< Log NIC Tx errors */

    LOG_PCIE_MEMORY_ALLOCATIONS                     = LOG_INIT | LOG_DETAIL | LOG_PCIe,                 /**< Log PCIe memory allocations/deallocations */
    LOG_MEMORY_ALLOCATIONS                          = LOG_INIT | LOG_DETAIL,                            /**< Log kernel memory allocations/deallocations */

} LogMask_t;

extern LogMask_t logmask;

#if defined(SUPPORT_LOGGING) && (SUPPORT_LOGGING == 1)
    #define LOGANY(logMask)         unlikely((((logMask) & (LOG_ALWAYS)) != 0) || ((logmask & (logMask)) != 0))         /**< Log if any bits are set inclusive LOG_ALWAYS */
    #define LOGALL(logMask)         unlikely((((logMask) & (LOG_ALWAYS)) != 0) || ((logmask & (logMask)) == (logMask))) /**< Log if all bits are set except if any bit of LOG_ALWAYS is set */
    #define LOGOTHER(logMask)       ((logmask & LOG_IF_ALL_OTHER_BITS_MATCH) == LOG_IF_ALL_OTHER_BITS_MATCH ? LOGALL((logMask) & ~LOG_IF_ALL_OTHER_BITS_MATCH) : LOGALL(logMask))
    #define LOG(logMask)            ((logmask & LOG_IF_ALL_BITS_MATCH) == LOG_IF_ALL_BITS_MATCH ? LOGOTHER(logMask) : LOGANY(logMask))
    #define FUNCTION_ENTRY(logType, format, args...) \
        if (LOGALL((logType) | LOG_ENTRY)) \
        { \
            char buffer[100]; \
            snprintf(buffer, sizeof(buffer), format, ##args); \
            PRINTK(">Entering function %s(%s)\n", __func__, buffer); \
        }
    #define FUNCTION_EXIT(logType, format, args...) \
        if (LOGALL((logType) | LOG_ENTRY)) \
        { \
            char buffer[100]; \
            snprintf(buffer, sizeof(buffer), format "\n", ##args); \
            PRINTK("<Exiting function %s%s%s", __func__, buffer[0] == '\n' ? "" : ": ", buffer); \
        }
#else
    #define LOGALL(logMask)     false
    #define LOGANY(logMask)     false
    #define LOG(logMask)        false
    #define FUNCTION_ENTRY(logType, format, args...)
    #define FUNCTION_EXIT(logType, format, args...)
#endif

extern unsigned int rto;                // module option (retransmission timeout)
extern int pad_frames;                  // module option (pad_frames=true|false)
extern unsigned int max_etlm_channels;  // module option (maximum number of ETLM channels used)
extern char * interface_name;           // module option (network interface base name)

bool is_valid_udp_lane(const device_context_t * pDevExt, int lane);

void update_link_status(device_context_t * pDevExt);

#ifdef DUMP
    #include "dump.h"
#else
    #define DUMP_CHANNELS(pDevExt, options, channelNumber)
    #define DUMP_PACKET(pDevExt, message, buffer, length)
    #define DUMP_BUCKETS(pChannel)
#endif

inline static uint32_t get_local_ip(const sc_net_interface_t * nif)
{
    //IPBUF(1); IPBUF(2); IPBUF(3);
    //PRINTK("+++ get_local_ip on lane %u: local IP %s netmask %s gateway %s\n", nif->pDevExt->minor, nif->lane,
    //    LogIP(buf1, siz1, nif->local_ip), LogIP(buf2, siz2, nif->netmask), LogIP(buf3, siz3, nif->gateway));
    return nif->local_ip;
}

inline static void set_local_ip(sc_net_interface_t * nif, uint32_t local_ip_address)
{
    //IPBUF(1); IPBUF(2); IPBUF(3);
    //PRINTK("+++ set_local_ip on lane %u: local IP %s netmask %s gateway %s\n", nif->pDevExt->minor, nif->lane,
    //    LogIP(buf1, siz1, local_ip_address), LogIP(buf2, siz2, nif->netmask), LogIP(buf3, siz3, nif->gateway));
    nif->local_ip = local_ip_address;
}

#endif // MAIN_H
