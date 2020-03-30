#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

#include "printlog.h"

FILE *logFile = NULL;
FILE *acpFile = NULL;
FILE *pScreen = NULL;

FL_LogLevel * pLogLevel = NULL;

#ifdef WIN32

#define FORMAT_BUFFER_SIZE  500

// Fix format strings on Windows to avoid -Wformat warnings
// from mingw C compiler. %ll[dux] formats will printf correctly
// on Windows and Linux but %l[dux] formats print as 32-bit ints on
// Windows and 64-bit ints on Linux.
static int FixWindowsFormat(char * correctedFormat, size_t correctedFormatSize, const char * format)
{
    // Replace %l[dux] formats with %ll[dux]:
    size_t newFormatLength = 0;
    while (*format && newFormatLength < correctedFormatSize)
    {
        if (*format == '%' && *(format + 1) == 'l' && *(format + 2) != 'l')
        {
            *correctedFormat++ = *format++;   // '%'
            *correctedFormat++ = *format++;   // 'l'
            *correctedFormat++ = 'l';         // Insert extra 'l' so format becomes %ll...
        }
        else
        {
            *correctedFormat++ = *format++;
        }
        ++newFormatLength;
    }
    *correctedFormat = '\0';
    return newFormatLength; // Terminating NUL character not included!
}

int _printf(const char * format, ...)
{
    char windowsFormat[FORMAT_BUFFER_SIZE];

    FixWindowsFormat(windowsFormat, sizeof(windowsFormat), format);

    va_list args;
    va_start(args, format);
    int totalNumberOfCharacters = vprintf(windowsFormat, args);
    va_end(args);
    return totalNumberOfCharacters;
}

#endif // WIN32

/**
 * Log to file and screen
 */
void prn( uint32_t mask, const char* format, ... )
{
#ifdef WIN32
    char windowsFormat[FORMAT_BUFFER_SIZE];

    FixWindowsFormat(windowsFormat, sizeof(windowsFormat), format);
    const char * correctFormat = &windowsFormat[0];
#else
    const char * correctFormat = format;
#endif

    va_list args;

    if ((mask & 0xFFFF) <= (uint32_t)FL_GetBasicLogLevel(*pLogLevel))
    {
        if (logFile && (mask & LOG_FILE))
        {
            va_start(args, format);
            vfprintf(logFile, correctFormat, args);
            va_end(args);
            fflush(logFile);
        }
        if (acpFile && (mask & LOG_ACP))
        {
            va_start(args, format);
            vfprintf(acpFile, correctFormat, args);
            va_end(args);
            fflush(acpFile);
        }
        if (pScreen && mask & LOG_SCR)
        {
            va_start(args, format);
            vfprintf(pScreen, correctFormat, args);
            va_end(args);
            fflush(stdout);
        }
    }
}

/**
 *  Log function that takes a va_list of arguments instead of variable number
 *  of arguments (...). Used when FLash Library logs using the legacy logger.
 */
static void vprn(uint32_t logMask, const char * format, va_list arguments)
{
    static char message[1000];

    int messageLength = vsnprintf(message, sizeof(message) - 1, format, arguments);
    if (message[messageLength - 1] != '\n' && message[messageLength - 1] != '\r' && message[0] != '\r')
    {
        strcat(message, "\n");
    }
    prn(LOG_SCR | LOG_FILE | logMask, "%s", message);
}

/***************************************************************************/

/**
 *  This function outputs the legacy log message as seen in the original fbupdate or testfpga.
 *
 *  @param      fileName        Name of the file where message originates from
 *  @param      lineNumber      Line number in the file where the message originates from.
 *  @parama     functionName    Name of the function where the message originates from.
 *  @param      logLevel        Logging level.
 *  @param      format          C language printf function compatible format string.
 *  @param      arguments       Argument list to format arguments.
 *
 *  @return                     Returns true if a legacy log message was handled, false otherwise.
 */
