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
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/netdevice.h>
#include <linux/byteorder/generic.h>

#include "main.h"
#include "fpga.h"
#include "regs.h"

#include "fpgatypes.h" // From FB2022_test

#define LED_MUX_SW_CH0_LED0 0x100000000LL
#define LED_MUX_SW_CH0_LED1 0x200000000LL
#define LED_MUX_SW_CH1_LED0 0x400000000LL
#define LED_MUX_SW_CH1_LED1 0x800000000LL
#define LED_ON_CH0_LED0     0x1LL
#define LED_ON_CH0_LED1     0x4LL
#define LED_ON_CH1_LED0     0x10LL
#define LED_ON_CH1_LED1     0x40LL

//#define CON_CTRL_TYPE                   0x01
//#define DEST_MAC_TYPE_LSB               0x05
//#define DEST_MAC_TYPE_MSB               0x06
//#define WINDOW_SIZE_TYPE                0x07
//#define INITAL_SEQ_NO_TYPE              0x08
//#define LAST_ACK_NO                     0x09
//#define RTO_TYPE                        0x0A
//#define DEST_PORT_TYPE                  0x12
//#define SRC_IP_TYPE                     0x13
//#define DEST_IP_TYPE                    0x14
//#define GATEWAY_TYPE                    0x15
//#define SUBNET_TYPE                     0x16
//#define IID_TYPE                        0x17
//#define SRV_LISTEN_IID_TYPE             0x18

typedef enum
{
    UDP_PARAMETER_TYPE_CONNECTION_MODE  = 0x02,
    UDP_PARAMETER_TYPE_MAC_LSB          = 0x03,
    UDP_PARAMETER_TYPE_MAC_MSB          = 0x04,
    UDP_PARAMETER_TYPE_VLAN_TAG         = 0x0B,
    UDP_PARAMETER_TYPE_PORT             = 0x11,
    UDP_PARAMETER_TYPE_DMA_CHANNEL      = 0x20
} udp_parameter_type_t;

//#define FLASH_CMD_READ 0x4000000000000000LL
//#define FLASH_BUSY_MASK 0x8000000000000000LL

#define PLDMA_ATTR_ACTIVATE            7    // Start the pldma subsystem
#define PLDMA_ATTR_FAST               15    // Enable fine grained pointer list updates (updates only the relevant cache lines)
#define PLDMA_ATTR_PKT_COUNT          16    // Trigger a pldma update when the number of packets received reaches this amount (must be enabled)
#define PLDMA_ATTR_PKT_COUNT_ENABLE   31    // Enable triggering pldma update on packet count
#define PLDMA_ATTR_TIMEOUT            32    // Trigger a pldma update when this timeout is reached (4ns units) (must be enabled)
#define PLDMA_ATTR_TIMEOUT_ENABLE     63    // Enable triggering pldma update on timeout

// RX DMA attributes:
#define DMA_ATTR_TIME_MODE      0    // Time mode controls the format of the timestamps.
#define DMA_ATTR_FILLING_SCALE  5    // Filling scale controls the scale of the filling in the status DMA.
#define DMA_ATTR_START          7    // Activate the DMA channel
#define DMA_ATTR_INTR_ENABLE    8    // Request interrupt when pldma changes
#define DMA_ATTR_LATENCY_GRP    9    // Set latency group of pldma updates (currently 0=fast, 1=counter/timer based)
#define DMA_ATTR_PL_CLEAR      16    // Request the pldma for this channel to be cleared

#define MAC_ADDR_LEN 6

#define actelReadFlash       0x01 //Asynch read from flash RAM
#define actelWriteFlash      0x02 //Asynch write to flash RAM
#define actelReadRevLsb      0x08 //Read back revision LSB from Actel FPGA.
#define actelReadRevMsb      0x09 //Read back revision MSB from Actel FPGA.
#define actelReadStatus      0x0A //Read back status from Actel FPGA.
#define actelWriteScratchPad 0x0B //Scratch pad write command.
#define actelReadScratchPad  0x0C //Scratch pad read command.
#define actelResetFlash      0x0D //Reset flash command.
#define actelRebootVirtex    0x0E //Reboot Virtex-5 with new Image.

#define actelBitFlashReady 0x00010000L
#define actelBitFlashNoAck 0x00020000L


#define FPGA_RESET_TIME_THR 50000

#define LOG_FPGA_FUNCTION(log, format, args...)     if (log) PRINTK("FPGA: %s" format "\n", __func__, ##args)
#define LOG_ETLM_FUNCTION(log, format, args...)     if (log) PRINTK("ETLM: %s" format "\n", __func__, ##args)

static int adjust_cardtime_thread(void* pData);

#define RETURN_CASE(value)  case value: return #value

const char * EtlmState(int etlmState)
{
    switch (etlmState)
    {
        case ETLM_TCP_STATE_CLOSED:                return "ETLM_TCP_STATE_CLOSED";
        case ETLM_TCP_STATE_LISTEN:                return "ETLM_TCP_STATE_LISTEN";
        case ETLM_TCP_STATE_SYN_SENT:              return "ETLM_TCP_STATE_SYN_SENT";
        case ETLM_TCP_STATE_SYN_RCVD:              return "ETLM_TCP_STATE_SYN_RCVD";
        case ETLM_TCP_STATE_ESTABLISHED:           return "ETLM_TCP_STATE_ESTABLISHED";
        case ETLM_TCP_STATE_CLOSE_WAIT:            return "ETLM_TCP_STATE_CLOSE_WAIT";
        case ETLM_TCP_STATE_FIN_WAIT1:             return "ETLM_TCP_STATE_FIN_WAIT1";
        case ETLM_TCP_STATE_CLOSING:               return "ETLM_TCP_STATE_CLOSING";
        case ETLM_TCP_STATE_LAST_ACK:              return "ETLM_TCP_STATE_LAST_ACK";
        case ETLM_TCP_STATE_FIN_WAIT2:             return "ETLM_TCP_STATE_FIN_WAIT2";
        case ETLM_TCP_STATE_TIME_WAIT:             return "ETLM_TCP_STATE_TIME_WAIT";
        case ETLM_TCP_STATE_CLEAR_TCB:             return "ETLM_TCP_STATE_CLEAR_TCB";
        case ETLM_TCP_STATE_NONE:                  return "ETLM_TCP_STATE_NONE";
        case ETLM_TCP_STATE_CLOSED_DATA_PENDING:   return "ETLM_TCP_STATE_CLOSED_DATA_PENDING";
        default:
            {
                static char buffer[30];
                snprintf(buffer, sizeof(buffer), " *** UNKNOWN ETLM STATE: %d", etlmState);
                return buffer;
            }
    }
}

const char * IsTestFpga(uint16_t fpgaType)
{
    switch (fpgaType)
    {
        case FPGA_TYPE_LATINA_TEST: return "FPGA_TYPE_LATINA_TEST";
        case FPGA_TYPE_LIVIGNO_TEST: return "FPGA_TYPE_LIVIGNO_TEST";
        case FPGA_TYPE_LIVORNO_TEST: return "FPGA_TYPE_LIVORNO_TEST";
        case FPGA_TYPE_LUCCA_TEST: return "FPGA_TYPE_LUCCA_TEST";
        default: return NULL;
    }
}

const char * IsLeonbergFpga(uint16_t fpgaType)
{
    switch (fpgaType)
    {
        case FPGA_TYPE_LATINA_LEONBERG: return "FPGA_TYPE_LATINA_LEONBERG";
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330: return "FPGA_TYPE_LIVIGNO_LEONBERG_V7330";
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690: return "FPGA_TYPE_LIVIGNO_LEONBERG_V7690";
        RETURN_CASE(FPGA_TYPE_LIVORNO_LEONBERG);
        //case FPGA_TYPE_LIVORNO_LEONBERG: return "FPGA_TYPE_LIVORNO_LEONBERG";
        case FPGA_TYPE_LUCCA_LEONBERG_V7330: return "FPGA_TYPE_LUCCA_LEONBERG_V7330";
        case FPGA_TYPE_LUCCA_LEONBERG_V7690: return "FPGA_TYPE_LUCCA_LEONBERG_V7690";
        case FPGA_TYPE_MANGO_LEONBERG_VU125: return "FPGA_TYPE_MANGO_LEONBERG_VU125";
        case FPGA_TYPE_MANGO_LEONBERG_VU9P: return "FPGA_TYPE_MANGO_LEONBERG_VU9P";
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1: return "FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1";
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P: return "FPGA_TYPE_MANGO_04_LEONBERG_VU9P";
        case FPGA_TYPE_PADUA_LEONBERG_VU125: return "FPGA_TYPE_PADUA_LEONBERG_VU125";
        default: return NULL;
    }
}

const char * FpgaName(uint16_t fpgaType)
{
    const char * fpgaName = IsLeonbergFpga(fpgaType);
    if (fpgaName == NULL)
    {
        fpgaName = IsTestFpga(fpgaType);
        if (fpgaName == NULL)
        {
            return "??? UNKNOWN CARD ???";
        }
    }
    return fpgaName;
}

#ifdef CONFIG_X86_32
static inline __u64 readq(const volatile void __iomem *addr)
{
    const volatile u32 __iomem *p = addr;
    u32 low, high;

    low = readl(p);
    high = readl(p + 1);

    return low + ((u64)high << 32);
}

static inline void writeq(__u64 val, volatile void __iomem *addr)
{
    writel(val, addr);
    writel(val >> 32, addr + 4);
}
#endif


static inline void writeq_log(const device_context_t * pDevExt, __u64 val, volatile void __iomem *addr, bool log)
{
    PRINTK_C(unlikely(log), "reg w: %p %016llx\n", addr, val);

    writeq(val, addr);
}

static inline void writel_log(const device_context_t * pDevExt, __u32 val, volatile void __iomem *addr, bool log)
{
    PRINTK_C(unlikely(log), "reg w: %p %08x\n", addr, val);

    writel(val, addr);
}

//#define writeq writeq_debug
//#define writel writel_debug

uint64_t getFpgaVersion(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision
    return readq(pDevExt->bar0_va + FPGA_REVISION);
}

void getFeatures(const device_context_t * pDevExt, bool * pGroupPLDMA, bool * pExtendedRegisterMode,
    bool * pBulkSupported, bool * pTxDmaSupport, bool * pMmuSupport, bool * pWarningSupport)
{
    uint64_t features;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision
    features = readq(pDevExt->bar0_va + FPGA_REVISION);

    if (pGroupPLDMA != NULL)
    {
        *pGroupPLDMA = ((features >> 48) & 1) == 0;
    }
    if (pExtendedRegisterMode != NULL)
    {
        *pExtendedRegisterMode = ((features >> 49) & 1) != 0;
    }
    if (pMmuSupport != NULL)
    {
        *pMmuSupport = ((features >> 53) & 1) != 0;
    }
    if (pBulkSupported != NULL)
    {
        *pBulkSupported = false;
    }
    if (pTxDmaSupport != NULL)
    {
        *pTxDmaSupport = false;
    }
    if (pWarningSupport != NULL)
    {
        *pWarningSupport = false;
    }
}

uint64_t getFpgaBuildTime(const device_context_t * pDevExt)
{
    uint64_t fpgaBuildTime;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(1ULL, pDevExt->bar0_va + FPGA_REVISION);           // select the fpga build time
    fpgaBuildTime = readq(pDevExt->bar0_va + FPGA_REVISION);
    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision
    return fpgaBuildTime;
}

