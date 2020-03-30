#ifndef __product_internal_h__
#define __product_internal_h__

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
 * @brief   Internal nonpublic definitions inside API library, the device driver and some internal applications.
 *          Do not use in client applications. These definitions can change from release to release.
 *
 */

/**
 *  @cond PRIVATE_CODE
 *  The data structure and macros in the PRIVATE_CODE section need to be
 *  defined here such that the inline functions in this file can work. You must
 *  under no circumstances access its contents directly but instead use the
 *  access functions provided.
 */

#include <assert.h>
#include <stdbool.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) /* Microsoft or MingW compiler */

#else

#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>

#endif

/**
 *  Code compilation options. The default values can
 *  be with an environment variable or make command line.
 *
 *  Environment variable example: export SUPPORT_TX_FLOW_CONTROL_STATISTICS=1
 *
 *  Make command line example: make SUPPORT_TX_FLOW_CONTROL_STATISTICS=1
 */

/* Default values of compile time options if no external values are defined: */
#if !defined(SUPPORT_TX_FLOW_CONTROL_STATISTICS) || SC_IS_EMPTY(SUPPORT_TX_FLOW_CONTROL_STATISTICS)
    #undef SUPPORT_TX_FLOW_CONTROL_STATISTICS
    #define SUPPORT_TX_FLOW_CONTROL_STATISTICS      0
#endif

#if !defined(SUPPORT_LOGGING) || SC_IS_EMPTY(SUPPORT_LOGGING)
    #undef SUPPORT_LOGGING
    #define SUPPORT_LOGGING                         0
#endif

#if !defined(SANITY_CHECKS) || SC_IS_EMPTY(SANITY_CHECKS)
    #undef SANITY_CHECKS
    #define SANITY_CHECKS                           0
#endif

#if !defined(USE_SPINLOCK) || SC_IS_EMPTY(USE_SPINLOCK)
    #undef USE_SPINLOCK
    #define USE_SPINLOCK                            1
#endif

#if !defined(API_TIMING) || SC_IS_EMPTY(API_TIMING)
    #undef API_TIMING
    #define API_TIMING                              0
#endif

/* Sanity check of compile time options: */
#if SUPPORT_TX_FLOW_CONTROL_STATISTICS != 0 && SUPPORT_TX_FLOW_CONTROL_STATISTICS != 1
    #error SUPPORT_TX_FLOW_CONTROL_STATISTICS must be 0 or 1!
#endif
#if SUPPORT_LOGGING != 0 && SUPPORT_LOGGING != 1
    #error SUPPORT_LOGGING must be 0 or 1!
#endif
#if SANITY_CHECKS != 0 && SANITY_CHECKS != 1
    #error SANITY_CHECKS must be 0 or 1!
#endif
#if USE_SPINLOCK != 0 && USE_SPINLOCK != 1
    #error USE_SPINLOCK must be 0 or 1!
#endif
#if API_TIMING != 0 && API_TIMING != 1
    #error API_TIMING must be 0 or 1!
#endif

/* C headers: */

#include PRODUCT_H
#include "ctls.h"

#ifdef __cplusplus

/* C++ headers: */

#include PRODUCT_TOOLS_H
#include "CommonUtilities.h"
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Statistics.h"

#endif /* C++ headers */

/**
 *  Collect various statistics for evaluation of Tx flow control performance.
 *  Including the statistics will incur a miniscule performance degradation (<~1%)
 *  but is very useful to evaluate the different parameters affecting Tx flow control.
 */
#if SUPPORT_TX_FLOW_CONTROL_STATISTICS == 1
    #define COLLECT_STATISTICS(x)       x
#else
    #define COLLECT_STATISTICS(x)
#endif /* SUPPORT_TX_FLOW_CONTROL_STATISTICS */

/**
 *  Choose between using spinlocks or mutexes for locking in ETLM Tx flow control.
 *  There is a sweet point where spin lock is faster than mutex.
 *  With lots of threads (> ~30) mutex becomes more effective.
 */
#if USE_SPINLOCK == 1
    typedef pthread_spinlock_t SC_ConsumedBytesLock;    /**< Consumed bytes counter is protected by a spin lock. */
#else
    typedef pthread_mutex_t SC_ConsumedBytesLock;       /**< Consumed bytes counter is protected by a mutex. */
#endif /* USE_SPINLOCK */

#if defined(SUPPORT_LOGGING) && SUPPORT_LOGGING == 1
    // Log if any combinedLogMask bits are set in deviceId->LogMask
    #define LOGANY(deviceId, combinedLogMask)           unlikely((deviceId->LogMask) && ((((static_cast<const SC_DeviceId>(deviceId)->LogMask) & (combinedLogMask)) != 0) || (((combinedLogMask) & (LOG_ALWAYS)) != 0)))
    #define LOGGANY(combinedLogMask)                    unlikely((deviceId->LogMask) && ((((ApiLibraryStaticInitializer::LogMask) & (combinedLogMask)) != 0) || (((combinedLogMask) & (LOG_ALWAYS)) != 0)))
    // Log if all combinedLogMask bits are set in deviceId->LogMask
    #define LOGALL(deviceId, combinedLogMask)           unlikely((deviceId->LogMask) && ((((static_cast<const SC_DeviceId>(deviceId)->LogMask) & (combinedLogMask)) == (combinedLogMask)) || (((combinedLogMask) & (LOG_ALWAYS)) != 0)))
    #define LOGGALL(combinedLogMask)                    unlikely((deviceId->LogMask) && ((((ApiLibraryStaticInitializer::LogMask) & (combinedLogMask)) == (combinedLogMask)) || (((combinedLogMask) & (LOG_ALWAYS)) != 0)))
    #define LOG(deviceId, combinedLogMask)              unlikely((deviceId->LogMask) && (((deviceId)->LogMask & LOG_IF_ALL_BITS_MATCH) ? LOGALL((deviceId), (combinedLogMask)) : LOGANY((deviceId), (combinedLogMask))))
#else
    #define LOGANY(deviceId, combinedLogMask)           false
    #define LOGGANY(combinedLogMask)                    false
    #define LOGALL(deviceId, combinedLogMask)           false
    #define LOGGALL(combinedLogMask)                    false
    #define LOG(deviceId, combinedLogMask)              false
#endif // SUPPORT_LOGGING

/**
 *  Assert that triggers always, also in release builds.
 */
