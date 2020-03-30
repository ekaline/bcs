#ifndef __FL_FlashLibrary_h__
#define __FL_FlashLibrary_h__

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

#ifdef __cplusplus
    #include <cerrno>
    #include <cstddef>
    #include <cstdint>
    #include <cstdio>
    #include <ctime>
#else
    #include <errno.h>
    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <time.h>
#endif

#include <sys/mman.h>

#include "fpgadeviceids.h"
#include "fpgatypes.h"
#include "fpgamodels.h"

#define FL_VERSION_MAJOR        2       /**< Major number. */
#define FL_VERSION_MINOR        0       /**< Minor number. */
#define FL_VERSION_MAINTENANCE  3       /**< Maintenance number. */
#define FL_VERSION_BUILD        0       /**< Build number is always zero for external releases. */

#ifdef __cplusplus
extern "C"
{
#endif

/***************************************************************************/

//#ifdef DEBUG
#define FL_Assert(logicalExpression) \
    do \
    { \
        if (!(logicalExpression)) \
        { \
            fprintf(stderr, "Assertion '%s' failed! File %s#%d, function %s\n", #logicalExpression, __FILE__, __LINE__, __func__); \
            fflush(stderr); \
            exit(EXIT_FAILURE); \
        } \
    } while (false)
#define FL_AssertW(what, logicalExpression) \
    do \
    { \
        if (!(logicalExpression)) \
        { \
            fprintf(stderr, "%s: assertion '%s' failed! File %s#%d, function %s\n", what, #logicalExpression, __FILE__, __LINE__, __func__); \
            fflush(stderr); \
            exit(EXIT_FAILURE); \
        } \
    } while (false)
/*#else
    #define FL_Assert(logical_expression)
    #define FL_AssertW(what, logical_expression)
#endif*/

/***************************************************************************/

/**
 *  Macro that returns with an error code if the called function failed.
 *
 *  @param  functionCall    Function call whose return error code is checked
 *                          and returned in case of failure.
 */
#define FL_ExitOnError(pLogContext, functionCall) \
    { \
        FL_Error localErrorCode = functionCall; \
        if (localErrorCode != FL_SUCCESS) \
        { \
            if (FL_LogErrorCallStack(pLogContext)) \
            { \
                /*fprintf(stderr, "\n *** ERROR in FL_ExitOnError: %s#%u function %s error code %d\n", __FILE__, __LINE__, __func__, localErrorCode);*/ \
                FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, " *** ERROR in FL_ExitOnError: %s#%u function %s error code %d\n", __FILE__, __LINE__, __func__, localErrorCode); \
            } \
            return localErrorCode; \
        } \
    }

 /***************************************************************************/

#define FL_OnErrorGotoExit(functionCall) \
    { \
        FL_Error localErrorCode = functionCall; \
        if (localErrorCode != FL_SUCCESS) \
        { \
            fprintf(stderr, "\n *** ERROR in FL_OnErrorGotoExit: %s#%u function %s error code %d\n", __FILE__, __LINE__, __func__, localErrorCode); \
            errorCode = localErrorCode; \
            goto ErrorExit; \
        } \
    }

/** **************************************************************************
 * @defgroup ConstantsMacros Constants, macros, enumerations and data structures.
 * @{
 */

/**
 *  Default FPGA memory mapped FPGA registers size. Can be overridden by external user definition.
 */
#ifndef FL_DEFAULT_FPGA_REGISTERS_MEMORY_SIZE
    #define FL_DEFAULT_FPGA_REGISTERS_MEMORY_SIZE       0x10000     /**< (64 KB) Default FPGA memory mapped FPGA registers size. */
#endif

/**
 *  Default FPGA memory mapped FPGA BAR2 registers size. Can be overridden by external user definition.
 */
#ifndef FL_DEFAULT_FPGA_BAR2_REGISTERS_MEMORY_SIZE
    #define FL_DEFAULT_FPGA_BAR2_REGISTERS_MEMORY_SIZE       0x200000     /**< (2 MB) Default FPGA memory mapped FPGA registers size. */
#endif
    
/**
 *  FPGA register offsets from PCIe BAR0 base address.
 */
typedef enum
{
    FL_FPGA_REGISTER_REVISION               = 0x00,     /**< (0x00) FPGA revision register. */
    FL_FPGA_REGISTER_PCIE_CONFIG_STATUS     = 0x10,     /**< (0x10) PCIe configuration and status register. */
    FL_FPGA_REGISTER_ADMIN_COMMAND          = 0x40,     /**< (0x40) Card microcontroller command register. */
    FL_FPGA_REGISTER_QSPI_COMMAND           = 0x48,     /**< (0x48) QSPI flash command register. */

} FL_FpgaRegisterOffset;

/**
 *  Flash library logging levels.
 */
typedef enum
{
    FL_LOG_LEVEL_NONE           = -1,           /**< (-1) Unknown or undefined log level */
    FL_LOG_LEVEL_ALWAYS         = 0,            /**< (0) Log always. */
    FL_LOG_LEVEL_FATAL          = 1,            /**< (1) Log fatal errors. */
    FL_LOG_LEVEL_ERROR          = 2,            /**< (2) Log errors. */
    FL_LOG_LEVEL_WARNING        = 3,            /**< (3) Log warnings. */
    FL_LOG_LEVEL_INFO           = 4,            /**< (4) Log information. */
    FL_LOG_LEVEL_DEBUG          = 5,            /**< (5) Log debug information. */
    FL_LOG_LEVEL_TRACE          = 6,            /**< (6) Log trace information (more detailed than debug). */
    FL_LOG_LEVEL_MASK           = 0x00FF,       /**< (0x00FF) Mask to get to the above log level values. */
    FL_LOG_LEVEL_CALL_STACK     = 0x0100,       /**< (0x0100) Bit mask enabling limited call stack tracing of errors. */
    FL_LOG_LEVEL_ERROR_DETAILS  = 0x0200        /**< (0x0200) Bit mask enabling more detailed error messages
                                                     including file name, line number and function name. */

} FL_LogLevel;

/**
 *  Different flash file formats supported.
 */
typedef enum
{
    FL_FLASH_FILE_FORMAT_UNKNOWN    = 0,    /**< (0) Unknown flash file format. */
    FL_FLASH_FILE_FORMAT_BIT        = 1,    /**< (1) Flash file is in bit format.  */
    FL_FLASH_FILE_FORMAT_BIN        = 2,    /**< (2) Flash file is binary format. */
    FL_FLASH_FILE_FORMAT_RBF        = 3,    /**< (3) Flash file is RBF format. */
    FL_FLASH_FILE_FORMAT_SPI        = 4     /**< (4) Flash file is in SPI format. */

} FL_FlashFileFormat;

/**
 *  Flash library error codes.
 */
typedef enum
{
    FL_SUCCESS                                                  = 0,    /**< (0) Success, no errors. */
    FL_ERROR_INVALID_CONTROLLER_VERSION                         = -1,   /**< (-1) Invalid controller version. */
    FL_ERROR_FLASH_READY_NO_ACK                                 = -2,   /**< (-2) Flash ready status reported no ACK. */
    FL_ERROR_FLASH_READY_TIMEOUT                                = -3,   /**< (-3) Flash status poll timed out. */
    FL_ERROR_INVALID_FLASH_ADDRESS_SIZE                         = -4,   /**< (-4) Flash address size (24 or 28 bits). */
    FL_ERROR_UNLIKELY_FPGA_REVISION_FOUND                       = -5,   /**< (-5) Unlikely FPGA revision found. Please check that driver matches PCI vendor and device ID. */
    FL_ERROR_FAILED_TO_CONVERT_BUILD_TIME_TO_UTC                = -6,   /**< (-6) Failed to convert FPGA build time to UTC. */
    FL_ERROR_FLASH_INVALID_ADMIN_SOFTWARE_REVISION              = -7,   /**< (-7) Invalid admin SPI controller software revision read from FPGA. */
    FL_ERROR_FAILED_TO_OPEN_DEVICE                              = -8,   /**< (-8) Failed to open a user given device file. */
    FL_ERROR_FAILED_TO_OPEN_ANY_DEVICE                          = -9,   /**< (-9) Failed to open any of the predefined device names. */
    FL_ERROR_IS_DRIVER_LOADED                                   = -10,  /**< (-10) Expected a loaded Silicom device driver. Is driver loaded? */

    FL_ERROR_MMAP_FAILED                                        = -11,  /**< (-11) Failed to map FPGA registers to user space. */
    FL_ERROR_MUNMAP_FAILED                                      = -12,  /**< (-12) Unmapping FPGA register space to user space failed. */
    FL_ERROR_DEVICE_CLOSE_FAILED                                = -13,  /**< (-13) Failed to close device file. */
    FL_ERROR_PCIE_END_POINT_NOT_UP_AND_READY                    = -14,  /**< (-14) PCIe end point is not up and ready. */
    FL_ERROR_INVALID_LOG_LEVEL                                  = -15,  /**< (-15) Invalid log level, i.e. value is not one of @ref FL_LogLevel. */
    FL_ERROR_INVALID_PCB_VERSON_NUMBER                          = -16,  /**< (-16) Invalid PCB version number. */
    FL_ERROR_FLASH_WRITE_ADDRESS_IS_OUT_OF_RANGE                = -17,  /**< (-17) Flash write address is out of range. */
    FL_ERROR_UNKNOWN_FLASH_COMMAND_SET                          = -18,  /**< (-18) Unknown flash command set. */
    FL_ERROR_TIMEOUT_WAITING_FOR_FLASH_READY                    = -19,  /**< (-19) Timeout waiting for flash ready. */

    FL_ERROR_INVALID_PCIE_BOOT_MODE                             = -20,  /**< (-20) Invalid PCIe boot mode read from the FPGA. Valid boot modes are defined by @ref FL_PCIeBootMode. */
    FL_ERROR_NOT_IMPLEMENTED                                    = -21,  /**< (-21) Not implemented feature. */
    FL_ERROR_FLASH_IS_NOT_READY                                 = -22,  /**< (-22) Generic flash is not ready error. The actual cause might be either
                                                                               @ref FL_ERROR_SPANSION_FLASH_IS_NOT_READY or @ref FL_ERROR_MICRON_FLASH_IS_NOT_READY
                                                                               or @ref FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY. */
    FL_ERROR_SPANSION_FLASH_IS_NOT_READY                        = -23,  /**< (-23) Spansion flash is not ready. */
    FL_ERROR_MICRON_FLASH_IS_NOT_READY                          = -24,  /**< (-24) Micron flash is not ready. */
    FL_ERROR_SWITCH_TO_PRIMARY_IMAGE_NOT_SUPPORTED              = -25,  /**< (-25) Switch to primary flash image not supported. */
    FL_ERROR_SWITCH_TO_SECONDARY_IMAGE_NOT_SUPPORTED            = -26,  /**< (-26) Switch to secondary flash image not supported. */
    FL_ERROR_SECTOR_LOCKED_CANNOT_ERASE                         = -27,  /**< (-27) Flash sector is locked, cannot erase. */
    FL_ERROR_FLASH_WRITE_FILE_IMAGE_FAILED                      = -28,  /**< (-28) Failed to write image file to flash. */
    FL_ERROR_FLASH_VERIFY_FAILED                                = -29,  /**< (-29) Flash verify failed. */

    FL_ERROR_INVALID_FLASH_IMAGE_START_ADDRESS                  = -30,  /**< (-30) Invalid flash image start address. */
    FL_ERROR_INVALID_SPANSION_FLASH_DEVICE                      = -31,  /**< (-31) Invalid Spansion flash device. */
    FL_ERROR_INVALID_MICRON_FLASH_DEVICE                        = -32,  /**< (-32) Invalid Micron flash device. */
    FL_ERROR_UNKNOWN_FLASH_DEVICE                               = -33,  /**< (-33) Unknown flash device. */
    FL_ERROR_FSTAT_FAILED                                       = -34,  /**< (-34) Call to system fstat function failed, errno shows the system error code. */
    FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE                         = -35,  /**< (-35) Failed to close image file, errno shows the system error code. */
    FL_ERROR_IMAGE_FILE_TOO_BIG                                 = -36,  /**< (-36) Image file is too big to fit into the flash. */
    FL_ERROR_NO_FLASH_SPACE_RESERVED_FOR_IMAGE                  = -37,  /**< (-37) No flash space reserved for the image. */
    FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE                          = -38,  /**< (-38) Failed to open image file, errno shows the system error code. */
    FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW                    = -39,  /**< (-39) Xilinx bit file header field data is longer than internal buffer capacity. */

    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_1_LENGTH             = -40,  /**< (-40) Xilinx bit file header field 1 is not 9 bytes in length. */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_2_LENGTH             = -41,  /**< (-41) Xilinx bit file header field 2 is not 1 bytes in length. */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_2_KEY                = -42,  /**< (-42) Xilinx bit file header field 2 invalid key, 'a' expected. */
    FL_ERROR_FPGA_TYPES_DO_NOT_MATCH_FOR_UPGRADE                = -43,  /**< (-57) Bit image file FPGA type cannot replace/upgrade the existing FPGA type. */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_KEY                = -44,  /**< (-44) Xilinx bit file header field 4 invalid key, 'b' expected. */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_PART_NUMBER        = -45,  /**< (-45) Xilinx bit file header field 4 invalid part number. */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_5_KEY                = -46,  /**< (-46) Xilinx bit file header field 5 invalid key, 'c' expected */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_6_KEY                = -47,  /**< (-47) Xilinx bit file header field 6 invalid key, 'd' expected */
    FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_7_KEY                = -48,  /**< (-48) Xilinx bit file header field 7 invalid key, 'e' expected */
    FL_ERROR_INVALID_BIT_FILE_FORMAT                            = -49,  /**< (-49) Invalid bit image file format. */

    FL_ERROR_BIT_FILE_BUFFER_WRITE_OVERFLOW                     = -50,  /**< (-50) Bit image file buffer write overflow. */
    FL_ERROR_BIT_FILE_BUFFER_READ_OVERFLOW                      = -51,  /**< (-51) Bit image file buffer read overflow. */
    FL_ERROR_BIT_FILE_BUFFER_OVERFLOW                           = -52,  /**< (-52) Bit image file buffer overflow. */
    FL_ERROR_INVALID_BIT_FILE_FORMAT_BYTES_NOT_SWAPPED          = -53,  /**< (-53) Bit image file bytes are not swapped as expected. */
    FL_ERROR_BIT_FILE_UNEXPECTED_VALUE                          = -54,  /**< (-54) Bit image file unexpected value at offset 24, expeced 0x5599. */
    FL_ERROR_IMAGE_FILE_WRONG_SIZE                              = -55,  /**< (-55) Bit image file wrong size. */
    FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF              = -56,  /**< (-56) Bit image file unexpected End Of File. */
    FL_ERROR_FLASH_VERIFY_WORD_DATA_AGAINST_FILE_FAILED         = -57,  /**< (-57) Bit image file verify word data against flashed image failed. */
    FL_ERROR_FLASH_VERIFY_WORD_DATA_AGAINST_FILE_TOTAL_ERRORS   = -58,  /**< (-58) Bit image file verify word data against flashed image total errors. */
    FL_ERROR_MICRON_SF2_IS_NOT_READY                            = -59,  /**< (-59) SF2 controller is not ready.  */

    FL_ERROR_NO_ADMIN_CONTROLLER_PRESENT                        = -60,  /**< (-60) No admin controller (Actel SF2 or SavonaControllerX) present on card. */
    FL_ERROR_INVALID_ADMIN_CONTROLLER_VERSION                   = -61,  /**< (-61) Invalid admin controller (Actel SF2 or SavonaControllerX) version. */
    FL_ERROR_INVALID_ADMIN_SOFTWARE_VERSION                     = -62,  /**< (-62) Invalid admin controller (Actel SF2 or SavonaControllerX) software version. */
    FL_ERROR_UNSUPPORTED_ADMIN_SPI_FLASH_TYPE                   = -63,  /**< (-63) Unsupported admin (Actel SF2 or SavonaControllerX) SPI flash type. */
    FL_ERROR_UNLIKELY_ADMIN_SPI_FLASH_SIZE                      = -64,  /**< (-64) Unlikely admin controller (Actel SF2 or SavonaControllerX) SPI flash size. */
    FL_ERROR_UNKNOWN_PRIMARY_FLASH_IMAGE                        = -65,  /**< (-65) Unknown primary flash image */
    FL_ERROR_UNKNOWN_SECONDARY_FLASH_IMAGE                      = -66,  /**< (-66) Unknown secondary flash image */
    FL_ERROR_UNKNOWN_IMAGE_FILE_TYPE                            = -67,  /**< (-67) Unknown flash image file type. */
    FL_ERROR_INVALID_ADMIN_TYPE                                 = -68,  /**< (-68) Invalid microcontroller/admin controller type. */
    FL_ERROR_INVALID_MICRON_NOR_FLASH_SIZE                      = -69,  /**< (-69) Invalid Micron NOR flash size read via QSPI. */

    FL_ERROR_INVALID_MICRON_NOR_FLASH_MEMORY_TYPE               = -70,  /**< (-70) Invalid Micron NOR flash memory type read via QSPI */
    FL_ERROR_IMAGE_FILE_READ_ERROR                              = -71,  /**< (-71) An error occurred while reading an FPGA image file. */
    FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY                      = -72,  /**< (-72) Micron NOR flash is not ready. */
    FL_ERROR_INVALID_ARGUMENT                                   = -73,  /**< (-73) Invalid function call argument, the textual message gives more details. */
    FL_ERROR_FAILED_TO_WRITE_TO_FLASH_IMAGE_FILE                = -74,  /**< (-74) Failed to write to flash image file. */
    FL_ERROR_INVALID_FLASH_MANUFACTURER_ID                      = -75,  /**< (-75) Invalid flash manufacturer id read from the flash. */
    FL_ERROR_TIMEOUT_WAITING_FOR_MICRON_NOR_FLASH_READY         = -76,  /**< (-76) Timeout waiting for Micron NOR flash to become ready. */
    FL_ERROR_FAILED_TO_READ_FROM_FLASH_IMAGE_FILE               = -77,  /**< (-77) Failed to read data from a flash image file. */
    FL_ERROR_FAILED_TO_TELL_POSITION_IN_IMAGE_FILE              = -78,  /**< (-78) Failed to tell read position in a flash image file. */
    FL_ERROR_FAILED_TO_SEEK_IN_OF_IMAGE_FILE                    = -79,  /**< (-79) Failed to seek in a flash image file. */

    FL_ERROR_FAILED_TO_GET_SYSTEM_TIME                          = -80,  /**< (-80) Failed to get system time with function clock_gettime. */
    FL_ERROR_INVALID_COMMAND_LINE_ARGUMENT                      = -81,  /**< (-81) Invalid command line argument. */


    FL_ERROR_GECKO_TIMEOUT                                      = -90,  /**< (-80) Wating Giant Gecko timed out. */



    FL_ERROR_FAILED_TO_READ_FPGA_REVISION                       = -100, /**< TODO */



    /* Special error codes that can be returned from a user defined error handler. */
    FL_ERROR_HANDLER_THROW_EXCEPTION                            = -997, /**< (-997) Throw a C++ exception FL_Exception when error handler returns.
                                                                             This is an experimental feature and not currently implemented. */
    FL_ERROR_HANDLER_CONTINUE_NO_DEFAULT_MESSAGE                = -998, /**< (-998) Continue execution in library when error callback returns.
                                                                             User defined error callback should return this error code from the
                                                                             callback if the failing API call should return with the original
                                                                             error code. Default error message is not output by library. */
    FL_ERROR_HANDLER_CONTINUE_WITH_DEFAULT_MESSAGE              = -999, /**< (-999) Continue execution in library when error callback returns.
                                                                             User defined error callback should return this error code from the
                                                                             callback if the failing API call should return with the original
                                                                             error code. Default error message is output by library. */
    FL_ERROR_MESSAGE_ONLY_MASK                                  = 0xBFFF    /**< And this mask to error code to print the message only and nothing extra. */

} FL_Error;

/**
 *  Flash types.
 *
 *  For Micron NOR flash memory types see Micron Serial NOR Flash Memory data sheet at
 *  https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&cad=rja&uact=8&ved=2ahUKEwiMnJzS4cPdAhVBhiwKHbVFBQAQFjAAegQIAxAC&url=https%3A%2F%2Fwww.micron.com%2F~%2Fmedia%2Fdocuments%2Fproducts%2Fdata-sheet%2Fnor-flash%2Fserial-nor%2Fmt25q%2Fdie-rev-b%2Fmt25q_qlkt_u_512_abb_0.pdf&usg=AOvVaw1s3BCiladf0FUPSnyvKazX
 */
typedef enum
{
    FL_FLASH_TYPE_UNKNOWN               = 0,    /**< (0) Unknown flash type. */
    FL_FLASH_TYPE_NONE                  = 1,    /**< (1) No flash type. */
    FL_FLASH_TYPE_20XX                  = 2,    /**< (2) 2010, 2015, 2022 flash type. */
    FL_FLASH_TYPE_PARALLEL_SPANSION     = 3,    /**< (3) Spansion flash type. */
    FL_FLASH_TYPE_PARALLEL_MICRON       = 4,    /**< (4) Micron parallel flash type used in Mango and Padua cards. */
    FL_FLASH_TYPE_MICRON_NOR            = 5,    /**< (5) Micron QSPI serial flash type used in Savona cards. */
    FL_FLASH_TYPE_MICRON_NOR_N25Q_3V    = 6,    /**< (0xBA) Micron NOR flash memory type B25Q 3V */
    FL_FLASH_TYPE_MICRON_NOR_N25Q_1_8V  = 7     /**< (0xBB) Micron NOR flash memory type B25Q 1.8V */

} FL_FlashType;

/**
 *  Different command sets used to access different types of FPGA flash
 *  and different types of admin controllers.
 */
typedef enum
{
    FL_FLASH_COMMAND_SET_UNKNOWN        = 0,    /**< (0) Unknown or invalid command set. */
    FL_FLASH_COMMAND_SET_NONE           = 1,    /**< (1) No command set defined. */
    FL_FLASH_COMMAND_SET_SPANSION       = 2,    /**< (2) Spansion FPGA flash specific commands. */
    FL_FLASH_COMMAND_SET_MICRON         = 3,    /**< (3) Micron FPGA flash specific commands. */
    FL_FLASH_COMMAND_SET_MACRONIX       = 4,    /**< (4) Macronix FPGA flash specific commands (they are compatible with Micron).
                                                         Used for example in Savona cards. */
    FL_FLASH_COMMAND_SET_SF2            = 5,    /**< (5) SF2 admin controller commands (for example Mango cards). */
    FL_FLASH_COMMAND_SET_GIANT_GECKO    = 6     /**< (6) Giant Gecko admin controller commands (for example Savona cards). */

} FL_FlashCommandSet;

/**
 *  Micron flash read modes.
 */
typedef enum
{
    FL_MICRON_READ_MODE_UNKNOWN     = 0,     /**< (0)  */
    FL_MICRON_READ_MODE_ARRAY       = 1,     /**< (1)  */
    FL_MICRON_READ_MODE_STATUS      = 2,     /**< (2)  */
    FL_MICRON_READ_MODE_ID          = 3,     /**< (3)  */
    FL_MICRON_READ_MODE_CFI         = 4      /**< (4)  */

} FL_MicronFlashReadMode;

/**
 *  Manufacturer of SF2 SPI flash.
 */
typedef enum
{
    FL_SF2_SPI_FLASH_MANUFACTURER_ID_MICRON     = 0x20,     /**< (0x20) SF2 SPI flash manufacturer is Micron. */
    FL_SF2_SPI_FLASH_MANUFACTURER_ID_ADESTO     = 0x1F      /**< (0x1F) SF2 SPI flash manufacturer is Adesto. */

} FL_SF2SpiFlashManufacturer;

/**
 *  Manufacturer of SF2 SPI device identifiers.
 */
typedef enum
{
    FL_SF2_SPI_FLASH_DEVICE_ID1_MICRON              = 0x80, /**< (0x80) */
    FL_SF2_SPI_FLASH_DEVICE_ID2_MICRON              = 0x40, /**< (0x40) */
    FL_SF2_SPI_FLASH_DEVICE_ID1_ADESTO              = 0x26, /**< (0x26) */
    FL_SF2_SPI_FLASH_DEVICE_ID2_ADESTO              = 0x00  /**< (0x00) */

} FL_SF2SpiFlashDeviceId;

/**
 *  Giant Gecko commands.
 *
 *  
 */
typedef enum
{
    FL_GECKO_CMD_NOP                                = 0x00, /**< (0) No operation. */
    FL_GECKO_CMD_WRITE_FLASH                        = 0x01, /**< (1) Write flash. */
    FL_GECKO_CMD_READ_FLASH                         = 0x02, /**< (2) Read flash. */
    FL_GECKO_CMD_LOG_AREA_READ                      = 0x03, /**< (3) Read the log area. */
    FL_GECKO_CMD_READ_TELEMETRY                     = 0x04, /**< (4) Read board telemetry information. */

    /* Commands 0x5, 0x6 are unused */

    FL_GECKO_CMD_CONTROL                            = 0x07  /**< (7) Execute one of the @ref FL_GeckoSubcommand subcommands. */

} FL_GeckoCommand;

/**
 *  Giant Gecko subcommands.
 */
typedef enum
{
    /* Gecko Control sub commands */
    FL_GECKO_SUBCMD_CTRL_GECKO_STATUS                   = 0x01, /**< (0x01) Return Giant Gecko status. */
    FL_GECKO_SUBCMD_CTRL_FLASH_STATUS                   = 0x02, /**< (0x02) Return Giant Gecko flash status. */
    FL_GECKO_SUBCMD_CTRL_SELECT_PRIMARY_FPGA_FLASH      = 0x03, /**< (0x03) Select primary FPGA flash for QSPI operations. */
    FL_GECKO_SUBCMD_CTRL_SELECT_FAILSAFE_FPGA_FLASH     = 0x04, /**< (0x04) Select primary FPGA failsafe for QSPI operations. */

    /* Write flash @ref GECKO_CMD_WRITE_FLASH sub commands */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_IMAGE_1           = 0x01, /**< (0x01)  */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_PLL               = 0x02, /**< (0x02)  */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_1    = 0x03, /**< (0x03) Reserved for internal Silicom use. */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_2    = 0x04, /**< (0x04) Reserved for internal Silicom use. */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_3    = 0x05, /**< (0x05) Reserved for internal Silicom use. */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_SILICOM_AREA_4    = 0x06, /**< (0x06) Reserved for internal Silicom use. */
    FL_GECKO_SUBCMD_FLASH_WRITE_START_IMAGE_2           = 0x07, /**< (0x07)  */
    FL_GECKO_SUBCMD_FLASH_WRITE_ERASE_PAGES             = 0x08, /**< (0x08)  */
    FL_GECKO_SUBCMD_FLASH_WRITE_DATA                    = 0x09, /**< (0x09)  */
    FL_GECKO_SUBCMD_FLASH_WRITE_END                     = 0x0A, /**< (0x0A)  */

    /* Read flash @ref GECKO_CMD_READ_FLASH sub commands */
    FL_GECKO_SUBCMD_FLASH_READ_IMAGE_1                  = 0x01, /**< (0x01)  */
    FL_GECKO_SUBCMD_FLASH_READ_PLL                      = 0x02, /**< (0x02)  */
    FL_GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_1           = 0x03, /**< (0x03)  */
    FL_GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_2           = 0x04, /**< (0x04)  */
    FL_GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_3           = 0x05, /**< (0x05)  */
    FL_GECKO_SUBCMD_FLASH_READ_SILICOM_AREA_4           = 0x06, /**< (0x06)  */
    FL_GECKO_SUBCMD_FLASH_READ_IMAGE_2                  = 0x07, /**< (0x07)  */

    /* Telemetry read @ref GECKO_CMD_READ_TELEMETRY sub commands */
    FL_GECKO_SUBCMD_17V                                 = 0x40, /**< (0x40)  */
    FL_GECKO_SUBCMD_PCIE_12V_SOURCE                     = 0x41, /**< (0x41)  */
    FL_GECKO_SUBCMD_AUX_12V_SOURCE                      = 0x42, /**< (0x42)  */
    FL_GECKO_SUBCMD_PCIE_12V_OUTPUT_CURRENT             = 0x43, /**< (0x43)  */
    FL_GECKO_SUBCMD_AUX_12V_OUTPUT_CURRENT              = 0x44, /**< (0x44)  */
    FL_GECKO_SUBCMD_INIT_CPU_1V8                        = 0x45, /**< (0x45)  */
    FL_GECKO_SUBCMD_DDR_VTT_AB_0V6                      = 0x46, /**< (0x46)  */
    FL_GECKO_SUBCMD_DDR_VTT_CD_0V6                      = 0x47, /**< (0x47)  */
    FL_GECKO_SUBCMD_FPGA_VCORE1_0V85                    = 0x48, /**< (0x48)  */
    FL_GECKO_SUBCMD_FPGA_VCORE2_0V85                    = 0x49, /**< (0x49)  */
    FL_GECKO_SUBCMD_FPGA_MGT_AVCC_0V9                   = 0x4A, /**< (0x4A)  */
    FL_GECKO_SUBCMD_FPGA_1V2                            = 0x4B, /**< (0x4B)  */
    FL_GECKO_SUBCMD_FPGA_1V8                            = 0x4C, /**< (0x4C)  */
    FL_GECKO_SUBCMD_DDR4_2V5                            = 0x4D, /**< (0x4D)  */
    FL_GECKO_SUBCMD_FPGA_V_3V3                          = 0x4E, /**< (0x4E)  */
    FL_GECKO_SUBCMD_FPGA_I_3V3                          = 0x4F, /**< (0x4F)  */
    FL_GECKO_SUBCMD_CPU_TEMP                            = 0x50, /**< (0x50)  */
    FL_GECKO_SUBCMD_FPGA_CORE_TEMP                      = 0x51, /**< (0x51)  */
    FL_GECKO_SUBCMD_VCORE_TEMP                          = 0x52, /**< (0x52)  */
    FL_GECKO_SUBCMD_FAN_TEMP                            = 0x53, /**< (0x53)  */
    FL_GECKO_SUBCMD_DDR4_TEMP                           = 0x54, /**< (0x54)  */
    FL_GECKO_SUBCMD_QSFP1_TEMP                          = 0x55, /**< (0x55)  */
    FL_GECKO_SUBCMD_QSFP2_TEMP                          = 0x56, /**< (0x56)  */
    FL_GECKO_SUBCMD_PLL_REVISION                        = 0x57, /**< (0x57)  */
    FL_GECKO_SUBCMD_FLASH_STATUS                        = 0x58, /**< (0x58)  */
    FL_GECKO_SUBCMD_WRITE_COMMIT                        = 0x59, /**< (0x59)  */

} FL_GeckoSubcommand;

/**
 * Type to chose Gecko flash area
 */
typedef enum
{
    FL_GECKO_FLASH_PLL,            /**< Select PLL area */
    FL_GECKO_FLASH_SILICOM_AREA_1, /**< Select Silicom area 1 */
    FL_GECKO_FLASH_SILICOM_AREA_2, /**< Select Silicom area 2 */
    FL_GECKO_FLASH_SILICOM_AREA_3, /**< Select Silicom area 3 */
    FL_GECKO_FLASH_SILICOM_AREA_4, /**< Select Silicom area 4 */

} FL_GeckoFlashArea;

/**
 *  Silicom Denmark card types.
 */
typedef enum
{
    /* Add other card types after demand, please retain alphabetical sorting! */

    FL_CARD_TYPE_UNKNOWN,
    FL_CARD_TYPE_RAW,

    FL_CARD_TYPE_20XX,
    FL_CARD_TYPE_ANCONA,
    FL_CARD_TYPE_ATHEN,
    FL_CARD_TYPE_BERGAMO,
    FL_CARD_TYPE_CARDIFF,
    FL_CARD_TYPE_COMO,
    FL_CARD_TYPE_ERCOLANO,
    FL_CARD_TYPE_ESSEN,
    FL_CARD_TYPE_FIRENZE,
    FL_CARD_TYPE_GENOA,
    FL_CARD_TYPE_HERCULANEUM,
    FL_CARD_TYPE_LATINA,
    FL_CARD_TYPE_LIVIGNO,
    FL_CARD_TYPE_LIVORNO,
    FL_CARD_TYPE_LUCCA,
    FL_CARD_TYPE_LUXG,
    FL_CARD_TYPE_MANGO,
    FL_CARD_TYPE_MARSALA,
    FL_CARD_TYPE_MILAN,
    FL_CARD_TYPE_MONZA,
    FL_CARD_TYPE_PADUA,
    FL_CARD_TYPE_PISA,
    FL_CARD_TYPE_RIMINI,
    FL_CARD_TYPE_SAVONA,
    FL_CARD_TYPE_SILIC100,
    FL_CARD_TYPE_TIVOLI,
    FL_CARD_TYPE_TORINO,

} FL_CardType;

/**
 *  Onboard microcontroller/admin controller type.
 */
typedef enum
{
    FL_ADMIN_TYPE_UNKNOWN           = 0,        /**< (0) Unknown microcontroller type. */
    FL_ADMIN_TYPE_NONE              = 1,        /**< (1) No microcontroller present on the card. */
    FL_ADMIN_TYPE_ATMEL             = 2,        /**< (2) Atmel microcontroller. */
    FL_ADMIN_TYPE_SMART_FUSION_2    = 3,        /**< (3) Microcontroller on card is Microsemi (used to be Actel) SmartFusion2 SoC aka SF2. */
    FL_ADMIN_TYPE_GIANT_GECKO       = 4         /**< (4) Microcontroller on card is Silicon Labs Giant Gecko. */

} FL_AdminType;

/**
 *  PCIe boot modes.
 */
typedef enum
{
    FL_PCIE_BOOT_MODE_NONE          = 0,        /**< (0) No or unknown PCIe boot mode. */
    FL_PCIE_BOOT_MODE_SINGLE        = 1,        /**< (1) PCIe boot mode single. */
    FL_PCIE_BOOT_MODE_SIMULTANEOUS  = 2,        /**< (2) PCIe boot mode simultaneous. */
    FL_PCIE_BOOT_MODE_SEQUENTIAL    = 3,        /**< (3) PCIe boot mode sequential. */

    FL_PCIE_BOOT_MODE_MAX           = 3         /**< (3) Max value of PCIe boot mode. */

} FL_PCIeBootMode;

/**
 *  Flash image target types.
 */
typedef enum
{
    FL_IMAGE_TARGET_PRIMARY             = 0,    /**< (0) Primary flash image target (AKA probe image). */
    FL_IMAGE_TARGET_SECONDARY           = 1,    /**< (1) Secondary flash image target (AKA fail safe or golden image). */
    FL_IMAGE_TARGET_CLOCK_CALIBRATION   = 2,    /**< (2) Clock calibration flash image target. */
    FL_IMAGE_TARGET_NUMBER_OF_TARGETS   = 3     /**< (3) Total number of different image targets. */

} FL_ImageTarget;

/**
 *  A bit mask that defines different library initialization configurations,
 *  i.e. how much and what to initialize.
 */
typedef enum
{
    FL_INITIALIZATION_LEVEL_NONE                    = 0x00,    /**< (0x00) No initialization specified, does nothing at all. */
    FL_INITIALIZATION_LEVEL_MAPPING                 = 0x01,    /**< (0x01) Library initialization that only maps FPGA registers base to PCIe BAR0. */
    FL_INITIALIZATION_LEVEL_BASIC                   = 0x03,    /**< (0x03) Library initialization that maps FPGA registers base
                                                                    and reads information from basic FPGA register zero like
                                                                    FPGA revision, FPGA type, FPGA model and derives from these card and admin type. */
    FL_INITIALIZATION_LEVEL_FULL                    = 0x07     /**< (0x07) Runs a full library initialization reading in all available
                                                                    configuration and status information. */

} FL_InitializationLevel;

/**
 *  Define a boolean type due to possible size differences between C and C++ bool.
 */
typedef uint32_t FL_Boolean;

/**
 *  Convenience redefines of some Silicom Denmark internal types.
 */
typedef pcie_vendor_id_t FL_PCIeVendorId;       /**< Library PCIe vendor id. */
typedef pcie_device_id_t FL_PCIeDeviceId;       /**< Library PCIe device id. */
typedef fpga_type_t FL_FpgaType;                /**< Library FPGA type. */
typedef fpga_model_t FL_FpgaModel;              /**< Library FPGA model. */

/**
 *  Signature definition of Silicom internal logging function.
 */

/**
 *  User error handler function signature.
 *
 *  @param  pUserContext        Pointer to user defined context.
 *  @param  fileName            Name of file where the log occurred.
 *  @param  lineNumber          Line number in file where the log occurred.
 *  @param  functionName        Name of function where log occurred.
 *  @param  errorCode           Library error code @ref FL_Error.
 *  @param  format              Format string compatible with printf.
 *  @param  arguments           Arguments list.
 *
 *  @return     Return true if the handler processed the error message, false if the message was not handled and the library should handle it.
 */
typedef FL_Error (*FL_pUserErrorHandler)(void * pUserContext, const char * fileName, int lineNumber, const char * functionName, FL_Error errorCode, const char * format, va_list arguments);

/**
 *  Internal log function signature.
 *
 *  @param  logMask             Log mask.
 *  @param  format              Format string compatible with printf.
 *  @param  arguments           Arguments list.
 */
typedef void (*FL_pInternalLoggerV)(uint32_t logMask, const char * format, va_list arguments);

/**
 *  Internal log function signature.
 *
 *  @param  logMask             Log mask.
 *  @param  format              Format string compatible with printf.
 */
typedef void (*FL_pInternalLogger)(uint32_t logMask, const char * format, ...);

/**
 *  Legacy log message function signature.
 *
 *  @param  fileName            Name of file where the log occurred.
 *  @param  lineNumber          Line number in file where the log occurred.
 *  @param  functionName        Name of function where log occurred.
 *  @param  logLevel            Log level @ref FL_LogLevel of the message.
 *  @param  format              Format string compatible with printf.
 *  @param  arguments           Arguments list.
 *
 *  @return     Return true if the handler processed the error message, false if the message was not handled and the library should handle it.
 */
typedef bool (*FL_pLegacyLogMessage)(const char * fileName, int lineNumber, const char * functionName, FL_LogLevel logLevel, const char * format, va_list arguments);

/**
 *  Legacy error message function signature.
 *
 *  @param  fileName            Name of file where the error occurred.
 *  @param  lineNumber          Line number in file where the error occurred.
 *  @param  functionName        Name of function where error occurred.
 *  @param  errorCode           Library error code @ref FL_Error.
 *  @param  format              Format string compatible with printf.
 *  @param  arguments           Arguments list.
 *
 *  @return     Return true if the handler processed the error message, false if the message was not handled and the library should handle it.
 */
typedef bool (*FL_pLegacyErrorMessage)(const char * fileName, int lineNumber, const char * functionName, FL_Error errorCode, const char * format, va_list arguments);

/**
 *  Logging context.
 */
typedef struct
{
    FL_LogLevel *           pLogLevel;                  /**< Pointer to the log level. */
    FL_pInternalLoggerV     pLegacyLoggerV;             /**< Pointer to a Silicom Denmark internal legacy log function.
                                                             Only used by Silicom internal applications. */
    FL_pInternalLogger      pLegacyLogger;              /**< Pointer to a Silicom Denmark internal legacy log function.
                                                             Only used by Silicom internal applications. */
    FL_pLegacyLogMessage    pLegacyLogMessage;          /**< Pointer to a legacy log message handler used in relation to Silicom in-house applications.
                                                            This function converts and outputs log messages in the legacy format instead of the format
                                                            defined in the Flash Library. */

} FL_LogContext;

/**
 *  Error handling context.
 */
typedef struct
{
    FL_Error                LastErrorCode;              /**< Last error code from a Flash Library API function. */
    FL_pUserErrorHandler    pUserErrorHandlerV;         /**< Pointer to a user defined error handling function. */
    void *                  pUserContext;               /**< Pointer to a user defined context passed to the user defined error handling function @ref pUserErrorHandlerV. */
    FL_pInternalLoggerV     pLegacyErrorLoggerV;        /**< Pointer to a legacy error andler used in relation to Silicom in-house applications. */
    FL_pInternalLogger      pLegacyErrorLogger;         /**< Pointer to a legacy error handler used in relation to Silicom in-house applications. */
    FL_pLegacyErrorMessage  pLegacyErrorMessage;        /**< Pointer to a legacy error message handler used in relation to Silicom in-house applications.
                                                             This function converts and outputs error messages in the legacy format instead of the format 
                                                             defined in the Flash Library. */
    FL_LogContext *         pLogContext;                /**< Pointer to the logging context. */

} FL_ErrorContext;

/**
 *  Flash information that can be updated even on const @ref FL_FlashInfo.
 */
typedef struct
{
    FL_MicronFlashReadMode  MicronFlashReadMode;    /**< Micron flash read mode @ref FL_MicronFlashReadMode. */

} FL_UpdatableFlashInfo;

/**
 *  Detailed FPGA flash information read via QSPI.
 */
typedef struct
{
    FL_FlashType            Type;                       /**< Flash memory type. */
    uint32_t                Size;                       /**< Flash memory size in bytes. */

} FL_DetailedFlashInfo;

/**
 *  FPGA related configuration and features information.
 */
typedef struct
{
    uint64_t                RawRevision;                    /**< Full 64-bit revision register raw contents. */
    uint8_t                 Major;                          /**< Major revision number. */
    uint8_t                 Minor;                          /**< Minor revision number. */
    uint8_t                 Sub;                            /**< Sub revision number. */
    uint32_t                Build;                          /**< Revision (legacy) build number. But have place for the 32-bit extended build number too. */
    uint64_t                RawBuildTime;                   /**< Raw Firmware build time value as read from the FPGA. */
    struct tm               BuildTime;                      /**< Firmware build time as struct tm local time. */
    char                    BuildTimeString[30];            /**< Firmware build time as local time string in format YYYY-MM-DD HH:MM:SS. */
    uint32_t                ExtendedBuild;                  /**< Extended build number. */
    uint64_t                RawMercurialRevision;           /**< Raw Mercurial revision read from the FPGA. */
    FL_Boolean              MercurialRevisionIsDirty;       /**< Mercurial @ref MercurialRevision revision number was not clean. */
    uint64_t                MercurialRevision;              /**< Mercurial version control system revision number of FPGA firmware. */
    const char *            PartNumber;                     /**< Part number or name. */
    FL_FpgaType             Type;                           /**< FPGA type identifier. */
    FL_FpgaModel            Model;                          /**< Model identifier. */
    uint8_t                 MaximumNumberOfLogicalPorts;    /**< Maximum number of logical ports (aka lanes) based on FPGA type. */
    uint8_t                 NumberOfPhysicalPorts;          /**< Number of physical ports based on FPGA type. */
    FL_CardType             CardType;                       /**< Silicom card type based based on FPGA type.  */
    FL_AdminType            AdminType;                      /**< Type of the administrative micro controller on the card. */

    FL_LogContext *         pLogContext;                    /**< Pointer to logging context. */
    FL_ErrorContext *       pErrorContext;                  /**< Pointer to error handling context. */

} FL_FpgaInfo;

/**
 *  Flash related configuration and features information.
 */
typedef struct
{
    uint32_t *      pRegistersBaseAddress;      /**< PCIe BAR0 memory address of FPGA registers . */
    uint32_t        FlashSize;                  /**< Size of flash in bytes. */
    uint8_t         AddressSize;                /**< Address size of the flash. */

    uint32_t        LowSectorCount;             /**< Number of low sectors. */
    uint32_t        MiddleSectorCount;          /**< Number of middle sectors. */
    uint32_t        TopSectorCount;             /**< Number of top sectors. */
    uint32_t        SectorCount;                /**< Total sector count. */

    uint32_t        SectorZeroSize;             /**< Size of sector zero. */
    uint32_t        LowSectorSize;              /**< Size of low sector. */
    uint32_t        MiddleSectorSize;           /**< Size of middle sector. */
    uint32_t        TopSectorSize;              /**< Size of top sector. */

    uint32_t        BankSize;                   /**< Size of a single bank. */
    uint32_t        BankCount;                  /**< Number of flash banks. */
    uint32_t        BufferCount;                /**< Buffer size in 16-bit words? */

    // All times in microseconds
    uint32_t        TypicalWordProgramTime;     /**< Typical flash word program time in microseconds. */
    uint32_t        TypicalBufferProgramTime;   /**< Typical flash program time in microseconds. */
    uint32_t        TypicalSectorEraseTime;     /**< Typical sector erase time in microseconds. */
    uint32_t        MaxWordProgramTime;         /**< Maximum time to flash a word in microseconds. */
    uint32_t        MaxBufferProgramTime;       /**< Maximum time to program flash in microseconds. */
    uint32_t        MaxSectorEraseTime;         /**< Maximum time to erase a sector in microseconds. */
    uint32_t        MaxBulkEraseTime;           /**< Maximum time in microseconds that a bulk erase of whole flash takes. */

    uint32_t        SectorsPerImage[FL_IMAGE_TARGET_NUMBER_OF_TARGETS];     /**< Number of sectors of different image targets. */
    uint32_t        ImageStartAddress[FL_IMAGE_TARGET_NUMBER_OF_TARGETS];   /**< Start addresses of different image targets. */

    FL_FlashType            Type;               /**< Flash type based on FPGA type. */

    FL_FlashCommandSet      FlashCommandSet;    /**< Flash command set to use. */

    FL_DetailedFlashInfo    DetailedFlashInfo;  /**< Detailed FPGA flash information read via QSPI. */

    FL_UpdatableFlashInfo   Updatable;          /**< Updatable flash information even on a constanct @ref FL_FlashInfo. */

    FL_LogContext *         pLogContext;        /**< Pointer to logging context. */
    FL_ErrorContext *       pErrorContext;      /**< Pointer to error handling context. */

    FL_FpgaInfo *           pFpgaInfo;          /**< Pointer to FPGA information. */

} FL_FlashInfo;

/**
 *  PCIe bus end point information.
 */
typedef struct
{
    FL_Boolean              LinkUpAndReady;             /**< True if PICe bus link is up and ready, false otherwise. Generation and width are only valid 
                                                             link is up and ready. */
    uint8_t                 Generation;                 /**< PCIe generation (1,2, or 3). */
    uint8_t                 Width;                      /**< PCIe bus width that the card has negotiated to use. Can be less than the card PCIe slot width. */
    uint8_t                 ExpectedGeneration;         /**< Expected PCIe bus generation based on FPGA model. */
    uint8_t                 ExpectedWidth;              /**< Expected PCIe bus width based on FPGA model. */

} FL_PCIeEndPointInfo;

/**
 *  PCIe bus related information.
 */
typedef struct
{
    uint64_t                Status;                     /**< 64-bit PCIe bus status. */
    FL_PCIeEndPointInfo     EndPoints[2];               /**< Information about 2 PCIe end points. */

} FL_PCIeInfo;

/**
 *  Command line parameters.
 */
typedef struct
{
    FL_InitializationLevel  RequiredInitializationLevel;            /**< Required library initialization level. */
    FL_InitializationLevel  CurrentInitializationLevel;             /**< Current library initialization level. */

    char                    DeviceName[100];                        /**< Device name. For example "/dev/fbcard0" or "fbcard0" or "/dev/smartnic" or "smartnic".
                                                                         "/dev/" is automatically prependended to the name if it is missing and the device does not exist.
                                                                         For example device name "fbcard0" is tried first and if that fails then "/dev/fbcard0" is tried. */
    FL_Boolean              UseForceProgramming;                    /**< Program the FPGA even if the existing imgage type is not compatible with the new one.
                                                                         This overrides the usual protection that an existing FPGA image cannot be replaced with
                                                                         an incompatible image. Default value is false i.e. do not replace with incompatible image. */
    FL_Boolean              SkipMemoryMapping;                      /**< Do not map the FPGA register memory to user space. Default is false i.e. FPGA register memory
                                                                         is mapped to user space. */
    int                     ExternalCommandPosition;                /**< An external command was detected on the command line and this value tells the start index in argv.
                                                                         The rest of the command line is parsed in each particual command. */
    FL_FlashFileFormat      FlashFileFormat;                        /**< File format of an flash image file. */
    FL_Boolean              ReadPcbVersion;                         /**< Read and display PCB version. */
    const char *            FlashFileName[FL_IMAGE_TARGET_NUMBER_OF_TARGETS];       /**< Flash primary image if FlashFileName[0] is not NULL, flash safe image if FlashFileName[1] is not NULL
                                                                                         and flash clock calibration image if FlashFileName[2] is not NULL. */
    const char *            FlashVerifyFileName[FL_IMAGE_TARGET_NUMBER_OF_TARGETS]; /**< Verify primary image if FlashVerifyFileName[0] is not NULL, verify safe image if FlashVerifyFileName[1] is not NULL
                                                                                         and verify clock calibration image if FlashVerifyFileName[2] is not NULL. */
    FL_Boolean              FlashErase[FL_IMAGE_TARGET_NUMBER_OF_TARGETS];          /**< Erase primary image if FlashErase[0] is true and erase fail safe image if FlashErase[1] is true.
                                                                                         Default values are false. FlashErase[2] is unused. */
    FL_FpgaType             ExpectedBitFileFpgaType;                /**< Expected FPGA type in the FPGA image bit file. */
    bool                    PCIeBootModeReadOrWrite;                /**< Read of write value of PCIe boot mode @ref FL_PCIeBootMode if true, do nothing if false. */
    FL_PCIeBootMode         PCIeBootMode;                           /**< PCIe boot mode @ref FL_PCIeBootMode. */
    FL_Boolean              CalculateClockCalibrationImageMD5Sum;   /**< Calculate MD5 sum of the flash clock calibration image. */
    uint8_t                 ImpersonatePCIeGeneration;              /**< Impersonate PCIe bus generation 1, 2, or 3 or 0 for using the actual PCIe generation decoded from PCIe status register. */
    uint8_t                 ImpersonatePCIeWidth;                   /**< Impersonate PCIe bus width or 0 for using the actual PCIe width decoded from PCIe status register. */
    FL_LogLevel             LogLevel;                               /**< Defines logging level in the library. Default value is FL_LOGGING_LEVEL_INFO. */
    FL_Boolean              ReadFlashStatus;                        /**< True if flash status should be read and displayed, false otherwise. */
    FL_Boolean              ClearFlashStatus;                       /**< True if flash status should cleared, false otherwise. */
    FL_Boolean              FlashIs8Bit;                            /**< True if flash is 8-bit, false otherwise. */

    FL_LogContext *         pLogContext;                            /**< Pointer to logging context. */
    FL_ErrorContext *       pErrorContext;                          /**< Pointer to error handling context. */

} FL_Parameters;

/**
 *  Forward definition of @ref FL_FlashLibraryContext.
 */
struct FL_FlashLibraryContext;

/**
 *  Signature of a command execution function.
 */
typedef int (*FL_pCommandFunction)(struct FL_FlashLibraryContext * pFlashLibraryContext, int argc, char * argv[]);

/**
 *  Command table entry definition for "-c" command line option commands.
 *  CommandName and HelpText must be supported by external storage, the internal
 *  command table only contains the pointers;
 */
typedef struct
{
    const char *            CommandName;                    /**< Command line parameter "-c" command name. */
    FL_pCommandFunction     pCommandFunction;               /**< Pointer to a function to execute the named function. */
    FL_InitializationLevel  RequiredInitializationLevel;    /**< Library initialization level required by each command. */
    const char *            HelpText;                       /**< Help text for the command. */

} FL_CommandEntry;

/**
 *  A common context for the whole Silicom Flash Library that contains as subsets various
 *  information categories.
 */
typedef struct FL_FlashLibraryContext
{
    FL_LogContext               LogContext;                     /**< Logging context. */
    FL_ErrorContext             ErrorContext;                   /**< Error handling context. */

    int                         DeviceFileDescriptor;           /**< File descriptor of opened device file. */

    FL_FlashInfo                FlashInfo;                      /**< Flash related configuration and features basic information. */
    FL_FpgaInfo                 FpgaInfo;                       /**< FPGA related configuration and features information. */
    FL_PCIeInfo                 PCIeInfo;                       /**< PCIe bus information. */
    FL_Parameters               Parameters;                     /**< Publicly available (command line) parameters. */

    FL_FlashFileFormat          FlashFileFormat;                /**< Flash file format. */

    const FL_CommandEntry *     Commands;                       /**< Pointer to "-c" command line option commands. */

} FL_FlashLibraryContext;

/**
 *  Signature definition of Silicom internal initialization function.
 */
typedef void (*FL_InternalInitializer)(FL_FlashLibraryContext * pFlashLibraryContext);

/** @} ConstantsMacros */

/** **************************************************************************
 * @defgroup InitializationAndSetupGroup Library initialization and setup.
 * @{
 * @brief Library initialization, configuration and setup.
 */

/**
 *  Register an error handler function.
 *
 *  Initialization of the library and library context should only be done once.
 *
 *  @param  pFlashLibraryContext        Pointer to a uninitialized library context.
 *  @param  pUserErrorHandlerV;         Pointer to a user defined error handling function.
 *  @param  pUserContext                Pointer to optional user defined context that is passed on to the error handler.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_RegisterErrorHandler(FL_FlashLibraryContext * pFlashLibraryContext, FL_pUserErrorHandler pUserErrorHandlerV, void * pUserContext);

/**
 *  Add "-c" command line option commands that are supported by the library.
 *  These commands can be implemented externally and are added to the internal command set
 *  already supported by the library. Replacing an existing command with a new implementation
 *  is allowed, i.e. if a new command has the same name as an already existing command then
 *  the existing entry is replaced with the new one effectively giving an existing command
 *  a new external implementation. Multiple entries of the same command name are not checked
 *  and only the first found name is an executable command, later duplicates are ignored.
 *
 *  @param  pFlashLibraryContext    Pointer to flash library context.
 *  @param  newCommands             Pointer to a command definition array of @ref FL_CommandEntry entries.
 *                                  The array MUST end with a termination entry that has NULL as CommandName.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AddCommands(FL_FlashLibraryContext * pFlashLibraryContext, const FL_CommandEntry newCommands[]);

/**
 *  Determine and set library initialization type.
 *
 *  @param  pFlashLibraryContext    Pointer to flash library context.
 *  @param  command                 One of the "-c" option commands.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_SetLibraryInitializationForCommand(FL_FlashLibraryContext * pFlashLibraryContext, const char * command);

/**
 *  Output flash library command line option "-c" commands help.
 *
 *  @param  pFlashLibraryContext    Pointer to flash library context.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_CommandsHelp(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Parse and execute command line option "-c" command.
 *
 *  @param  pFlashLibraryContext    Pointer to flash library context.
 *  @param  argc                    Number of command arguments.
 *  @param  argv                    Pointers to command and its arguments.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_ExecuteCommand(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char * argv[]);

/**
 *  Open Silicom device file.
 *
 *  This function is publicly available but normally you should have no reason
 *  to call it from your own application because device opening is already handled
 *  by @ref FL_OpenFlashLibrary which should be used instead.
 *
 *  Initialization of the library and library context should only be done once.
 *
 *  @param  pFlashLibraryContext        Pointer to a uninitialized library context.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_OpenDevice(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Close an opened device.
 *
 *  Closing the device also releases allocated system resources.
 *  This function is publicly available but normally you should have no reason
 *  to call it from your own application because device closing is already handled
 *  by @ref FL_CloseFlashLibrary which should be used instead.
 *
 *  @param  pFlashLibraryContext        Pointer to a uninitialized library context.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_CloseDevice(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Initialize flash library context.
 *
 *  Initialization of the library context after allocation.
 *
 *  @param  pFlashLibraryContext        Pointer to a uninitialized library context.
 *  @param  pInternalInitializer        Pointer to Silicom internal initialization function or NULL.
 */
void FL_InitializeFlashLibraryContext(FL_FlashLibraryContext * pFlashLibraryContext, FL_InternalInitializer pInternalInitializer);

/**
 *  Initialize library and library context.
 *
 *  Initialization of the library and library context should only be done once.
 *
 *  @param  pFlashLibraryContext        Pointer to a uninitialized library context.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_OpenFlashLibrary(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Close the flash programming library.
 *
 *  Closing the flash programming library releases and closes all allocated resources.
 *
 *  @param  pFlashLibraryContext        Pointer to a uninitialized library context.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_CloseFlashLibrary(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Set the last error code returned by a library function call.
 *
 *  @param  pErrorContext   Pointer to an error context @ref FL_ErrorContext.
 *  @param  errorCode       The @ref FL_Error error code to set as last error.
 *
 *  @return     The value of parameter @p errorCode.
 */
FL_Error FL_SetLastErrorCode(FL_ErrorContext * pErrorContext, FL_Error errorCode);

/**
 *  Get the last error code returned by a library function call.
 *
 *  @param  pErrorContext      Pointer to an error context.
 *
 *  @return     The last error code returned by a library function call.
 */
FL_Error FL_GetLastErrorCode(const FL_ErrorContext * pErrorContext);

/**
 *  Call Flash Library error handler and return an @ref FL_Error error code.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  fileName        Name of the file where the error occurred.
 *  @param  lineNumber      Line number in file where the error occurred.
 *  @param  functionName    Name of the function where the error occurred.
 *  @param  errorCode       An @ref FL_Error error code that is returned by default.
 *  @param  format          C language printf compatible format statement.
 *
 *  @return                 Return @p errorCode by default but internal or user supplied error handler can optionally return another @ref FL_Error error code.
 */
FL_Error FL_ErrorDetails(FL_ErrorContext * pErrorContext, const char * fileName, int lineNumber, const char * functionName, FL_Error errorCode, const char * format, ...) __attribute__((format(printf, 6, 7)));

/**
 *  Call Flash Library error handler and return an @ref FL_Error error code.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  errorCode       An @ref FL_Error error code that is returned by default.
 *  @param  format          C language printf compatible format statement.
 *  @param  args            C language printf compatible arguments.
 *
 *  @return                 Return @p errorCode by default but internal or user supplied error handler can optionally return another @ref FL_Error error code.
 */
#define FL_Error(pErrorContext, errorCode, format, args...)  FL_ErrorDetails(pErrorContext, __FILE__, __LINE__, __func__, errorCode, format, ##args)

/**
 *  Log information inside library functions based on different log levels.
 *
 *  @param  pLogContext         Pointer to a logging context.
 *  @param  loggingLevel        Logging level @ref FL_LogLevel.
 *  @param  format              C language printf compatible format statement followed by optional values.
 *  @param  args                Variable arguments.
 */
#define FL_Log(pLogContext, loggingLevel, format, args...)     FL_LogDetails(pLogContext, __FILE__, __LINE__, __func__, (loggingLevel), (format), ##args)

/**
 *  Log information inside library functions based on different log levels.
 *
 *  @param  pLogContext         Pointer to a logging context.
 *  @param  fileName            Name of the file where logging was called from.
 *  @param  lineNumber          Line number in the file where logging was called from.
 *  @param  functionName        Name of the function where logging was called from.
 *  @param  loggingLevel        Logging level @ref FL_LogLevel.
 *  @param  format              C language printf compatible format statement followed by optional values.
 */
void FL_LogDetails(const FL_LogContext * pLogContext, const char * fileName, int lineNumber, const char * functionName, FL_LogLevel loggingLevel, const char * format, ...) __attribute__((format(printf, 6, 7)));

/**
 *  Get basic log levels excluding high bit mask logging options.
 *
 *  @param  logLevel            Log level that possibly includes high bit option masks.
 *
 *  @return                     Return basic logging level that does not include high bit mask options.
 */
FL_LogLevel FL_GetBasicLogLevel(FL_LogLevel logLevel);

/**
 *  Log limited information about the call stack if an error occurred.
 *
 *  @param  pLogContext         Pointer to a logging context.
 *
 *  @return                     True if error details like file name, line number and function name should be printed.
 */
FL_Boolean FL_LogErrorCallStack(const FL_LogContext * pLogContext);

/**
 *  Parse command line parameters.
 *
 *  @param  argc                    Number of arguments.
 *  @param  argv                    Array of argumenets.
 *  @param  pArg                    Pointer to how far argument parsing advanced before stopping.
 *  @param  pFlashLibraryContext    Pointer to flash library context.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_ParseCommandLineArguments(int argc, char * argv[], int * const pArg, FL_FlashLibraryContext * pFlashLibraryContext);

/** @} InitializationAndSetupGroup */

/** **************************************************************************
 * @defgroup FpgaFunctions Functions that operate on the FPGA.
 * @{
 * @brief Functions that operate on the FPGA.
 */

 /**
 *  Get full value of FPGA revision.
 *
 *  @param  pRegistersBaseAddress       Base address of FPGA registers.
 *
 *  @return     Full 64-bit value of FPGA revision.
 */
uint64_t FL_GetRevision(void * pRegistersBaseAddress);

/**
 *  Get FPGA Mercurial revision.
 *
 *  @param  pRegistersBaseAddress    Base address of FPGA registers.
 *
 *  @return     64-bit value of FPGA Mercurial revision (changeset number).
 */
uint64_t FL_GetMercurialRevision(void * pRegistersBaseAddress);

/**
 *  Get value of FPGA extended build number.
 *
 *  @param  pRegistersBaseAddress    Base address of FPGA registers.
 *
 *  @return     32-bit value of FPGA extended build number.
 */
uint32_t FL_GetExtendedBuildNumber(void * pRegistersBaseAddress);

/**
 *  Get FPGA build time as raw value.
 *
 *  @param  pRegistersBaseAddress    Base address of FPGA registers.
 *
 *  @return     FPGA build time as raw value.
 */
uint64_t FL_GetBuildTime(void * pRegistersBaseAddress);

 /**
 *  Return the name of a PCIE vendor id as string.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  pcieVendorId    PCIE vendor identifier.
 *  @param  fullName        Full enum name if true, short name if false.
 *
 *  @return                 Name of PCIE vendor id or NULL if not found.
 */
const char * FL_GetVendorIdName(FL_ErrorContext * pErrorContext, FL_PCIeVendorId pcieVendorId, bool fullName);

/**
 *  Return the name of a PCIE device id as string.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  pcieDeviceId    PCIE device identifier.
 *  @param  fullName        Full enum name if true, short name if false.
 *
 *  @return                 Name of PCIE device id or NULL if not found.
 */
const char * FL_GetDeviceIdName(FL_ErrorContext * pErrorContext, FL_PCIeDeviceId pcieDeviceId, bool fullName);

/**
 *  Return the name of a FPGA type as string.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  fpgaType        FPGA type identifier.
 *  @param  fullName        Full enum name if true, short name if false.
 *
 *  @return                 Name of FPGA type or NULL if not found.
 */
const char * FL_GetFpgaTypeName(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType, bool fullName);

/**
 *  Return the name of a FPGA model as string.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  fpgaModel       FPGA model identifier.
 *  @param  fullName        Full enum name if true, short name if false.
 *
 *  @return                 Name of FPGA model or NULL if not found.
 */
const char * FL_GetFpgaModelName(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel, bool fullName);

/**
 *  Return the name of a card type as string.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  cardType        Card type idenfifier.
 *  @param  fullName        Full enum name if true, short name if false.
 *
 *  @return                 Name of card type or NULL if not found.
 */
const char * FL_GetCardTypeName(FL_ErrorContext * pErrorContext, FL_CardType cardType, bool fullName);

/**
 *  Return the name of a PCIe boot mode as string.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  pcieBootMode    PCIe boot mode @ref FL_PCIeBootMode.
 *  @param  fullName        Full enum name if true, short name if false.
 *
 *  @return                 Name of PCIe boot mode or NULL if not found.
 */
const char * FL_GetPCIeBootModeName(FL_ErrorContext * pErrorContext, FL_PCIeBootMode pcieBootMode, bool fullName);

/**
 *  Get Silicom Denmark card type from the FPGA type.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  fpgaType        FPGA type identifier.
 *
 *  @return                 Card identifier.
 */
FL_CardType FL_GetCardTypeFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType);

/**
 *  Get Silicom Denmark card microcontroller/admin controller type from the FPGA type.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  fpgaType        FPGA type identifier.
 *
 *  @return                 Microcontroller/admin controller type.
 */
FL_AdminType FL_GetAdminTypeFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType);