uint32_t getFpgaExtendedBuildNumber(const device_context_t * pDevExt)
{
    uint32_t fpgaExtendedBuildNumber;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(3ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga build extended number
    fpgaExtendedBuildNumber = (uint32_t)readq(pDevExt->bar0_va + FPGA_REVISION);
    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision
    return fpgaExtendedBuildNumber;
}

uint64_t getDeviceConfig(const device_context_t * pDevExt)
{
    uint64_t deviceConfig;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(32ULL, pDevExt->bar0_va + FPGA_REVISION);            // select the fpga new device configuration
    deviceConfig = readq(pDevExt->bar0_va + FPGA_REVISION);
    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision
    return deviceConfig;
}

/**
 *  Get FPGA ACL, QSFP and monitor port configuration masks.
 */
void getFpgaAclQsfpMonitorConfiguration(const device_context_t * pDevExt, uint16_t * pAclLaneEnabled,
    uint16_t * pQspfLaneEnabled, uint16_t * pMonitorLaneEnabled, bool * pStatisticsModuleIsPresent)
{
    uint64_t fpgaAclQsfpMonitorConfiguration;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(33ULL, pDevExt->bar0_va + FPGA_REVISION);            // select the fpga QSPF and monitor configuration
    fpgaAclQsfpMonitorConfiguration = readq(pDevExt->bar0_va + FPGA_REVISION);
    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision

    if (pAclLaneEnabled != NULL)
    {
        *pAclLaneEnabled = (fpgaAclQsfpMonitorConfiguration >> 24) & 0xFFFF;
    }
    if (pQspfLaneEnabled != NULL)
    {
        *pQspfLaneEnabled = (fpgaAclQsfpMonitorConfiguration >> 16) & 0xFF;
    }
    if (pMonitorLaneEnabled != NULL)
    {
        *pMonitorLaneEnabled = fpgaAclQsfpMonitorConfiguration & 0xFFFF;
    }
    if (pStatisticsModuleIsPresent != NULL)
    {
        *pStatisticsModuleIsPresent = ((fpgaAclQsfpMonitorConfiguration >> 40) & 0x1) == 0 ? false : true;
    }
}

/**
 *  Get implemented (actual!) TOE Tx/Rx window memory sizes shared among all TCP sessions.
 *  The values returned by @ref etlm_getCapabilities are maximum design sizes, not necessarily
 *  same as what is implemented.
 */
void getFpgaToeWindowMemorySizes(const device_context_t * pDevExt, uint64_t * pTxWindowSize, uint64_t * pRxWindowSize,
    bool * pToeIsConnectedToUserLogic, uint64_t * pEvaluationModes)
{
    uint64_t value;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(34ULL, pDevExt->bar0_va + FPGA_REVISION);            // select the fpga TOE window memory sizes
    value = readq(pDevExt->bar0_va + FPGA_REVISION);
    writeq(0ULL, pDevExt->bar0_va + FPGA_REVISION);             // select the fpga revision
    if (pTxWindowSize != NULL)
    {
        *pTxWindowSize = (1ULL << (value & 0xff)) * 8ULL;
    }
    if (pRxWindowSize != NULL)
    {
        *pRxWindowSize = (1ULL << ((value >> 8) & 0xff)) * 8ULL;
    }
    if (pToeIsConnectedToUserLogic != NULL)
    {
        *pToeIsConnectedToUserLogic = ((value >> 16) & 1) == 1 ? true : false;
    }
    if (pEvaluationModes != NULL)
    {
        *pEvaluationModes = value & 0x1F000000;
    }
}

uint64_t getPanic(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    return readq(pDevExt->bar0_va + FPGA_PANIC);
}

static inline int waitForEngine(const device_context_t * pDevExt, unsigned int engine)
{
    uint64_t value;
    int timeout = 1000;

    value = readq(pDevExt->bar0_va + engine);
    while (timeout && !(value & (1ULL << 63)))
    {
        value = readq(pDevExt->bar0_va + engine);
        timeout--;
    }
    if (!timeout)
    {
        PRINTK_ERR("Timeout in waitForEngine in engine: %d reached!\n", engine);
        return -1;
    }
    else {
        return 0;
    }
}

/**
 *  This function does a full reset of the FPGA. It is called once from
 *  sc_probe and all FPGA and some PCIe initialization occurs afterwards.
 */
int fpga_reset(const device_context_t * pDevExt)
{
    fpga_revision_t rev;
    int i = 0;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(1, pDevExt->bar0_va + FPGA_RESET);
    udelay(1);
    while (((readq(pDevExt->bar0_va + FPGA_RESET) & 1) == 1) && (i++ < FPGA_RESET_TIME_THR))
    {
        udelay(10);
        if ((i % 100000) == 0)
        {
            PRINTK(".. still waiting for reset: %i", i);
        }
    }
    if (i >= FPGA_RESET_TIME_THR)
    {
        PRINTK_ERR("FPGA reset timed out!!!\n");
        return -1;
    }
    //PRINTK_ERR("Reset ready after %d iterations!!!\n", i);

    if (pDevExt->tcp_lane_mask != 0)
    {
        // Only reset if ETLM is present and if firmware is not a TESTFPGA image
        rev.raw_rev = getFpgaVersion(pDevExt);
        if (IsLeonbergFpga(rev.parsed.type) != NULL)
        {
            uint32_t val = (1 << 3); // See doc 16.5.1 EtlmRxCtrl register
            writel(val, pDevExt->bar0_va + ETLM_RX_CTRL);
        }
    }

    return 0;
}

bool set_mmu(const device_context_t * pDevExt, uint64_t virtualAddress, uint64_t busAddress)
{
    uint64_t value = 0ULL;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "virtualAddress = 0x%llX, busAddress = 0x%llX", virtualAddress, busAddress);

    if (virtualAddress >= 0x2000)
    {
        PRINTK_ERR("Virtual address 0x%llX has to fit into 13 bits (< 0x2000)\n", virtualAddress);
        return false;
    }
    if (busAddress >= 0x40000)
    {
        PRINTK_ERR("Bus address 0x%llX has to fit into 18 bits (< 0x40000)\n", busAddress);
        return false;
    }

    value |= 1ULL << 63;                /* Update the memory map */
    value |= 1ULL << 62;                /* Update the primary memory mode */
    value |= 3ULL << 60;                /* Mode = 3: 4 MB page size */
    value |= 0ULL << 59;                /* HighMem = 0: page sizes available from 512 KB to 4 MB */
    value |= 0ULL << 58;                /* UpdSec = 0: primary address space */
    value |= 0ULL << 48;                /* Mask = 0: all endpoints */
    value |= virtualAddress << 32;      /* virtual address */
    value |= busAddress;                /* bus address */

    writeq(value, pDevExt->bar0_va + REG_MMU);

    PRINTK_C(LOGANY(LOG_MMU | LOG_REG_W), "FPGA %s: wrote value 0x%llX to register 0x%llX\n",
        __func__, value, (uint64_t)pDevExt->bar0_va + REG_MMU);

    return true;
}

void enable_mmu(const device_context_t * pDevExt, bool enable)
{
    uint64_t value = enable ? (1ULL << 2) : (1ULL << 3);

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "MMU %s", enable ? "enabled" : "disabled");

    writeq(value, pDevExt->bar0_va + REG_PCIE_CONFIG);

    PRINTK_C(LOGANY(LOG_MMU | LOG_REG_W), "MMU %s: wrote 0x%016llX to register 0x%llX\n",
        enable ? "enabled" : "disabled", value, (uint64_t)pDevExt->bar0_va + REG_PCIE_CONFIG);
}

void set_dma_read_mark_offset(const device_context_t * pDevExt, int channelNo, uint32_t va)
{
    const unsigned int offset = fb_phys_channel(channelNo);
    fb_channel_t * channel = pDevExt->channel[channelNo];
    uint64_t value = (uint64_t)((void*)channel->recvDma + va);
    void * readMarkerRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_RD_MARK);
    writeq(value, readMarkerRegister);

    PRINTK_C(LOG(LOG_FPGA | LOG_PLDMA | LOG_REG_W), "FPGA: set_dma_read_mark_offset: channel %d, wrote value 0x%llX to register 0x%p\n",
        channelNo, value, readMarkerRegister);
}

void set_rx_dma_read_mark(const device_context_t * pDevExt, int channelNo, const uint8_t * readPtr)
{
    const unsigned int offset = fb_phys_channel(channelNo);
    fb_channel_t * channel = pDevExt->channel[channelNo];
    uint64_t value = channel->prbFpgaStartAddress + (readPtr - (uint8_t *)channel->recv);
    void * readMarkerRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_RD_MARK);

    writeq(value, readMarkerRegister);

#if 0
    PRINTK_C(LOG(LOG_FPGA | LOG_PLDMA | LOG_REG_W | LOG_DETAIL),
        "FPGA: set_dma_read_mark: channel %d, wrote value 0x%llX to register 0x%p\n",
        channelNo, value, readMarkerRegister);
#endif
}

void enable_pldma(const device_context_t * pDevExt, int enabled)
{
  uint64_t attrs = enabled ? (0x1 << PLDMA_ATTR_ACTIVATE) : 0;
  // Fixed by Ekaline for Martin's patch
  /* attrs |= 0x1ULL << PLDMA_ATTR_FAST; // enable fine grained pointer list updates (updates only the relevant cache lines) */

  attrs |= (0LL << PLDMA_ATTR_PKT_COUNT_ENABLE); // enable packet count throttling
  attrs |= (0x1LL << PLDMA_ATTR_PKT_COUNT); // packet count value throttling
  attrs |= (1ULL << PLDMA_ATTR_TIMEOUT_ENABLE); // enable timeout send
  attrs |= (0x100LL << PLDMA_ATTR_TIMEOUT); // timeout (in 4ns unit)

  // Fixed by Ekaline for Martin's patch
  //  attrs |= 0x0ULL << PLDMA_ATTR_FAST; // enable fine grained pointer list updates (updates only the relevant cache lines)

  /* attrs |= (1LL << PLDMA_ATTR_PKT_COUNT_ENABLE); // enable packet count throttling */
  /* attrs |= (0x2LL << PLDMA_ATTR_PKT_COUNT); // packet count value throttling */
  /* attrs |= (1ULL << PLDMA_ATTR_TIMEOUT_ENABLE); // enable timeout send */
  /* attrs |= (0x100LL << PLDMA_ATTR_TIMEOUT); // timeout (in 4ns unit) */

  writeq(attrs, pDevExt->bar0_va + FPGA_PL_DMA_ATTRIBUTES);

  PRINTK_C(LOG(LOG_FPGA | LOG_PLDMA | LOG_REG_W), "FPGA: enable_pldma: %s, wrote attributes 0x%llX to register 0x%p\n",
	   enabled != 0 ? "enable" : "disable", attrs, pDevExt->bar0_va + FPGA_PL_DMA_ATTRIBUTES);
}