#define SCI_Assert(logical_expression) \
    do \
    { \
        if (!(logical_expression)) \
        { \
            fprintf(stderr, "Assertion '%s' failed! File %s#%d, function %s\n", #logical_expression, __FILE__, __LINE__, __func__); \
        } \
    } while (false)

// C linkage:

// Internal and private send options:
#define SC_SEND_DISABLE_CONSUMER_FLOW_CHECK                 0x1000
#define SC_SEND_FREE_BUCKET_AFTER_ACK_WAIT                  0x2000

#ifdef __cplusplus
extern "C"
{
#endif

struct CallErrorHandlerFunctor
{
    const char *    _fileName;
    int             _lineNumber;
    const char *    _functionName;
    bool            _setLastErrorCode;

    inline explicit CallErrorHandlerFunctor(const char * fileName, int lineNumber, const char * functionName, bool setLastErrorCode)
        : _fileName(fileName), _lineNumber(lineNumber), _functionName(functionName), _setLastErrorCode(setLastErrorCode)
    {
    }

    SC_Error operator()(SC_Error errorCode, const char * format, ...) __attribute__((format(printf, 3, 4)));
};

#define CallErrorHandler                    CallErrorHandlerFunctor(__FILE__, __LINE__, __func__, true)
#define CallErrorHandlerDontSetLastError    CallErrorHandlerFunctor(__FILE__, __LINE__, __func__, false)

/// Indicate that this is the most likely code path taken
#define likely(x)       __builtin_expect((x),1)

/// Indicate that this is the most unlikely code path taken
#define unlikely(x)     __builtin_expect((x),0)

struct SCI_FillingLevels
{
    SC_FillingLevels        FillingLevels;

#if !(__GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ == 7) /* g++ 4.4.7 on CentOS 6.9 cannot handle NO_COPY_CONSTRUCTION_AND_ASSIGNMENT here */
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_FillingLevels);
#endif

public:

    inline explicit SCI_FillingLevels()
        : FillingLevels()
    {
        FillingLevels.StructSize = sizeof(FillingLevels);
    }

    inline ~SCI_FillingLevels()
    {
        assert(FillingLevels.StructSize == sizeof(FillingLevels));
        memset(&FillingLevels, 0, sizeof(FillingLevels));   // Zero fill SC_FillingLevels part
    }
};

struct SCI_LinkStatus
{
    SC_LinkStatus       LinkStatus;

#if !(__GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ == 7) /* g++ 4.4.7 on CentOS 6.9 cannot handle NO_COPY_CONSTRUCTION_AND_ASSIGNMENT here */
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_LinkStatus);
#endif

public:

    inline explicit SCI_LinkStatus()
        : LinkStatus()
    {
        LinkStatus.StructSize = sizeof(LinkStatus);
    }

    inline ~SCI_LinkStatus()
    {
        assert(LinkStatus.StructSize == sizeof(LinkStatus));
        memset(&LinkStatus, 0, sizeof(LinkStatus));   // Zero fill SC_LinkStatus part
    }
};

struct SCI_StatusInfo
{
    SC_StatusInfo       StatusInfo;

    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_StatusInfo);

public:

    std::vector<SCI_LinkStatus>     NetworkInterfacesLinkStatus;        ///< Line status of all network interfaces
    std::vector<SCI_FillingLevels>  ChannelsRxDmaFillings;              ///< Rx DMA fillings for all DMA channels
    std::vector<SC_TcpState>        TcpChannelsTcpConnectionStatus;     ///< TCP connection states for all TOE channels

    inline explicit SCI_StatusInfo(std::size_t numberOfNetworkInterfaces, std::size_t numberOfTcpChannels, std::size_t numberOfChannels)
        : StatusInfo()
        , NetworkInterfacesLinkStatus(numberOfNetworkInterfaces)
        , ChannelsRxDmaFillings(numberOfChannels)
        , TcpChannelsTcpConnectionStatus(numberOfTcpChannels)
    {
        StatusInfo.StructSize = sizeof(StatusInfo);
        StatusInfo.LinkStatus = &NetworkInterfacesLinkStatus[0].LinkStatus;
        StatusInfo.TcpConnectionStatus = &TcpChannelsTcpConnectionStatus[0];
        StatusInfo.RxDmaFillings = &ChannelsRxDmaFillings[0].FillingLevels;
    }

    inline ~SCI_StatusInfo()
    {
        assert(StatusInfo.StructSize == sizeof(StatusInfo));
        memset(&StatusInfo, 0, sizeof(StatusInfo));   // Zero fill SC_StatusInfo part
    }
};

struct SCI_NetworkInterfaceStatistics
{
    SC_NetworkInterfaceStatistics           NetworkInterfaceStatistics;

#if !(__GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ == 7) /* g++ 4.4.7 on CentOS 6.9 cannot handle NO_COPY_CONSTRUCTION_AND_ASSIGNMENT here */
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_NetworkInterfaceStatistics);
#endif

public:

    inline explicit SCI_NetworkInterfaceStatistics()
        : NetworkInterfaceStatistics()
    {
        NetworkInterfaceStatistics.StructSize = sizeof(NetworkInterfaceStatistics);
    }

    inline ~SCI_NetworkInterfaceStatistics()
    {
        assert(NetworkInterfaceStatistics.StructSize == sizeof(NetworkInterfaceStatistics));
        memset(&NetworkInterfaceStatistics, 0, sizeof(NetworkInterfaceStatistics));   // Zero fill SC_NetworkInterfaceStatistics part
    }

    SCI_NetworkInterfaceStatistics & operator=(const sc_stat_nif_t & networkInterfaceStatistics)
    {
        NetworkInterfaceStatistics.NumberOfOkFrames              = networkInterfaceStatistics.okFrames;
        NetworkInterfaceStatistics.NumberOfErrorFrames           = networkInterfaceStatistics.errorFrames;
        NetworkInterfaceStatistics.NumberOfFramesSize_1_63       = networkInterfaceStatistics.size_1_63;
        NetworkInterfaceStatistics.NumberOfFramesSize_64         = networkInterfaceStatistics.size_64;
        NetworkInterfaceStatistics.NumberOfFramesSize_65_127     = networkInterfaceStatistics.size_65_127;
        NetworkInterfaceStatistics.NumberOfFramesSize_128_255    = networkInterfaceStatistics.size_128_255;
        NetworkInterfaceStatistics.NumberOfFramesSize_256_511    = networkInterfaceStatistics.size_256_511;
        NetworkInterfaceStatistics.NumberOfFramesSize_512_1023   = networkInterfaceStatistics.size_512_1023;
        NetworkInterfaceStatistics.NumberOfFramesSize_1024_1518  = networkInterfaceStatistics.size_1024_1518;
        NetworkInterfaceStatistics.NumberOfFramesSize_Above_1518 = networkInterfaceStatistics.size_1519_2047 +
                                                                   networkInterfaceStatistics.size_2048_4095 +
                                                                   networkInterfaceStatistics.size_4096_8191 +
                                                                   networkInterfaceStatistics.size_8192_9022 +
                                                                   networkInterfaceStatistics.size_above_9022;
        NetworkInterfaceStatistics.NumberOfOkBytes               = networkInterfaceStatistics.okBytes;
        NetworkInterfaceStatistics.NumberOfErrorBytes            = networkInterfaceStatistics.errorBytes;
        NetworkInterfaceStatistics.NumberOfRxOverflowFrames      = networkInterfaceStatistics.filter_drops;

        assert(NetworkInterfaceStatistics.StructSize == sizeof(NetworkInterfaceStatistics));

        return *this;
    }
};


struct SCI_PacketStatistics
{
    SC_PacketStatistics         PacketStatistics;

    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_PacketStatistics);

