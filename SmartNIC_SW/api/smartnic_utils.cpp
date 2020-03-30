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

// Some utility stuff available in the API Library.
// Mostly used by Silicom Denmark internal tools.
// Do not use this stuff in your production code!

#include <getopt.h>

#include <string>
#include <vector>

#include "CommonUtilities.h"
#include "fpgatypes.h"

#include PRODUCT_H
#include PRODUCT_INTERNAL_H

#ifndef SC_RETURN_CASE
    #define SC_RETURN_CASE(value)  case value: return #value
#endif // SC_RETURN_CASE

const char * MemoryUnitSize(uint64_t sizeInBytes, double & sizeInUnits)
{
    const uint64_t Kilo = 1024UL;
    const double KiloF = 1024.0;

    if (sizeInBytes >= Kilo * Kilo * Kilo * Kilo)
    {
        sizeInUnits = double(sizeInBytes) / KiloF / KiloF / KiloF / KiloF;
        return "TB"; // Terabytes
    }
    else if (sizeInBytes >= Kilo * Kilo * Kilo)
    {
        sizeInUnits = double(sizeInBytes) / KiloF / KiloF / KiloF;
        return "GB"; // Gigabytes
    }
    else if (sizeInBytes >= Kilo * Kilo)
    {
        sizeInUnits = double(sizeInBytes) / KiloF / KiloF;
        return "MB"; // Megabytes
    }
    else if (sizeInBytes >= Kilo)
    {
        sizeInUnits = double(sizeInBytes) / KiloF;
        return "kB"; // Kilobytes
    }
    sizeInUnits = double(sizeInBytes);
    return "bytes"; // Bytes
}

const char * UnitSizeBitsPerSecond(uint64_t sizeInBytes, double & sizeInUnits)
{
    const uint64_t Kilo = 1000UL;
    const double KiloF = 1000.0;

    if (sizeInBytes >= Kilo * Kilo * Kilo * Kilo / 8UL)
    {
        sizeInUnits = double(sizeInBytes) * 8.0 / KiloF / KiloF / KiloF / KiloF;
        return "terabits/s";
    }
    else if (sizeInBytes >= Kilo * Kilo * Kilo / 8UL)
    {
        sizeInUnits = double(sizeInBytes) * 8.0 / KiloF / KiloF / KiloF;
        return "gigabits/s";
    }
    else if (sizeInBytes >= Kilo * Kilo / 8)
    {
        sizeInUnits = double(sizeInBytes) * 8.0 / KiloF / KiloF;
        return "megabits/s";
    }
    else if (sizeInBytes >= Kilo / 8)
    {
        sizeInUnits = double(sizeInBytes) * 8.0 / KiloF;
        return "kilobits/s";
    }
    sizeInUnits = double(sizeInBytes) * 8.0;
    return "bits/s";
}

const char * UnitSizeBitsPerSecond(double sizeInBytes, double & sizeInUnits)
{
    return UnitSizeBitsPerSecond(uint64_t(sizeInBytes), sizeInUnits);
}

const char * UnitTime(uint64_t timeInNanoseconds, double & timeInUnits)
{
    const uint64_t Kilo = 1000UL;
    const double KiloF = 1000.0;

    if (timeInNanoseconds >= Kilo * Kilo * Kilo)
    {
        timeInUnits = double(timeInNanoseconds) / KiloF / KiloF / KiloF;
        return "seconds"; // Seconds
    }
    else if (timeInNanoseconds >= Kilo * Kilo)
    {
        timeInUnits = double(timeInNanoseconds) / KiloF / KiloF;
        return "ms"; // Milliseconds
    }
    else if (timeInNanoseconds >= Kilo)
    {
        timeInUnits = double(timeInNanoseconds) / KiloF;
        return UTF_8_GREEK_MU "s"; // UTF-8 Greek Mu, used for "microsecond"
    }
    timeInUnits = double(timeInNanoseconds);
    return "ns"; // Nanoseconds
}

const char * UnitTime(double timeInNanoseconds, double & timeInUnits)
{
    return UnitTime(uint64_t(timeInNanoseconds), timeInUnits);
}