void setup_pl_dma(const device_context_t * pDevExt)
{
    writeq(pDevExt->plDmaPa, pDevExt->bar0_va + FPGA_PL_DMA_START_ADDRESS);

    PRINTK_C(LOG(LOG_FPGA | LOG_PLDMA | LOG_REG_W), "FPGA: setup_pl_dma: wrote value 0x%llX to register 0x%p\n",
        pDevExt->plDmaPa, pDevExt->bar0_va + FPGA_PL_DMA_START_ADDRESS);
}

int start_rx_dma_address(const device_context_t * pDevExt, unsigned int dma_channel_no, dma_addr_t prbPhysicalAddress, uint64_t prbSize, bool initializeChannel)
{
    return -1;
}
void setup_external_group_rx_pl_dma(const device_context_t * pDevExt, unsigned channelNumber, uint64_t plDmaGroupPhysicalAddress, bool enable)
{
}

void enable_group_rx_pl_dma(const device_context_t * pDevExt, unsigned group, int64_t flushPacketCount, int64_t flushTimeoutInNs, bool enabled)
{
}

// setup ring_buffer configuration to MCDMA
static int setup_dma(device_context_t * pDevExt, unsigned int dma_channel_no, uint64_t prbSize)
{
    bool log = LOG(LOG_DMA | LOG_REG_W);
    const unsigned int offset = fb_phys_channel(dma_channel_no);
    fb_channel_t * channel = pDevExt->channel[dma_channel_no];
    void * startRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_START);
    void * endRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_END);
    void * readMarkerRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_RD_MARK);

    FUNCTION_ENTRY(LOG_DMA, "channel=%u PRB size %llu (0x%llX) bytes, %llu MB", dma_channel_no, prbSize, prbSize, prbSize / 1024 / 1024);

    // setup start address of host buffer for FPGA/MCDMA
    writeq(channel->prbFpgaStartAddress, startRegister);
    PRINTK_C(unlikely(log), "%s: channel %u, wrote value 0x%llX to PRB start register 0x%p\n",
        __func__, dma_channel_no, channel->prbFpgaStartAddress, startRegister);

    // Setup PRB end and initial read marker:
    writeq_log(pDevExt, channel->prbFpgaStartAddress + prbSize, endRegister, log);
    PRINTK_C(unlikely(log), "%s: channel %u, wrote value 0x%llX to PRB end register 0x%p\n",
        __func__, dma_channel_no, channel->prbFpgaStartAddress + prbSize, endRegister);

    writeq_log(pDevExt, channel->prbFpgaStartAddress + prbSize - 8, readMarkerRegister, log);
    PRINTK_C(unlikely(log), "%s: channel %u, wrote value 0x%llX to read marker register 0x%p\n",
        __func__, dma_channel_no, channel->prbFpgaStartAddress + prbSize - 8, readMarkerRegister);

    channel->lastPacketPa = SC_INVALID_PLDMA_ADDRESS;
    *((uint64_t*)channel->recv) = 0; /* Zero first 8 bytes in PRB */
    channel->pCurrentPacket = NULL;
    channel->curLength = 0;

    FUNCTION_EXIT(LOG_DMA, "");

    return 0;
}

int start_dma(device_context_t * pDevExt, unsigned int dma_channel_no, uint64_t prbSize)
{
    const unsigned int offset = fb_phys_channel(dma_channel_no);
    uint64_t attrs;
    void * controlRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_ATTR);

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " DMA channel %u", dma_channel_no);

    FUNCTION_ENTRY(LOG_DMA, "channel=%u", dma_channel_no);

    setup_dma(pDevExt, dma_channel_no, prbSize);

    // Martin Allingham's comment about this delay: highly probably not necessary. Only if there is a possibility
    // that the below start DMA command somehow gets through before setup_dma addresses then there could be a problem.
    // - If a bundled PCIe bus write somehow arrives at the same time to the FPGA and below start command is written slightly before setup
    //   due to internal FPGA delays.
    // - Or if internal FPGA delays otherwise affect the order of setup and start DMA writes.
    // Even 1 microsecond delay should be enough to prevent above scenarios.
    udelay(10); // Needed in capture - because of some internal fifo need to flush - for safty we added here as well

    attrs = (1ULL << DMA_ATTR_START) + (pDevExt->filling_scale << DMA_ATTR_FILLING_SCALE) + (pDevExt->time_mode << DMA_ATTR_TIME_MODE);
    if (dma_channel_no >= FIRST_OOB_CHANNEL && dma_channel_no < FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS)
    {
        // OOB channels are hardcoded to use interrupts and to be in the high latency group (group 1)
        attrs |= (1ULL << DMA_ATTR_LATENCY_GRP) | (1ULL << DMA_ATTR_INTR_ENABLE);
    }
    writeq(attrs, controlRegister); // Start DMA!

    PRINTK_C(LOG(LOG_PLDMA | LOG_REG_W), "start_dma: channel %u, wrote attributes 0x%llX to register 0x%p\n",
        dma_channel_no, attrs, controlRegister);

    FUNCTION_EXIT(LOG_DMA, "");

    return 0;
}

/**
 *   Stop Rx DMA and clear the corresponding PL DMA entry to an invalid value.
 *
 *  In earlier versions of the software the invalid address value was zero
 *  but with MMU address zero is now a valid address. The FPGA still clears the
 *  PL DMA entry to zero below which will cause problems with valid
 *  MMU address zero so after the FPGA clears the PL DMA entry we fill
 *  the entry with an invalid address value that is not zero.
 */
int stop_dma(device_context_t * pDevExt, unsigned int dma_channel_no)
{
    unsigned int timeout = 1000;
    const unsigned int offset = fb_phys_channel(dma_channel_no);
    bool log = LOG(LOG_DMA | LOG_REG_W);
    void * controlRegister = pDevExt->bar0_va + FB_RXDMA_REG(offset, PKG_RXDMA_ATTR);

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " DMA channel %u", dma_channel_no);

    FUNCTION_ENTRY(LOG_DMA, "channel=%u", dma_channel_no);

    pDevExt->plDmaVa->lastPacket[offset] = 1; // SC_INVALID_PLDMA_ADDRESS; /* Invalidate contents of PL DMA entry before clearing it */

    writeq(0, controlRegister); // Stop dma

    PRINTK_C(unlikely(log), "stop_dma: channel %u, wrote wrote 0 (stopped DMA) to register 0x%p, next write clears PL DMA\n",
        dma_channel_no, controlRegister);

    // Martin Allignham's comment: this delay is necessary!
    // Cannot clear driver's pointer list DMA entries before internal fifo in FPGA is cleared.
    // Otherwise driver's pointer list DMA entry could still be set after it has been cleared by software.
    udelay(10); // Wait for internal fifo to flush.

    writeq_log(pDevExt, 1ULL << DMA_ATTR_PL_CLEAR, controlRegister, log);  // Clear pldma

    /* Wait the PL DMA entry to be cleared */
    while (--timeout && (pDevExt->plDmaVa->lastPacket[offset] != 0))
    {
        udelay(1);
    }
    if (!timeout)
    {
        PRINTK_ERR("timeout in pl clearing for channel %d\n", dma_channel_no);
    }

    /* Again invalidate contents of PL DMA entry because zero is now a valid address. */
    pDevExt->plDmaVa->lastPacket[offset] = SC_INVALID_PLDMA_ADDRESS;

    pDevExt->channel[dma_channel_no]->lastPacketPa = SC_INVALID_PLDMA_ADDRESS;
    pDevExt->channel[dma_channel_no]->pCurrentPacket = NULL;
    pDevExt->channel[dma_channel_no]->curLength = 0;

    FUNCTION_EXIT(LOG_DMA | LOG_DEBUG,
        "stop_dma: done channel %d, offset %u, pldma %p\n",
        dma_channel_no, offset, (void*)pDevExt->plDmaVa->lastPacket[offset]);

    return 0;
}

int setup_status_dma(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    enable_status_dma(pDevExt, 0);
    udelay(100);
    writeq(pDevExt->statusDmaPa, pDevExt->bar0_va + STATUS_DMA_START);
    return 0;
}

int enable_status_dma(const device_context_t * pDevExt, int enabled)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    writeq(enabled ? (0x1 << 7) : 0, pDevExt->bar0_va + STATUS_DMA_ATTR);
    return 0;
}

void get_channel_fillings(const device_context_t * pDevExt, int dma_channel_no, uint16_t* cur_filling, uint16_t* peak_filling)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    if (cur_filling)
        *cur_filling = pDevExt->statusDmaVa->dma_filling[dma_channel_no].filling;
    if (peak_filling)
        *peak_filling = pDevExt->statusDmaVa->dma_filling[dma_channel_no].peak_filling;
}

int setup_statistics_dma(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    // setup start address of host buffer for FPGA/MCDMA
    //PRINTK("fb_dma_reg = %x\n", FB_DMA_REG(offset, PKG_DMA_START));
    writeq(pDevExt->statisticsDmaPa, pDevExt->bar0_va + STATISTIC_DMA_START);
    writeq(pDevExt->statisticsDmaPa + STATISTICS_DMA_SIZE, pDevExt->bar0_va + STATISTIC_DMA_END);
    writeq(pDevExt->statisticsDmaPa - sizeof(uint64_t), pDevExt->bar0_va + STATISTIC_RD_MARK);
    *(uint64_t*)(pDevExt->statisticsDmaVa) = 0;

    return 0;
}

int set_statistics_read_mark(const device_context_t * pDevExt, uint64_t addr)
{
    //LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " address %llu", addr);

    writeq(addr, pDevExt->bar0_va + STATISTIC_RD_MARK);
    return 0;
}

int enable_statistic_dma(device_context_t * pDevExt, int enabled, sc_filling_scale_t fillingScale, sc_time_mode_t timeMode)
{
    uint64_t value;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    value = enabled ? (0x1 << DMA_ATTR_START) + (fillingScale << DMA_ATTR_FILLING_SCALE) + (timeMode << DMA_ATTR_TIME_MODE) : 0;
    writeq(value, pDevExt->bar0_va + STATISTIC_ATTR);
    return 0;
}

// Enabling bypass means that the traffic that is not handled by the TOE should be sent on the OOB (Out Of Band) channel for that device
int setBypass(device_context_t * pDevExt, unsigned int lane, uint32_t enable)
{
    uint64_t value;
    unsigned long flags;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " lane %u %s", lane, enable ? "enabled" : "disabled");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);
    value = enable != 0 ? 4 : 8;
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_CONNECTION_MODE << 8) | (0)) << 32;

    PRINTK_C(LOG(LOG_DEBUG), "setBypass writes 0x%016llx to reg 0x%0x\n", value, SC_ENGINE);

    writeq(value, pDevExt->bar0_va + SC_ENGINE);
    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return 0;
}

int setPromisc(device_context_t * pDevExt, unsigned int lane, uint32_t enable)
{
    uint64_t value;
    unsigned long flags;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " lane %u %s", lane, enable ? "enabled" : "disabled");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    value = enable != 0 ? 5 : 6;
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_CONNECTION_MODE << 8) | (0)) << 32;

    PRINTK_C(LOG(LOG_DEBUG), "setPromisc writes 0x%016llx to reg 0x%0x\n", value, SC_ENGINE);

    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return 0;
}