public:

    std::vector<SCI_NetworkInterfaceStatistics> NetworkInterfacesRxStatistics;      ///< Receive statistics for all network interfaces
    std::vector<SCI_NetworkInterfaceStatistics> NetworkInterfacesTxStatistics;      ///< Transmit statistics for all network interfaces

    inline explicit SCI_PacketStatistics(std::size_t maxNumberOfNetworkInterfaces)
        : PacketStatistics()
        , NetworkInterfacesRxStatistics(maxNumberOfNetworkInterfaces)
        , NetworkInterfacesTxStatistics(maxNumberOfNetworkInterfaces)
    {
        PacketStatistics.StructSize = sizeof(PacketStatistics);
        PacketStatistics.RxStatisticsOfNetworkInterfaces = &NetworkInterfacesRxStatistics[0].NetworkInterfaceStatistics;
        PacketStatistics.TxStatisticsOfNetworkInterfaces = &NetworkInterfacesTxStatistics[0].NetworkInterfaceStatistics;
        PacketStatistics.NumberOfNetworkInterfaces = uint16_t(NetworkInterfacesRxStatistics.size());
#ifdef DEBUG
        for (std::size_t i = 0; i < NetworkInterfacesRxStatistics.size(); ++i)
        {
            assert(NetworkInterfacesRxStatistics[i].NetworkInterfaceStatistics.StructSize == sizeof(NetworkInterfacesRxStatistics[i].NetworkInterfaceStatistics));
            assert(NetworkInterfacesTxStatistics[i].NetworkInterfaceStatistics.StructSize == sizeof(NetworkInterfacesTxStatistics[i].NetworkInterfaceStatistics));
        }
#endif /* DEBUG */
    }

    inline ~SCI_PacketStatistics()
    {
        assert(PacketStatistics.StructSize == sizeof(PacketStatistics));
#ifdef DEBUG
        for (std::size_t i = 0; i < NetworkInterfacesRxStatistics.size(); ++i)
        {
            assert(NetworkInterfacesRxStatistics[i].NetworkInterfaceStatistics.StructSize == sizeof(NetworkInterfacesRxStatistics[i].NetworkInterfaceStatistics));
            assert(NetworkInterfacesTxStatistics[i].NetworkInterfaceStatistics.StructSize == sizeof(NetworkInterfacesTxStatistics[i].NetworkInterfaceStatistics));
        }
#endif /* DEBUG */
        memset(&PacketStatistics, 0, sizeof(PacketStatistics));   // Zero fill SC_PacketStatistics part
    }
};

struct SCI_CardInfo
{

    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_CardInfo);

private:

    uint32_t        Zero;   // Dummy zero value to avoid a compiler warning later below.

    struct SCI_NetworkInterfaceInfo
    {
        NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_NetworkInterfaceInfo);

    public:

        std::string                 InterfaceName;

        inline explicit SCI_NetworkInterfaceInfo()
            : InterfaceName()
        {
        }

        inline ~SCI_NetworkInterfaceInfo()
        {
            InterfaceName.clear();
        }

        inline void SetReferencedValues(const char * interfaceName)
        {
            InterfaceName = interfaceName;
        }
    };

    struct SCI_NicInfo
    {
        NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_NicInfo);

    public:

        std::string                 IpAddressString;
        std::string                 IpNetMaskString;
        std::string                 IpBroadcastAddressString;

        inline explicit SCI_NicInfo()
            : IpAddressString()
            , IpNetMaskString()
            , IpBroadcastAddressString()
        {
        }

        inline ~SCI_NicInfo()
        {
            IpAddressString.clear();
            IpNetMaskString.clear();
            IpBroadcastAddressString.clear();
        }

        inline void SetReferencedValues(const char * ipAddress, const char * ipNetMask, const char * ipBroadcastAddress)
        {
            IpAddressString = ipAddress;
            IpNetMaskString = ipNetMask;
            IpBroadcastAddressString = ipBroadcastAddress;
        }
    };

    struct SCI_DmaChannelInfo
    {
        NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_DmaChannelInfo);

    public:

        SC_DmaChannelInfo       DmaChannelInfo;

        inline explicit SCI_DmaChannelInfo()
            : DmaChannelInfo()
        {
            DmaChannelInfo.StructSize = sizeof(DmaChannelInfo);
        }

        inline ~SCI_DmaChannelInfo()
        {
            assert(DmaChannelInfo.StructSize == sizeof(DmaChannelInfo));
            memset(&DmaChannelInfo, 0, sizeof(DmaChannelInfo));   // Zero fill SC_DmaChannelInfo part
        }
    };

    std::string                                 PciDeviceNameString;
    std::string                                 BoardSerialNumberString;
    std::vector<SC_NetworkInterfaceInfo>        NetworkInterfaceInfo;
    std::vector<SCI_NetworkInterfaceInfo>       NetworkInterfaceInfoBackend;
    std::vector<SC_NicInfo>                     NicInfo;
    std::vector<SCI_NicInfo>                    NicInfoBackend;
    std::vector<SC_DmaChannelInfo>              DmaChannelInfo;
    std::vector<std::vector<SC_Memory>>         PacketRingBufferMemoryBlocks;

    void AssignReferencedValues()
    {
        CardInfo.PciDeviceName = PciDeviceNameString.c_str();
        CardInfo.BoardSerialNumberString = BoardSerialNumberString.c_str();
    }

    void AssignReferencedValues(std::size_t i)
    {
        NetworkInterfaceInfo[i].InterfaceName   = NetworkInterfaceInfoBackend[i].InterfaceName.c_str();

        NicInfo[i].IpAddressString = NicInfoBackend[i].IpAddressString.c_str();
        NicInfo[i].IpNetMaskString = NicInfoBackend[i].IpNetMaskString.c_str();
        NicInfo[i].IpBroadcastAddressString = NicInfoBackend[i].IpBroadcastAddressString.c_str();
    }

