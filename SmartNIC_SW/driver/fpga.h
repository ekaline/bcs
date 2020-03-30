#ifndef FPGA_H
#define FPGA_H

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

// ETLM TCP states
#define ETLM_TCP_STATE_CLOSED                    0x00
#define ETLM_TCP_STATE_LISTEN                    0x01
#define ETLM_TCP_STATE_SYN_SENT                  0x02
#define ETLM_TCP_STATE_SYN_RCVD                  0x03
#define ETLM_TCP_STATE_ESTABLISHED               0x04
#define ETLM_TCP_STATE_CLOSE_WAIT                0x05
#define ETLM_TCP_STATE_FIN_WAIT1                 0x06
#define ETLM_TCP_STATE_CLOSING                   0x07
#define ETLM_TCP_STATE_LAST_ACK                  0x08
#define ETLM_TCP_STATE_FIN_WAIT2                 0x09
#define ETLM_TCP_STATE_TIME_WAIT                 0x0a
#define ETLM_TCP_STATE_CLEAR_TCB                 0x0b
#define ETLM_TCP_STATE_NONE                      0x0c // Not used
#define ETLM_TCP_STATE_CLOSED_DATA_PENDING       0x0d

const char * EtlmState(int etlmState);

// Map from internal number to dma number (could be removed now)
static inline unsigned int fb_phys_channel(unsigned int chan)
{
#ifndef DMA_CHANNEL_CANARY_UL8_OOB8_UNUSED8_MON8_UDP32_TCP64
    #error "fb_phys_channel: DMA channel mapping has changed, code needs review."
#endif
    return chan;
}

const char * IsTestFpga(uint16_t fpgaType);
const char * IsLeonbergFpga(uint16_t fpgaType);
const char * FpgaName(uint16_t fpgaType);

uint64_t getDeviceConfig(const device_context_t * pDevExt);
uint64_t getFpgaVersion(const device_context_t * pDevExt);
void getFeatures(const device_context_t * pDevExt, bool * pGroupPLDMA, bool * pExtendedRegisterMode,
    bool * pBulkSupported, bool * pTxDmaSupport, bool * pMmuSupport, bool * pWarningSupport);
uint64_t getFpgaBuildTime(const device_context_t * pDevExt);
uint32_t getFpgaExtendedBuildNumber(const device_context_t * pDevExt);
void getFpgaAclQsfpMonitorConfiguration(const device_context_t * pDevExt, uint16_t * pAclLaneEnabled, uint16_t * pQspfLaneEnabled, uint16_t * pMonitorLaneEnabled, bool * pStatisticsModuleIsPresent);
void getFpgaToeWindowMemorySizes(const device_context_t * pDevExt, uint64_t * pTxWindowSize, uint64_t * pRxWindowSize, bool * pToeIsConnectedToUserLogic, uint64_t * pEvaluationModes);

int fpga_reset(const device_context_t * pDevExt);

int readFpgaTemperature(const device_context_t * pDevExt, uint16_t *minTemp, uint16_t *curTemp, uint16_t *maxTemp) ;

uint64_t getPanic(const device_context_t * pDevExt);

bool set_mmu(const device_context_t * pDevExt, uint64_t virtualAddress, uint64_t busAddress);
void enable_mmu(const device_context_t * pDevExt, bool enable);

void set_rx_dma_read_mark(const device_context_t * pDevExt, int channelNo, const uint8_t* readPtr);
void set_dma_read_mark_offset(const device_context_t * pDevExt, int channelNo, uint32_t va);
void enable_pldma(const device_context_t * pDevExt, int enabled);
void setup_pl_dma(const device_context_t * pDevExt);
int start_dma(device_context_t * pDevExt, unsigned int dma_channel_no, uint64_t prbSize);
int start_rx_dma_address(const device_context_t * pDevExt, unsigned int dma_channel_no, dma_addr_t prbPhysicalAddress, uint64_t prbSize, bool initializeChannel);
void setup_external_group_rx_pl_dma(const device_context_t * pDevExt, unsigned channelNumber, uint64_t plDmaGroupPhysicalAddress, bool enable);
void enable_group_rx_pl_dma(const device_context_t * pDevExt, unsigned group, int64_t flushPacketCount, int64_t flushTimeoutInNs, bool enabled);
int stop_dma(device_context_t * pDevExt, unsigned int dma_channel_no);
int setup_status_dma(const device_context_t * pDevExt);
int enable_status_dma(const device_context_t * pDevExt, int enabled);
void get_channel_fillings(const device_context_t * pDevExt, int dma_channel_no, uint16_t* cur_filling, uint16_t* peak_filling);
int setup_statistics_dma(const device_context_t * pDevExt);
int enable_statistic_dma(device_context_t * pDevExt, int enabled, sc_filling_scale_t fillingScale, sc_time_mode_t timeMode);
int set_statistics_read_mark(const device_context_t * pDevExt, uint64_t addr);

