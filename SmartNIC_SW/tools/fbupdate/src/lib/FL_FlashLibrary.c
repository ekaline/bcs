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

/*

Source and information links:

admin_commands.h: file://net/bohr/var/fiberblaze/software/Savona/SW-design/admin_commands.h
FPGA and Gecko register interface: file://net/bohr/var/fiberblaze/software/Savona/SW-design/fpga_hk_reg_interface_20180911.odt
Specification of requirements for Savona TestFPGA RTL + SW: https://docs.google.com/document/d/1VVSH-6I29MXZrzdzIloi68BSjhrLPgn3Q33XVI_yvSk/edit#heading=h.o0foycbh057s
Micron flash data sheet: https://www.micron.com/~/media/documents/products/data-sheet/nor-flash/serial-nor/mt25q/die-rev-b/mt25q_qlkt_u_512_abb_0.pdf
fbupdate Savona hack: file://net/bohr/var/fiberblaze/filedump/mm/savona_testfpga/src/qspi.c
Savona Gecko software repository: file://net/bohr/srv/hg/Fiberblaze/sw/savona/


Atmel is a microcontroller
Actel is a FPGA type
SF2 is SmartFusion2
Gecko is Giant Gecko
*/

#pragma region Include files

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xmmintrin.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "FL_FlashLibrary.h"
#include "fpgaregs.h"
#include "actel.h"
#include "actel_common.h"
#include "sf2commands.h"

#include "gecko.h"
#include "qspi.c" // TODO remove!!!

#pragma endregion Include files

#ifndef nameof
    #define nameof(x) #x
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#pragma region Logging and Error Handling

/***************************************************************************/

#define ContinueOnSuccess(x)        = (x), errorCode != FL_SUCCESS ? errorCode : errorCode = errorCode
#define NameOf(x)                   #x

/***************************************************************************/

#define PCB_INFO        0x0060  /**< The lower byte is PCB version, upper byte is FPGA type. */
#define PCI_BOOT_MODE   0x0070  /**< Only the 2 lower bits are defined. */

/***************************************************************************/

FL_Error FL_SetLastErrorCode(FL_ErrorContext * pErrorContext, FL_Error errorCode)
{
    pErrorContext->LastErrorCode = errorCode;
    return errorCode;
}

/***************************************************************************/

FL_Error FL_GetLastErrorCode(const FL_ErrorContext * pErrorContext)
{
    return pErrorContext->LastErrorCode;
}

/***************************************************************************/

FL_LogLevel FL_GetBasicLogLevel(FL_LogLevel logLevel)
{
    return logLevel & FL_LOG_LEVEL_MASK;
}

/***************************************************************************/

FL_Boolean FL_LogErrorDetails(const FL_LogContext * pLogContext)
{
    return (*pLogContext->pLogLevel & FL_LOG_LEVEL_ERROR_DETAILS) != 0;
}

/***************************************************************************/

FL_Boolean FL_LogErrorCallStack(const FL_LogContext * pLogContext)
{
    return (*pLogContext->pLogLevel & FL_LOG_LEVEL_CALL_STACK) != 0;
}

/***************************************************************************/

FL_Error FL_ErrorDetails(FL_ErrorContext * pErrorContext, const char * fileName, int lineNumber, const char * functionName, FL_Error errorCode, const char * format, ...)
{
    if (pErrorContext != NULL)
    {
        FL_Error handlerErrorCode = FL_SUCCESS;

        if (pErrorContext->pUserErrorHandlerV != NULL)
        {
            va_list arguments;
            va_start(arguments, format);

            handlerErrorCode = pErrorContext->pUserErrorHandlerV(pErrorContext->pUserContext, fileName, lineNumber, functionName, errorCode, format, arguments);

            va_end(arguments);

            if (handlerErrorCode == FL_ERROR_HANDLER_CONTINUE_NO_DEFAULT_MESSAGE ||
                handlerErrorCode == FL_ERROR_HANDLER_CONTINUE_WITH_DEFAULT_MESSAGE)
            {
                /* Continue processing, failed API function will return the original error code. */
            }
            else if (handlerErrorCode == FL_ERROR_HANDLER_THROW_EXCEPTION)
            {
                /* C++ EXCEPTIONS NOT CURRENTLY SUPPORTED! */
                fprintf(stderr, "\n *** ERROR: Silicom flash programming library does not currently support C++ exceptions!\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                /* Continue processing, failed API function will return the error code that error handler returned. */
                errorCode = handlerErrorCode;
            }
        }

        if (handlerErrorCode != FL_ERROR_HANDLER_CONTINUE_NO_DEFAULT_MESSAGE)
        {
            bool handledAsLegacyErrorMessage = false;

            {
                va_list arguments;
                va_start(arguments, format);

                handledAsLegacyErrorMessage = pErrorContext->pLegacyErrorMessage(fileName, lineNumber, functionName, errorCode, format, arguments);

                va_end(arguments);
            }

            if (pErrorContext->pLegacyErrorMessage == NULL || !handledAsLegacyErrorMessage)
            {
                /* Legacy handler not present or did not handle the error message. */

                if (pErrorContext->pLegacyErrorLoggerV != NULL)
                {
                    va_list arguments;
                    va_start(arguments, format);

                    char legacyFormat[20 + strlen(functionName) + strlen(format)];
                    legacyFormat[0] = '\0';
                    if (FL_LogErrorDetails(pErrorContext->pLogContext))
                    {
                        strcat(legacyFormat, "In function ");
                        strcat(legacyFormat, functionName);
                        strcat(legacyFormat, " error code ");
                        sprintf(&legacyFormat[strlen(legacyFormat)], "%d", errorCode);
                        strcat(legacyFormat, ": ");
                    }
                    strcat(legacyFormat, format);
                    if (format[strlen(format) - 1] != '\n')
                    {
                        strcat(legacyFormat, "\n");
                    }
                    pErrorContext->pLegacyErrorLoggerV(FL_LOG_LEVEL_ERROR, legacyFormat, arguments);

                    va_end(arguments);
                }
                else
                {
                    char errorMessage[1000];
                    va_list arguments;
                    va_start(arguments, format);

                    int errorMessageLength = vsnprintf(errorMessage, sizeof(errorMessage) - 1, format, arguments);

                    va_end(arguments);

                    if (errorMessage[errorMessageLength - 1] != '\n')
                    {
                        strcat(errorMessage, "\n");
                    }
                    fprintf(stderr, "*** In function %s error code %d: %s\n", functionName, errorCode, errorMessage);
                    fflush(stderr);
                }
            }
        }
    }

    /* Convert some specific error codes to a more generic error code */
    switch (errorCode)
    {
        case FL_ERROR_SPANSION_FLASH_IS_NOT_READY:
        case FL_ERROR_MICRON_FLASH_IS_NOT_READY:
        case FL_ERROR_MICRON_SF2_IS_NOT_READY: // TODO: is this correct???
        case FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY:

            /* Convert flash specific error code to more generic. */
            errorCode = FL_ERROR_FLASH_IS_NOT_READY;
            break;
        default:
            break;
    }

    return FL_SetLastErrorCode(pErrorContext, errorCode);
}

/***************************************************************************/

void FL_LogDetails(const FL_LogContext * pLogContext, const char * fileName, int lineNumber, const char * functionName, FL_LogLevel logLevel, const char * format, ...)
{
    fileName = fileName;
    lineNumber = lineNumber;
    functionName = functionName;

    va_list arguments;
    va_start(arguments, format);

    if (pLogContext->pLegacyLogMessage != NULL)
    {
        if (pLogContext->pLegacyLogMessage(fileName, lineNumber, functionName, logLevel, format, arguments))
        {
            /* Legacy log message was output, don't log anything more */
            return;
        }
    }

    if (pLogContext->pLegacyLoggerV != NULL)
    {
        pLogContext->pLegacyLoggerV(logLevel, format, arguments);
    }
    else if (logLevel <= FL_GetBasicLogLevel(*pLogContext->pLogLevel))
    {
        FILE * pFile = stdout;
        if ((logLevel & FL_LOG_LEVEL_WARNING) != 0 &&
            (logLevel & FL_LOG_LEVEL_ERROR) != 0 &&
            (logLevel & FL_LOG_LEVEL_FATAL) != 0)
        {
            pFile = stderr;
        }
        vfprintf(pFile, format, arguments);
        if (format[strlen(format) - 1] != '\n' && format[strlen(format) - 1] != '\r' && format[0] != '\r')
        {
            fprintf(pFile, "\n");
        }
        fflush(pFile);
    }

    va_end(arguments);
}

/***************************************************************************/

FL_Error FL_RegisterErrorHandler(FL_FlashLibraryContext * pFlashLibraryContext, FL_pUserErrorHandler pUserErrorHandlerV, void * pUserContext)
{
    pFlashLibraryContext->ErrorContext.pUserErrorHandlerV = pUserErrorHandlerV;
    pFlashLibraryContext->ErrorContext.pUserContext = pUserContext;

    return FL_SetLastErrorCode(&pFlashLibraryContext->ErrorContext, FL_SUCCESS);
}

#pragma endregion

#pragma region Initialization

/***************************************************************************/

void FL_InitializeFlashLibraryContext(FL_FlashLibraryContext * pFlashLibraryContext, FL_InternalInitializer pInternalInitializer)
{
    FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;

    /* Zero whole context */
    memset(pFlashLibraryContext, 0, sizeof(*pFlashLibraryContext));

    /* Initialize members that have default values different from zero: */
    pFlashLibraryContext->DeviceFileDescriptor = -1;

    pFlashLibraryContext->ErrorContext.pLogContext = &pFlashLibraryContext->LogContext;

    /* Initialize cross references between structures: */
    pFlashInfo->pLogContext = &pFlashLibraryContext->LogContext;
    pFlashInfo->pErrorContext = &pFlashLibraryContext->ErrorContext;
    pFlashInfo->pFpgaInfo = pFpgaInfo;

    pFpgaInfo->pLogContext = &pFlashLibraryContext->LogContext;
    pFpgaInfo->pErrorContext = &pFlashLibraryContext->ErrorContext;

    pParameters->RequiredInitializationLevel = FL_INITIALIZATION_LEVEL_MAPPING;
    pParameters->LogLevel = FL_LOG_LEVEL_INFO;
    pParameters->pLogContext = &pFlashLibraryContext->LogContext;
    pParameters->pErrorContext = &pFlashLibraryContext->ErrorContext;
    pParameters->ExpectedBitFileFpgaType = -1;

    pFlashLibraryContext->LogContext.pLogLevel = &pParameters->LogLevel;

    if (pInternalInitializer != NULL)
    {
        /* Initialize Silicom internal applications like testfpga etc. */
        pInternalInitializer(pFlashLibraryContext);
    }
}

/***************************************************************************/

FL_Error FL_OpenDevice(FL_FlashLibraryContext * pFlashLibraryContext)
{
    if (pFlashLibraryContext->DeviceFileDescriptor < 0)
    {
        FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
        FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;

        if (pFlashInfo->pRegistersBaseAddress != NULL)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ARGUMENT, "pFlashInfo->pRegistersBaseAddress must be NULL here!");
        }

        /* Open user specified device else try to open a device from the list */
        if (strlen(pParameters->DeviceName) > 0)
        {
            pFlashLibraryContext->DeviceFileDescriptor = open(pParameters->DeviceName, O_RDWR, 0);
            if (pFlashLibraryContext->DeviceFileDescriptor < 0)
            {
                /* Prepend /dev/ if it is not present */
                if (memcmp(pParameters->DeviceName, "/dev/", 5) != 0)
                {
                    /* Move device 5 places */
                    memmove(pParameters->DeviceName + 5, pParameters->DeviceName, strlen(pParameters->DeviceName));
                    /* Insert /dev/ */
                    memmove(pParameters->DeviceName, "/dev/", 5);
                }
                pFlashLibraryContext->DeviceFileDescriptor = open(pParameters->DeviceName, O_RDWR, 0);
                if (pFlashLibraryContext->DeviceFileDescriptor < 0)
                {
                    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_DEVICE,
                        "Failed to open device '%s' with errno %d. Is the driver loaded?", pParameters->DeviceName, errno);
                }
            }
        }
        else
        {
            /* No user defined device name given, try a to open one of known device names. */
            static const char s_DeviceList[][sizeof(pParameters->DeviceName)] =
            {
                "/dev/fiberblaze",
                "/dev/fbcard0",
                "/dev/smartnic",
                "/dev/packetmover"
            };
            size_t deviceIndex = 0;
            const size_t DeviceListSize = sizeof(s_DeviceList) / sizeof(s_DeviceList[0]);

            for (deviceIndex = 0; deviceIndex < DeviceListSize; ++deviceIndex)
            {
                pFlashLibraryContext->DeviceFileDescriptor = open(s_DeviceList[deviceIndex], O_RDWR, 0);
                if (pFlashLibraryContext->DeviceFileDescriptor >= 0)
                {
                    strcpy(pParameters->DeviceName, s_DeviceList[deviceIndex]);
                    break;
                }
            }

            if (pFlashLibraryContext->DeviceFileDescriptor < 0)
            {
                for (deviceIndex = 0; deviceIndex < DeviceListSize; ++deviceIndex)
                {
                    FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_ANY_DEVICE,
                        "Failed to open device '%s'", s_DeviceList[deviceIndex]);
                }
                if (!pParameters->SkipMemoryMapping)
                {
                    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IS_DRIVER_LOADED, "Is the driver loaded?");
                }
            }
        }

        pFlashInfo->pRegistersBaseAddress = NULL;
        if (!pParameters->SkipMemoryMapping)
        {
            pFlashInfo->pRegistersBaseAddress = mmap(0, FL_DEFAULT_FPGA_REGISTERS_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pFlashLibraryContext->DeviceFileDescriptor, 0);
            if (pFlashInfo->pRegistersBaseAddress == MAP_FAILED)
            {
                pFlashInfo->pRegistersBaseAddress = NULL;
                FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MMAP_FAILED, "Failed to map, errno %d ", errno);

                return FL_CloseDevice(pFlashLibraryContext);
            }
        }
    }

    return FL_SetLastErrorCode(&pFlashLibraryContext->ErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_CloseDevice(FL_FlashLibraryContext * pFlashLibraryContext)
{
    FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    FL_Error errorCode = FL_SUCCESS;

    if (pFlashInfo->pRegistersBaseAddress != NULL)
    {
        if (munmap(pFlashInfo->pRegistersBaseAddress, FL_DEFAULT_FPGA_REGISTERS_MEMORY_SIZE) != 0)
        {
            errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MUNMAP_FAILED, "munmap of address %p failed with errno %d", pFlashInfo->pRegistersBaseAddress, errno);
        }
    }

    if (pFlashLibraryContext->DeviceFileDescriptor != -1)
    {
        if (close(pFlashLibraryContext->DeviceFileDescriptor) != 0)
        {
            errorCode = errorCode == FL_SUCCESS ? FL_Error(pFlashInfo->pErrorContext, FL_ERROR_DEVICE_CLOSE_FAILED, "Failed to close device, errno %d", errno) : errorCode;
        }
    }

    pFlashInfo->pRegistersBaseAddress = NULL;
    pFlashLibraryContext->DeviceFileDescriptor = -1;

    return FL_SetLastErrorCode(&pFlashLibraryContext->ErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_OpenFlashLibrary(FL_FlashLibraryContext * pFlashLibraryContext)
{
    FL_ErrorContext * pErrorContext = &pFlashLibraryContext->ErrorContext;
    FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    FL_LogLevel logLevel = FL_GetBasicLogLevel(pParameters->LogLevel);
    if (FL_GetBasicLogLevel(logLevel) < FL_LOG_LEVEL_ALWAYS || FL_GetBasicLogLevel(logLevel) > FL_LOG_LEVEL_TRACE)
    {
        return FL_Error(pErrorContext, FL_ERROR_INVALID_LOG_LEVEL, "Invalid log level %d should be [%d-%d]", pParameters->LogLevel, FL_LOG_LEVEL_ALWAYS, FL_LOG_LEVEL_TRACE);
    }

    if (pParameters->ExternalCommandPosition == 0)
    {
        // TODO FIXME THIS IS A CLUDGE; FIND SOMETHING BETTER!!!!
        pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
    }

    if (pParameters->CurrentInitializationLevel != pParameters->RequiredInitializationLevel)
    {
        FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
        FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
        FL_PCIeInfo * pPCIeInfo = &pFlashLibraryContext->PCIeInfo;
        const FL_LogContext * pLogContext = &pFlashLibraryContext->LogContext;
        void * pRegistersBaseAddress = NULL;

        if ((pParameters->CurrentInitializationLevel & FL_INITIALIZATION_LEVEL_MAPPING) != (pParameters->RequiredInitializationLevel & FL_INITIALIZATION_LEVEL_MAPPING))
        {
            if ((pParameters->CurrentInitializationLevel & FL_INITIALIZATION_LEVEL_MAPPING) != FL_INITIALIZATION_LEVEL_MAPPING)
            {
                /* Only map FPGA registers to user space memory. */
                FL_ExitOnError(pLogContext, FL_OpenDevice(pFlashLibraryContext));

                /* pFlashInfo->pRegistersBaseAddress is available now */
                pRegistersBaseAddress = pFlashInfo->pRegistersBaseAddress;

                pParameters->CurrentInitializationLevel |= FL_INITIALIZATION_LEVEL_MAPPING;
            }
        }

        if ((pParameters->CurrentInitializationLevel & FL_INITIALIZATION_LEVEL_BASIC) != (pParameters->RequiredInitializationLevel & FL_INITIALIZATION_LEVEL_BASIC))
        {
            if ((pParameters->CurrentInitializationLevel & FL_INITIALIZATION_LEVEL_BASIC) != FL_INITIALIZATION_LEVEL_BASIC)
            {
                /* Basic initialization only reads revision from register zero.                                           */
                /* With initialization type FL_INITIALIZATION_TYPE_NO_REGISTER_ZERO_WRITE the register is not written to. */

                FL_ExitOnError(pLogContext, FL_GetFpgaRevisionInformation(pFlashInfo, pFpgaInfo));

                pFpgaInfo->Type = (pFpgaInfo->RawRevision >> 32) & 0xFF;
                pFpgaInfo->Model = (pFpgaInfo->RawRevision >> 40) & 0xFF;
                pFpgaInfo->CardType = FL_GetCardTypeFromFpgaType(pErrorContext, pFpgaInfo->Type);
                pFpgaInfo->AdminType = FL_GetAdminTypeFromFpgaType(pErrorContext, pFpgaInfo->Type);

                pFpgaInfo->PartNumber = FL_GetPartNumberFromFpgaModel(pErrorContext, pFpgaInfo->Model);
                if (pParameters->UseForceProgramming)
                {
                    pFlashLibraryContext->FpgaInfo.PartNumber = "";
                }

                pFpgaInfo->MaximumNumberOfLogicalPorts = FL_GetMaximumNumberOfLogicalPorts(pErrorContext, pFpgaInfo->Type);
                pFpgaInfo->NumberOfPhysicalPorts = FL_GetNumberOfPhysicalPorts(pErrorContext, pFpgaInfo->Type);

                pFlashInfo->Type = FL_GetFlashTypeFromFpgaType(pErrorContext, pFpgaInfo->Type);

                FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "FPGA type %s (0x%02X) Model %s (0x%02X)\n",
                    FL_GetFpgaTypeName(pErrorContext, pFpgaInfo->Type, false), pFpgaInfo->Type,
                    FL_GetFpgaModelName(pErrorContext, pFpgaInfo->Model, false), pFpgaInfo->Model);

                pParameters->CurrentInitializationLevel |= FL_INITIALIZATION_LEVEL_BASIC;
            }
        }

        if ((pParameters->CurrentInitializationLevel & FL_INITIALIZATION_LEVEL_FULL) != (pParameters->RequiredInitializationLevel & FL_INITIALIZATION_LEVEL_FULL))
        {
            if ((pParameters->CurrentInitializationLevel & FL_INITIALIZATION_LEVEL_FULL) != FL_INITIALIZATION_LEVEL_FULL)
            {
                /* Full initialization. */
                FL_ExitOnError(pLogContext, FL_GetFpgaBuildTimeInformation(pFlashInfo, pFpgaInfo));

                FL_ExitOnError(pLogContext, FL_GetFlashInformation(pFlashLibraryContext));

                /* Get Mercurial version control system revision */
                pFpgaInfo->RawMercurialRevision = FL_GetMercurialRevision(pRegistersBaseAddress);
                pFpgaInfo->MercurialRevisionIsDirty = (pFpgaInfo->RawMercurialRevision & (1ULL << 63)) != 0;
                pFpgaInfo->MercurialRevision = pFpgaInfo->RawMercurialRevision & ~(1ULL << 63);

                FL_ExitOnError(pLogContext, FL_GetPCIeInformation(pFlashInfo, pFpgaInfo, pParameters, pPCIeInfo));

                pParameters->CurrentInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
            }
        }

        FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "Flash library successfully opened on level 0x%X.\n", pParameters->CurrentInitializationLevel);
    }

    return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_CloseFlashLibrary(FL_FlashLibraryContext * pFlashLibraryContext)
{
    return FL_CloseDevice(pFlashLibraryContext);
}

/***************************************************************************/

FL_Error FL_AddCommands(FL_FlashLibraryContext * pFlashLibraryContext, const FL_CommandEntry newCommands[])
{
    FL_ErrorContext * pErrorContext = &pFlashLibraryContext->ErrorContext;
    size_t numberOfNewCommands = 0;
    size_t numberOfExistingCommands = 0;
    const FL_CommandEntry * pCommandEntry = NULL;
    FL_CommandEntry * pNewCommandTable;
    FL_CommandEntry * pNewCommands;

    /* Count number of new commands: */
    if (newCommands != NULL)
    {
        pCommandEntry = newCommands;
        while (pCommandEntry->CommandName != NULL)
        {
            if (pCommandEntry->pCommandFunction == NULL)
            {
                return FL_Error(pErrorContext, FL_ERROR_INVALID_ARGUMENT,
                    "Command '%s' is missing a command execute function", pCommandEntry->CommandName);
            }
            if (pCommandEntry->HelpText == NULL)
            {
                return FL_Error(pErrorContext, FL_ERROR_INVALID_ARGUMENT,
                    "Command '%s' is missing a help text", pCommandEntry->CommandName);
            }
            if (pCommandEntry->HelpText[strlen(pCommandEntry->HelpText) - 1] != '\n')
            {
                return FL_Error(pErrorContext, FL_ERROR_INVALID_ARGUMENT,
                    "Command '%s' help text must finish with a new line '\n'", pCommandEntry->CommandName);
            }
            ++numberOfNewCommands;
            ++pCommandEntry;
        }
    }

    if (numberOfNewCommands == 0)
    {
        return FL_Error(pErrorContext, FL_ERROR_INVALID_ARGUMENT, "Must add at least one command entry, none found!");
    }

    /* Count number of existing commands: */
    if (pFlashLibraryContext->Commands != NULL)
    {
        pCommandEntry = pFlashLibraryContext->Commands;
        while (pCommandEntry->CommandName != NULL)
        {
            ++numberOfExistingCommands;
            ++pCommandEntry;
        }
    }

    /* Allocate memory for new combined command table including the last terminating empty entry*/
    pNewCommandTable = pNewCommands = malloc((numberOfExistingCommands + numberOfNewCommands + 1) * sizeof(*pCommandEntry));

    if (pFlashLibraryContext->Commands != NULL)
    {
        /* Copy existing commands to new table: */
        pCommandEntry = pFlashLibraryContext->Commands;
        while (pCommandEntry->CommandName != NULL)
        {
            *pNewCommands++ = *pCommandEntry++;
        }
        *pNewCommands = *pCommandEntry; /* Copy terminating entry */
    }

    if (newCommands != NULL)
    {
        /* Copy new commands to new table: */
        pCommandEntry = newCommands;
        while (pCommandEntry->CommandName != NULL)
        {
            bool commandReplaced = false;

            if (pFlashLibraryContext->Commands != NULL)
            {
                const FL_CommandEntry * pSearch = pFlashLibraryContext->Commands;
                while (pSearch->CommandName != NULL)
                {
                    if (strcmp(pSearch->CommandName, pCommandEntry->CommandName) == 0)
                    {
                        /* Existing command to be replaced by a new one */
                        *pNewCommands = *pCommandEntry++;
                        commandReplaced = true;
                        break; /* Only first found instance is replaced. Later duplicate command(s) may exist and are ignored. */
                    }
                    ++pSearch;
                }
            }

            if (!commandReplaced)
            {
                *pNewCommands++ = *pCommandEntry++;
            }
        }
    }

    *pNewCommands = *pCommandEntry; /* Copy terminating entry */

    if (pFlashLibraryContext->Commands != NULL)
    {
        free((FL_CommandEntry *)pFlashLibraryContext->Commands);
    }
    pFlashLibraryContext->Commands = pNewCommandTable;

    return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_SetLibraryInitializationForCommand(FL_FlashLibraryContext * pFlashLibraryContext, const char * command)
{
    FL_ErrorContext * pErrorContext = &pFlashLibraryContext->ErrorContext;
    FL_Error errorCode = FL_SUCCESS;

    if (command != NULL)
    {
        const FL_CommandEntry * pCommandEntry = pFlashLibraryContext->Commands;
        while (pCommandEntry->CommandName != NULL)
        {
            if (strcmp(pCommandEntry->CommandName, command) == 0)
            {
                FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;

                pParameters->RequiredInitializationLevel |= pCommandEntry->RequiredInitializationLevel;
                return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
            }
            ++pCommandEntry;
        }
        errorCode = FL_Error(pErrorContext, FL_ERROR_INVALID_ARGUMENT, "Unknown -c command '%s'", command);
    }
    
    return FL_SetLastErrorCode(pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_CommandsHelp(FL_FlashLibraryContext * pFlashLibraryContext)
{
    FL_LogContext * pLogContext = &pFlashLibraryContext->LogContext;
    FL_ErrorContext * pErrorContext = &pFlashLibraryContext->ErrorContext;
    const FL_CommandEntry * pCommandEntry = pFlashLibraryContext->Commands;
    size_t lengthOfLongestCommand = 0;

    if (pCommandEntry == NULL)
    {
        return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
    }

    while (pCommandEntry->CommandName != NULL)
    {
        size_t commandLength = strlen(pCommandEntry->CommandName);
        if (commandLength > lengthOfLongestCommand)
        {
            lengthOfLongestCommand = commandLength;
        }
        ++pCommandEntry;
    }

    FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\"-c\" command help:\n\n");

    pCommandEntry = pFlashLibraryContext->Commands;
    while (pCommandEntry->CommandName != NULL)
    {
        const char * pHelp = pCommandEntry->HelpText;
        const char * pNextHelp;
        bool commandIncluded = false;
        char helpLine[200];
        helpLine[sizeof(helpLine) - 1] = '\0';
        const size_t spaceAfterCommand = 4;

        do
        {
            memset(helpLine, ' ', sizeof(helpLine) - 1);

            if (!commandIncluded)
            {
                strcpy(helpLine, pCommandEntry->CommandName);
                strcat(helpLine, ": ");
                helpLine[strlen(helpLine)] = ' ';
                commandIncluded = true;
            }
            pNextHelp = strchr(pHelp, '\n');
            if (pNextHelp != NULL)
            {
                strncpy(&helpLine[lengthOfLongestCommand + spaceAfterCommand], pHelp, pNextHelp - pHelp);
                helpLine[lengthOfLongestCommand + spaceAfterCommand + pNextHelp - pHelp] = '\0';

                FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "%s\n", helpLine);

                pHelp = pNextHelp + 1;
            }

        } while (pNextHelp != NULL);

        ++pCommandEntry;
    }

    return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_ExecuteCommand(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char * argv[])
{
    if (argc > 0 && argv[0] != NULL)
    {
        const FL_CommandEntry * pCommandEntry = pFlashLibraryContext->Commands;
        while (pCommandEntry->CommandName != NULL)
        {
            if (strcmp(pCommandEntry->CommandName, argv[0]) == 0)
            {
                const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
                if (pParameters->CurrentInitializationLevel != pParameters->RequiredInitializationLevel)
                {
                    const FL_LogContext * pLogContext = &pFlashLibraryContext->LogContext;
                    FL_ExitOnError(pLogContext, FL_OpenFlashLibrary(pFlashLibraryContext));
                }
                return pCommandEntry->pCommandFunction(pFlashLibraryContext, argc - 1, argv + 1);
            }
            ++pCommandEntry;
        }

        return FL_Error(&pFlashLibraryContext->ErrorContext, FL_ERROR_INVALID_ARGUMENT & FL_ERROR_MESSAGE_ONLY_MASK,
            "No such command: %s\n", argv[0]);
    }

    return FL_CommandsHelp(pFlashLibraryContext);
}

#pragma endregion

#pragma region Admin

/***************************************************************************/

// SF2InitFlash
FL_Error FL_AdminGetConfigurationInformation(FL_FlashInfo * pFlashInfo)
{
    uint32_t * pRegistersBaseAddress = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_ErrorContext * pErrorContext = pFlashInfo->pErrorContext;
    uint16_t sf2SpiFlashType;
    FL_ImageTarget imageTarget;
    uint16_t adminStatus;
    uint32_t softwareRevision;
    uint16_t flashWordValue;

    FL_ExitOnError(pLogContext, FL_AdminIsControllerPresent(pFlashInfo, pFlashInfo->Type));

    //TODO: select here between FL_FLASH_COMMAND_SET_SF2 and FL_FLASH_COMMAND_SET_GIANT_GECKO
    pFlashInfo->FlashCommandSet = FL_FLASH_COMMAND_SET_SF2;

    /* Set Actel Controller address size */
    switch (FL_AdminGetControllerVersion(pRegistersBaseAddress))
    {
        case 0x01:
            // prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "BootFPGA is using 28 address lines.\n");
            FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "BootFPGA is using 28 address lines.\n");
            pFlashInfo->AddressSize = 28;
            break;
        default:
            //prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Invalid Actel controller version.\n");
            return FL_Error(pErrorContext, FL_ERROR_INVALID_ADMIN_CONTROLLER_VERSION, "Invalid SF2 controller version.");
    }

    /* Test that we have access to the SF2 software */
    FL_ExitOnError(pLogContext, FL_AdminGetSoftwareRevision(pFlashInfo, &softwareRevision));
    if (softwareRevision == 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Invalid SF2 SW version.\n");
        return FL_Error(pErrorContext, FL_ERROR_INVALID_ADMIN_SOFTWARE_VERSION, "Invalid SF2 software version.");
    }

    FL_ExitOnError(pLogContext, FL_AdminReset(pFlashInfo)); /* It could be in some unknown state */
    while (FL_FlashIsReady(pFlashInfo, 0L) != FL_SUCCESS)
    {
        usleep(100);
    }

    /* Get SPI flash type and size */
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, 0, READ_IDENTIFICATION));
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &sf2SpiFlashType));

    /* MICRON and ADESTO(mango-3) are the SPI flashes supported by SF2. */
    if (((sf2SpiFlashType != (FL_SF2_SPI_FLASH_MANUFACTURER_ID_MICRON | FL_SF2_SPI_FLASH_DEVICE_ID1_MICRON << 8)) &&
        (sf2SpiFlashType != (FL_SF2_SPI_FLASH_MANUFACTURER_ID_MICRON | FL_SF2_SPI_FLASH_DEVICE_ID2_MICRON << 8)))
        && (sf2SpiFlashType != (FL_SF2_SPI_FLASH_MANUFACTURER_ID_ADESTO | FL_SF2_SPI_FLASH_DEVICE_ID1_ADESTO << 8)))
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Unsupported SF2 SPI Flash type. flashData:%x \n", flashData);
        return FL_Error(pErrorContext, FL_ERROR_UNSUPPORTED_ADMIN_SPI_FLASH_TYPE,
            "Unsupported SF2 SPI Flash type: 0x%X", sf2SpiFlashType);
    }

    /* Read size as the first thing in order to enable range checking */
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, 1, READ_IDENTIFICATION));
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &flashWordValue));
    pFlashInfo->FlashSize = (uint32_t)flashWordValue << 16;

    if ((pFlashInfo->FlashSize > (1 << 24)) || (pFlashInfo->FlashSize < (1 << 20)))
    {
        uint32_t i, idLength;

        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unlikely SF2 SPI Flash Size 0x%X bytes\n", pFlashInfo->FlashSize);
        FL_Error errorCode = FL_Error(pErrorContext, FL_ERROR_UNLIKELY_ADMIN_SPI_FLASH_SIZE,
            "Unlikely SF2 SPI Flash Size 0x%X bytes\n", pFlashInfo->FlashSize);

        /* Dump CFI data to standard output */
        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, 3, READ_IDENTIFICATION));
        while (FL_FlashIsReady(pFlashInfo, 0) != FL_SUCCESS)
        {
            usleep(100);
        }
        idLength = 2;
        for (i = 0; i < idLength; ++i)
        {
            FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, i, &flashWordValue));
            printf("Flash CFI #1 0x%02X %04X\n", i * 2, flashWordValue);
            if (i == 1)
            {
                idLength += ((flashWordValue >> 8) + 1) / 2;
            }
        }
        return errorCode;
    }
    //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "SF2 SPI Flash Size 0x%X bytes\n", pFlashInfo->FlashSize);
    FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "SF2 SPI Flash Size 0x%X bytes\n", pFlashInfo->FlashSize);

    /* These timings are from the M25PE80 Datasheet */
    pFlashInfo->TypicalWordProgramTime = 11000;
    pFlashInfo->TypicalBufferProgramTime = 11000;
    pFlashInfo->TypicalSectorEraseTime = 10000;
    pFlashInfo->MaxWordProgramTime = 25000;
    pFlashInfo->MaxBufferProgramTime = 25000;
    pFlashInfo->MaxSectorEraseTime = 20000;

    /* Read size of write buffer */
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, 2, READ_IDENTIFICATION));
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &flashWordValue));
    pFlashInfo->BufferCount = flashWordValue / 2;

    /* Micron SPI Flash sectors are all of same size = write buffer size */
    pFlashInfo->LowSectorSize =
        pFlashInfo->MiddleSectorSize =
        pFlashInfo->TopSectorSize = pFlashInfo->BufferCount * 2;
    pFlashInfo->SectorCount = pFlashInfo->FlashSize / pFlashInfo->LowSectorSize;

    pFlashInfo->LowSectorCount = 0x00004000 * 2 / pFlashInfo->LowSectorSize;
    pFlashInfo->MiddleSectorCount = pFlashInfo->SectorCount - pFlashInfo->LowSectorCount - 1;
    pFlashInfo->TopSectorCount = 1;

    pFlashInfo->BankCount = 1;

    /* Compute derived data */
    pFlashInfo->BankSize = pFlashInfo->FlashSize / pFlashInfo->BankCount;

    FL_ExitOnError(pLogContext, FL_AdminReadStatus(pFlashInfo, &adminStatus));

    /* Set image start addresses */
    switch ((adminStatus & ACTEL_STATUS_ADDRESS_MAP) >> 8)
    {
        default:
            pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0x00004000;
            pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY] = 0x00080000;
            pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0x000E0000;
            pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] =
                (pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY] -
                    pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY]) / (pFlashInfo->MiddleSectorSize / 2);
            pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] =
                (pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] -
                    pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY]) / (pFlashInfo->MiddleSectorSize / 2);
            pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] =
                (pFlashInfo->FlashSize / 2 - pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION]) /
                (pFlashInfo->MiddleSectorSize / 2) - pFlashInfo->TopSectorCount;
            break;

            /*
            default:
                prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Unsupported flash image start address. %s:%d\n", __FILE__,__LINE__);
                break;
            */
    }
    for (imageTarget = 0; imageTarget < FL_IMAGE_TARGET_NUMBER_OF_TARGETS; ++imageTarget)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "ImageStartAddr[%u]:0x%X\n", i, pFlashInfo->ImageStartAddress[i]);
        FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "ImageStartAddr[%u]:0x%X\n", imageTarget, pFlashInfo->ImageStartAddress[imageTarget]);
        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "SectorsPerImage[%u]:%u\n", i, pFlashInfo->SectorsPerImage[i]);
        FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "SectorsPerImage[%u]:%u\n", imageTarget, pFlashInfo->SectorsPerImage[imageTarget]);
    }