uint64_t Log2n(uint64_t n)
{
    return (n > 1) ? 1 + Log2n(n / 2) : 0;
}

bool IsPowerOfTwo(uint64_t n)
{
    return (n && (!(n & (n - 1)))) != 0;
}

int GetPositionOfSingleBit(uint64_t n)
{
    if (!IsPowerOfTwo(n))
    {
        return -1;
    }
    return int(Log2n(n));
}

std::string GetSendOptionsString(SC_ChannelId channelId, SC_SendOptions sendOptions)
{
    uint32_t value = sendOptions;
    uint32_t mask = 1;
    const char * separator = "";
    std::string options = "";

    while (value != 0)
    {
        options += separator;
        separator = " | ";

        switch (value & mask)
        {
        case SC_SEND_NONE:                  separator = ""; break;
        case SC_SEND_DEFAULT:
            options += "DEFAULT: ";
            SC_GetSendBucketDefaultOptions(channelId, &sendOptions);
            SCI_Assert(SC_GetLastErrorCode() == SC_ERR_SUCCESS);
            assert(sendOptions != SC_SEND_DEFAULT);
            options += GetSendOptionsString(channelId, sendOptions);
            return options;
        case SC_SEND_SINGLE_PACKET:                 options += "SINGLE_PACKET"; break;
        case SC_SEND_SINGLE_LIST:                   options += "SINGLE_LIST"; break;
        case SC_SEND_QUEUE_LIST:                    options += "QUEUE_LIST"; break;
        case SC_SEND_WAIT_FOR_COMPLETION:           options += "WAIT_FOR_COMPLETION"; break;
        case SC_SEND_PRIORITY:                      options += "PRIORITY"; break;
        case SC_SEND_WAIT_FOR_PIPE_FREE:            options += "WAIT_FOR_PIPE_FREE"; break;
        case SC_SEND_DISABLE_CONSUMER_FLOW_CHECK:   options += "DISABLE_CONSUMER_FLOW_CHECK"; break;
        case SC_SEND_FREE_BUCKET_AFTER_ACK_WAIT:    options += "FREE_BUCKET_AFTER_ACK_WAIT"; break;
        default:
        {
            options += "*** UNKNOWN *** ";
            std::stringstream ss;
            ss << "(value & mask = 0x" << std::hex << (value & mask) << ", value = 0x" << value << ", mask = 0x" << mask << ")";
            options += ss.str();
            break;
        }
        }

        value &= ~mask;
        mask <<= 1;

    }

    return options;
}

void PrintSendOptions(FILE * pFile, SC_ChannelId channelId, SC_SendOptions sendOptions)
{
    std::string options = GetSendOptionsString(channelId, sendOptions);
    fprintf(pFile, "Send options: %s\n", options.c_str());
}

static std::string & AppendTo(std::string & s, const char * format, ...) __attribute__((format(printf, 2, 3)));

static std::string & AppendTo(std::string & s, const char * format, ...)
{
    char buffer[200];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    s += buffer;
    return s;
}

static std::string GetTxDmaFifoStatistics(const char * name, const SCI_TxDmaFifoStatistics & s, bool showAlways)
{
    std::string txDmaFifoStatistics;

    if (s.FwSuccessCount != 0 || s.FwFullCount != 0 || s.FwMaximumFillLevel != 0 ||
        s.SwSuccessCount != 0 || s.SwFullCount != 0 || s.SwMaximumFillLevel != 0 ||
        showAlways)
    {
        AppendTo(txDmaFifoStatistics, "%s: fwSuccessCount:     %lu\n", name, s.FwSuccessCount);
        AppendTo(txDmaFifoStatistics, "%s: fwFullCount:        %lu\n", name, s.FwFullCount);
        AppendTo(txDmaFifoStatistics, "%s: fwMaximumFillLevel: %u\n", name, s.FwMaximumFillLevel);
        AppendTo(txDmaFifoStatistics, "%s: swSuccessCount:     %lu\n", name, s.SwSuccessCount);
        AppendTo(txDmaFifoStatistics, "%s: swFullCount:        %lu\n", name, s.SwFullCount);
        AppendTo(txDmaFifoStatistics, "%s: swMaximumFillLevel: %u\n", name, s.SwMaximumFillLevel);
    }

    return txDmaFifoStatistics;
}