public:

    SC_CardInfo     CardInfo;

    inline explicit SCI_CardInfo(std::size_t maxNumberOfNetworkInterfaces, std::size_t maxNumberOfDmaChannels)
        : Zero(0)
        , PciDeviceNameString()
        , BoardSerialNumberString()
        , NetworkInterfaceInfo(maxNumberOfNetworkInterfaces)
        , NetworkInterfaceInfoBackend(maxNumberOfNetworkInterfaces)
        , NicInfo(maxNumberOfNetworkInterfaces)
        , NicInfoBackend(maxNumberOfNetworkInterfaces)
        , DmaChannelInfo(maxNumberOfDmaChannels)
        , PacketRingBufferMemoryBlocks(maxNumberOfDmaChannels)
        , CardInfo()
    {
        CardInfo.StructSize = sizeof(CardInfo);
        CardInfo.FpgaInfo.StructSize = sizeof(CardInfo.FpgaInfo);
        AssignReferencedValues();

        CardInfo.NetworkInterfaces = &NetworkInterfaceInfo[0];
        for (std::size_t i = 0; i < NetworkInterfaceInfo.size(); ++i)
        {
            NetworkInterfaceInfo[i].StructSize = sizeof(NetworkInterfaceInfo[i]);
            NetworkInterfaceInfo[i].InterfaceNumber = uint8_t(i);
            NetworkInterfaceInfo[i].Speed = 10 * 1000; // 10 Gbps TODO get this info from driver
            AssignReferencedValues(i);
        }

        CardInfo.NICs = &NicInfo[0];
        for (std::size_t nif = 0; nif < NicInfo.size(); ++nif)
        {
            NicInfo[nif].StructSize = sizeof(NicInfo[nif]);
            NicInfo[nif].InterfaceNumber = static_cast<decltype(NicInfo[nif].InterfaceNumber)>(-1);
            NicInfo[nif].IpAddress = 0;
            NicInfo[nif].IpNetMask = 0;
            NicInfo[nif].IpBroadcastAddress = 0;
            AssignReferencedValues(nif);
        }

        CardInfo.DmaChannels = &DmaChannelInfo[0];
        for (std::size_t dmaChannel = 0; dmaChannel < DmaChannelInfo.size(); ++dmaChannel)
        {
            bool validDmaChannel = true;

            memset(&DmaChannelInfo[dmaChannel], 0, sizeof(DmaChannelInfo[dmaChannel]));

            DmaChannelInfo[dmaChannel].StructSize = sizeof(DmaChannelInfo[dmaChannel]);
            DmaChannelInfo[dmaChannel].DmaChannelNumber = uint8_t(dmaChannel);
            if (dmaChannel >= FIRST_OOB_CHANNEL + Zero && dmaChannel < FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS)
            {
                DmaChannelInfo[dmaChannel].DmaChannelType = SC_DMA_CHANNEL_TYPE_STANDARD_NIC;
            }
            else if (dmaChannel >= FIRST_UL_CHANNEL + Zero && dmaChannel < FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS)
            {
                DmaChannelInfo[dmaChannel].DmaChannelType = SC_DMA_CHANNEL_TYPE_USER_LOGIC;
            }
            else if (dmaChannel >= FIRST_TCP_CHANNEL + Zero && dmaChannel < FIRST_TCP_CHANNEL + SC_MAX_TCP_CHANNELS)
            {
                DmaChannelInfo[dmaChannel].DmaChannelType = SC_DMA_CHANNEL_TYPE_TCP;
            }
            else if (dmaChannel >= FIRST_UDP_CHANNEL + Zero && dmaChannel < FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS)
            {
                DmaChannelInfo[dmaChannel].DmaChannelType = SC_DMA_CHANNEL_TYPE_UDP_MULTICAST;
            }
            else
            {
                validDmaChannel = false;
            }

            PacketRingBufferMemoryBlocks[dmaChannel].resize(64); // TODO FIXME this is maximum possible; use the real number
            SC_MappedMemory & prb = DmaChannelInfo[dmaChannel].PacketRingBuffer;
            memset(&prb, 0, sizeof(prb));
            prb.StructSize              = sizeof(DmaChannelInfo[dmaChannel].PacketRingBuffer);
            prb.Length                  = RECV_DMA_SIZE; // Correct actual value has to be fetched from the driver. This is the default.
            prb.UserSpaceVirtualAddress = 0;
            prb.NumberOfMemoryBlocks    = 0;
            prb.MemoryBlocks            = &PacketRingBufferMemoryBlocks[dmaChannel][0];

            for (std::size_t j = 0; j < PacketRingBufferMemoryBlocks[dmaChannel].size(); ++j)
            {
                SC_Memory & memory = PacketRingBufferMemoryBlocks[dmaChannel][j];
                memset(&memory, 0, sizeof(memory));
                memory.StructSize = sizeof(memory);
            }

            if (validDmaChannel)
            {
                DmaChannelInfo[dmaChannel].MaximumSizeOfFpgaTxPacket     = 10 * 1024;
                DmaChannelInfo[dmaChannel].MaximumSizeOfTxBucketPayload  = SC_MAX_MTU_SIZE;
                DmaChannelInfo[dmaChannel].MaximumNumberOfTxBuckets      = SC_NO_BUCKETS;
            }
            else
            {
                DmaChannelInfo[dmaChannel].MaximumSizeOfFpgaTxPacket     = 0;
                DmaChannelInfo[dmaChannel].MaximumSizeOfTxBucketPayload  = 0;
                DmaChannelInfo[dmaChannel].MaximumNumberOfTxBuckets      = 0;
            }
        }
    }

    inline ~SCI_CardInfo()
    {
        assert(CardInfo.StructSize == sizeof(CardInfo));
        memset(&CardInfo, 0, sizeof(CardInfo));   // Zero fill SC_CardInfo part
#ifdef DEBUG
        for (std::size_t i = 0; i < NetworkInterfaceInfo.size(); ++i)
        {
            assert(NetworkInterfaceInfo[i].StructSize == sizeof(NetworkInterfaceInfo[i]));
        }
        for (std::size_t i = 0; i < DmaChannelInfo.size(); ++i)
        {
            assert(DmaChannelInfo[i].StructSize == sizeof(DmaChannelInfo[i]));
        }
#endif /* DEBUG */
    }

    inline void SetStringValues(const char * pciDeviceNameString, const char * boardSerialNumberString)
    {
        PciDeviceNameString = pciDeviceNameString;
        BoardSerialNumberString = boardSerialNumberString;
        AssignReferencedValues();
        assert(CardInfo.StructSize == sizeof(CardInfo));
    }

    inline void SetValues(std::size_t i,  uint8_t macAddress[MAC_ADDR_LEN], char * interfaceName, const char * ipAddress,
        const char * ipNetMask, const char * ipBroadcastAddress)
    {
        if (macAddress != nullptr)
        {
            memcpy(NetworkInterfaceInfo[i].MacAddress, macAddress, sizeof(NetworkInterfaceInfo[i].MacAddress));
        }
        if (interfaceName != nullptr)
        {
            NetworkInterfaceInfoBackend[i].InterfaceName = interfaceName;
        }
        if (ipAddress != nullptr)
        {
            NicInfoBackend[i].IpAddressString = ipAddress;
        }
        if (ipNetMask != nullptr)
        {
            NicInfoBackend[i].IpNetMaskString = ipNetMask;
        }
        if (ipBroadcastAddress != nullptr)
        {
            NicInfoBackend[i].IpBroadcastAddressString = ipBroadcastAddress;
        }
        AssignReferencedValues(i);
    }
};

struct SCI_UserLogicInterruptOptions
{
    SC_UserLogicInterruptOptions        UserLogicInterruptOptions;

    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SCI_UserLogicInterruptOptions);

public:

    inline explicit SCI_UserLogicInterruptOptions()
        : UserLogicInterruptOptions()
    {
        UserLogicInterruptOptions.StructSize = sizeof(UserLogicInterruptOptions);
    }

    inline ~SCI_UserLogicInterruptOptions()
    {
        assert(UserLogicInterruptOptions.StructSize == sizeof(UserLogicInterruptOptions));
        memset(&UserLogicInterruptOptions, 0, sizeof(UserLogicInterruptOptions));   // Zero fill SC_UserLogicInterruptOptions part
    }

    SCI_UserLogicInterruptOptions & operator=(const SC_UserLogicInterruptOptions & userLogicInterruptOptions)
    {
        assert(userLogicInterruptOptions.StructSize == sizeof(userLogicInterruptOptions));
        if (&userLogicInterruptOptions != &UserLogicInterruptOptions)
        {
            UserLogicInterruptOptions = userLogicInterruptOptions;
        }
        assert(UserLogicInterruptOptions.StructSize == sizeof(UserLogicInterruptOptions));
        return *this;
    }
};

struct SCI_LibraryOptions
{
private:

    std::string                     SharedMemoryFileName;           ///< Name of the shared memory file.

public:

    SC_LibraryOptions               LibraryOptions;

    static std::string              LogFileNameString;              ///< Log file name to be used during static initialization.
    static const char * const       DefaultLogFileName;
    static const char * const       DefaultSharedMemoryFileName;
    static const mode_t             DefaultSharedMemoryPermissions;

    inline explicit SCI_LibraryOptions()
        : SharedMemoryFileName(DefaultSharedMemoryFileName)
        , LibraryOptions()
    {
        LibraryOptions.StructSize = sizeof(LibraryOptions);
        LibraryOptions.SharedMemoryFlowControlOptions.StructSize = sizeof(SC_SharedMemoryFlowControlOptions);
        LibraryOptions.SharedMemoryFlowControlOptions.FileName = SharedMemoryFileName.c_str();
        LibraryOptions.LogFileName = LogFileNameString.c_str();
    }

    inline ~SCI_LibraryOptions()
    {
        assert(LibraryOptions.StructSize == sizeof(LibraryOptions));
        assert(LibraryOptions.SharedMemoryFlowControlOptions.StructSize == sizeof(LibraryOptions.SharedMemoryFlowControlOptions));
        memset(&LibraryOptions, 0, sizeof(LibraryOptions));   // Zero fill SC_LibraryOptions part
    }

