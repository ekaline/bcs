#define bool	_Bool
#define true	1
#define false	0

#define AXIREG 0x48

#define SPICR 0x60
#define SPISR 0x64
#define SPIDTR 0x68
#define SPIDRR 0x6C
#define SPISSR 0x70
#define SPIRFIFOFILL 0x78
#define SPITFIFOFILL 0x74


#define COMMAND_PAGE_PROGRAM		0x02 /* Page Program command */
#define COMMAND_QUAD_WRITE			0x32 /* Quad Input Fast Program */
#define COMMAND_RANDOM_READ			0x03 /* Random read command */
#define COMMAND_DUAL_READ			0x3B /* Dual Output Fast Read */
#define COMMAND_DUAL_IO_READ		0xBB /* Dual IO Fast Read */
#define COMMAND_QUAD_READ           0x6B /* Quad Output Fast Read */
#define COMMAND_QUAD_READ_4BYTE     0x6C /* Quad Output Fast Read */
#define COMMAND_QUAD_IO_READ		0xEB /* Quad IO Fast Read */
#define	COMMAND_WRITE_ENABLE		0x06 /* Write Enable command */
#define COMMAND_SECTOR_ERASE		0xD8 /* Sector Erase command */
#define COMMAND_BULK_ERASE			0xC7 /* Bulk Erase command */
#define COMMAND_STATUS_REG_READ		0x05 /* Status read command */
#define COMMAND_FSTATUS_REG_READ	0x70 /* Flag Status read command */
#define COMMAND_ENTER_4BYTE_ADDRESS_MODE                        0xB7 /* ENTER 4-BYTE ADDRESS MODE B7 */
#define COMMAND_4BYTE_QUAD_OUTPUT_FAST_READ 0x6c
#define  COMMAND_RD_NONVLT_CFG_REG     0xB5 /* READ NONVOLATILE CONFIGURATION REGISTER */
#define  COMMAND_WR_NONVLT_CFG_REG     0xB1 /* WRITE NONVOLATILE CONFIGURATION REGISTER */
#define  COMMAND_VLT_CFG_REG        0x85 /* READ VOLATILE CONFIGURATION REGISTER */
#define  COMMAND_ENH_VLT_CFG_REG    0x65 /* READ ENHANCED VOLATILE CONFIGURATION REGISTER */
#define COMMAND_EXT_REG_READ    0xC8 /* READ EXTENDED ADDRESS REGISTER */
#define COMMAND_EXT_REG_WRITE     0xC5 /* WRITE EXTENDED ADDRESS REGISTER */
#define COMMAND_DEV_READ_ID			0x9F /* Read Device ID */

#define FLASH_SR_IS_READY_MASK          0x01 /* Ready mask */

#define QUAD_IO_READ_DUMMY_BYTES        1
#define ADDRSIZE 3
#define CMDSIZE 1

#define XSP_CR_LOOPBACK_MASK	   0x00000001 /**< Local loopback mode */
#define XSP_CR_ENABLE_MASK	       0x00000002 /**< System enable */
#define XSP_CR_MASTER_MODE_MASK	   0x00000004 /**< Enable master mode */
#define XSP_CR_CLK_POLARITY_MASK   0x00000008 /**< Clock polarity high or low */
#define XSP_CR_CLK_PHASE_MASK	   0x00000010 /**< Clock phase 0 or 1 */
#define XSP_CR_TXFIFO_RESET_MASK   0x00000020 /**< Reset transmit FIFO */
#define XSP_CR_RXFIFO_RESET_MASK   0x00000040 /**< Reset receive FIFO */
#define XSP_CR_MANUAL_SS_MASK	   0x00000080 /**< Manual slave select assert */
#define XSP_CR_TRANS_INHIBIT_MASK  0x00000100 /**< Master transaction inhibit */
#define XSP_SR_TX_FULL_MASK		   0x00000008 /**< Transmit Reg/FIFO is full */
#define XSP_SR_TX_EMPTY_MASK       0x00000004 /**< Transmit Reg/FIFO is empty */
#define XSP_SR_RX_EMPTY_MASK       0x00000001 /**< Receive Reg/FIFO is Enpty */

#define CS_TRIG       0x4000000000000000

#define READ_WRITE_EXTRA_BYTES 4

#include "FL_FlashLibrary.h"

typedef struct{
//	int p_readbuf;
//	uint32_t readbuf_size;
//	uint8_t *readbuf;
//
//	int p_writebuf;
	uint32_t writebuf_size;
	uint8_t *writebuf;
} flash_t;

/*
// Defiane a vector type
typedef struct {
  int size;      // slots used so far
  int capacity;  // total available slots
  int *data;     // array of integers we're storing
} Vector;

*/