static std::string GetTxConsumedBytesStatistics(const char * name, SCI_TxConsumedBytesStatistics & s, bool showAlways)
{
    std::string txConsumedBytesStatistics;

    if (s.SuccessCount != 0 || s.FullCount != 0 || s.CachedReads != 0 || s.RegisterReads != 0 || showAlways)
    {
        AppendTo(txConsumedBytesStatistics, "%s: successCount:       %lu\n", name, s.SuccessCount);
        AppendTo(txConsumedBytesStatistics, "%s: fullCount:          %lu\n", name, s.FullCount);
        AppendTo(txConsumedBytesStatistics, "%s: cachedReads:        %lu\n", name, s.CachedReads);
        AppendTo(txConsumedBytesStatistics, "%s: registerReads:      %lu\n", name, s.RegisterReads);
    }

    return txConsumedBytesStatistics;
}

static std::string GetStatistics(const std::vector<Statistics> & statistics)
{
    std::string statisticsString;

    for (std::size_t i = 0; i < statistics.size(); ++i)
    {
        const Statistics & s = statistics[i];

        double mean, standardDeviation, minimum, maximum;
        const char * meanUnit = UnitTime(s.Mean, mean);
        const char * standardDeviationUnit = UnitTime(s.SD(), standardDeviation);
        const char * minimumUnit = UnitTime(s.Minimum(), minimum);
        const char * maximumUnit = UnitTime(s.Maximum(), maximum);

        AppendTo(statisticsString, "%s: mean %.2F %s, SD %.2f %s, Min %.2f %s, Max %.2f %s\n",
            s.Name.c_str(), mean, meanUnit, standardDeviation, standardDeviationUnit,
            minimum, minimumUnit, maximum, maximumUnit);
    }

    return statisticsString;
}

std::string GetTxFlowControlStatistics(SC_ChannelId channelId, bool showAlways)
{
    std::string txFullFlowControlStatistics;

    SC_TxFlowControlStatistics txFlowControlStatistics;
    std::vector<Statistics> statistics;
    SCI_GetTxFlowControlStatistics(channelId, &txFlowControlStatistics, &statistics);

    txFullFlowControlStatistics += GetTxDmaFifoStatistics("  Normal FIFO", txFlowControlStatistics.NormalFifo, showAlways);
    txFullFlowControlStatistics += GetTxDmaFifoStatistics("Priority FIFO", txFlowControlStatistics.PriorityFifo, showAlways);
    txFullFlowControlStatistics += GetTxConsumedBytesStatistics("     Consumer", txFlowControlStatistics.ConsumerChannel, showAlways);
    txFullFlowControlStatistics += GetTxConsumedBytesStatistics("   TCP Window", txFlowControlStatistics.TcpWindow, showAlways);

    AppendTo(txFullFlowControlStatistics, " Flow control: waitPipeFreeCount:  %lu\n", txFlowControlStatistics.WaitPipeFreeCount);
    AppendTo(txFullFlowControlStatistics, " Flow control: PipeFullCount:      %lu\n", txFlowControlStatistics.PipeFullCount);
    AppendTo(txFullFlowControlStatistics, " Flow control: bookingsCancelled:  %lu\n", txFlowControlStatistics.BookingsCancelled);

    AppendTo(txFullFlowControlStatistics, "  Bucket Pool: FlowControlChecks:  %lu\n", txFlowControlStatistics.FlowControlCheckFreeBuckets);
    AppendTo(txFullFlowControlStatistics, "  Bucket Pool: GetBucketChecks:    %lu\n", txFlowControlStatistics.GetBucketCheckFreeBuckets);

    AppendTo(txFullFlowControlStatistics, "SendReason: %s\n", SCI_GetSendReasonString(txFlowControlStatistics.SendReason).c_str());

    txFullFlowControlStatistics += GetStatistics(statistics);

    return txFullFlowControlStatistics;
}