#if 0 // TODO Consider removing this code by Mathis comment!
#define SF2DIRECTORY_SIZE 0x100
    static uint16_t SF2FlashDir[SF2DIRECTORY_SIZE * 16];
    uint32_t flashAddress = 0;
    //Read current SF2 Flash directory
    memset(SF2FlashDir, 0xffff, sizeof(SF2FlashDir));
    int state = SF2ReadFlash(pFlashInfo, flashAddress); // Fill read buffer in SF2
    if (state != 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Error reading current SF2 Flash directory\n");
        return FL_Error(pErrorContext, FK_ERROR_CANNOT_READ_SF2_FLASH_DIRECTORY,
            "Error reading current SF2 Flash directory, state 0x%X", state);
    }

    for (; flashAddress < pFlashInfo->BufferCount; ++flashAddress)
    {
        FL_ExitOnError(pLogContext, FL_ReadAdminWord(pFlashInfo, flashAddress, &SF2FlashDir[flashAddress]));
    }

    //Make sure the first directory entry is initialized to point to the Fail-Safe image
    Psf2Directory pDir = (Psf2Directory)SF2FlashDir;
    if (pDir->Image[0].StartAddr != 0x00004000)
    {
        pDir->Image[0].StartAddr = 0x00004000;
        if (FL_WriteAdminCommand(pFlashInfo, SF2FlashDir, 0) != FL_SUCCESS)
        {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Error writing SF2 Flash directory\n");
            return FL_Error(pErrorContext, FK_ERROR_CANNOT_READ_SF2_FLASH_DIRECTORY,
                "",
        }
    }
#endif
    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_AdminType FL_GetAdminTypeFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    /* Find out which type of microcontroller card has if any: */
    switch (fpgaType)
    {
        case FPGA_TYPE_ANCONA_AUGSBURG:
        case FPGA_TYPE_ANCONA_CHEMNITZ:
        case FPGA_TYPE_ANCONA_CLOCK_CALIBRATION:
        case FPGA_TYPE_ANCONA_TEST:
        case FPGA_TYPE_LUXG_PROBE:
        case FPGA_TYPE_LUXG_TEST:

        case FPGA_TYPE_ATHEN_TEST:

        case FPGA_TYPE_ERCOLANO_DEMO:
        case FPGA_TYPE_ERCOLANO_TEST:

        case FPGA_TYPE_ESSEN:

        case FPGA_TYPE_FIRENZE_CHEMNITZ:
        case FPGA_TYPE_FIRENZE_TEST:

        case FPGA_TYPE_GENOA_AUGSBURG:
        case FPGA_TYPE_GENOA_TEST:

        case FPGA_TYPE_HERCULANEUM_AUGSBURG:
        case FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION:
        case FPGA_TYPE_HERCULANEUM_TEST:

        case FPGA_TYPE_LATINA_AUGSBURG:
        case FPGA_TYPE_LATINA_LEONBERG:
        case FPGA_TYPE_LATINA_TEST:

        case FPGA_TYPE_LIVIGNO_AUGSBURG:
        case FPGA_TYPE_LIVIGNO_AUGSBURG_4X1:
        case FPGA_TYPE_LIVIGNO_FULDA_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690:
        case FPGA_TYPE_LIVIGNO_TEST:

        case FPGA_TYPE_LIVORNO_AUGSBURG:
        case FPGA_TYPE_LIVORNO_AUGSBURG_40:
        case FPGA_TYPE_LIVORNO_FULDA:
        case FPGA_TYPE_LIVORNO_LEONBERG:
        case FPGA_TYPE_LIVORNO_ODESSA_40:
        case FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G:
        case FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G:
        case FPGA_TYPE_LIVORNO_TEST:
        case FPGA_TYPE_LIVORNO_TEST_40:

        case FPGA_TYPE_LUCCA_AUGSBURG:
        case FPGA_TYPE_LUCCA_AUGSBURG_40:
        case FPGA_TYPE_LUCCA_LEONBERG_V7330:
        case FPGA_TYPE_LUCCA_LEONBERG_V7690:
        case FPGA_TYPE_LUCCA_TEST:

        case FPGA_TYPE_MILAN_AUGSBURG:
        case FPGA_TYPE_MILAN_TEST:

        case FPGA_TYPE_MONZA_AUGSBURG:
        case FPGA_TYPE_MONZA_TEST:

        case FPGA_TYPE_PISA_TEST:

            return FL_ADMIN_TYPE_ATMEL;

        case FPGA_TYPE_MANGO_AUGSBURG_100:
        case FPGA_TYPE_MANGO_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_AUGSBURG_40:
        case FPGA_TYPE_MANGO_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_ODESSA_40:
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1: /* Used to be FPGA_TYPE_PADUA_LEONBERG_VU9P */
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X40:
        case FPGA_TYPE_MANGO_04_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_LEONBERG_VU125:
        case FPGA_TYPE_MANGO_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_TEST:
        case FPGA_TYPE_MANGO_TEST_QDR:
        case FPGA_TYPE_MANGO_WOLFSBURG_4X100G:

        case FPGA_TYPE_PADUA_AUGSBURG_100:
        case FPGA_TYPE_PADUA_AUGSBURG_2X25:
        case FPGA_TYPE_PADUA_AUGSBURG_40:
        case FPGA_TYPE_PADUA_AUGSBURG_8X10:
        case FPGA_TYPE_PADUA_LEONBERG_VU125:
        case FPGA_TYPE_PADUA_TEST:

        case FPGA_TYPE_TORINO_TEST:

            return FL_ADMIN_TYPE_SMART_FUSION_2;

        case FPGA_TYPE_SAVONA_AUGSBURG_2X100:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X40:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X25:
        case FPGA_TYPE_SAVONA_AUGSBURG_8X10:
        case FPGA_TYPE_SAVONA_TEST:
        case FPGA_TYPE_SAVONA_WOLFSBURG_2X100G:
        case FPGA_TYPE_TIVOLI_TEST:
        case FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G:
        case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:

            return FL_ADMIN_TYPE_GIANT_GECKO;

        case FPGA_TYPE_2010:
        case FPGA_TYPE_2015:
        case FPGA_TYPE_2022:
        case FPGA_TYPE_BERGAMO_TEST:
        case FPGA_TYPE_COMO_TEST:

        case FPGA_TYPE_MARSALA_TEST:
        case FPGA_TYPE_SILIC100_AUGSBURG:
        case FPGA_TYPE_SILIC100_TEST:

        case FPGA_TYPE_CARDIFF_TEST:        /* TODO what controller??? */
        case FPGA_TYPE_CARDIFF_TEST_4X40G:  /* TODO what controller??? */
        case FPGA_TYPE_RIMINI_TEST:         /* TODO what controller??? */
        case FPGA_TYPE_VCU1525_TEST:        /* Not supported by this library */
        case FPGA_TYPE_UNKNOWN:

            break;

            /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
            /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return FL_ADMIN_TYPE_UNKNOWN;
}

/***************************************************************************/

// ActelRevision
FL_Error FL_AdminGetSoftwareRevision(const FL_FlashInfo * pFlashInfo, uint32_t * pSoftwareRevision)
{
    const FL_FpgaInfo * pFpgaInfo = pFlashInfo->pFpgaInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pFpgaInfo->AdminType)
    {
        case FL_ADMIN_TYPE_ATMEL:
        case FL_ADMIN_TYPE_SMART_FUSION_2:
            {
                uint16_t wordData = 0;

                *pSoftwareRevision = 0xDEAD;
                FL_ExitOnError(pLogContext, FL_AdminWaitStatusReady(pFlashInfo));
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0x9, 0, 0));/* Read MSB */
                FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &wordData));
                *pSoftwareRevision = (uint32_t)wordData << 16;
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0x8, 0, 0));/* Read LSB */
                FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &wordData));
                *pSoftwareRevision |= wordData;

                //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "%s Revision %d.%d.%d.%d\n",
                //    (flash_command_set == FLASH_CMD_SET_SF2 ? "SF2 SW" : "Actel"), (rev >> 24), ((rev >> 16) & 0xFF), ((rev >> 8) & 0xFF), (rev & 0xFF));
                FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "%s Revision %d.%d.%d.%d\n",
                    (pFpgaInfo->AdminType == FL_ADMIN_TYPE_SMART_FUSION_2 ? "SF2 SW" : "Actel"), /* TODO: output Atmel instead of Actel */
                    (*pSoftwareRevision >> 24), ((*pSoftwareRevision >> 16) & 0xFF), ((*pSoftwareRevision >> 8) & 0xFF), (*pSoftwareRevision & 0xFF));

            }
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_ADMIN_TYPE_GIANT_GECKO:

            //fprintf(stderr, " *** TODO Savona function %s not implemented\n", __func__);
            *pSoftwareRevision = 12345;
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_ADMIN_TYPE_UNKNOWN:
        case FL_ADMIN_TYPE_NONE:
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ADMIN_TYPE, "%s: invalid admin type %d (0x%X)",
        __func__, pFpgaInfo->AdminType, pFpgaInfo->AdminType);
}

/***************************************************************************/

FL_Error FL_AdminIsControllerPresent(const FL_FlashInfo * pFlashInfo, FL_FlashType flashType)
{
    if (flashType == FL_FLASH_TYPE_PARALLEL_MICRON) /* All Micron flashes have SF2 controller */
    {
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_ERROR_NO_ADMIN_CONTROLLER_PRESENT);
}

/***************************************************************************/

// ActelWaitReady
FL_Error FL_AdminWaitStatusReady(const FL_FlashInfo * pFlashInfo)
{
    void * pRegistersBaseAddress = pFlashInfo->pRegistersBaseAddress;
    int timeout = 20000; /* Number of read flash address rounds */

    while (true)
    {
        uint32_t adminStatus = FL_Read64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_ADMIN_COMMAND);
        if ((adminStatus & 0x00010000L) != 0) /* Is SPI controller ready? */
        {
            if ((adminStatus & 0x00020000L) != 0) /* SPI controller returned no ACK */
            {
                return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_READY_NO_ACK, "No ACK waiting for flash ready.");
            }
            else
            {
                return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
            }
        }
        else
        {
            if (--timeout == 0)
            {
                return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_READY_TIMEOUT, "Timeout waiting for SPI controller ready.");
            }
        }
    }
}

/***************************************************************************/

// (*Sf2Bindings.ActelFlashIsReadySF2)
FL_Error FL_AdminFlashIsReady(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    /* SF2 flash is ready when ??? TODO */
    uint16_t sf2Status;
    usleep(10);
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, flashWordAddress, 0));/* Write read-status command to Actel Controller */
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &sf2Status));

    if ((sf2Status & SF2_STATUS_FLASH_BUSY) != 0)
    {
        //prn(LOG_SCR | LEVEL_TRACE, "ActelFlashIsReady cmd: %04X\n", sf2Status);
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MICRON_SF2_IS_NOT_READY,
            "SF2 is not ready, status is 0x%04X%s", sf2Status, "");
    }
    else if ((sf2Status & (SF2_STATUS_READ_OVERRUN + SF2_STATUS_WRITE_OVERRUN + SF2_STATUS_EXEC_OVERRUN + SF2_STATUS_UNKNOWN_CMD)) != 0)
    {
        const char * errorMessage = "Unknown Command";
        if ((sf2Status & SF2_STATUS_READ_OVERRUN) != 0)
        {
            errorMessage = "Read Overrun";
        }
        if ((sf2Status & SF2_STATUS_WRITE_OVERRUN) != 0)
        {
            errorMessage = "Write Overrun";
        }
        if ((sf2Status & SF2_STATUS_EXEC_OVERRUN) != 0)
        {
            errorMessage = "Command Overrun";
        }
        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, flashWordAddress, SF2_CLEAR_OVERRUN)); /* Clear Status */

        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ActelFlashIsReady cmd: %04X (%s)\n", sf2Status, errorMessage);
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MICRON_SF2_IS_NOT_READY,
            "SF2 is not ready, status is 0x%04X, error %s", sf2Status, errorMessage);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelRead
FL_Error FL_AdminReadCommandRegister(const FL_FlashInfo * pFlashInfo, uint16_t * pWordData)
{
    FL_Error errorCode = FL_AdminWaitStatusReady(pFlashInfo); /* Wait until actel controller is ready. */
    if (errorCode == FL_SUCCESS)
    {
        *pWordData = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_ADMIN_COMMAND) & 0xFFFF;
    }

    return errorCode;
}

/***************************************************************************/

// ActelReadFlash
FL_Error FL_AdminReadWord(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress, uint16_t * pWordData)
{
    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        *pWordData = 0;
        fprintf(stderr, " *** TODO Savona function %s not implemented, flashAddress 0x%X", __func__, flashAddress);
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }
    FL_Error errorCode = FL_AdminWriteCommand(pFlashInfo, 1, flashAddress, 0);  /* Set read command */
    if (errorCode == FL_SUCCESS)
    {
        errorCode = FL_AdminReadCommandRegister(pFlashInfo, pWordData); /* Read data back */
    }
    return errorCode;
}

/***************************************************************************/

uint16_t FL_AdminGetWord(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress, FL_Error * pErrorCode)
{
    if (*pErrorCode == FL_SUCCESS)
    {
        const FL_FpgaInfo * pFpgaInfo = pFlashInfo->pFpgaInfo;
        uint16_t wordData;

        switch (pFpgaInfo->AdminType)
        {
            case FL_ADMIN_TYPE_ATMEL:
            case FL_ADMIN_TYPE_SMART_FUSION_2:
                *pErrorCode = FL_AdminReadWord(pFlashInfo, flashAddress, &wordData);
                if (*pErrorCode == FL_SUCCESS)
                {
                    return wordData;
                }
                break;

            case FL_ADMIN_TYPE_GIANT_GECKO:
                fprintf(stderr, " *** TODO Savona function %s not implemented, flashAddress 0x%X\n", __func__, flashAddress);
                if (flashAddress == 0x27)
                {
                    *pErrorCode = FL_SUCCESS;
                    return 26; /* Flash size in address bits */
                }
                break;

            case FL_ADMIN_TYPE_NONE:
            case FL_ADMIN_TYPE_UNKNOWN:
                *pErrorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ADMIN_TYPE, "%s: invalid admin type %d (0x%X) flashAddress 0x%X",
                    __func__, pFpgaInfo->AdminType, pFpgaInfo->AdminType, flashAddress);
                break;
        }
    }

    return 0xDEAD;
}

/***************************************************************************/

// ActelStatus
FL_Error FL_AdminReadStatus(const FL_FlashInfo * pFlashInfo, uint16_t * pAdminStatus)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, 0, 0)); /* Read status */
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, pAdminStatus));

    //prn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "%s Status: 0x%04X\n", (flash_command_set == FLASH_CMD_SET_SF2 ? "SF2 SW" : "Actel"), tmp);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_TRACE, "%s Status: 0x%04X\n",
        (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_SF2 ? "SF2 SW" : "FPGA"), *pAdminStatus);

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

//SF2ReadFlash
//Fill the SF2 read buffer
// Esa TODO : what the f*ck does this function do? What is SF2 read buffer?
FL_Error FL_AdminFillReadBuffer(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    int milliSeconds;
    FL_Error errorCode = FL_SUCCESS;

    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, flashAddress, READ_ARRAY_OPCODE)); /* Fill read buffer in SF2 */

    milliSeconds = 1000;
    while (((errorCode = FL_FlashIsReady(pFlashInfo, flashAddress)) == FL_ERROR_FLASH_IS_NOT_READY) && milliSeconds != 0) /* Wait until reading is done. */
    {
        --milliSeconds;
        usleep(1000);
        //prn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "SF2ReadFlash cmd: Not ready @ 0x%08X!\n", flashAddress);
        FL_Log(pLogContext, FL_LOG_LEVEL_TRACE, "Admin cmd: Not ready @ 0x%08X!\n", flashAddress);
    }
    if (milliSeconds <= 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_FATAL, "Timeout waiting for flash to be ready 1\n");
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MICRON_SF2_IS_NOT_READY, "Timeout waiting for SF2 read buffer to be filled\n");
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelWrite
FL_Error FL_AdminWriteCommand(const FL_FlashInfo * pFlashInfo, uint16_t command, uint32_t flashAddress, uint16_t wordData)
{
    void * pRegistersBaseAddress = pFlashInfo->pRegistersBaseAddress;
    FL_Error errorCode = FL_SUCCESS;
    const FL_FpgaInfo * pFpgaInfo = pFlashInfo->pFpgaInfo;

    if (pFpgaInfo->AdminType == FL_ADMIN_TYPE_GIANT_GECKO)
    {
        //fprintf(stderr, " *** TODO Savona function %s not implemented! command 0x%X, address 0x%X, data 0x%X.\n",
        //    __func__, command, flashAddress, wordData);
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    if (pFlashInfo->FlashSize != 0 && flashAddress >= pFlashInfo->FlashSize / 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_WRITE_ADDRESS_IS_OUT_OF_RANGE,
            "Trying to write outside Flash Address range! 0x%X outside of [0-0x%X]\n", flashAddress, pFlashInfo->AddressSize);
    }
    else if (pFlashInfo->AddressSize == 24)
    {
        const uint32_t MAX_ADDRESS = 16 * 1024 * 1024; /* 16 megabytes */

        if (flashAddress >= MAX_ADDRESS)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_WRITE_ADDRESS_IS_OUT_OF_RANGE,
                "Trying to write outside valid Flash Address range! 0x%X outside of [0-0x%X]\n", flashAddress, MAX_ADDRESS);
        }
        else
        {
            uint64_t writeWord = ((uint64_t)command << 56) + ((uint64_t)flashAddress << 32) + (wordData & 0xFFFF); /* No support for SF2 mode */
            errorCode = FL_AdminWaitStatusReady(pFlashInfo); /* Wait until actel controller is ready. */
            if (errorCode == FL_SUCCESS)
            {
                FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_ADMIN_COMMAND, writeWord); /* Write read command to Actel Controller */
            }
        }
    }
    else if (pFlashInfo->AddressSize == 28)
    {
        const uint32_t MAX_ADDRESS = 256 * 1024 * 1024; /* 256 megabytes */

        if (flashAddress >= MAX_ADDRESS)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_WRITE_ADDRESS_IS_OUT_OF_RANGE,
                "Trying to write outside valid Flash Address range! 0x%X outside of [0-0x%X]\n", flashAddress, MAX_ADDRESS);
        }
        else
        {
            uint64_t writeWord = ((uint64_t)command << 60) + ((uint64_t)flashAddress << 32) + (wordData & 0xFFFF);

            /* Here we select between writing to SF2 or Spansion/Micron flash, address bit 59 selects one or the other. */
            if (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_SF2)
            {
                writeWord += (1ULL << 59);
            }
            errorCode = FL_AdminWaitStatusReady(pFlashInfo); /* Wait until actel controller is ready. */
            if (errorCode == FL_SUCCESS)
            {
                FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_ADMIN_COMMAND, writeWord); /* Write read command to Actel Controller */
            }
        }
    }
    else
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_FLASH_ADDRESS_SIZE,
            "pFlashInfo->AddressSize has invalid size! %u: command 0x%X, address 0x%X, data 0x%X.\n",
            pFlashInfo->AddressSize, command, flashAddress, wordData);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

// ActelFlashReset
FL_Error FL_AdminReset(const FL_FlashInfo * pFlashInfo)
{
    const FL_FpgaInfo * pFpgaInfo = pFlashInfo->pFpgaInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pFpgaInfo->AdminType)
    {
        case FL_ADMIN_TYPE_ATMEL:
        case FL_ADMIN_TYPE_SMART_FUSION_2:
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xD, 0L, 1L));
#ifdef WIN32
            /* On windows it seems that the usleep is not very precise, we need to make sure that reset is finished */
            usleep(10000); /* Min 30us */
#else
            usleep(100); /* Min 30us */
#endif
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xD, 0L, 0L));
            usleep(1); /* Max 150ns */

            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_ADMIN_TYPE_GIANT_GECKO:
            //fprintf(stderr, " *** TODO Savona function %s not implemented\n", __func__);
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_ADMIN_TYPE_UNKNOWN:
        case FL_ADMIN_TYPE_NONE:
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ADMIN_TYPE, "%s: invalid admin type %d (0x%X)",
        __func__, pFpgaInfo->AdminType, pFpgaInfo->AdminType);
}

#pragma endregion

#pragma region Flash

/***************************************************************************/

// ActelInitFlash
FL_Error FL_GetFlashInformation(FL_FlashLibraryContext * pFlashLibraryContext)
{
    uint32_t adminSoftwareRevision = 0;
    FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    uint32_t * pRegistersBaseAddress = (uint32_t *)pFlashInfo->pRegistersBaseAddress;
    FL_Error errorCode = FL_SUCCESS;
    uint16_t adminStatus = 0;
    uint16_t flashSize;
    uint8_t adminGetControllerVersion;

    adminGetControllerVersion = FL_AdminGetControllerVersion(pRegistersBaseAddress);

    /* Set Actel Controller address size */
    switch (adminGetControllerVersion)
    {
        case 0x00:
            //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "BootFPGA is using 24 address lines.\n");
            FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "BootFPGA is using 24 address lines.");
            pFlashInfo->AddressSize = 24;
            break;
        case 0x01:
            //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "BootFPGA is using 28 address lines.\n");
            FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "BootFPGA is using 28 address lines.");
            pFlashInfo->AddressSize = 28;
            break;
        case 0x02:
//printf(" *** TODO Savona Boot FPGA Address Size ???\n");
            break;
        default:
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_CONTROLLER_VERSION,
                "Invalid controller version: %u (0x%02X)", adminGetControllerVersion, adminGetControllerVersion);
    }

    /* Test that we have access to the Actel FPGA */
    FL_ExitOnError(pLogContext, FL_AdminGetSoftwareRevision(pFlashInfo, &adminSoftwareRevision));
    if (adminSoftwareRevision == 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_INVALID_ADMIN_SOFTWARE_REVISION, "Invalid FPGA revision: %u", adminSoftwareRevision);
    }

    FL_ExitOnError(pLogContext, FL_AdminReset(pFlashInfo)); /* It could be in some unknown state */

    /* Read size as the first thing in order to enable range checking */
    if (pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
        pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
    {
        FL_ExitOnError(pLogContext, FL_QSPI_ReadFlashInfo(pFlashInfo, &pFlashInfo->DetailedFlashInfo));
        pFlashInfo->FlashSize = pFlashInfo->DetailedFlashInfo.Size;
        pFlashInfo->BankCount = 1;

        pFlashInfo->SectorZeroSize = 64 * 1024; // TODO what is Micron NOR flash SectorZeroSize
        pFlashInfo->LowSectorSize = 64 * 1024; // TODO what is Micron NOR flash LowSectorSize
        pFlashInfo->MiddleSectorSize = 64 * 1024; // TODO what is Micron NOR flash MiddleSectorSize
        pFlashInfo->TopSectorSize = 64 * 1024; // TODO what is Micron NOR flash TopSectorSize
        pFlashInfo->SectorCount = pFlashInfo->FlashSize / pFlashInfo->MiddleSectorSize; // TODO what is Micron NOR flash SectorCount
        pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] = pFlashInfo->FlashSize / pFlashInfo->MiddleSectorSize; // TODO set nonzero value to fool callers.
        pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] = pFlashInfo->FlashSize / pFlashInfo->MiddleSectorSize; // TODO set nonzero value to fool callers.
        pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = pFlashInfo->FlashSize / pFlashInfo->MiddleSectorSize; // TODO set nonzero value to fool callers.

        pFlashInfo->BufferCount = 128;

        pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0;
        pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY] = 0;
        pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0;

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
    }
    else
    {
        FL_ExitOnError(pLogContext, FL_GetFlashSize(pFlashInfo, &flashSize)); /* Determine pFlashInfo->FlashCommandSet */
        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0x98)); /* CFI entry (same command in Spansion and Micron command sets) */

        pFlashInfo->FlashSize = 1 << FL_AdminGetWord(pFlashInfo, 0x27, &errorCode);
        FL_ExitOnError(pLogContext, errorCode);

        if ((pFlashInfo->FlashSize > (1 << 28)) || (pFlashInfo->FlashSize < (1 << 24)))
        {
            int i;

            /* Dump CFI data to screen */
            for (i = 0x10; i < 0x39; ++i)
            {
                uint16_t flashData = FL_AdminGetWord(pFlashInfo, i, &errorCode);
                FL_ExitOnError(pLogContext, errorCode);
                printf("Flash CFI #2 0x%02X %04X\n", i, flashData);
            }
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_FLASH_ADDRESS_SIZE, "Unlikely flash size 0x%X bytes.", pFlashInfo->FlashSize);
        }
    }

    //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Flash Size 0x%X bytes\n", FlashInfo.FlashSize);
    FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "Flash Size 0x%X bytes\n", pFlashInfo->FlashSize);

    pFlashInfo->TypicalWordProgramTime = 1 << FL_AdminGetWord(pFlashInfo, 0x1F, &errorCode);
    pFlashInfo->TypicalBufferProgramTime = 1 << FL_AdminGetWord(pFlashInfo, 0x20, &errorCode);
    pFlashInfo->TypicalSectorEraseTime = (1 << FL_AdminGetWord(pFlashInfo, 0x21, &errorCode)) * 1000;
    pFlashInfo->MaxWordProgramTime = pFlashInfo->TypicalWordProgramTime * (1 << FL_AdminGetWord(pFlashInfo, 0x23, &errorCode));
    pFlashInfo->MaxBufferProgramTime = pFlashInfo->TypicalBufferProgramTime * (1 << FL_AdminGetWord(pFlashInfo, 0x24, &errorCode));
    pFlashInfo->MaxSectorEraseTime = pFlashInfo->TypicalSectorEraseTime * (1 << FL_AdminGetWord(pFlashInfo, 0x25, &errorCode));

    pFlashInfo->BufferCount = (1 << FL_AdminGetWord(pFlashInfo, 0x2A, &errorCode)) / 2;

    FL_ExitOnError(pLogContext, errorCode); /* Check errors in above FL_GetAdminWord calls */

    if (FL_AdminGetWord(pFlashInfo, 0x2C, &errorCode) == 3)
    {
        /* Spansion MirrorBit Flash has 3 descriptors */
        pFlashInfo->LowSectorCount = (FL_AdminGetWord(pFlashInfo, 0x2E, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x2D, &errorCode);
        pFlashInfo->LowSectorCount++;

        pFlashInfo->LowSectorSize = (FL_AdminGetWord(pFlashInfo, 0x30, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x2F, &errorCode);
        pFlashInfo->LowSectorSize *= 256;

        pFlashInfo->MiddleSectorCount = (FL_AdminGetWord(pFlashInfo, 0x32, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x31, &errorCode);
        pFlashInfo->MiddleSectorCount++;

        pFlashInfo->MiddleSectorSize = (FL_AdminGetWord(pFlashInfo, 0x34, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x33, &errorCode);
        pFlashInfo->MiddleSectorSize *= 256;

        pFlashInfo->TopSectorCount = (FL_AdminGetWord(pFlashInfo, 0x36, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x35, &errorCode);
        pFlashInfo->TopSectorCount++;

        pFlashInfo->TopSectorSize = (FL_AdminGetWord(pFlashInfo, 0x38, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x37, &errorCode);
        pFlashInfo->TopSectorSize *= 256;

        pFlashInfo->SectorCount = pFlashInfo->LowSectorCount + pFlashInfo->MiddleSectorCount + pFlashInfo->TopSectorCount;
    }
    else
    {
        /* Micron Strata Flash has only 1 descriptor, because all sectors are of same size */
        pFlashInfo->SectorCount = (FL_AdminGetWord(pFlashInfo, 0x2E, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x2D, &errorCode);
        pFlashInfo->SectorCount++;

        pFlashInfo->LowSectorSize = (FL_AdminGetWord(pFlashInfo, 0x30, &errorCode) << 8) | FL_AdminGetWord(pFlashInfo, 0x2F, &errorCode);
        pFlashInfo->LowSectorSize *= 256;

        pFlashInfo->MiddleSectorSize = pFlashInfo->LowSectorSize;
        pFlashInfo->TopSectorSize = pFlashInfo->LowSectorSize;

        pFlashInfo->LowSectorCount = 1;
        pFlashInfo->MiddleSectorCount = pFlashInfo->SectorCount - 1 - 1;
        pFlashInfo->TopSectorCount = 1;
    }

    FL_ExitOnError(pLogContext, errorCode); /* Check errors in above FL_GetAdminWord calls */

    pFlashInfo->SectorZeroSize = SECTOR0DATA_SIZE;

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            pFlashInfo->BankCount = FL_AdminGetWord(pFlashInfo, 0x57, &errorCode);
            FL_ExitOnError(pLogContext, errorCode); /* Check errors in above FL_GetAdminWord call */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0xF0)); /* CFI Exit command for bank 0 */
            break;

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            pFlashInfo->BankCount = 8;
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0xFF)); /* Return to Read Array command */
            break;

        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            fprintf(stderr, " *** TODO Savona function %s pFlashInfo->FlashCommandSet not implemented!\n", __func__);
            break;

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_SF2:
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
                "Unknown or invalid flash command set in this context: %u\n", pFlashInfo->FlashCommandSet);
    }

#if 0
    if (FlashInfo.TypBufProgTime == 0)
        DoExceptMsg("No typical buffer program time found in flash\n");

    if (FlashInfo.BufCnt == 0)
        DoExceptMsg("No max buffer count found in flash\n");

    if (FlashInfo.BankCnt == 0)
        DoExceptMsg("Could not read number of banks in flash\n");
#endif

    /* Compute derived flash data */
    pFlashInfo->BankSize = pFlashInfo->FlashSize / pFlashInfo->BankCount;

    if (pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
        pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
    {
        pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0x10000;
        pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY] = 0x800000;
        pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0xF00000;
        pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] = 127 - 16;
        pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] = 127 - 16;
        pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 15;
    }
    else
    {
        /* Set image start addresses */
        FL_ExitOnError(pLogContext, FL_AdminReadStatus(pFlashInfo, &adminStatus));
        switch ((adminStatus & ACTEL_STATUS_ADDRESS_MAP) >> 8)
        {
            case 0: /* Ancona, WHAT ELSE? */
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0x10000;
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY] = 0x800000;
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0xF00000;
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] = 127 - 16;
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] = 127 - 16;
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 15;
                /* Modify middle sector count and top sector count in the case that we have a */
                /* large 512Mbit flash, but only 24 address lines available */
                if (pFlashInfo->FlashSize > 0x2000000) /* 32 MB */
                {
                    pFlashInfo->TopSectorCount = 0;
                    pFlashInfo->MiddleSectorCount /= 2;
                    //prn(LOG_SCR | LOG_FILE | LEVEL_ALL, "Note: 512Mbit flash detected. Only use of first half is supported.\n");
                    FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "Note: 512Mbit flash detected. Only use of first half is supported.\n");
                }
                break;
            case 1: /* Livorno, WHAT ELSE? */
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0x10000;
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_SECONDARY] = 0x1000000;
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0x1F00000;
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] = 255 - 16; /* 239 */
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] = 255 - 16; /* 239 */
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 15;
                break;
            case 2: /* WHAT ELSE? */
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0x0020000;
                pFlashInfo->ImageStartAddress[1] = 0x1000000;
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0x1F00000;
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] = (pFlashInfo->ImageStartAddress[1] - pFlashInfo->ImageStartAddress[0]) / (pFlashInfo->MiddleSectorSize / 2);
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] = (pFlashInfo->ImageStartAddress[2] - pFlashInfo->ImageStartAddress[1]) / (pFlashInfo->MiddleSectorSize / 2);
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = (pFlashInfo->FlashSize / 2 - pFlashInfo->ImageStartAddress[2]) / (pFlashInfo->MiddleSectorSize / 2);
                break;
            case 3: /* Mango, WHAT ELSE? */
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_PRIMARY] = 0x0020000;
                pFlashInfo->ImageStartAddress[1] = 0x0020000; //TODO ESA MANGO-20 ???
                pFlashInfo->ImageStartAddress[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0x1F00000;
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_PRIMARY] = 511 - 16; /* 495 */
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_SECONDARY] = 511 - 16; /* 495 */
                pFlashInfo->SectorsPerImage[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = 0;
                break;
            default:
                errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_FLASH_IMAGE_START_ADDRESS, "Unsupported flash image start address.");
                break;
        }
    }

    {
        FL_ImageTarget i;
        for (i = 0; i < 3; ++i)
        {
            //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "ImageStartAddr[%u]:0x%X\n", i, FlashInfo.ImageStartAddr[i]);
            FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "ImageStartAddr[%u]:0x%X\n", i, pFlashInfo->ImageStartAddress[i]);
            //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "SectorsPerImage[%u]:%u\n", i, FlashInfo.SecPerImg[i]);
            FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "SectorsPerImage[%u]:%u\n", i, pFlashInfo->SectorsPerImage[i]);
        }
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

static void DecodePCIeEndPoint(const FL_Parameters * pParameters, uint16_t status, FL_PCIeEndPointInfo * pEndPoint)
{
    pEndPoint->LinkUpAndReady = ((status & 0xC000ULL) >> 14) == 0x3 ? true : false;
    pEndPoint->Generation = (status & 0xF) == 4 ? 3 : status & 0xF;
    pEndPoint->Width = (status >> 4) & 0x3F;
    if (pEndPoint->Width == 15)
    {
        pEndPoint->Width = 16;
    }
    if (pParameters->ImpersonatePCIeGeneration > 0)
    {
        pEndPoint->Generation = pParameters->ImpersonatePCIeGeneration;
    }
    if (pParameters->ImpersonatePCIeWidth)
    {
        pEndPoint->Width = pParameters->ImpersonatePCIeWidth;
    }

    FL_Log(pParameters->pLogContext, FL_LOG_LEVEL_TRACE, "status 0x%4x: link %5s gen %u width %u expwidth %u\n",
        status, pEndPoint->LinkUpAndReady ? "true" : "false", pEndPoint->Generation, pEndPoint->Width, pEndPoint->ExpectedWidth);
}

/***************************************************************************/

FL_Error FL_GetFpgaRevisionInformation(const FL_FlashInfo * pFlashInfo, FL_FpgaInfo * pFpgaInfo)
{
    /* Get FPGA revision */
    pFpgaInfo->RawRevision = FL_GetRevision(pFlashInfo->pRegistersBaseAddress);
    if ((pFpgaInfo->RawRevision == 0) || (pFpgaInfo->RawRevision == 0xFFFFFFFFFFFFFFFFLL))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_READ_FPGA_REVISION, "Failed to read FPGA revision, PCIe failure?");
    }
    pFpgaInfo->Major = (pFpgaInfo->RawRevision >> 24) & 0xFF;
    pFpgaInfo->Minor = (pFpgaInfo->RawRevision >> 16) & 0xFF;
    pFpgaInfo->Sub = (pFpgaInfo->RawRevision >> 8) & 0xFF;
    pFpgaInfo->Build = (pFpgaInfo->RawRevision >> 0) & 0xFF;
    pFpgaInfo->ExtendedBuild = FL_GetExtendedBuildNumber(pFlashInfo->pRegistersBaseAddress);
    /* Select extended build number when it is not a release and the extended build number is not zero and the capability structure is present in firmware */
    if ((pFpgaInfo->Build != 0) && (pFpgaInfo->ExtendedBuild != 0) && ((uint32_t)(pFpgaInfo->RawRevision & 0xFFFFFFFF) != pFpgaInfo->ExtendedBuild))
    {
        pFpgaInfo->Build = pFpgaInfo->ExtendedBuild;
    }
    if (pFpgaInfo->Major > 20)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "FPGA version: %u.%u.%u.%u  [ FAILED ]\n", revision_major, revision_minor, revision_sub, revision_build);
        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Unlikely FPGA version found. Please check that driver matches PCI vendor and device ID.\n");
        return -FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNLIKELY_FPGA_REVISION_FOUND,
            "Unlikely FPGA version %u.%u.%u.%u found. Please check that driver matches PCI vendor and device ID.",
            pFpgaInfo->Major, pFpgaInfo->Minor, pFpgaInfo->Sub, pFpgaInfo->Build);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_GetFpgaBuildTimeInformation(const FL_FlashInfo * pFlashInfo, FL_FpgaInfo * pFpgaInfo)
{
    /* Get FPGA firmware build time: */
    pFpgaInfo->RawBuildTime = FL_GetBuildTime(pFlashInfo->pRegistersBaseAddress);
    if (pFpgaInfo->RawBuildTime < 1E9 || pFpgaInfo->RawBuildTime > 3E9) /* Sanity check on build time */
    {
        pFpgaInfo->RawBuildTime = 0;
        memset(pFpgaInfo->BuildTimeString, 0, sizeof(pFpgaInfo->BuildTimeString));
    }
    else
    {
        /* Build time is the result of bash command "date +%s" */
        struct tm buildTime;
        time_t rawBuildTime = (time_t)pFpgaInfo->RawBuildTime; /* Raw build time as time_t */
        struct tm * pBuildTime = gmtime_r(&rawBuildTime, &buildTime);
        if (pBuildTime == NULL)
        {
            FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CONVERT_BUILD_TIME_TO_UTC, "Failed to convert FPGA build time to local time.");
        }
        strftime(pFpgaInfo->BuildTimeString, sizeof(pFpgaInfo->BuildTimeString), "%Y-%m-%d %H:%M:%S", pBuildTime);
        pFpgaInfo->BuildTime = *pBuildTime;
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_GetPCIeInformation(const FL_FlashInfo * pFlashInfo, const FL_FpgaInfo * pFpgaInfo, const FL_Parameters * pParameters, FL_PCIeInfo * pPCIeInfo)
{
    /* PCIe related information */
    pPCIeInfo->Status = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_PCIE_CONFIG_STATUS);
    if ((pPCIeInfo->Status & 0xFFFFFFFF00000000ULL) == 0xDEADBEEF00000000ULL)
    {
        DecodePCIeEndPoint(pParameters, pPCIeInfo->Status & 0xFFFF, &pPCIeInfo->EndPoints[0]);          /* First PCIe end point: */
        DecodePCIeEndPoint(pParameters, (pPCIeInfo->Status >> 16) & 0xFFFF, &pPCIeInfo->EndPoints[1]);  /* Second PCIe end point: */
    }
    pPCIeInfo->EndPoints[0].ExpectedGeneration = FL_GetExpectedPCIeGenerationFromFpgaModel(pFlashInfo->pErrorContext, pFpgaInfo->Model);
    pPCIeInfo->EndPoints[0].ExpectedWidth = FL_GetExpectedPCIeWidthFromFpgaModel(pFlashInfo->pErrorContext, pFpgaInfo->Model);
    pPCIeInfo->EndPoints[1].ExpectedGeneration = FL_GetExpectedPCIeGenerationFromFpgaModel(pFlashInfo->pErrorContext, pFpgaInfo->Model);
    pPCIeInfo->EndPoints[1].ExpectedWidth = FL_GetExpectedPCIeWidthFromFpgaModel(pFlashInfo->pErrorContext, pFpgaInfo->Model);
    if (pPCIeInfo->EndPoints[0].LinkUpAndReady != FL_GetExpectedPCIeLinkStateFromFpgaModel(pFlashInfo->pErrorContext, pFpgaInfo->Model))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_PCIE_END_POINT_NOT_UP_AND_READY, "PCIe end point is not up and ready.");
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelGetFlashSize
FL_Error FL_GetFlashSize(FL_FlashInfo * pFlashInfo, uint16_t * pFlashSize)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint16_t deviceid[4];

    if (pFlashInfo->pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
        pFlashInfo->pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
    {
        fprintf(stderr, " *** TODO Savona function %s not implemented\n", __func__);
        *pFlashSize = 12345;
        pFlashInfo->FlashCommandSet = FL_FLASH_COMMAND_SET_MACRONIX;
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    /* Get Device ID */
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Unlock 0
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Unlock 1
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0x90));//Autoselect set entry
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 1, 0x00, 0x0));//Autoselect device id
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &deviceid[0]));
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 1, 0x01, 0x0));//Autoselect device id
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &deviceid[1]));
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 1, 0x0E, 0x0));//Autoselect device id
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &deviceid[2]));
    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 1, 0x0F, 0x0));//Autoselect device id
    FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &deviceid[3]));

    if (deviceid[0] == 0x0001) /* Spansion MirrorBit Flash */
    {
        pFlashInfo->FlashCommandSet = FL_FLASH_COMMAND_SET_SPANSION;
        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0xF0));//Autoselect Exit by writing Reset command

        if (deviceid[1] != 0x227E)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_SPANSION_FLASH_DEVICE,
                "Invalid Spansion flash device. Expected 0x%04X != 0x%04X", 0x227E, deviceid[1]);
        }
        if (deviceid[3] != 0x2200)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_SPANSION_FLASH_DEVICE,
                "Invalid Spansion flash device. Expected 0x%04X != 0x%04X", 0x2200, deviceid[1]);
        }
        if (deviceid[2] == 0x2244)
        {
            *pFlashSize = 128;
        }
        else if (deviceid[2] == 0x2242)
        {
            *pFlashSize = 256;
        }
        else if (deviceid[2] == 0x223D)
        {
            *pFlashSize = 512;
        }
        else
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_DEVICE, "Unknown flash device size: %04X %04X %04X %04X",
                deviceid[0], deviceid[1], deviceid[2], deviceid[3]);
        }

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }
    else if (deviceid[0] == 0x0089) /* Micron Strata Flash */
    {
        pFlashInfo->FlashCommandSet = FL_FLASH_COMMAND_SET_MICRON;
        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0xFF));//Return to Read Array mode
        pFlashInfo->Updatable.MicronFlashReadMode = FL_MICRON_READ_MODE_ARRAY;

        if (deviceid[1] == 0x8901 || deviceid[1] == 0x8904)
        {
            *pFlashSize = 256;
        }
        else if (deviceid[1] == 0x887E || deviceid[1] == 0x8881)
        {
            *pFlashSize = 512;
        }
        else if (deviceid[1] == 0x88B0 || deviceid[1] == 0x88B1)
        {
            *pFlashSize = 1024;
        }
        else
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_MICRON_FLASH_DEVICE,
                "Invalid Micron flash device. Expected 0x%04X != 0x%04X", 0x227E, deviceid[1]);
        }

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_DEVICE, "Unknown flash device: %04X %04X %04X %04X",
        deviceid[0], deviceid[1], deviceid[2], deviceid[3]);
}