static int setMulticastBypass(device_context_t * pDevExt, unsigned int lane, uint32_t enable, bool lock)
{
    uint64_t value;
    unsigned long flags;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " lane %u %s", lane, enable ? "enabled" : "disabled");

    if (lock)
    {
        spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);
    }

    value = enable != 0 ? 0x44 : 0x84;
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_CONNECTION_MODE << 8) | (0)) << 32;

    PRINTK_C(LOG(LOG_DEBUG), "setPromisc writes 0x%016llx to reg 0x%0x\n", value, SC_ENGINE);

    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    if (lock)
    {
        spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    }

    return 0;
}

int setSourceMac(device_context_t * pDevExt, unsigned int lane, uint8_t mac[6])
{
    uint64_t value;
    unsigned long flags;

    const unsigned int positionIndex = 0;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " lane %u MAC %02x:%02x:%02x:%02x:%02x:%02x", lane, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    value = ((uint64_t)mac[2] << 24) | ((uint64_t)mac[3] << 16) | ((uint64_t)mac[4] << 8) | (mac[5]);
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_MAC_LSB << 8) | (positionIndex)) << 32;
    writeq(value, pDevExt->bar0_va + SC_ENGINE);
    //PRINTK("setSrcMac: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0] , mac[1] , mac[2] , mac[3] , mac[4] , mac[5]);
    value = ((uint64_t)mac[0] << 8) | (mac[1]);
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_MAC_MSB << 8) | (positionIndex)) << 32;
    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return 0;
}

static int waitActelFlashReady(device_context_t * pDevExt)
{
    uint64_t tmp;
    unsigned int timeout = 3;

    do
    {
        tmp = readq(pDevExt->bar0_va + REG_FLASH_CMD);
        if ((tmp & actelBitFlashReady) != 0)
            break;
        udelay(1);
    } while (timeout--);

    if (!timeout)
    {
        PRINTK_ERR("Actel controller timeout\n");
        return -1;
    }

    if ((tmp & actelBitFlashNoAck) != 0)
    {
        PRINTK_ERR("Actel controller no ack\n");
        return -1;
    }

    return 0;
}

static int writeActelFlashCmd(device_context_t * pDevExt, unsigned int cmd, unsigned int addr, uint16_t value)
{
    uint64_t tmp;
    if (waitActelFlashReady(pDevExt) != 0)
    {
        return -1;
    }
    tmp = ((uint64_t)cmd << 60) | ((uint64_t)addr << 32) | (uint64_t)value;
    writeq(tmp, pDevExt->bar0_va + REG_FLASH_CMD);
    return 0;
}

static int flashRead(device_context_t * pDevExt, uint32_t addr, uint32_t len, uint16_t *data)
{
    int l = len / 2;
    int i = 0;

    for (i = 0; i < l; i++)
    {
        writeActelFlashCmd(pDevExt, actelReadFlash, addr + i, 0);
        if (waitActelFlashReady(pDevExt) != 0)
        {
            return -1;
        }
        data[i] = ntohs(readq(pDevExt->bar0_va + REG_FLASH_CMD) & 0xffff);
    }
    return 0;
}

int fpga_readMac(device_context_t * pDevExt, uint8_t mac[MAC_ADDR_LEN])
{
    uint8_t mac1[MAC_ADDR_LEN];
    uint8_t mac2[MAC_ADDR_LEN];
    int count = 10;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    memset(mac1, 0, MAC_ADDR_LEN);
    memset(mac2, 0, MAC_ADDR_LEN);

    do
    {
        flashRead(pDevExt, 0, MAC_ADDR_LEN, (uint16_t*)mac1);
        flashRead(pDevExt, 0, MAC_ADDR_LEN, (uint16_t*)mac2);
    } while (strncmp(mac1, mac2, MAC_ADDR_LEN) && --count);

    memcpy(mac, mac1, MAC_ADDR_LEN);
    //FbDbgDumpMacAddr("Mac from flash: ", mac);
    return count == 0 ? -1 : 0;
}

void setLedOn(device_context_t * pDevExt, unsigned int led_no)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    switch (led_no)
    {
        case 0:
            pDevExt->ledState |= LED_MUX_SW_CH0_LED0 | LED_ON_CH0_LED0;
            break;
        case 1:
            pDevExt->ledState |= LED_MUX_SW_CH0_LED1 | LED_ON_CH0_LED1;
            break;
        case 2:
            pDevExt->ledState |= LED_MUX_SW_CH1_LED0 | LED_ON_CH1_LED0;
            break;
        case 3:
            pDevExt->ledState |= LED_MUX_SW_CH1_LED1 | LED_ON_CH1_LED1;
            break;
    }
    writeq(pDevExt->ledState, pDevExt->bar0_va + REG_LED_CTRL);
}

void setLedOff(device_context_t * pDevExt, unsigned int led_no)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    switch (led_no)
    {
        case 0:
            pDevExt->ledState &= ~(LED_MUX_SW_CH0_LED0 | LED_ON_CH0_LED0);
            break;
        case 1:
            pDevExt->ledState &= ~(LED_MUX_SW_CH0_LED1 | LED_ON_CH0_LED1);
            break;
        case 2:
            pDevExt->ledState &= ~(LED_MUX_SW_CH1_LED0 | LED_ON_CH1_LED0);
            break;
        case 3:
            pDevExt->ledState &= ~(LED_MUX_SW_CH1_LED1 | LED_ON_CH1_LED1);
            break;
    }
    writeq(pDevExt->ledState, pDevExt->bar0_va + REG_LED_CTRL);
}

uint32_t get_consumed_counter(const device_context_t * pDevExt, unsigned int dma_channel_no)
{
    uint32_t consumed_bytes;
    uint32_t reg = FLOWCTRL_BASE + (fb_phys_channel(dma_channel_no) * 0x8);

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " DMA channel %u", dma_channel_no);

#ifndef DMA_CHANNEL_CANARY_UL8_OOB8_UNUSED8_MON8_UDP32_TCP64
#error "get_consumed_counter: DMA channel mapping has changed, code needs to be updated."
#endif
    // Channels UDP and MON do not have Tx (so no need for a consumed counter)
    BUG_ON(dma_channel_no >= FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS && dma_channel_no < FIRST_TCP_CHANNEL);

    consumed_bytes = readl(pDevExt->bar0_va + reg);
    return consumed_bytes;
}

void send_descriptor(const device_context_t * pDevExt, unsigned int dma_channel_no, uint64_t desc)
{
    uint64_t send_reg = SENDREG_NORMAL_BASE + (fb_phys_channel(dma_channel_no) * 0x10);

    //LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

#ifndef DMA_CHANNEL_CANARY_UL8_OOB8_UNUSED8_MON8_UDP32_TCP64
#error "send_descriptor: DMA channel mapping has changed, code needs to be updated."
#endif

    BUG_ON(dma_channel_no >= FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS && dma_channel_no < FIRST_TCP_CHANNEL);

    writeq(desc, pDevExt->bar0_va + send_reg);
}

/**
 *  Configure multicast group in the UDP engine.
 *
 *  @param  from                    A string describing where from this function was called from. Used in logging.
 *  @param  pDevExt                 Pointer to device context.
 *  @param  lane                    Lane to use.
 *  @param  positionIndex           Number of UDP subscription (0, 1-64) to use. Subscription zero is a bypass to NIC.
 *  @param  mac                     Local MAC address to use.
 *  @param  port                    IP port number of the UDP subscription.
 *  @param  vlanTag                 VLAN tag 1-4094 or -1 if VLAN is not used. (VLAN tags 0 and 4095 are reserved!)
 *  @param  channel                 DMA channel (0-31) to use. Also called UDP stream in software documentation.
 *  @param  enableMulticastBypass   Enable multicast bypass.
 *
 *  @return     Zero on success, nonzero on error.
 */
int setSourceMulticast(const char * from, device_context_t * pDevExt, unsigned int lane, unsigned int positionIndex, uint8_t mac[6],
    uint16_t port, int16_t vlanTag, uint16_t channel, bool enableMulticastBypass)
{
    uint64_t value;
    unsigned long flags;

    // start of Ekaline fix
    PRINTK("EKALINE: setSourceMulticast: lane=%u, position %u MAC %02x:%02x:%02x:%02x:%02x:%02x port %u channel %u",
	   lane, positionIndex, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], port, channel);
    
    //    positionIndex += 1;
    positionIndex = (positionIndex+1) & 0x3F;
    // end of ekaline fix

    if (!is_valid_udp_lane(pDevExt, lane))
    {
        return 1;
    }


    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), " lane %u position %u MAC %02x:%02x:%02x:%02x:%02x:%02x port %u channel %u", lane, positionIndex, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], port, channel);

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // MAC address
    value = ((uint64_t)mac[2] << 24) | ((uint64_t)mac[3] << 16) | ((uint64_t)mac[4] << 8) | (mac[5]);
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_MAC_LSB << 8) | (positionIndex)) << 32;
    //    PRINTK("setSrcMac: Idx: %d %02x:%02x:%02x:%02x:%02x:%02x\n", pos, mac[0] , mac[1] , mac[2] , mac[3] , mac[4] , mac[5]);
    writeq(value, pDevExt->bar0_va + SC_ENGINE);
    value = ((uint64_t)mac[0] << 8) | (mac[1]);
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_MAC_MSB << 8) | (positionIndex)) << 32;
    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    // VLAN tag
    value = vlanTag == NO_VLAN_TAG ? 0 : vlanTag;
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_VLAN_TAG << 8) | (positionIndex)) << 32;
    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    // Port
    value = port;
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_PORT << 8) | (positionIndex)) << 32;
    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    // Dma channel (called UDP stream in software documentation!)
    value = fb_phys_channel(channel);
    value |= (lane << 24 | ((uint64_t)UDP_PARAMETER_TYPE_DMA_CHANNEL << 8) | (positionIndex)) << 32;
    writeq(value, pDevExt->bar0_va + SC_ENGINE);

    // Set multicast bypass mode.
    setMulticastBypass(pDevExt, lane, enableMulticastBypass, false);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    if (LOG(LOG_UDP) && (port != 0 || channel != 0 || mac[0] != 0 || mac[1] != 0 || mac[2] != 0 || mac[3] != 0 || mac[4] != 0 || mac[5] != 0))
    {
        MACBUF(1);

        PRINTK("setSourceMulticast(from %s): lane %u, positionIndex %u, mac %s, port %u, channel %u, multicast bypass %s\n",
            from, lane, positionIndex, LogMAC(1, mac), port, channel, enableMulticastBypass ? "enabled" : "disabled");
    }

    return 0;
}

static uint16_t sysmon_read(const device_context_t * pDevExt, uint16_t addr)
{
    uint32_t timeout = 100;
    writeq((addr & 0x7f) << 16, pDevExt->bar0_va + REG_SYSMON);
    //wait for system monitor to be ready
    while (!(readq(pDevExt->bar0_va + REG_SYSMON) & 0x10000) && --timeout)
    {
        udelay(1000);
    }
    return readq(pDevExt->bar0_va + REG_SYSMON) & 0xffff;
}

int readFpgaTemperature(const device_context_t * pDevExt, uint16_t *minTemp, uint16_t *curTemp, uint16_t *maxTemp)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    *curTemp = sysmon_read(pDevExt, 0);
    *minTemp = sysmon_read(pDevExt, 0x24);
    *maxTemp = sysmon_read(pDevExt, 0x20);
    return 0;
}