void PrintTxFlowControlStatistics(SC_ChannelId channelId, FILE * pFile, bool showAlways)
{
    std::string txFullFlowControlStatistics = GetTxFlowControlStatistics(channelId, showAlways);
    fprintf(pFile, "%s", txFullFlowControlStatistics.c_str());
}

void DecodeIoCtlCmd(unsigned int op)
{
    LogError(nullptr, "ioctl op = %u, SC_IOCTL_MISC = %lu\n", op, SC_IOCTL_MISC);
    LogError(nullptr, "_IOC_DIRBITS = %u, _IOC_SIZEBITS = %u, _IOC_TYPEBITS = %u, _IOC_NRBITS = %u\n", _IOC_DIRBITS, _IOC_SIZEBITS, _IOC_TYPEBITS, _IOC_NRBITS);
    LogError(nullptr, "_IOC_DIRSHIFT = %u, _IOC_SIZESHIFT = %u, _IOC_TYPESHIFT = %u, _IOC_NRSHIFT = %u\n", _IOC_DIRSHIFT, _IOC_SIZESHIFT, _IOC_TYPESHIFT, _IOC_NRSHIFT);
    LogError(nullptr, "dir = %u, size = %u, type = %u, nr = %u\n", _IOC_DIR(op), _IOC_SIZE(op), _IOC_TYPE(op), _IOC_NR(op));
}

int64_t SCI_GetNumber(const char * stringValue, const char * context, int64_t minimum, int64_t maximum, bool acceptMinusOne)
{
    char c;
    const char * s = stringValue;

    if (s == nullptr)
    {
        LogErrorToFile(stderr, nullptr, "%s value cannot be NULL\n", context);
        exit(EXIT_FAILURE);
        return -1;
    }

    if (*s == '+' || *s == '-')
    {
        ++s;
    }

    while ((c = *s++) != '\0')
    {
        if (!isdigit(c))
        {
            LogErrorToFile(stderr, nullptr, "%s value '%s' is not a number\n", context, stringValue);
            exit(EXIT_FAILURE);
            return -1;
        }
    }

    int64_t value = atol(stringValue);
    if (value == -1)
    {
        if (acceptMinusOne)
        {
            return value;
        }
        else
        {
            LogErrorToFile(stderr, nullptr, "%s value cannot be -1\n", context);
            exit(EXIT_FAILURE);
            return -1;
        }
    }
    if (value < minimum || value > maximum)
    {
        LogErrorToFile(stderr, nullptr, "%s value '%s' is outside of valid range [%ld..%ld]\n", context, stringValue, minimum, maximum);
        exit(EXIT_FAILURE);
        return -1;
    }

    return value;
}

const char * SCI_LogMAC(std::string & macString, const uint8_t macAddress[MAC_ADDR_LEN])
{
    char buffer[3 * MAC_ADDR_LEN];

    snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
        macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    macString = buffer;

    return macString.c_str();
}

int SCI_getopt_long(int argc, char * const argv[], const char * shortOptionsString, const struct option * longOptions, int * pLongOptionIndex,
                    char * combinedOptionsName, size_t combinedOptionsNameSize)
{
    int optionSingleCharacter = getopt_long(argc, argv, shortOptionsString, longOptions, pLongOptionIndex);

    if (combinedOptionsName != nullptr && combinedOptionsNameSize > 0)
    {
        combinedOptionsName[0] = '\0';
    }

    if (optionSingleCharacter == -1)
    {
        return -1;
    }

    if (optionSingleCharacter != 0)
    {
        *pLongOptionIndex = 0;
        // long_option_index is not updated if short option found!
        while (longOptions[*pLongOptionIndex].val != '\0' && longOptions[*pLongOptionIndex].val != optionSingleCharacter)
        {
            ++*pLongOptionIndex;
        }
    }

    if (combinedOptionsName != nullptr && combinedOptionsNameSize > 3 + strlen(longOptions[*pLongOptionIndex].name))
    {
        if (optionSingleCharacter != 0)
        {
            char tmp[] = { char(optionSingleCharacter), '\0' };
            strcpy(combinedOptionsName, "-");
            strcat(combinedOptionsName, tmp);
            strcat(combinedOptionsName, "/");
        }
        strcat(combinedOptionsName, longOptions[*pLongOptionIndex].name);
    }

    return optionSingleCharacter;
}

