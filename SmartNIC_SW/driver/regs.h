#ifndef REGS_H
#define REGS_H

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

/**
 *  References below to @ref SNSI refer to internal Silicom Denmark Software Interfaces document.
 */

/* PCIe BAR 0: */
#define FPGA_BASE_OLD                   (0x0)
#define FPGA_REVISION                   (FPGA_BASE_OLD + 0x0000)
#define FPGA_RESET                      (FPGA_BASE_OLD + 0x0008)
#define REG_PCIE_CONFIG                 (FPGA_BASE_OLD + 0x0010)
#define REG_LED_CTRL                    (FPGA_BASE_OLD + 0x0018)
#define REG_SYSMON                      (FPGA_BASE_OLD + 0x0020)
#define REG_FLASH_CMD                   (FPGA_BASE_OLD + 0x0040)
#define REG_FLASH_RD                    (FPGA_BASE_OLD + 0x0050)

#define DEVICE_CONFIG                   (FPGA_BASE_OLD + 0x00A0)

#define FPGA_TIMER_TIME                 (FPGA_BASE_OLD + 0x0100)    /**< (R/W)  TimerTime */
#define FPGA_TIMER_CONTROL              (FPGA_BASE_OLD + 0x0108)    /**< (W)    TimerControl */
#define FPGA_TIMER_LEAP                 (FPGA_BASE_OLD + 0x0110)    /**< (W)    TimerLeap */
#define FPGA_TIMER_ERROR_INPUT_DELAY    (FPGA_BASE_OLD + 0x0118)    /**< (R/W)  TimerError / InputDelay */

#define FPGA_LINK_STATUS_CLEAR          (FPGA_BASE_OLD + 0x01B0)    /**< (W)    Link Status Clear ports 0-3 */
#define FPGA_LINK_STATUS_CLEAR_EXT      (FPGA_BASE_OLD + 0x01B8)    /**< (W)    Link Status Clear ports 4-7 */

#define FPGA_PANIC                      (FPGA_BASE_OLD + 0x0180)
#define FPGA_DISABLE_GLOBAL_PANIC       (FPGA_BASE_OLD + 0x0188)

#define TX_DMA_FIFO_FILL_LEVEL          (FPGA_BASE_OLD + 0x0200)    /**< (R) Tx DMA request FIFO fill level (@ref SNSI 8.2) */

#define IRQ_CTRL_STATUS0                (FPGA_BASE_OLD + 0x02f0)
#define IRQ_CTRL_STATUS1                (FPGA_BASE_OLD + 0x02f8)

#define SC_ENGINE                       (FPGA_BASE_OLD + 0x300)

#define STATUS_DMA_START                (FPGA_BASE_OLD + 0x0420)
#define STATUS_DMA_ATTR                 (FPGA_BASE_OLD + 0x0438)

#define STATISTIC_DMA_START             (FPGA_BASE_OLD + 0x0440)
#define STATISTIC_DMA_END               (FPGA_BASE_OLD + 0x0448)
#define STATISTIC_RD_MARK               (FPGA_BASE_OLD + 0x0450)
#define STATISTIC_ATTR                  (FPGA_BASE_OLD + 0x0458)

#define FPGA_PL_DMA_START_ADDRESS       (FPGA_BASE_OLD + 0x0480)    /**< (W)    Pointer List DMA start address */
#define FPGA_PL_DMA_ATTRIBUTES          (FPGA_BASE_OLD + 0x0498)    /**< (W)    Pointer List DMA attributes */

#define REG_MMU                         (FPGA_BASE_OLD + 0x04a0)    /**< (W)    Memory Management Unit - MMU. */

#define UL_READ_BASE                    (FPGA_BASE_OLD + 0x0600)

#define PKG_DMA_REGS                    (FPGA_BASE_OLD + 0x1000)    /**< Start of Rx DMA registers. */
#define PKG_RXDMA_REGS                  (FPGA_BASE_OLD + 0x1000)    /**< Start of Rx DMA registers. */

#define SENDREG_NORMAL_BASE             (FPGA_BASE_OLD + 0x3000)    /**< Start of send registers, 0x3010 is next channel etc (offset 0x10 between channels) */
#define SENDREG_PRIORITY_BASE           (FPGA_BASE_OLD + 0x3008)    /**< Start of priority send registers, 0x3018 is next channel etc (offset 0x10 between channels) */
#define SENDREG_OFFSET                  0x10                        /**< Offset between send registers from one channel to the next (both normal and priority send) */

#define FLOWCTRL_BASE                   (FPGA_BASE_OLD + 0x3800)    /**< Fill level [0x3800-0x3bf8] or [0x3800-0x3c00) */
                                                                    /**< Combined TcbSndUna + Fill Level for TCP channels [0x3a00-0x3c00) */
#define FLOWCTRL_SIZE                   0x400

// Setting up Host Rx DMAs on MCDMA (Multi-Channel DMA)
#define PKG_RXDMA_OFFSET    0x40
#define PKG_RXDMA_START     0x00
#define PKG_RXDMA_END       0x08
#define PKG_RXDMA_RD_MARK   0x10
#define PKG_RXDMA_ATTR      0x18
#define PKG_RXDMA_ATTR_2    0x20
#define PKG_RXDMA_PLDMA     0x28


// used for setting up MCDMA and ring_buffers
#define FB_RXDMA_REG(offset, reg) ((PKG_RXDMA_REGS) + (PKG_RXDMA_OFFSET) * (offset) + (reg))