int getFpgaTime(const device_context_t * pDevExt, uint64_t *rawtime)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    *rawtime = readq(pDevExt->bar0_va + FPGA_TIMER_TIME);
    return 0;
}

int setFpgaTime(const device_context_t * pDevExt)
{
    struct timeval tv;
    uint64_t timer;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    // We try to set the time approximately between to seconds
    do_gettimeofday(&tv);
    if (tv.tv_usec < 500000)
    {
        mdelay((500000 - tv.tv_usec) / 1000);
    }
    else {
        mdelay((500000 + 1000000 - tv.tv_usec) / 1000);
    }
    do_gettimeofday(&tv);
    timer = (uint64_t)tv.tv_usec * 25 * 100ULL | (uint64_t)tv.tv_sec << 32;
    writeq(timer, pDevExt->bar0_va + FPGA_TIMER_TIME);
    return 0;
}

int setTimerDelay(const device_context_t * pDevExt, int32_t value)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    if (value >= 0)
    {
        value = -value;
        writeq((1UL << 31) | (value & 0x7fffffff), pDevExt->bar0_va + FPGA_TIMER_ERROR_INPUT_DELAY);
    }
    else
    {
        writeq(value & 0x7fffffff, pDevExt->bar0_va + FPGA_TIMER_ERROR_INPUT_DELAY);
    }
    return 0;
}

int setClockSyncMode(device_context_t * pDevExt, uint32_t mode)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    switch (mode)
    {
        case TIMECONTROL_HARDWARE:
            // Stop SW time adjust thread and indicate that it is stopped
            kthread_stop(pDevExt->time_adjust_thread);
            pDevExt->time_adjust_thread = NULL;

            // Set HW control on
            writeq(0x1LL << 34, pDevExt->bar0_va + FPGA_TIMER_CONTROL);
            break;

        case TIMECONTROL_SOFTWARE:
            // Set SW control on
            writeq(0x1LL << 35, pDevExt->bar0_va + FPGA_TIMER_CONTROL);

            // Create and start SW time adjust thread
            pDevExt->time_adjust_thread = kthread_run(adjust_cardtime_thread, pDevExt, "fbctadj%d", pDevExt->minor);
            break;

        case TIMECONTROL_SOFTWARE_FPGA_RESET:
            // Set SW control on, do not start or stop the thread because it is already running.
            writeq(0x1LL << 35, pDevExt->bar0_va + FPGA_TIMER_CONTROL);
            break;
    }
    return 0;
}

static int adjust_cardtime_thread(void * pData)
{
    struct timespec ts;
    uint64_t currentTime = 0;
    int64_t diff = 0;
    int64_t val = 0;
    int32_t frac = 0;
    int dir;
    device_context_t * pDevExt = (device_context_t *)pData;
    unsigned int ticksPer100PicoSec = 25;

    while (!kthread_should_stop())
    {
        const int64_t time_integral = pDevExt->time_integral;
        const int64_t time_deltaDiff = pDevExt->time_deltaDiff;

        if (pDevExt->reset_fpga)
        {
            goto dont_do_anything;
        }

        // Get time of day from computer and from network card
        getnstimeofday(&ts);
        currentTime = readq(pDevExt->bar0_va + FPGA_TIMER_TIME);

        // Enable this to compare integer and floating point calculations in older kernels that support floating point.
        // 64-bit integer calculations are more precise because 52-bit mantissa (or fraction in IEEE nomenclature)
        // of double floating point loses 12 bits of accuracy.
        //#define FLOATING_POINT_SUPPORTED
#ifdef FLOATING_POINT_SUPPORTED
        {
            kernel_fpu_begin();
            // Compute difference between the two in nanoseconds
            diff = (ts.tv_sec * 1e9 + ts.tv_nsec) // NB: double loses precision here!!! int64_t can calculate this precisely!
            //diff = (ts.tv_sec * 1000000000ULL + ts.tv_nsec) // NB: double loses precision here!!! int64_t can calculate this precisely!
                - ((currentTime >> 32) * 1e9 + ((currentTime & 0xffffffff) / (ticksPer100PicoSec / 10.0)));

            pDevExt->time_integral += diff;

            val = 0.5 * diff + 0.2 * pDevExt->time_integral + 0.01 * time_deltaDiff;

            if (pDevExt->time_integral > 1e10)
            {
                pDevExt->time_integral = 1e10;
            }
            else if (pDevExt->time_integral < -1e10)
            {
                pDevExt->time_integral = -1e10;
            }

            // Compute amount of clock cycles between each adjustment. If greater than max set to max.
            if (val == 0)
            {
                frac = 0;
            }
            else {
                frac = min((uint64_t)0xfffffffLL, (uint64_t)abs((6.25e6 * ticksPer100PicoSec) / val)) & 0xfffffff;
            }
            kernel_fpu_end();
        }
#endif  // FLOATING_POINT_SUPPORTED

        // Linux kernel version 3.16 and upwards kernels do not allow floating point operations in a driver
        // so we have to do the above computations using only integer arithmetic (which actually results
        // in better precision).
        {
            const int64_t BILLION = 1000000000LL; // Short scale Billion, long scale Milliard, 10^9
            const int64_t TEN_BILLION = 10LL * BILLION;

            int64_t _time_integral = time_integral;
            int64_t _diff, _val;
            int32_t _frac;

            // Compute difference between the two in nanoseconds
            //diff = (ts.tv_sec * 1e9 + ts.tv_nsec)
            //    - ((currentTime >> 32) * 1e9 + ((currentTime & 0xffffffff) / (ticksPer100PicoSec / 10.0)));
            int64_t _diffPart1 = ts.tv_sec * BILLION + ts.tv_nsec; // NB: double loses precision here!!! int64_t can calculate this precisely!
            int64_t _diffPart2 = (currentTime >> 32) * BILLION;
            int64_t _diffPart3 = (currentTime & 0xffffffff) * 10;
            _diffPart3 /= ticksPer100PicoSec;
            _diff = _diffPart1 - (_diffPart2 + _diffPart3);

#ifndef FLOATING_POINT_SUPPORTED
            pDevExt->time_integral += _diff;
#endif
            _time_integral += _diff;

            //val = 0.5 * diff + 0.2 * pDevExt->time_integral + 0.01 * pDevExt->time_deltaDiff;
            _val = 50 * _diff + 20 * _time_integral + time_deltaDiff;
            _val /= 100;

            if (pDevExt->time_integral > TEN_BILLION)
            {
                pDevExt->time_integral = TEN_BILLION;
            }
            else if (pDevExt->time_integral < -TEN_BILLION)
            {
                pDevExt->time_integral = -TEN_BILLION;
            }

            // Compute amount of clock cycles between each adjustment. If greater than max set to max.
            if (_val == 0)
            {
                _frac = 0;
            }
            else
            {
                //frac = min((uint64_t)0xfffffffLL, (uint64_t)abs((6.25e6 * ticksPer100PicoSec) / val) ) & 0xfffffff;
                int32_t roundUp = 0;
                _frac = 6250000 * ticksPer100PicoSec;
                roundUp = _frac % _val >= _val / 2 ? 1 : 0;
                _frac /= _val;
                _frac += roundUp;
                // abs(_frac)
                if (_frac < 0)
                {
                    _frac = -_frac;
                }
                // min(0xfffffff, _frac)
                if (_frac > 0xfffffff)
                {
                    _frac = 0xfffffff;
                }
            }

#ifdef FLOATING_POINT_SUPPORTED
            // Compare floating point and integer calculations:
            {
                static volatile int ok_count = 0;
                static double floatingAverageDiffDiff = 0.0;

                if (frac == _frac && diff == _diff && val == _val)
                {
                    ++ok_count;
                }
                else
                {
                    static volatile bool first = true;
                    static volatile int error_count = 0;
                    char buf[20];
                    const uint32_t bufSize = sizeof(buf);
                    double diffPart1, diffPart2, diffPart3, fracPart;
                    static double n = 0.0;
                    int64_t diff_diff;

                    kernel_fpu_begin();
                    diffPart1 = ts.tv_sec * 1e9 + ts.tv_nsec; // NB: double loses precision here!!! int64_t can calculate this precisely!
                    diffPart2 = (currentTime >> 32) * 1e9;
                    diffPart3 = (currentTime & 0xffffffff) / (ticksPer100PicoSec / 10.0);

                    fracPart = (6.25e6 * ticksPer100PicoSec) / val;
                    kernel_fpu_end();

                    if (first)
                    {
                        PRINTK("TIME: ----------------\n", pDevExt->minor);
                        first = false;
                    }

                    diff_diff = diff - _diff;
                    if (diff_diff < 0)
                    {
                        diff_diff = -diff_diff;
                    }

                    // Average difference between diff and _diff:
                    kernel_fpu_begin();
                    floatingAverageDiffDiff = (floatingAverageDiffDiff * n + diff_diff) / (n + 1);
                    ++n;
                    kernel_fpu_end();

                    PRINTK("TIME: %d ok, %d errors:\n", ok_count, ++error_count);
                    PRINTK("TIME:     diffPart1=%lld,  diffPart2=%lld,  diffPart3=%lld\n", (uint64_t)diffPart1, (uint64_t)diffPart2, (uint64_t)diffPart3);
                    PRINTK("TIME:    _diffPart1=%lld, _diffPart2=%lld, _diffPart3=%lld\n", _diffPart1, _diffPart2, _diffPart3);
                    PRINTK("TIME:     frac=%d,  diff=%lld,  val=%lld, fracPart=%s\n", frac, diff, val, PrDbl(buf, bufSize, fracPart));
                    PRINTK("TIME:    _frac=%d, _diff=%lld, _val=%lld  AverageDiffDiff=%s\n", _frac, _diff, _val, PrDbl(buf, bufSize, floatingAverageDiffDiff));
                    PRINTK("TIME: \n", pDevExt->minor);
                }
            }
#endif
            diff = _diff;
            val = _val;
            frac = _frac;
        }

        // Adjust timer speed up or down?
        dir = val > 0 ? 1 : 0;

        //PRINTK("adjust_cardtime called sec: %li nsec: %li cardsec: %li : cardticks: %li diff: %li\n", ts.tv_sec, ts.tv_nsec/10, currentTime >> 32, (currentTime & 0xffffffff)/25, ts.tv_nsec/10 - (currentTime & 0xffffffff) / 25);
        //PRINTK(" diff %lli deltaDiff %lli integral %lli frac %i dir %d\n", diff, pDevExt->time_deltaDiff, pDevExt->time_integral, frac, dir);

        writeq((1ULL << 35) | (1ULL << 33) | (1ULL << 32) | (dir << 28) | frac, pDevExt->bar0_va + FPGA_TIMER_CONTROL);
        pDevExt->time_deltaDiff = diff - pDevExt->time_lastDiff;
        pDevExt->time_lastDiff = diff;

dont_do_anything:

        // Has something happened since last time we looked?
        if (kthread_should_stop())
            break;

        schedule_timeout_interruptible(msecs_to_jiffies(10));
    }
    return 0;
}

void clearLinkStatus(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    // Clear status flags
    writeq(0x03030303L, pDevExt->bar0_va + FPGA_LINK_STATUS_CLEAR);
    writeq(0x03030303L, pDevExt->bar0_va + FPGA_LINK_STATUS_CLEAR_EXT);
}