bool SCI_IsDebugBuild()
{
#if defined(DEBUG)
    return true;
#elif defined(NDEBUG)
    return false;
#else
    #error DEBUG for debug build or NDEBUG for release build must be defined!
#endif
}

static const char * IsTestFpga(uint16_t fpgaType)
{
    switch (fpgaType)
    {
        SC_RETURN_CASE(FPGA_TYPE_LATINA_TEST);
        SC_RETURN_CASE(FPGA_TYPE_LIVIGNO_TEST);
        SC_RETURN_CASE(FPGA_TYPE_LIVORNO_TEST);
        SC_RETURN_CASE(FPGA_TYPE_LUCCA_TEST);
        SC_RETURN_CASE(FPGA_TYPE_MANGO_TEST);
        SC_RETURN_CASE(FPGA_TYPE_SAVONA_TEST);
        SC_RETURN_CASE(FPGA_TYPE_TIVOLI_TEST);
        default: return nullptr;
    }
}

static const char * IsLeonbergFpga(uint16_t fpgaType)
{
    switch (fpgaType)
    {
        SC_RETURN_CASE(FPGA_TYPE_LATINA_LEONBERG);
        SC_RETURN_CASE(FPGA_TYPE_LIVIGNO_LEONBERG_V7330);
        SC_RETURN_CASE(FPGA_TYPE_LIVIGNO_LEONBERG_V7690);
        SC_RETURN_CASE(FPGA_TYPE_LIVORNO_LEONBERG);
        SC_RETURN_CASE(FPGA_TYPE_LUCCA_LEONBERG_V7330);
        SC_RETURN_CASE(FPGA_TYPE_LUCCA_LEONBERG_V7690);
        SC_RETURN_CASE(FPGA_TYPE_MANGO_LEONBERG_VU125);
        SC_RETURN_CASE(FPGA_TYPE_MANGO_LEONBERG_VU9P);
        SC_RETURN_CASE(FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1);
        SC_RETURN_CASE(FPGA_TYPE_MANGO_04_LEONBERG_VU9P);
        SC_RETURN_CASE(FPGA_TYPE_PADUA_LEONBERG_VU125);
        default: return nullptr;
    }
}

bool SCI_IsSmartNicFpgaImage(uint16_t fpgaImageType)
{
    return IsLeonbergFpga(fpgaImageType) != nullptr || IsTestFpga(fpgaImageType) != nullptr;
}

static const char * IsWolfsburgFpga(uint16_t fpgaType)
{
    switch (fpgaType)
    {
        SC_RETURN_CASE(FPGA_TYPE_MANGO_WOLFSBURG_4X100G);
        SC_RETURN_CASE(FPGA_TYPE_SAVONA_WOLFSBURG_2X100G);
        SC_RETURN_CASE(FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G);
        default: return nullptr;
    }
}

bool SCI_IsPacketMoverFpgaImage(uint16_t fpgaImageType)
{
    return IsWolfsburgFpga(fpgaImageType) != nullptr || IsTestFpga(fpgaImageType) != nullptr;
}