/**
 *  Get card part number from the FPGA model.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  fpgaModel       FPGA model identifier.
 *
 *  @return                 Card Silicom part number.
 */
const char * FL_GetPartNumberFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel);

/**
 *  Get flash type from FPGA type.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaType        FPGA type.
 *
 *  @return                 Flash type identifier.
 */
FL_FlashType FL_GetFlashTypeFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType);

/**
 *  Test if FPGA type is a test FPGA.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaType        FPGA type.
 *
 *  @return                 Return true if FPGA type is a test FPGA, false otherwise.
 */
FL_Boolean FL_IsTestFpgaFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType);

/**
 *  Get number of physical ports based on the FPGA type.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaType        FPGA type.
 *
 *  @return                 Number of physical ports.
 */
uint8_t FL_GetNumberOfPhysicalPorts(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType);

/**
 *  Get number of logical ports based (aka lanes) on the FPGA type.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaType        FPGA type.
 *
 *  @return                 Number of logical ports (aka lanes).
 */
uint8_t FL_GetNumberOfLogicalPorts(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType) __attribute__((error("FL_GetNumberOfLogicalPorts is not supported in this release")));

/**
 *  Get maximum number of logical ports (aka lanes) based on the FPGA type. This means maximum number
 *  of logical ports (aka lanes) that any FPGA image for this card can have. Usually this means number
 *  of 10G logical ports (aka lanes) that a particular card FPGA image can have.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaType        FPGA type.
 *
 *  @return                 Maximum number of logical ports (aka lanes).
 */