    inline SCI_LibraryOptions & operator=(const SC_LibraryOptions & libraryOptions)
    {
        assert(libraryOptions.StructSize == sizeof(libraryOptions));
        assert(libraryOptions.SharedMemoryFlowControlOptions.StructSize == sizeof(SC_SharedMemoryFlowControlOptions));
        if (&libraryOptions != &LibraryOptions)
        {
            LibraryOptions = libraryOptions;
            LogFileNameString = libraryOptions.LogFileName == nullptr ? DefaultLogFileName : libraryOptions.LogFileName;
            LibraryOptions.LogFileName = LogFileNameString.c_str();
            SharedMemoryFileName = libraryOptions.SharedMemoryFlowControlOptions.FileName == nullptr
                ? DefaultSharedMemoryFileName
                : libraryOptions.SharedMemoryFlowControlOptions.FileName;
            LibraryOptions.SharedMemoryFlowControlOptions.FileName = SharedMemoryFileName.c_str();
        }
        assert(LibraryOptions.StructSize == sizeof(LibraryOptions));
        assert(LibraryOptions.SharedMemoryFlowControlOptions.StructSize == sizeof(LibraryOptions.SharedMemoryFlowControlOptions));
        return *this;
    }
};

struct SCI_DeviceOptions
{
    SC_DeviceOptions            DeviceOptions;

    inline explicit SCI_DeviceOptions()
        : DeviceOptions()
    {
        DeviceOptions.StructSize = sizeof(DeviceOptions);
    }

    inline ~SCI_DeviceOptions()
    {
        assert(DeviceOptions.StructSize == sizeof(DeviceOptions));
        memset(&DeviceOptions, 0, sizeof(DeviceOptions));   // Zero fill SC_DeviceOptions part
    }

    inline SCI_DeviceOptions & operator=(const SC_DeviceOptions & deviceOptions)
    {
        assert(deviceOptions.StructSize == sizeof(deviceOptions));
        if (&deviceOptions != &DeviceOptions)
        {
            DeviceOptions = deviceOptions;
        }
        assert(DeviceOptions.StructSize == sizeof(DeviceOptions));
        return *this;
    }
};

struct SCI_ChannelOptions
{
    SC_ChannelOptions           ChannelOptions;

    inline explicit SCI_ChannelOptions()
        : ChannelOptions()
    {
        ChannelOptions.StructSize = sizeof(ChannelOptions);
    }

    inline ~SCI_ChannelOptions()
    {
        assert(ChannelOptions.StructSize == sizeof(ChannelOptions));
        memset(&ChannelOptions, 0, sizeof(ChannelOptions));   // Zero fill SC_ChannelOptions part
    }

    inline SCI_ChannelOptions & operator=(const SC_ChannelOptions & channelOptions)
    {
        assert(channelOptions.StructSize == sizeof(channelOptions));
        if (&channelOptions != &ChannelOptions)
        {
            ChannelOptions = channelOptions;
        }
        assert(ChannelOptions.StructSize == sizeof(ChannelOptions));
        return *this;
    }
};


struct SCI_ConnectOptions
{
    SC_ConnectOptions           ConnectOptions;

    std::string     LocalIpAddressString;

    inline explicit SCI_ConnectOptions()
        : ConnectOptions()
        , LocalIpAddressString()
    {
        ConnectOptions.StructSize = sizeof(ConnectOptions);
    }

    inline ~SCI_ConnectOptions()
    {
        assert(ConnectOptions.StructSize == sizeof(ConnectOptions));
        memset(&ConnectOptions, 0, sizeof(ConnectOptions));   // Zero fill SC_ConnectOptions part
    }

    inline SCI_ConnectOptions & operator=(const SC_ConnectOptions & connectOptions)
    {
        assert(connectOptions.StructSize == sizeof(connectOptions));
        if (&connectOptions != &ConnectOptions)
        {
            ConnectOptions = connectOptions;
            LocalIpAddressString = connectOptions.LocalIpAddress == nullptr ? "" : connectOptions.LocalIpAddress;
            ConnectOptions.LocalIpAddress = LocalIpAddressString.c_str();
        }
        assert(ConnectOptions.StructSize == sizeof(ConnectOptions));
        return *this;
    }
};

struct SCI_ListenOptions
{
    SC_ListenOptions        ListenOptions;

    std::string     LocalIpAddressString;

    inline explicit SCI_ListenOptions()
        : ListenOptions()
        , LocalIpAddressString()
    {
        ListenOptions.StructSize = sizeof(ListenOptions);
    }

    inline ~SCI_ListenOptions()
    {
        assert(ListenOptions.StructSize == sizeof(ListenOptions));
        memset(&ListenOptions, 0, sizeof(ListenOptions));   // Zero fill SC_ListenOptions part
    }

    inline SCI_ListenOptions & operator=(const SC_ListenOptions & listenOptions)
    {
        assert(listenOptions.StructSize == sizeof(listenOptions));
        if (&listenOptions != &ListenOptions)
        {
            ListenOptions = listenOptions;
            LocalIpAddressString = listenOptions.LocalIpAddress == nullptr ? "" : listenOptions.LocalIpAddress;
            ListenOptions.LocalIpAddress = LocalIpAddressString.c_str();
        }
        assert(ListenOptions.StructSize == sizeof(ListenOptions));
        return *this;
    }
};

struct SCI_IgmpOptions
{
    SC_IgmpOptions      IgmpOptions;

    inline explicit SCI_IgmpOptions()
        : IgmpOptions()
    {
        IgmpOptions.StructSize = sizeof(IgmpOptions);
    }

    inline ~SCI_IgmpOptions()
    {
        assert(IgmpOptions.StructSize == sizeof(IgmpOptions));
        memset(&IgmpOptions, 0, sizeof(IgmpOptions));   // Zero fill SC_IgmpOptions part
    }

    inline SCI_IgmpOptions & operator=(const SC_IgmpOptions & igmpOptions)
    {
        assert(igmpOptions.StructSize == sizeof(igmpOptions));
        if (&igmpOptions != &IgmpOptions)
        {
            IgmpOptions = igmpOptions;
        }
        assert(IgmpOptions.StructSize == sizeof(IgmpOptions));
        return *this;
    }
};



/**
 *  Tx DMA software based flow control that sets channel specific limits of how many pending
 *  FIFO entries are allowed on this channel.
 */
typedef struct
{
    uint16_t                FillLevel;                      ///< Current Tx DMA FIFO fill level per channel.
    uint16_t                FillLevelLimit;                 ///< Maximum Tx DMA FIFO fill level per channel allowed.

} TxDmaFifoFlowControl;

/**
 *  Tx DMA bucket used in the API library.
 */
struct __SC_Bucket__
{
    uint64_t                BucketPhysicalAddress;          ///< Buckets physical address in driver used by FPGA DMA.
    sc_bucket *             pUserSpaceBucket;               ///< Pointer to device driver bucket that is mapped into user space.
    volatile uint64_t *     pTxDmaAcknowledgement;          ///< Pointer to where after bucket payload SC_SEND_SINGLE_PACKET
                                                            ///< send receives feedback that the packet has left
                                                            ///< Tx DMA FIFO and can be resued.
                                                            ///< The value is either @ref SC_BUCKET_ACKNOWLEDGE_PENDING or @ref SC_BUCKET_ACKNOWLEDGED.
    TxDmaFifoFlowControl *  pTxDmaFifoFlowControl;          ///< Pointer Tx DMA FIFO control whoes fill level needs decrementing
                                                            ///< after packet send has been confirmed.
    SC_Error                LastErrorCode;                  ///< Last error code from SC_SendBucket. Used to remember if it was SC_ERR_PIPE_FULL which allows retry.
    uint64_t                SendCount;                      ///< Increasing send count, just for debugging purposes.
    uint16_t                PayloadLength;                  ///< Bucket length; used by flow control mechanism
    uint8_t                 BucketNumber;                   ///< Bucket number for internal API book keeping.
    bool                    BucketIsUsed;                   ///< Bucket is in use.

};