static void enable_interrupts(const device_context_t * pDevExt, uint64_t mask0, uint64_t mask1)
{
    /* FPGA only considers set bits. Zero bits are ignored */
    if (mask0 != 0)
    {
        writeq(mask0, pDevExt->bar0_va + IRQ_CTRL_STATUS0);
    }
    if (mask1 != 0)
    {
        writeq(mask1, pDevExt->bar0_va + IRQ_CTRL_STATUS1);
    }
}

int are_interrupts_enabled(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    return (0x1 & readq(pDevExt->bar0_va + IRQ_CTRL_STATUS0));
}

void start_all_oob_interrupts(const device_context_t * pDevExt)
{
    uint64_t intr_mask;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    intr_mask = (1 << INTR_NIC0OOB) | (1 << INTR_NIC1OOB) | (1 << INTR_NIC2OOB) | (1 << INTR_NIC3OOB);
    intr_mask |= (1 << INTR_NIC4OOB) | (1 << INTR_NIC5OOB) | (1 << INTR_NIC6OOB) | (1 << INTR_NIC7OOB);
    enable_interrupts(pDevExt, intr_mask, INTR_NONE);
}

void start_single_oob_interrupt(sc_net_interface_t *nif)
{
    device_context_t * pDevExt = nif->pDevExt;
    uint64_t intr_mask;

    //LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    intr_mask = 1 << (INTR_NIC0OOB + (nif->oob_channel_no - FIRST_OOB_CHANNEL));
    enable_interrupts(pDevExt, intr_mask, INTR_NONE);
}

void stop_all_interrupts(const device_context_t * pDevExt)
{
    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    // This call is unnecessary, writing zero bits into FPGA interrupt registers has no effect!
    enable_interrupts(pDevExt, INTR_NONE, INTR_NONE);
}

void enable_user_logic_interrupts(const device_context_t * pDevExt, uint8_t mask)
{
    uint64_t intr_mask;

    LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    intr_mask = mask;
    enable_interrupts(pDevExt, intr_mask, INTR_NONE);
}

void enable_single_user_logic_interrupt(const device_context_t * pDevExt, unsigned int dmaChannelNumber)
{
    uint64_t intr_mask;

    //LOG_FPGA_FUNCTION(LOG(LOG_FPGA), "");

    intr_mask = 1 << (INTR_UL0 + (dmaChannelNumber - FIRST_UL_CHANNEL));
    enable_interrupts(pDevExt, intr_mask, INTR_NONE);
}



void etlm_read_mac(const device_context_t * pDevExt, uint8_t *mac)
{
    uint32_t lsb24, msb24;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    memset(mac, 0xaa, MAC_ADDR_LEN);

    lsb24 = readl(pDevExt->bar0_va + ETLM_LOCAL_MAC_LSB24);
    msb24 = readl(pDevExt->bar0_va + ETLM_LOCAL_MAC_MSB24);

    //PRINTK("lsb24 0x%06x, msb24 0x%06x\n", lsb24, msb24);
    mac[5] = (lsb24 >> 0) & 0xff;
    mac[4] = (lsb24 >> 8) & 0xff;
    mac[3] = (lsb24 >> 16) & 0xff;
    mac[2] = (msb24 >> 0) & 0xff;
    mac[1] = (msb24 >> 8) & 0xff;
    mac[0] = (msb24 >> 16) & 0xff;
}

int etlm_setup_rx_window(device_context_t * pDevExt, unsigned int conn_id, int win_start, int win_size)
{
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    // Constrainst on start address and size
    if (win_size < 512 || !is_power_of_2(win_size))
    {
        PRINTK_ERR("etlm_setup_rx_window: invalid win size (%d): must be > 512 and power of 2!\n", win_size);
        return -1;
    }

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // EtlmTxWinCtrlConnId
    writel(conn_id, pDevExt->bar0_va + ETLM_CONN_RXID_SEL);
    // EtlmTxWinStartAddr
    writel(win_start, pDevExt->bar0_va + ETLM_CONN_RXWIN_START);
    // EtlmTxWinSize
    writel(win_size, pDevExt->bar0_va + ETLM_CONN_RXWIN_SIZE);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return 0;
}

int etlm_setup_tx_window(device_context_t * pDevExt, unsigned int conn_id, int win_start, int win_size)
{
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    // Constrainst on start address and size
    if (win_size < 512 || !is_power_of_2(win_size))
    {
        PRINTK_ERR("etlm_setup_tx_window: invalid win size (%d): must be > 512 and power of 2!\n", win_size);
        return -1;
    }

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // EtlmTxWinCtrlConnId
    writel(conn_id, pDevExt->bar0_va + ETLM_CONN_TXID_SEL);
    // EtlmTxWinStartAddr
    writel(win_start, pDevExt->bar0_va + ETLM_CONN_TXWIN_START);
    // EtlmTxWinSize
    writel(win_size, pDevExt->bar0_va + ETLM_CONN_TXWIN_SIZE);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return 0;
}

/**
 *  Set ETLM local IP address that will be used for all connections on all TOE channels.
 */
int etlm_setSrcIp(device_context_t * pDevExt, uint32_t ip)
{
    char ipbuf[20];
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    util_format_ip(ipbuf, sizeof(ipbuf), htonl(ip));

    PRINTK_C(LOG(LOG_IP_ADDRESS), "etlm_setSrcIp: %s\n", ipbuf);

    writel(ip, pDevExt->bar0_va + ETLM_LOCAL_IP);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return 0;
}

/**
 *  Set ETLM MAC address that will be used for all connections on all TOE channels.
 */
int etlm_setSrcMac(device_context_t * pDevExt, const uint8_t mac[MAC_ADDR_LEN])
{
    uint32_t value;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), " MAC %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    value = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | (mac[5]);
    writel(value, pDevExt->bar0_va + ETLM_LOCAL_MAC_LSB24);

    if (LOG(LOG_MAC))
    {
        MACBUF(0);
        PRINTK("etlm_setSrcMac: %s\n", LogMAC(0, mac));
    }

    value = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | (mac[2]);
    writel(value, pDevExt->bar0_va + ETLM_LOCAL_MAC_MSB24);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return 0;
}

void etlm_getTimerIntervals(const device_context_t * pDevExt, uint32_t * pSlowTimerIntervalUs, uint32_t * pFastTimerIntervalUs)
{
    static const uint64_t FastTimerUs[] = {
        100 * 1000          /* 100   ms*/,
         12 * 1000 + 500    /*  12.5 ms*/,
          1 * 1000 + 500    /*   1.5 ms*/,
              100           /*   0.1 ms*/,
    };
    uint32_t val;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    val = readl(pDevExt->bar0_va + ETLM_SA_TIMER0_CTRL);

    *pFastTimerIntervalUs = FastTimerUs[(val >> 4) & 0x3];   // Bits 4-5
    *pSlowTimerIntervalUs = (1 << ((val >> 8) & 0xf)) * *pFastTimerIntervalUs;    // Bits 8-11
}

void etlm_getTCPTimers(const device_context_t * pDevExt, uint32_t* retransMinUs, uint32_t* retransMaxUs, uint32_t* persistMinUs, uint32_t* persistMaxUs)
{
    uint32_t val, fastTimerIntervalUs, x;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    etlm_getTimerIntervals(pDevExt, &x, &fastTimerIntervalUs);

    val = readl(pDevExt->bar0_va + ETLM_SA_TIMER2_CTRL);

    *retransMaxUs = ((val >> 24) & 0xff) * fastTimerIntervalUs;
    *retransMinUs = ((val >> 16) & 0xff) * fastTimerIntervalUs;
    *persistMaxUs = ((val >> 8) & 0xff) * fastTimerIntervalUs;
    *persistMinUs = ((val >> 0) & 0xff) * fastTimerIntervalUs;
}

void etlm_setDelayedAck(const device_context_t * pDevExt, bool enable)
{
    uint32_t value;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), " %s", enable ? "enabled" : "disabled");

    value = readl(pDevExt->bar0_va + ETLM_SA_TIMER0_CTRL);
    value = enable ? value | (1U << 17) : value & ~(1U << 17);
    writel(value, pDevExt->bar0_va + ETLM_SA_TIMER0_CTRL);

    value = readl(pDevExt->bar0_va + ETLM_TCPI_DBG_CTRL);
    value = enable ? value & ~(1U << 5) : value | (1U << 5);
    writel(value, pDevExt->bar0_va + ETLM_TCPI_DBG_CTRL);
}

void etlm_setRetransmissionTimeout(const device_context_t * pDevExt, uint32_t rto)
{
    uint32_t val, fastTimerIntervalUs, x, retransMin;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    etlm_getTimerIntervals(pDevExt, &x, &fastTimerIntervalUs);

    val = readl(pDevExt->bar0_va + ETLM_SA_TIMER2_CTRL);

    retransMin = rto / fastTimerIntervalUs;
    if (retransMin > 0xff)
        retransMin = 0xff;
    else if (retransMin == 0)
        retransMin = 1;

    val &= 0xff00ffff;  // Clear the bits
    val |= (retransMin << 16) & 0xff00ffff;

    writel(val, pDevExt->bar0_va + ETLM_SA_TIMER2_CTRL);

    PRINTK("retransmission timeout set to %dus (request was %dus)\n", retransMin*fastTimerIntervalUs, rto);
}

static void etlm_selectTcpConnId_log(device_context_t * pDevExt, unsigned int conn_id, bool log)
{
    if (conn_id >= SC_MAX_TCP_CHANNELS)
    {
        PRINTK_ERR("etlm_selectTcpConnId_log: invalid conn_id %u\n", conn_id);
    }

    assert_spin_locked(&pDevExt->tcpEngineLock);

    writel_log(pDevExt, conn_id, pDevExt->bar0_va + ETLM_SA_TCB_ACC_CMD, log);
    pDevExt->shadow_reg_tcb_cmd_conn_id = conn_id;
}

static void inline etlm_selectTcpConnId(device_context_t * pDevExt, unsigned int conn_id)
{
    etlm_selectTcpConnId_log(pDevExt, conn_id, false);
}

static void etlm_send_oneshot_unlocked(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t val;

    assert_spin_locked(&pDevExt->tcpEngineLock);

    WARN_ON(conn_id > 0xfff);

    val = readl(pDevExt->bar0_va + ETLM_SA_DBG_PROC_CONN);
    val &= ~(0xfff | (1 << 14));
    val |= (conn_id & 0xfff) | (1 << 14); // conn_id and ONE_SHOT
    writel(val, pDevExt->bar0_va + ETLM_SA_DBG_PROC_CONN);
}

static int etlm_getTcpState_unlocked(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t val;
    int state;

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    val = readl(pDevExt->bar0_va + ETLM_SA_TCB_STATE);
    state = (val >> 24) & 0xf;
    return state;
}

int etlm_getTcpState(device_context_t * pDevExt, unsigned int conn_id)
{
    int state;
    unsigned long flags;

    //LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    state = etlm_getTcpState_unlocked(pDevExt, conn_id);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return state;
}

static void etlm_setTcpState_unlocked(device_context_t * pDevExt, unsigned int conn_id, int state)
{
    uint32_t val;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    val = readl(pDevExt->bar0_va + ETLM_SA_TCB_STATE);
    val &= ~(uint32_t)(0xf << 24);
    val |= ((uint32_t)(state & 0xf)) << 24;
    writel(val, pDevExt->bar0_va + ETLM_SA_TCB_STATE);
}