uint8_t FL_GetMaximumNumberOfLogicalPorts(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType);

/**
 *  Get expected PCIe generation from the FPGA model. Note that the actual PCIe
 *  generation used to access the card can be lower due to PCIe bus negotiations.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaModel       FPGA model identifier.
 *
 *  @return                 PCIe generation supported by this FPGA model.
 */
uint8_t FL_GetExpectedPCIeGenerationFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel);

/**
 *  Get expected PCIe link state from the FPGA model.
 *  Virtex 6 cards do not set link true.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaModel       FPGA model identifier.
 *
 *  @return                 PCIe expected link state true or false.
 */
FL_Boolean FL_GetExpectedPCIeLinkStateFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel);

/**
 *  Get expected PCIe width from the FPGA model. Note that the actual PCIe
 *  width used to access the card can be lower due to PCIe bus negotiations.
 *
 *  @param  pErrorContext   Pointer to error context.
 *  @param  fpgaModel       FPGA model identifier.
 *
 *  @return                 PCIe width supported by this FPGA model.
 */
uint8_t FL_GetExpectedPCIeWidthFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel);

/**
 *  Get decoded FPGA revision information.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pFpgaInfo           Pointer to FPGA information where revision information is being returned.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetFpgaRevisionInformation(const FL_FlashInfo * pFlashInfo, FL_FpgaInfo * pFpgaInfo);

/**
 *  Get decoded FPGA build time information.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pFpgaInfo           Pointer to FPGA information where build time information is being returned.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetFpgaBuildTimeInformation(const FL_FlashInfo * pFlashInfo, FL_FpgaInfo * pFpgaInfo);

/**
 *  Get decoded PCIe information.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pFpgaInfo           Pointer to FPGA information.
 *  @param  pParameters         Pointer to parameters information.
 *  @param  pPCIeInfo           Pointer to FPGA information where PCIe related information is returned.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetPCIeInformation(const FL_FlashInfo * pFlashInfo, const FL_FpgaInfo * pFpgaInfo, const FL_Parameters * pParameters, FL_PCIeInfo * pPCIeInfo);

/**
 *  Write 2-byte words to flash memory sector zero.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  flashWordAddress    Word aligned flash address to write to
 *. @param  pWordData           Word aligned data to write to flash.
 *  @param  lengthInWords       Number of words to write to flash.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_WriteSectorZeroWords(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint16_t * pWordData, uint16_t lengthInWords);

/**
 *  Tell if there is a primary flash present.
 *
 *  @param  pFpgaInfo           Pointer to FPGA information.
 *  @param  pHasPrimaryFlash    Pointer to a @ref FL_Boolean variable that receives true
 *                              if there is a primary flash and false otherwise.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_HasPrimaryFlash(const FL_FpgaInfo * pFpgaInfo, FL_Boolean * pHasPrimaryFlash);

/**
 *  Tell if there is a secondary flash present.
 *
 *  @param  pFpgaInfo           Pointer to FPGA information.
 *  @param  pHasSecondaryFlash  Pointer to a @ref FL_Boolean variable that receives true
 *                              if there is a secondary flash and false otherwise.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_HasSecondaryFlash(FL_FpgaInfo * pFpgaInfo, FL_Boolean * pHasSecondaryFlash);

/**
 *  Select primary (probe) Flash image.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_SelectPrimaryImage(const FL_FlashInfo * pFlashInfo);

/**
 *  >Select secondary (golden, fail-safe) image.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_SelectSecondaryImage(const FL_FlashInfo * pFlashInfo);

/**
 *  Write 2-byte words to flash memory.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  flashWordAddress    Word aligned flash address to write to.
 *. @param  pWordData           Word aligned data to write to flash.
 *  @param  lengthInWords       Number of words to write to flash.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_WriteFlashWords(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint16_t * pWordData, uint32_t lengthInWords);

/**
 *  Get flash related information.
 *
 *  @param  pFlashLibraryContext        Pointer to flash library context.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetFlashInformation(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Erase a flash sector.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  sector              Sector index to erase.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashEraseSector(const FL_FlashInfo * pFlashInfo, uint32_t sector);

/**
 *  Test if flash sector is locked.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  sector              Sector index to check locking.
 *  @param  pSectorIsLocked     Pointer to a @ref FL_Boolean variable that is set to true if sector is locked
 *                              and to false if sector is unlocked.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashSectorIsLocked(const FL_FlashInfo * pFlashInfo, uint32_t sector, FL_Boolean * pSectorIsLocked);

/**
 *  Lock a flash sector.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  sector              Sector index to lock.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashSectorLock(const FL_FlashInfo * pFlashInfo, uint32_t sector);

/**
 *  Unlock all flash sectors.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashUnlockAllSectors(const FL_FlashInfo * pFlashInfo);

/**
 *  Write protect flash image.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  imageTarget         Image target @ref FL_ImageTarget to protect.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashProtectImage(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget);

/**
 *  Remove write protection of flash image.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  imageTarget         Image target @ref FL_ImageTarget to unprotect.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashUnprotectImage(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget);

/**
 *  Erase flash image optionally based on an image file size.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  imageTarget         Image target @ref FL_ImageTarget to erase.
 *  @param  fileName            File name to base the sectors to erase or NULL to erase whole flash.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashEraseImage(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget, const char * fileName);

/**
 *  Flash a bit image file to flash memory.
 *
 *  @param  pParameters         Pointer to parameters information @ref FL_Parameters.
 *  @param  pFlashInfo          Pointer to flash information @ref FL_FlashInfo.
 *  @param  fileName            File name to base the sectors to erase or NULL to erase whole flash.
 *  @param  imageType           Image type @ref FL_ImageTarget to unprotect.
 *  @param  expectedPartNumber  Name of expected part number, one of the strings returned by @ref
 *                              FL_GetPartNumberFromFpgaModel.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashWriteImage(
    const FL_Parameters *   pParameters,
    const FL_FlashInfo *    pFlashInfo,
    const char *            fileName,
    FL_ImageTarget          imageType,
    const char *            expectedPartNumber);

/**
 *  Verify flash image data against a bit image file.
 *
 *  @param  pParameters         Pointer to parameters information @ref FL_Parameters.
 *  @param  pFlashInfo          Pointer to flash information @ref FL_FlashInfo.
 *  @param  imageTarget         Image target @ref FL_ImageTarget to unprotect.
 *  @param  fileName            File name to base the sectors to verify against.
 *  @param  expectedPartNumber  Name of expected part number, one of the strings returned by @ref
 *                              FL_GetPartNumberFromFpgaModel.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashVerifyImage(
    const FL_Parameters *   pParameters,
    const FL_FlashInfo *    pFlashInfo,
    const char *            fileName,
    FL_ImageTarget          imageTarget,
    const char *            expectedPartNumber);

/**
 *  Top level function that erase, flashes and verifies in one go.
 *
 *  @param  pFlashLibraryContext    Pointer to a flash library context @ref FL_FlashLibraryContext.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashEraseAndWriteAndVerify(FL_FlashLibraryContext * pFlashLibraryContext);

/**
 *  Read card PCB version number.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  pPcbVersion     Pointer to variable that receives PCB version number.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_ReadPcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t * pPcbVersion);

/**
 *  Write card PCB version number.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  pcbVersion      PCB version number to write.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_WritePcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t pcbVersion);

/**
 *  Read card PCIe boot mode.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  pPciBootMode    Pointer to variable that receives PCIe boot mode.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_ReadPciBootMode(const FL_FlashInfo * pFlashInfo, uint16_t * pPciBootMode);

/**
 *  Write card PCIe boot mode.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  pciBootMode     PCIe boot mode to write.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_WritePciBootMode(const FL_FlashInfo * pFlashInfo, uint16_t pciBootMode);

/** @} FpgaFunctions */