/***************************************************************************/

FL_Error FL_HasPrimaryFlash(const FL_FpgaInfo * pFpgaInfo, FL_Boolean * pHasPrimaryFlash)
{
    switch (pFpgaInfo->CardType)
    {
        case FL_CARD_TYPE_LIVORNO:
        case FL_CARD_TYPE_MANGO:
        case FL_CARD_TYPE_PADUA:
            *pHasPrimaryFlash = true;
            return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_SUCCESS);

        case FL_CARD_TYPE_RAW: /* actually we don't know but let's say there is no primary flash */
            *pHasPrimaryFlash = false;
            return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_SUCCESS);

        case FL_CARD_TYPE_20XX:
        case FL_CARD_TYPE_ANCONA:
        case FL_CARD_TYPE_ATHEN:
        case FL_CARD_TYPE_BERGAMO:
        case FL_CARD_TYPE_CARDIFF:
        case FL_CARD_TYPE_COMO:
        case FL_CARD_TYPE_ERCOLANO:
        case FL_CARD_TYPE_ESSEN:
        case FL_CARD_TYPE_FIRENZE:
        case FL_CARD_TYPE_GENOA:
        case FL_CARD_TYPE_HERCULANEUM:
        case FL_CARD_TYPE_LATINA:
        case FL_CARD_TYPE_LIVIGNO:
        case FL_CARD_TYPE_LUCCA:
        case FL_CARD_TYPE_LUXG:
        case FL_CARD_TYPE_MARSALA:
        case FL_CARD_TYPE_MILAN:
        case FL_CARD_TYPE_MONZA:
        case FL_CARD_TYPE_PISA:
        case FL_CARD_TYPE_RIMINI:
        case FL_CARD_TYPE_SAVONA:
        case FL_CARD_TYPE_TIVOLI:
        case FL_CARD_TYPE_SILIC100:
        case FL_CARD_TYPE_TORINO:
        case FL_CARD_TYPE_UNKNOWN:
            break; // TODO categorize these cards!
    }

    FL_Log(pFpgaInfo->pLogContext, FL_LOG_LEVEL_ERROR,
        "Card type %u (0x%X, %s) UNKNOWN primary image.",
        pFpgaInfo->CardType, pFpgaInfo->CardType,
        FL_GetCardTypeName(pFpgaInfo->pErrorContext, pFpgaInfo->CardType, true));
    *pHasPrimaryFlash = false;

    return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_SUCCESS); // TODO: should return the below error code?
    //return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_ERROR_UNKNOWN_PRIMARY_FLASH_IMAGE);
}

/***************************************************************************/

FL_Error FL_HasSecondaryFlash(FL_FpgaInfo * pFpgaInfo, FL_Boolean * pHasSecondaryFlash)
{
    switch (pFpgaInfo->CardType)
    {
        case FL_CARD_TYPE_LIVORNO:
        case FL_CARD_TYPE_MANGO:
        case FL_CARD_TYPE_PADUA:
            *pHasSecondaryFlash = true;
            return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_SUCCESS);

        case FL_CARD_TYPE_RAW: /* actually we don't know but let's say there is no secondary flash */
            *pHasSecondaryFlash = false;
            return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_SUCCESS);

        case FL_CARD_TYPE_20XX:
        case FL_CARD_TYPE_ANCONA:
        case FL_CARD_TYPE_ATHEN:
        case FL_CARD_TYPE_BERGAMO:
        case FL_CARD_TYPE_CARDIFF:
        case FL_CARD_TYPE_COMO:
        case FL_CARD_TYPE_ERCOLANO:
        case FL_CARD_TYPE_ESSEN:
        case FL_CARD_TYPE_FIRENZE:
        case FL_CARD_TYPE_GENOA:
        case FL_CARD_TYPE_HERCULANEUM:
        case FL_CARD_TYPE_LATINA:
        case FL_CARD_TYPE_LIVIGNO:
        case FL_CARD_TYPE_LUCCA:
        case FL_CARD_TYPE_LUXG:
        case FL_CARD_TYPE_MARSALA:
        case FL_CARD_TYPE_MILAN:
        case FL_CARD_TYPE_MONZA:
        case FL_CARD_TYPE_PISA:
        case FL_CARD_TYPE_RIMINI:
        case FL_CARD_TYPE_SAVONA:
        case FL_CARD_TYPE_TIVOLI:
        case FL_CARD_TYPE_SILIC100:
        case FL_CARD_TYPE_TORINO:
        case FL_CARD_TYPE_UNKNOWN:
            break; // TODO categorize these cards!
    }

    FL_Log(pFpgaInfo->pLogContext, FL_LOG_LEVEL_ERROR,
        "Card type %u (0x%X, %s) UNKNOWN seconday image.",
        pFpgaInfo->CardType, pFpgaInfo->CardType,
        FL_GetCardTypeName(pFpgaInfo->pErrorContext, pFpgaInfo->CardType, true));
    *pHasSecondaryFlash = false;

    return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_SUCCESS); // TODO: should return the below error code?
    //return FL_SetLastErrorCode(pFpgaInfo->pErrorContext, FL_ERROR_UNKNOWN_SECONDARY_FLASH_IMAGE);
}

#pragma endregion

#pragma region Miscellaneous

/***************************************************************************/

void FL_HexDump(int(*pPrint)(const char * format, ...), const void * pMemory, uint64_t length, uint16_t bytesPerLine, ssize_t addressOffset)
{
    const uint8_t * bytes = (const uint8_t *)pMemory;
    char * ascii = alloca(bytesPerLine + 1);
    uint64_t lineCount = 0;
    uint64_t i = 0;

    if (pPrint == NULL)
    {
        pPrint = &printf;
    }

    memset(ascii, ' ', bytesPerLine + 1);
    ascii[bytesPerLine] = '\0';
    for (i = 0; i < length; ++i)
    {
        if (i % bytesPerLine == 0)
        {
            uint64_t address = (uint64_t)(bytes + i + addressOffset);
            pPrint("%07X ", address);
        }
        uint8_t byte = bytes[i];
        ascii[i % bytesPerLine] = byte >= 32 && byte < 127 ? (char)byte : '.';
        pPrint(" %02X", byte);
        if ((i + 1) % 8 == 0 && (i + 1) % bytesPerLine != 0)
        {
            pPrint(" ");
        }
        if ((i + 1) % bytesPerLine == 0)
        {
            pPrint("  |%s|\n", ascii);
            if (++lineCount % 8 == 0)
            {
                pPrint("\n");
            }
        }
    }
    if (i % bytesPerLine != 0)
    {
        uint64_t remainingLineBytes = bytesPerLine - i % bytesPerLine;
        for (i = 0; i < remainingLineBytes; ++i)
        {
            pPrint("   ");
        }
        while (remainingLineBytes > 8)
        {
            pPrint("  "); /* Extra fill for "  " */
            remainingLineBytes -= 8;
        }
        ascii[i % bytesPerLine] = '\0';
        pPrint("  |%s|\n", ascii);
    }
}

/***************************************************************************/

const uint64_t FL_ONE_BILLION_NS = 1000000000;

/***************************************************************************/

FL_Error FL_GetTimestamp(FL_ErrorContext * pErrorContext, uint64_t * pTimeStamp)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_GET_SYSTEM_TIME,
            "clock_gettime failed with errno %d", errno);
    }
    *pTimeStamp = (uint64_t)now.tv_sec * FL_ONE_BILLION_NS + (uint64_t)now.tv_nsec;

    return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_GetRawTimestamp(FL_ErrorContext * pErrorContext, uint64_t * pRawTimeStamp)
{
#ifdef CLOCK_MONOTONIC_RAW
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0)
    {
        return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_GET_SYSTEM_TIME,
            "clock_gettime failed with errno %d", errno);
    }
    *pRawTimeStamp = (uint64_t)now.tv_sec * FL_ONE_BILLION_NS + (uint64_t)now.tv_nsec;

    return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
#else
    return FL_GetTimestamp(pErrorContext, pRawTimeStamp);
#endif
}

/***************************************************************************/

/**
 *  Get current system time in nanoseconds since the Epoch (UTC 00:00:00 1 January 1970).
 *
 *  @return     Current system time in nanoseconds.
 */
FL_Error FL_GetTimeNow(FL_ErrorContext * pErrorContext, uint64_t * pTimeNow)
{
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0)
    {
        return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_GET_SYSTEM_TIME,
            "clock_gettime failed with errno %d", errno);
    }
    *pTimeNow = (uint64_t)now.tv_sec * FL_ONE_BILLION_NS + (uint64_t)now.tv_nsec;

    return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
}

#pragma endregion

#pragma region Gecko

/***************************************************************************/

/*
 * Wait for FPGA to indicate Gecko ready, maybe with data
 */
FL_Error FL_GeckoWaitReadyWithData(const FL_FlashInfo * pFlashInfo, uint16_t * pData)
{
#if 1
    int timeout = 20000;

    while (true)
    {
        uint64_t registerValue;

        usleep(1); //needed, otherwise some bytes are read as 0x35, to be fixed in FPGA

        registerValue = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_ADMIN_COMMAND);
        if ((registerValue & (1UL << 16)) == (1UL << 16))
        {
            /* Microcontroller is ready */
            *pData = (uint16_t)(registerValue & 0xFFFF);

            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
        }
        else
        {
            if (--timeout <= 0)
            {
                return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_GECKO_TIMEOUT, "TImeout waiting for Gecko data");
            }
            usleep(300); /* with this value polls 1-3 times */
        }
    }
#else
    int timeout = 20000;

    while (true)
    {
        uint64_t registerValue;

        //usleep(1); //needed, otherwise some bytes are read as 0x35, to be fixed in FPGA

        registerValue = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_OFFSET_ADMIN_COMMANDS);
        if ((registerValue & (1UL << 16)) == (1UL << 16))
        {
            /* Microcontroller is ready */
            if ((regVal & (1UL << 18)) == (1UL << 18))
            {
                *pData = (uint16_t)(registerValue & 0xFFFF);
                //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "timeout reg: 0x%0X, Read: 0x%0X, timeout %d\n", registerValue, *pData, timeout);
                FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "timeout reg: 0x%0X, Read: 0x%0X, timeout %d\n", registerValue, *pData, timeout);
                return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_ERROR_GECKO_TIMEOUT);
            }
            *pData = registerValue & 0xFFFF;
            //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "reg: 0x%0X, Read: 0x%0X, timeout %d\n", regVal, *pData, timeout);
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "reg: 0x%0X, Read: 0x%0X, timeout %d\n", regVal, *pData, timeout);
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
        }
        else
        {
            if (--timeout <= 0)
            {
                //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Timeout waiting for Gecko\n");
                FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Timeout waiting for Gecko\n");
                //return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_GECKO_TIMEOUT, "TImeout waiting for Gecko data");
                return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_ERROR_GECKO_TIMEOUT);
            }
            usleep(10); /* with this value polls 1-3 times */
        }
    }
#endif
}

/***************************************************************************/

/*
 * Wait for FPGA to indicate Gecko ready for new command (discard data)
 */
FL_Error FL_GeckoWaitReadyForCommand(const FL_FlashInfo * pFlashInfo)
{
    uint16_t dummyData;

    return FL_GeckoWaitReadyWithData(pFlashInfo, &dummyData);
}

/***************************************************************************/

/*
 * Write a command to Gecko
 */
FL_Error FL_GeckoWrite(const FL_FlashInfo * pFlashInfo, uint8_t command, uint16_t subCommand, uint32_t data)
{
#if 1
    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, GeckoWrite(pFlashInfo->pRegistersBaseAddress, command, subCommand, data) == 0 ? FL_SUCCESS : FL_ERROR_GECKO_TIMEOUT);
#else
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint64_t registerValue = ((uint64_t)command & 0x7) << 60 | ((uint64_t)subCommand & 0xFFF) << 48 | data;

    //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "GeckoWrite cmd: 0x%X\n", registerValue);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Gecko command 0x%016lX\n", registerValue);

    //add check ready before write ?
    FL_ExitOnError(pLogContext, FL_GeckoWaitReadyForCommand(pFlashInfo));

    /* Write command to Gecko Microcontroller */
    FL_Write64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_OFFSET_ADMIN_COMMANDS, registerValue);

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
#endif
}

/***************************************************************************/

FL_Error FL_GeckoReadPcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t * pPcbVersion)
{
#if 1
    *pPcbVersion = GeckoReadPcbVer(pFlashInfo->pRegistersBaseAddress);
    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
#else
    uint8_t data[2]; /* Space for 2 bytes required */
    FL_Error errorCode = FL_GeckoReadFlash1PageWords(pFlashInfo, FL_GECKO_FLASH_SILICOM_AREA_1, FL_GECKO_PCB_INFO_BYTE_OFFSET, sizeof(data), data);
    //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "PCB version 0: %d, 1: %d\n", data[0], data[1]);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "PCB version 0: %u, 1: %u\n", data[0], data[1]);

    *pPcbVersion = data[0];

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
#endif
}

/***************************************************************************/

#if 0 // TODO FL_GeckoWritePcbVersion not implemented yet!
FL_Error FL_GeckoWritePcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t pcbVersion)
{
    if (pcbVersion > 255)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: illegal PCB version number\n");
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ARGUMENT,
            "Invalid PCB version number %u", pcbVersion);
        return 1;
    }
    else
    {
        uint8_t buffer[GeckoFlash1PageSize];
        GeckoReadFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer);

        // Only use the last byte for PCB ver, other reserved for FPGA type;
        uint8_t  pcbVerData = pcbVersion & 0xFF;
        buffer[GECKO_PCB_INFO] = pcbVerData;
        GeckoWriteFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, buffer);

        //Check read
        uint8_t pcbVerRead[2]; // space for 2 bytes required
        GeckoReadLenFlash1Page(pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, GECKO_PCB_INFO, 2, pcbVerRead);
        prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "PCB version written: %d, read: %d\n", pcbVerData, pcbVerRead[0]);
        if (pcbVerRead[0] == pcbVerData)
        {
            prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCB version written: %d\n", pcbVerData);
        }
        else
        {
            prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: PCB version number verification failed\n");
            return 1;
        }
    }
    return 0;
}
#endif

/***************************************************************************/

FL_Error FL_GeckoSelectPrimaryFlash(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    FL_ExitOnError(pLogContext, FL_GeckoWaitReadyForCommand(pFlashInfo));
    FL_ExitOnError(pLogContext, FL_GeckoWrite(pFlashInfo, FL_GECKO_CMD_CONTROL, FL_GECKO_SUBCMD_CTRL_SELECT_PRIMARY_FPGA_FLASH, 0));
    sleep(1);
    return FL_GeckoWaitReadyForCommand(pFlashInfo);
}

/***************************************************************************/

FL_Error FL_GeckoSelectFailSafeFlash(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    FL_ExitOnError(pLogContext, FL_GeckoWaitReadyForCommand(pFlashInfo));
    FL_ExitOnError(pLogContext, FL_GeckoWrite(pFlashInfo, FL_GECKO_CMD_CONTROL, FL_GECKO_SUBCMD_CTRL_SELECT_FAILSAFE_FPGA_FLASH, 0));
    sleep(1);
    return FL_GeckoWaitReadyForCommand(pFlashInfo);
}

#pragma endregion

#pragma region QSPI

/***************************************************************************/

/*
    Reference:
        XILINX AXI Quad SPI v3.2
        LogiCORE IP Product Guide
        file pg153-axi-quad-spi.pdf

*/

/***************************************************************************/

/**
 *
 */
typedef enum
{
    /* SPI registers */
    FL_SPI_REGISTER_SRR             = 0x40, /**< (0x40) SPI Software Reset Register. */
    FL_SPI_REGISTER_CR              = 0x60, /**< (0x60) SPI Control Register. */
    FL_SPI_REGISTER_SR              = 0x64, /**< (0x64) SPI Status Register. */
    FL_SPI_REGISTER_DTR             = 0x68, /**< (0x68) SPI DTR (Double Transfer Rate) register */
    FL_SPI_REGISTER_DRR             = 0x6C, /**< (0x6C) SPI Data Receive Register. A single register or a FIFO. */
    FL_SPI_REGISTER_SSR             = 0x70, /**< (0x70) SPI Slave Select Register */
    FL_SPI_REGISTER_TX_FIFO_FILL    = 0x74, /**< (0x74) SPI Transmit FIFO Occupancy (fill) Register */
    FL_SPI_REGISTER_RX_FIFO_FILL    = 0x78, /**< (0x78) SPI Receive FIFO Occupancy (fill) Register */

#ifndef XSP_CR_LOOPBACK_MASK
    XSP_CR_LOOPBACK_MASK        = 0x00000001,   /**< (0x00000001) Local loopback modlle */
    XSP_CR_ENABLE_MASK          = 0x00000002,   /**< (0x00000002) System enable */
    XSP_CR_MASTER_MODE_MASK     = 0x00000004,   /**< (0x00000004) Enable master mode */
    XSP_CR_CLK_POLARITY_MASK    = 0x00000008,   /**< (0x00000008) Clock polarity high or low */
    XSP_CR_CLK_PHASE_MASK       = 0x00000010,   /**< (0x00000010) Clock phase 0 or 1 */
    XSP_CR_TXFIFO_RESET_MASK    = 0x00000020,   /**< (0x00000020) Reset transmit FIFO */
    XSP_CR_RXFIFO_RESET_MASK    = 0x00000040,   /**< (0x00000040) Reset receive FIFO */
    XSP_CR_MANUAL_SS_MASK       = 0x00000080,   /**< (0x00000080) Manual slave select assert */
    XSP_CR_TRANS_INHIBIT_MASK   = 0x00000100,   /**< (0x00000100) Master transaction inhibit */
    XSP_SR_TX_FULL_MASK         = 0x00000008,   /**< (0x00000008) Transmit Reg/FIFO is full */
    XSP_SR_TX_EMPTY_MASK        = 0x00000004,   /**< (0x00000004) Transmit Reg/FIFO is empty */
    XSP_SR_RX_EMPTY_MASK        = 0x00000001,   /**< (0x00000001) Receive Reg/FIFO is empty */
#endif

    /**
     *  These commands are from Table 21: Command Set in Micron Serial NOR Flash Memory data sheet
     *  at https://www.micron.com/~/media/documents/products/data-sheet/nor-flash/serial-nor/mt25q/die-rev-b/mt25q_qlkt_u_512_abb_0.pdf
     */
    FL_QSPI_COMMAND_READ_STATUS_REGISTER            = 0x05, /**< (0x05) Read Status Register command */
    FL_QSPI_COMMAND_WRITE_ENABLE                    = 0x06, /**< (0x06) Write Enable command. */
    FL_QSPI_COMMAND_QUAD_INPUT_FAST_PROGRAM         = 0x32, /**< (0x32) Quad Input Fast Program */
    FL_QSPI_COMMAND_QUAD_OUTPUT_FAST_READ           = 0x6B, /**< (0x6B) 3-byte address mode quad output fast read. */
    FL_QSPI_COMMAND_4_BYTE_QUAD_OUTPUT_FAST_READ    = 0x6C, /**< (0x6C)  */
    FL_QSPI_COMMAND_READ_FLAG_STATUS_REGISTER       = 0x70, /**< (0x70) Read Flag Status Register command */
    FL_QSPI_COMMAND_ENTER_4_BYTE_ADDRESS_MODE       = 0xB7, /**< (0xB7) Enter 4-byte address mode. */
    FL_QSPI_COMMAND_READ_ID                         = 0x9F, /**< (0x9F) Read flash manufacturer id, memory type and capacity (size). */
    FL_QSPI_COMMAND_WRITE_EXTENDED_ADDRESS_REGISTER = 0xC5, /**< (0xC5) Write extended address register. */
    FL_QSPI_COMMAND_BULK_ERASE                      = 0xC7, /**< (0xC7) Bulk Erase command. */

    FL_QSPI_COMMAND_WRITE_MASK                      = 0x8000000000000000ULL,    /**< (0x8000000000000000) TODO ??? */
    FL_QSPI_COMMAND_READ_RESPONSE_MASK              = 0x6000000000000000ULL     /**< (0x6000000000000000) TODO ??? */

} FL_QSPI_Commands;

/***************************************************************************/

FL_Error FL_QSPI_AXI_WaitIsReady(const FL_FlashInfo * pFlashInfo)
{
    uint64_t readData = 0;
    //uint64_t count = 0;
    time_t startTime = time(NULL);

    while (((readData = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_QSPI_COMMAND)) & FL_QSPI_COMMAND_WRITE_MASK) == 0 &&
        (readData & FL_QSPI_COMMAND_READ_RESPONSE_MASK) != 0)
    {
        //printf("RD DEBUG 0x%llx\n",rd);
        //nsleep(1);
        if (time(NULL) - startTime > 10)
        {
            fprintf(stderr, "\n *** ERROR: %s took over 10 seconds\n", __func__);
            exit(EXIT_FAILURE);
        }
        /*if (count++ > 10)
        {
            fprintf(stderr, "\n ********************************************************** ERROR: %s failed!\n", __func__);
            return -999;
        }*/
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_AXI_Read(const FL_FlashInfo * pFlashInfo, uint64_t axireg, uint32_t * pData)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    FL_Write64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_QSPI_COMMAND, FL_QSPI_COMMAND_WRITE_MASK + (axireg << 32));

    FL_ExitOnError(pLogContext, FL_QSPI_AXI_WaitIsReady(pFlashInfo));

    *pData = (uint32_t)FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_QSPI_COMMAND);

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_AXI_Write(const FL_FlashInfo * pFlashInfo, uint64_t axireg, uint64_t data)
{
    FL_Write64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_QSPI_COMMAND, (axireg << 32) + data);

#if 1
    uint64_t readData = 0;
    uint64_t count = 0;
    readData = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_QSPI_COMMAND);
    while ((readData & FL_QSPI_COMMAND_WRITE_MASK) == 0 /*&& (readData & FL_QSPI_COMMAND_READ_RESPONSE_MASK) != 0*/) // TODO: WHAT THE F*CK???
    {
        //printf("WR DEBUG 0x%llx\n", readData);
        readData = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, FL_FPGA_REGISTER_QSPI_COMMAND);
        //nsleep(1);
        if (count++ > 10)
        {
            fprintf(stderr, "\n *** ERROR: %s failed!\n", __func__);
            return -999;
        }
    }
#endif
    //FL_ExitOnError(pLogContext, FL_AXI_WaitIsReady(pRegistersBaseAddress));

    //printf("DEBUG 0x%llx\n",rd64(baseAddr,REG));
    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_FIFO_Empty(const FL_FlashInfo * pFlashInfo, uint32_t * pEmpty)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Read(pFlashInfo, FL_SPI_REGISTER_SR, pEmpty));
    *pEmpty &= 0x4; /* Tx FIFO empty bit 2 */

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_Transfer(const FL_FlashInfo * pFlashInfo, uint8_t * pReadBuffer, uint16_t readBufferSize, const uint8_t * pWriteBuffer, uint16_t writeBufferSize)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint32_t controlRegister; //control register
    uint16_t i;
    uint32_t empty;

    /* Read contents of control register */
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Read(pFlashInfo, FL_SPI_REGISTER_CR, &controlRegister));

    /* Reset FIFOs. */
    controlRegister |= XSP_CR_TXFIFO_RESET_MASK | XSP_CR_RXFIFO_RESET_MASK | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK;

    /* write to control register */
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Write(pFlashInfo, FL_SPI_REGISTER_CR, controlRegister));

    /* Send CMD + dummies. Write bytes from pWriteBuffer to FPGA FIFO */
    for (i = 0; i < writeBufferSize; ++i)
    {
        FL_ExitOnError(pLogContext, FL_QSPI_AXI_Write(pFlashInfo, FL_SPI_REGISTER_DTR, pWriteBuffer[i]));
    }

    /* Select slave. Pull SS (= Slave Select) low. */
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Write(pFlashInfo, FL_SPI_REGISTER_SSR, 0));

    /* Setup controller to start transmit on the physical QSPI lines. */
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Read(pFlashInfo, FL_SPI_REGISTER_CR, &controlRegister));
    controlRegister &= ~XSP_CR_TRANS_INHIBIT_MASK;
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Write(pFlashInfo, FL_SPI_REGISTER_CR, controlRegister));

    /* Read tx&rx fifo empty bits */
    FL_ExitOnError(pLogContext, FL_FIFO_Empty(pFlashInfo, &empty));

    /* Wait for the tx fifo to be empty */
    while ((empty & XSP_SR_TX_EMPTY_MASK) == 0)
    {
        FL_ExitOnError(pLogContext, FL_FIFO_Empty(pFlashInfo, &empty));
    }

    if (pReadBuffer != NULL && readBufferSize > 0)
    {
        uint32_t readData;

        if (writeBufferSize > readBufferSize)
        {
            for (i = 0; i < writeBufferSize - readBufferSize; ++i)
            {
                FL_ExitOnError(pLogContext, FL_QSPI_AXI_Read(pFlashInfo, FL_SPI_REGISTER_DRR, &readData));
            }
        }
        for (i = 0; i < readBufferSize; ++i)
        {
            FL_ExitOnError(pLogContext, FL_QSPI_AXI_Read(pFlashInfo, FL_SPI_REGISTER_DRR, &readData));
            pReadBuffer[i] = (uint8_t)readData;
        }
    }

    /* Release SPI bus */
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Read(pFlashInfo, FL_SPI_REGISTER_CR, &controlRegister));
    controlRegister |= XSP_CR_TRANS_INHIBIT_MASK;
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Write(pFlashInfo, FL_SPI_REGISTER_CR, controlRegister));

    /* Deselect slave. Pull SS (= Slave Select) high. */
    FL_ExitOnError(pLogContext, FL_QSPI_AXI_Write(pFlashInfo, FL_SPI_REGISTER_SSR, 1));

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_ReadStatusRegister(const FL_FlashInfo * pFlashInfo, uint8_t * pStatus)
{
    uint8_t readBuffer[1] = { 0 }, writeBuffer[2];
    FL_Error errorCode;

    writeBuffer[0] = FL_QSPI_COMMAND_READ_STATUS_REGISTER;
    writeBuffer[1] = 0;

    errorCode = FL_QSPI_Transfer(pFlashInfo, readBuffer, sizeof(readBuffer), writeBuffer, sizeof(writeBuffer));

    *pStatus = readBuffer[0];

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_QSPI_ReadFlagStatusRegister(const FL_FlashInfo * pFlashInfo, uint8_t * pFlagStatus)
{
    uint8_t writeBuffer[1];

    writeBuffer[0] = FL_QSPI_COMMAND_READ_FLAG_STATUS_REGISTER;

    return FL_QSPI_Transfer(pFlashInfo, pFlagStatus, sizeof(*pFlagStatus), writeBuffer, sizeof(writeBuffer));
}

/***************************************************************************/

FL_Error FL_QSPI_IsFlashReady(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t busy;

    /* Check the status register using the "READ STATUS REGISTER" command. return 0 for ready, 1 for busy. */
    FL_ExitOnError(pLogContext, FL_QSPI_ReadStatusRegister(pFlashInfo, &busy));
    busy &= 0x1;

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, busy == 1 ? FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY : FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_WaitFlashIsReady(const FL_FlashInfo * pFlashInfo, int32_t timeoutInMilliseconds)
{
#if 1
    FL_Error errorCode;
    //uint64_t waitTimeInNanoseconds;

    //FL_ExitOnError(pLogContext, FL_GetRawTimestamp(pFlashInfo->pErrorContext, &waitTimeInNanoseconds));

    while ((errorCode = FL_QSPI_IsFlashReady(pFlashInfo)) == FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY)
    {
        if (timeoutInMilliseconds >= 0)
        {
            if (--timeoutInMilliseconds > 0)
            {
                usleep(1000); /* wait 1 millisecond */
            }
            else
            {
fprintf(stderr, "\nTimeout in %s: %d\n", __func__, timeoutInMilliseconds);
                return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_TIMEOUT_WAITING_FOR_MICRON_NOR_FLASH_READY,
                    "Waiting for Micron NOR flash to become ready timed out after %d milliseconds", timeoutInMilliseconds);
            }
        }
    }

    return errorCode;
#else
    FL_Error errorCode;
    int count = 0;
    uint64_t startTimeInNanoseconds = 0;
    const uint64_t timeoutInNanoseconds = timeoutInMilliseconds * 1000000UL;

    FL_ExitOnError(pLogContext, FL_GetRawTimestamp(pFlashInfo->pErrorContext, &startTimeInNanoseconds));

    while ((errorCode = FL_QSPI_FlashBusy(pFlashInfo)) == FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY)
    {
        if (timeoutInMilliseconds >= 0)
        {
            if (++count < 10)
            {
                _mm_pause();
            }
            else
            {
                uint64_t currentTimeInNanoseconds = 0;

                FL_ExitOnError(pLogContext, FL_GetRawTimestamp(pFlashInfo->pErrorContext, &currentTimeInNanoseconds));
                count = 0;

                if (currentTimeInNanoseconds - startTimeInNanoseconds >= timeoutInNanoseconds)
                {
fprintf(stderr, "\nTimeout in %s: %d\n", __func__, timeoutInMilliseconds);
                    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_TIMEOUT_WAITING_FOR_MICRON_NOR_FLASH_READY,
                        "Waiting for Micron NOR flash to become ready timed out after %d milliseconds", timeoutInMilliseconds);
                }
            }
        }
    }

    return errorCode;
#endif
}

/***************************************************************************/

FL_Error FL_QSPI_ExecuteCommand(const FL_FlashInfo * pFlashInfo, uint8_t command, uint8_t commandData)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t writeBuffer[2];

    FL_ExitOnError(pLogContext, FL_QSPI_EnableFlashWrite(pFlashInfo));

    writeBuffer[0] = command;
    writeBuffer[1] = commandData;

    return FL_QSPI_Transfer(pFlashInfo, NULL, 0, writeBuffer, sizeof(writeBuffer));
}

/***************************************************************************/

FL_Error FL_QSPI_ReadFlashInfo(const FL_FlashInfo * pFlashInfo, FL_DetailedFlashInfo * pMicronNorFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t readBuffer[3];
    uint8_t writeBuffer[1 + 3]; /* TODO : WHY IS EXTRA 3 REQUIRED? WITH OTHER BUFFER LENGTHS WE GET 0XFF FIRST TIME */

    memset(readBuffer, 0, sizeof(readBuffer));
    memset(writeBuffer, 0, sizeof(writeBuffer));

    FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));

    writeBuffer[0] = FL_QSPI_COMMAND_READ_ID;

    /* write 4 bytes. CMD + dummys for 3 expected bytes (ID,Type,Capacity) */
    FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, readBuffer, sizeof(readBuffer), writeBuffer, sizeof(writeBuffer)));

    if (readBuffer[0] == 0x20)
    {
        uint32_t capacityInMegaBits = 0xFF;
        const char * memoryTypeName = NULL;

        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Flash Manufacturer Id:  %s (0x%02X)\n",
            "Micron NOR", readBuffer[0]);

        if (readBuffer[1] == 0xBA)
        {
            memoryTypeName = "N25Q 3V0";
            pMicronNorFlashInfo->Type = FL_FLASH_TYPE_MICRON_NOR_N25Q_3V;
        }
        else if (readBuffer[1] == 0xBB)
        {
            memoryTypeName = "N25Q 1V8";
            pMicronNorFlashInfo->Type = FL_FLASH_TYPE_MICRON_NOR_N25Q_1_8V;
        }
        else
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_MICRON_NOR_FLASH_MEMORY_TYPE,
                "Invalid Micron flash memory type 0x%02X read via QSPI", readBuffer[1]);
        }

        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Flash Memory Type:      %s (0x%02X)\n",
            memoryTypeName, readBuffer[1]);

        switch (readBuffer[2])
        {
            case 0x18:
                capacityInMegaBits = 128;
                pMicronNorFlashInfo->Size = 16U * 1024U * 1024U;
                break;

            case 0x19:
                capacityInMegaBits = 256;
                pMicronNorFlashInfo->Size = 32U * 1024U * 1024U;
                break;

            case 0x20:
                capacityInMegaBits = 512;
                pMicronNorFlashInfo->Size = 64U * 1024U * 1024U;
                break;

            case 0x21:
                capacityInMegaBits = 1024;
                pMicronNorFlashInfo->Size = 128U * 1024U * 1024U;
                break;

            case 0x22:
                capacityInMegaBits = 2048;
                pMicronNorFlashInfo->Size = 256U * 1024U * 1024U;
                break;

            default:
                return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_MICRON_NOR_FLASH_SIZE, "Invalid flash size/capacity read via QSPI: 0x%02X", readBuffer[2]);
        }

        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Flash Memory Capacity: %u Mbit (0x%02X)\n", capacityInMegaBits, readBuffer[2]);
    }
    else
    {
        FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_FLASH_MANUFACTURER_ID,
            "Invalid flash manufacturer ID 0x%02X, expected id 0x%02X", readBuffer[0], 0x20);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_EnableFlashWrite(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t writeBuffer[1];

    writeBuffer[0] = FL_QSPI_COMMAND_WRITE_ENABLE;

    FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, NULL, 0, writeBuffer, sizeof(writeBuffer)));

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_EnableFlash4ByteMode(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t writeBuffer[1];

    FL_ExitOnError(pLogContext, FL_QSPI_EnableFlashWrite(pFlashInfo));

    writeBuffer[0] = FL_QSPI_COMMAND_ENTER_4_BYTE_ADDRESS_MODE;

    FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, NULL, 0, writeBuffer, sizeof(writeBuffer)));

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_ReadBytes(const FL_FlashInfo * pFlashInfo, uint32_t flashByteAddress, uint8_t readBuffer[], uint32_t readBufferSize, uint32_t * pNumberOfReadBytes)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t writeBuffer[5 + 4 /*dummy cycles*/];

    FL_ExitOnError(pLogContext, FL_QSPI_EnableFlashWrite(pFlashInfo));

    writeBuffer[0] = FL_QSPI_COMMAND_4_BYTE_QUAD_OUTPUT_FAST_READ;
    /* set flassh byt address */
    writeBuffer[1] = (uint8_t)(flashByteAddress >> 24);
    writeBuffer[2] = (uint8_t)(flashByteAddress >> 16);
    writeBuffer[3] = (uint8_t)(flashByteAddress >> 8);
    writeBuffer[4] = (uint8_t)flashByteAddress;
    /* set dummy cycles to zero */
    writeBuffer[5] = 0;
    writeBuffer[6] = 0;
    writeBuffer[7] = 0;
    writeBuffer[8] = 0;

    FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, readBuffer, readBufferSize, writeBuffer, sizeof(writeBuffer)));

    *pNumberOfReadBytes = readBufferSize;

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

