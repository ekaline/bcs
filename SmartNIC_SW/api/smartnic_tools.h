#ifndef __product_tools_h__
#define __product_tools_h__

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
 * @brief   Internal definitions for use by internal tools.
 *          These definitions can change from release to release and should not be used
 *          by user programs or in production code.
 */

/** @cond PRIVATE_CODE */

#include <stdbool.h>
#include <stdint.h>

#include PRODUCT_H
#include "CommonUtilities.h"

#ifdef __cplusplus

#include "Stopwatch.h"
#include "Statistics.h"

extern "C"
{
#endif

#define SCI_SEND_SPECIAL    0x8000000000000000UL    /**< Special send operation for special test usage */

#define ARRAY_SIZE(array)  (sizeof(array) / sizeof((array)[0]))

#pragma pack(push,1)

/**
 *  These send reasons register for internal logging and performance measurement purposes
 *  why a bucket send operation succeeded or failed.
 */
typedef enum
{
    SCI_SEND_REASON_NONE                        = 0x000,    /**< No reasons available */

    /* Tx DMA normal or priority FIFO */
    SCI_SEND_REASON_NORMAL_FIFO                 = 0x001,    /**< Tx DMA normal FIFO */
    SCI_SEND_REASON_PRIORITY_FIFO               = 0x002,    /**< Tx DMA priority FIFO */

    /* Firmware fill level of Tx DMA FIFO success/failure reasons */
    SCI_SEND_REASON_FW_FIFO_SUCCESS             = 0x004,    /**< Tx DMA FIFO fill level (512 entries) is not full */
    SCI_SEND_REASON_FW_FIFO_FULL                = 0x008,    /**< Tx DMA FIFO fill level (512 entries) is full (read from register) */

    /* Channel specific software count of Tx DMA FIFO success/failure reasons */
    SCI_SEND_REASON_SW_FIFO_SUCCESS             = 0x010,    /**< Channel specific Tx DMA normal FIFO limit has not been reached */
    SCI_SEND_REASON_SW_FIFO_FULL                = 0x020,    /**< Channel specific Tx DMA normal FIFO limit has been reached */

    /* Channel specific consumer firmware reported consumed bytes success/failure reasons */
    SCI_SEND_REASON_CONSUMER_SUCCESS            = 0x040,    /**< Channel specific consumed bytes allow the send */
    SCI_SEND_REASON_CONSUMER_FULL               = 0x080,    /**< Channel specific consumed bytes deny the send */

    /* TCP window success/failure reasons */
    SCI_SEND_REASON_TCP_WINDOW_SUCCESS          = 0x100,    /**< Channel specific window sent bytes allow the send */
    SCI_SEND_REASON_TCP_WINDOW_FULL             = 0x200,    /**< Channel specific window sent bytes deny the send */

    SCI_SEND_REASON_LIST_SIZE                   = 0x400,    /**< Accumulated list was flushed because cumulative payload length */
                                                            /**< exceeded a limit. */

    SCI_SEND_REASON_LIST_LENGTH                 = 0x800,    /**< Accumulated list was flushed because list length */

#if 1 /* TODO */
    SCI_SEND_REASON_SUCCESS                     = SCI_SEND_REASON_CONSUMER_SUCCESS,

    SCI_SEND_REASON_TCP_SUCCESS                 = SCI_SEND_REASON_SUCCESS +             /**< All criteria specific to a TCP */
                                                  SCI_SEND_REASON_TCP_WINDOW_SUCCESS    /**< channel succeeded, can send! */
#endif
} SCI_SendReason;

/**
 *  Internal Tx DMA related statistics information.
 */
typedef struct
{
    uint64_t                FwSuccessCount;         /**< Count of times the Tx DMA FIFO was not full */
    uint64_t                FwFullCount;            /**< Count of times the Tx DMA FIFO was full */
    uint16_t                FwMaximumFillLevel;     /**< Maximum fill level (request count) seen. */

    uint64_t                SwSuccessCount;         /**< Count of times the Tx DMA FIFO channel count was not full */
    uint64_t                SwFullCount;            /**< Count of times the Tx DMA FIFO channel count was not full */
    uint16_t                SwMaximumFillLevel;     /**< Maximum fill level (request count) seen. */

} SCI_TxDmaFifoStatistics;

/**
 *  Internal consumed bytes (2nd level 16KB buffer) related statistics information.
 */
typedef struct
{
    uint64_t                SuccessCount;       /**< Count of times the channel specific limit was not exceeded */
    uint64_t                FullCount;          /**< Count of times the channel specific limit was exceeded */
    uint64_t                CachedReads;        /**< Number of flow control checks that used the cached consumed bytes value. */
    uint64_t                RegisterReads;      /**< Number of flow control checks that read the cached consumed bytes value */
                                                /**< from a register.. */

} SCI_TxConsumedBytesStatistics;

/**
 *  Internal flow control related statistics information.
 */
typedef struct
{
    SCI_TxDmaFifoStatistics         NormalFifo;             /**< Tx DMA normal FIFO statistics */
    SCI_TxDmaFifoStatistics         PriorityFifo;           /**< Tx DMA priority FIFO statistics */
    SCI_TxConsumedBytesStatistics   ConsumerChannel;        /**< Second level consumer Tx buffers statistics */
    SCI_TxConsumedBytesStatistics   TcpWindow;              /**< TCP window buffer statistics */

    uint64_t                        WaitPipeFreeCount;      /**< Count of times for waiting until can send */
    uint64_t                        PipeFullCount;          /**< Count of times flow control returned SC_ERR_PIPE_FULL */
    uint64_t                        BookingsCancelled;      /**< Count of bookings cancelled */

    uint64_t                        FlowControlCheckFreeBuckets;    /**< Calls to CheckFreeBuckets from FlowControl */
    uint64_t                        GetBucketCheckFreeBuckets;      /**< Calls to CheckFreeBuckets from SC_GetBucket */

    SCI_SendReason                  SendReason;             /**< Reasons for success or failure */

} SC_TxFlowControlStatistics;

#pragma pack(pop)

SC_Error SCI_GetTxFlowControlStatistics(SC_ChannelId channelId, SC_TxFlowControlStatistics * pTxFlowControlStatistics, void * pStatistics);

/**
 *  Send a packet directly from the receive ring buffer.
 */
SC_Error SCI_SendPacket(SC_ChannelId channelId, SC_Packet * pPacket, SC_SendOptions sendOptions, void * pReserved);

/**
 *  Is the library a debug build or release build?
 *
 *  @return     True if library is a debug build. false if a release build.
 */
bool SCI_IsDebugBuild();

/**
 *  Is the given FPGA image type a correct image for this product?
 *
 *  @param      fpgaImageType   FPGA image type to test.
 *
 *  @return     True @ref fpgaImageType is a correct image for this product, false otherwise.
 */
bool SCI_IsSmartNicFpgaImage(uint16_t fpgaImageType);

/**
 *  Hex dump of memory to standard output.
 *
 *  @param      pMemory         Pointer to memory hex dump start address.
 *  @param      length          Length of memory starting form start address to hex dump.
 *  @param      bytesPerLine    Bytes per line in hex dump.
 *  @param      showAddress     If true then memory address is shown, else a count from zero.
 */
void SCI_HexDumpStdout(const void * pMemory, uint64_t length, uint16_t bytesPerLine, bool showAddress);

/**
 *  Hex dump of memory.
 *
 *  @param      pPrint          Output function with printf signature.
 *  @param      pMemory         Pointer to memory hex dump start address.
 *  @param      length          Length of memory starting form start address to hex dump.
 *  @param      bytesPerLine    Bytes per line in hex dump.
 *  @param      showAddress     If true then memory address is shown, else a count from zero.
 */
void SCI_HexDump(int (*pPrint)(const char * format, ...), const void * pMemory, uint64_t length, uint16_t bytesPerLine, bool showAddress);

uint32_t SCI_GetNumberOfUserLogicRegisters();

/**
 *  Get value of an integer from a numeric string or print error message to stderr and exit with EXIT_FAILURE
 *  if string cannot be converted to an integer.
 *
 *  @param  stringValue
 */
int64_t SCI_GetNumber(const char * stringValue, const char * context, int64_t minimum, int64_t maximum, bool acceptMinusOne);

/**
 *  Get TOE channel TCP state.
 *
 *  @param      deviceId        Device id.
 *  @param      toeChannel      TOE channel number between 0-63.
 *
 *  @return TCP state of the channel or negative @ref SC_Error error code.
 */
SC_TcpState SCI_GetTcpState(SC_DeviceId deviceId, uint16_t toeChannel);

/**
 *  Get FPGA DMA channel type.
 *
 *  @param      deviceId            Device id.
 *  @param      dmaChannelNumber    FPGA DMA channel number in the range [0-127].
 *
 *  @return FPGA DMA channel type or negative @ref SC_Error error code.
 */
SC_ChannelType SCI_GetChannelType(SC_DeviceId deviceId, uint16_t dmaChannelNumber);

/**
 *  Get channel type name from channel type.
 *
 *  @param      channelType     Channel type as @ref SC_ChannelType.
 *
 *  @return Name of channel type as a string if channel type is known, an error string otherwise.
 */
const char * SCI_GetChannelTypeName(SC_ChannelType channelType);

/**
 *  Get the last thread specific error code from the API library.
 *
 *  @return The last error code in the current thread.
 */
SC_Error SCI_GetLastThreadErrorCode();

/**
 *   Set the last thread specific error code in the API library.
 *
 *  @param  errorCode       The error code to set.
 *
 *  @return The previous error code.
 */
SC_Error SCI_SetLastThreadErrorCode(SC_Error errorCode);

/**
 *  For use in applications instead of getopt_long. Fixes GNU getopt_long which does not update value pointed
 *  by pLongOptionIndex if a short option is found. This function always updates the index.
 *
 *  @param  argc                    Argument count.
 *  @param  argv                    Argument.
 *  @param  shortOptionsString      Short options string.
 *  @param  longOptions             Options table.
 *  @param  pLongOptionIndex        Pointer to index into the options table.
 *  @param  combinedOptionsName     If not NULL then is filled with the combined options name like this: "-o/--option"
 *  @param  combinedOptionsNameSize Size of combinedOptionsName.
 *
 *  @return     Returns the option character from GNU getopt_long, see "man getopt_long".
 */
int SCI_getopt_long(int argc, char * const argv[], const char * shortOptionsString, const struct option * longOptions, int * pLongOptionIndex,
    char * combinedOptionsName, size_t combinedOptionsNameSize);

/**
 *  Get some network interface information.
 */
SC_Error SCI_GetNetworkInterfaceInfo(SC_DeviceId deviceId, uint16_t networkInterface, struct in_addr * pLocalIP,
    struct in_addr * pNetMask, struct in_addr * pBroadcastAddress, uint32_t * pFlags);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus

#include <iostream>
#include <string>

/**
 *  Get a send reason as a string.
 *
 *  @param  sendReason  Send reason value.
 *
 *  @return Send reason as a string.
 */
std::string SCI_GetSendReasonString(SCI_SendReason sendReason);

/**
 *  Get a send options as a string.
 *
 *  @param  channelId    Channel identifier.
 *  @param  sendOptions  Send options value.
 *
 *  @return Send options as a string.
 */
std::string GetSendOptionsString(SC_ChannelId channelId, SC_SendOptions sendOptions);

const char * MemoryUnitSize(uint64_t sizeInBytes, double & sizeInUnits);
const char * UnitSizeBitsPerSecond(uint64_t sizeInBytes, double & sizeInUnits);
const char * UnitSizeBitsPerSecond(double sizeInBytes, double & sizeInUnits);
const char * UnitTime(uint64_t timeInNanoseconds, double & timeInUnits);
const char * UnitTime(double timeInNanoseconds, double & timeInUnits);

void PrintSendOptions(FILE * f, SC_ChannelId channelId, SC_SendOptions sendOptions);
std::string GetTxFlowControlStatistics(SC_ChannelId channelId, bool showAlways);
void PrintTxFlowControlStatistics(SC_ChannelId channelId, FILE * f, bool showAlways = false);

void DecodeIoCtlCmd(unsigned int op);

template <typename T>
static inline T SCI_GetNumberT(const char * stringValue, const char * context, T minimum, T maximum, bool acceptMinusOne)
{
    return T(SCI_GetNumber(stringValue, context, int64_t(minimum), int64_t(maximum), acceptMinusOne));
}

/**
*   Convert MAC address to a string.
*
*  @param  macString       String instance that receives the MAC address as string.
*  @param  macAddress      The MAC address to convert.
*
*  @return const char pointer to the contents of the @ref macString.
*/
const char * SCI_LogMAC(std::string & macString, const uint8_t macAddress[MAC_ADDR_LEN]);

void SCI_DumpARP(FILE * pFile);
SC_Error SCI_ARP_Test(uint8_t networkInterface, const std::string & interface, const std::string & remoteHost, uint8_t macAddress[MAC_ADDR_LEN], uint64_t timeout);

/**
 *  Abstract print interface. To be implemented by objects that want to print to miscellaneous outputs.
 */
struct SCI_IPrint
{
    /**
     *  Print method used to print. Signature and usage is same as C standard library printf function.
     *
     *  @param      format      Format string that is compatible with C standard library printf function.
     *
     *  @return     Return the number characters output in the formatted string like printf.
     */
    virtual int Print(const char * format, ...) const = 0;

    /**
     *  Virtual destructor.
     */
    virtual ~SCI_IPrint() {}
};

/**
 *  Print object that implements the @ref SCI_IPrint interface.
 *  Supports printing to FILE or C++ std::ostream.
 */
class SCI_Print : public SCI_IPrint
{
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_Print);

private:

    FILE *          _pFile;
    std::ostream *  _pOutputStream;
    int (*_pPrint)(const char * format, ...);

public:

    /**
    *  Default constructor is equivalent of using printf compatible print function.
    *
    *  @param  pFile   FILE to print to.
    */
    SCI_Print();

    /**
     *  Construct a print object that prints to a FILE.
     *
     *  @param  pFile   FILE to print to.
     */
    SCI_Print(FILE * pFile);

    /**
     *  Construct a print object that prints to a C++ stream.
     *
     *  @param  pOutputStream   C++ output stream to print to.
     */
    SCI_Print(std::ostream * pOutputStream);

    /**
     *  Construct a print object that prints via a printf compatible function pointer.
     *
     *  @param  pPrint       C printf signature and usage function to print with.
     */
    SCI_Print(int (*pPrint)(const char * format, ...));

    /**
     *  Destruct the print object.
     */
    virtual ~SCI_Print();

    /**
     *  Print method that does the actual printing. Follows C printf signature.
     */
    virtual int Print(const char * format, ...) const;
};