/** **************************************************************************
 * @defgroup FileFunctions Functions that operate on the files.
 * @{
 * @brief Functions that operate on the files.
 */

/**
 *  Read Xilinx bit file header. The bit file is usually created with the "bitgen" program.
 *  For more information about Xilinx bit file format please see for example
 *  <a href="http://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm">Tell me about the .BIT file format</a>.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  fileName            File name to read.
 *  @param  expectedPartNumber  Expected part number.
 *  @param  expectedUserid      Expected user identifier.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FileReadXilinxBitFileHeader1(const FL_FlashInfo * pFlashInfo, const char * fileName, const char * expectedPartNumber, uint32_t expectedUserid);

/**
 *  Read Xilinx bit file header. The bit file is usually created with the "bitgen" program.
 *  For more information about Xilinx bit file format please see for example
 *  <a href="http://www.fpga-faq.com/FAQ_Pages/0026_Tell_me_about_bit_files.htm">Tell me about the .BIT file format</a>.
 *
 *  @param  pFlashInfo                  Pointer to flash information.
 *  @param  pFile                       Pointer to C FILE structure.
 *  @param  currentFpgaPartNumber       Expected part number in the file, this is from the current FPGA and
 *                                      and has to be compatible with the part number read from the file.
 *  @param  expectedBitFileFpgaType     Expected image bit file FPGA type. This value is checked
 *                                      against the value of the bit file header "UserID" field
 *                                      which can contain the FPGA type of the file FPGA image or
 *                                      0xFFFFFFFF for no value.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FileReadXilinxBitFileHeader2(
    const FL_FlashInfo *    pFlashInfo,
    FILE *                  pFile,
    const char *            currentFpgaPartNumber,
    FL_FpgaType             expectedBitFileFpgaType);

/**
 *  Check if an existing FPGA type can be upgraded to a new FPGA type.
 *
 *  @param  pErrorContext       Pointer to an error context.
 *  @param  existingFpgaType    Existing FPGA type.
 *  @param  newFpgaType         New FPGA type.
 *
 *  @return                     @ref FL_SUCCESS if @p existingFpgaType can be upgraded to
 *                              @p newFpgaType, otherwise returns error code @ref
 *                              FL_ERROR_FPGA_TYPES_DO_NOT_MATCH_FOR_UPGRADE.
 */