/**
 *  Second level buffer flow control based on a consumed bytes count.
 */
typedef struct
{
    int64_t                     SentBytesCount;                 ///< Number of bytes sent modulo 4 GB so far either per channel or per ETLM for TCP.
    uint32_t                    CachedLastConsumedBytesCount;   ///< Cached last consumed bytes count that was read from the register
    uint32_t                    PreviousConsumedBytesCount;     ///< Previous value of last consumed bytes count read from the FPGA register.
    uint32_t                    BufferSize;                     ///< Size of the channel specific buffer.
    uint32_t                    ListFlushLimit;                 ///< List flush limit when it will be automatically flushed.
    SC_ConsumedBytesLock *      pLock;                          ///< Lock to protect multithreaded access, only used for TCP channels, NULL otherwise.

} SC_TxConsumedBytesFlowControl;

/**
 *  ETLM TCP window flow control based on consumed bytes as reported by the TcbSndUna count.
 */
typedef struct
{
    int64_t                 SentBytesCount;                 ///< Number of bytes sent on this channel so far modulo 4 GB.
    uint32_t                CachedLastTcbSndUna;            ///< Cached last ETLM TcbSndUna value that was read from the register
    uint32_t                PreviousLastTcbSndUna;          ///< Cached last ETLM TcbSndUna value that was read from the register
    uint32_t                EtlmTxWindowSize;               ///< ETLM TX windows size.

} SC_TxWindowFlowControl;

/**
 *  Tx DMA list entry for book keeping of all data needed
 *  in building and sending a Tx DMA list. Both @ref SC_SEND_SINGLE_LIST
 *  and @ref SC_SEND_QUEUE_LIST options use this data.
 */
typedef struct
{
    volatile uint64_t *     pUserSpaceStart;                ///< Virtual address of list start in user space.
    uint64_t                KernelSpaceStart;               ///< List physical start in kernel space.
    uint32_t                CumulatedPayloadLength;         ///< Cumulated (accumulated) payload length of all packets in the list.
    SC_Bucket *             Buckets[32];                    /**< List of buckets referred to in the list (FPGA theoretical maximum is 2048 buckets in a single list!).
                                                                 Maximum number of lists is the FIFO size of 512. */
    TxDmaFifoFlowControl *  pTxDmaFifoFlowControl;          ///< Pointer to last channel Tx DMA flow control that sent this list or NULL
                                                            ///< if the fillCount has been already decremented.
    SC_Error                LastErrorCode;                  ///< Last error code from SC_SendBucket. Used to remember if it was SC_ERR_PIPE_FULL which allows retry.
    uint16_t                ListLength;                     ///< Length of list as number of buckets included.
    bool                    ListHasBeenFlushed;             ///< Tells if the list has been flushed or not.
    bool                    Priority;                       ///< Select normal or priority FIFO in case list is automatically flushed.
} TxDmaList;

/**
 *  A static initializer to initialize static stuff in a known order and central place.
 */
class ApiLibraryStaticInitializer
{
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(ApiLibraryStaticInitializer);

private:

    ApiLibraryStaticInitializer();

public:

    mutable tthread::mutex          ErrorHandlingLock;          ///< Lock used to protect multithreaded access to the error handler function and context.
    SC_ErrorHandler *               pErrorHandler;              ///< Pointer to a user defined error handler function.
    void *                          pErrorHandlerUserContext;   ///< Pointer to a user defined error handler function context.

    SCI_DeviceOptions               DefaultDeviceOptions;
    SCI_ChannelOptions              DefaultChannelOptions;
    SCI_ConnectOptions              DefaultConnectOptions;
    SCI_ListenOptions               DefaultListenOptions;
    SCI_IgmpOptions                 DefaultIgmpOptions;

    bool                            LibraryARP;                 ///< Resolve ARP in user space library if true, otherwise resolve ARP in driver.
    SCI_UserLogicInterruptOptions   DefaultUserLogicInterruptOptions;

    SCI_LibraryOptions              LibraryOptions;

    bool                            BucketViolationsAllowed;    ///< Only for internal test use: allow bucket integrity violations.

    ~ApiLibraryStaticInitializer();

    static SC_Error SanityCheckOptions(SC_LibraryOptions * pLibraryOptions);
    static SC_Error SanityCheckOptions(SC_DeviceId deviceId, SC_DeviceOptions * pDeviceOptions);
    static SC_Error SanityCheckOptions(SC_DeviceId deviceId, const SC_ChannelOptions * pChannelOptions);
    static SC_Error SanityCheckOptions(SC_DeviceId deviceId, SC_ConnectOptions * pConnectOptions);
    static SC_Error SanityCheckOptions(SC_DeviceId deviceId, SC_ListenOptions * pListenOptions);
    static SC_Error SanityCheckOptions(SC_DeviceId deviceId, SC_IgmpOptions * pIgmpOptions);
    static SC_Error SanityCheckOptions(SC_DeviceId deviceId, SC_UserLogicInterruptOptions * pUserLogicInterruptOptions);

    static volatile uint64_t        LogMask;                    ///< Global logging mask for use when device id is not available.
    static volatile SC_Error        LastErrorCode;              /**< Last error code in the library. Make this a static member
                                                                so it can be accessed efficiently in the @ref Return function.
                                                                All other accesses are through the @ref GetInstance and
                                                                @ref GetConstInstance methods. */

    static ApiLibraryStaticInitializer & GetInstance();
    static const ApiLibraryStaticInitializer & GetConstInstance();

    static inline bool IsStaticInitializationInProgress()
    {
        return StaticInitializationInProgress;
    }

private:

    static volatile bool    StaticInitializationInProgress;     ///< Only used while executing the @ref ApiLibraryStaticInitializer constructor

    void _HandleSanityCheckError(SC_Error errorCode, const char * nameOfSanityCheckFunction);
    #define HandleSanityCheckError(errorCode)       _HandleSanityCheckError(errorCode, __func__)
};

inline const ApiLibraryStaticInitializer & ApiLibraryStaticInitializer::GetConstInstance()
{
    return ApiLibraryStaticInitializer::GetInstance();
}

/**
 *  A static initializer to initialize static thread specific stuff in a known order.
 */
struct ApiLibraryThreadStaticInitializer
{
    volatile static __thread SC_Error      LastThreadErrorCode;

    static ApiLibraryThreadStaticInitializer & GetThreadInstance();
    static inline const ApiLibraryThreadStaticInitializer & GetThreadConstInstance()
    {
        return GetThreadInstance();
    }
};

static SC_Error inline SetLastErrorCode(SC_Error errorCode)
{
    ApiLibraryStaticInitializer::LastErrorCode = errorCode;
#ifdef DEBUG // Debug feature to keep a thread specific last error code, not included in release code.
    ApiLibraryThreadStaticInitializer::GetThreadInstance().LastThreadErrorCode = errorCode;
#endif
    return errorCode;
}