int setSourceMac(device_context_t * pDevExt, unsigned int lane, uint8_t mac[6]);
//int setDestinationMac(device_context_t * pDevExt, unsigned int channel_no, uint8_t mac[6]);
//int setWindowSize(device_context_t * pDevExt, unsigned int channel_no, uint32_t winSize);
//int setInitialSequence(device_context_t * pDevExt, unsigned int channel_no, uint32_t seqNo);

// Rto is in microseconds
//int setRetransmissionTimeout(device_context_t * pDevExt, unsigned int channel_no, uint32_t rto);

int setBypass(device_context_t * pDevExt, unsigned int lane, uint32_t enable);
int setPromisc(device_context_t * pDevExt, unsigned int lane, uint32_t enable);
int fpga_readMac(device_context_t * pDevExt, uint8_t mac[6]);
void setLedOn(device_context_t * pDevExt, unsigned int led_no);
void setLedOff(device_context_t * pDevExt, unsigned int led_no);

uint32_t get_consumed_counter(const device_context_t * pDevExt, unsigned int dma_channel_no);
void send_descriptor(const device_context_t * pDevExt, unsigned int dma_channel_no, uint64_t desc);

int setSourceMulticast(const char * from, device_context_t * pDevExt, unsigned int lane, unsigned int positionIndex, uint8_t mac[6],
    uint16_t port, int16_t vlanTag, uint16_t channel, bool enableMulticastBypass);

int getFpgaTime(const device_context_t * pDevExt, uint64_t *rawtime);
int setFpgaTime(const device_context_t * pDevExt);
int setTimerDelay(const device_context_t * pDevExt, int32_t value);
int setClockSyncMode(device_context_t * pDevExt, uint32_t mode);
void clearLinkStatus(const device_context_t * pDevExt);

enum InterruptFlags {
#ifndef DMA_CHANNEL_CANARY_UL8_OOB8_UNUSED8_MON8_UDP32_TCP64
    #error "Channel number allocation has changed, enum InterruptFlags must be revalidated"
#endif
    // User Logic interrupts
    INTR_UL0 = FIRST_UL_CHANNEL + 0,
    INTR_UL1 = FIRST_UL_CHANNEL + 1,
    INTR_UL2 = FIRST_UL_CHANNEL + 2,
    INTR_UL3 = FIRST_UL_CHANNEL + 3,
    INTR_UL4 = FIRST_UL_CHANNEL + 4,
    INTR_UL5 = FIRST_UL_CHANNEL + 5,
    INTR_UL6 = FIRST_UL_CHANNEL + 6,
    INTR_UL7 = FIRST_UL_CHANNEL + 7,
    // OOB receive
    INTR_NIC0OOB  = FIRST_OOB_CHANNEL + 0,
    INTR_NIC1OOB  = FIRST_OOB_CHANNEL + 1,
    INTR_NIC2OOB  = FIRST_OOB_CHANNEL + 2,
    INTR_NIC3OOB  = FIRST_OOB_CHANNEL + 3,
    INTR_NIC4OOB  = FIRST_OOB_CHANNEL + 4,
    INTR_NIC5OOB  = FIRST_OOB_CHANNEL + 5,
    INTR_NIC6OOB  = FIRST_OOB_CHANNEL + 6,
    INTR_NIC7OOB  = FIRST_OOB_CHANNEL + 7,
    // TCP State change
/*
    INTR_TCP0STAT = 24,
    INTR_TCP1STAT = 25,
    INTR_TCP2STAT = 26,
    INTR_TCP3STAT = 27,
    INTR_TCP4STAT = 28,
    INTR_TCP5STAT = 29,
    INTR_TCP6STAT = 30,
    INTR_TCP7STAT = 31,*/
};
static const uint64_t INTR_NONE = 0x0;
static const uint64_t INTR_ALL  = 0xffffffffffffffff;

int  are_interrupts_enabled(const device_context_t * pDevExt);