FL_Error FL_IsUpgradableFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType existingFpgaType, FL_FpgaType newFpgaType);

/**
 *  Check that bit file is a known and supported type.
 *
 *  @param  pFlashInfo                  Pointer to flash information.
 *  @param  fileName                    File name to read.
 *  @param  expectedPartNumber          Expected part number.
 *  @param  expectedBitFileFpgaType     Expected bit file FPGA type.
 *  @param  pParameters                 Pointer to parameters.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_CheckFlashFileType(const FL_FlashInfo * pFlashInfo, const char * fileName, const char * expectedPartNumber, FL_FpgaType expectedBitFileFpgaType, FL_Parameters * pParameters);


/**
 *  Verify bit file contents.
 *
 *  @param  pFlashLibraryContext        Pointer to flash library context.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_VerifyBitFiles(FL_FlashLibraryContext * pFlashLibraryContext);

/** @} FileFunctions */

/** **************************************************************************
 * @defgroup FlashFunctions Functions that operate on the flash.
 * @{
 * @brief Functions that operate on the flash.
 */

/**
 *  Get flash size (???).
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pFlashSize          Pointer to variable that receives the flash size in kilobytes.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetFlashSize(FL_FlashInfo * pFlashInfo, uint16_t * pFlashSize);

/**
 *  Convert flash word address to a sector number.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  flashWordAddress    Flash word address.
 *  @param  pSector             Pointer to variable that receives the sector number.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashConvertAddressToSector(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint32_t * pSector);

/**
 *  Convert sector number to a flash word address.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  sector              Sector number.
 *  @param  pFlashWordAddress   Pointer to variable that receives the flash word address.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_FlashConvertSectorToAddress(const FL_FlashInfo * pFlashInfo, uint32_t sector, uint32_t * pFlashWordAddress);

/**
 *  Check if Flash Memory is ready.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  flashWordAddress    2-byte word aligned flash address that is used to test flash readiness.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS when flash is ready.
 */