// __attribute__((format(printf, 2, 3))): 2 = position of format, 3 = position of arguments, position indexing starts from 1!
void Log(SC_DeviceId deviceId, const char * format, ...) __attribute__((format(printf, 2, 3)));
void LogError(SC_DeviceId deviceId, const char * format, ...) __attribute__((format(printf, 2, 3)));
void LogNoTime(SC_DeviceId deviceId, const char * format, ...) __attribute__((format(printf, 2, 3)));
void LogToFile(FILE * pFile, SC_DeviceId deviceId, const char * format, ...) __attribute__((format(printf, 3, 4)));
void LogErrorToFile(FILE * pFile, SC_DeviceId deviceId, const char * format, ...) __attribute__((format(printf, 3, 4)));
void LogToFileNoTime(FILE * pFile, SC_DeviceId deviceId, const char * format, ...) __attribute__((format(printf, 3, 4)));

uint64_t Log2n(uint64_t n);
bool IsPowerOfTwo(uint64_t n);
int GetPositionOfSingleBit(uint64_t n);

/// @endcond

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

/* C++ linkage: */

/**
 *  Deleter function that does nothing, i.e. does not delete.
 */
template<typename T> inline void NullDeleter(T *) {}

/**
 *  ARP handling.
 */
class ARP
{
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(ARP);

public:

    enum ArpMethod
    {
        ARP_METHOD_NONE,                            ///< No ARP method defined.
        ARP_METHOD_ARPING_UTILITY,                  ///< Use Linux arping utility
        ARP_METHOD_PING_UTILITY,                    ///< Use Linux ping utility
        ARP_METHOD_ARP_REQUEST_VIA_DRIVER,          ///< Make an ARP request via the driver
        ARP_METHOD_ARP_TABLE_LOOKUP_ONLY            ///< Linux ARP table lookup only
    };

private:

    /**
     *  ARP information needed for ETLM connects (@ref SC_Connect).
     *  This information is retrieved from the Linux ARP table /proc/net/arp.
     */
    struct ARP_Info
    {
        uint32_t        IpAddress;                  ///< IP version 4 address
        std::string     Host;                       ///< IP address as string
        uint32_t        HWType;                     ///< HW type
        uint32_t        Flags;                      ///< Flags
        MacAddress_t    MacAddress;                 ///< Corresponding MAC address for the above host.
        std::string     Mask;                       ///< Mask
        std::string     Interface;                  ///< Name of network interface

        explicit ARP_Info()
            : IpAddress(0)
            , Host()
            , HWType(0)
            , Flags(0)
            , MacAddress()
            , Mask()
            , Interface()
        {
        }
    };

    SC_DeviceId             _deviceId;              ///< Device id
    std::vector<ARP_Info>   _arpTable;              ///< ARP table
    std::string             _linuxArpTableName;     ///< Name of Linux ARP table file (default is /proc/net/arp)

    int                     _retryLimit;            ///< ARP request retry limit
    int                     _retryCount;            ///< ARP request retry count
    std::string             _quiet;                 ///< Quiet arping or ping if " -q", empty otherwise (not quiet)

public:

    /**
     *  Construct an ARP handler.
     *
     *  @param      deviceId            Device identifier
     *  @param      linuxArpTableName   Path to Linux ARP table
     */
    explicit ARP(SC_DeviceId deviceId, const std::string & linuxArpTableName = "/proc/net/arp");

    /**
     *  Update ARP information by reading the Linux ARP table via the file system.
     */
    SC_Error UpdateARP();

    /**
     *  Dump internal ARP table.
     *
     *  @param  pFile       Pointer to file.
     */
    void DumpARP(FILE * pFile);

    /**
     *  Make an ARP request with different methods.
     *
     *  @param  arpMethod               ARP request method to use.
     *  @param  networkInterface        Network interface in the physical port.
     *  @param  interface               Name of network interface to use for ARP requests.
     *  @param  remoteHost              Remote host IP address as string
     *  @param  vlanTag                 VLAN tag or NO_VLAN_TAG if not a VLAN.
     *
     *  @return             SC_ERR_SUCCESS on success, an error code otherwise.
     */
    SC_Error MakeArpRequest(ArpMethod arpMethod, uint8_t networkInterface, const std::string & interface, const std::string & remoteHost, int16_t vlanTag);

    /**
     *  Find a MAC address of a host. If internal table does not have the address and #arping is true then
     *  either Linux arping command is called or internal arping code is used to similar effect.
     *
     *  @param  arpMethod                       The ARP method to resolve the MAC address.
     *  @param  networkInterface                Network interface in the physical port.
     *  @param  interface                       Name of network interface to use for ARP requests.
     *  @param  remoteHost                      Remote host IP address as string
     *  @param  vlanTag                         VLAN tag or NO_VLAN_TAG if not a VLAN.
     *  @param  macAddress                      MAC that is returned if found. If not found MAC address zero.
     *  @param  acquireMacAddressViaArpTimeout  Timeout in milliseconds for acquiring MAC address via ARP.
     *
     *  @return             SC_ERR_SUCCESS on success, an error code otherwise.
     */
    SC_Error GetMacAddress(ArpMethod arpMethod, uint8_t networkInterface, const std::string & interface, const std::string & remoteHost,
        int16_t vlanTag, uint8_t macAddress[MAC_ADDR_LEN], uint64_t acquireMacAddressViaArpTimeout);
};

/**
 *  Common context base for both device and channel contexts.
 */
struct CommonContextBase
{
private:

    SC_Error                        _firstErrorCode;                        ///< First error code indicating something went wrong in constructor
    SC_Error                        _lastErrorCode;                         ///< Last error code indicating something went wrong in constructor

protected:

    /**
     *  Set error code that occurred during construction.
     */
    inline void SetConstructorErrorCode(SC_Error errorCode)
    {
        if (_firstErrorCode == SC_ERR_SUCCESS)
        {
            _firstErrorCode = errorCode;
        }
        _lastErrorCode = errorCode;
    }

    inline explicit CommonContextBase(int fileDescriptor)
        : _firstErrorCode(SC_ERR_SUCCESS)
        , _lastErrorCode(SC_ERR_SUCCESS)
        , FileDescriptor(fileDescriptor)
    {
    }

    ~CommonContextBase()
    {
        FileDescriptor = -1;
    }

public:

    int                                         FileDescriptor;                         ///< File descriptor

    /**
     *  Return any error codes that were set in constructor.
     */
    inline SC_Error GetConstructorErrorCode() const
    {
        if (_firstErrorCode != SC_ERR_SUCCESS)
        {
            return _firstErrorCode;
        }
        return _lastErrorCode;
    }
};

/**
 *  Context base for a device. Only C type members so it can be initialized with memset.
 */
class DeviceContextBase : public CommonContextBase
{
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(DeviceContextBase);

protected:

    explicit DeviceContextBase(int fileDescriptor);
    ~DeviceContextBase();

public:

    int                             LogMask;                                ///< Logging mask

    const sc_status_dma_t *         pStatusDma;                             ///< Pointer to start of mapped status DMA memory

    SCI_DeviceOptions               InternalDeviceOptions;                  ///< Device options use in SC_OpenDevice
    SCI_CardInfo                    InternalCardInfo;                       ///< Static card info
    SCI_UserLogicInterruptOptions   InternalUserLogicInterruptOptions;      ///< User logic interrupts information.

    bool                            TxCapable;                              ///< Is the channel able to do Tx
    void *                          pBar0Registers;                         ///< Pointer to the start of user space mapped FPGA registers in BAR 0
    volatile uint64_t *             pUserLogicRegisters;                    ///< Pointer to the start of user space mapped FPGA user logic registers in BAR 2
    sc_pldma_t *                    pPointerListDMA;                        ///< Pointer to start of mapped pointer list of PL DMA memory