#if 0
/* This function has not been implemented, use FL_QPSI_BulkErase instead */
int FL_QSPI_EraseSector(uint32_t *baseAddr, uint32_t Addr)
{
    //int Status;
    int i;
    //int const flash_size=256  ;//00000;//0x1fff0;// 256;//31072;//*4;//262144;

    uint16_t wr_size = 256;
    uint16_t rd_size = 256;

    uint8_t *wr_buf = calloc(wr_size + 10, sizeof(uint8_t));
    uint8_t *rd_buf = calloc(rd_size + 10, sizeof(uint8_t));


    i = Addr;
    //for(i=0;i<flash_size;i=i+256){
    //printf("i=%d\n",i);
    SpiFlash4bytemodeEnable(baseAddr);
    printf("Read status register = 0x%02x \n", QSPI_ReadStatusReg(baseAddr));
    QSPIFlashGetFlagStatus(baseAddr);

    QSPIFlashWriteEnable(baseAddr);
    QSPIFlashGetFlagStatus(baseAddr);
    printf("Read status register = 0x%02x \n", QSPI_ReadStatusReg(baseAddr));

    wr_buf[0] = COMMAND_SECTOR_ERASE;//ReadCmd;
    //BYTE4
    wr_buf[1] = (uint8_t)(i >> 24);
    wr_buf[2] = (uint8_t)(i >> 16);
    wr_buf[3] = (uint8_t)(i >> 8);
    wr_buf[4] = (uint8_t)i;



    /*Status =*/ QSPI_Transfer_write(baseAddr, NULL, 0, wr_buf, 5/*writebytes*/);// + 4 /*dummy cycles*/);
    printf("Read status register = 0x%02x \n", QSPI_ReadStatusReg(baseAddr));

    QSPIFlashGetFlagStatus(baseAddr);

    free(wr_buf);
    free(rd_buf);


    return 0;
}
#endif

/***************************************************************************/

FL_Error FL_QSPI_BulkErase(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t writeBuffer[1];
    FL_Error errorCode = FL_SUCCESS;

    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_ALWAYS, "\rBulk erasing flash ...");

    FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));
    FL_ExitOnError(pLogContext, FL_QSPI_EnableFlashWrite(pFlashInfo));

    writeBuffer[0] = FL_QSPI_COMMAND_BULK_ERASE;

    FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, NULL, 0, writeBuffer, sizeof(writeBuffer)));

    FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 60 * 1000)); // TODO: make timeout a configurable parameter

    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_ALWAYS, "\n");

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_QSPI_ReadFlashToBuffer(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint8_t * buffer, uint32_t bufferLength, bool showProgress)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint64_t previousPercentage = ~0;
    uint32_t flashAddress = startAddress;
    uint32_t totalNumberOfBytesRead = 0;
    uint8_t readBuffer[128];
    uint8_t writeBuffer[128 + 4/*writebytes*/ + 4 /*dummy cycles*/];

    if (startAddress > pFlashInfo->FlashSize)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ARGUMENT,
            "Start address %u cannot be greater than flash size %u", startAddress, pFlashInfo->FlashSize);
    }
    if (bufferLength > pFlashInfo->FlashSize)
    {
        bufferLength = pFlashInfo->FlashSize;
    }
    if (bufferLength % sizeof(readBuffer) != 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ARGUMENT,
            "Buffer length %u has to be multiple of %lu (remainder is %lu but should be 0)",
            bufferLength, sizeof(readBuffer), bufferLength % sizeof(readBuffer));
    }

    memset(buffer, 0xFF, bufferLength);

    memset(writeBuffer, 0, sizeof(writeBuffer));
    writeBuffer[0] = FL_QSPI_COMMAND_QUAD_OUTPUT_FAST_READ;

    while (true)
    {
        uint8_t segment;
        int i;

        FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));

        /* 3-byte address mode, compute segment: */
        segment = (uint8_t)(flashAddress / (16 * 1024 * 1024));
        FL_ExitOnError(pLogContext, FL_QSPI_ExecuteCommand(pFlashInfo, FL_QSPI_COMMAND_WRITE_EXTENDED_ADDRESS_REGISTER, segment));

        writeBuffer[1] = (uint8_t)(flashAddress >> 16);
        writeBuffer[2] = (uint8_t)(flashAddress >> 8);
        writeBuffer[3] = (uint8_t)flashAddress;

        FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));

        FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, readBuffer, sizeof(readBuffer), writeBuffer, sizeof(writeBuffer)));

        for (i = 0; i < sizeof(readBuffer); ++i)
        {
            buffer[totalNumberOfBytesRead + i] = readBuffer[i];
        }

        FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));

        flashAddress += sizeof(readBuffer);
        totalNumberOfBytesRead += sizeof(readBuffer);

        if (showProgress)
        {
            uint64_t percentage = 100UL * flashAddress;
            percentage /= bufferLength;
            if (percentage != previousPercentage)
            {
                FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\rReading flash %lu%%, %u of %d bytes from address 0x%X   ",
                    percentage, totalNumberOfBytesRead, bufferLength, flashAddress);
                previousPercentage = percentage;
            }
        }

        if (totalNumberOfBytesRead >= bufferLength)
        {
            break;
        }
    }

    // Restore selected flash segment to zero
    FL_ExitOnError(pLogContext, FL_QSPI_ExecuteCommand(pFlashInfo, FL_QSPI_COMMAND_WRITE_EXTENDED_ADDRESS_REGISTER, 0));

    if (showProgress)
    {
        FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\n");
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_ReadFlashToFile(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName, FILE * pFile)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint8_t * fileBuffer = calloc(length + 128, sizeof(uint8_t));

    FL_ExitOnError(pLogContext, FL_QSPI_ReadFlashToBuffer(pFlashInfo, startAddress, fileBuffer, length, true));

    size_t numberOfItemsWritten = fwrite(fileBuffer, 1, length, pFile);
    if (numberOfItemsWritten != length)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_WRITE_TO_FLASH_IMAGE_FILE,
            "Failed to write %u bytes to file '%s', errno %d\n", length, fileName, errno);
    }

    free(fileBuffer);

    FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "Wrote %u bytes to file '%s'", length, fileName);

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_ReadFlashToFileName(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName)
{
    FL_Error errorCode;

    FILE * pFile = fopen(fileName, "wb");
    if (pFile == NULL)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE,
            "Failed to open file '%s' for writing, errno %d\n", fileName, errno);
    }

    errorCode = FL_QSPI_ReadFlashToFile(pFlashInfo, startAddress, length, fileName, pFile);

    if (fclose(pFile) != 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close file '%s', errno %d\n", fileName, errno);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

static FL_Error FL_QSPI_CommonFileHandling(const FL_FlashInfo * pFlashInfo, const char * fileName, FILE * pFile, long * pFileSize)
{
    long fileSize;
    long fileOffset;

    /* Remember file initial position or offset from beginning of file */
    fileOffset = ftell(pFile);
    if (fileOffset == -1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_TELL_POSITION_IN_IMAGE_FILE,
            "Failed to tell initial position in flash image file '%s', errno %d", fileName, errno);
    }

    /* Find file size by seeking to end of file and reading position. */
    if (fseek(pFile, 0, SEEK_END) != 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_SEEK_IN_OF_IMAGE_FILE,
            "Failed to seek to end of flash image file '%s', errno %d\n", fileName, errno);
    }
    fileSize = ftell(pFile);
    if (fileSize == -1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_TELL_POSITION_IN_IMAGE_FILE,
            "Failed to tell flash image file '%s' size, errno %d", fileName, errno);
    }

    /* Restore initial file position */
    if (fseek(pFile, fileOffset, SEEK_SET) != 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_TELL_POSITION_IN_IMAGE_FILE,
            "Failed to tell seek to start of flash image file '%s', errno %d", fileName, errno);
    }

    /* Reduce file size with initial file offset */
    fileSize -= fileOffset;
    if (fileSize <= 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_ARGUMENT, "File size %ld cannot be negative or zero", fileSize);
    }

    *pFileSize = fileSize;

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_ProgramFile(const FL_FlashInfo * pFlashInfo, const char * fileName, FILE * pFile, bool showProgress)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    long fileSize;
    uint64_t previousPercentage = ~0;
    uint8_t writeBuffer[128 + 10];
    const uint32_t flashStartAddress = 0;
    uint32_t numberOfBytesWritten = 0;
    uint32_t flashAddress = flashStartAddress;

    FL_ExitOnError(pLogContext, FL_QSPI_CommonFileHandling(pFlashInfo, fileName, pFile, &fileSize));

    memset(writeBuffer, 0, sizeof(writeBuffer));
    writeBuffer[0] = FL_QSPI_COMMAND_QUAD_INPUT_FAST_PROGRAM;

    while (true)
    {
        uint8_t fileReadBuffer[128];
        int eof;
        uint8_t segment;
        size_t numberOfBytesRead;

        numberOfBytesRead = fread(fileReadBuffer, 1, sizeof(fileReadBuffer), pFile);
        if (ferror(pFile) != 0)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_READ_FROM_FLASH_IMAGE_FILE,
                "Failed to read from flash image file '%s', errno %d", fileName, errno);
        }
        eof = feof(pFile);

        while (QSPI_FlashBusy(pFlashInfo));
        //while (FL_QSPI_FlashBusy(pFlashInfo) == FL_ERROR_MICRON_NOR_FLASH_IS_NOT_READY);
        //FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashBusy(pFlashInfo, 1000));

        FL_ExitOnError(pLogContext, FL_QSPI_EnableFlashWrite(pFlashInfo));

        /* Compute 3-byte address mode segment: */
        segment = (uint8_t)(flashAddress / (16 * 1024 * 1024));
        FL_ExitOnError(pLogContext, FL_QSPI_ExecuteCommand(pFlashInfo, FL_QSPI_COMMAND_WRITE_EXTENDED_ADDRESS_REGISTER, segment));

        FL_ExitOnError(pLogContext, FL_QSPI_EnableFlashWrite(pFlashInfo));

        writeBuffer[1] = (uint8_t)(flashAddress >> 16);
        writeBuffer[2] = (uint8_t)(flashAddress >> 8);
        writeBuffer[3] = (uint8_t)flashAddress;

        memcpy(&writeBuffer[4], fileReadBuffer, numberOfBytesRead);

        FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));

        FL_ExitOnError(pLogContext, FL_QSPI_Transfer(pFlashInfo, NULL, 0, writeBuffer, 4 + numberOfBytesRead));

        flashAddress += numberOfBytesRead;
        numberOfBytesWritten += numberOfBytesRead;

        uint64_t percentage = 100ULL * numberOfBytesWritten;
        percentage /= fileSize;
        if (percentage != previousPercentage || eof != 0)
        {
            FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS,
                "\rWriting flash %lu%%, %u of %ld bytes   ", percentage, numberOfBytesWritten, fileSize);
            previousPercentage = percentage;
        }

        if (eof != 0)
        {
            break;
        }
    }

    // Restore selected flash segment to zero
    FL_ExitOnError(pLogContext, FL_QSPI_ExecuteCommand(pFlashInfo, FL_QSPI_COMMAND_WRITE_EXTENDED_ADDRESS_REGISTER, 0));

    FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\n");

    FL_ExitOnError(pLogContext, FL_QSPI_WaitFlashIsReady(pFlashInfo, 1000));

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_QSPI_ProgramFileName(const FL_FlashInfo * pFlashInfo, const char * fileName, bool showProgress)
{
    FL_Error errorCode;

    FILE * pFile = fopen(fileName, "rb");
    if (pFile == NULL)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE,
            "Failed to open file '%s' for writing, errno %d\n", fileName, errno);
    }

    errorCode = FL_QSPI_ProgramFile(pFlashInfo, fileName, pFile, showProgress);

    if (fclose(pFile) != 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close file '%s', errno %d\n", fileName, errno);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_QSPI_VerifyFile(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName, FILE * pFile)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    long fileSize;
    uint64_t previousPercentage = ~0;
    uint32_t flashAddress = startAddress;
    FL_Error errorCode = FL_SUCCESS;
    uint32_t numberOfBytesVerified = 0;

    FL_ExitOnError(pLogContext, FL_QSPI_CommonFileHandling(pFlashInfo, fileName, pFile, &fileSize));

    if (length != 0)
    {
        fileSize = length;
    }

    while (true)
    {
        size_t i;
        uint8_t flashReadBuffer[128];
        uint8_t fileReadBuffer[sizeof(flashReadBuffer)];
        uint64_t percentage;
        int eof;
        size_t numberOfBytesRead;

        numberOfBytesRead = fread(fileReadBuffer, 1, sizeof(fileReadBuffer), pFile);
        if (ferror(pFile) != 0)
        {
            errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_READ_FROM_FLASH_IMAGE_FILE,
                "Failed to read data from flash image file '%s', errno %d", fileName, errno);
            goto exit;
        }
        eof = feof(pFile);

        FL_ExitOnError(pLogContext, FL_QSPI_ReadFlashToBuffer(pFlashInfo, flashAddress, flashReadBuffer, sizeof(flashReadBuffer), false));

        /* Compare data read from file to data read from flash. */
        for (i = 0; i < numberOfBytesRead; ++i)
        {
            if (flashReadBuffer[i] != (uint8_t)fileReadBuffer[i])
            {
                FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\nFlash address = 0x%X\n", flashAddress);
                FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "flash:\n");
                FL_HexDump(printf, flashReadBuffer, numberOfBytesRead, 16, flashAddress - (ssize_t)flashReadBuffer);
                FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "file:\n");
                FL_HexDump(printf, fileReadBuffer, numberOfBytesRead, 16, flashAddress - (ssize_t)fileReadBuffer);
                errorCode = FL_ERROR_FLASH_VERIFY_FAILED;
                goto exit;
            }
        }

        flashAddress += numberOfBytesRead;
        numberOfBytesVerified += numberOfBytesRead;

        /* Show progress percentage */
        percentage = 100ULL * numberOfBytesVerified;
        percentage /= fileSize;
        if (percentage != previousPercentage || eof != 0)
        {
            FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\rVerifying flash %lu%%, %u of %ld bytes   ",
                percentage, numberOfBytesVerified, fileSize);
            previousPercentage = percentage;
        }

        if (eof != 0 || numberOfBytesVerified >= fileSize)
        {
            break; /* End of file, verify is finished */
        }
    }

    FL_Log(pLogContext, FL_LOG_LEVEL_ALWAYS, "\n");