/**
 *  Hex dump a piece of memory.
 *
 *  @param      pPrint          Pointer to a @ref SCI_IPrint interface. Hex dump will use the print method of the interface.
 *  @param      pMemory         Pointer to memory hex dump start address.
 *  @param      length          Length of memory starting form start address to hex dump.
 *  @param      bytesPerLine    Bytes per line in hex dump.
 *  @param      showAddress     If true then memory address is shown, else a count from zero.
 */
void SCI_HexDump(SCI_IPrint * pPrint, const void * pMemory, uint64_t length, uint16_t bytesPerLine = 16, bool showAddress = true);

/**
 *  Hex dump a piece of memory.
 *
 *  @param      outputStream    C++ output stream where memory hex dump is written.
 *  @param      pMemory         Pointer to memory hex dump start address.
 *  @param      length          Length of memory starting form start address to hex dump.
 *  @param      bytesPerLine    Bytes per line in hex dump.
 *  @param      showAddress     If true then memory address is shown, else a count from zero.
 */
void SCI_HexDump(std::ostream & outputStream, const void * pMemory, uint64_t length, uint16_t bytesPerLine = 16, bool showAddress = true);

/**
 *  Read ETLM consumed bytes and TcSndUna values.
 *
 *  @param      channelId       Channel id.
 *  @param      consumedBytes   Reference that receives the consumed bytes value.
 *  @param      tcbSndUna       Reference that receives the TcbSndUna value.
 */