static bool LegacyLogMessage(const char * fileName, int lineNumber, const char * functionName, FL_LogLevel logLevel, const char * format, va_list arguments)
{
    if (strcmp(format, "flashByteAddress: 0x%X lowSectorBoundary: 0x%X middleSectorBoundary: 0x%X\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "ActelFlashAddrToSector: FlashAddr:0x%X Low:0x%X mid:0x%X\n", arguments);
        return true;
    }
    else if (strcmp(format, "flashByteAddress 0x%X sector %u\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "ActelFlashAddrToSector: flashAddr:0x%X sector %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Write protecting FPGA image in flash memory. Protecting sectors %u to %u.\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Write protecting FPGA image in flash memory. Protecting sector %d to %d.\n", arguments);
        return true;
    }
    else if (strcmp(format, "Removing write protection on FPGA image. Sectors %u to %u.\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Removing write protection on FPGA image. Sector %d to %d.\n", arguments);
        return true;
    }
    else if (strcmp(format, "No flash space reserved for image #%u\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "No flash space reserved for image #%d\n", arguments);
        return true;
    }
    else if (strcmp(format, "%s: flash is not ready!\n") == 0)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "WriteFlash cmd: Not ready!\n");
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "Admin cmd: Not ready @ 0x%08X!\n") == 0)
    {
#if 0
        unsigned char args[100];
        unsigned char * p = &args[0];
        *(int *)p = 16;
        p += sizeof(int);
        /*
        *(const char * *)p = "Hello";
        p += sizeof(const char *);
        *(const char * *)p = "world";
        p += sizeof(const char *);
        *(long long *)p = ~0LL;
        p += sizeof(long long);
        */

        typedef int (*pVprintf_t)(const char *, ...);
        pVprintf_t pVprintf = (pVprintf_t)vprintf;
        //pVprintf("%d %s %s 0x%X\n", &args[0]);
        pVprintf("%d\n", &args[0]);
#endif
        printf("HOWDY THERE!\n");
        //const char * functionName = va_arg(arguments, const char *);
        //if (functionName == NULL) return false; // Avoid compiler warning of unused functionName
        //uint32_t flashWordAddress = va_arg(arguments, uint32_t);
        vprn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "SF2ReadFlash cmd: Not ready @ 0x%08X!\n", arguments);
        return true;
    }
    else if (strcmp(format, "Verifying FPGA image\r") == 0)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Verifying FPGA image\r");
        return true;
    }
    else if (strcmp(format, "Verifying flash memory: %u %%  \r") == 0)
    {
        vprn(LOG_SCR | LEVEL_INFO, "Verifying flash memory: %d %%  \r", arguments);
        return true;
    }
    else if (strcmp(format, "Verifying FPGA image: done with no errors      \n") == 0)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Verifying FPGA image: done with no errors      \n");
        return true;
    }
    else if (strcmp(format, "PCB version: %u\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCB version: %d\n", arguments);
        return true;
    }
    else if (strcmp(format, "PCIe boot mode set to: %s\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCI boot mode set to: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "PCIe boot mode: %s\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCI boot mode: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 1 len %u\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 1 len %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 2 len %u\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 2 len %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 3 len %u\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 3 len %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 3: %s\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 3: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "UserID: %s\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "UserID: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "UserID as integer: 0x%X (%d)\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "UserID as integer: 0x%lx\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 4 key %c\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 4 key %c\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 4 len %u\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 4 len %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 4: %s\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 4: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 5 key %c\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 5 key %c\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 5 len %u\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 5 len %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 5: %s\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 5: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 6 key %c\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 6 key %c\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 6 len %u\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 6 len %u\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 6: %s\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 6: %s\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 7 key %c\n") == 0)
    {
        vprn(LOG_SCR | LEVEL_DEBUG, "Field 7 key %c\n", arguments);
        return true;
    }
    else if (strcmp(format, "Field 7 len %u %08X\n") == 0)
    {
        union
        {
            uint32_t data32;
            char     data[4];
        } data;

        uint32_t fieldLength = va_arg(arguments, uint32_t);
        data.data32 = va_arg(arguments, uint32_t);
        prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Field 7 len %u %02X %02X %02X %02X\n",
            fieldLength, data.data[3], data.data[2], data.data[1], data.data[0]);
        return true;
    }
    else if (strcmp(format, "Image type of file %s is bit-file\n") == 0)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Image type is bit-file\n");
        return true;
    }
    else if (strcmp(format, "Image type of file %s is bin-file\n") == 0)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Image type is bin-file\n");
        return true;
    }
    else if (strcmp(format, "Image type of file %s is rbf-file\n") == 0)
    {
        prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Image type is rbf-file\n");
        return true;
    }
    else if (strcmp(format, "%s Status: 0x%04X\n") == 0)
    {
        vprn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "%s Status: 0x%04X\n", arguments);
        return true;
    }
    else if (strcmp(format, "%s Revision %d.%d.%d.%d\n") == 0)
    {
        const char * revisionSource = va_arg(arguments, const char *);
        if (strcmp(revisionSource, "FPGA") == 0)
        {
            revisionSource = "Actel";
        }
        int32_t major = va_arg(arguments, int32_t);
        int32_t minor = va_arg(arguments, int32_t);
        int32_t sub = va_arg(arguments, int32_t);
        int32_t build = va_arg(arguments, int32_t);
        prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "%s Revision %d.%d.%d.%d\n", revisionSource, major, minor, sub, build);
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }
    else if (strcmp(format, "") == 0)
    {
        return true;
    }

    //fprintf(stderr, "Library format not handled in %s: '%s'\n", __func__, format);

    return false;
}