void etlm_setTcpState(device_context_t * pDevExt, unsigned int conn_id, int state)
{
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    etlm_setTcpState_unlocked(pDevExt, conn_id, state);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
}

// Busy waits for the connection to arrive in the specified state, within the timeout period
// return 1 if expected state, 0 if timeout
int etlm_waitForTcpState(device_context_t * pDevExt, unsigned int conn_id, int expectedState, uint32_t utimeout)
{
    const uint32_t DeltaTime = 1000; // 1 millisecond
    uint32_t total_time = 0;
    int state;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    state = etlm_getTcpState(pDevExt, conn_id);
    while (state != expectedState && total_time < utimeout)
    {
        udelay(DeltaTime);
        total_time += DeltaTime;
        state = etlm_getTcpState(pDevExt, conn_id);
    }

    return state == expectedState ? 1 : 0;
}

#if 0 // Not used anymore or anywhere
static void
etlm_setTcpConnSrcMac_unlocked(device_context_t * pDevExt, unsigned int conn_id, uint8_t mac[MAC_ADDR_LEN])
{
    uint32_t mac_lsb32, mac_msb16;

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    // TODO: read values first and only modify necessary bits

    mac_lsb32 = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16)
        | ((uint32_t)mac[4] << 8) | (mac[5]);
    writel(mac_lsb32, pDevExt->bar0_va + ETLM_SA_TCB_SRC_MAC_LSB32);

    mac_msb16 = ((uint32_t)mac[0] << 8) | (mac[1]);
    writel(mac_msb16, pDevExt->bar0_va + ETLM_SA_TCB_SRC_MAC_MSB16);
}

void
etlm_setTcpConnSrcMac(device_context_t * pDevExt, unsigned int conn_id, uint8_t mac[MAC_ADDR_LEN])
{
    unsigned long flags;
    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    etlm_setTcpConnSrcMac_unlocked(pDevExt, conn_id, mac);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
}
#endif

static void etlm_setTcpConnDstMac_unlocked(device_context_t * pDevExt, unsigned int conn_id, uint8_t mac[MAC_ADDR_LEN])
{
    uint32_t mac_lsb32, mac_msb16;

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    mac_lsb32 = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16)
        | ((uint32_t)mac[4] << 8) | (mac[5]);
    writel(mac_lsb32, pDevExt->bar0_va + ETLM_SA_TCB_DST_MAC_LSB32);

    mac_msb16 = ((uint32_t)mac[0] << 8) | (mac[1]);
    writel(mac_msb16, pDevExt->bar0_va + ETLM_SA_TCB_DST_MAC_MSB16);
}

static void inline etlm_setSrcDstPorts_unlocked(device_context_t * pDevExt, unsigned int conn_id, uint16_t src_port, uint16_t dst_port, bool log)
{
    uint32_t ports;

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId_log(pDevExt, conn_id, log);

    ports = (dst_port << 16) | src_port;
    writel_log(pDevExt, ports, pDevExt->bar0_va + ETLM_SA_TCB_PORTS, log);
}

void etlm_setSrcDstPorts(device_context_t * pDevExt, unsigned int conn_id, uint16_t src_port, uint16_t dst_port)
{
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    etlm_setSrcDstPorts_unlocked(pDevExt, conn_id, src_port, dst_port, false);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
}

void etlm_getSrcDstPorts(device_context_t * pDevExt, unsigned int conn_id, uint16_t* src_port, uint16_t* dst_port)
{
    uint32_t ports;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    ports = readl(pDevExt->bar0_va + ETLM_SA_TCB_PORTS);
    *src_port = (ports >> 0) & 0xffff;
    *dst_port = (ports >> 16) & 0xffff;

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
}

/* Next sequence number to send out of tx window (data from up to sndtoseq is to be sent) */
uint32_t etlm_getTcpConnSndNxt(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t snd_nxt;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    snd_nxt = readl(pDevExt->bar0_va + ETLM_SA_TCB_SNDNXT);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return snd_nxt;
}

/* Earliest sequence number not yet acknowledged */
uint32_t etlm_getTcpConnSndUna(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t snd_una;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    snd_una = readl(pDevExt->bar0_va + ETLM_SA_TCB_SNDUNA);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return snd_una;
}

/* Last valid sequence number in the tx window (contains data up to that point) */
uint32_t etlm_getTcpConnSndToSeq(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t snd_seq;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    snd_seq = readl(pDevExt->bar0_va + ETLM_SA_TCB_SNDTOSEQ);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return snd_seq;
}

uint32_t etlm_getTcpConnRcvToSeq(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t rcv_seq;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    rcv_seq = readl(pDevExt->bar0_va + ETLM_SA_TCB_RCVTOSEQ);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return rcv_seq;
}

uint32_t etlm_getRdSeqNum(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t seq_num;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    writel(conn_id, pDevExt->bar0_va + ETLM_CONN_RXID_SEL);

    seq_num = readl(pDevExt->bar0_va + ETLM_CONN_RD_SEQ_NUM);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return seq_num;
}

uint32_t etlm_getRdBytesAcked(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t bytes;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    writel(conn_id, pDevExt->bar0_va + ETLM_CONN_RXID_SEL);

    bytes = readl(pDevExt->bar0_va + ETLM_CONN_RD_SEQ_OFF);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return bytes;
}

uint32_t etlm_getWrSeqNum(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t seq_num;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    writel(conn_id, pDevExt->bar0_va + ETLM_CONN_TXID_SEL);

    seq_num = readl(pDevExt->bar0_va + ETLM_CONN_WR_SEQ_NUM);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return seq_num;
}

uint32_t etlm_getInitialWriteSequenceOffset(device_context_t * pDevExt, unsigned int conn_id)
{
    uint32_t bytes;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    writel(conn_id, pDevExt->bar0_va + ETLM_CONN_TXID_SEL);

    bytes = readl(pDevExt->bar0_va + ETLM_CONN_WR_SEQ_OFF);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return bytes;
}

void etlm_getCapabilities(const device_context_t * pDevExt, uint32_t* num_conn, uint32_t* txPacketSz,
                          uint32_t* rxPacketSz, uint32_t* txTotalWinSz, uint32_t* rxTotalWinSz,
                          bool * vlan_supported)
{
    uint32_t val;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    val = readl(pDevExt->bar0_va + ETLM_CAPABILITIES);

    // values in the register are actually log base 2
    *num_conn = 1 << ((val >> 28) & 0x0f);
    *txPacketSz = 1 << ((val >> 24) & 0x0f);
    *rxPacketSz = 1 << ((val >> 20) & 0x0f);
    *txTotalWinSz = 1 << ((val >> 14) & 0x3f); // Maximum design size, implemented size might be less
    *rxTotalWinSz = 1 << ((val >> 8) & 0x3f); // Maximum design size, implemented size might be less
    *vlan_supported = ((val >> 0) & 0x01);
}

int etlm_conn_close(device_context_t * pDevExt, unsigned int conn_id)
{
    int retval = 0;
    unsigned long flags;
    uint32_t val;
    int state;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    state = etlm_getTcpState_unlocked(pDevExt, conn_id);
    switch (state)
    {
        case ETLM_TCP_STATE_ESTABLISHED:
        case ETLM_TCP_STATE_CLOSE_WAIT:
            // Recipe from 12.2.4: Close Connection
            // Send FIN
            val = readl(pDevExt->bar0_va + ETLM_SA_TCB_FLSNDFINREQSW);
            val |= 1 << 19;
            writel(val, pDevExt->bar0_va + ETLM_SA_TCB_FLSNDFINREQSW);

            etlm_send_oneshot_unlocked(pDevExt, conn_id);

            // FIXME make sure to read all data on the connection otherwise the state will stay in ETLM_TCP_STATE_CLOSED_DATA_PENDING
            // FIXME here the procedure states that we should actually drain the Rx window.
            spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

            if (!etlm_waitForTcpState(pDevExt, conn_id, ETLM_TCP_STATE_CLOSED, 2 * 1000 * 1000/*us*/))
            {
                int state = etlm_getTcpState(pDevExt, conn_id);

                PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION), "etlm tcp conn %d not going to CLOSED state (state is %s)\n", conn_id + FIRST_TCP_CHANNEL, EtlmState(state));

                retval = -1;
            }

            spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);
            /*
                        int cnt = 100;
                        while(cnt && (state = etlm_getTcpState_unlocked(pDev, conn_id) ) != ETLM_TCP_STATE_CLOSED)
                        {
                            udelay(10);
                        }
                        if (state != ETLM_TCP_STATE_CLOSED)
                            //PRINTK_WARN("etlm tcp conn %d not going to CLOSED state. (state is %s)\n", conn_id, EtlmState(state));
            */
            if (retval == 0 && LOG(LOG_ETLM_TCP_CONNECTION))
            {
                PRINTK("etlm connection on channel %u successfully closed\n", conn_id + FIRST_TCP_CHANNEL);
            }
            break;

        case ETLM_TCP_STATE_LISTEN:
            // Recipe from 12.2.2: Stop a Listening Connection
            etlm_setTcpState_unlocked(pDevExt, conn_id, ETLM_TCP_STATE_CLEAR_TCB);

            etlm_send_oneshot_unlocked(pDevExt, conn_id);
            break;

        case ETLM_TCP_STATE_CLOSED:
            PRINTK("etlm tcp channel %d already in CLOSED state.\n", conn_id + FIRST_TCP_CHANNEL);
            break;

        default:
            //if (LOG(LOG_WARNING))
            {
                PRINTK_WARN("etlm tcp channel %d not in ESTABLISHED, CLOSE_WAIT, CLOSED or LISTEN state (state is %s). Cannot close gracefully!\n",
                    conn_id + FIRST_TCP_CHANNEL, EtlmState(state));
            }
            retval = -1;
            break;
    }

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);
    return retval;
}


/*
    12.2.7 Abort Connection or other ETLM state.

    1. Tell the ETLM which connection to access by applying the connection ID:
    BG_WRITE SaTcbAccCmd.SaTcbSelConnId $connection_id
    2. Set flag FLSndRst, forcing the ETLM to send out a RST:
    BG_WRITE Tcb.FlSndRst 1
    3. Tell the ETLM to process the changed values by issuing a Process Connection command:
    BG_WRITE SaDbgProcConn.SaProcConnId $connection_id
    BG_WRITE SaDbgProcConn.SaProcConnOneShot 1
 */
void etlm_abort_conn(device_context_t * pDevExt, unsigned int conn_id)
{
    unsigned long flags;
    uint32_t val;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    val = readl(pDevExt->bar0_va + ETLM_SA_TCB_FLSNDRST);
    val |= 1 << 23;
    writel(val, pDevExt->bar0_va + ETLM_SA_TCB_FLSNDRST);

    etlm_send_oneshot_unlocked(pDevExt, conn_id);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION), "Connection (id %u) aborted\n", conn_id);
}