void SCI_ReadConsumedBytesAndTcbSndUna(SC_ChannelId channelId, uint32_t & consumedBytes, uint32_t & tcbSndUna);

struct MAC
{
    uint8_t         Address[MAC_ADDR_LEN];
    std::string     AddressString;

    inline explicit MAC()
        : Address()
        , AddressString()
    {
    }

    inline explicit MAC(const MAC & mac)
        : AddressString(mac.AddressString)
    {
        memcpy(Address, mac.Address, sizeof(Address));
    }

    inline MAC & operator=(const MAC & mac)
    {
        if (&mac != this)
        {
            /* See http://cplusplus.bordoon.com/copyConstructors.html about this way of implementing assignment */
            this->MAC::~MAC();  /* First destruct this instance */
            new(this) MAC(mac); /* Then copy construct it in-place */
        }
        return *this;
    }

    inline explicit MAC(const uint8_t macAddress[MAC_ADDR_LEN])
        : Address()
        , AddressString()
    {
        if (macAddress == nullptr)
        {
            memset(Address, 0, sizeof(Address));
        }
        else
        {
            memcpy(Address, macAddress, sizeof(Address));
        }
        AddressString = Stringify(Address);
    }

    static std::string Stringify(const uint8_t macAddress[MAC_ADDR_LEN])
    {
        char macAddressString[19];

        snprintf(macAddressString, sizeof(macAddressString), "%02X:%02X:%02X:%02X:%02X:%02X",
            macAddress[0], macAddress[1], macAddress[2],
            macAddress[3], macAddress[4], macAddress[5]);

        return macAddressString;
    }