/***************************************************************************/

/**
 *  This function outputs the legacy error message as seen in the original fbupdate or testfpga.
 *
 *  @param      fileName        Name of the file where message originates from
 *  @param      lineNumber      Line number in the file where the message originates from.
 *  @parama     functionName    Name of the function where the message originates from.
 *  @param      errorCode       FLash library error code.
 *  @param      format          C language printf function compatible format string.
 *  @param      arguments       Argument list to format arguments.
 *
 *  @return                     Returns true if a legacy error message was handled, false otherwise.
 */
static bool LegacyErrorMessage(const char * fileName, int lineNumber, const char * functionName, FL_Error errorCode, const char * format, va_list arguments)
{
    switch (errorCode)
    {
        case FL_ERROR_INVALID_CONTROLLER_VERSION:                   vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Invalid Actel controller version.\n", arguments); return true;
        case FL_ERROR_FLASH_INVALID_ADMIN_SOFTWARE_REVISION:        vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Invalid Actel version.\n", arguments); return true;
        case FL_ERROR_INVALID_FLASH_ADDRESS_SIZE:                   vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unlikely Flash Size 0x%X bytes\n", arguments); return true;
        case FL_ERROR_INVALID_FLASH_IMAGE_START_ADDRESS:            vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Unsupported flash image start address. %s:%d\n", arguments); return true;
        case FL_ERROR_FLASH_READY_NO_ACK:                           vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "No ACK from Actel\n", arguments); return true;
        case FL_ERROR_FLASH_READY_TIMEOUT:                          vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Timeout waiting for Actel\n", arguments); return true;
        case FL_ERROR_FAILED_TO_READ_FPGA_REVISION:                 vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "PCIe  [ FAILED ]\n", arguments); return true;
        case FL_ERROR_UNLIKELY_FPGA_REVISION_FOUND:
            vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "FPGA version: %u.%u.%u.%u  [ FAILED ]\n"
                "Unlikely FPGA version found. Please check that driver matches PCI vendor and device ID.\n", arguments);
            return true;
        case FL_ERROR_FAILED_TO_OPEN_DEVICE:                        vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Failed to open: %s\nIs the driver loaded?\n", arguments); return true;
        case FL_ERROR_FAILED_TO_OPEN_ANY_DEVICE:                    vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Failed to open: %s\n", arguments); return true;
        case FL_ERROR_MMAP_FAILED:                                  vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Failed to make mmap. Is the driver loaded?\n", arguments); return true;
        case FL_ERROR_FLASH_IS_NOT_READY:
        case FL_ERROR_SPANSION_FLASH_IS_NOT_READY:
        case FL_ERROR_MICRON_FLASH_IS_NOT_READY:                    vprn(LOG_SCR | LEVEL_TRACE, "ActelFlashIsReady cmd: %04X != %04X != %04X\n", arguments); return true;
        case FL_ERROR_FLASH_WRITE_FILE_IMAGE_FAILED:                vprn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Flash file [ FAILED ] \n", arguments); return true;
        case FL_ERROR_FLASH_VERIFY_FAILED:                          vprn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Flash verify [ FAILED ] \n", arguments); return true;
        case FL_ERROR_IMAGE_FILE_TOO_BIG:                           vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Bit file too big: %s\n", arguments); return true;
        case FL_ERROR_SECTOR_LOCKED_CANNOT_ERASE:                   vprn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "The sectors are locked(%u). Unlock before erase\n", arguments); return true;
        case FL_ERROR_NO_FLASH_SPACE_RESERVED_FOR_IMAGE:            vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "No flash space reserved for image #%u\n", arguments); return true;
        case FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW:              vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: buffer overflow #1, %u > %u\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_1_LENGTH:       vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 1 invalid. len != 9\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_2_LENGTH:       vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 2 invalid. len != 1\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_2_KEY:          vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 2 invalid. key != a\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_KEY:          vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 4 invalid. key != b\n", arguments); return true;
      //case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_PART_NUMBER:  vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file part: %s  != %s\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_5_KEY:          vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 5 invalid. key != c\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_6_KEY:          vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 6 invalid. key != d\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_7_KEY:          vprn(LOG_FILE | LOG_SCR | LEVEL_ERROR, "ERROR: Bit-file header 6 invalid. key != e\n", arguments); return true;
        case FL_ERROR_INVALID_BIT_FILE_FORMAT_BYTES_NOT_SWAPPED:    vprn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "%s should be byte swapped\n", arguments); return true;
        case FL_ERROR_IMAGE_FILE_WRONG_SIZE:                        vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Wrong spi file size: %s\n", arguments); return true;
        case FL_ERROR_INVALID_ADMIN_CONTROLLER_VERSION:             vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Invalid Actel controller version.\n", arguments); return true;
        case FL_ERROR_INVALID_ADMIN_SOFTWARE_VERSION:               vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Invalid SF2 SW version.\n", arguments); return true;
        case FL_ERROR_UNSUPPORTED_ADMIN_SPI_FLASH_TYPE:             vprn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Unsupported SF2 SPI Flash type. flashData:%x \n", arguments); return true;
        case FL_ERROR_UNLIKELY_ADMIN_SPI_FLASH_SIZE:                vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unlikely SF2 SPI Flash Size 0x%X bytes\n", arguments); return true;
        case FL_ERROR_UNKNOWN_IMAGE_FILE_TYPE:                      prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Image type is unknown\n"); return true;

        case FL_ERROR_INVALID_BIT_FILE_FORMAT:
            // TODO: Handle this too:
            // errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT,
            //    "%s is not a valid bit image file. The bit stream should start with 32 x 0xFF.", fileName);

            vprn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "%s is not a valid file. The bit stream should start with 32 x 0xFF\n", arguments);
            return true;
        case FL_ERROR_BIT_FILE_BUFFER_WRITE_OVERFLOW:
            {
                fileName = va_arg(arguments, const char *);
                uint32_t wordCount = va_arg(arguments, uint32_t);
                uint32_t bufferCount = va_arg(arguments, uint32_t);
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: buffer memmove write overflow: %d - 16 > %u\n", wordCount, bufferCount);
            }
            return true;
        case FL_ERROR_BIT_FILE_BUFFER_READ_OVERFLOW:
            {
                fileName = va_arg(arguments, const char *);
                uint32_t wordCount = va_arg(arguments, uint32_t);
                uint32_t bufferCount = va_arg(arguments, uint32_t);
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: buffer memmove read overflow: %d - 16 > %u - 16\n", wordCount, bufferCount);
            }
            return true;
        case FL_ERROR_BIT_FILE_BUFFER_OVERFLOW:
            {
                fileName = va_arg(arguments, const char *);
                uint32_t wordCount = va_arg(arguments, uint32_t);
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: buffer overflow: 16 > %u\n", wordCount);
            }
            return true;
        case FL_ERROR_BIT_FILE_UNEXPECTED_VALUE:
            {
                fileName = va_arg(arguments, const char *);
                const char * hexDump = va_arg(arguments, const char *);
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Wrong value in bitfile at offset 24. Expected value 0x5599.\n");
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Dump of the first 32 16-bit words after bitfile header:\n%s", hexDump);
            }
            return true;
        case FL_ERROR_FPGA_TYPES_DO_NOT_MATCH_FOR_UPGRADE:
            {
                FL_FpgaType existingFpgaType = (FL_FpgaType)va_arg(arguments, int);
                fileName = va_arg(arguments, const char *);
                FL_FpgaType newFpgaType = (FL_FpgaType)va_arg(arguments, int);
                prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: Bitfile UserID(0x%x) does not match currently loaded fpga(0x%x)\n",
                    newFpgaType, existingFpgaType);
            }
            return true;
        case FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE:
            if (strcmp(functionName, "FL_FileReadBitFileHeader") == 0)
            {
                vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: No such bit-file\n", arguments);
            }
            else if (strcmp(functionName, "FL_FlashWriteImage") == 0 ||
                     strcmp(functionName, "FL_FlashVerifyImage") == 0 ||
                     strcmp(functionName, "FL_CheckFlashFIleType") == 0)
            {
                vprn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "No such file: %s\n", arguments);
            }
            else
            {
                return false;
            }
            return true;
        case FL_ERROR_FLASH_VERIFY_WORD_DATA_AGAINST_FILE_FAILED:
            {
                fileName = va_arg(arguments, const char *);
                uint32_t readFlashAddress = va_arg(arguments, uint32_t);
                uint16_t readFlashWord = va_arg(arguments, uint32_t);
                uint16_t readFileWord = va_arg(arguments, uint32_t);
                prn(LOG_SCR | LOG_FILE | LEVEL_WARNING,
                    "\nError at address 0x%06X. Flash Data: 0x%04X. File Data: 0x%04X\n",
                    readFlashAddress, readFlashWord, readFileWord);
            }
            return true;
        case FL_ERROR_FLASH_VERIFY_WORD_DATA_AGAINST_FILE_TOTAL_ERRORS:
            {
                fileName = va_arg(arguments, const char *);
                uint32_t readFlashAddress = va_arg(arguments, uint32_t);
                uint16_t readFlashWord = va_arg(arguments, uint32_t);
                uint16_t readFileWord = va_arg(arguments, uint32_t);
                uint32_t totalReadErrorsSeen = va_arg(arguments, uint32_t);
                prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG,
                    "\n *** %d read errors seen before success at address 0x%06X. Flash Data: 0x%04X. File Data: 0x%04X\n",
                    totalReadErrorsSeen, readFlashAddress, readFlashWord, readFileWord);
            }
            return true;
        case FL_ERROR_MICRON_SF2_IS_NOT_READY:
            {
                if (strcmp(functionName, "FL_SF2ReadFlash") == 0)
                {
                    prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Timeout waiting for flash to be ready 1\n");
                }
                else
                {
                    uint16_t sf2Status = va_arg(arguments, uint32_t);
                    const char * errorMessage = va_arg(arguments, const char *);
                    if (errorMessage[0] == '\0')
                    {
                        prn(LOG_SCR | LEVEL_TRACE, "ActelFlashIsReady cmd: %04X\n", sf2Status);
                    }
                    else
                    {
                        prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ActelFlashIsReady cmd: %04X (%s)\n", sf2Status, errorMessage);
                    }
                }
            }
            return true;

        /* Not handled by legacy logging */
        case FL_SUCCESS:
        case FL_ERROR_FAILED_TO_CONVERT_BUILD_TIME_TO_UTC:
        case FL_ERROR_IS_DRIVER_LOADED:
        case FL_ERROR_MUNMAP_FAILED:
        case FL_ERROR_DEVICE_CLOSE_FAILED:
        case FL_ERROR_PCIE_END_POINT_NOT_UP_AND_READY:
        case FL_ERROR_INVALID_LOG_LEVEL:
        case FL_ERROR_INVALID_PCB_VERSON_NUMBER:
        case FL_ERROR_FLASH_WRITE_ADDRESS_IS_OUT_OF_RANGE:
        case FL_ERROR_UNKNOWN_FLASH_COMMAND_SET:
        case FL_ERROR_TIMEOUT_WAITING_FOR_FLASH_READY:
        case FL_ERROR_INVALID_PCIE_BOOT_MODE:
        case FL_ERROR_NOT_IMPLEMENTED:
        case FL_ERROR_SWITCH_TO_PRIMARY_IMAGE_NOT_SUPPORTED:
        case FL_ERROR_SWITCH_TO_SECONDARY_IMAGE_NOT_SUPPORTED:
        case FL_ERROR_INVALID_SPANSION_FLASH_DEVICE:
        case FL_ERROR_INVALID_MICRON_FLASH_DEVICE:
        case FL_ERROR_UNKNOWN_FLASH_DEVICE:
        case FL_ERROR_FSTAT_FAILED:
        case FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE:
        case FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF:
        case FL_ERROR_NO_ADMIN_CONTROLLER_PRESENT:
        case FL_ERROR_UNKNOWN_PRIMARY_FLASH_IMAGE:
        case FL_ERROR_UNKNOWN_SECONDARY_FLASH_IMAGE:
        case FL_ERROR_HANDLER_THROW_EXCEPTION:
        case FL_ERROR_HANDLER_CONTINUE_NO_DEFAULT_MESSAGE:
        case FL_ERROR_HANDLER_CONTINUE_WITH_DEFAULT_MESSAGE:
        case FL_ERROR_INVALID_ADMIN_TYPE:
        case FL_ERROR_INVALID_MICRON_NOR_FLASH_SIZE:
        case FL_ERROR_INVALID_MICRON_NOR_FLASH_MEMORY_TYPE:
        case FL_ERROR_IMAGE_FILE_READ_ERROR:
        case FL_ERROR_GECKO_TIMEOUT:
        case FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY:
        case FL_ERROR_INVALID_ARGUMENT:
        case FL_ERROR_FAILED_TO_WRITE_TO_FLASH_IMAGE_FILE:
        case FL_ERROR_INVALID_FLASH_MANUFACTURER_ID:
        case FL_ERROR_TIMEOUT_WAITING_FOR_MICRON_NOR_FLASH_READY:
        case FL_ERROR_FAILED_TO_READ_FROM_FLASH_IMAGE_FILE:
        case FL_ERROR_FAILED_TO_TELL_POSITION_IN_IMAGE_FILE:
        case FL_ERROR_FAILED_TO_SEEK_IN_OF_IMAGE_FILE:
        case FL_ERROR_FAILED_TO_GET_SYSTEM_TIME:
        case FL_ERROR_MESSAGE_ONLY_MASK:
        case FL_ERROR_INVALID_COMMAND_LINE_ARGUMENT:
        case FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_PART_NUMBER:
            break; /* Not handled, fall through */

        /* DO NOT ADD A DEFAULT CASE TO THIS SWITCH STATEMENT! */
    }

    return false;
}

/**
 *  Flash Library calls this function to make more Silicom Denmark internal application specific initializations.
 *
 *  @param  pFlashLibraryContext    Pointer to the Flash Library context.
 */
void SilicomInternalInitialize(FL_FlashLibraryContext * pFlashLibraryContext)
{
    pLogLevel = pFlashLibraryContext->LogContext.pLogLevel;
    *pLogLevel = LEVEL_INFO; // Default logging level
    pScreen = stdout;

    pFlashLibraryContext->LogContext.pLegacyLoggerV = vprn;
    pFlashLibraryContext->LogContext.pLegacyLogger = prn;
    pFlashLibraryContext->LogContext.pLegacyLogMessage = LegacyLogMessage;

    pFlashLibraryContext->ErrorContext.pLegacyErrorLoggerV = vprn;
    pFlashLibraryContext->ErrorContext.pLegacyErrorLogger = prn;
    pFlashLibraryContext->ErrorContext.pLegacyErrorMessage = LegacyErrorMessage;
}