FL_Error FL_FlashIsReady(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress);

/**
 *  Wait for Flash Memory to become ready.
 *
 *  @param  pFlashInfo              Pointer to flash information.
 *  @param  flashWordAddress        2-byte word aligned flash address.
 *  @param  timeoutInMilliseconds   Timeout in milliseconds.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS when flash is ready.
 */
FL_Error FL_FlashWaitIsReady(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint32_t timeoutInMilliseconds);

/** @} FlashFunctions */

/** **************************************************************************
* @defgroup AdminProtocol Admin controller low level commands sent over the SPI bus.
* @{
* @brief Admin controller low level commands sent over the SPI bus.
*/

/**
 *  Admin commands.
 */
typedef enum
{
    FL_ADMIN_NOP,               /**< TBD */
    FL_ADMIN_READ_FLASH,        /**< TBD */
    FL_ADMIN_WRITE_FLASH,       /**< TBD */

} FL_AdminCommand;

/**
 *  Return info about admin controller presence or not on this card. This is static
 *  information based on the @ref FL_FlashType.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  flashType       Flash type of the card.
 *
 *  @return                 @ref FL_SUCCESS if card has admin controller, @ref FL_ERROR_NO_ADMIN_CONTROLLER_PRESENT otherwise.
 */
FL_Error FL_AdminIsControllerPresent(const FL_FlashInfo * pFlashInfo, FL_FlashType flashType);

