/***************************************************************************/ /**
                                                                               * SPI flash controller commands
                                                                               */

#ifndef _SF2COMMANDS_H_
#define _SF2COMMANDS_H_

#define SF2_COMMAND_BITNO 28
#define SF2_COMMAND_MASK (-(1 << SF2_COMMAND_BITNO))

// 4(WRITE_DISABLE_CMD), 5(READ_STATUS_OPCODE) 11,
// 12(SF2_READ_SCRATCHPAD),13(SF2_RESET_FLASH) are
// available.
#define SF2_READ_FLASH                                     \
  1 // The address field is ignored. Data is read
    // sequentially from the read buffer.
#define SF2_WRITE_FLASH                                    \
  2 // The address field is ignored. Data is appended to the
    // write buffer.
#define SF2_READ_EXAR_CMD                                  \
  6 // Read EXAR cmd 2 byte result into flash read buffer
#define SF2_READ_EXAR_REG                                  \
  7 // Read EXAR reg 1 byte result into flash read buffer
#define SF2_READ_REVISION_LSH 8
#define SF2_READ_REVISION_MSH 9
#define SF2_EXECUTE SF2_READ_STATUS
#define SF2_READ_STATUS 10
#define SF2_WRITE_SCRATCHPAD 11
#define SF2_READ_SCRATCHPAD 12
#define SF2_RESET_FLASH 13
#define SF2_LOAD_IMAGE 14
#define SF2_LOAD_EXAR1_IMAGE                               \
  5 // subcommand for SF2_LOAD_IMAGE
#define SF2_LOAD_EXAR2_IMAGE                               \
  6 // subcommand for SF2_LOAD_IMAGE
#define SF2_READ_EXAR1_CMD                                 \
  SF2_READ_EXAR_CMD // Read EXAR1 cmd 2 byte result into
                    // flash read buffer
#define SF2_READ_EXAR2_CMD                                 \
  12 // Read EXAR2 cmd 2 byte result into flash read buffer
#define SF2_READ_EXAR1_REG SF2_READ_EXAR_REG
#define SF2_READ_EXAR2_REG 0x04
#define SF2_LOAD_LTC_IMAGE 7 // subcommand for
                             // SF2_LOAD_IMAGE

/* SF2_EXECUTE/READ_STATUS subcommands */
/* The data field of this command may contain a Flash
 * command, which is then executed on the specified address.
 */
/* The indented commands are not supported on the SF2 Comm
 * Block interface, but are only used internally in the
 * implementation. */
/* Standard JEDEC op codes in flash terminology; not all
 * need to be supported by a flash vendor */
#define READ_ARRAY_OPCODE                                  \
  0x0B // Fill the read buffer from the specified address
#define READ_IDENTIFICATION                                \
  0x9F // Fill the read buffer with Identification data from
       // the Flash chip
#define WRITE_ENABLE_CMD 0x06
//  #define WRITE_DISABLE_CMD       0x04
#define WRITE_PAGE_CMD                                     \
  0x02 // Flush contents of the write buffer to the flash
       // chip from the specified address
#define WRITE_STATUS_OPCODE 0x01
#define CHIP_ERASE_OPCODE 0xC7
#define ERASE_256_BLOCK_OPCODE 0xDB
#define ERASE_4K_BLOCK_OPCODE 0x20
#define ERASE_64K_BLOCK_OPCODE 0xD8
#define READ_STATUS_OPCODE                                 \
  0x05 // Mandatory, the rest is optional
#define SF2_CLEAR_OVERRUN READ_STATUS_OPCODE
#define SF2_NO_SUBCOMMAND 0
#define READ_TELEMETRY 0x03

/* SF2_READ_STATUS result */
#define SF2_STATUS_READ_OVERRUN (1 << 0)
#define SF2_STATUS_WRITE_OVERRUN (1 << 1)
#define SF2_STATUS_EXEC_OVERRUN (1 << 2)
#define SF2_STATUS_FLASH_BUSY (1 << 3)
#define SF2_STATUS_UNKNOWN_CMD (1 << 4)
#define SF2_STATUS_POWER_GOOD_N (1 << 5)
#define SF2_STATUS_PLL_LOCK_N (1 << 6)
#define SF2_STATUS_CLK_LOCK_N (1 << 7)
#define SF2_STATUS_ADDRESS_MAP (0xf << 8)
#define SF2_STATUS_RESERVED12 (0xf << 12)

#define SILICOM_VENDOR_ID 0x1C2C
#define PLL_REVISION_NUM 0x1B
/* fixed by ekaline
enum {
    PLL_CONFIG=0x0A,
    EXAR_CONFIG=0x0B,
    SF2_IMAGE=0x0C,
    LTC_CONFIG=0X0D
} FLASH_IMAGE_TYPE;
*/
#define MANGO_02_PCB 0x02
#define MANGO_03_PCB 0x03

#define PLL_CONFIG_SPI_FLASH_ADDR 0x100
#define EXAR1_CONFIG_SPI_FLASH_ADDR 0x800
#define EXAR2_CONFIG_SPI_FLASH_ADDR 0xB00
#define LTC_CONFIG_SPI_FLASH_ADDR 0xD00
#define SF2_IMAGE_SPI_FLASH_ADDR 0x2000

#endif