exit:

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_QSPI_VerifyFileName(const FL_FlashInfo * pFlashInfo, uint32_t startAddress, uint32_t length, const char * fileName)
{
    FL_Error errorCode;
    FILE * pFile;

    pFile = fopen(fileName, "rb");
    if (pFile == NULL)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Failed to open flash image file '%s', errno %d\n", fileName, errno);
    }

    errorCode = FL_QSPI_VerifyFile(pFlashInfo, startAddress, length, fileName, pFile);

    if (fclose(pFile) != 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close file '%s', errno %d\n", fileName, errno);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

#define CASE_NAME(registerValue, shortName)    case registerValue: return fullName ? #registerValue : (shortName)

/***************************************************************************/

const char * FL_GetVendorIdName(FL_ErrorContext * pErrorContext, FL_PCIeVendorId pcieVendorId, bool fullName)
{
    switch (pcieVendorId)
    {
        CASE_NAME(VENDOR_ID_XILINX, "XiLinx");
        CASE_NAME(VENDOR_ID_ANRITSU, "Anritsu");
       /*CASE_NAME(VENDOR_ID_FIBERBLAZE); duplicate value! */
        CASE_NAME(VENDOR_ID_SILICOM_DENMARK, "Silicom Denmark");

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for PCIe vendor id %u (0x%X) not implemented", __func__, pcieVendorId, pcieVendorId);
    }

    return NULL;
}

/***************************************************************************/

const char * FL_GetDeviceIdName(FL_ErrorContext * pErrorContext, FL_PCIeDeviceId pcieDeviceId, bool fullName)
{
    switch (pcieDeviceId)
    {
        CASE_NAME(DEVICE_ID_UNKNOWN, "Unknown");
        CASE_NAME(DEVICE_ID_ANCONA_AUGSBURG, "Ancona Augsburg");
        CASE_NAME(DEVICE_ID_BERLIN, "Berlin");
        CASE_NAME(DEVICE_ID_CHEMNITZ, "Chemnitz");
        CASE_NAME(DEVICE_ID_DEMO, "Demo");
        CASE_NAME(DEVICE_ID_ESSEN, "Essen");
        CASE_NAME(DEVICE_ID_FB2022_AUGSBURG, "2022 Augsburg");
        CASE_NAME(DEVICE_ID_FBSLAVE, "Slave");
        CASE_NAME(DEVICE_ID_GENOA_AUGSBURG, "Genoa Augsburg");
        CASE_NAME(DEVICE_ID_HERCULANEUM_AUGSBURG, "Herculaneum Augsburg");
        CASE_NAME(DEVICE_ID_LATINA_AUGSBURG, "Latina Augsburg");
        CASE_NAME(DEVICE_ID_LATINA_LEONBERG, "Latina Leonberg");
        CASE_NAME(DEVICE_ID_LIVIGNO_AUGSBURG, "Livigno Augsburg");
        CASE_NAME(DEVICE_ID_LIVIGNO_AUGSBURG_4X1, "Livigno Augsburg 4x1");
        CASE_NAME(DEVICE_ID_LIVIGNO_FULDA_V7330, "Livigno Fulda V7330");
        CASE_NAME(DEVICE_ID_LIVIGNO_LEONBERG_V7330, "Livigno Leonberg V7330");
        CASE_NAME(DEVICE_ID_LIVIGNO_LEONBERG_V7690, "Livigno Leonberg V7690");
        CASE_NAME(DEVICE_ID_LIVORNO_FULDA, "Fulda");
        CASE_NAME(DEVICE_ID_LIVORNO_AUGSBURG, "Livorno Augsburg");
        CASE_NAME(DEVICE_ID_LIVORNO_AUGSBURG_40, "Livorno Augsburg 40");
        CASE_NAME(DEVICE_ID_LIVORNO_JENA, "Livorno Jena");
        CASE_NAME(DEVICE_ID_LIVORNO_JENA_40, "Livorno Jena 40");
        CASE_NAME(DEVICE_ID_LIVORNO_LEONBERG, "Livorno Leonberg");
        CASE_NAME(DEVICE_ID_LIVORNO_ODESSA_CRYPTO_2X40, "Livorno Odessa Crypto 2x40G");
        CASE_NAME(DEVICE_ID_LIVORNO_ODESSA_CRYPTO_2X10, "Livorno Odessa Crypto 2x10G");
        CASE_NAME(DEVICE_ID_LIVORNO_ODESSA_REGEX_4X10, "Livorno Odessa RegEx 4x10G");
        CASE_NAME(DEVICE_ID_LUCCA_AUGSBURG, "Lucca Augsburg");
        CASE_NAME(DEVICE_ID_LUCCA_AUGSBURG_40, "Lucca Augsburg 40");
        CASE_NAME(DEVICE_ID_LUCCA_LEONBERG_V7330, "Lucca Leonberg V7330");
        CASE_NAME(DEVICE_ID_LUCCA_LEONBERG_V7690, "Lucca Leonberg V7690");
        CASE_NAME(DEVICE_ID_LU50, "U50");
        CASE_NAME(DEVICE_ID_LUXG, "LUXG");
        CASE_NAME(DEVICE_ID_MANGO_AUGSBURG_8X10, "Mango Augsburg 8x10");
        CASE_NAME(DEVICE_ID_MANGO_AUGSBURG_2X25, "Mango Augsburg 2x25");
        CASE_NAME(DEVICE_ID_MANGO_AUGSBURG_40, "Mango Augsburg 40");
        CASE_NAME(DEVICE_ID_MANGO_AUGSBURG_100, "Mango Augsburg 100");
        CASE_NAME(DEVICE_ID_MANGO_AUGSBURG_16X10, "Mango Augsburg 16x10");
        CASE_NAME(DEVICE_ID_MANGO_LEONBERG_VU125, "Mango Leonberg VU125");
        CASE_NAME(DEVICE_ID_MANGO_LEONBERG_VU9P, "Mango Leonberg VU9P");
        CASE_NAME(DEVICE_ID_MANGO_03_LEONBERG_VU9P_ES1, "Mango 3 Leonberg VU9P ES1");
        CASE_NAME(DEVICE_ID_MANGO_04_AUGSBURG_2X100, "Mango 4 Augsburg 2x100");
        CASE_NAME(DEVICE_ID_MANGO_04_AUGSBURG_2X40, "Mango 4 Augsburg 2x40");
        CASE_NAME(DEVICE_ID_MANGO_04_AUGSBURG_2X25, "Mango 4 Augsburg 2x25");
        CASE_NAME(DEVICE_ID_MANGO_04_AUGSBURG_16X10, "Mango 4 Augsburg 16x10");
        CASE_NAME(DEVICE_ID_MANGO_04_AUGSBURG_8X10, "Mango 4 Augsburg 8x10");
        CASE_NAME(DEVICE_ID_MANGO_04_AUGSBURG_2X100_VU7P, "Mango 4 Augsburg 2x100 VU7P");
        CASE_NAME(DEVICE_ID_MANGO_04_LEONBERG_VU9P, "Mango 4 Leonberg VU9P");
        CASE_NAME(DEVICE_ID_MANGO_ODESSA_CRYPTO_2X40, "Mango 4 Odessa Crypto 2x40G");
        CASE_NAME(DEVICE_ID_MANGO_WOLFSBURG_4X100G, "Mango Wolfsburg 4x100G");
        CASE_NAME(DEVICE_ID_MILAN_AUGSBURG, "Milan Augsburg");
        CASE_NAME(DEVICE_ID_MONZA_AUGSBURG, "Monza Augsburg");
        CASE_NAME(DEVICE_ID_PADUA_AUGSBURG_8X10, "Padua Augsburg 8x10");
        CASE_NAME(DEVICE_ID_PADUA_AUGSBURG_2X25, "Padua Augsburg 2x25");
        CASE_NAME(DEVICE_ID_PADUA_AUGSBURG_40, "Padua Augsburg 40");
        CASE_NAME(DEVICE_ID_PADUA_AUGSBURG_100, "Padua Augsburg 100");
        CASE_NAME(DEVICE_ID_PADUA_LEONBERG_VU125, "Padua Leonberg VU125");
        CASE_NAME(DEVICE_ID_SAVONA_AUGSBURG_8X10, "Savona Augsburg 8x10");
        CASE_NAME(DEVICE_ID_SAVONA_AUGSBURG_2X25, "Savona Augsburg 2x25");
        CASE_NAME(DEVICE_ID_SAVONA_AUGSBURG_2X40, "Savona Augsburg 2x40");
        CASE_NAME(DEVICE_ID_SAVONA_AUGSBURG_2X100, "Savona Augsburg 2x100");
        CASE_NAME(DEVICE_ID_SAVONA_WOLFSBURG_2X100G, "Savona Wolfsburg 2x100G");
        CASE_NAME(DEVICE_ID_SILIC100_AUGSBURG, "Silic100 Augsburg");
        CASE_NAME(DEVICE_ID_TEST_FPGA, "Test FPGA");
        CASE_NAME(DEVICE_ID_TEST_FPGA_SLAVE, "Test FPGA Slave");
        CASE_NAME(DEVICE_ID_TIVOLI_AUGSBURG_2X40, "Tivoli Augsburg 2x40G");
        CASE_NAME(DEVICE_ID_TIVOLI_WOLFSBURG_2X100G, "Tivoli Wolfsburg 2x100G");
        CASE_NAME(DEVICE_ID_XILINX, "XiLinx");

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for PCIe device id %u (0x%X) not implemented", __func__, pcieDeviceId, pcieDeviceId);
    }

    return NULL;
}

/***************************************************************************/

const char * FL_GetFpgaTypeName(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType, bool fullName)
{
    switch (fpgaType)
    {
        CASE_NAME(FPGA_TYPE_2010, "2010");
        CASE_NAME(FPGA_TYPE_2015, "2015");
        CASE_NAME(FPGA_TYPE_2022, "2022");
        CASE_NAME(FPGA_TYPE_ANCONA_AUGSBURG, "Ancona Augsburg");
        CASE_NAME(FPGA_TYPE_ANCONA_CHEMNITZ, "Ancona Chemnitz");
        CASE_NAME(FPGA_TYPE_ANCONA_CLOCK_CALIBRATION, "Ancona Clock Calibration");
        CASE_NAME(FPGA_TYPE_ANCONA_TEST, "Ancona Test");
        CASE_NAME(FPGA_TYPE_ATHEN_TEST, "Athen Test");
        CASE_NAME(FPGA_TYPE_CARDIFF_TEST, "Cardiff Test");
        CASE_NAME(FPGA_TYPE_CARDIFF_TEST_4X40G, "Cardiff Test 4x40G");
        CASE_NAME(FPGA_TYPE_ERCOLANO_DEMO, "Ercolano Demo");
        CASE_NAME(FPGA_TYPE_ERCOLANO_TEST, "Ercolano Test");
        CASE_NAME(FPGA_TYPE_ESSEN, "Essen");
        CASE_NAME(FPGA_TYPE_FIRENZE_CHEMNITZ, "Firenze Chemnitz");
        CASE_NAME(FPGA_TYPE_FIRENZE_TEST, "Firenze Test");
        CASE_NAME(FPGA_TYPE_GENOA_AUGSBURG, "Genoa Augsburg");
        CASE_NAME(FPGA_TYPE_GENOA_TEST, "Genoa Test");
        CASE_NAME(FPGA_TYPE_HERCULANEUM_AUGSBURG, "Herculaneum Augsburg");
        CASE_NAME(FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION, "Herculaneum Clock Calibration");
        CASE_NAME(FPGA_TYPE_HERCULANEUM_TEST, "Herculaneum Test");
        CASE_NAME(FPGA_TYPE_LATINA_AUGSBURG, "Latina Augsburg");
        CASE_NAME(FPGA_TYPE_LATINA_LEONBERG, "Latina Leonberg");
        CASE_NAME(FPGA_TYPE_LATINA_TEST, "Latina Test");
        CASE_NAME(FPGA_TYPE_LIVIGNO_AUGSBURG, "Livigno Augsburg");
        CASE_NAME(FPGA_TYPE_LIVIGNO_AUGSBURG_4X1, "Livigno Augsburg 4x1");
        CASE_NAME(FPGA_TYPE_LIVIGNO_FULDA_V7330, "Livigno Fulda V7330");
        CASE_NAME(FPGA_TYPE_LIVIGNO_LEONBERG_V7330, "Livigno Leonberg V7330");
        CASE_NAME(FPGA_TYPE_LIVIGNO_LEONBERG_V7690, "Livigno Leonberg V7690");
        CASE_NAME(FPGA_TYPE_LIVIGNO_TEST, "Livigno Test");
        CASE_NAME(FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G, "Livorno Odessa Crypto 2x10G");
        CASE_NAME(FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G, "Livorno Odessa RegEx 4x10G");
        CASE_NAME(FPGA_TYPE_LIVORNO_ODESSA_40, "Livorno Odessa 40");
        CASE_NAME(FPGA_TYPE_LIVORNO_AUGSBURG, "Livorno Augsburg");
        CASE_NAME(FPGA_TYPE_LIVORNO_AUGSBURG_40, "Livorno Augsburg 40");
        CASE_NAME(FPGA_TYPE_LIVORNO_FULDA, "Livorno Fulda");
        CASE_NAME(FPGA_TYPE_LIVORNO_LEONBERG, "Livorno Leonberg");
        CASE_NAME(FPGA_TYPE_LIVORNO_TEST, "Livorno Test");
        CASE_NAME(FPGA_TYPE_LIVORNO_TEST_40, "Livorno Test 40");
        CASE_NAME(FPGA_TYPE_LUCCA_AUGSBURG, "Lucca Augsburg");
        CASE_NAME(FPGA_TYPE_LUCCA_AUGSBURG_40, "Lucca Augsburg 40");
        CASE_NAME(FPGA_TYPE_LUCCA_LEONBERG_V7330, "Lucca Leonberg V7330");
        CASE_NAME(FPGA_TYPE_LUCCA_LEONBERG_V7690, "Lucca Leonberg V7690");
        CASE_NAME(FPGA_TYPE_LUCCA_TEST, "Lucca Test");
        CASE_NAME(FPGA_TYPE_LUXG_PROBE, "LUXG Probe");
        CASE_NAME(FPGA_TYPE_LUXG_TEST, "LUXG Test");
        CASE_NAME(FPGA_TYPE_MANGO_ODESSA_40, "Mango Odessa 40");
        CASE_NAME(FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1, "Mango 3 Leonberg VU9P ES1");
        CASE_NAME(FPGA_TYPE_MANGO_04_LEONBERG_VU9P, "Mango 4 Leonberg VU9P");
        CASE_NAME(FPGA_TYPE_MANGO_04_AUGSBURG_8X10, "Mango 4 Augsburg_8x10");
        CASE_NAME(FPGA_TYPE_MANGO_04_AUGSBURG_16X10, "Mango 4 Augsburg 16x10");
        CASE_NAME(FPGA_TYPE_MANGO_04_AUGSBURG_2X25, "Mango 4 Augsburg 2x25");
        CASE_NAME(FPGA_TYPE_MANGO_04_AUGSBURG_2X40, "Mango 4 Augsburg 2x40");
        CASE_NAME(FPGA_TYPE_MANGO_04_AUGSBURG_2X100, "Mango 4 Augsburg 2x100");
        CASE_NAME(FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P, "Mango 4 Augsburg 2x100 VU7P");
        CASE_NAME(FPGA_TYPE_MANGO_AUGSBURG_8X10, "Mango Augsburg 8x10");
        CASE_NAME(FPGA_TYPE_MANGO_AUGSBURG_16X10, "Mango Augsburg 16x10");
        CASE_NAME(FPGA_TYPE_MANGO_AUGSBURG_2X25, "Mango Augsburg 2x25");
        CASE_NAME(FPGA_TYPE_MANGO_AUGSBURG_40, "Mango Augsburg 40");
        CASE_NAME(FPGA_TYPE_MANGO_AUGSBURG_100, "Mango Augsburg 100");
        CASE_NAME(FPGA_TYPE_MANGO_LEONBERG_VU125, "Mango Leonberg VU125");
        CASE_NAME(FPGA_TYPE_MANGO_LEONBERG_VU9P, "Mango Leonberg VU9P");
        CASE_NAME(FPGA_TYPE_MANGO_TEST, "Mango Test");
        CASE_NAME(FPGA_TYPE_MANGO_TEST_QDR, "Mango Test QDR");
        CASE_NAME(FPGA_TYPE_MANGO_WOLFSBURG_4X100G, "Mango Wolfsburg 4x100G");
        CASE_NAME(FPGA_TYPE_MILAN_AUGSBURG, "Milan Augsburg");
        CASE_NAME(FPGA_TYPE_MILAN_TEST, "Milan Test");
        CASE_NAME(FPGA_TYPE_MONZA_TEST, "Monza Test");
        CASE_NAME(FPGA_TYPE_PADUA_AUGSBURG_100, "Padua Augsburg 100");
        CASE_NAME(FPGA_TYPE_PADUA_AUGSBURG_2X25, "Padua Augsburg 2x25");
        CASE_NAME(FPGA_TYPE_PADUA_AUGSBURG_40, "Padua Augsburg 40");
        CASE_NAME(FPGA_TYPE_PADUA_AUGSBURG_8X10, "Padua Augsburg 8x10");
        CASE_NAME(FPGA_TYPE_PADUA_LEONBERG_VU125, "Padua Leonberg VU125");
        CASE_NAME(FPGA_TYPE_PADUA_TEST, "Padua Test");
        CASE_NAME(FPGA_TYPE_PISA_TEST, "Pisa Test");
        CASE_NAME(FPGA_TYPE_RIMINI_TEST, "Rimini Test");
        CASE_NAME(FPGA_TYPE_TORINO_TEST, "Torino Test");
        CASE_NAME(FPGA_TYPE_MONZA_AUGSBURG, "Monza Augsburg");
        CASE_NAME(FPGA_TYPE_SILIC100_AUGSBURG, "Silic100 Augsburg");
        CASE_NAME(FPGA_TYPE_SILIC100_TEST, "Silic100 Test");
        CASE_NAME(FPGA_TYPE_MARSALA_TEST, "Marsala Test");
        CASE_NAME(FPGA_TYPE_BERGAMO_TEST, "Bergamo Test");
        CASE_NAME(FPGA_TYPE_COMO_TEST, "Como Test");
        CASE_NAME(FPGA_TYPE_UNKNOWN, "Unknown");
        CASE_NAME(FPGA_TYPE_SAVONA_AUGSBURG_8X10, "Savona Augsburg 8x10");
        CASE_NAME(FPGA_TYPE_SAVONA_AUGSBURG_2X25, "Savona Augsburg 2x25");
        CASE_NAME(FPGA_TYPE_SAVONA_AUGSBURG_2X40, "Savona Augsburg 2x40");
        CASE_NAME(FPGA_TYPE_SAVONA_AUGSBURG_2X100, "Savona Augsburg 2x100");
        CASE_NAME(FPGA_TYPE_SAVONA_TEST, "Savona Test");
        CASE_NAME(FPGA_TYPE_SAVONA_WOLFSBURG_2X100G, "Savona Wolfsburg 2x100G");
        CASE_NAME(FPGA_TYPE_TIVOLI_AUGSBURG_2X40, "Tivoli Augsburg 2x40");
        CASE_NAME(FPGA_TYPE_TIVOLI_TEST, "Tivoli Test");
        CASE_NAME(FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G, "Tivoli Wolfsburg 2x100G");
        CASE_NAME(FPGA_TYPE_VCU1525_TEST, "Xilinx VCU1525 Test");


        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return NULL;
}

/***************************************************************************/

const char * FL_GetFpgaModelName(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel, bool fullName)
{
    switch (fpgaModel)
    {
        CASE_NAME(FPGA_MODEL_V5_LX110, "_V5_LX110");
        CASE_NAME(FPGA_MODEL_V5_LX155, "V5_LX155");
        CASE_NAME(FPGA_MODEL_V6_LX240, "V6_LX240");
        CASE_NAME(FPGA_MODEL_V6_LX130, "V6_LX130");
        CASE_NAME(FPGA_MODEL_V6_SX315, "V6_SX315");
        CASE_NAME(FPGA_MODEL_V7_X690, "V7_X690");
        CASE_NAME(FPGA_MODEL_V7_HXT580, "V7_HXT580");
        CASE_NAME(FPGA_MODEL_V7_X330, "V7_X330");
        CASE_NAME(FPGA_MODEL_V7_LX690_FFG1927, "V7_LX690_FFG1927");
        CASE_NAME(FPGA_MODEL_KU_X115, "KU_X115");
        CASE_NAME(FPGA_MODEL_US_V095_FFVE1924, "US_V095_FFVE1924");
        CASE_NAME(FPGA_MODEL_US_V080, "US_V080");
        CASE_NAME(FPGA_MODEL_US_V125, "US_V125");
        CASE_NAME(FPGA_MODEL_US_V190, "US_V190");
        CASE_NAME(FPGA_MODEL_US_V9P_ES1, "US_V9P_ES1");
        CASE_NAME(FPGA_MODEL_US_V9P_I, "US_V9P_I");
        CASE_NAME(FPGA_MODEL_US_V7P, "US_V7P");
        CASE_NAME(FPGA_MODEL_STRATIX_5, "STRATIX_5");
        CASE_NAME(FPGA_MODEL_ZYNQ_7000_45, "ZYNQ_7000_45");
        CASE_NAME(FPGA_MODEL_US_KU15P, "US_KU15P");
        CASE_NAME(FPGA_MODEL_US_V9P_SG3, "_US_V9P_SG3");
        CASE_NAME(FPGA_MODEL_US_V9P_D, "US_V9P_D");
        CASE_NAME(FPGA_MODEL_UNKNOWN, "Unknown");

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA model %u (0x%X) not implemented", __func__, fpgaModel, fpgaModel);
    }

    return NULL;
}

/***************************************************************************/

const char * FL_GetCardTypeName(FL_ErrorContext * pErrorContext, FL_CardType cardType, bool fullName)
{
    switch (cardType)
    {
        CASE_NAME(FL_CARD_TYPE_UNKNOWN, "UNKNOWN");
        CASE_NAME(FL_CARD_TYPE_RAW, "Raw");
        CASE_NAME(FL_CARD_TYPE_20XX, "20XX");
        CASE_NAME(FL_CARD_TYPE_ANCONA, "Ancona");
        CASE_NAME(FL_CARD_TYPE_ATHEN, "Athen");
        CASE_NAME(FL_CARD_TYPE_BERGAMO, "Bergamo");
        CASE_NAME(FL_CARD_TYPE_CARDIFF, "Cardiff");
        CASE_NAME(FL_CARD_TYPE_COMO, "Como");
        CASE_NAME(FL_CARD_TYPE_ERCOLANO, "Ercolano");
        CASE_NAME(FL_CARD_TYPE_ESSEN, "Essen");
        CASE_NAME(FL_CARD_TYPE_FIRENZE, "Firenze");
        CASE_NAME(FL_CARD_TYPE_GENOA, "Genoa");
        CASE_NAME(FL_CARD_TYPE_HERCULANEUM, "Herculaneum");
        CASE_NAME(FL_CARD_TYPE_LATINA, "Latina");
        CASE_NAME(FL_CARD_TYPE_LIVIGNO, "Livigno");
        CASE_NAME(FL_CARD_TYPE_LIVORNO, "Livorno");
        CASE_NAME(FL_CARD_TYPE_LUCCA, "Lucca");
        CASE_NAME(FL_CARD_TYPE_LUXG, "LUXG");
        CASE_NAME(FL_CARD_TYPE_MANGO, "Mango");
        CASE_NAME(FL_CARD_TYPE_MARSALA, "Marsala");
        CASE_NAME(FL_CARD_TYPE_MILAN, "Milan");
        CASE_NAME(FL_CARD_TYPE_MONZA, "Monza");
        CASE_NAME(FL_CARD_TYPE_PADUA, "Padua");
        CASE_NAME(FL_CARD_TYPE_PISA, "Pisa");
        CASE_NAME(FL_CARD_TYPE_RIMINI, "Rimini");
        CASE_NAME(FL_CARD_TYPE_SAVONA, "Savona");
        CASE_NAME(FL_CARD_TYPE_SILIC100, "Silic100");
        CASE_NAME(FL_CARD_TYPE_TIVOLI, "Tivoli");
        CASE_NAME(FL_CARD_TYPE_TORINO, "Torino");

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for card type %u (0x%X) not implemented", __func__, cardType, cardType);
    }

    return NULL;
}

/***************************************************************************/

const char * FL_GetPCIeBootModeName(FL_ErrorContext * pErrorContext, FL_PCIeBootMode pcieBootMode, bool fullName)
{
    switch (pcieBootMode) /* mode to be set match index + 1 */
    {
        CASE_NAME(FL_PCIE_BOOT_MODE_SINGLE, "single");
        CASE_NAME(FL_PCIE_BOOT_MODE_SIMULTANEOUS, "simultaneous");
        CASE_NAME(FL_PCIE_BOOT_MODE_SEQUENTIAL, "sequential");
        CASE_NAME(FL_PCIE_BOOT_MODE_NONE, "none");
            break;
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_INVALID_PCIE_BOOT_MODE, "Invalid PCIe boot mode %u", pcieBootMode);
    }

    return NULL;
}

/***************************************************************************/

FL_FlashType FL_GetFlashTypeFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    /* SF2 = SmartFusion2 */

    switch (fpgaType)                                   /* MCU, MCU's Flash Type, FPGA Flash type */
    {
        case FPGA_TYPE_2010:                            /* NONE, NONE, Spansion */
        case FPGA_TYPE_2015:                            /* NONE, NONE, Spansion */
        case FPGA_TYPE_2022:                            /* NONE, NONE, Spansion */
            return FL_FLASH_TYPE_20XX;//FL_FLASH_TYPE_PARALLEL_SPANSION_20XX;

        case FPGA_TYPE_ANCONA_AUGSBURG:                 /* Atmel (MCU), Actel (FPGA), Spansion (flash) */
        case FPGA_TYPE_ANCONA_CHEMNITZ:                 /* Atmel, Actel, Spansion */
        case FPGA_TYPE_ANCONA_CLOCK_CALIBRATION:        /* Atmel, Actel, Spansion */
        case FPGA_TYPE_ANCONA_TEST:                     /* Atmel, Actel, Spansion */

        case FPGA_TYPE_ATHEN_TEST:                      /* Atmel, Actel, Spansion */

        case FPGA_TYPE_ERCOLANO_DEMO:                   /* Atmel, Actel, Spansion */
        case FPGA_TYPE_ERCOLANO_TEST:                   /* Atmel, Actel, Spansion */

        case FPGA_TYPE_ESSEN:                           /* Atmel, Actel, Spansion */

        case FPGA_TYPE_FIRENZE_CHEMNITZ:                /* Atmel, Actel, Spansion */
        case FPGA_TYPE_FIRENZE_TEST:                    /* Atmel, Actel, Spansion */

        case FPGA_TYPE_GENOA_AUGSBURG:                  /* Atmel, Actel, Spansion */
        case FPGA_TYPE_GENOA_TEST:                      /* Atmel, Actel, Spansion */

        case FPGA_TYPE_HERCULANEUM_AUGSBURG:            /* Atmel, Actel, Spansion */
        case FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION:   /* Atmel, Actel, Spansion */
        case FPGA_TYPE_HERCULANEUM_TEST:                /* Atmel, Actel, Spansion */

        case FPGA_TYPE_LATINA_AUGSBURG:                 /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LATINA_LEONBERG:                 /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LATINA_TEST:                     /* Atmel, Actel, Spansion */

        case FPGA_TYPE_LIVIGNO_AUGSBURG:                /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVIGNO_AUGSBURG_4X1:            /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVIGNO_FULDA_V7330:             /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330:          /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690:          /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVIGNO_TEST:                    /* Atmel, Actel, Spansion */

        case FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G:     /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G:      /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_ODESSA_40:               /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_AUGSBURG:                /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_AUGSBURG_40:             /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_FULDA:                   /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_LEONBERG:                /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_TEST:                    /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LIVORNO_TEST_40:                 /* Atmel, Actel, Spansion */

        case FPGA_TYPE_LUCCA_AUGSBURG:                  /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LUCCA_AUGSBURG_40:               /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LUCCA_LEONBERG_V7330:            /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LUCCA_LEONBERG_V7690:            /* Atmel, Actel, Spansion */
        case FPGA_TYPE_LUCCA_TEST:                      /* Atmel, Actel, Spansion */

        case FPGA_TYPE_LUXG_PROBE:                      /* Atmel, Actel, Spansion (Ancona card) */
        case FPGA_TYPE_LUXG_TEST:                       /* Atmel, Actel, Spansionn (Ancona card) */

        case FPGA_TYPE_MILAN_AUGSBURG:                  /* Atmel, Actel, Spansion */
        case FPGA_TYPE_MILAN_TEST:                      /* Atmel, Actel, Spansion */

        case FPGA_TYPE_MONZA_AUGSBURG:                  /* Atmel, Actel, Spansion */
        case FPGA_TYPE_MONZA_TEST:                      /* Atmel, Actel, Spansion */

        case FPGA_TYPE_PISA_TEST:                       /* Atmel, Actel, Spansion */

            return FL_FLASH_TYPE_PARALLEL_SPANSION;

        case FPGA_TYPE_MANGO_ODESSA_40:                 /* SF2 (MCU and FPGA), Micron (flash) */
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1:      /* SF2, Micron, used to be FPGA_TYPE_PADUA_LEONBERG_VU9P */

        /* Mango 4 (2 different flashes): */
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P:          /* SF2, Micron */
        case FPGA_TYPE_MANGO_04_AUGSBURG_8X10:          /* SF2, Micron */
        case FPGA_TYPE_MANGO_04_AUGSBURG_16X10:         /* SF2, Micron */
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X25:          /* SF2, Micron */
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X40:          /* SF2, Micron */
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100:         /* SF2, Micron */
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P:    /* SF2, Micron */

        /* Mango 2: */
        case FPGA_TYPE_MANGO_AUGSBURG_8X10:             /* SF2, Micron */
        case FPGA_TYPE_MANGO_AUGSBURG_16X10:            /* SF2, Micron */
        case FPGA_TYPE_MANGO_AUGSBURG_2X25:             /* SF2, Micron */
        case FPGA_TYPE_MANGO_AUGSBURG_40:               /* SF2, Micron */
        case FPGA_TYPE_MANGO_AUGSBURG_100:              /* SF2, Micron */
        case FPGA_TYPE_MANGO_LEONBERG_VU125:            /* SF2, Micron */
        case FPGA_TYPE_MANGO_LEONBERG_VU9P:             /* SF2, Micron */

        case FPGA_TYPE_MANGO_TEST:                      /* SF2, Micron */

        case FPGA_TYPE_MANGO_TEST_QDR:                  /* SF2, Micron */

        case FPGA_TYPE_MANGO_WOLFSBURG_4X100G:          /* SF2, Micron */

        case FPGA_TYPE_PADUA_AUGSBURG_8X10:             /* SF2, Micron */
        case FPGA_TYPE_PADUA_AUGSBURG_2X25:             /* SF2, Micron */
        case FPGA_TYPE_PADUA_AUGSBURG_40:               /* SF2, Micron */
        case FPGA_TYPE_PADUA_AUGSBURG_100:              /* SF2, Micron */
        case FPGA_TYPE_PADUA_LEONBERG_VU125:            /* SF2, Micron */
        case FPGA_TYPE_PADUA_TEST:                      /* SF2, Micron */

        case FPGA_TYPE_TORINO_TEST:                     /* SF2, Micron */

            return FL_FLASH_TYPE_PARALLEL_MICRON;

        case FPGA_TYPE_SAVONA_AUGSBURG_8X10:            /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_SAVONA_AUGSBURG_2X25:            /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_SAVONA_AUGSBURG_2X40:            /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_SAVONA_AUGSBURG_2X100:           /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_SAVONA_TEST:                     /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_SAVONA_WOLFSBURG_2X100G:         /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:            /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_TIVOLI_TEST:                     /* Giant Gecko, QSPI Micron NOR */
        case FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G:         /* Giant Gecko, QSPI Micron NOR */

            return FL_FLASH_TYPE_MICRON_NOR;

        case FPGA_TYPE_SILIC100_AUGSBURG:               /* QSPI Micron? */
        case FPGA_TYPE_SILIC100_TEST:                   /* QSPI Micron? */
#if 1 // TODO delete totally
        case FPGA_TYPE_MARSALA_TEST:                    /* Flemming */
        case FPGA_TYPE_BERGAMO_TEST:                    /* Flemming */
        case FPGA_TYPE_COMO_TEST:                       /* Flemming */
#endif
            break; /* TODO unknown flash type, please add! */

        case FPGA_TYPE_CARDIFF_TEST:                    /* TODO unknown flash type???  */
        case FPGA_TYPE_CARDIFF_TEST_4X40G:              /* TODO unknown flash type???  */
        case FPGA_TYPE_RIMINI_TEST:                     /* TODO unknown flash type???  */
        case FPGA_TYPE_VCU1525_TEST:                    /* Not supported by this library */
        case FPGA_TYPE_UNKNOWN:
            break; /* Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return FL_FLASH_TYPE_NONE;
}

/***************************************************************************/

FL_Boolean FL_IsTestFpgaFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    switch (fpgaType)
    {
        case FPGA_TYPE_ANCONA_TEST:
        case FPGA_TYPE_ATHEN_TEST:
        case FPGA_TYPE_BERGAMO_TEST:
        case FPGA_TYPE_CARDIFF_TEST:
        case FPGA_TYPE_CARDIFF_TEST_4X40G:
        case FPGA_TYPE_COMO_TEST:
        case FPGA_TYPE_ERCOLANO_TEST:
        case FPGA_TYPE_FIRENZE_TEST:
        case FPGA_TYPE_GENOA_TEST:
        case FPGA_TYPE_HERCULANEUM_TEST:
        case FPGA_TYPE_LATINA_TEST:
        case FPGA_TYPE_LIVIGNO_TEST:
        case FPGA_TYPE_LIVORNO_TEST:
        case FPGA_TYPE_LIVORNO_TEST_40:
        case FPGA_TYPE_LUCCA_TEST:
        case FPGA_TYPE_LUXG_TEST:
        case FPGA_TYPE_MANGO_TEST:
        case FPGA_TYPE_MANGO_TEST_QDR:
        case FPGA_TYPE_MARSALA_TEST:
        case FPGA_TYPE_MILAN_TEST:
        case FPGA_TYPE_MONZA_TEST:
        case FPGA_TYPE_PADUA_TEST:
        case FPGA_TYPE_PISA_TEST:
        case FPGA_TYPE_RIMINI_TEST:
        case FPGA_TYPE_SAVONA_TEST:
        case FPGA_TYPE_SILIC100_TEST:
        case FPGA_TYPE_TIVOLI_TEST:
        case FPGA_TYPE_TORINO_TEST:
            return true;

        case FPGA_TYPE_2010:
        case FPGA_TYPE_2015:
        case FPGA_TYPE_2022:

        case FPGA_TYPE_ANCONA_AUGSBURG:
        case FPGA_TYPE_ANCONA_CHEMNITZ:
        case FPGA_TYPE_ANCONA_CLOCK_CALIBRATION:

        case FPGA_TYPE_ERCOLANO_DEMO:

        case FPGA_TYPE_ESSEN:

        case FPGA_TYPE_FIRENZE_CHEMNITZ:

        case FPGA_TYPE_GENOA_AUGSBURG:

        case FPGA_TYPE_HERCULANEUM_AUGSBURG:
        case FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION:

        case FPGA_TYPE_LATINA_AUGSBURG:
        case FPGA_TYPE_LATINA_LEONBERG:

        case FPGA_TYPE_LIVIGNO_AUGSBURG:
        case FPGA_TYPE_LIVIGNO_AUGSBURG_4X1:
        case FPGA_TYPE_LIVIGNO_FULDA_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690:

        case FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G:
        case FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G:
        case FPGA_TYPE_LIVORNO_ODESSA_40:
        case FPGA_TYPE_LIVORNO_AUGSBURG:
        case FPGA_TYPE_LIVORNO_AUGSBURG_40:
        case FPGA_TYPE_LIVORNO_FULDA:
        case FPGA_TYPE_LIVORNO_LEONBERG:

        case FPGA_TYPE_LUCCA_AUGSBURG:
        case FPGA_TYPE_LUCCA_AUGSBURG_40:
        case FPGA_TYPE_LUCCA_LEONBERG_V7330:
        case FPGA_TYPE_LUCCA_LEONBERG_V7690:

        case FPGA_TYPE_LUXG_PROBE:

        case FPGA_TYPE_MILAN_AUGSBURG:

        case FPGA_TYPE_MONZA_AUGSBURG:

        case FPGA_TYPE_MANGO_ODESSA_40:
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1:

        /* Mango 4 (2 different flashes): */
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_04_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X40:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P:

        /* Mango 2: */
        case FPGA_TYPE_MANGO_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_AUGSBURG_40:
        case FPGA_TYPE_MANGO_AUGSBURG_100:

        case FPGA_TYPE_MANGO_LEONBERG_VU125:
        case FPGA_TYPE_MANGO_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_WOLFSBURG_4X100G:

        case FPGA_TYPE_PADUA_AUGSBURG_8X10:
        case FPGA_TYPE_PADUA_AUGSBURG_2X25:
        case FPGA_TYPE_PADUA_AUGSBURG_40:
        case FPGA_TYPE_PADUA_AUGSBURG_100:
        case FPGA_TYPE_PADUA_LEONBERG_VU125:

        case FPGA_TYPE_SAVONA_AUGSBURG_8X10:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X25:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X40:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X100:
        case FPGA_TYPE_SAVONA_WOLFSBURG_2X100G:

        case FPGA_TYPE_SILIC100_AUGSBURG:

        case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:
        case FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G:

            return false;

        case FPGA_TYPE_VCU1525_TEST: /* Not supported by this library */
        case FPGA_TYPE_UNKNOWN:
            break; /* Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return false;
}

/***************************************************************************/

uint8_t FL_GetNumberOfPhysicalPorts(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    switch (fpgaType)
    {
        case FPGA_TYPE_LUCCA_AUGSBURG_40:
        case FPGA_TYPE_MILAN_AUGSBURG:
        case FPGA_TYPE_MILAN_TEST:
            return 1;

        case FPGA_TYPE_CARDIFF_TEST:
        case FPGA_TYPE_CARDIFF_TEST_4X40G:
        case FPGA_TYPE_GENOA_AUGSBURG:
        case FPGA_TYPE_GENOA_TEST:
        case FPGA_TYPE_LATINA_AUGSBURG:
        case FPGA_TYPE_LATINA_LEONBERG:
        case FPGA_TYPE_LATINA_TEST:
        case FPGA_TYPE_LIVORNO_AUGSBURG:
        case FPGA_TYPE_LIVORNO_AUGSBURG_40:
        case FPGA_TYPE_LIVORNO_FULDA:
        case FPGA_TYPE_LIVORNO_LEONBERG:
        case FPGA_TYPE_LIVORNO_ODESSA_40:
        case FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G:
        case FPGA_TYPE_LIVORNO_TEST:
        case FPGA_TYPE_LIVORNO_TEST_40:
        case FPGA_TYPE_LUCCA_LEONBERG_V7690:
        case FPGA_TYPE_LUCCA_LEONBERG_V7330:
        case FPGA_TYPE_LUCCA_TEST:
        case FPGA_TYPE_MANGO_04_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X40:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P:
        case FPGA_TYPE_MANGO_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_AUGSBURG_40:
        case FPGA_TYPE_MANGO_AUGSBURG_100:
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1:
        case FPGA_TYPE_MANGO_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_LEONBERG_VU125:
        case FPGA_TYPE_MANGO_ODESSA_40:
        case FPGA_TYPE_MANGO_TEST:
        case FPGA_TYPE_MANGO_TEST_QDR:
        case FPGA_TYPE_PADUA_AUGSBURG_100:
        case FPGA_TYPE_PADUA_AUGSBURG_2X25:
        case FPGA_TYPE_PADUA_AUGSBURG_40:
        case FPGA_TYPE_PADUA_AUGSBURG_8X10:
        case FPGA_TYPE_PADUA_LEONBERG_VU125:
        case FPGA_TYPE_PADUA_TEST:
        case FPGA_TYPE_PISA_TEST:
        case FPGA_TYPE_SAVONA_AUGSBURG_8X10:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X25:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X100:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X40:
        case FPGA_TYPE_SAVONA_TEST:
        case FPGA_TYPE_SAVONA_WOLFSBURG_2X100G:
        case FPGA_TYPE_SILIC100_AUGSBURG:
        case FPGA_TYPE_SILIC100_TEST:
        case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:
        case FPGA_TYPE_TIVOLI_TEST:
        case FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G:
            return 2;

        case FPGA_TYPE_ANCONA_AUGSBURG:
        case FPGA_TYPE_ANCONA_CHEMNITZ:
        case FPGA_TYPE_ANCONA_CLOCK_CALIBRATION:
        case FPGA_TYPE_ANCONA_TEST:
        case FPGA_TYPE_HERCULANEUM_AUGSBURG:
        case FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION:
        case FPGA_TYPE_HERCULANEUM_TEST:
        case FPGA_TYPE_LIVIGNO_AUGSBURG:
        case FPGA_TYPE_LIVIGNO_AUGSBURG_4X1:
        case FPGA_TYPE_LIVIGNO_TEST:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330:
        case FPGA_TYPE_LIVIGNO_FULDA_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690:
        case FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G:
        case FPGA_TYPE_LUCCA_AUGSBURG:
        case FPGA_TYPE_LUXG_PROBE:
        case FPGA_TYPE_LUXG_TEST:
        case FPGA_TYPE_MANGO_04_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_WOLFSBURG_4X100G:
        case FPGA_TYPE_RIMINI_TEST:
            return 4;

#if 1
        // Flemming: delete all these
        case FPGA_TYPE_FIRENZE_CHEMNITZ:
        case FPGA_TYPE_ESSEN:               /* actually has 4 physical ports but only one is ever used */
        case FPGA_TYPE_2010:
        case FPGA_TYPE_2022:
        case FPGA_TYPE_2015:
        case FPGA_TYPE_MONZA_AUGSBURG:
        case FPGA_TYPE_MONZA_TEST:
        case FPGA_TYPE_ATHEN_TEST:          /* Flemming */
        case FPGA_TYPE_FIRENZE_TEST:        /* Flemming */
        case FPGA_TYPE_MARSALA_TEST:        /* Flemming */
        case FPGA_TYPE_TORINO_TEST:         /* Flemming */
        case FPGA_TYPE_ERCOLANO_TEST:       /* Flemming */
        case FPGA_TYPE_ERCOLANO_DEMO:       /* Flemming */
        case FPGA_TYPE_BERGAMO_TEST:        /* Flemming */
        case FPGA_TYPE_COMO_TEST:           /* Flemming */
#endif

            break; /* TODO unknown number of physical ports, please implement! */

        case FPGA_TYPE_VCU1525_TEST: /* Not supported by this library */
        case FPGA_TYPE_UNKNOWN:
            break; /* Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return 0;
}

/***************************************************************************/

uint8_t FL_GetNumberOfLogicalPorts(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    //TODO
    return FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
}

/***************************************************************************/

uint8_t FL_GetMaximumNumberOfLogicalPorts(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    switch (fpgaType)
    {
        case FPGA_TYPE_MILAN_AUGSBURG:
        case FPGA_TYPE_MILAN_TEST:
            return 1;

        case FPGA_TYPE_FIRENZE_CHEMNITZ:
        case FPGA_TYPE_GENOA_AUGSBURG:
        case FPGA_TYPE_GENOA_TEST:
        case FPGA_TYPE_LATINA_AUGSBURG:
        case FPGA_TYPE_LATINA_LEONBERG:
        case FPGA_TYPE_LATINA_TEST:
        case FPGA_TYPE_PISA_TEST:
        case FPGA_TYPE_SILIC100_AUGSBURG:
        case FPGA_TYPE_SILIC100_TEST:
            return 2;

        case FPGA_TYPE_ANCONA_AUGSBURG:
        case FPGA_TYPE_ANCONA_CHEMNITZ:
        case FPGA_TYPE_ANCONA_TEST:
        case FPGA_TYPE_ANCONA_CLOCK_CALIBRATION:
        case FPGA_TYPE_CARDIFF_TEST:                /* TODO is this correct??? */
        case FPGA_TYPE_CARDIFF_TEST_4X40G:          /* TODO is this correct??? */
        case FPGA_TYPE_HERCULANEUM_AUGSBURG:
        case FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION:
        case FPGA_TYPE_HERCULANEUM_TEST:
        case FPGA_TYPE_LIVIGNO_AUGSBURG:
        case FPGA_TYPE_LIVIGNO_AUGSBURG_4X1:
        case FPGA_TYPE_LIVIGNO_FULDA_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690:
        case FPGA_TYPE_LIVIGNO_TEST:
        case FPGA_TYPE_LUXG_PROBE:
        case FPGA_TYPE_LUXG_TEST:
            return 4;

        case FPGA_TYPE_LIVORNO_AUGSBURG:
        case FPGA_TYPE_LIVORNO_AUGSBURG_40:
        case FPGA_TYPE_LIVORNO_FULDA:
        case FPGA_TYPE_LIVORNO_LEONBERG:
        case FPGA_TYPE_LIVORNO_TEST:
        case FPGA_TYPE_LIVORNO_TEST_40:
        case FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G:
        case FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G:
        case FPGA_TYPE_LIVORNO_ODESSA_40:
        case FPGA_TYPE_LUCCA_AUGSBURG:
        case FPGA_TYPE_LUCCA_AUGSBURG_40:
        case FPGA_TYPE_LUCCA_LEONBERG_V7330:
        case FPGA_TYPE_LUCCA_LEONBERG_V7690:
        case FPGA_TYPE_LUCCA_TEST:
        case FPGA_TYPE_PADUA_AUGSBURG_100:
        case FPGA_TYPE_PADUA_AUGSBURG_2X25:
        case FPGA_TYPE_PADUA_AUGSBURG_40:
        case FPGA_TYPE_PADUA_AUGSBURG_8X10:
        case FPGA_TYPE_PADUA_LEONBERG_VU125:
        case FPGA_TYPE_PADUA_TEST:
        case FPGA_TYPE_RIMINI_TEST:             /* TODO is this correct??? */
        case FPGA_TYPE_SAVONA_AUGSBURG_8X10:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X25:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X40:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X100:
        case FPGA_TYPE_SAVONA_TEST:
        case FPGA_TYPE_SAVONA_WOLFSBURG_2X100G:
        case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:
        case FPGA_TYPE_TIVOLI_TEST:
        case FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G:
            return 8;

        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P:
        case FPGA_TYPE_MANGO_ODESSA_40:
        case FPGA_TYPE_MANGO_AUGSBURG_100:
        case FPGA_TYPE_MANGO_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_AUGSBURG_40:
        case FPGA_TYPE_MANGO_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_LEONBERG_VU125:
        case FPGA_TYPE_MANGO_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1:
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_04_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X40:
        case FPGA_TYPE_MANGO_TEST:
        case FPGA_TYPE_MANGO_TEST_QDR:
        case FPGA_TYPE_MANGO_WOLFSBURG_4X100G:
            return 16;

#if 1 // delete all
        case FPGA_TYPE_ESSEN:
        case FPGA_TYPE_2010:
        case FPGA_TYPE_2022:
        case FPGA_TYPE_2015:
        case FPGA_TYPE_ATHEN_TEST:
        case FPGA_TYPE_FIRENZE_TEST:
        case FPGA_TYPE_MARSALA_TEST:
        case FPGA_TYPE_MONZA_AUGSBURG:
        case FPGA_TYPE_MONZA_TEST:
        case FPGA_TYPE_TORINO_TEST:
            // Flemming:
        case FPGA_TYPE_ERCOLANO_TEST:       /* Flemming */
        case FPGA_TYPE_ERCOLANO_DEMO:       /* Flemming */
        case FPGA_TYPE_BERGAMO_TEST:        /* Flemming */
        case FPGA_TYPE_COMO_TEST:           /* Flemming */
#endif

            break; /* TODO unknown number of logical ports (aka lanes), please implement! */

        case FPGA_TYPE_VCU1525_TEST: /* Not supported by this library */
        case FPGA_TYPE_UNKNOWN:
            break; /* Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return 0;
}

/***************************************************************************/

FL_CardType FL_GetCardTypeFromFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType fpgaType)
{
    // Find out which card we have:
    switch (fpgaType)
    {
        case FPGA_TYPE_2010:
        case FPGA_TYPE_2015:
        case FPGA_TYPE_2022:
            return FL_CARD_TYPE_20XX;

        case FPGA_TYPE_UNKNOWN:
            return FL_CARD_TYPE_UNKNOWN;

        case FPGA_TYPE_ANCONA_AUGSBURG:
        case FPGA_TYPE_ANCONA_CHEMNITZ:
        case FPGA_TYPE_ANCONA_CLOCK_CALIBRATION:
        case FPGA_TYPE_ANCONA_TEST:
            return FL_CARD_TYPE_ANCONA;

        case FPGA_TYPE_ATHEN_TEST:
            return FL_CARD_TYPE_ATHEN;

        case FPGA_TYPE_BERGAMO_TEST:
            return FL_CARD_TYPE_BERGAMO;

        case FPGA_TYPE_CARDIFF_TEST:
        case FPGA_TYPE_CARDIFF_TEST_4X40G:
            return FL_CARD_TYPE_CARDIFF;

        case FPGA_TYPE_COMO_TEST:
            return FL_CARD_TYPE_COMO;

        case FPGA_TYPE_ERCOLANO_DEMO:
        case FPGA_TYPE_ERCOLANO_TEST:
            return FL_CARD_TYPE_ERCOLANO;

        case FPGA_TYPE_ESSEN:
            return FL_CARD_TYPE_ESSEN;

        case FPGA_TYPE_FIRENZE_CHEMNITZ:
        case FPGA_TYPE_FIRENZE_TEST:
            return FL_CARD_TYPE_FIRENZE;

        case FPGA_TYPE_GENOA_AUGSBURG:
        case FPGA_TYPE_GENOA_TEST:
            return FL_CARD_TYPE_GENOA;

        case FPGA_TYPE_HERCULANEUM_AUGSBURG:
        case FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION:
        case FPGA_TYPE_HERCULANEUM_TEST:
            return FL_CARD_TYPE_HERCULANEUM;

        case FPGA_TYPE_LATINA_AUGSBURG:
        case FPGA_TYPE_LATINA_LEONBERG:
        case FPGA_TYPE_LATINA_TEST:
            return FL_CARD_TYPE_LATINA;

        case FPGA_TYPE_LIVIGNO_AUGSBURG:
        case FPGA_TYPE_LIVIGNO_AUGSBURG_4X1:
        case FPGA_TYPE_LIVIGNO_FULDA_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7330:
        case FPGA_TYPE_LIVIGNO_LEONBERG_V7690:
        case FPGA_TYPE_LIVIGNO_TEST:
            return FL_CARD_TYPE_LIVIGNO;

        case FPGA_TYPE_LIVORNO_AUGSBURG:
        case FPGA_TYPE_LIVORNO_AUGSBURG_40:
        case FPGA_TYPE_LIVORNO_FULDA:
        case FPGA_TYPE_LIVORNO_LEONBERG:
        case FPGA_TYPE_LIVORNO_ODESSA_40:
        case FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G:
        case FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G:
        case FPGA_TYPE_LIVORNO_TEST:
        case FPGA_TYPE_LIVORNO_TEST_40:
            return FL_CARD_TYPE_LIVORNO;

        case FPGA_TYPE_LUCCA_AUGSBURG:
        case FPGA_TYPE_LUCCA_AUGSBURG_40:
        case FPGA_TYPE_LUCCA_LEONBERG_V7330:
        case FPGA_TYPE_LUCCA_LEONBERG_V7690:
        case FPGA_TYPE_LUCCA_TEST:
            return FL_CARD_TYPE_LUCCA;

        case FPGA_TYPE_LUXG_PROBE:
        case FPGA_TYPE_LUXG_TEST:
            return FL_CARD_TYPE_LUXG;

        case FPGA_TYPE_MANGO_AUGSBURG_100:
        case FPGA_TYPE_MANGO_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_AUGSBURG_40:
        case FPGA_TYPE_MANGO_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_ODESSA_40:
        case FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1: // Used to be FPGA_TYPE_PADUA_LEONBERG_VU9P
        case FPGA_TYPE_MANGO_04_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X25:
        case FPGA_TYPE_MANGO_04_AUGSBURG_2X40:
        case FPGA_TYPE_MANGO_04_AUGSBURG_8X10:
        case FPGA_TYPE_MANGO_04_AUGSBURG_16X10:
        case FPGA_TYPE_MANGO_LEONBERG_VU125:
        case FPGA_TYPE_MANGO_LEONBERG_VU9P:
        case FPGA_TYPE_MANGO_TEST:
        case FPGA_TYPE_MANGO_TEST_QDR:
        case FPGA_TYPE_MANGO_WOLFSBURG_4X100G:
            return FL_CARD_TYPE_MANGO;

        case FPGA_TYPE_MARSALA_TEST:
            return FL_CARD_TYPE_MARSALA;

        case FPGA_TYPE_MILAN_AUGSBURG:
        case FPGA_TYPE_MILAN_TEST:
            return FL_CARD_TYPE_MILAN;

        case FPGA_TYPE_MONZA_AUGSBURG:
        case FPGA_TYPE_MONZA_TEST:
            return FL_CARD_TYPE_MONZA;

        case FPGA_TYPE_PADUA_AUGSBURG_100:
        case FPGA_TYPE_PADUA_AUGSBURG_2X25:
        case FPGA_TYPE_PADUA_AUGSBURG_40:
        case FPGA_TYPE_PADUA_AUGSBURG_8X10:
        case FPGA_TYPE_PADUA_LEONBERG_VU125:
        case FPGA_TYPE_PADUA_TEST:
            return FL_CARD_TYPE_PADUA;

        case FPGA_TYPE_PISA_TEST:
            return FL_CARD_TYPE_PISA;

        case FPGA_TYPE_RIMINI_TEST:
            return FL_CARD_TYPE_RIMINI;

        case FPGA_TYPE_SAVONA_AUGSBURG_8X10:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X25:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X100:
        case FPGA_TYPE_SAVONA_AUGSBURG_2X40:
        case FPGA_TYPE_SAVONA_TEST:
        case FPGA_TYPE_SAVONA_WOLFSBURG_2X100G:
            return FL_CARD_TYPE_SAVONA;

        case FPGA_TYPE_SILIC100_AUGSBURG:
        case FPGA_TYPE_SILIC100_TEST:
            return FL_CARD_TYPE_SILIC100;
            
        case FPGA_TYPE_TIVOLI_AUGSBURG_2X40:
        case FPGA_TYPE_TIVOLI_TEST:
        case FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G:
            return FL_CARD_TYPE_TIVOLI;

        case FPGA_TYPE_TORINO_TEST:
            return FL_CARD_TYPE_TORINO;

        case FPGA_TYPE_VCU1525_TEST:
            return FL_CARD_TYPE_UNKNOWN; /* Not supported by this library! */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA type %u (0x%X) not implemented", __func__, fpgaType, fpgaType);
    }

    return FL_CARD_TYPE_UNKNOWN;
}

/***************************************************************************/

const char * FL_GetPartNumberFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel)
{
    /* Make a part number from FPGA model */

    switch (fpgaModel)
    {
        case FPGA_MODEL_V5_LX110:           return "5vlx110tff1136";
        case FPGA_MODEL_V5_LX155:           return "5vlx155tff1136";
        case FPGA_MODEL_V6_LX240:           return "6vlx240tff1156";
        case FPGA_MODEL_V6_LX130:           return "6vlx130tff1156";
        case FPGA_MODEL_V6_SX315:           return "6vsx315tff1156";
        case FPGA_MODEL_V7_X690:            return "7vx690tffg1157";
        case FPGA_MODEL_V7_HXT580:          return "7vh580thcg1931";
        case FPGA_MODEL_V7_X330:            return "7vx330tffg1157";
        case FPGA_MODEL_V7_LX690_FFG1927:   return "7vx690tffg1927";
        case FPGA_MODEL_KU_X115:            return "xcku115-flva1517-2-e-es2";
        case FPGA_MODEL_US_V095_FFVE1924:   return "xcvu095-ffve1924-2-e-es1";
        case FPGA_MODEL_US_V080:            return "xcvu080-ffvb2104-2-e";
        case FPGA_MODEL_US_V125:            return "xcvu125-flvb2104-2-e";
        case FPGA_MODEL_US_V190:            return "xcvu190-flgb2104-2-e";
        case FPGA_MODEL_US_V9P_ES1:         return "xcvu9p-flgb2104-2-i-es1";
        case FPGA_MODEL_US_V9P_I:           return "xcvu9p-flgb2104-2-i";
        case FPGA_MODEL_US_V7P:             return "xcvu7p-flvb2104-2-i";
        case FPGA_MODEL_US_V9P_SG3:         return "xcvu9p-flgb2104-3-e";
        case FPGA_MODEL_US_KU15P:           return "xcku15p-ffve1760-2-e";
        case FPGA_MODEL_US_V9P_D:           return "xcvu9p-fsgd2104-2L-e";
        case FPGA_MODEL_STRATIX_5:          return "5SGXMA4K2F40C2";
        case FPGA_MODEL_ZYNQ_7000_45:       return "xc7z045ffg900-2";

        case FPGA_MODEL_UNKNOWN:
            break; /* Fall through to error handling */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA model %u (0x%X) not implemented", __func__, fpgaModel, fpgaModel);
    }

    return NULL; /* Unknown FPGA model! */
}

/***************************************************************************/

uint8_t FL_GetExpectedPCIeGenerationFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel)
{
    switch (fpgaModel)
    {
        case FPGA_MODEL_V5_LX110:
        case FPGA_MODEL_V5_LX155:
            return 1;

        case FPGA_MODEL_V6_LX240:
        case FPGA_MODEL_V6_SX315:
        case FPGA_MODEL_V6_LX130:
        case FPGA_MODEL_ZYNQ_7000_45:
            return 2;

        case FPGA_MODEL_KU_X115:
        case FPGA_MODEL_STRATIX_5:
        case FPGA_MODEL_V7_HXT580:
        case FPGA_MODEL_V7_LX690_FFG1927:
        case FPGA_MODEL_V7_X330:
        case FPGA_MODEL_V7_X690:
        case FPGA_MODEL_US_V095_FFVE1924:
        case FPGA_MODEL_US_V080:
        case FPGA_MODEL_US_V125:
        case FPGA_MODEL_US_V9P_ES1:
        case FPGA_MODEL_US_V190:
        case FPGA_MODEL_US_V9P_I:
        case FPGA_MODEL_US_V9P_D:
        case FPGA_MODEL_US_V7P:
        case FPGA_MODEL_US_V9P_SG3:
        case FPGA_MODEL_US_KU15P:
            return 3;

        case FPGA_MODEL_UNKNOWN:
            break; /* Unknown PCIe generation, please add! Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA model %u (0x%X) not implemented", __func__, fpgaModel, fpgaModel);
    }

    return 0;
}

/***************************************************************************/

FL_Boolean FL_GetExpectedPCIeLinkStateFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel)
{
    switch (fpgaModel)
    {
        case FPGA_MODEL_V5_LX110:
        case FPGA_MODEL_V5_LX155:
            return false;

        case FPGA_MODEL_V6_LX240:
        case FPGA_MODEL_V6_SX315:
        case FPGA_MODEL_V6_LX130:
        case FPGA_MODEL_ZYNQ_7000_45:
            return false;

        case FPGA_MODEL_KU_X115:
        case FPGA_MODEL_STRATIX_5:
        case FPGA_MODEL_V7_HXT580:
        case FPGA_MODEL_V7_LX690_FFG1927:
        case FPGA_MODEL_V7_X330:
        case FPGA_MODEL_V7_X690:
        case FPGA_MODEL_US_V095_FFVE1924:
        case FPGA_MODEL_US_V080:
        case FPGA_MODEL_US_V125:
        case FPGA_MODEL_US_V9P_ES1:
        case FPGA_MODEL_US_V190:
        case FPGA_MODEL_US_V9P_I:
        case FPGA_MODEL_US_V9P_D:
        case FPGA_MODEL_US_V7P:
        case FPGA_MODEL_US_V9P_SG3:
        case FPGA_MODEL_US_KU15P:
            return true;

        case FPGA_MODEL_UNKNOWN:
            break; /* Unknown PCIe generation, please add! Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA model %u (0x%X) not implemented", __func__, fpgaModel, fpgaModel);
    }

    return 0;
}

/***************************************************************************/

uint8_t FL_GetExpectedPCIeWidthFromFpgaModel(FL_ErrorContext * pErrorContext, FL_FpgaModel fpgaModel)
{
    switch (fpgaModel)
    {
        case FPGA_MODEL_V5_LX110:
        case FPGA_MODEL_V5_LX155:
        case FPGA_MODEL_V6_LX240:
        case FPGA_MODEL_V6_SX315:
        case FPGA_MODEL_V6_LX130:
        case FPGA_MODEL_ZYNQ_7000_45:
        case FPGA_MODEL_KU_X115:
        case FPGA_MODEL_STRATIX_5:
        case FPGA_MODEL_V7_HXT580:
        case FPGA_MODEL_V7_LX690_FFG1927:
        case FPGA_MODEL_V7_X330:
        case FPGA_MODEL_V7_X690:
        case FPGA_MODEL_US_V095_FFVE1924:
        case FPGA_MODEL_US_V080:
        case FPGA_MODEL_US_V125:
        case FPGA_MODEL_US_V190:
            return 8;

        case FPGA_MODEL_US_V7P:
        case FPGA_MODEL_US_V9P_ES1:
        case FPGA_MODEL_US_V9P_I:
        case FPGA_MODEL_US_V9P_SG3:
        case FPGA_MODEL_US_KU15P:
        case FPGA_MODEL_US_V9P_D:
            return 16;

        case FPGA_MODEL_UNKNOWN:
            break; /* Unknown expected PCIe with, please add! Fall through */

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (pErrorContext != NULL)
    {
        FL_Error(pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "%s for FPGA model %u (0x%X) not implemented", __func__, fpgaModel, fpgaModel);
    }

    return 0;
}

/***************************************************************************/

uint32_t FL_Read32Bits(const void * pRegistersBaseAddress, uint32_t byteOffset)
{
    return ((volatile uint32_t *)pRegistersBaseAddress)[byteOffset / sizeof(uint32_t)];
}

/***************************************************************************/

void FL_Write32Bits(void * pRegistersBaseAddress, uint64_t byteOffset, uint32_t value)
{
    ((volatile uint32_t *)pRegistersBaseAddress)[byteOffset / sizeof(uint32_t)] = value;
}

/***************************************************************************/

uint64_t FL_Read64Bits(const void * pRegistersBaseAddress, uint64_t byteOffset)
{
    return ((volatile uint64_t *)pRegistersBaseAddress)[byteOffset / sizeof(uint64_t)];
}

/***************************************************************************/

void FL_Write64Bits(void * pRegistersBaseAddress, uint64_t byteOffset, uint64_t value)
{
    ((volatile uint64_t *)pRegistersBaseAddress)[byteOffset / sizeof(uint64_t)] = value;
}

/***************************************************************************/

uint16_t FL_Swap16Bits(uint16_t word)
{
    /* swap odd and even bits */
    word = ((word >> 1) & 0x5555) | ((word & 0x5555) << 1);
    /* swap consecutive bit pairs */
    word = ((word >> 2) & 0x3333) | ((word & 0x3333) << 2);
    /* swap nibbles */
    word = ((word >> 4) & 0x0F0F) | ((word & 0x0F0F) << 4);
    /* swap bytes */
    word = ((word >> 8) & 0x00FF) | ((word & 0x00FF) << 8);

    return word;
};

/***************************************************************************/

uint32_t FL_Swap32Bits(uint32_t dword)
{
    /* swap odd and even bits */
    dword = ((dword >> 1) & 0x55555555) | ((dword & 0x55555555) << 1);
    /* swap consecutive 2-bit pairs */
    dword = ((dword >> 2) & 0x33333333) | ((dword & 0x33333333) << 2);
    /* swap nibbles */
    dword = ((dword >> 4) & 0x0F0F0F0F) | ((dword & 0x0F0F0F0F) << 4);
    /* swap bytes */
    dword = ((dword >> 8) & 0x00FF00FF) | ((dword & 0x00FF00FF) << 8);
    /* swap 2-byte pairs or 16-bit words */
    dword = (dword >> 16) | (dword << 16);

    return dword;
};

/***************************************************************************/

uint64_t FL_Swap64Bits(uint64_t qword)
{
    /* swap odd and even bits */
    qword = ((qword >> 1) & 0x5555555555555555UL) | ((qword & 0x5555555555555555UL) << 1);
    /* swap consecutive 2-bit pairs */
    qword = ((qword >> 2) & 0x3333333333333333UL) | ((qword & 0x3333333333333333UL) << 2);
    /* swap nibbles */
    qword = ((qword >> 4) & 0x0F0F0F0F0F0F0F0FUL) | ((qword & 0x0F0F0F0F0F0F0F0FUL) << 4);
    /* swap bytes */
    qword = ((qword >> 8) & 0x00FF00FF00FF00FFUL) | ((qword & 0x00FF00FF00FF00FFUL) << 8);
    /* swap 2-byte pairs or 16-bit words */
    qword = ((qword >> 16) & 0x0000FFFF0000FFFFUL) | ((qword & 0x0000FFFF0000FFFFUL) << 16);
    /* swap double words or 32-bit words */
    qword = (qword >> 32) | (qword << 32);

    return qword;
};

/***************************************************************************/

uint16_t FL_SwapWordBytes(uint16_t word)
{
    return ((word & 0xFF) << 8) | ((word & 0xFF00) >> 8);
}

/***************************************************************************/

uint64_t FL_GetRevision(void * pRegistersBaseAddress)
{
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 0); /* Write 0 to revision register to read revision */
    usleep(1);
    return FL_Read64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION); /* Read revision */
}

/***************************************************************************/

uint64_t FL_GetBuildTime(void * pRegistersBaseAddress)
{
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 1); /* Write one to revision register to read build time */
    usleep(1);
    uint64_t fpgaBuildTime = FL_Read64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION);
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 0); /* Write 0 to revision register so next reads will return revision */
    return fpgaBuildTime;
}

/***************************************************************************/

uint64_t FL_GetMercurialRevision(void * pRegistersBaseAddress)
{
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 2); /* Write 2 to revision register to read hg info */
    usleep(1);
    uint64_t mercurialRevisionInfo = FL_Read64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION); /* Read extended build_number */
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 0); /* Write 0 to revision register so next reads will return revision */
    return mercurialRevisionInfo;
}