uint32_t axird(const FL_FlashInfo * pFlashInfo,uint64_t axireg);
int axiwr(const FL_FlashInfo * pFlashInfo,uint64_t axireg, uint64_t data);
int QSPIFlashReadID(const FL_FlashInfo * pFlashInfo);


int QSPI_Transfer(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf,uint16_t rd_size, const uint8_t *wr_buf, uint16_t wr_size);
int QSPIFlashReadID(const FL_FlashInfo * pFlashInfo);
int QSPI_Transfer2(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf,uint16_t rd_size, uint8_t *wr_buf, uint16_t  wr_size);
uint8_t QSPI_ReadStatusReg(const FL_FlashInfo * pFlashInfo);
int QSPIFlashGetFlagStatus(const FL_FlashInfo * pFlashInfo);
int QSPIFlashWriteEnable(const FL_FlashInfo * pFlashInfo);
int SpiFlash4bytemodeEnable(const FL_FlashInfo * pFlashInfo);
int SpiFlash3bytemodeEnable(const FL_FlashInfo * pFlashInfo);
int SpiFlashRead_test(const FL_FlashInfo * pFlashInfo, uint32_t Addr, uint8_t *wr_buf,uint16_t wr_size,uint8_t *rd_buf,uint16_t rd_size );
int SpiFlashRead(const FL_FlashInfo * pFlashInfo, uint32_t Addr, uint32_t ByteCount);
int SpiFlashRead03(const FL_FlashInfo * pFlashInfo, uint32_t Addr, uint32_t ByteCount);
int SpiFlashRead03Dump(const FL_FlashInfo * pFlashInfo, uint32_t Addr, uint32_t ByteCount,uint8_t *rd_buf);//, uint32_t ReadCmd)
int QSPIDump(const FL_FlashInfo * pFlashInfo);
int QSPIPage(const FL_FlashInfo * pFlashInfo);
int QSPIFlashReady(const FL_FlashInfo * pFlashInfo);

int SpiFlashRead_long(const FL_FlashInfo * pFlashInfo, uint32_t Addr, uint8_t *wr_buf,uint32_t wr_size,uint8_t *rd_buf,uint32_t rd_size );
int QSPI_Transfer_long(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf, uint32_t rd_size, uint8_t *wr_buf, uint16_t wr_size);
//int QSPI_Transfer_long(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf,uint32_t rd_size, uint8_t *wr_buf, uint32_t wr_size);
int QSPILong(const FL_FlashInfo * pFlashInfo);
int QSPI_TransferLong(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf,uint32_t rd_size, uint8_t *wr_buf, uint16_t wr_size);
//int QSPI_ReadSector(const FL_FlashInfo * pFlashInfo, uint32_t Addr);
//int QSPI_Transfer_write(const FL_FlashInfo * pFlashInfo, uint8_t *rd_buf,uint16_t rd_size, uint8_t *wr_buf, uint16_t wr_size);
int QSPI_ProgramPage(const FL_FlashInfo * pFlashInfo, uint32_t Addr);
int QSPI_ProgramFile(const FL_FlashInfo * pFlashInfo, const char * fileName);
int QSPI_BulkErase(const FL_FlashInfo * pFlashInfo);
int QSPI_FlashBusy(const FL_FlashInfo * pFlashInfo);
int QSPI_ReadPage(const FL_FlashInfo * pFlashInfo, uint32_t Addr, const char * fileName);
int QSPI_ReadFlashToFile(const FL_FlashInfo * pFlashInfo, uint32_t Addr, const char * fileName);
int QSPI_EraseSector(const FL_FlashInfo * pFlashInfo, uint32_t Addr);
int QspiSetExtendedAddress(const FL_FlashInfo * pFlashInfo, uint8_t segment);
int SpiFlash4bytemodeEnableNVR(const FL_FlashInfo * pFlashInfo);
int SpiFlashSWReset(const FL_FlashInfo * pFlashInfo);
int FIFO_empty(const FL_FlashInfo * pFlashInfo);
int FIFO_fill(const FL_FlashInfo * pFlashInfo,uint8_t txrx);
int QSPI_ReadFlash(const FL_FlashInfo * pFlashInfo);
int QSPI_Program(const FL_FlashInfo * pFlashInfo);
int QSPI_ReadByte(const FL_FlashInfo * pFlashInfo, uint32_t Addr);
int QSPI_ProgramPage4Byte(const FL_FlashInfo * pFlashInfo, uint32_t Addr);
int QSPIReadReg(const FL_FlashInfo * pFlashInfo,uint8_t cmd);
int QSPIReadRegWrite(const FL_FlashInfo * pFlashInfo,uint8_t cmd,uint8_t data);
int QSPI_VerifyFile(const FL_FlashInfo * pFlashInfo, uint32_t Addr, const char * fileName);