void SCI_HexDump(SCI_IPrint * pPrint, const void * pMemory, uint64_t length, uint16_t bytesPerLine, bool showAddress)
{
    const uint8_t * bytes = static_cast<const uint8_t *>(pMemory);
    std::string ascii(bytesPerLine + 1, ' ');
    uint64_t lineCount = 0;
    uint64_t i = 0;

    ascii[bytesPerLine] = '\0';
    for (i = 0; i < length; ++i)
    {
        if (i % bytesPerLine == 0)
        {
            if (showAddress)
            {
                pPrint->Print("%p:", bytes + i);
            }
            else
            {
                pPrint->Print("%04X:", i);
            }
        }
        uint8_t byte = bytes[i];
        ascii[i % bytesPerLine] = byte >= 32 && byte < 127 ? char(byte) : '.';
        pPrint->Print(" %02X", byte);
        if ((i + 1) % 8 == 0 && (i + 1) % bytesPerLine != 0)
        {
            pPrint->Print(" |");
        }
        if ((i + 1) % bytesPerLine == 0)
        {
            pPrint->Print("   %s\n", ascii.c_str());
            if (++lineCount % 8 == 0)
            {
                pPrint->Print("\n");
            }
        }
    }
    if (i % bytesPerLine != 0)
    {
        uint64_t remainingLineBytes = bytesPerLine - i % bytesPerLine;
        for (uint64_t j = 0; j < remainingLineBytes; ++j)
        {
            pPrint->Print("   ");
        }
        while (remainingLineBytes > 8)
        {
            pPrint->Print("  "); // Extra fill for " | "
            remainingLineBytes -= 8;
        }
        ascii[i % bytesPerLine] = '\0';
        pPrint->Print("   %s\n", ascii.c_str());
    }
}

void SCI_HexDump(std::ostream & outputStream, const void * pMemory, uint64_t length, uint16_t bytesPerLine, bool showAddress)
{
    SCI_Print print(&outputStream);

    SCI_HexDump(&print, pMemory, length, bytesPerLine, showAddress);
}

void SCI_HexDump(int(*pPrint)(const char * format, ...), const void * pMemory, uint64_t length, uint16_t bytesPerLine, bool showAddress)
{
    SCI_Print print(pPrint);

    SCI_HexDump(&print, pMemory, length, bytesPerLine, showAddress);
}

void SCI_HexDumpStdout(const void * pMemory, uint64_t length, uint16_t bytesPerLine, bool showAddress)
{
    SCI_HexDump(&printf, pMemory, length, bytesPerLine, showAddress);
}

SCI_Print::SCI_Print()
    : _pFile(nullptr), _pOutputStream(nullptr), _pPrint(&printf)
{
}

SCI_Print::SCI_Print(FILE * pFile)
    : _pFile(pFile), _pOutputStream(nullptr), _pPrint(nullptr)
{
}

SCI_Print::SCI_Print(std::ostream * pOutputStream)
    : _pFile(nullptr), _pOutputStream(pOutputStream), _pPrint(nullptr)
{
}

SCI_Print::SCI_Print(int(*pPrint)(const char * format, ...))
    : _pFile(nullptr), _pOutputStream(nullptr), _pPrint(pPrint)
{
}

SCI_Print::~SCI_Print()
{
    _pFile = nullptr;
    _pOutputStream = nullptr;
    _pPrint = nullptr;
}

int SCI_Print::Print(const char * format, ...) const
{
    int numberOfCharactersFormatted = 0;

    if (_pFile != nullptr)
    {
        va_list args;
        va_start(args, format);
        numberOfCharactersFormatted = vfprintf(_pFile, format, args);
        va_end(args);

        fflush(_pFile);
    }

    if (_pOutputStream != nullptr || _pPrint != nullptr)
    {
        int formattedStringSize = 128;
        std::vector<char> formattedString(static_cast<std::size_t>(formattedStringSize));

        while (true)
        {
            va_list args;
            va_start(args, format);
            numberOfCharactersFormatted = vsnprintf(&formattedString[0], formattedString.size(), format, args);
            va_end(args);

            if (numberOfCharactersFormatted < formattedStringSize)
            {
                break;
            }

            // Formatted string vector size was not big enough, double the size and try again:
            formattedStringSize *= 2;
            formattedString.resize(static_cast<std::size_t>(formattedStringSize));
        }

        if (_pOutputStream != nullptr)
        {
            *_pOutputStream << &formattedString[0] << std::flush;
        }

        if (_pPrint != nullptr)
        {
            numberOfCharactersFormatted = (*_pPrint)("%s", &formattedString[0]);
        }
    }

    return numberOfCharactersFormatted;
}