/***************************************************************************/

uint32_t FL_GetExtendedBuildNumber(void * pRegistersBaseAddress)
{
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 3); /* Write 3 to revision register to read extended build number */
    usleep(1);
    uint32_t extendedBuildNumber = (uint32_t)FL_Read64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION) & 0xFFFFFFFF; /* Read extended build_number */
    FL_Write64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_REVISION, 0); /* Write 0 to revision register so next reads will return revision */
    return extendedBuildNumber;
}

/***************************************************************************/

uint8_t FL_AdminGetControllerVersion(void * pRegistersBaseAddress)
{
    return (FL_Read64Bits(pRegistersBaseAddress, FL_FPGA_REGISTER_ADMIN_COMMAND) >> 24) & 0xFF;
}

/***************************************************************************/

// SwitchToMangoPaduaPrimaryFlash
FL_Error FL_SelectPrimaryImage(const FL_FlashInfo * pFlashInfo)
{
    // TODO add handling of all card types
    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR) // TODO
    {
        return FL_GeckoSelectPrimaryFlash(pFlashInfo);
    }
    else
    {
        const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
        FL_Boolean hasPrimaryFlash = false;

        FL_ExitOnError(pLogContext, FL_HasPrimaryFlash(pFlashInfo->pFpgaInfo, &hasPrimaryFlash));
        if (hasPrimaryFlash)
        {
            return FL_AdminWriteCommand(pFlashInfo, 0xD, 0, 0x2);
        }

        /* Do nothing successfully if there is no primary image */
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }
}

/***************************************************************************/

// SwitchToMangoPaduaSecondaryFlash
FL_Error FL_SelectSecondaryImage(const FL_FlashInfo * pFlashInfo)
{
    // TODO add handling of all card types
    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        return FL_GeckoSelectFailSafeFlash(pFlashInfo);
    }
    else
    {
        const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
        FL_Boolean hasSecondaryFlash = false;

        FL_ExitOnError(pLogContext, FL_HasSecondaryFlash(pFlashInfo->pFpgaInfo, &hasSecondaryFlash));
        if (hasSecondaryFlash)
        {
            return FL_AdminWriteCommand(pFlashInfo, 0xD, 0, 0x6);
        }

        /* Do nothing successfully if there is no primary image */
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }
}

/***************************************************************************/

// ActelWriteFlash
FL_Error FL_WriteFlashWords(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint16_t * pWordData, uint32_t lengthInWords)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_UpdatableFlashInfo * pWritableFlashInfo = (FL_UpdatableFlashInfo *)&pFlashInfo->Updatable;;
    int i;
    FL_Error errorCode = FL_SUCCESS;
    bool validFlashCommandSet;

    if (flashWordAddress >= pFlashInfo->FlashSize / 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_WRITE_ADDRESS_IS_OUT_OF_RANGE,
            "Trying to write outside valid flash address range: word address %u >= flash size in words %u!\n",
            flashWordAddress, pFlashInfo->FlashSize / 2);
    }

    /* Not needed when checking after each write, as we do here */
    while (FL_FlashIsReady(pFlashInfo, flashWordAddress) != 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "WriteFlash cmd: Not ready!\n");
        FL_Log(pLogContext, FL_LOG_LEVEL_TRACE, "%s: flash is not ready!\n", __func__);
    }

#if 0
    //Calculate Sector Address
    if ((FlashAddr > 0x00FFFF) && (FlashAddr < 0x7F0000))//128KB sectors
        SectorAddr = FlashAddr & 0xFF0000;
    else
        //32KB sectors
        SectorAddr = FlashAddr & 0xFFC000;
#endif

    /* Write using "Write to buffer" commands */
    validFlashCommandSet = false;
    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Unlock 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Unlock 1
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0x25));//Write to buffer command
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 31));//WordCount - 1
            validFlashCommandSet = true;
            break;

        case FL_FLASH_COMMAND_SET_MICRON:
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0xE9));//Write to buffer command
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, lengthInWords - 1));//WordCount - 1
            validFlashCommandSet = true;
            break;

        case FL_FLASH_COMMAND_SET_SF2:
            /* Nothing to do in SF2 */
            validFlashCommandSet = true;
            break;

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_MACRONIX:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            /* These command sets are invalid in this context. */
            validFlashCommandSet = false;
            break;

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (!validFlashCommandSet)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
            "Unknown or invalid flash command set in this context: %u\n", pFlashInfo->FlashCommandSet);
    }

    /* Write the data */
    for (i = 0; i < lengthInWords; ++i)
    {
        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress + i, pWordData[i]));
    }

    /* Write "Program Buffer to Flash" Command */
    validFlashCommandSet = false;
    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0x29));
            validFlashCommandSet = true;
            break;

        case FL_FLASH_COMMAND_SET_MICRON:
            pWritableFlashInfo->MicronFlashReadMode = FL_MICRON_READ_MODE_STATUS;
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0xD0));
            validFlashCommandSet = true;
            break;

        case FL_FLASH_COMMAND_SET_SF2:
            // (*Sf2Bindings.ActelWriteFlashSF2WritePageCmd)(baseAddr, FlashAddr);
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 0xA, flashWordAddress, WRITE_PAGE_CMD));
            validFlashCommandSet = true;
            break;

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_MACRONIX:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            /* These command sets are invalid in this context. */
            validFlashCommandSet = false;
            break;

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (!validFlashCommandSet)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
            "Unknown or invalid flash command set in this context: %u\n", pFlashInfo->FlashCommandSet);
    }

    i = 1000;
    usleep(pFlashInfo->TypicalBufferProgramTime >> 1);
    while (((errorCode = FL_FlashIsReady(pFlashInfo, flashWordAddress)) == FL_ERROR_FLASH_IS_NOT_READY) && i != 0) /* Wait until programming is done. Between 300us and 3000us. */
    {
        --i;
        usleep(5);
        //prn(LOG_SCR | LOG_FILE | LEVEL_TRACE, "WriteFlash cmd: Not ready @ 0x%08X!\n", FlashAddr);
        FL_Log(pLogContext, FL_LOG_LEVEL_TRACE, "%s cmd: Not ready @ 0x%08X!\n", __func__, flashWordAddress);
    }
    if (i == 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_TIMEOUT_WAITING_FOR_FLASH_READY,
            "Timeout waiting for flash to be ready in %s\n", __func__);
    }

    return errorCode;
}

/***************************************************************************/

// ActelFlashIsReady
FL_Error FL_FlashIsReady(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_UpdatableFlashInfo * pWritableFlashInfo = (FL_UpdatableFlashInfo *)&pFlashInfo->Updatable;;

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            {
                /* Spansion Flash Memory is ready when two consecutive reads to the same bank returns the same word. */
                uint16_t firstRead, secondRead, thirdRead;

                /* Read first time */
                FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, flashWordAddress, &firstRead));

                /* Read a second time */
                FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, flashWordAddress, &secondRead));

                /* Read a third time and compare */
                FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, flashWordAddress, &thirdRead));

                if (firstRead != secondRead)
                {
                    FL_Log(pLogContext, FL_LOG_LEVEL_TRACE,
                        "Spansion flash is not ready: 0x%04X != 0x%04X != 0x%04X",
                        firstRead, secondRead, thirdRead);

                    // TODO: FL_ERROR_STANSION_FLASH_IS_NOT_READY???
                    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_ERROR_FLASH_IS_NOT_READY);
                }
                else
                {
                    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
                }
            }
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            {
                /* Micron Flash Memory is ready when Status Register bit 7 is 1. */
                uint16_t readData;

                if (pWritableFlashInfo->MicronFlashReadMode != FL_MICRON_READ_MODE_STATUS)
                {
                    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0x70));//Read Status Register
                    pWritableFlashInfo->MicronFlashReadMode = FL_MICRON_READ_MODE_STATUS;
                }
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 1, flashWordAddress, 0));//Write read command to Actel Controller
                FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &readData));

                if ((readData & 0x0080) == 0)
                {
                    FL_Log(pLogContext, FL_LOG_LEVEL_TRACE,
                        "cmd: %04X", readData); // TODO what is the message here?

                    // TODO: FL_ERROR_MICRON_FLASH_IS_NOT_READY???
                    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_ERROR_FLASH_IS_NOT_READY);
                }
                else
                {
                    FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0xFF));//Read Array
                    pWritableFlashInfo->MicronFlashReadMode = FL_MICRON_READ_MODE_ARRAY;
                    if ((readData & 0x033A) != 0)
                    {
                        FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, flashWordAddress, 0x50));//Clear Status

                        if ((readData & 0x0002) != 0)
                        {
                            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MICRON_FLASH_IS_NOT_READY,
                                "Micron flash is not ready: 0x%04X (block locked)", readData);
                        }
                        else if ((readData & 0x0300) != 0)
                        {
                            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MICRON_FLASH_IS_NOT_READY,
                                "Micron flash is not ready: 0x%04X (block not erased)", readData);
                        }
                        else
                        {
                            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_MICRON_FLASH_IS_NOT_READY,
                                "Micron flash is not ready: 0x%04X", readData); // TODO what message?
                        }
                    }
                    else
                    {
                        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
                    }
                }
            }
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_SF2:
            return FL_AdminFlashIsReady(pFlashInfo, flashWordAddress);

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
        "Unknown or invalid flash command set in this context: %u\n", pFlashInfo->FlashCommandSet);
}

/***************************************************************************/

FL_Error FL_FlashWaitIsReady(const FL_FlashInfo * pFlashInfo, uint32_t flashAddress, uint32_t timeoutInMilliseconds)
{
    FL_Error errorCode;

    while ((errorCode = FL_FlashIsReady(pFlashInfo, flashAddress)) == FL_ERROR_FLASH_IS_NOT_READY)
    {
        /* Wait until flash SPI controller is ready */
    }

    return errorCode;
}

/***************************************************************************/

// ActelFlashAddrToSector
FL_Error FL_FlashConvertAddressToSector(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint32_t * pSector)
{
    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        // TODO: address to sector conversion not supported for Micron NOR flashes.
        *pSector = 1; // TODO set nonzero value to fool callers
    }
    else
    {
        uint32_t lowSectorBoundary = pFlashInfo->LowSectorCount * pFlashInfo->LowSectorSize;
        uint32_t middleSectorBoundary = lowSectorBoundary + pFlashInfo->MiddleSectorCount * pFlashInfo->MiddleSectorSize;
        uint32_t flashByteAddress = flashWordAddress * 2; /* Make flash word address a byte address */

        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "ActelFlashAddrToSector: FlashAddr:0x%X Low:0x%X mid:0x%X\n",
        //    FlashAddr, LowSecBoundary, MidSecBoundary);
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "flashByteAddress: 0x%X lowSectorBoundary: 0x%X middleSectorBoundary: 0x%X\n",
            flashByteAddress, lowSectorBoundary, middleSectorBoundary);

        /* Convert a flash address to a sector number */
        if (flashByteAddress < lowSectorBoundary)
        {
            /* Lower sector(s) */
            *pSector = flashByteAddress / pFlashInfo->LowSectorSize;
        }
        else if (flashByteAddress < middleSectorBoundary)
        {
            /* Middle sectors */
            *pSector = ((flashByteAddress - lowSectorBoundary) / pFlashInfo->MiddleSectorSize) + pFlashInfo->LowSectorCount;
        }
        else
        {
            /* Top sector(s) */
            *pSector = ((flashByteAddress - middleSectorBoundary) / pFlashInfo->TopSectorSize) + pFlashInfo->LowSectorCount + pFlashInfo->MiddleSectorCount;
        }

        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "ActelFlashAddrToSector: flashAddr:0x%X sector %u\n", FlashAddr, sector);
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "flashByteAddress 0x%X sector %u\n", flashByteAddress, *pSector);
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelFlashSectorToAddr
FL_Error FL_FlashConvertSectorToAddress(const FL_FlashInfo * pFlashInfo, uint32_t sector, uint32_t * pFlashWordAddress)
{
    // Sector boundary
    uint32_t lowSectorBoundary = pFlashInfo->LowSectorCount * pFlashInfo->LowSectorSize;
    uint32_t middleSectorBoundary = lowSectorBoundary + pFlashInfo->MiddleSectorCount * pFlashInfo->MiddleSectorSize;
    uint32_t flashByteAddress;

    //Convert Sector number to an address within that sector
    if (sector < pFlashInfo->LowSectorCount)
    {
        /* Lower sector(s) */
        flashByteAddress = pFlashInfo->LowSectorSize * sector;
    }
    else if (sector < pFlashInfo->LowSectorCount + pFlashInfo->MiddleSectorCount)
    {
        /* Middle sectors */
        flashByteAddress = lowSectorBoundary + pFlashInfo->MiddleSectorSize * (sector - pFlashInfo->LowSectorCount);
    }
    else
    {
        /* Top sector(s) */
        flashByteAddress = middleSectorBoundary + pFlashInfo->TopSectorSize * (sector - pFlashInfo->LowSectorCount - pFlashInfo->MiddleSectorCount);
    }

    *pFlashWordAddress = flashByteAddress / 2; /* Convert byte address to word address */

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelSectorErase
FL_Error FL_FlashEraseSector(const FL_FlashInfo * pFlashInfo, uint32_t sector)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_UpdatableFlashInfo * pWritableFlashInfo = (FL_UpdatableFlashInfo *)&pFlashInfo->Updatable;
    uint32_t sectorFlashAddress;
    FL_Error errorCode = FL_SUCCESS;
    bool validFlashCommandSet;

    FL_ExitOnError(pLogContext, FL_FlashConvertSectorToAddress(pFlashInfo, sector, &sectorFlashAddress));

    FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_NOT_IMPLEMENTED,
            " *** Function %s not implemented for Micron NOR flash; use function %s instead.", __func__, nameof(FL_QSPI_BulkErase));
    }

    // TODO: look at flash type instead of command set:

    validFlashCommandSet = false;
    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:

            /* Write Unlock Commands */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Unlock 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Unlock 1

            /* Write Sector Erase Commands */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0x80));//Sector Erase 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Sector Erase 1
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Sector Erase 2
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress, 0x30));//Sector Erase 3

            validFlashCommandSet = true;

            break;

        case FL_FLASH_COMMAND_SET_MICRON:

            /* Write Unlock Commands */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress, 0x60));//Unlock 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress, 0xD0));//Unlock 1

            /* Write Sector Erase Commands */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress, 0x20));//Sector Erase 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress, 0xD0));//Sector Erase 3
            pWritableFlashInfo->MicronFlashReadMode = FL_MICRON_READ_MODE_STATUS;

            validFlashCommandSet = true;

            break;

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_MACRONIX:
        case FL_FLASH_COMMAND_SET_SF2:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            /* These command sets are invalid in this context. */
            validFlashCommandSet = false;
            break;

        /* DO NOT ADD DEFAULT CASE TO THIS SWITCH! */
        /* DEFAULT CASE WILL DISABLE COMPILER WARNING FOR MISSING EXPLICIT ENUM VALUE CASE! */
    }

    if (!validFlashCommandSet)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
            "Unknown or invalid flash command set in this context: %u\n", pFlashInfo->FlashCommandSet);
    }

    FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_CalculateFlashImageMD5Sum(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget, uint8_t md5sum[FL_MD5_HASH_SIZE])
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_MD5Context md5Context;
    uint32_t flashAddress = pFlashInfo->ImageStartAddress[imageTarget];
    uint32_t flashSize = (pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize) >> 1; /* size of image 2 area in words */
    uint32_t i;

    FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "Flash Address: 0x%x, Flash Size: 0x%x, MiddleSectorSize: %u, SectorsPerImage: %u\n",
        flashAddress, flashSize, pFlashInfo->MiddleSectorSize, pFlashInfo->SectorsPerImage[imageTarget]);

    FL_MD5_Start(&md5Context);
    for (i = 0; i < flashSize; ++i, ++flashAddress)
    {
        uint16_t flashData;

        FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, flashAddress, &flashData)); /* Read data from flash memory */
        FL_MD5_Add(&md5Context, (const uint8_t *)&flashData, sizeof(flashData));
    }
    FL_MD5_End(&md5Context, md5sum);

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelReadPcbVer
FL_Error FL_ReadPcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t * pPcbVersion)
{
    if (pFlashInfo->pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
        pFlashInfo->pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
    {
        return FL_GeckoReadPcbVersion(pFlashInfo, pPcbVersion);
    }
    else
    {
        FL_Error errorCode = FL_AdminReadWord(pFlashInfo, PCB_INFO, pPcbVersion);
        *pPcbVersion &= 0xFF;
        return errorCode;
    }
}

/***************************************************************************/

// ActelWritePciBootMode
FL_Error FL_WritePciBootMode(const FL_FlashInfo * pFlashInfo, uint16_t pciBootMode)
{
    return FL_WriteSectorZeroWords(pFlashInfo, PCI_BOOT_MODE, &pciBootMode, 1);
}

/***************************************************************************/

// ActelWritePcbVer
FL_Error FL_WritePcbVersion(const FL_FlashInfo * pFlashInfo, uint16_t pcbVersion)
{
    if (pFlashInfo->pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
        pFlashInfo->pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
    {
        //GeckoWritePcbVer()
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_NOT_IMPLEMENTED, "FL_WritePcbVersion not implemented yet for SAVONA");
    }
    else
    {
        FL_Error errorCode = FL_SUCCESS;

        if (pcbVersion > 255)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_PCB_VERSON_NUMBER, "illegal PCB version number: %u", pcbVersion);
        }

        // Only use the last byte for PCB ver, other reserved for FPGA type;
        pcbVersion &= 0xff;
        errorCode = FL_WriteSectorZeroWords(pFlashInfo, PCB_INFO, &pcbVersion, 1);
        if (errorCode == FL_SUCCESS)
        {
            //prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "PCB version written: %d\n", pcb_ver_data);
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "PCB version written: %u\n", pcbVersion);
        }

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
    }
}

/***************************************************************************/

// ActelReadPciBootMode
FL_Error FL_ReadPciBootMode(const FL_FlashInfo * pFlashInfo, uint16_t * pPciBootMode)
{
    FL_Error errorCode = FL_AdminReadWord(pFlashInfo, PCI_BOOT_MODE, pPciBootMode);
    *pPciBootMode &= 0x3;
    return errorCode;
}

/***************************************************************************/

// ActelWriteSector0Words
//Update len words to sector0, starting with reading existing data and erasing sector
FL_Error FL_WriteSectorZeroWords(const FL_FlashInfo * pFlashInfo, uint32_t flashWordAddress, uint16_t * pData, uint16_t lengthInWords)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint16_t sectorZeroCopy[pFlashInfo->SectorZeroSize];
    uint32_t flashAddress;
    FL_Error errorCode = FL_SUCCESS;

    /* Make a local copy of flash memory so we can erase the sector and update the password */
    for (flashAddress = 0; flashAddress < pFlashInfo->SectorZeroSize; ++flashAddress)
    {
        FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, flashAddress, &sectorZeroCopy[flashAddress]));
    }

    /* Erase sector 0 */
    FL_ExitOnError(pLogContext, FL_FlashEraseSector(pFlashInfo, 0));

    /* Update local flash storage with data */
    memcpy(sectorZeroCopy + flashWordAddress, pData, 2 * lengthInWords);

    /* Write updated sector data data back to flash memory sector 0 */
    for (flashAddress = 0; flashAddress < pFlashInfo->SectorZeroSize; flashAddress += pFlashInfo->BufferCount)
    {
        FL_ExitOnError(pLogContext, FL_WriteFlashWords(pFlashInfo, flashAddress, sectorZeroCopy + flashAddress, pFlashInfo->BufferCount));
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

// ActelSectorIsLocked
FL_Error FL_FlashSectorIsLocked(const FL_FlashInfo * pFlashInfo, uint32_t sector, FL_Boolean * pSectorIsLocked)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        *pSectorIsLocked = false;
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    // TODO: look at flash type instead of command set!!!

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            {
                uint16_t ppbStatus;
                uint32_t sectorFlashAddress = 0;;

                FL_ExitOnError(pLogContext, FL_FlashConvertSectorToAddress(pFlashInfo, sector, &sectorFlashAddress));

                FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));

                /* Write PPB - Non-Volatile Sector Protection Command Set Entry */
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Unlock 0
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Unlock 1
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress + 0x555, 0xC0));//Command set entry

                /* Read PPB status */
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 1, sectorFlashAddress, 0));//Read PPB status in sector
                FL_ExitOnError(pLogContext, FL_AdminReadCommandRegister(pFlashInfo, &ppbStatus));
                ppbStatus &= 0x1;

                /* Write PPB - Non-Volatile Sector Protection Command Set Exit */
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x90));//PPB Exit 0
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x00));//PPB Exit 1

                //FL_ExitOnError(pLogContext, FL_WriteAdminCommand(pFlashInfo, 2, 0x0, 0xF0));//PPB exit by writing Reset command. needed?

                FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));

                /* PPB status 0 is locked, status 1 is not locked. */
                *pSectorIsLocked = ppbStatus == 0 ? true : false;

                return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
            }

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            /* Micron Strata Flash doesn't have persistent protection of sectors, do nothing */
            *pSectorIsLocked = false; /* Not locked! */
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_SF2:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            *pSectorIsLocked = false; /* Not locked! */
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
        "Flash sector locking not supported on this flash command set %u", pFlashInfo->FlashCommandSet);
}

/***************************************************************************/

// ActelSectorLock
FL_Error FL_FlashSectorLock(const FL_FlashInfo * pFlashInfo, uint32_t sector)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            {
                uint32_t sectorFlashAddress = 0;

                FL_ExitOnError(pLogContext, FL_FlashConvertSectorToAddress(pFlashInfo, sector, &sectorFlashAddress));

                FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));

                /* Write PPB - Non-Volatile Sector Protection Command Set Entry */
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Unlock 0
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Unlock 1
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress + 0x555, 0xC0));//Command set entry
                /* Program PPB sector lock */
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0xA0));//Setup Program Command
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, sectorFlashAddress, 0));//Set Lock bit

                FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));

                /* Write PPB - Non-Volatile Sector Protection Command Set Exit */
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x90));//PPB Exit 0
                FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x00));//PPB Exit 1

                FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, sectorFlashAddress, 0));
            }
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            /* Micron Strata Flash doesn't have locking of sectors, do nothing. */
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_SF2:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            break;

    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
        "Flash sector locking not supported on this flash command set %u", pFlashInfo->FlashCommandSet);
}

/***************************************************************************/

// ActelSectorUnlockAll
FL_Error FL_FlashUnlockAllSectors(const FL_FlashInfo * pFlashInfo)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:

            /* Wait until flash bank 0 is ready. This only tests that a complete chip erase is not in
               progress, or a program/erase on the bank.
              I assume that the program/erase commands will wait until done before continuing. */
            FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, 0, 0));

            /* Write PPB - Non-Volatile Sector Protection Command Set Entry */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xAA));//Unlock 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x2AA, 0x55));//Unlock 1
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x555, 0xC0));//Command set entry

            /* Write Erase All Persistent Protection Bits Command */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x80));//All Erase 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x30));//All Erase 1

            FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, 0, 0));

            /* Write PPB - Non-Volatile Sector Protection Command Set Exit */
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x90));//PPB Exit 0
            FL_ExitOnError(pLogContext, FL_AdminWriteCommand(pFlashInfo, 2, 0x0, 0x00));//PPB Exit 1

            FL_ExitOnError(pLogContext, FL_FlashWaitIsReady(pFlashInfo, 0, 0));

            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            /* Micron Strata Flash doesn't have lockable sectors, do nothing */
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_SF2:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
        "Flash sector unlocking not supported on this flash command set %u", pFlashInfo->FlashCommandSet);
}

/***************************************************************************/

// ActelProtectImage
FL_Error FL_FlashProtectImage(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        /* Flash protection not implemented/supported for Micron NOR flashes. */
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            {
                /* Calculate Sector number from flash address. */
                uint32_t startSector = 0; 
                uint32_t sector;

                FL_ExitOnError(pLogContext, FL_FlashConvertAddressToSector(pFlashInfo, pFlashInfo->ImageStartAddress[imageTarget], &startSector));

                //prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Write protecting FPGA image in flash memory. Protecting sector %d to %d.\n",
                //    StartSector, StartSector + FlashInfo.SecPerImg[image_no] - 1);
                FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Write protecting FPGA image in flash memory. Protecting sectors %u to %u.\n",
                    startSector, startSector + pFlashInfo->SectorsPerImage[imageTarget] - 1);

                for (sector = startSector; sector < startSector + pFlashInfo->SectorsPerImage[imageTarget]; ++sector)
                {
                    //prn(LOG_SCR | LEVEL_INFO, "Protecting flash memory sector %d\r", i);
                    FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Protecting flash memory sector %u\r", sector);
                    FL_ExitOnError(pLogContext, FL_FlashSectorLock(pFlashInfo, sector));
                }
                //prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "FPGA image protected.                     \n");
                FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "FPGA image protected.                     \n");
            }
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            /* Micron Strata Flash doesn't have persistent protection of sectors, do nothing. */
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_SF2:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
        "Protect flash image not supported on this flash command set %u", pFlashInfo->FlashCommandSet);
}

/***************************************************************************/

// ActelUnprotectImage
FL_Error FL_FlashUnprotectImage(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        /* Flash protection not implemented/supported for Micron NOR flashes. */
        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    switch (pFlashInfo->FlashCommandSet)
    {
        case FL_FLASH_COMMAND_SET_SPANSION:
            {
                //Calculate Sector number from flash address.
                uint32_t startSector = 0;
                FL_FlashConvertAddressToSector(pFlashInfo, pFlashInfo->ImageStartAddress[imageTarget], &startSector);
                uint32_t sector;
                FL_Boolean sectorLocked[pFlashInfo->SectorCount]; /* contains locks for all sectors */

                /* Read the lock bits for all sectors */
                //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Reading locks for individual sectors\n");
                FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "Reading locks for individual sectors\n");

                for (sector = 0; sector < pFlashInfo->SectorCount; ++sector)
                {
                    FL_ExitOnError(pLogContext, FL_FlashSectorIsLocked(pFlashInfo, sector, &sectorLocked[sector]));
                }

                FL_ExitOnError(pLogContext, FL_FlashUnlockAllSectors(pFlashInfo)); /* Unlock all sectors */

                //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Removing write protection on FPGA image. Sector %d to %d.\n",
                //    StartSector, StartSector + FlashInfo.SecPerImg[image_no] - 1);
                FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG,
                    "Removing write protection on FPGA image. Sectors %u to %u.\n",
                    startSector, startSector + pFlashInfo->SectorsPerImage[imageTarget] - 1);

                for (sector = startSector; sector < startSector + pFlashInfo->SectorsPerImage[imageTarget]; sector++)
                {
                    sectorLocked[sector] = true; /* Clear locks for this image */
                }
                for (sector = 0; sector < pFlashInfo->SectorCount; sector++)
                {
                    if (!sectorLocked[sector]) /* Apply sector protection to all previously locked sectors, except selected image. */
                    {
                        FL_ExitOnError(pLogContext, FL_FlashSectorLock(pFlashInfo, sector));
                    }
                }

                //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "\nFPGA image unprotected.\n");
                FL_Log(pLogContext, FL_LOG_LEVEL_DEBUG, "\nFPGA image unprotected.\n");
            }
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_MICRON:
        case FL_FLASH_COMMAND_SET_MACRONIX: /* Macronix flash is compatible with Micron */
            /* Micron Strata Flash doesn't have persistent protection of sectors, do nothing */
            return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);

        case FL_FLASH_COMMAND_SET_UNKNOWN:
        case FL_FLASH_COMMAND_SET_NONE:
        case FL_FLASH_COMMAND_SET_SF2:
        case FL_FLASH_COMMAND_SET_GIANT_GECKO:
            break;
    }

    return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_FLASH_COMMAND_SET,
        "Unprotect flash image not supported on this flash command set %u", pFlashInfo->FlashCommandSet);
}

/***************************************************************************/

// ActelEraseImage
FL_Error FL_FlashEraseImage(const FL_FlashInfo * pFlashInfo, FL_ImageTarget imageTarget, const char * fileName)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    uint32_t startSector = 0;
    uint32_t endSector = startSector + pFlashInfo->SectorsPerImage[imageTarget];
    uint32_t sector;

    FL_ExitOnError(pLogContext, FL_FlashConvertAddressToSector(pFlashInfo, pFlashInfo->ImageStartAddress[imageTarget], &startSector));

    if (pFlashInfo->SectorsPerImage[imageTarget] == 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "No flash space reserved for image #%d\n", imageType);
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_NO_FLASH_SPACE_RESERVED_FOR_IMAGE,
            "No flash space reserved for image type %u\n", imageTarget);
    }