/**
 *  Get admin controller information.
 *
 *  @param  pFlashInfo      Pointer to admin SPI flash information.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminGetConfigurationInformation(FL_FlashInfo * pFlashInfo);

/**
 *  Poll flash status until ready or failed.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on status ready.
 */
FL_Error FL_AdminWaitStatusReady(const FL_FlashInfo * pFlashInfo);

/**
 *  Get admin controller flash is ready status.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  flashWordAddress    2-byte word aligned flash address that is used to test flash readiness.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on flash ready.
 */
FL_Error FL_AdminFlashIsReady(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress);

/**
 *  Read a 2-byte word from SPI controller command register.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  pFlashWord      Pointer to 2-byte word that receives the word read from the SPI
 *                          controller command register.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminReadCommandRegister(const FL_FlashInfo * pFlashInfo, uint16_t * pFlashWord);

/**
 *  Read a 2-byte word from SPI controller.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  flashAddress    Word aligned flash address to read from.
 *  @param  pWordData       Pointer to 2-byte word that receives the flash data.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminReadWord(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress, uint16_t * pWordData);

/**
 *  Read status from SPI controller.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  pAdminStatus    Pointer to 2-byte word that receives the SPI controller status.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminReadStatus(const FL_FlashInfo * pFlashInfo, uint16_t * pAdminStatus);

/**
 *  Fill admin controller read buffer.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  flashAddress    Word aligned flash address to read from.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminFillReadBuffer(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress);

/**
 *  Get a 2-byte word from SPI controller. The read is only performed if @p *pErrorCode equals @ref FL_SUCCESS.
 *  This makes this function callable several times before the error code is actually checked. *pErrorCode is
 *  unchanged if it is not FL_SUCCESS.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  flashAddress    Word aligned flash address to read from.
 *  @param  pErrorCode      Pointer to Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 *
 *  @return                 2-byte word read.
 */
uint16_t FL_AdminGetWord(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress, FL_Error * pErrorCode);

/**
 *  Write a command to the SPI controller which is implemented the FPGA.
 *
 *  @param  pFlashInfo      Pointer to flash information.
 *  @param  command         SPI controller command to execute.
 *  @param  flashAddress    Word aligned address to write to.
 *  @param  wordData        2-byte word data value to write to SPI controller.
 *
 *  @return                 Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminWriteCommand(const FL_FlashInfo * pFlashInfo, uint16_t command, uint32_t flashAddress, uint16_t wordData);

/**
 *  Get version of flash controller.
 *
 *  @param  pRegistersBaseAddress   Base address of the flash register space.
 *
 *  @return                     Return the flash controller version.
 */
uint8_t FL_AdminGetControllerVersion(void * pRegistersBaseAddress);

/**
 *  Get software revision of the flash controller.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pSoftwareRevision   Pointer to a 32-bit word that receives the flash controller
 *                              software revision.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminGetSoftwareRevision(const FL_FlashInfo * pFlashInfo, uint32_t * pSoftwareRevision);

/**
 *  Reset flash controller.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_AdminReset(const FL_FlashInfo * pFlashInfo);

/** @} AdminProtocol */

/** **************************************************************************
 * @defgroup GeckoAccess Access to Giant Gecko microcontroller.
 * @{
 * @brief Low level access to Giant Gecko microcontroller.
 */

/* not implemented yet
FL_Error FL_GeckoReadMacAddress(const FL_FlashInfo * pFlashInfo);
FL_Error FL_GeckoWriteMacAddress(const FL_FlashInfo * pFlashInfo);
FL_Error FL_GeckoReadPcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t * pPcbVersion);
FL_Error FL_GeckoWritePcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t pcbVersion);
FL_Error FL_GeckoEraseAndFlashAndVerify(const FL_FlashInfo * pFlashInfo);
*/

/**
 *  Select primary flash via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GeckoSelectPrimaryFlash(const FL_FlashInfo * pFlashInfo);

/**
 *  Select failsafe flash via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GeckoSelectFailSafeFlash(const FL_FlashInfo * pFlashInfo);

/** @} GeckoAccess */

/** **************************************************************************
 * @defgroup QSPI_functions QSPI access functions.
 * @{
 * @brief Low level access to FPGA via QSPI.
 */

 /**
  *  Wait until QSPI AXI controller is ready.
  *
  *  @param  pFlashInfo          Pointer to flash information.
  *
  *  @return                     Negative @ref FL_Error error code controller is not ready or @ref FL_SUCCESS when controller is ready.
  */
FL_Error FL_QSPI_AXI_WaitIsReady(const FL_FlashInfo * pFlashInfo);

/**
 *  Read data from the QSPI AXI controller.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  axireg              AXI register identifier.
 *  @param  pData               Pointer to read data.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_AXI_Read(const FL_FlashInfo * pFlashInfo, uint64_t axireg, uint32_t * pData);

/**
 *  Write data to the QSPI AXI controller.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  axireg              AXI register identifier.
 *  @param  data                Data to write.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_AXI_Write(const FL_FlashInfo * pFlashInfo, uint64_t axireg, uint64_t data);

/**
 *  Transfer (read/write) data via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pReadBuffer         Pointer to byte buffer that receives read data.
 *  @param  readBufferSize      Read buffer size.
 *  @param  pWriteBuffer        Pointer to byte data that is written to QSPI.
 *  @param  writeBufferSize     Write buffer size.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_Transfer(const FL_FlashInfo * pFlashInfo, uint8_t * pReadBuffer, uint16_t readBufferSize, const uint8_t * pWriteBuffer, uint16_t writeBufferSize);

/**
 *  Read flash status register.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pStatus             Pointer to byte that receives the value of the status register.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadStatusRegister(const FL_FlashInfo * pFlashInfo, uint8_t * pStatus);

/**
 *  Read flash flag status register.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pFlagStatus         Pointer to byte that receives the value of the flag status register.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadFlagStatusRegister(const FL_FlashInfo * pFlashInfo, uint8_t * pFlagStatus);

/**
 *  Wait until flash is ready.
 *
 *  @param  pFlashInfo              Pointer to flash information.
 *  @param  timeoutInMilliseconds   Maximum time to wait in milliseconds.
 *
 *  @return                     Negative @ref FL_Error error code flash is not ready or @ref FL_SUCCESS when flash is ready.
 */
FL_Error FL_QSPI_WaitFlashIsReady(const FL_FlashInfo * pFlashInfo, int32_t timeoutInMilliseconds);

/**
 *  Check if flash is ready.
 *
 *  @param   pFlashInfo         Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code flash is not ready or @ref FL_SUCCESS if flash is ready.
 */
FL_Error FL_QSPI_IsFlashReady(const FL_FlashInfo * pFlashInfo);

/**
 *  Execute a flash command via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  command             Flash command to execute.
 *  @param  commandData         Related command data or zero if not relevant.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ExecuteCommand(const FL_FlashInfo * pFlashInfo, uint8_t command, uint8_t commandData);

/**
 *  Enable flash writes via QSPI.
 *
 *  @param   pFlashInfo         Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_EnableFlashWrite(const FL_FlashInfo * pFlashInfo);

/**
 *  Read detailed FPGA flash information via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  pDetailedFlashInfo  Pointer to detailed flash information structure @ref FL_DetailedFlashInfo.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadFlashInfo(const FL_FlashInfo * pFlashInfo, FL_DetailedFlashInfo * pDetailedFlashInfo);

/**
 *  Read bytes FPGA flash via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  flashByteAddress    Byte index to flash address to read from.
 *  @param  readBuffer          Pointer to byte buffer that receives the read bytes.
 *  @param  readBufferSize      Maximum size or capacity of @p readBuffer.
 *  @param  pNumberOfReadBytes  Pointer to a variable that receives the number of bytes actually read.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadBytes(const FL_FlashInfo * pFlashInfo, uint32_t flashByteAddress, uint8_t readBuffer[], uint32_t readBufferSize, uint32_t * pNumberOfReadBytes);

/**
 *  Bulk erase FPGA flash via QSPI.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_BulkErase(const FL_FlashInfo * pFlashInfo);


/**
 *  Read FPGA flash via QSPI into a memory buffer.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  startAddress        Flash start address where reading starts.
 *  @param  buffer              Pointer to a memory buffer where data is read into.
 *  @param  bufferLength        Length of memory buffer. This also determines the number of bytes read from the flash.
 *  @param  showProgress        True if read progress should be shown, false otherwise.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadFlashToBuffer(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint8_t * buffer, uint32_t bufferLength, bool showProgress);

/**
 *  Read FPGA flash via QSPI into an already open file.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  startAddress        Flash start address where reading starts.
 *  @param  length              Length of flash to read.
 *  @param  fileName            Name of a file to write read flash data into. Only .bin files supported.
 *  @param  pFile               Pointer to an already open FILE structure. Possible file headers should
 *                              already have been written when calling this function.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadFlashToFile(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName, FILE * pFile);

/**
 *  Read FPGA flash via QSPI into a file.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  startAddress        Flash start address where reading starts.
 *  @param  length              Length of flash to read.
 *  @param  fileName            Name of a file to write read flash data into. Only .bin files supported.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ReadFlashToFileName(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName);

/**
 *  Program FPGA flash via QSPI from an already opem file.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  fileName            Name of a file to program. Only .bit and .bin files supported.
 *  @param  pFile               Pointer to a FILE structure of an already open file. Function assumes
 *                              that all file headers have already been read and binary data starts
 *                              current file location.
 *  @param  showProgress        True if read progress should be shown, false otherwise.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ProgramFile(const FL_FlashInfo * pFlashInfo, const char * fileName, FILE * pFile, bool showProgress);

/**
 *  Program FPGA flash via QSPI from a file.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  fileName            Name of a file to program. Only .bit and .bin files supported.
 *  @param  showProgress        True if read progress should be shown, false otherwise.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_ProgramFileName(const FL_FlashInfo * pFlashInfo, const char * fileName, bool showProgress);

/**
 *  Verify FPGA flash via QSPI against an already open file.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  startAddress        Flash start address where verify starts.
 *  @param  length              Length of flash to verify.
 *  @param  fileName            Name of a file to verify against. Only .bit and .bin files supported.
 *  @param  pFile               Pointer to a FILE structure of an already open file. Function assumes
 *                              that all file headers have already been read and binary data starts
 *                              current file location.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_VerifyFile(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName, FILE * pFile);

/**
 *  Verify FPGA flash via QSPI against a file.
 *
 *  @param  pFlashInfo          Pointer to flash information.
 *  @param  startAddress        Flash start address where verify starts.
 *  @param  length              Length of flash to verify.
 *  @param  fileName            Name of a file to verify against. Only .bit and .bin files supported.
 *
 *  @return                     Negative @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_QSPI_VerifyFileName(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName);

/** @} QSPI_functions */