void start_all_oob_interrupts(const device_context_t * pDevExt);
void start_single_oob_interrupt(sc_net_interface_t *nif);
void stop_all_interrupts(const device_context_t * pDevExt) ;

void enable_user_logic_interrupts(const device_context_t * pDevExt, uint8_t mask);
void enable_single_user_logic_interrupt(const device_context_t * pDevExt, unsigned int dmaChannelNumber);

void etlm_read_mac(const device_context_t * pDevExt, uint8_t *mac);
int  etlm_setup_rx_window(device_context_t * pDevExt, unsigned int conn_id, int addr_start, int addr_size);
int  etlm_setup_tx_window(device_context_t * pDevExt, unsigned int conn_id, int addr_start, int addr_size);
int  etlm_setSrcIp(device_context_t * pDevExt, uint32_t ip);
int  etlm_setSrcMac(device_context_t * pDevExt, const uint8_t mac[MAC_ADDR_LEN]);
void etlm_getTimerIntervals(const device_context_t * pDevExt, uint32_t * pSlowTimerIntervalUs, uint32_t * pFastTimerIntervalUs);
void etlm_getTCPTimers(const device_context_t * pDevExt, uint32_t* retransMinUs, uint32_t* retransMaxUs, uint32_t* persistMinUs, uint32_t* persistMaxUs);
void etlm_setDelayedAck(const device_context_t * pDevExt, bool enable);
void etlm_setRetransmissionTimeout(const device_context_t * pDevExt, uint32_t rto);
int  etlm_getTcpState(device_context_t * pDevExt, unsigned int conn_id);
int  etlm_waitForTcpState(device_context_t * pDevExt, unsigned int conn_id, int st, uint32_t utimeout);
//void etlm_setTcpConnSrcMac(device_context_t * pDevExt, unsigned int conn_id, uint8_t mac[MAC_ADDR_LEN]); // Not used anymore, anywhere
//void etlm_setTcpConnSrcIp(device_context_t * pDevExt, unsigned int conn_id, uint32_t ip);
void etlm_setSrcDstPorts(device_context_t * pDevExt, unsigned int conn_id, uint16_t src_port, uint16_t dst_port);
void etlm_getSrcDstPorts(device_context_t * pDevExt, unsigned int conn_id, uint16_t* src_port, uint16_t* dst_port);
uint32_t etlm_getTcpConnSndNxt(device_context_t * pDevExt, unsigned int conn_id);
uint32_t etlm_getTcpConnSndUna(device_context_t * pDevExt, unsigned int conn_id);
uint32_t etlm_getTcpConnSndToSeq(device_context_t * pDevExt, unsigned int conn_id);
uint32_t etlm_getTcpConnRcvToSeq(device_context_t * pDevExt, unsigned int conn_id);
/*
uint32_t etlm_getRdSeqNum(device_context_t * pDevExt, unsigned int conn_id);
uint32_t etlm_getRdBytesAcked(device_context_t * pDevExt, unsigned int conn_id);
uint32_t etlm_getWrSeqNum(device_context_t * pDevExt, unsigned int conn_id);
 */
uint32_t etlm_getInitialWriteSequenceOffset(device_context_t * pDevExt, unsigned int conn_id);
void etlm_getCapabilities(const device_context_t * pDevExt, uint32_t* num_conn,  uint32_t* txPacketSz,  uint32_t* rxPacketSz,  uint32_t* txTotalWinSz,  uint32_t* rxTotalWinSz, bool * vlan_supported);
int etlm_conn_close(device_context_t * pDevExt, unsigned int conn_id);
void etlm_abort_conn(device_context_t * pDevExt, unsigned int conn_id);
int etlm_tcp_connect(device_context_t * pDevExt, unsigned int conn_id, uint16_t src_port, uint16_t dst_port, uint32_t dst_ip, uint8_t mac[MAC_ADDR_LEN], uint32_t local_ip_address, int16_t vlan_tag);
int etlm_tcp_listen(device_context_t * pDevExt, unsigned int conn_id, int port, uint32_t local_ip_address, int16_t vlan_tag);

uint32_t etlm_read_debug_assertions(const device_context_t * pDevExt);
void etlm_getRemoteIpPortMac(device_context_t * pDevExt, unsigned int conn_id, uint32_t * pIpAddress, uint16_t * pPort, uint8_t macAddress[MAC_ADDR_LEN]);

#endif /* FPGA_H */