    static std::string Stringify(const MAC & mac)
    {
        return Stringify(mac.Address);
    }

    inline const char * Str()
    {
        if (AddressString == "")
        {
            AddressString = Stringify(Address);
        }
        return AddressString.c_str();
    }
};

#if defined(API_TIMING) && API_TIMING == 1

    /**
     *  A class to measure execution time of a scope of code.
     *  Measurement is taken when destructor is executed at the end of scope.
     */
    class _ApiScopedTimer
    {
    private:

        Statistics &        _statistics;
        volatile uint64_t   _startTime;

    public:

        static int64_t      EmptyFunctionOverhead;

        /**
         *  Start time measurement in constructor.
         */
        inline explicit _ApiScopedTimer(Statistics & statistics)
            : _statistics(statistics)
            , _startTime(Stopwatch::GetTimestamp())
        {
        }

        /**
         *  Stop time measurement in destructor.
         */
        inline ~_ApiScopedTimer()
        {
            volatile uint64_t stopTime = Stopwatch::GetTimestamp();
            _statistics.Add(double(int64_t(stopTime) - int64_t(_startTime) /*- EmptyFunctionOverhead*/));
        }
    };

    /**
     *  All available API time measurements.
     */
    enum ApiStatisticsIndex
    {
        API_TIMER_EMPTY_FUNCTION,
        API_TIMER_SEND_BUCKET,
        API_TIMER_SEND_SINGLE_PACKET,
        API_TIMER_UPDATE_CACHED_CONSUMED_BYTES,
        API_TIMER_CAN_SEND_ON_THIS_CHANNEL,
        API_TIMER_TX_DMA_FIRMWARE_FLOW_CONTROL,
        API_TIMER_TX_DMA_SOFTWARE_FLOW_CONTROL,
        API_TIMER_FIRMWARE_CONSUMED_BYTES_FLOW_CONTROL,
        API_TIMER_TCP_WINDOW_FLOW_CONTROL,
        API_TIMER_FLOW_CONTROL,
        API_TIMER_GET_BUCKET,
        API_TIMER_GET_NEXT_PACKET,
        API_TIMER_MAX
    };
    #define ApiScopedTimer(apiStatisticsIndex)    _ApiScopedTimer __apiScopedTimer__(ApiTimerStatistics[apiStatisticsIndex])
    extern Statistics ApiTimerStatistics[API_TIMER_MAX];
#else
    #define ApiScopedTimer(statisticsIndex)
#endif

#endif /* __cplusplus */

/** @endcond PRIVATE_CODE */

#endif /* __product_tools_h__ */