// If we have a filename then only erase the sectors that are actually used
    if (fileName != NULL)
    {
        struct stat fileStatus;
        FL_Error errorCode = FL_SUCCESS;

        FILE * pImagefile = fopen(fileName, "rb");
        if (pImagefile == NULL)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "No such file: %s", fileName);
        }

        if (fstat(fileno(pImagefile), &fileStatus) != 0) /* Get file statistics */
        {
            errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FSTAT_FAILED,
                "fstat call of file %s failed with errno %d", fileName, errno);
        }
        if (fclose(pImagefile) != 0 || errorCode != FL_SUCCESS)
        {
            FL_Error latestErrorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
                "fclose of file %s call failed with errno %d", fileName, errno);
            return errorCode != FL_SUCCESS ? errorCode : latestErrorCode;
        }
        if (fileStatus.st_size > pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IMAGE_FILE_TOO_BIG,
                "Image file %s is too big: file size %lu > flash size %u", fileName,
                fileStatus.st_size, pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize);
        }
        /* All sectors for clock calibration image 2 are erased to be able to calculate same MD5 over smaller image */
        if (imageTarget != FL_IMAGE_TARGET_CLOCK_CALIBRATION)
        {
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Erasing %lu bytes in flash\n", fileStatus.st_size);
            endSector = startSector + (fileStatus.st_size + pFlashInfo->MiddleSectorSize - 1) / pFlashInfo->MiddleSectorSize;
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "EndSector %d\n", endSector);
        }
    }

    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Erasing image @ 0x%X. Sector %u\n", pFlashInfo->ImageStartAddress[imageTarget], startSector);
    for (sector = startSector; sector < endSector; sector++)
    {
        FL_Boolean sectorIsLocked = false;

        FL_ExitOnError(pLogContext, FL_FlashSectorIsLocked(pFlashInfo, sector, &sectorIsLocked));
        if (sectorIsLocked)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_SECTOR_LOCKED_CANNOT_ERASE, "Flash sector %u is locked. Unlock before erase.\n", sector);
        }
    }

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "Erasing FPGA image from flash memory. Bulk erasing whole flash.\n");
        FL_ExitOnError(pLogContext, FL_QSPI_BulkErase(pFlashInfo));
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "FPGA image erased.\n");
    }
    else
    {
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "Erasing FPGA image from flash memory. Erasing sectors %u to %u.\n", startSector, endSector - 1);
        for (sector = startSector; sector < endSector; ++sector)
        {
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "Erasing flash memory sector %d\r", sector);
            FL_ExitOnError(pLogContext, FL_FlashEraseSector(pFlashInfo, sector));
        }

        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "\nFPGA image erased.\n");
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// ActelWriteImage
FL_Error FL_FlashWriteImage(
    const FL_Parameters *   pParameters,
    const FL_FlashInfo *    pFlashInfo,
    const char *            fileName,
    FL_ImageTarget          imageTarget,
    const char *            expectedPartNumber)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_Error errorCode = FL_SUCCESS;
    FILE * pImagefile;
    struct stat fileStatistics;
    long fileSize;
    uint32_t wordCount;
    uint32_t flashWriteWordAddress = pFlashInfo->ImageStartAddress[imageTarget];
    uint32_t writtenBytesCount = 0;

    if (pFlashInfo->SectorsPerImage[imageTarget] == 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_NO_FLASH_SPACE_RESERVED_FOR_IMAGE, "No flash space reserved for image #%u\n", imageTarget);
    }
    pImagefile = fopen(fileName, "rb");
    if (pImagefile == NULL)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE,
            "Failed to open Xilinx bit image file %s with errno %d", fileName, errno);
    }
    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT)
    {
        FL_ExitOnError(pLogContext, FL_FileReadXilinxBitFileHeader2(pFlashInfo, pImagefile, expectedPartNumber, -1));
    }
    if (fstat(fileno(pImagefile), &fileStatistics) != 0) /* Get file statistics */
    {
        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FSTAT_FAILED,
            "system function fstat failed on Xilinx bit image file %s, errno %d",
            fileName, errno);
        goto ErrorExit;
    }
    fileSize = fileStatistics.st_size - ftell(pImagefile);
    if (fileSize > pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize)
    {
        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IMAGE_FILE_TOO_BIG,
            "Xilinx bit image file %s is too big: %lu > %u",
            fileName, fileSize, pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize);
        goto ErrorExit;
    }
    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_SPI)
    {
        if (fileSize % 64 != 0) // SPI files are apparently a multiple of 64 bytes long
        {
            //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Wrong spi file size: %s\n", fileName);
            errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IMAGE_FILE_WRONG_SIZE,
                "Xilinx bit image file %s is wrong size: %lu %% 64 != 0", fileName, fileSize);
            goto ErrorExit;
        }
    }

    FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Writing flash memory\r");

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        FL_ExitOnError(pLogContext, FL_QSPI_ProgramFile(pFlashInfo, fileName, pImagefile, true));
    }
    else
    {
        do
        {
            uint16_t readWordBuffer[pFlashInfo->BufferCount]; /* Buffer word data read from file. */

            /* Read FlashInfo.BufCnt x 2 bytes from file */
            wordCount = fread(readWordBuffer, 2, pFlashInfo->BufferCount, pImagefile);
            //FL_Log(pLogContext, FL_LOG_LEVEL_TRACE, "Read %d words from file\n", wordCount);
            if (wordCount > 0)
            {
                uint32_t i;

                if (writtenBytesCount == 0)
                {
                    /* This is the beginning of the file except bit file header has already been read. */

                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_SPI)
                    {
                        //TODO: SPI file validation
                    }
                    else
                    {
                        /* Check the first 32 bytes. They should be 0xFF
                           In UltraScale it is the first 64 bytes, but we only check the first 32 bytes. */
                        for (i = 0; i < 16; ++i)
                        {
                            if (readWordBuffer[i] != 0xFFFF)
                            {
                                //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "%s is not a valid file. The bit stream should start with 32 x 0xFF\n", fileName);
                                errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT,
                                    "%s is not a valid bit image file. The bit stream should start with 32 x 0xFF.", fileName);
                                goto ErrorExit;
                            }
                        }

                        if (readWordBuffer[16] == 0xFFFF)
                        {
                            size_t numberOfItemsRead;
                            //Skip the extra 16 words with 0xFFFF and read 16 new words from the file
                            if (wordCount - 16U > pFlashInfo->BufferCount)
                            {
                                //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: buffer memmove write overflow: %d - 16 > %u\n", wordCount, pFlashInfo->BufferCount);
                                errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_BUFFER_WRITE_OVERFLOW,
                                    "Xilinx bit image file %s buffer write overflow: %u - 16 > %u", fileName, wordCount, pFlashInfo->BufferCount);
                                goto ErrorExit;
                            }
                            if (wordCount - 16U > pFlashInfo->BufferCount - 16)
                            {
                                //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: buffer memmove read overflow: %d - 16 > %u - 16\n", wordCount, pFlashInfo->BufferCount);
                                errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_BUFFER_READ_OVERFLOW,
                                    "Xilinx bit image file %s buffer read overflow: %u - 16 > %u", fileName, wordCount, pFlashInfo->BufferCount);
                                goto ErrorExit;
                            }
                            /* Overlapping move so memcpy produces unpredictable results, use memmove instead: */
                            memmove(readWordBuffer, readWordBuffer + 16U, (wordCount - 16U) * sizeof(readWordBuffer[0]));
                            if (16U > pFlashInfo->BufferCount)
                            {
                                //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "ERROR: buffer overflow: 16 > %u\n", wordCount);
                                errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_BUFFER_OVERFLOW,
                                    "Xilinx bit image file %s buffer overflow: %u - 16 > %u", fileName, wordCount, pFlashInfo->BufferCount);
                                goto ErrorExit;
                            }
                            numberOfItemsRead = fread(readWordBuffer + wordCount - 16U, 2, 16, pImagefile);
                            if (numberOfItemsRead == 0)
                            {
                            }
                        }
                    }
                }

                /* Swap bits when using bit file */
                if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT)
                {
                    for (i = 0; i < pFlashInfo->BufferCount; ++i)
                    {
                        readWordBuffer[i] = FL_Swap16Bits(readWordBuffer[i]);
                    }
                }

                if (writtenBytesCount == 0)
                {
                    /* This is still the beginning of the file except bit file header has already been read
                       and possibly 16 words of FFFF.
                       Check pattern at word address 24 or 40 (UltraScale) */

                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIN && readWordBuffer[24] != 0x5599)
                    {
                        //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "%s should be byte swapped\n", fileName);
                        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT_BYTES_NOT_SWAPPED,
                            "Xilinx bit image file %s should be byte swapped", fileName);
                        goto ErrorExit;
                    }
                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT && readWordBuffer[24] != 0x5599)
                    {
                        //
                        char hexDump[500];
                        size_t hexDumpLength = 0;

                        for (i = 0; i < 32; ++i)
                        {
                            if ((i % 8) == 0)
                            {
                                hexDumpLength = snprintf(&hexDump[hexDumpLength], sizeof(hexDump) - hexDumpLength, "%2X: ", i);
                            }
                            hexDumpLength = snprintf(&hexDump[hexDumpLength], sizeof(hexDump) - hexDumpLength, "%04X ", readWordBuffer[i]);
                            if ((i % 8) == 7)
                            {
                                hexDumpLength = snprintf(&hexDump[hexDumpLength], sizeof(hexDump) - hexDumpLength, "\n");
                            }
                        }

                        //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Wrong value in bitfile at offset 24. Expected value 0x5599.\n");
                        //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "Dump of the first 32 16-bit words after bitfile header:\n");
                        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_UNEXPECTED_VALUE,
                            "Expected value 0x5599 at Xilinx image bit file %s offset 24. "
                            "Dump of the first 32 16 - bit words after bitfile header:\n%s\n", fileName, hexDump);
                        goto ErrorExit;
                    }
                }
                for (i = wordCount; i < pFlashInfo->BufferCount; ++i)
                {
                    readWordBuffer[i] = 0; /* Fill unused part of buffer with 0 */
                }

                if ((writtenBytesCount % (1 << 15)) == 0) /* Print progress. */
                {
                    //prn(LOG_SCR | LEVEL_INFO, "Writing flash memory: %d %%  \r", (writtenBytesCount * 100L) / fileSize);
                    //fflush(stdout);
                    FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Writing flash memory: %ld %%  \r", (writtenBytesCount * 100L) / fileSize);
                }

                FL_OnErrorGotoExit(FL_WriteFlashWords(pFlashInfo, flashWriteWordAddress, readWordBuffer, pFlashInfo->BufferCount));

                flashWriteWordAddress += pFlashInfo->BufferCount;
                writtenBytesCount += pFlashInfo->BufferCount * 2;
            }
        } while (wordCount == pFlashInfo->BufferCount);
    }

    FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Writing flash memory: done        \n");

ErrorExit:

    if (fclose(pImagefile) != 0)
    {
        FL_Error latestErrorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close image file %s with errno %d", fileName, errno);
        errorCode = errorCode != FL_SUCCESS ? errorCode : latestErrorCode;
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

// ActelVerifyImage
FL_Error FL_FlashVerifyImage(
    const FL_Parameters *   pParameters,
    const FL_FlashInfo *    pFlashInfo,
    const char *            fileName,
    FL_ImageTarget          imageTarget,
    const char *            expectedPartNumber)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FILE * pImagefile;
    struct stat fileStatistics;
    long fileSize;
    int numberOfWordsRead;
    uint16_t readFileWord;//Buffer data read from file.
    uint32_t readFlashAddress = pFlashInfo->ImageStartAddress[imageTarget];
    uint32_t verifiedFlashWordsCount = 0;
    int numberOfErrors = 0;
    int i;
    FL_Error errorCode = FL_SUCCESS;

    if (pFlashInfo->SectorsPerImage[imageTarget] == 0)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_NO_FLASH_SPACE_RESERVED_FOR_IMAGE, "No flash space reserved for image #%u\n", imageTarget);
    }

    pImagefile = fopen(fileName, "rb");
    if (pImagefile == NULL)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE,
            "Failed to open Xilinx bit image file %s with errno %d", fileName, errno);
    }
    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT)
    {
        FL_ExitOnError(pLogContext, FL_FileReadXilinxBitFileHeader2(pFlashInfo, pImagefile, expectedPartNumber, -1));
    }
    else
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT,
            "Verify againts file format %d not supported", pParameters->FlashFileFormat);
    }
    if (fstat(fileno(pImagefile), &fileStatistics) != 0) /* Get file statistics */
    {
        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FSTAT_FAILED,
            "system function fstat failed on Xilinx bit image file %s, errno %d",
            fileName, errno);
        goto ErrorExit;
    }
//printf("ftell(pImagefile) = %ld\n", ftell(pImagefile)); fclose(pImagefile); exit(1); // TODO remove
    fileSize = fileStatistics.st_size - ftell(pImagefile);
    if (fileSize > pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize)
    {
        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IMAGE_FILE_TOO_BIG,
            "Xilinx bit image file %s is too big: %lu > %u",
            fileName, fileSize, pFlashInfo->SectorsPerImage[imageTarget] * pFlashInfo->MiddleSectorSize);
        goto ErrorExit;
    }
    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_SPI)
    {
        if (fileSize % 64 != 0) /* SPI files are apparently a multiple of 64 bytes long */
        {
            //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "Wrong spi file size: %s\n", fileName);
            errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IMAGE_FILE_WRONG_SIZE,
                "Xilinx bit image file %s is wrong size: %lu %% 64 != 0", fileName, fileSize);
            goto ErrorExit;
        }
    }

    //prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Verifying FPGA image\r");
    FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Verifying FPGA image\r");

    if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
    {
        errorCode = FL_QSPI_VerifyFile(pFlashInfo, 0, 0, fileName, pImagefile);
        if (errorCode != FL_SUCCESS)
        {
            numberOfErrors = 1;
        }
    }
    else
    {
        if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_SPI)
        {
            /* TODO: SPI file validation */
        }
        else if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT)
        {
            //printf("pParameters->FlashFileFormat %u\n", pParameters->FlashFileFormat);
            //printf("there\n");
            if (pFlashInfo->Type == FL_FLASH_TYPE_MICRON_NOR)
            {
                uint8_t flashReadBuffer[256], fileReadBuffer[sizeof(flashReadBuffer)];
                uint32_t flashAddress;
                //numberOfWordsRead = fread(&readFileWord, 2, 1, pImagefile);
                for (flashAddress = 0; flashAddress < pFlashInfo->FlashSize; flashAddress += sizeof(flashReadBuffer))
                {
                    size_t numberOfFileBytesRead;
                    uint32_t numberOfFlashBytesRead;
                    bool showProgress = false;

                    numberOfFileBytesRead = fread(&fileReadBuffer, 1, sizeof(fileReadBuffer), pImagefile);
                    if (ferror(pImagefile) != 0)
                    {
                        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_IMAGE_FILE_READ_ERROR,
                            "Read from image file %s failed with errno %d", fileName, errno);
                    }

                    FL_ExitOnError(pLogContext, FL_QSPI_ReadBytes(pFlashInfo, flashAddress, flashReadBuffer, numberOfFileBytesRead, &numberOfFlashBytesRead));

                    if (memcmp(flashReadBuffer, fileReadBuffer, numberOfFileBytesRead) != 0)
                    {
                        fprintf(stderr, " **** DIFFERENT!\n");
                        printf("flashAddress = %u, numberOfBytesRead = %lu\n", flashAddress, numberOfFileBytesRead);

                        printf("file:\n");
                        FL_HexDump(NULL, fileReadBuffer, sizeof(fileReadBuffer), 16, true);
                        printf("flash:\n");
                        FL_HexDump(NULL, flashReadBuffer, sizeof(flashReadBuffer), 16, true);

                        fclose(pImagefile);
                        exit(EXIT_FAILURE);
                    }

                    if (feof(pImagefile) != 0)
                    {
                        showProgress = true;
                        break; /* End of file (EOF) reached */
                    }

                    verifiedFlashWordsCount += numberOfFileBytesRead;
                    showProgress |= (verifiedFlashWordsCount % (32 * 1024)) == 0;
                    if (showProgress)
                    {
                        //prn(LOG_SCR | LEVEL_INFO, "Verifying flash memory: %d %%  \r", (VerCount * 100L) / fsize);
                        FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Verifying flash memory: %u %% (%u KiB)  \r",
                            (uint32_t)((verifiedFlashWordsCount * 100L) / fileSize), verifiedFlashWordsCount / 1024U);
                    }
                }
            }
            else
            {
                do
                {
                    /*
                       Read 2 bytes from file
                       Check the first 32 bytes. They should be 0xFF
                       In UltraScale it is the first 64 bytes, but we only check the first 32 bytes.
                    */
                    numberOfWordsRead = fread(&readFileWord, 2, 1, pImagefile);

                    if (verifiedFlashWordsCount < 32)
                    {
                        if (readFileWord != 0xFFFF)
                        {
                            //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "%s is not a valid file. The bit stream should start with 32 x 0xFF\n", fileName);
                            errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT,
                                "%s is not a valid bit image file. The bit stream should start with 32 x 0xFF.", fileName);
                            goto ErrorExit;
                        }
                    }
                    if (verifiedFlashWordsCount == 32 && readFileWord == 0xFFFF)
                    {
                        /* Skip the extra 16 words in file header with 0xFFFF and read a new word from the file */
                        for (i = 0; i < 16; i++)  numberOfWordsRead = fread(&readFileWord, 2, 1, pImagefile);
                    }
                    /* Swap bits when using bit file */
                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT)
                    {
                        readFileWord = FL_Swap16Bits(readFileWord);
                    }
                    /* Check pattern at word address 24 or 40 (UltraScale) */
                    if (pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIT || pParameters->FlashFileFormat == FL_FLASH_FILE_FORMAT_BIN)
                    {
                        if (verifiedFlashWordsCount == 48)
                        {
                            if (readFileWord != 0x5599)
                            {
                                //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "%s should be byte swapped\n", fileName);
                                errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT_BYTES_NOT_SWAPPED,
                                    "Xilinx bit image file %s should be byte swapped", fileName);
                                goto ErrorExit;
                            }
                        }
                    }

                    //readBuffer = FL_SwapWordBytes(readBuffer);
                    if (numberOfWordsRead == 1)
                    {
                        uint16_t readFlashWord = 0xDEAD;
                        uint32_t totalReadErrorsSeen = 0;

                        if (pFlashInfo->FlashCommandSet == FL_FLASH_COMMAND_SET_SF2)
                        {
                            if ((readFlashAddress & (pFlashInfo->BufferCount - 1)) == 0) // TODO: what is this???
                            {
                                FL_ExitOnError(pLogContext, FL_AdminFillReadBuffer(pFlashInfo, readFlashAddress)); /* Fill read buffer in SF2 */
                            }
                            /* Read data from flash memory */
                            FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, readFlashAddress, &readFlashWord));
                            numberOfErrors = (readFlashWord != readFileWord);
                            /* The current design of the read buffer in SF2 is not well suited for repeated read of the same address. */
                        }
                        else
                        {
                            numberOfErrors = 0;
                            errorCode = FL_AdminReadWord(pFlashInfo, readFlashAddress, &readFlashWord);
                            do
                            {
                                if (errorCode == FL_SUCCESS && readFlashWord == readFileWord)
                                {
                                    /* Successfull read, start errors count from zero */
                                    numberOfErrors = 0;
                                }
                                else
                                {
                                    /* Failed read, increment counters and try again */
                                    ++numberOfErrors;
                                    ++totalReadErrorsSeen;
                                    errorCode = FL_AdminReadWord(pFlashInfo, readFlashAddress, &readFlashWord);
                                }
                                /* Read 5 times if there is an error to make sure that the error exist */
                            } while ((numberOfErrors > 0) && (numberOfErrors < 5));
                        }
                        if (numberOfErrors > 0)
                        {
                            //prn(LOG_SCR | LOG_FILE | LEVEL_WARNING, "\nError at address 0x%06X. Flash Data: 0x%04X. File Data: 0x%04X\n", readFlashAddress, readFlashWord, readFileWord);
                            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_VERIFY_WORD_DATA_AGAINST_FILE_FAILED,
                                "Verify word data against file %s failed at flash word address 0x%04X: flash word 0x%04X, file word 0x%04X",
                                fileName, readFlashAddress, readFlashWord, readFileWord);
                        }
                        if (totalReadErrorsSeen > 0)
                        {
                            //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "\n *** %d read errors seen before success at address 0x%06X. Flash Data: 0x%04X. File Data: 0x%04X\n",
                            //    totalReadErrorsSeen, readFlashAddress, readFlashWord, readFileWord);
                            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FLASH_VERIFY_WORD_DATA_AGAINST_FILE_TOTAL_ERRORS,
                                "Verify word data against file %s succeeded at flash word address 0x%04X: "
                                "flash word 0x%04X, file word 0x%04X after seeing %u total errors",
                                fileName, readFlashAddress, readFlashWord, readFileWord, totalReadErrorsSeen);
                        }
                    }
                    if ((verifiedFlashWordsCount % (1 << 15)) == 0) /* Show progress. */
                    {
                        //prn(LOG_SCR | LEVEL_INFO, "Verifying flash memory: %d %%  \r", (VerCount * 100L) / fsize);
                        FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Verifying flash memory: %u %%  \r",
                            (uint32_t)((verifiedFlashWordsCount * 100L) / fileSize));
                    }
                    ++readFlashAddress;
                    verifiedFlashWordsCount += 2;
                } while ((numberOfWordsRead == 1) && (numberOfErrors == 0));
            }
        }
        else
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_FORMAT,
                "Flash verify not supported for file type %d", pParameters->FlashFileFormat);
        }
    }

    if (numberOfErrors == 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "Verifying FPGA image: done with no errors      \n");
        FL_Log(pLogContext, FL_LOG_LEVEL_INFO, "Verifying FPGA image: done with no errors      \n");
    }


ErrorExit:

    if (fclose(pImagefile) != 0)
    {
        FL_Error latestErrorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close image file %s with errno %d", fileName, errno);
        errorCode = errorCode != FL_SUCCESS ? errorCode : latestErrorCode;
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

// EraseAndFlashAndVerify
FL_Error FL_FlashEraseAndWriteAndVerify(FL_FlashLibraryContext * pFlashLibraryContext)
{
    const FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_ImageTarget imageTarget;
    FL_Error errorCode = FL_SUCCESS;
    bool failsafeSelected[FL_IMAGE_TARGET_NUMBER_OF_TARGETS];
    bool flashProtected[FL_IMAGE_TARGET_NUMBER_OF_TARGETS];

    if (pParameters->CurrentInitializationLevel != pParameters->RequiredInitializationLevel)
    {
        FL_ExitOnError(pLogContext, FL_OpenFlashLibrary(pFlashLibraryContext));
    }

    //The first 2 are separate functions that do not need the rest of functionality
    if (pParameters->CalculateClockCalibrationImageMD5Sum)
    {
        uint8_t md5sum[FL_MD5_HASH_SIZE];

        FL_Error errorCode = FL_CalculateFlashImageMD5Sum(pFlashInfo, FL_IMAGE_TARGET_CLOCK_CALIBRATION, md5sum);
        if (errorCode != FL_SUCCESS)
        {
            return errorCode;
        }
        FL_LogMD5Sum("Calculate Clkcal image", pFlashInfo, md5sum);

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    };

    if (pParameters->ReadPcbVersion)
    {
        uint16_t pcbVersion;
        if (pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
            pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
        {
            FL_ExitOnError(pLogContext, FL_GeckoReadPcbVersion(pFlashInfo, &pcbVersion));
        }
        else
        {
            FL_Error errorCode = FL_ReadPcbVersion(pFlashInfo, &pcbVersion);
            if (errorCode != FL_SUCCESS)
            {
                return errorCode;
            }
        }
        //prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "PCB version: %d\n", pcb_ver);
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "PCB version: %u\n", pcbVersion);

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    if (pParameters->PCIeBootModeReadOrWrite)
    {
        if (pParameters->PCIeBootMode != FL_PCIE_BOOT_MODE_NONE)
        {
            FL_Error errorCode = FL_WritePciBootMode(pFlashInfo, pParameters->PCIeBootMode);
            if (errorCode != FL_SUCCESS)
            {
                return errorCode;
            }
            //prn(LOG_SCR|LOG_FILE|LEVEL_INFO, "PCI boot mode set to: %s\n", pciBootModes[pci_boot_mode - 1]);
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "PCIe boot mode set to: %s\n", FL_GetPCIeBootModeName(pParameters->pErrorContext, pParameters->PCIeBootMode, false));
        }
        else
        {
            const char * pciBooModeName = NULL;
            uint16_t pciBootMode;
            FL_Error errorCode = FL_ReadPciBootMode(pFlashInfo, &pciBootMode);
            if (errorCode != FL_SUCCESS)
            {
                return errorCode;
            }
            if ((pciBooModeName = FL_GetPCIeBootModeName(pParameters->pErrorContext, pciBootMode, true)) != NULL)
            {
                //prn(LOG_SCR | LOG_FILE | LEVEL_INFO, "PCI boot mode: %s\n", pciBootModes[boot_mode - 1]);
                FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "PCIe boot mode: %s\n", pciBooModeName);
            }
        }

        return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
    }

    for (imageTarget = 0; imageTarget < FL_IMAGE_TARGET_NUMBER_OF_TARGETS; ++imageTarget)
    {
        failsafeSelected[imageTarget] = false;
        flashProtected[imageTarget] = true;
    }

    /* Loop through images, Herculaneum has extra Clock Calibration preload image (imageTarget == FL_IMAGE_TARGET_CLOCK_CALIBRATION) */
    for (imageTarget = 0; imageTarget < FL_IMAGE_TARGET_NUMBER_OF_TARGETS; ++imageTarget)
    {
        bool doSomething = pParameters->FlashErase[imageTarget] || pParameters->FlashFileName[imageTarget] != NULL || pParameters->FlashVerifyFileName[imageTarget] != NULL;

        if (doSomething)
        {
            if (pFpgaInfo->CardType == FL_CARD_TYPE_MANGO ||
                pFpgaInfo->CardType == FL_CARD_TYPE_PADUA ||
                pFpgaInfo->CardType == FL_CARD_TYPE_SAVONA ||
                pFpgaInfo->CardType == FL_CARD_TYPE_TIVOLI)
            {
                if (imageTarget == FL_IMAGE_TARGET_PRIMARY) /* Primary image */
                {
                    FL_ExitOnError(pLogContext, FL_SelectPrimaryImage(pFlashInfo));
                    failsafeSelected[imageTarget] = false;
                }
                else if (imageTarget == FL_IMAGE_TARGET_SECONDARY) /* Secondary fail-safe image */
                {
                    FL_ExitOnError(pLogContext, FL_SelectSecondaryImage(pFlashInfo));
                    failsafeSelected[imageTarget] = true;
                }
            }
        }

        if (pParameters->FlashErase[imageTarget])
        {
            errorCode = FL_FlashEraseImage(pFlashInfo, imageTarget, pParameters->FlashFileName[imageTarget]);
            if (errorCode != FL_SUCCESS)
            {
                break;
            }
        }

        if (pParameters->FlashFileName[imageTarget] != NULL)
        {
            /* Remove protection (should only be set on Clock Calibration preload image, but just in case...) */
            FL_ExitOnError(pLogContext, FL_FlashUnprotectImage(pFlashInfo, imageTarget));
            flashProtected[imageTarget] = false;

            /* Erase Image */
            errorCode = FL_FlashEraseImage(pFlashInfo, imageTarget, pParameters->FlashFileName[imageTarget]);
            if (errorCode != FL_SUCCESS)
            {
                break;
            }
            /* Write image */
            errorCode = FL_FlashWriteImage(pParameters, pFlashInfo, pParameters->FlashFileName[imageTarget], imageTarget, pFpgaInfo->PartNumber);
            if (errorCode != FL_SUCCESS)
            {
                break;
            }
        }

        if (pParameters->FlashVerifyFileName[imageTarget] != NULL)
        {
            /* Verify image */
            errorCode = FL_FlashVerifyImage(pParameters, pFlashInfo, pParameters->FlashVerifyFileName[imageTarget], imageTarget, pFpgaInfo->PartNumber);
            if (errorCode != FL_SUCCESS)
            {
                break;
            }
        }
    }

    for (imageTarget = 0; imageTarget < FL_IMAGE_TARGET_NUMBER_OF_TARGETS; ++imageTarget)
    {
        if (!flashProtected[imageTarget])
        {
            /* Protect Clock Calibration preload image. */
            /* Other image types are left unprotected even if they were protected to start with. */
            if ((imageTarget == FL_IMAGE_TARGET_CLOCK_CALIBRATION) && pParameters->FlashFileName[FL_IMAGE_TARGET_CLOCK_CALIBRATION] != NULL)
            {
                errorCode = errorCode == FL_SUCCESS ? FL_FlashProtectImage(pFlashInfo, imageTarget) : errorCode;
            }
        }

        if (failsafeSelected[imageTarget])
        {
            errorCode = errorCode == FL_SUCCESS ? FL_SelectPrimaryImage(pFlashInfo) : errorCode;
        }
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

FL_Error FL_FileReadXilinxBitFileHeader1(const FL_FlashInfo * pFlashInfo, const char * fileName, const char * expectedPartNumber, uint32_t expectedUserid)
{
    FL_Error errorCode = FL_SUCCESS;

    FILE * pFile = fopen(fileName, "rb");
    if (pFile == NULL)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE,
            "Failed to open image file %s with errno %d", fileName, errno);
    }

    errorCode = FL_FileReadXilinxBitFileHeader2(pFlashInfo, pFile, expectedPartNumber, expectedUserid);

    if (fclose(pFile) != 0)
    {
        FL_Error latestErrorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close image file %s with errno %d", fileName, errno);
        errorCode = errorCode != FL_SUCCESS ? errorCode : latestErrorCode;
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

// flash_read_bit_file_header
FL_Error FL_FileReadXilinxBitFileHeader2(
    const FL_FlashInfo *    pFlashInfo,
    FILE *                  pFile,
    const char *            currentFpgaPartNumber,
    FL_FpgaType             expectedBitFileFpgaType)
{
    size_t numberOfFieldBytes;
    char data[200];
    const char * userIdString;
    uint32_t fieldLength;

    /* Read field 1 length */
    numberOfFieldBytes = fread(data, 1, 2, pFile);
    if (numberOfFieldBytes != 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 1 length unexpected EOF: %lu != %u", numberOfFieldBytes, 2);
    }
    fieldLength = (data[0] << 8) + data[1];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 1 len %u\n", len);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 1 len %u\n", fieldLength);
    if (fieldLength != 9)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_1_LENGTH, "Xilinx bit file header field 1 invalid: length != 9");
    }
    /* Read field 1 data */
    if (fieldLength > sizeof(data))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW,
            "Xilinx bit file header field 1 buffer overflow: %u > %lu", fieldLength, sizeof(data));
    }
    numberOfFieldBytes = fread(data, 1, fieldLength, pFile);
    if (numberOfFieldBytes != fieldLength)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 1 data unexpected EOF: %lu != %u", numberOfFieldBytes, fieldLength);
    }

    /* Read field 2 length */
    numberOfFieldBytes = fread(data, 1, 2, pFile);
    if (numberOfFieldBytes != 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 2 length unexpected EOF: %lu != %u", numberOfFieldBytes, 2);
    }
    fieldLength = (data[0] << 8) + data[1];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 2 len %u\n", len);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 2 len %u\n", fieldLength);
    if (fieldLength != 1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_2_LENGTH,
            "Xilinx bit file header field 2 invalid: length != 1");
    }
    // Read field 2 data
    if (fieldLength > sizeof(data))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW,
            "Xilinx bit file header field 2 buffer overflow: %u > %lu", fieldLength, sizeof(data));
    }
    numberOfFieldBytes = fread(data, 1, fieldLength, pFile);
    if (numberOfFieldBytes != fieldLength)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 2 data unexpected EOF: %lu != %u", numberOfFieldBytes, fieldLength);
    }
    if (data[0] != 0x61)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_2_KEY,
            "Xilinx bit file header field 2 invalid: key 0x%02X != expected 0x61 ('a')", data[0]);
    }

    /* Read field 3 length */
    numberOfFieldBytes = fread(data, 1, 2, pFile);
    if (numberOfFieldBytes != 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 3 length unexpected EOF: %lu != %u", numberOfFieldBytes, 2);
    }
    fieldLength = (data[0] << 8) + data[1];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 3 len %u\n", len);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 3 len %u\n", fieldLength);
    /* Read field 3 data */
    if (fieldLength > sizeof(data))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW,
            "Xilinx bit file header field 3 buffer overflow: %u > %lu", fieldLength, sizeof(data));
    }
    numberOfFieldBytes = fread(data, 1, fieldLength, pFile);
    if (numberOfFieldBytes != fieldLength)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 3 data unexpected EOF: %lu != %u", numberOfFieldBytes, fieldLength);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 3: %s\n", data);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 3: %s\n", data);
    /* Locate UserID */
    userIdString = strstr(data, "UserID=");
    if (userIdString != NULL)
    {
        /* Header field UserID containts the FPGA type of the image or 0xFFFFFFFF for no type. */
        uint32_t bitFileFpgaType = strtoul(userIdString + 7, NULL, 16);
        //prn(LOG_SCR|LEVEL_DEBUG, "UserID: %s\n", userid_s+7);
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "UserID: %s\n", userIdString + 7);
        if (bitFileFpgaType != 0)
        {
            //prn(LOG_SCR|LEVEL_DEBUG, "UserID as integer: 0x%lx\n", userid);
            FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "UserID as integer: 0x%X (%d)\n", bitFileFpgaType, (int32_t)bitFileFpgaType);
        }
        if ((bitFileFpgaType != 0) && (bitFileFpgaType != 0xFFFFFFFF) &&
            (expectedBitFileFpgaType != 0xFFFFFFFF) && (bitFileFpgaType != expectedBitFileFpgaType))
        {
            FL_Error isUpradable = FL_IsUpgradableFpgaType(pFlashInfo->pErrorContext, expectedBitFileFpgaType, bitFileFpgaType);
            if (isUpradable != FL_SUCCESS)
            {
                return isUpradable;
            }
        }
    }

    /* Read field 4 key */
    numberOfFieldBytes = fread(data, 1, 1, pFile);
    if (numberOfFieldBytes != 1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 1 key unexpected EOF: %lu != %u", numberOfFieldBytes, 1);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 4 key %c\n", data[0]);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 4 key %c\n", data[0]);
    if (data[0] != 0x62)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_KEY,
            "Xilinx bit file header field 4 invalid: key 0x%02X != expected 0x62 ('b')", data[0]);
    }
    /* Read field 4 length */
    numberOfFieldBytes = fread(data, 1, 2, pFile);
    if (numberOfFieldBytes != 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 4 length unexpected EOF: %lu != %u", numberOfFieldBytes, 2);
    }
    fieldLength = (data[0] << 8) + data[1];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 4 len %u\n", len);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 4 len %u\n", fieldLength);
    /* Read field 4 data */
    if (fieldLength > sizeof(data))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW,
            "Xilinx bit file header field 4 buffer overflow: %u > %lu", fieldLength, sizeof(data));
    }
    numberOfFieldBytes = fread(data, 1, fieldLength, pFile);
    if (numberOfFieldBytes != fieldLength)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 4 data unexpected EOF: %lu != %u", numberOfFieldBytes, fieldLength);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 4: %s\n", data);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 4: %s\n", data);
    /* Check part name if it exists */
    if (currentFpgaPartNumber != NULL && (strlen(currentFpgaPartNumber) > 0))
    {
        if (strncmp(data, currentFpgaPartNumber, 32) != 0)
        {
            return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_4_PART_NUMBER,
                "Xilinx bit file header field 4 invalid: "
                "file FPGA part number %s different from current FPGA part number %s. "
                "FPGA models don't match.", data, currentFpgaPartNumber);
        }
    }

    /* Read field 5 key */
    numberOfFieldBytes = fread(data, 1, 1, pFile);
    if (numberOfFieldBytes != 1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 5 key unexpected EOF: %lu != %u", numberOfFieldBytes, 1);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 5 key %c\n", data[0]);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 5 key %c\n", data[0]);
    if (data[0] != 0x63)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_5_KEY,
            "Xilinx bit file header field 5 invalid: key 0x%02X != expected 0x63 ('c')", data[0]);
    }
    /* Read field 5 length */
    numberOfFieldBytes = fread(data, 1, 2, pFile);
    if (numberOfFieldBytes != 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 5 length unexpected EOF: %lu != %u", numberOfFieldBytes, 2);
    }
    fieldLength = (data[0] << 8) + data[1];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 5 len %u\n", len);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 5 len %u\n", fieldLength);
    /* Read field 5 data */
    if (fieldLength > sizeof(data))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW,
            "Xilinx bit file header field 5 buffer overflow: %u > %lu", fieldLength, sizeof(data));
    }
    numberOfFieldBytes = fread(data, 1, fieldLength, pFile);
    if (numberOfFieldBytes != fieldLength)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 5 data unexpected EOF: %lu != %u", numberOfFieldBytes, fieldLength);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 5: %s\n", data);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 5: %s\n", data);

    /* Read field 6 key */
    numberOfFieldBytes = fread(data, 1, 1, pFile);
    if (numberOfFieldBytes != 1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 6 key unexpected EOF: %lu != %u", numberOfFieldBytes, 1);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 6 key %c\n", data[0]);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 6 key %c\n", data[0]);
    if (data[0] != 0x64)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_6_KEY,
            "Xilinx bit file header field 6 invalid: key 0x%02X != expected 0x64 ('d')", data[0]);
    }
    /* Read field 6 length */
    numberOfFieldBytes = fread(data, 1, 2, pFile);
    if (numberOfFieldBytes != 2)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 6 length unexpected EOF: %lu != %u", numberOfFieldBytes, 2);
    }
    fieldLength = (data[0] << 8) + data[1];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 6 len %u\n", len);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 6 len %u\n", fieldLength);
    /* Read field 6 data */
    if (fieldLength > sizeof(data))
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_OVERFLOW,
            "Xilinx bit file header field 6 buffer overflow: %u > %lu", fieldLength, sizeof(data));
    }
    numberOfFieldBytes = fread(data, 1, fieldLength, pFile);
    if (numberOfFieldBytes != fieldLength)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 6 data unexpected EOF: %lu != %u", numberOfFieldBytes, fieldLength);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 6: %s\n", data);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 6: %s\n", data);

    /* Read field 7 key */
    numberOfFieldBytes = fread(data, 1, 1, pFile);
    if (numberOfFieldBytes != 1)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 7 key unexpected EOF: %lu != %u", numberOfFieldBytes, 1);
    }
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 7 key %c\n", data[0]);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 7 key %c\n", data[0]);
    if (data[0] != 0x65)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_INVALID_BIT_FILE_HEADER_FIELD_7_KEY,
            "Xilinx bit file header field 7 invalid: key 0x%02X != expected 0x65 ('e')", data[0]);
    }
    /* Read field 7 length */
    numberOfFieldBytes = fread(data, 1, 4, pFile);
    if (numberOfFieldBytes != 4)
    {
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_BIT_FILE_HEADER_BUFFER_UNEXPECTED_EOF,
            "Xilinx bit file header field 7 length unexpected EOF: %lu != %u", numberOfFieldBytes, 4);
    }
    fieldLength = (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3];
    const char * pData = &data[0];
    //prn(LOG_SCR|LEVEL_DEBUG, "Field 7 len %u %02X %02X %02X %02X\n", len, data[0], data[1], data[2], data[3]);
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Field 7 len %u %08X\n", fieldLength,
        *(const uint32_t *)pData);

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