/* ETLM registers */
#define ETLM_BASE                       (0x80000)

/* 16.2 Block TCPI */
#define ETLM_TCPI_DBG_CTRL              (ETLM_BASE + 0x504)    /* 16.2.2 TcpiDbgCtrl register */

/* 16.3.1 SaTimerCtrl0 register */
#define ETLM_SA_TIMER0_CTRL             (ETLM_BASE + 0x904)
#define ETLM_SA_TIMER1_CTRL             (ETLM_BASE + 0x908)
#define ETLM_SA_TIMER2_CTRL             (ETLM_BASE + 0x90c)

/* 16.3.5 SaTcbAccCmd register */
#define ETLM_SA_TCB_ACC_CMD             (ETLM_BASE + 0x944)

#define ETLM_SA_DBG_PROC_CONN           (ETLM_BASE + 0x958)

/* 16.3.9 SaTcbAccData[Idx] register */
#define ETLM_SA_TCB_BASE                (ETLM_BASE + 0x980)
#define ETLM_SA_TCB_SNDNXT              (ETLM_SA_TCB_BASE + 0x0004)
#define ETLM_SA_TCB_SNDUNA              (ETLM_SA_TCB_BASE + 0x0008)
#define ETLM_SA_TCB_SNDTOSEQ            (ETLM_SA_TCB_BASE + 0x000c)
#define ETLM_SA_TCB_RCVTOSEQ            (ETLM_SA_TCB_BASE + 0x0010)
#define ETLM_SA_TCB_FLSNDRST            (ETLM_SA_TCB_BASE + 0x0018)
#define ETLM_SA_TCB_FLACKNOW            (ETLM_SA_TCB_BASE + 0x001c)
#define ETLM_SA_TCB_STATE               (ETLM_SA_TCB_BASE + 0x0024)
#define ETLM_SA_TCB_DST_MAC_LSB32       (ETLM_SA_TCB_BASE + 0x0068)
#define ETLM_SA_TCB_DST_MAC_MSB16       (ETLM_SA_TCB_BASE + 0x006c)
#define ETLM_SA_TCB_L23VLANQTAG         (ETLM_SA_TCB_BASE + 0x006c)
#define ETLM_SA_TCB_DST_IP              (ETLM_SA_TCB_BASE + 0x0070)
#define ETLM_SA_TCB_SRC_IP              (ETLM_SA_TCB_BASE + 0x0074)     /* 11.3: Tcb.L23LocalIp (optional per connection local IP address) */
#define ETLM_SA_TCB_PORTS               (ETLM_SA_TCB_BASE + 0x0078)     /* 11.3: Tcb.L23LocalPort + Tcb.L23RemotePort */
#define ETLM_SA_TCB_FLSNDFINREQSW       (ETLM_SA_TCB_BASE + 0x007c)
#define ETLM_SA_TCB_L23VLANINSQTAG      (ETLM_SA_TCB_BASE + 0x007c)
#define ETLM_SA_TCB_SRC_MAC_LSB32       (ETLM_SA_TCB_BASE + 0x0080)
#define ETLM_SA_TCB_SRC_MAC_MSB16       (ETLM_SA_TCB_BASE + 0x0084)

#define ETLM_CORE_BASE                  (ETLM_BASE + 0x50000)

#define ETLM_LOCAL_MAC_LSB24            (ETLM_CORE_BASE + 0x0000)       /* 11.3: EtlmLocMacAddrLsb */
#define ETLM_LOCAL_MAC_MSB24            (ETLM_CORE_BASE + 0x0004)       /* 11.3: EtlmLocMacAddrMsb */
#define ETLM_LOCAL_IP                   (ETLM_CORE_BASE + 0x0008)       /* 11.3: EtlmLocIpAddr */
#define ETLM_CAPABILITIES               (ETLM_CORE_BASE + 0x0014)
/* Undocumented! */
#define ETLM_DBG_ASSERTIONS             (ETLM_CORE_BASE + 0x0028)       /* Read panic bits from ETLM */


/* 16.5 Block ETLM_RX */
#define ETLM_RX_CTRL                    (ETLM_CORE_BASE + 0x0100)
#define ETLM_CONN_RXID_SEL              (ETLM_CORE_BASE + 0x0104)
#define ETLM_CONN_RXWIN_START           (ETLM_CORE_BASE + 0x0108)
#define ETLM_CONN_RXWIN_SIZE            (ETLM_CORE_BASE + 0x010c)
#define ETLM_CONN_RD_SEQ_NUM            (ETLM_CORE_BASE + 0x01b8)
#define ETLM_CONN_RD_SEQ_OFF            (ETLM_CORE_BASE + 0x01bc)

/* 16.6 Block ETLM_TX */
#define ETLM_CONN_TXID_SEL              (ETLM_CORE_BASE + 0x0204)
#define ETLM_CONN_TXWIN_START           (ETLM_CORE_BASE + 0x0208)
#define ETLM_CONN_TXWIN_SIZE            (ETLM_CORE_BASE + 0x020c)
#define ETLM_CONN_WR_SEQ_NUM            (ETLM_CORE_BASE + 0x02ac)
#define ETLM_CONN_WR_SEQ_OFF            (ETLM_CORE_BASE + 0x02b0)   /* 16.6.12, readonly, EtlmTxFifoCtrlSndSeqOff */


/*
   PCIe BAR 2:
   Whole BAR 2 is used for 64-bit user logic registers
*/

#endif /* REGS_H */