/*
C.2 Connect to a Server - VLAN (Active Open):

1. Tell the ETLM which connection to access by applying the connection ID:
BG_WRITE SaTcbAccCmd.SaTcbSelConnId $connection_id
2. Get connection state:
BG_READ Tcb.State
3. Test that state is CLOSED
4. Write Source Port:
BG_WRITE Tcb.L23LocalPort $local_port
5. Write Destination Port:
BG_WRITE Tcb.L23RemotePort $remote_port
6. Write Destination IP Address:
BG_WRITE Tcb.L23RemoteIp $remote_ip
7. Write Destination MAC Address:
BG_WRITE Tcb.L23RemoteMacAddrLow ($remote_mac & 0xffffffff)
BG_WRITE Tcb.L23RemoteMacAddrHigh ($remote_mac >> 32) & 0xffff

8. If local Ip address for this connection is not the one set in the EtlmLocIpAddr register then set it now:
BG_WRITE Tcb.L23LocalIp $LocalIpAddr
9. Enable VALN for this connection:
BG_WRITE Tcb.L23VlanInsQtag 1
10. Set the VLAN tag for this connection:
BG_WRITE Tcb.L23VlanQtag $VlanTag

11. Set into state SYN_SENT:
BG_WRITE Tcb.State 2
12. Ask for a transmission:
BG_WRITE Tcb.FlAckNow 1
13. Tell the ETLM to process the changed values by issuing a Process Connection command:
BG_WRITE SaDbgProcConn.SaProcConnId $connection_id
BG_WRITE SaDbgProcConn.SaProcConnOneShot 1
 */
int etlm_tcp_connect(device_context_t * pDevExt, unsigned int conn_id, uint16_t src_port, uint16_t dst_port, uint32_t dst_ip, uint8_t mac[MAC_ADDR_LEN],
    uint32_t local_ip_address, int16_t vlan_tag)
{
    uint32_t val;
    unsigned long flags;
    int state;
    bool log = LOG(LOG_ETLM_TCP_CONNECTION_REG_W);

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    if (unlikely(log))
    {
        IPBUF(1); MACBUF(2);
        PRINTK("etlm_tcp_connect: reg w conn_id=%u, src_port=%u, dst_port=%u, dst_ip=%s, mac=%s\n",
            conn_id, src_port, dst_port, LogIP(1, dst_ip), LogMAC(2, mac));
    }

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Step 1 and 2:
    state = etlm_getTcpState_unlocked(pDevExt, conn_id);

    // Step 3:
    if (state != ETLM_TCP_STATE_CLOSED)
    {
        spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

        PRINTK_ERR("Connection %d not in closed (0) state but in state %d\n", conn_id, state);

        return -1;
    }

    // Steps 4 and 5:
    etlm_setSrcDstPorts_unlocked(pDevExt, conn_id, src_port, dst_port, log);

    // Step 6:
    writel(dst_ip, pDevExt->bar0_va + ETLM_SA_TCB_DST_IP);

    // Step 7:
    etlm_setTcpConnDstMac_unlocked(pDevExt, conn_id, mac);

    if (local_ip_address != 0)
    {
        // Step 8 from C.4 Connect to a Server - Individual IP Addr (Active Open)
        // Normal connection (no VLAN) but a per connection IP address is specified.
        // Uses EtlmLocIpAddr if not set: fbconfig calls SC_SetLocalAddress which sets local IP address (EtlmLocIpAddr)
        writel_log(pDevExt, local_ip_address, pDevExt->bar0_va + ETLM_SA_TCB_SRC_IP, log);
    }

    if (vlan_tag > 0 && vlan_tag < 4095) // tags 0 and 4095 are reserved
    {
        // Setup connection via VLAN:

        // Step 8:
        // Already done above if applicable

        // Step 9:
        val = readl(pDevExt->bar0_va + ETLM_SA_TCB_L23VLANINSQTAG);
        val |= 1 << 23;
        writel_log(pDevExt, val, pDevExt->bar0_va + ETLM_SA_TCB_L23VLANINSQTAG, log);

        // Step 10:
        val = readl(pDevExt->bar0_va + ETLM_SA_TCB_L23VLANQTAG);
        val |= vlan_tag << 16;
        writel_log(pDevExt, val, pDevExt->bar0_va + ETLM_SA_TCB_L23VLANQTAG, log);
    }

    // Step 11:
    etlm_setTcpState_unlocked(pDevExt, conn_id, ETLM_TCP_STATE_SYN_SENT);

    // Step 12:
    val = readl(pDevExt->bar0_va + ETLM_SA_TCB_FLACKNOW);
    val |= 1 << 30;
    writel_log(pDevExt, val, pDevExt->bar0_va + ETLM_SA_TCB_FLACKNOW, log);

    // Step 13:
    etlm_send_oneshot_unlocked(pDevExt, conn_id);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    PRINTK_C(LOG(LOG_ETLM_TCP_CONNECTION), "Connect requested (id %u)\n", conn_id);

    return 0;
}


/*
C.1 Start Listening for Incoming Connections - VLAN (Passive Open)

1. Tell the ETLM which connection to access by applying the connection ID:
BG_WRITE SaTcbAccCmd.SaTcbSelConnId $connection_id
2. Get connection state:
BG_READ Tcb.State
3. Test that state is CLOSED
4. Write port number:
BG_WRITE Tcb.L23LocalPort $local_port

5. If local Ip address for this connection is not the one set in the EtlmLocIpAddr register then set it now:
BG_WRITE Tcb.L23LocalIp $LocalIpAddr
6. Enable VALN for this connection:
BG_WRITE Tcb.L23VlanInsQtag 1
7. Set the VLAN tag for this connection:
BG_WRITE Tcb.L23VlanQtag $VlanTag

8. Set into state LISTEN:
BG_WRITE Tcb.State 1
9. Tell the ETLM to process the changed values by issuing a Process Connection command:
BG_WRITE SaDbgProcConn.SaProcConnId $connection_id
BG_WRITE SaDbgProcConn.SaProcConnOneShot 1
 */
int etlm_tcp_listen(device_context_t * pDevExt, unsigned int conn_id, int port, uint32_t local_ip_address, int16_t vlan_tag)
{
    unsigned long flags;
    int state;
    bool log = LOG(LOG_ETLM_TCP_CONNECTION_REG_W);

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Step 1 and 2:
    state = etlm_getTcpState_unlocked(pDevExt, conn_id);

    // Step 3:
    if (state != ETLM_TCP_STATE_CLOSED)
    {
        spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

        PRINTK_ERR("connection %d not in closed (%d) state but in state %d\n", conn_id, ETLM_TCP_STATE_CLOSED, state);

        return -1;
    }

    // Step 4:
    etlm_setSrcDstPorts_unlocked(pDevExt, conn_id, port, 0 /*dst port*/, log);

    // Step 5:
    if (local_ip_address != 0)
    {
        // Step 5 from C.3 Start Listening for Incoming Connections - Individual IP Addr (Passive Open)
        // Normal connection (no VLAN) but a per connection IP address is specified.
        // Uses EtlmLocIpAddr if not set: fbconfig calls SC_SetLocalAddress which sets local IP address (EtlmLocIpAddr)
        writel_log(pDevExt, local_ip_address, pDevExt->bar0_va + ETLM_SA_TCB_SRC_IP, log);
    }

    if (vlan_tag > 0 && vlan_tag < 4095) // tags 0 and 4095 are reserved
    {
        // Listen on VLAN:

        // Step 5:
        // Already done above if applicable

        // Step 6:
        uint32_t val = readl(pDevExt->bar0_va + ETLM_SA_TCB_L23VLANINSQTAG);
        val |= 1 << 23;
        writel_log(pDevExt, val, pDevExt->bar0_va + ETLM_SA_TCB_L23VLANINSQTAG, log);

        // Step 7:
        val = readl(pDevExt->bar0_va + ETLM_SA_TCB_L23VLANQTAG);
        val |= vlan_tag << 16;
        writel_log(pDevExt, val, pDevExt->bar0_va + ETLM_SA_TCB_L23VLANQTAG, log);
    }

    // Step 8:
    etlm_setTcpState_unlocked(pDevExt, conn_id, ETLM_TCP_STATE_LISTEN);

    // Step 9:
    etlm_send_oneshot_unlocked(pDevExt, conn_id);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    PRINTK_C(unlikely(log), "connection %d listening on port %d\n", conn_id, port);

    return 0;
}

/*
12.2.2 Stop a Listening Connection
1. Tell the ETLM which connection to access by applying the connection ID:
BG_WRITE SaTcbAccCmd.SaTcbSelConnId $connection_id
2. Get connection state:
BG_READ Tcb.State
3. Test that state is LISTEN:
4. Set into state CLEAR_TCB:
BG_WRITE Tcb.State 11
5. Tell the ETLM to process the changed values by issuing a Process Connection command:
BG_WRITE SaDbgProcConn.SaProcConnId $connection_id
BG_WRITE SaDbgProcConn.SaProcConnOneShot 1
 */
int etlm_tcp_unlisten(device_context_t * pDevExt, unsigned int connection_id)
{
    unsigned long flags;
    int state;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Step 1 and 2:
    state = etlm_getTcpState_unlocked(pDevExt, connection_id);

    // Step 3:
    if (state != ETLM_TCP_STATE_LISTEN)
    {
        spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

        PRINTK_ERR("connection %d not in listen (%d) state but in state %d\n", connection_id, ETLM_TCP_STATE_LISTEN, state);

        return -1;
    }

    // Step 4:
    etlm_setTcpState_unlocked(pDevExt, connection_id, ETLM_TCP_STATE_CLEAR_TCB);

    // Step 5:
    etlm_send_oneshot_unlocked(pDevExt, connection_id);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    return 0;
}

uint32_t etlm_read_debug_assertions(const device_context_t * pDevExt)
{
    //LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    uint32_t value = readl(pDevExt->bar0_va + ETLM_DBG_ASSERTIONS);

    return value;
}


void etlm_getRemoteIpPortMac(device_context_t * pDevExt, unsigned int conn_id, uint32_t * pIpAddress, uint16_t * pPort, uint8_t macAddress[MAC_ADDR_LEN])
{
    uint32_t macAddressLow, macAddressHigh, ports;
    unsigned long flags;

    LOG_ETLM_FUNCTION(LOG(LOG_ETLM), "");

    spin_lock_irqsave(&pDevExt->tcpEngineLock, flags);

    // Select which Tcp conn to query/modify
    etlm_selectTcpConnId(pDevExt, conn_id);

    *pIpAddress = readl(pDevExt->bar0_va + ETLM_SA_TCB_DST_IP);

    ports = readl(pDevExt->bar0_va + ETLM_SA_TCB_PORTS);

    macAddressLow = readl(pDevExt->bar0_va + ETLM_SA_TCB_DST_MAC_LSB32);
    macAddressHigh = readl(pDevExt->bar0_va + ETLM_SA_TCB_DST_MAC_MSB16);

    spin_unlock_irqrestore(&pDevExt->tcpEngineLock, flags);

    *pIpAddress = ntohl(*pIpAddress);

    *pPort = (ports >> 16) & 0xffff;

    macAddressHigh &= 0xFFFF;

    macAddress[0] = (uint8_t)(macAddressHigh >> 8);
    macAddress[1] = (uint8_t)(macAddressHigh);
    macAddress[2] = (uint8_t)(macAddressLow >> 24);
    macAddress[3] = (uint8_t)(macAddressLow >> 16);
    macAddress[4] = (uint8_t)(macAddressLow >> 8);
    macAddress[5] = (uint8_t)(macAddressLow);
}