/** **************************************************************************
 * @defgroup LowLevelAccessGroup Low level access functions.
 * @{
 * @brief Low level access functions to memory and registers.
 */

/**
 *  Read a 32-bit value from a memory address with an offset.
 *
 *  @param      pRegistersBaseAddress   Address of the beginning of the memory block to read from.
 *  @param      byteOffset              Byte offset from the beginning of the memory block in bytes.
 *
 *  @return                      32-bit value read from the memory.
 */
uint32_t FL_Read32Bits(const void * pRegistersBaseAddress, uint32_t byteOffset);

/**
 *  Write a 32-bit value to a memory address with an offset.
 *
 *  @param      pRegistersBaseAddress   Address of the beginning of the memory block to write to.
 *  @param      byteOffset              Byte offset from the beginning of the memory block in bytes.
 *  @param      value                   32-bit value to write into memory.
 */
void FL_Write32Bits(void * pRegistersBaseAddress, uint64_t byteOffset, uint32_t value);

/**
 *  Read a 64-bit value from a memory address with an offset.
 *
 *  @param      pRegistersBaseAddress   Address of the beginning of the memory block to read from.
 *  @param      byteOffset              Byte offset from the beginning of the memory block in bytes.
 *
 *  @return                     64-bit value read from the memory.
 */
uint64_t FL_Read64Bits(const void * pRegistersBaseAddress, uint64_t byteOffset);

/**
 *  Write a 64-bit value to a memory address with an offset.
 *
 *  @param      pRegistersBaseAddress   Address of the beginning of the memory block to write to.
 *  @param      byteOffset              Byte offset from the beginning of the memory block in bytes.
 *  @param      value                   64-bit value to write into memory.
 */
void FL_Write64Bits(void * pRegistersBaseAddress, uint64_t byteOffset, uint64_t value);

/** @} LowLevelAccessGroup */

/** **************************************************************************
 *  @defgroup UtilityFunctionsGroup   Miscellaneous utility functions.
 *  @{
 *  @brief 
 */

/**
 *  Swap all bits in a 16-bit word.
 *
 *  @param  word    16-bit word to swap all bits.
 *
 *  @return         16-bit word with all bits swapped.
 */
uint16_t FL_Swap16Bits(uint16_t word);

/**
 *  Swap all bits in a 32-bit double word.
 *
 *  @param  dword   32-bit double word to swap all bits.
 *
 *  @return         32-bit double word with all bits swapped.
 */
uint32_t FL_Swap32Bits(uint32_t dword);

/**
 *  Swap all bits in a 64-bit double word.
 *
 *  @param  qword   64-bit quad word to swap all bits.
 *
 *  @return         64-bit quadword with all bits swapped.
 */
uint64_t FL_Swap64Bits(uint64_t qword);

/**
 *  Swap bytes in a 16-bit word.
 *
 *  @param  word    16-bit double word to swap all bits.
 *
 *  @return         16-bit word with bytes swapped.
 */
uint16_t FL_SwapWordBytes(uint16_t word);

/**
 *  Print a hex dump of memory..
 *
 *  @param  pPrint          Pointer to a printf compatible function or NULL to use printf.
 *  @param  pMemory         Pointer to a memory area to dump.
 *  @param  length          Length of memory area to dump.
 *  @param  bytesPerLine    Number of bytes per line to dump.
 *  @param  addressOffset   Offset to add to memory address.
 */
void FL_HexDump(int(*pPrint)(const char * format, ...), const void * pMemory, uint64_t length, uint16_t bytesPerLine, ssize_t addressOffset);

/**
 *  Get a timestamp in nanoseconds elapsed from some arbitrary
 *  fixed point in the past. The resulting timestamp has no relation
 *  to any specific time of day or date.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  pTimeStamp      Pointer to returned current timestamp in nanoseconds.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetTimestamp(FL_ErrorContext * pErrorContext, uint64_t * pTimeStamp);

/**
 *  Get a raw timestamp in nanoseconds elapsed from some arbitrary
 *  fixed point in the past. The resulting timestamp has no relation
 *  to any specific time of day or date.
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  pRawTimeStamp   Pointer to returned current raw timestamp in nanoseconds.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetRawTimestamp(FL_ErrorContext * pErrorContext, uint64_t * pRawTimeStamp);

/**
 *  Get current system time in nanoseconds since the Epoch (UTC 00:00:00 1 January 1970).
 *
 *  @param  pErrorContext   Pointer to an error context.
 *  @param  pTimeNow        Pointer to returned current system time in nanoseconds.
 *
 *  @return                 Negatice @ref FL_Error error code on failure or @ref FL_SUCCESS on success.
 */
FL_Error FL_GetTimeNow(FL_ErrorContext * pErrorContext, uint64_t * pTimeNow);

/** @} UtilityFunctionsGroup */

/** **************************************************************************
 * @defgroup MD5Functions MD5 related functions.
 * @{
 * @brief MD5 related functions.
 */

/**
 *  MD5 context data.
 */
typedef struct
{
    uint32_t    DataLength;     /**< Length of transformed data in bytes. */
    uint8_t     Data[64];       /**< Snapshot of data to transform. */
    uint64_t    BitLength;      /**< Length of transformed data in bits. */
    uint32_t    State[4];       /**< Containts state of the MD5 transformation. */

} FL_MD5Context;

#define FL_MD5_HASH_SIZE    16  /**< Size of MD5 hash result. */

/**
 *  Start computing a MD 5 hash.
 *
 *  @param  pFlashInfo      Pointer to flash specific context.
 *  @param  imageTarget     Image target @ref FL_ImageTarget.
 *  @param  md5sum          Byte array that receives the MD5 sum.
 */
FL_Error FL_CalculateFlashImageMD5Sum(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget, uint8_t md5sum[FL_MD5_HASH_SIZE]);

/**
 *  Compute image MD 5 hash.
 *
 *  @param  header          Optional header to print out or NULL for no header.
 *  @param  pFlashInfo      Pointer to flash specific context.
 *  @param  md5sum          Byte array that receives the MD5 sum.
 */
void FL_LogMD5Sum(const char * header, const FL_FlashInfo * pFlashInfo, const uint8_t md5sum[FL_MD5_HASH_SIZE]);

/**
 *  Start computing a MD 5 hash.
 *
 *  @param  pMD5Context     Pointer to a MD5 context.
 *
 */
void FL_MD5_Start(FL_MD5Context * pMD5Context);

/**
 *  Start computing a MD 5 hash.
 *
 *  @param  pMD5Context     Pointer to a MD5 context.
 *  @param  data            Pointer to byte array of data to add to MD5.
 *  @param  dataLength      Length of data array.
 *
 */
void FL_MD5_Add(FL_MD5Context * pMD5Context, const uint8_t * data, size_t dataLength);

/**
 *  End computing a MD 5 hash.
 *
 *  @param  pMD5Context     Pointer to a MD5 context.
 *  @param  hash            Pointer to a byte array receiving the MD5 hash.
 *
 */
void FL_MD5_End(FL_MD5Context * pMD5Context, uint8_t hash[FL_MD5_HASH_SIZE]);

/**
 *  Print MD 5 hash.
 *
 *  @param  pFile            Pointer to a FILE file.
 *  @param  md5sum           Pointer to a byte array containing the MD5 sum.
 *
 */
void FL_MD5_Print(FILE * pFile, const uint8_t md5sum[FL_MD5_HASH_SIZE]);

/**
 *  Convert MD 5 hash to a hex string.
 *
 *  @param  hash             Pointer to a byte array containing the MD5 sum.
 *  @param  hashHex          Pointer to a character array receiving the hex string.
 *  @param  upperCaseHex     True for upper case hex output, false for lower case hex output.
 *
 */
const char * FL_MD5_HashToHexString(const uint8_t hash[FL_MD5_HASH_SIZE], char hashHex[2 * FL_MD5_HASH_SIZE + 1], FL_Boolean upperCaseHex);

/** @} MD5Functions */

/**! @cond Doxygen_Suppress */

/**! @endcond */

/*
TODO:
flashClearStatus: does not do anything, what's the idea?
*/


#ifdef __cplusplus

#if 0
/* C++ definitions. */

#include <cstdarg>
#include <memory>
#include <stdexcept>
#include <string>

/** **************************************************************************
 * @addtogroup ConstantsMacros
 * @{
 */

 /**
  *  Optional exception thrown by the library instead of returning @ref FL_Error error code
  *  Please note that C++ exception support is currently an experimental feature and
  *  not recommended in production code.
  */
class FL_Exception : public std::runtime_error
{
private:

    std::string                 _fileName;      /**< Name of the file where the problem occurred. */
    int                         _lineNumber;    /**< Line number in the file where the problem occurred. */
    std::string                 _functionName;  /**< Name of the function where the problem occurred. */
    FL_Error                    _errorCode;     /**< @ref FL_Error error code that indentifies the problem.
                                                     This is the same error code that the API call would return
                                                     if exceptions were not enabled. */
    std::shared_ptr<void>       _pContext;      /**< Optional user specified context registered by @ref FL_RegisterErrorHandler or nullptr. */

    friend class FL_ExceptionFactory; /* Only place to create exceptions! */

    /**
     *  Private exception constructor. Private to allow construction
     *  only inside the API library by FL_ExceptionFactory.
     *
     *  @param  fileName        Name of the file where the problem occurred.
     *  @param  lineNumber      Line number of the file where the problem occurred.
     *  @param  functionName    Name of the function where the problem occurred.
     *  @param  errorCode       FL_Error error code related to this exception.
     *  @param  message         Message string.
     *  @param  pContext        Optional user specified context registered by @ref FL_RegisterErrorHandler or nullptr.
     */
    explicit FL_Exception(const char * fileName, int lineNumber, const char * functionName, FL_Error errorCode, const char * message, void * pContext);

public:

    /**
     *  Exception destructor.
     */
    virtual ~FL_Exception() throw(); /* Some versions of std::runtime_error have this destructor signature so we have to emulate it here. */

    /**
     *  Get the name of the file where the problem occurred.
     *
     *   @return Return the name of the file where the problem occurred.
     */
    const std::string & GetFileName() const;

    /**
     *  Get line number of the file where the problem occurred.
     *
     *   @return Return line number of the file where the problem occurred.
     */
    int GetLineNumber() const;

    /**
     *  Get the name of the function where the problem occurred.
     *
     *   @return Return the name of the funtion where the problem occurred.
     */
    const std::string & GetFunctionName() const;

    /**
     *  Get the @ref FL_Error error code related to this exception.
     *  To get the error message call the base class what() method.
     *
     *   @return Return the error code related to the problem.
     */
    FL_Error GetErrorCode() const;

    /**
     *  Get the user specified context passed to @ref FL_RegisterErrorHandler.
     *
     *   @return Return the optional user specified context.
     */
    void * GetContext() const;
};
#endif

} /* extern "C" */
#endif

#endif /* __FL_FlashLibrary_h__ */