FL_Error FL_IsUpgradableFpgaType(FL_ErrorContext * pErrorContext, FL_FpgaType existingFpgaType, FL_FpgaType newFpgaType)
{
    /*
       Now multiple entries with the same key is allowed because we have single test image to multiple other images.
       Key can be upgraded to user ids without force-programming.
    */

    /**
     *  An entry that describes which other FPGA types can be upgraded or replace an FPGA image of type @ref FL_FpgaType.
     *  Each entry can contain up to 2 replacing FPGA type definitions in fields NewFpgaType1 or NewFpgaType2.
     *  A value of -1 means there is no matching FPGA type that can upgrade or replace the FPGA type in @ref FL_FpgaType.
     *  An image bit file header field "UserID" contains the FPGA type or 0xFFFFFFFF for no FPGA type defined.
     *  This upgradability check is done in the API function FL_FileReadXilinxBitFileHeader2.
     *  The check can be overridden with a force programming option.
     */
    static struct
    {
        FL_FpgaType     ExistingFpgaType;       /**< Existing FPGA type is being upgraded or replaced. */
        FL_FpgaType     NewFpgaType1;           /**< This FPGA type can upgrade or replace the ExistingFpgaType FPGA type
                                                      or -1 for no match. */
        FL_FpgaType     NewFpgaType2;           /**< This FPGA type can upgrade or replace the ExistingFpgaType FPGA type
                                                      or -1 for no match. */
    } s_UpgradableFpgaTypes[] =
    {
        { FPGA_TYPE_LUXG_PROBE, FPGA_TYPE_LUXG_TEST, -1 },
        { FPGA_TYPE_ESSEN, -1, -1 },
        { FPGA_TYPE_ANCONA_CHEMNITZ, FPGA_TYPE_ANCONA_TEST, -1 },
        { FPGA_TYPE_FIRENZE_CHEMNITZ, FPGA_TYPE_FIRENZE_TEST, -1 },
        { FPGA_TYPE_ANCONA_AUGSBURG, FPGA_TYPE_ANCONA_TEST, -1 },
        { FPGA_TYPE_HERCULANEUM_TEST, FPGA_TYPE_HERCULANEUM_AUGSBURG, -1 },
        { FPGA_TYPE_HERCULANEUM_AUGSBURG, FPGA_TYPE_HERCULANEUM_TEST, -1 },
        { FPGA_TYPE_LIVORNO_TEST, FPGA_TYPE_LIVORNO_AUGSBURG, FPGA_TYPE_LIVORNO_FULDA },
        { FPGA_TYPE_LIVORNO_TEST, FPGA_TYPE_LIVORNO_AUGSBURG_40, -1 },
        { FPGA_TYPE_LIVORNO_TEST_40, FPGA_TYPE_LIVORNO_AUGSBURG_40, -1 },
        { FPGA_TYPE_LIVORNO_TEST_40, FPGA_TYPE_LIVORNO_AUGSBURG, -1 },
        { FPGA_TYPE_LIVORNO_AUGSBURG, FPGA_TYPE_LIVORNO_TEST, -1 },
        { FPGA_TYPE_LIVORNO_AUGSBURG, FPGA_TYPE_LIVORNO_AUGSBURG_40, -1 },
        { FPGA_TYPE_LIVORNO_AUGSBURG_40, FPGA_TYPE_LIVORNO_TEST_40, -1 },
        { FPGA_TYPE_LIVORNO_AUGSBURG_40, FPGA_TYPE_LIVORNO_AUGSBURG, -1 },
        { FPGA_TYPE_LIVORNO_FULDA, FPGA_TYPE_LIVORNO_TEST, -1 },
        { FPGA_TYPE_LIVORNO_LEONBERG, FPGA_TYPE_LIVORNO_TEST, -1 },
        { FPGA_TYPE_GENOA_TEST, FPGA_TYPE_GENOA_AUGSBURG, -1 },
        { FPGA_TYPE_GENOA_AUGSBURG, FPGA_TYPE_GENOA_TEST, -1 },
        { FPGA_TYPE_LIVIGNO_TEST, FPGA_TYPE_LIVIGNO_AUGSBURG, FPGA_TYPE_LIVIGNO_FULDA_V7330 },
        { FPGA_TYPE_LIVIGNO_AUGSBURG, FPGA_TYPE_LIVIGNO_TEST, -1 },
        { FPGA_TYPE_LIVIGNO_AUGSBURG_4X1, FPGA_TYPE_LIVIGNO_TEST, -1 },
        { FPGA_TYPE_LIVIGNO_FULDA_V7330, FPGA_TYPE_LIVIGNO_TEST, -1 },
        { FPGA_TYPE_LIVIGNO_LEONBERG_V7330, FPGA_TYPE_LIVIGNO_TEST, -1 },
        { FPGA_TYPE_MILAN_AUGSBURG, FPGA_TYPE_MILAN_TEST, -1 },
        { FPGA_TYPE_SILIC100_AUGSBURG, -1, -1 },
        { FPGA_TYPE_LATINA_TEST, FPGA_TYPE_LATINA_AUGSBURG, -1 },
        { FPGA_TYPE_LATINA_AUGSBURG, FPGA_TYPE_LATINA_TEST, -1 },
        { FPGA_TYPE_LATINA_LEONBERG, FPGA_TYPE_LATINA_TEST, -1 },
        { FPGA_TYPE_LUXG_TEST, FPGA_TYPE_LUXG_PROBE, -1 },
        { FPGA_TYPE_ANCONA_TEST, FPGA_TYPE_ANCONA_CHEMNITZ, FPGA_TYPE_ANCONA_AUGSBURG },
        { FPGA_TYPE_ERCOLANO_TEST, FPGA_TYPE_ERCOLANO_DEMO, -1 },
        { FPGA_TYPE_ERCOLANO_DEMO, FPGA_TYPE_ERCOLANO_TEST, -1 },
        { FPGA_TYPE_FIRENZE_TEST, FPGA_TYPE_FIRENZE_CHEMNITZ, -1 },
        { FPGA_TYPE_MILAN_TEST, FPGA_TYPE_MILAN_AUGSBURG, -1 },
        { FPGA_TYPE_PISA_TEST, -1, -1 },
        { FPGA_TYPE_MARSALA_TEST, -1, -1 },
        { FPGA_TYPE_MONZA_TEST, -1, -1 },
        { FPGA_TYPE_BERGAMO_TEST, -1, -1 },
        { FPGA_TYPE_COMO_TEST, -1, -1 },
        { FPGA_TYPE_2022, -1, -1 },
        { FPGA_TYPE_2015, -1, -1 },
        { FPGA_TYPE_2010, -1, -1 },
        { FPGA_TYPE_ATHEN_TEST, -1, -1 },
        { FPGA_TYPE_LUCCA_LEONBERG_V7690, FPGA_TYPE_LUCCA_TEST, -1 },
        { FPGA_TYPE_LUCCA_TEST, -1, -1 },
        { FPGA_TYPE_TORINO_TEST, -1, -1 },
        { FPGA_TYPE_LUCCA_TEST, FPGA_TYPE_LUCCA_AUGSBURG, -1 },
        { FPGA_TYPE_LUCCA_TEST, FPGA_TYPE_LUCCA_AUGSBURG_40, -1 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_AUGSBURG_16X10, -1 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_AUGSBURG_40, -1 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_AUGSBURG_100, -1 },
        { FPGA_TYPE_MANGO_AUGSBURG_16X10, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_AUGSBURG_2X25 },
        { FPGA_TYPE_MANGO_AUGSBURG_40, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_AUGSBURG_2X25 },
        { FPGA_TYPE_MANGO_AUGSBURG_100, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_AUGSBURG_2X25 },
        { FPGA_TYPE_MANGO_AUGSBURG_16X10, FPGA_TYPE_MANGO_AUGSBURG_40, FPGA_TYPE_MANGO_AUGSBURG_100 },
        { FPGA_TYPE_MANGO_AUGSBURG_40, FPGA_TYPE_MANGO_AUGSBURG_16X10, FPGA_TYPE_MANGO_AUGSBURG_100 },
        { FPGA_TYPE_MANGO_AUGSBURG_100, FPGA_TYPE_MANGO_AUGSBURG_40, FPGA_TYPE_MANGO_AUGSBURG_16X10 },
        { FPGA_TYPE_MANGO_AUGSBURG_2X25, FPGA_TYPE_MANGO_AUGSBURG_40, FPGA_TYPE_MANGO_AUGSBURG_16X10 },
        { FPGA_TYPE_MANGO_AUGSBURG_2X25, FPGA_TYPE_MANGO_AUGSBURG_100, FPGA_TYPE_MANGO_AUGSBURG_8X10 },
        { FPGA_TYPE_MANGO_AUGSBURG_16X10, FPGA_TYPE_MANGO_AUGSBURG_8X10, -1 },
        { FPGA_TYPE_MANGO_AUGSBURG_40,FPGA_TYPE_MANGO_AUGSBURG_8X10, -1 },
        { FPGA_TYPE_MANGO_AUGSBURG_100, FPGA_TYPE_MANGO_AUGSBURG_8X10, -1 },
        { FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_8X10, -1 },
        { FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_2X25, -1 },
        { FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_40, -1 },
        { FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_100, -1 },
        { FPGA_TYPE_PADUA_AUGSBURG_8X10, FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_2X25 },
        { FPGA_TYPE_PADUA_AUGSBURG_2X25, FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_8X10 },
        { FPGA_TYPE_PADUA_AUGSBURG_40, FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_2X25 },
        { FPGA_TYPE_PADUA_AUGSBURG_100, FPGA_TYPE_PADUA_TEST, FPGA_TYPE_PADUA_AUGSBURG_2X25 },
        { FPGA_TYPE_PADUA_AUGSBURG_8X10, FPGA_TYPE_PADUA_AUGSBURG_40, FPGA_TYPE_PADUA_AUGSBURG_100 },
        { FPGA_TYPE_PADUA_AUGSBURG_40, FPGA_TYPE_PADUA_AUGSBURG_8X10, FPGA_TYPE_PADUA_AUGSBURG_100 },
        { FPGA_TYPE_PADUA_AUGSBURG_100, FPGA_TYPE_PADUA_AUGSBURG_40, FPGA_TYPE_PADUA_AUGSBURG_8X10 },
        { FPGA_TYPE_PADUA_AUGSBURG_2X25, FPGA_TYPE_PADUA_AUGSBURG_40, FPGA_TYPE_PADUA_AUGSBURG_100 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_8X10, FPGA_TYPE_MANGO_04_AUGSBURG_16X10 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_2X25, FPGA_TYPE_MANGO_04_AUGSBURG_2X40 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_2X100, -1 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_LEONBERG_VU9P, -1 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_LEONBERG_VU9P, -1 },
        { FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_LEONBERG_VU125, -1 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_8X10, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_16X10 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_8X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X25, FPGA_TYPE_MANGO_04_AUGSBURG_2X40 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_8X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X100, -1 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_16X10, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_8X10 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_16X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X25, FPGA_TYPE_MANGO_04_AUGSBURG_2X40 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_16X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X100, -1 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X25, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_8X10 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X25, FPGA_TYPE_MANGO_04_AUGSBURG_16X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X40 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X25, FPGA_TYPE_MANGO_04_AUGSBURG_2X100, -1 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X40, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_8X10 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X40, FPGA_TYPE_MANGO_04_AUGSBURG_16X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X25 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X40, FPGA_TYPE_MANGO_04_AUGSBURG_2X100, -1 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X100, FPGA_TYPE_MANGO_TEST, FPGA_TYPE_MANGO_04_AUGSBURG_8X10 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X100, FPGA_TYPE_MANGO_04_AUGSBURG_16X10, FPGA_TYPE_MANGO_04_AUGSBURG_2X25 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X100, FPGA_TYPE_MANGO_04_AUGSBURG_2X40, -1 },
        { FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P, FPGA_TYPE_MANGO_TEST, -1 },
        { FPGA_TYPE_MANGO_04_LEONBERG_VU9P, FPGA_TYPE_MANGO_TEST, -1 },
        { FPGA_TYPE_MANGO_LEONBERG_VU9P, FPGA_TYPE_MANGO_TEST, -1 },
        { FPGA_TYPE_MANGO_LEONBERG_VU125, FPGA_TYPE_MANGO_TEST, -1 },
        { FPGA_TYPE_MANGO_WOLFSBURG_4X100G, FPGA_TYPE_MANGO_TEST, -1 },
        { FPGA_TYPE_SAVONA_AUGSBURG_2X100, FPGA_TYPE_SAVONA_TEST, FPGA_TYPE_SAVONA_AUGSBURG_2X40 },
        { FPGA_TYPE_SAVONA_AUGSBURG_2X100, FPGA_TYPE_SAVONA_AUGSBURG_2X25, FPGA_TYPE_SAVONA_AUGSBURG_8X10 },
        { FPGA_TYPE_SAVONA_AUGSBURG_2X40, FPGA_TYPE_SAVONA_TEST, FPGA_TYPE_SAVONA_AUGSBURG_2X100 },
        { FPGA_TYPE_SAVONA_AUGSBURG_2X40, FPGA_TYPE_SAVONA_AUGSBURG_2X25, FPGA_TYPE_SAVONA_AUGSBURG_8X10 },
        { FPGA_TYPE_SAVONA_AUGSBURG_8X10, FPGA_TYPE_SAVONA_TEST, FPGA_TYPE_SAVONA_AUGSBURG_2X40 },
        { FPGA_TYPE_SAVONA_AUGSBURG_8X10, FPGA_TYPE_SAVONA_AUGSBURG_2X25, FPGA_TYPE_SAVONA_AUGSBURG_2X100 },
        { FPGA_TYPE_SAVONA_AUGSBURG_2X25, FPGA_TYPE_SAVONA_TEST, FPGA_TYPE_SAVONA_AUGSBURG_2X40 },
        { FPGA_TYPE_SAVONA_AUGSBURG_2X25, FPGA_TYPE_SAVONA_AUGSBURG_2X40, FPGA_TYPE_SAVONA_AUGSBURG_2X100 },
        { FPGA_TYPE_SAVONA_TEST, FPGA_TYPE_SAVONA_AUGSBURG_2X100, FPGA_TYPE_SAVONA_AUGSBURG_2X40 },
        { FPGA_TYPE_SAVONA_TEST, FPGA_TYPE_SAVONA_AUGSBURG_2X25, FPGA_TYPE_SAVONA_AUGSBURG_8X10 },
        { FPGA_TYPE_SAVONA_WOLFSBURG_2X100G, FPGA_TYPE_SAVONA_TEST, -1 },
        { FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G, FPGA_TYPE_TIVOLI_TEST, -1 },
    };

    int i;
    /* Check for compatible FPGA types */
    for (i = sizeof(s_UpgradableFpgaTypes) / sizeof(s_UpgradableFpgaTypes[0]) - 1; --i >= 0;)
    {
        /* allow multiple items with same key */
        if (s_UpgradableFpgaTypes[i].ExistingFpgaType == existingFpgaType)
        {
            if (newFpgaType == s_UpgradableFpgaTypes[i].NewFpgaType1 ||
                newFpgaType == s_UpgradableFpgaTypes[i].NewFpgaType2)
            {
                /* Upgrade from existingFpgaType to nextFpgaType is allowed */
                return FL_SetLastErrorCode(pErrorContext, FL_SUCCESS);
            }
        }
    }

    return FL_Error(pErrorContext, FL_ERROR_FPGA_TYPES_DO_NOT_MATCH_FOR_UPGRADE,
        "Existing FPGA type 0x%02X (%s) cannot be upgraded to FPGA type 0x%02X(%s)",
        existingFpgaType, FL_GetFpgaTypeName(pErrorContext, existingFpgaType, true),
        newFpgaType, FL_GetFpgaTypeName(pErrorContext, newFpgaType, true));
}

/***************************************************************************/

// HandleCommonArguments
FL_Error FL_ParseCommandLineArguments(int argc, char * argv[], int * const pArg, FL_FlashLibraryContext * pFlashLibraryContext)
{
    FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_LogContext * pLogContext = &pFlashLibraryContext->LogContext;
    FL_ErrorContext * pErrorContext = &pFlashLibraryContext->ErrorContext;
    int argp = *pArg;
    const char * argument = argv[argp];

    pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_MAPPING;

    if (strcmp("-c", argument) == 0)
    {
        pParameters->ExternalCommandPosition = argp + 1;
        *pArg = argc; // Stop parsing
        FL_ExitOnError(pLogContext, FL_SetLibraryInitializationForCommand(pFlashLibraryContext, argv[argp + 1]));
    }
    else if (strcmp("--flash", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->FlashFileName[FL_IMAGE_TARGET_PRIMARY] = argv[argp];
            pParameters->FlashVerifyFileName[FL_IMAGE_TARGET_PRIMARY] = argv[argp];
            pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Missing image file name for '%s' option.", argument);
        }
    }
    else if (strcmp("--flashsafe", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->FlashFileName[FL_IMAGE_TARGET_SECONDARY] = argv[argp];
            pParameters->FlashVerifyFileName[FL_IMAGE_TARGET_SECONDARY] = argv[argp];
            pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Missing image file name for '%s' option.", argument);
        }
    }
    else if (strcmp("--verify", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->FlashVerifyFileName[FL_IMAGE_TARGET_PRIMARY] = argv[argp];
            pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Missing image file name for '%s' option.", argument);
        }
    }
    else if (strcmp("--verifysafe", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->FlashVerifyFileName[FL_IMAGE_TARGET_SECONDARY] = argv[argp];
            pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Missing image file name for '%s' option.", argument);
        }
    }
    else if (strcmp("--erase", argument) == 0)
    {
        pParameters->FlashErase[FL_IMAGE_TARGET_PRIMARY] = true;
        pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
    }
    else if (strcmp("--erasesafe", argument) == 0)
    {
        pParameters->FlashErase[FL_IMAGE_TARGET_SECONDARY] = true;
        pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
    }
    else if (strcmp("--force-programming", argument) == 0)
    {
        pParameters->UseForceProgramming = true;
    }
    else if (strcmp("--flash-8bit", argument) == 0)
    {
        pParameters->FlashIs8Bit = true; // TODO: move to testfpga
    }
    else if (strcmp("--skip-mmp", argument) == 0)
    {
        pParameters->SkipMemoryMapping = true;
    }
    else if(strcmp("--pcb", argument) == 0)
    {
        pParameters->ReadPcbVersion = true;
    }
    else if(strcmp("--pcibootmode", argument) == 0)
    {
        pParameters->PCIeBootModeReadOrWrite = true;
        pParameters->PCIeBootMode = FL_PCIE_BOOT_MODE_NONE; /* This means read the PCIe boot mode from the FPGA */
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            FL_PCIeBootMode i = FL_PCIE_BOOT_MODE_NONE + 1;
            for (;i <= FL_PCIE_BOOT_MODE_MAX; ++i)
            {
                if (strcmp(argv[argp], FL_GetPCIeBootModeName(pParameters->pErrorContext, i, true)) == 0)
                {
                    pParameters->PCIeBootMode = i; /* This means write the given PCIe boot mode to the FPGA */
                    break;
                }
            }
            if (pParameters->PCIeBootMode == FL_PCIE_BOOT_MODE_NONE)
            {
                pParameters->PCIeBootModeReadOrWrite = false;
                return FL_Error(pParameters->pErrorContext, FL_ERROR_INVALID_PCIE_BOOT_MODE, "PCIe boot mode not set. Unknown boot mode parameter: %s\n", argv[argp]);
            }
        }
    }
    else if (strcmp("--device", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            strncpy(pParameters->DeviceName, argv[argp], sizeof(pParameters->DeviceName));
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_DEVICE, "Missing device name for '%s' option.", argument);
        }
    }
    else if (strcmp("--debug", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->LogLevel = strtol(argv[argp], NULL, 0);
            printf("Log level set to %d (0x%X)\n", pParameters->LogLevel, pParameters->LogLevel);
        }
    }
    else if (strcmp("--flashclkcal", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->FlashFileName[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = argv[argp];
            pParameters->FlashVerifyFileName[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = argv[argp];
            pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Missing image file name for '%s' option.", argument);
        }
    }
    else if(strcmp("--verifyclkcal", argument) == 0)
    {
        argp = ++(*pArg); /* Point to next argument if any */
        if (argc > argp)
        {
            pParameters->FlashVerifyFileName[FL_IMAGE_TARGET_CLOCK_CALIBRATION] = argv[argp];
            pParameters->RequiredInitializationLevel |= FL_INITIALIZATION_LEVEL_FULL;
        }
        else
        {
            return FL_Error(pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "Missing image file name for '%s' option.", argument);
        }
    }
    else if(strcmp("--idclkcal", argument) == 0)
    {
        pParameters->CalculateClockCalibrationImageMD5Sum = true;
    }
    else
    {
        return FL_Error(pErrorContext, FL_ERROR_INVALID_COMMAND_LINE_ARGUMENT, "Invalid command line argument: '%s'", argument);
    }

    return FL_SetLastErrorCode(pParameters->pErrorContext, FL_SUCCESS);
}

/***************************************************************************/

// flash_check_file_type
FL_Error FL_CheckFlashFileType(const FL_FlashInfo * pFlashInfo, const char * fileName, const char * expectedPartNumber, FL_FpgaType expectedBitFileFpgaType, FL_Parameters * pParameters)
{
    FL_Error errorCode = 0;
    FILE * pImagefile = fopen(fileName, "rb");

    /* Test that the file we exists */
    if (pImagefile == NULL)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_ERROR, "No such file: %s\n", fileName);
        return FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_OPEN_IMAGE_FILE, "No such file: %s\n", fileName);
    }

    /* Test that the file is a type we accept */
    const char * fileExtension = rindex(fileName, '.');
    if (fileExtension != NULL && strcasecmp(".bit", fileExtension) == 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Image type is bit-file\n");
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Image type of file %s is bit-file\n", fileName);
        pParameters->FlashFileFormat = FL_FLASH_FILE_FORMAT_BIT;
        errorCode = FL_FileReadXilinxBitFileHeader2(pFlashInfo, pImagefile, expectedPartNumber, expectedBitFileFpgaType);
    }
    else if (fileExtension != NULL && strcasecmp(".bin", fileExtension) == 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Image type is bin-file\n");
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Image type of file %s is bin-file\n", fileName);
        pParameters->FlashFileFormat = FL_FLASH_FILE_FORMAT_BIN;
    }
    else if (fileExtension != NULL && strcasecmp(".rbf", fileExtension) == 0)
    {
        //prn(LOG_SCR | LOG_FILE | LEVEL_DEBUG, "Image type is rbf-file\n");
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_DEBUG, "Image type of file %s is rbf-file\n", fileName);
        pParameters->FlashFileFormat = FL_FLASH_FILE_FORMAT_RBF;
    }
    else
    {
        errorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_UNKNOWN_IMAGE_FILE_TYPE, "Image type of file %s is unknown\n", fileName);
        pParameters->FlashFileFormat = FL_FLASH_FILE_FORMAT_UNKNOWN;
    }

    if (fclose(pImagefile) != 0)
    {
        FL_Error latestErrorCode = FL_Error(pFlashInfo->pErrorContext, FL_ERROR_FAILED_TO_CLOSE_IMAGE_FILE,
            "Failed to close image file %s with errno %d", fileName, errno);
        errorCode = errorCode != FL_SUCCESS ? errorCode : latestErrorCode;
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, errorCode);
}

/***************************************************************************/

// VerifyBitFile
FL_Error FL_VerifyBitFiles(FL_FlashLibraryContext * pFlashLibraryContext)
{
    FL_Parameters * pParameters = &pFlashLibraryContext->Parameters;
    const FL_FpgaInfo * pFpgaInfo = &pFlashLibraryContext->FpgaInfo;
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;
    FL_ImageTarget imageTarget;

    //Verify that the bit-file is valid before doing an erase
    for (imageTarget = FL_IMAGE_TARGET_PRIMARY; imageTarget < FL_IMAGE_TARGET_NUMBER_OF_TARGETS; ++imageTarget)
    {
        if (pParameters->FlashFileName[imageTarget] != NULL)
        {
            /* Clock image (imageTarget == FL_IMAGE_TARGET_CLOCK_CALIBRATION) should use expected FPGA type -1 */
            FL_ExitOnError(pLogContext, FL_CheckFlashFileType(pFlashInfo, pParameters->FlashFileName[imageTarget], pFpgaInfo->PartNumber,
                imageTarget == FL_IMAGE_TARGET_CLOCK_CALIBRATION ? -1 : pParameters->ExpectedBitFileFpgaType, pParameters));
        }

        if (pParameters->FlashVerifyFileName[imageTarget] != NULL)
        {
            /* Clock image (imageTarget == FL_IMAGE_TARGET_CLOCK_CALIBRATION) should use expected FPGA type -1 */
            FL_ExitOnError(pLogContext, FL_CheckFlashFileType(pFlashInfo, pParameters->FlashVerifyFileName[imageTarget], pFpgaInfo->PartNumber,
                imageTarget == FL_IMAGE_TARGET_CLOCK_CALIBRATION ? -1 : pParameters->ExpectedBitFileFpgaType, pParameters));
        }
    }

    return FL_SetLastErrorCode(pFlashInfo->pErrorContext, FL_SUCCESS);
}

#pragma endregion

#pragma region MD5

/***************************************************************************/

/*
This code is loosely based on md5.c/.h by Brad Conte (brad AT bradconte.com) and 
the algorithm specification found here: http://tools.ietf.org/html/rfc1321 and
this implementation uses little endian byte order.

The inline functions and the MD5_Transform below are taken directly from the RFC.
*/

/***************************************************************************/

void FL_LogMD5Sum(const char * header, const FL_FlashInfo * pFlashInfo, const uint8_t md5sum[FL_MD5_HASH_SIZE])
{
    char md5sumHex[2 * FL_MD5_HASH_SIZE + 1];
    size_t i;

    if (header != NULL)
    {
        FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "%s\n", header);
    }
    for (i = 0; i < FL_MD5_HASH_SIZE; ++i)
    {
        sprintf(&md5sumHex[2 * i], "%02x", md5sum[i]);
    }
    FL_Log(pFlashInfo->pLogContext, FL_LOG_LEVEL_INFO, "Clkcal MD5 sum: %s\n", md5sumHex);
}

/***************************************************************************/

static inline uint32_t RotateLeft(uint32_t a, uint32_t b)
{
    return (a << b) | (a >> (32 - b));
}

/***************************************************************************/

static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) | (~x & z);
}

/***************************************************************************/

static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & z) | (y & ~z);
}

/***************************************************************************/

static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z)
{
    return x ^ y ^ z;
}

/***************************************************************************/

static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z)
{
    return y ^ (x | ~z);
}

/***************************************************************************/

static inline uint32_t FF(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += F(b, c, d) + x + ac;
    a = RotateLeft(a, s);
    a += b;
    return a;
}

/***************************************************************************/

static inline uint32_t GG(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += G(b, c, d) + x + ac;
    a = RotateLeft(a, s);
    a += b;
    return a;
}

/***************************************************************************/

static inline uint32_t HH(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += H(b, c, d) + x + ac;
    a = RotateLeft(a, s);
    a += b;
    return a;
}

/***************************************************************************/

static inline uint32_t II(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac)
{
    a += I(b, c, d) + x + ac;
    a = RotateLeft(a, s);
    a += b;
    return a;
}

/**
 *  This internal function transforms MD5 state based on a 64 byte data block.
 *  It is an almost direct copy from the RFC.
 *
 *  @param  state       Cumulating MD5 sum state.
 *  @param  data        64 bytes block to transform.
 */
static void MD5_Transform(uint32_t state[4], const uint8_t data[64])
{
    uint32_t a, b, c, d, m[16], i, j;

    /* Convert byte data to big endian 32-bit words */
    for (i = 0, j = 0; i < 16; ++i, j += 4)
    {
        m[i] = (data[j]) + (data[j + 1] << 8) + (data[j + 2] << 16) + (data[j + 3] << 24);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    /* Round 1 */
    a = FF(a, b, c, d, m[0],   7, 0xd76aa478); /* 1 */
    d = FF(d, a, b, c, m[1],  12, 0xe8c7b756); /* 2 */
    c = FF(c, d, a, b, m[2],  17, 0x242070db); /* 3 */
    b = FF(b, c, d, a, m[3],  22, 0xc1bdceee); /* 4 */
    a = FF(a, b, c, d, m[4],   7, 0xf57c0faf); /* 5 */
    d = FF(d, a, b, c, m[5],  12, 0x4787c62a); /* 6 */
    c = FF(c, d, a, b, m[6],  17, 0xa8304613); /* 7 */
    b = FF(b, c, d, a, m[7],  22, 0xfd469501); /* 8 */
    a = FF(a, b, c, d, m[8],   7, 0x698098d8); /* 9 */
    d = FF(d, a, b, c, m[9],  12, 0x8b44f7af); /* 10 */
    c = FF(c, d, a, b, m[10], 17, 0xffff5bb1); /* 11 */
    b = FF(b, c, d, a, m[11], 22, 0x895cd7be); /* 12 */
    a = FF(a, b, c, d, m[12],  7, 0x6b901122); /* 13 */
    d = FF(d, a, b, c, m[13], 12, 0xfd987193); /* 14 */
    c = FF(c, d, a, b, m[14], 17, 0xa679438e); /* 15 */
    b = FF(b, c, d, a, m[15], 22, 0x49b40821); /* 16 */

    /* Round 2 */
    a = GG(a, b, c, d, m[1],   5, 0xf61e2562); /* 17 */
    d = GG(d, a, b, c, m[6],   9, 0xc040b340); /* 18 */
    c = GG(c, d, a, b, m[11], 14, 0x265e5a51); /* 19 */
    b = GG(b, c, d, a, m[0],  20, 0xe9b6c7aa); /* 20 */
    a = GG(a, b, c, d, m[5],   5, 0xd62f105d); /* 21 */
    d = GG(d, a, b, c, m[10],  9, 0x02441453); /* 22 */
    c = GG(c, d, a, b, m[15], 14, 0xd8a1e681); /* 23 */
    b = GG(b, c, d, a, m[4],  20, 0xe7d3fbc8); /* 24 */
    a = GG(a, b, c, d, m[9],   5, 0x21e1cde6); /* 25 */
    d = GG(d, a, b, c, m[14],  9, 0xc33707d6); /* 26 */
    c = GG(c, d, a, b, m[3],  14, 0xf4d50d87); /* 27 */
    b = GG(b, c, d, a, m[8],  20, 0x455a14ed); /* 28 */
    a = GG(a, b, c, d, m[13],  5, 0xa9e3e905); /* 29 */
    d = GG(d, a, b, c, m[2],   9, 0xfcefa3f8); /* 30 */
    c = GG(c, d, a, b, m[7],  14, 0x676f02d9); /* 31 */
    b = GG(b, c, d, a, m[12], 20, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    a = HH(a, b, c, d, m[5],   4, 0xfffa3942); /* 33 */
    d = HH(d, a, b, c, m[8],  11, 0x8771f681); /* 34 */
    c = HH(c, d, a, b, m[11], 16, 0x6d9d6122); /* 35 */
    b = HH(b, c, d, a, m[14], 23, 0xfde5380c); /* 36 */
    a = HH(a, b, c, d, m[1],   4, 0xa4beea44); /* 37 */
    d = HH(d, a, b, c, m[4],  11, 0x4bdecfa9); /* 38 */
    c = HH(c, d, a, b, m[7],  16, 0xf6bb4b60); /* 39 */
    b = HH(b, c, d, a, m[10], 23, 0xbebfbc70); /* 40 */
    a = HH(a, b, c, d, m[13],  4, 0x289b7ec6); /* 41 */
    d = HH(d, a, b, c, m[0],  11, 0xeaa127fa); /* 42 */
    c = HH(c, d, a, b, m[3],  16, 0xd4ef3085); /* 43 */
    b = HH(b, c, d, a, m[6],  23, 0x04881d05); /* 44 */
    a = HH(a, b, c, d, m[9],   4, 0xd9d4d039); /* 45 */
    d = HH(d, a, b, c, m[12], 11, 0xe6db99e5); /* 46 */
    c = HH(c, d, a, b, m[15], 16, 0x1fa27cf8); /* 47 */
    b = HH(b, c, d, a, m[2],  23, 0xc4ac5665); /* 48 */

    /* Round 4 */
    a = II(a, b, c, d, m[0],   6, 0xf4292244); /* 49 */
    d = II(d, a, b, c, m[7],  10, 0x432aff97); /* 50 */
    c = II(c, d, a, b, m[14], 15, 0xab9423a7); /* 51 */
    b = II(b, c, d, a, m[5],  21, 0xfc93a039); /* 52 */
    a = II(a, b, c, d, m[12],  6, 0x655b59c3); /* 53 */
    d = II(d, a, b, c, m[3],  10, 0x8f0ccc92); /* 54 */
    c = II(c, d, a, b, m[10], 15, 0xffeff47d); /* 55 */
    b = II(b, c, d, a, m[1],  21, 0x85845dd1); /* 56 */
    a = II(a, b, c, d, m[8],   6, 0x6fa87e4f); /* 57 */
    d = II(d, a, b, c, m[15], 10, 0xfe2ce6e0); /* 58 */
    c = II(c, d, a, b, m[6],  15, 0xa3014314); /* 59 */
    b = II(b, c, d, a, m[13], 21, 0x4e0811a1); /* 60 */
    a = II(a, b, c, d, m[4],   6, 0xf7537e82); /* 61 */
    d = II(d, a, b, c, m[11], 10, 0xbd3af235); /* 62 */
    c = II(c, d, a, b, m[2],  15, 0x2ad7d2bb); /* 63 */
    b = II(b, c, d, a, m[9],  21, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

/***************************************************************************/

void FL_MD5_Start(FL_MD5Context * pMD5Context)
{
    pMD5Context->DataLength = 0;
    pMD5Context->BitLength = 0;
    pMD5Context->State[0] = 0x67452301;
    pMD5Context->State[1] = 0xEFCDAB89;
    pMD5Context->State[2] = 0x98BADCFE;
    pMD5Context->State[3] = 0x10325476;
}

/***************************************************************************/

void FL_MD5_Add(FL_MD5Context * pMD5Context, const uint8_t * Data, size_t dataLength)
{
    size_t i;

    for (i = 0; i < dataLength; ++i)
    {
        pMD5Context->Data[pMD5Context->DataLength++] = Data[i];
        if (pMD5Context->DataLength == 64)
        {
            MD5_Transform(pMD5Context->State, pMD5Context->Data);
            pMD5Context->BitLength += 512;
            pMD5Context->DataLength = 0;
        }
    }
}

/***************************************************************************/

void FL_MD5_End(FL_MD5Context * pMD5Context, uint8_t hash[FL_MD5_HASH_SIZE])
{
    size_t i = pMD5Context->DataLength;

    // Pad whatever Data is left in the buffer.
    if (pMD5Context->DataLength < 56)
    {
        pMD5Context->Data[i++] = 0x80;
        while (i < 56)
        {
            pMD5Context->Data[i++] = 0x00;
        }
    }
    else if (pMD5Context->DataLength >= 56)
    {
        pMD5Context->Data[i++] = 0x80;
        while (i < 64)
        {
            pMD5Context->Data[i++] = 0x00;
        }
        MD5_Transform(pMD5Context->State, pMD5Context->Data);
        memset(pMD5Context->Data, 0, 56);
    }

    // Append total data length:
    pMD5Context->BitLength += pMD5Context->DataLength * 8;
    pMD5Context->Data[56] = pMD5Context->BitLength;
    pMD5Context->Data[57] = pMD5Context->BitLength >> 8;
    pMD5Context->Data[58] = pMD5Context->BitLength >> 16;
    pMD5Context->Data[59] = pMD5Context->BitLength >> 24;
    pMD5Context->Data[60] = pMD5Context->BitLength >> 32;
    pMD5Context->Data[61] = pMD5Context->BitLength >> 40;
    pMD5Context->Data[62] = pMD5Context->BitLength >> 48;
    pMD5Context->Data[63] = pMD5Context->BitLength >> 56;
    MD5_Transform(pMD5Context->State, pMD5Context->Data);

    // Convert to big endian
    for (i = 0; i < 4; ++i)
    {
        hash[i] = (pMD5Context->State[0] >> (i * 8)) & 0xFF;
        hash[i + 4] = (pMD5Context->State[1] >> (i * 8)) & 0xFF;
        hash[i + 8] = (pMD5Context->State[2] >> (i * 8)) & 0xFF;
        hash[i + 12] = (pMD5Context->State[3] >> (i * 8)) & 0xFF;
    }

    /* Destroy all sensitive information in the context */
    FL_MD5_Start(pMD5Context);
}

/***************************************************************************/

void FL_MD5_Print(FILE * pFile, const uint8_t hash[FL_MD5_HASH_SIZE])
{
    char hashHex[2 * FL_MD5_HASH_SIZE + 1];

    fprintf(pFile, "%s", FL_MD5_HashToHexString(hash, hashHex, true));
}

/***************************************************************************/

const char * FL_MD5_HashToHexString(const uint8_t hash[FL_MD5_HASH_SIZE], char hashHex[2 * FL_MD5_HASH_SIZE + 1], FL_Boolean upperCaseHex)
{
    size_t i;
    const char * format = upperCaseHex ? "%02X" : "%02x";

    for (i = 0; i < FL_MD5_HASH_SIZE; ++i)
    {
        sprintf(&hashHex[2 * i], format, hash[i]);
    }

    return hashHex;
}

/***************************************************************************/

#pragma endregion

#ifdef __cplusplus
} /* extern "C" */
#endif