    SC_ChannelId                    Channels[SC_MAX_CHANNELS];              ///< All channels of this device

    // Tx flow control parameters
    uint16_t                        MaxNormalFifoConcurrency;
    uint16_t                        MaxPriorityFifoConcurrency;

    // Tx flow control
    volatile uint64_t *             pTxDmaRequestFifoFillLevelsRegister;    ///< Pointer to Tx DMA request FIFO fill levels register.
    SC_TxConsumedBytesFlowControl   TcpTxConsumedBytesFlowControl;          ///< TCP channels Tx common consumed bytes control

}; // DeviceContextBase

/**
 *  Device context.
 */
struct SC_DeviceContext : public DeviceContextBase
{
    std::string                     DeviceName;                     ///< Device name.
    tthread::mutex                  ChannelsLock;

    mutable SC_ConsumedBytesLock    TcpConsumedBytesLock;           ///< Lock to protect multithreaded access to TCP common consumed bytes

    std::stringstream               SharedMemoryLogStream;

    ARP                             Arp;                            ///< Handling of ARP related stuff.

    SCI_StatusInfo                  InternalStatusInfo;             ///< Dynamic status information returned by @ref SC_GetStatusInfo
    SCI_PacketStatistics            InternalPacketStatistics;       ///< Packet statistics returned by @ref SC_GetStatistics

    volatile uint64_t*                 pEkalineWcBase;                         ///< Pointer to the start of Ekaline WC base at BAR 0



    /**
     *  Device context constructor.
     */
    explicit SC_DeviceContext(const char * deviceName, int fileDescriptor);

    /**
     *  Device context destructor.
     */
    ~SC_DeviceContext();
};

/**
 *  Channel context base. Only C type members so it can be initialized with memset.
 */
class ChannelContextBase : public CommonContextBase
{
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(ChannelContextBase);

protected:

    explicit ChannelContextBase(int fileDescriptor);
    ~ChannelContextBase();

public:

    SC_DeviceId                                 DeviceId;                               ///< Channel context is associated with a single device context.
    int16_t                                     DmaChannelNumber;                       ///< Physical DMA channel number (0-127)
    sc_channel_type_t                           ChannelType;                            ///< Channel type
    uint8_t                                     NetworkInterface;                       ///< Network interface or physical interface for this channel.

    SCI_ChannelOptions                          InternalChannelOptions;
    SCI_ConnectOptions                          InternalConnectOptions;
    SCI_ListenOptions                           InternalListenOptions;
    SCI_IgmpOptions                             InternalIgmpOptions;

    // TODO: remove redundant mappings of FPGA registers, PL DMA and status DMA
    void *                                      pTotalMemory;                           ///< Pointer to location in user space of different FPGA DMA memory areas
    uint64_t                                    PrbSize;                                ///< Size of DMA channel Packet Ring Buffer (PRB).
    uint8_t *                                   pPRB;                                   ///< Pointer to Packet Ring Buffer (PRB) that is mapped into user space.

    // Rx stuff
    uint64_t                                    PrbPhysicalAddress;                     ///< Physical address of PRB as seen by the FPGA
    uint64_t                                    PrbFpgaStartAddress;                    ///< PRB start address, either physical address when MMU is not used or
                                                                                        ///< virtual MMU address if MMU is in use.
    volatile uint64_t *                         pReadMarkerRegister;                    ///< Pointer to channel read marker register.
    uint64_t                                    LastPacketPlDmaAddress;                 ///< Last cached pointer to head of PRB as read from PL DMA - HW address
    sc_pldma_t *                                pPointerListDMA;                        ///< Pointer to start of mapped pointer list of PL DMA memory. TODO REMOVE! Use device context pl instead!
    sc_pldma_t                                  PreviousPointerListDMA;                 ///< Previous contents pointer list of PL DMA memory. TODO REMOVE! Use device context pl instead!

    // Tx stuff:

    // Bucket book keeping:
    SC_Bucket                                   Buckets[SC_NO_BUCKETS];
    int                                         UsedBucketsCount;
    uint32_t                                    NextFreeBucketIndex;

    // Tx DMA specific stuff
    SC_SendOptions                              DefaultSendOptions;                     ///< Default @ref SC_SendBucket options for this channel

    // Tx DMA lists:
    TxDmaList                                   TxDmaLists[16];
    TxDmaList *                                 pCurrentTxDmaList;
    uint32_t                                    TxDmaListsMappedSize;
    uint32_t                                    NumberOfTxDmaLists;
    uint32_t                                    TxDmaListSize;
    uint32_t                                    FreeTxDmaSearchStartIndex;
    uint64_t                                    FreeTxDmaListWaitTime;

    //  Tx flow control:
    volatile uint64_t *                         pTcbSndUnaAndConsumedBytesRegister;     ///< Pointer to channel combined TcbSndUna and consumed
                                                                                        ///< bytes count register
    TxDmaFifoFlowControl                        TxDmaNormalFifoFlowControl;             ///< Tx DMA normal FIFO flow control based on a channel
                                                                                        ///< specific request count and limit
    TxDmaFifoFlowControl                        TxDmaPriorityFifoFlowControl;           ///< Tx DMA priority FIFO flow control based on a channel
                                                                                        ///< specific request count and limit
    volatile SC_TxConsumedBytesFlowControl *    pTxConsumedBytesFlowControl;
    SC_TxConsumedBytesFlowControl               TxConsumedBytesFlowControl;
    SC_TxWindowFlowControl *                    pTxWindowFlowControl;
    SC_TxWindowFlowControl                      TxWindowFlowControl;
    SC_TxFlowControlStatistics                  TxFlowControlStatistics;

    uint64_t                                    ListBucketSequenceNumber;               ///< Sequence number counting sent buckets.
    uint64_t                                    SentBucketsCount;

}; // ChannelContextBase

/**
 *  Channel context. C++ class members allowed.
 */
struct SC_ChannelContext : public ChannelContextBase
{
public:

    std::string                 InterfaceName;      ///< Interface name corresponding the member NetworkInterface in @ref ChannelContextBase.

    // Used in SC_Disconnect:
    std::string                 RemoteAddress;
    uint16_t                    RemotePort;

    // Used in SC_Listen:
    std::string                 ListenRemoteIpAddress;  ///< Storage for SC_ListenOptions.RemoteIpAddress pointer

    COLLECT_STATISTICS
    (
        std::vector<Statistics> AllStatistics;
        Statistics &            FlowControlTime;    ///< Time used for flow control when sending packets
        Statistics &            SendPacketTime;     ///< Time used for send packet itself.
        Statistics &            SendBucketTime;     ///< Time between 2 consecutive calls to SC_SendBucket.
        Statistics &            GetNextPacketTime;  ///< Time between 2 consecutive calls to SC_GetNextPacket.

        uint64_t                PreviousSendBucketTime;
        uint64_t                PreviousGetNextPacketTime;
    )

    /**
     *  Construct a channel context.
     */
    explicit SC_ChannelContext(int fileDescriptor);

    /**
     *  Cleanup and destroy a channel context.
     */
    ~SC_ChannelContext();
};

#include <cxxabi.h> // GNU ABI

inline std::string DemangleName(const std::string & nameToDemangle)
{
    // __cxxabiv1 is a namespace

    int status = 0;
    char * buff = __cxxabiv1::__cxa_demangle(nameToDemangle.c_str(), NULL, NULL, &status);
    std::string demangled = buff;
    std::free(buff);
    return demangled;
}


#endif /* __cplusplus */

#endif /* __product_internal_h__ */
