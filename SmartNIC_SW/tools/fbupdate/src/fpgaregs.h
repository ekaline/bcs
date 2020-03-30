/* 
 * File:   fpgaregs.h
 * Author: rdj
 *
 * Created on September 26, 2010, 8:53 PM
 */

#ifndef _FPGAREGS_H
#define         _FPGAREGS_H

#define rd64(base, offset) (((volatile uint64_t*)(base))[(offset)/8])
//#define rd64(base, offset) 1
#define wr64(base, offset, data) ((volatile uint64_t*)(base))[(offset)/8] = (data)
//#define wr64(base, offset, data) while(0);
//#define wr64(base, offset, data) printf("%08llX : %016llX\n", &((uint64_t*)(baseAddr))[(offset)/8], (data))

#define rd32(base, offset) (((volatile uint32_t*)(base))[(offset)/4])
//#define rd64(base, offset) 1
#define wr32(base, offset, data) ((volatile uint32_t*)(base))[(offset)/4] = (data)


#define REG_VERSION           0x0
#define REG_RESET             0x8
#define REG_PCIE_STAT         0x10
#define REG_SYSMON            0x20
#define REG_PCIE0_DRP         0x28
#define REG_PCIE1_DRP         0x2C
#define REG_ETH_DRP           0x30
#define REG_DNA               0x38
#define REG_FLASH_CMD         0x40
#define REG_FLASH_WR          0x48
#define REG_FLASH_RD          0x50
#define REG_SPI_ATMEL         0x48
#define REG_SPI_CG            0x50
#define REG_BERGAMO_FLASH_RD  0x58
#define REG_DEBUG0            0x60
#define REG_DEBUG1            0x68
#define REG_SCRATCHPAD70      0x70
#define REG_SCRATCHPAD78      0x78
#define REG_PERF_MON          0xA0
#define REG_MDIO              0x1A0
#define REG_I2C               0x1A8
#define REG_I2C_CLK           0x30
#define REG_LEN_MONITOR       0x170
#define REG_LEN_MONITOR2      0x178
#define REG_PANIC             0x180
#define REG_SPEED_CTRL        0x1B0
#define REG_SFP_LINK          0x1C0
//#define REG_ETH_DRP           0x1D0
#define REG_RXTX1G            0x200
#define REG_XAUI0_DATA        0x210
#define REG_XAUI0_CTRL        0x218
#define REG_XAUI1_DATA        0x220
#define REG_XAUI1_CTRL        0x228
#define REG_XAUI2_DATA        0x230
#define REG_XAUI2_CTRL        0x238
#define REG_XAUI3_DATA        0x240
#define REG_XAUI3_CTRL        0x248
#define REG_FRONTPORTTEST_CTRL      0x210
#define REG_FRONTPORTTEST_STTS0     0x218
#define REG_FRONTPORTTEST_STTS_OFFs 0x8
#define REG_ROCKET_LB         0x2A0
#define REG_CLK_COUNT         0x2C0
#define REG_CLK_COUNT1        0x2C8
#define REG_ZYNQ_TEST         0x1B8
#define REG_ZYNQ_STATUS       0x1C0

//Memory test registers for memory width 64
#define REG_MT64_RX0          0x300
#define REG_MT64_RX1          0x308
#define REG_MT64_ERR_ADDR     0x330
#define REG_MT64_STAT         0x338
#define REG_MT64_TX0          0x300
#define REG_MT64_TX1          0x308
#define REG_MT64_DM           0x320
#define REG_MT64_ST_ADDR      0x328
#define REG_MT64_END_ADDR     0x330
#define REG_MT64_MODE         0x338

//Memory test registers for memory width 128
#define REG_MT128_RX0          0x300
#define REG_MT128_RX1          0x308
#define REG_MT128_RX2          0x310
#define REG_MT128_RX3          0x318
#define REG_MT128_ERR_ADDR     0x330
#define REG_MT128_STAT         0x338
#define REG_MT128_TX0          0x300
#define REG_MT128_TX1          0x308
#define REG_MT128_TX2          0x310
#define REG_MT128_TX3          0x318
#define REG_MT128_DM           0x340                          
#define REG_MT128_ST_ADDR      0x348
#define REG_MT128_END_ADDR     0x350
#define REG_MT128_MODE         0x358

//Memory test registers for memory width 256
#define REG_MT256_RX0          0x300
#define REG_MT256_RX1          0x308
#define REG_MT256_RX2          0x310
#define REG_MT256_RX3          0x318
#define REG_MT256_RX4          0x320
#define REG_MT256_RX5          0x328
#define REG_MT256_RX6          0x330
#define REG_MT256_RX7          0x338
#define REG_MT256_ERR_ADDR     0x340
#define REG_MT256_STAT         0x348
#define REG_MT256_TX0          0x300
#define REG_MT256_TX1          0x308
#define REG_MT256_TX2          0x310
#define REG_MT256_TX3          0x318
#define REG_MT256_TX4          0x320
#define REG_MT256_TX5          0x328
#define REG_MT256_TX6          0x330
#define REG_MT256_TX7          0x338
#define REG_MT256_DM           0x340
#define REG_MT256_ST_ADDR      0x348
#define REG_MT256_END_ADDR     0x350
#define REG_MT256_MODE         0x358
#define MT_BANK_OFFSET         0x80
#define REG_STATDMA_STA        0x420
#define REG_STATDMA_CTL        0x438
#define REG_ISDMA_STA          0x440
#define REG_ISDMA_END          0x448
#define REG_ISDMA_RMK          0x450
#define REG_ISDMA_CTL          0x458
#define REG_PLDMA_STA          0x480
#define REG_PLDMA_CTL          0x498

#define REG_PLDMA_NEW_CTL      0x600

// Offset where RXDMA start
#define REG_RXDMA_OFFSET       0x1000
// Size of each RxDMA block in bytes
#define REG_RXDMA_BLOCK_SZ     0x40



#define REG_JENA_CTRL          0x628
#define REG_JENA_PORT          0x630
#define REG_JENA_STATUS        0x648
#define REG_JENA_PACKET_COUNT  0x630
#define REG_JENA_BYTE_COUNT    0x638
#define REG_JENA_IFG_COUNT     0x640

// PCAP registers
#define REG_PCAP_CTRL       0x200
#define REG_PCAP_RTN        0x220
#define REG_PCAP_DATA       0x228
#define REG_PCAP_FILL       0x228


// DRP registers
#define REG_DRP0        0x1d0

/////////////////////
// FPGA types      //
/////////////////////
#include "fpgatypes.h"

/////////////////////
// FPGA models     //
/////////////////////
#include "fpgamodels.h"

// To read the debug mux register
#define DEBUG_MUX_REG      REG_FLASH_CMD

#endif  /* _FPGAREGS_H */