/*****************************************************************************/

#if defined(API_TIMING) && API_TIMING == 1

/*****************************************************************************/

Statistics ApiTimerStatistics[API_TIMER_MAX];
int64_t _ApiScopedTimer::EmptyFunctionOverhead = 0;

/*****************************************************************************/

void EmptyFunction()
{
    ApiScopedTimer(API_TIMER_EMPTY_FUNCTION);
}

/*****************************************************************************/

/**
 *  API timer initialization and final output.
 */
static struct ApiTimerInitializer
{
    /**
     *  Constructor initializes all API timer related variables.
     */
    explicit ApiTimerInitializer()
    {
        ApiTimerStatistics[API_TIMER_EMPTY_FUNCTION].Name = "Empty Function";
        ApiTimerStatistics[API_TIMER_SEND_BUCKET].Name = nameof(SC_SendBucket);
        ApiTimerStatistics[API_TIMER_SEND_SINGLE_PACKET].Name = nameof(SendSinglePacket);
        ApiTimerStatistics[API_TIMER_FLOW_CONTROL].Name = nameof(FlowControl);
        ApiTimerStatistics[API_TIMER_CAN_SEND_ON_THIS_CHANNEL].Name = nameof(CanSendOnThisChannel);
        ApiTimerStatistics[API_TIMER_TCP_WINDOW_FLOW_CONTROL].Name = nameof(TcpWindowFlowControl);
        ApiTimerStatistics[API_TIMER_FIRMWARE_CONSUMED_BYTES_FLOW_CONTROL].Name = nameof(FirmwareConsumedBytesFlowControl);
        ApiTimerStatistics[API_TIMER_TX_DMA_SOFTWARE_FLOW_CONTROL].Name = nameof(TxDmaSoftwareFlowControl);
        ApiTimerStatistics[API_TIMER_TX_DMA_FIRMWARE_FLOW_CONTROL].Name = nameof(TxDmaFirmwareFlowControl);
        ApiTimerStatistics[API_TIMER_UPDATE_CACHED_CONSUMED_BYTES].Name = nameof(UpdateCachedConsumedBytes);
        ApiTimerStatistics[API_TIMER_GET_BUCKET].Name = nameof(SC_GetBucket);
        ApiTimerStatistics[API_TIMER_GET_NEXT_PACKET].Name = nameof(SC_GetNextPacket);

        _ApiScopedTimer::EmptyFunctionOverhead = 0;

        for (int i = 0; i < 1000; ++i)
        {
            EmptyFunction();
        }

        // Something fishy going on here! Don't use the value of API_TIMER_EMPTY_FUNCTION
        _ApiScopedTimer::EmptyFunctionOverhead = 0;// int64_t(ApiTimerStatistics[API_TIMER_EMPTY_SCOPE].Mean);
    }

    /**
     *  Destructor outputs all API timer measurements.
     */
    ~ApiTimerInitializer()
    {
        bool somethingToPrint = false;

        for (std::size_t i = 0; i < API_TIMER_MAX; ++i)
        {
            const Statistics & statistics = ApiTimerStatistics[i];
            if (statistics.Count > 0)
            {
                somethingToPrint = true;
                break;
            }
        }

        if (somethingToPrint)
        {
            fprintf(stdout, "=== API library call statistics. Times are in nanoseconds ===\n");
            fprintf(stdout, "%35s  %9s   %-9s %9s %9s %s\n",
                "Function Name", "Mean", "St. Dev.", "Minimum", "Maximum", "Samples");

            for (std::size_t i = 0; i < API_TIMER_MAX; ++i)
            {
                const Statistics & statistics = ApiTimerStatistics[i];
                if (statistics.Count > 0)
                {
                    fprintf(stdout, "%35s: %9.2f %s %-9.2f %9.2f %9.2f %lu\n",
                        statistics.Name.c_str(), statistics.Mean, UTF_8_PLUS_MINUS, statistics.SD(),
                        statistics.Minimum(), statistics.Maximum(), statistics.Count);
                }
            }
        }
    }

} s_ApiTimerInitializer;

#endif
