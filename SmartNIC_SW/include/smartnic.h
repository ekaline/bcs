#ifndef __public_api_h__
#define __public_api_h__

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
 * @brief Silicom API library public declarations and function definitions.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <signal.h>
#include <stdint.h>
#include <stdio.h>

/**! @cond Doxygen_Suppress */

#define SC_LEGACY_API
#ifdef SC_LEGACY_API

    /* Map old Silicom SmartNIC API to new Silicom generic API */

#define SN_DEMO_USER_LOGIC_GENERIC SC_DEMO_USER_LOGIC_GENERIC
#define SN_DEMO_USER_LOGIC_INTERRUPTS SC_DEMO_USER_LOGIC_INTERRUPTS
#define SN_DEMO_USER_LOGIC_RTT_LATENCY SC_DEMO_USER_LOGIC_RTT_LATENCY
#define SN_ERR_ACL_NOT_PRESENT SC_ERR_ACL_NOT_PRESENT
#define SN_ERR_ACL_OPERATION_FAILED SC_ERR_ACL_OPERATION_FAILED
#define SN_ERR_ARP_FAILED SC_ERR_ARP_FAILED
#define SN_ERR_BUCKET_PHYSICAL_ADDRESS_FAILED SC_ERR_BUCKET_PHYSICAL_ADDRESS_FAILED
#define SN_ERR_CHANNEL_ALLOCATION_FAILED SC_ERR_CHANNEL_ALLOCATION_FAILED
#define SN_ERR_CHANNEL_DEALLOCATION_FAILED SC_ERR_CHANNEL_DEALLOCATION_FAILED
#define SN_ERR_DEMO_DESIGN_FAILED SC_ERR_DEMO_DESIGN_FAILED
#define SN_ERR_DMA_BUFFER_FAILED SC_ERR_DMA_BUFFER_FAILED
#define SN_ERR_FAILED_TO_ABORT_ETLM_CHANNEL SC_ERR_FAILED_TO_ABORT_ETLM_CHANNEL
#define SN_ERR_FAILED_TO_CLOSE_DEVICE SC_ERR_FAILED_TO_CLOSE_DEVICE
#define SN_ERR_FAILED_TO_CONNECT SC_ERR_FAILED_TO_CONNECT
#define SN_ERR_FAILED_TO_DISCONNECT SC_ERR_FAILED_TO_DISCONNECT
#define SN_ERR_FAILED_TO_DUMP_RECEIVE_INFO SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO
#define SN_ERR_FAILED_TO_ENUMERATE_NETWORK_INTERFACES SC_ERR_FAILED_TO_ENUMERATE_NETWORK_INTERFACES
#define SN_ERR_FAILED_TO_GET_BOARD_MASKS SC_ERR_FAILED_TO_GET_BOARD_MASKS
#define SN_ERR_FAILED_TO_GET_BOARD_SERIAL SC_ERR_FAILED_TO_GET_BOARD_SERIAL
#define SN_ERR_FAILED_TO_GET_CARD_INFO SC_ERR_FAILED_TO_GET_CARD_INFO
#define SN_ERR_FAILED_TO_GET_DRIVER_LOG_MASK SC_ERR_FAILED_TO_GET_DRIVER_LOG_MASK
#define SN_ERR_FAILED_TO_GET_DRIVER_VERSION SC_ERR_FAILED_TO_GET_DRIVER_VERSION
#define SN_ERR_FAILED_TO_GET_FRAME_STATISTICS SC_ERR_FAILED_TO_GET_FRAME_STATISTICS
#define SN_ERR_FAILED_TO_GET_INFO SC_ERR_FAILED_TO_GET_INFO
#define SN_ERR_FAILED_TO_GET_INTERFACE_NAME SC_ERR_FAILED_TO_GET_INTERFACE_NAME
#define SN_ERR_FAILED_TO_GET_LAST_TCP_ACK_NUMBER SC_ERR_FAILED_TO_GET_LAST_TCP_ACK_NUMBER
#define SN_ERR_FAILED_TO_GET_LINK_STATUS SC_ERR_FAILED_TO_GET_LINK_STATUS
#define SN_ERR_FAILED_TO_GET_LOCAL_ADDRESS SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS
#define SN_ERR_FAILED_TO_GET_NEXT_PACKET SC_ERR_FAILED_TO_GET_NEXT_PACKET
#define SN_ERR_FAILED_TO_GET_PCI_DEVICE_NAME SC_ERR_FAILED_TO_GET_PCI_DEVICE_NAME
#define SN_ERR_FAILED_TO_GET_TCP_STATUS SC_ERR_FAILED_TO_GET_TCP_STATUS
#define SN_ERR_FAILED_TO_JOIN_IGMP_GROUP SC_ERR_FAILED_TO_JOIN_IGMP_GROUP
#define SN_ERR_FAILED_TO_LEAVE_IGMP_GROUP SC_ERR_FAILED_TO_LEAVE_IGMP_GROUP
#define SN_ERR_FAILED_TO_LISTEN_OR_CANCEL_LISTEN SC_ERR_FAILED_TO_LISTEN_OR_CANCEL_LISTEN
#define SN_ERR_FAILED_TO_OPEN_DEVICE SC_ERR_FAILED_TO_OPEN_DEVICE
#define SN_ERR_FAILED_TO_READ_FPGA_TEMPERATURE SC_ERR_FAILED_TO_READ_FPGA_TEMPERATURE
#define SN_ERR_FAILED_TO_READ_FPGA_VERSION SC_ERR_FAILED_TO_READ_FPGA_VERSION
#define SN_ERR_FAILED_TO_READ_USER_LOGIC_REGISTER SC_ERR_FAILED_TO_READ_USER_LOGIC_REGISTER
#define SN_ERR_FAILED_TO_RESET_FPGA SC_ERR_FAILED_TO_RESET_FPGA
#define SN_ERR_FAILED_TO_SET_DRIVER_LOG_MASK SC_ERR_FAILED_TO_SET_DRIVER_LOG_MASK
#define SN_ERR_FAILED_TO_SET_LOCAL_ADDRESS SC_ERR_FAILED_TO_SET_LOCAL_ADDRESS
#define SN_ERR_FAILED_TO_SET_TIME_STAMPING_MODE SC_ERR_FAILED_TO_SET_TIME_STAMPING_MODE
#define SN_ERR_FAILED_TO_UPDATE_RECEIVE_PTR SC_ERR_FAILED_TO_UPDATE_RECEIVE_PTR
#define SN_ERR_FPGA_DOES_NOT_SUPPORT_FRAME_STATISTICS SC_ERR_FPGA_DOES_NOT_SUPPORT_FRAME_STATISTICS
#define SN_ERR_HANDLER_CONTINUE SC_ERR_HANDLER_CONTINUE
#define SN_ERR_HANDLER_THROW_EXCEPTION SC_ERR_HANDLER_THROW_EXCEPTION
#define SN_ERR_HANDLER_WRITE_TO_STDERR_AND_CONTINUE SC_ERR_HANDLER_WRITE_TO_STDERR_AND_CONTINUE
#define SN_ERR_INVALID_ARGUMENT SC_ERR_INVALID_ARGUMENT
#define SN_ERR_INVALID_TCP_STATE SC_ERR_INVALID_TCP_STATE
#define SN_ERR_LISTEN_CANCELLED_BY_USER SC_ERR_LISTEN_CANCELLED_BY_USER
#define SN_ERR_LOCKING_FAILED SC_ERR_LOCKING_FAILED
#define SN_ERR_MMAP_OF_BUFFERS_FAILED SC_ERR_MMAP_OF_BUFFERS_FAILED
#define SN_ERR_MMAP_OF_FPGA_REGISTERS_FAILED SC_ERR_MMAP_OF_FPGA_REGISTERS_FAILED
#define SN_ERR_NO_MORE_BUCKETS_AVAILABLE SC_ERR_NO_MORE_BUCKETS_AVAILABLE
#define SN_ERR_NOTHING_TO_FLUSH SC_ERR_NOTHING_TO_FLUSH
#define SN_ERR_NOT_SUPPORTED_OR_NOT_IMPLEMENTED SC_ERR_NOT_SUPPORTED_OR_NOT_IMPLEMENTED
#define SN_ERR_OUT_OF_MEMORY SC_ERR_OUT_OF_MEMORY
#define SN_ERR_PIPE_FULL SC_ERR_PIPE_FULL
#define SN_ERR_RECEIVE_PHYSICAL_ADDRESS_FAILED SC_ERR_RECEIVE_PHYSICAL_ADDRESS_FAILED
#define SN_ERR_SHARED_MEMORY_FAILED SC_ERR_SHARED_MEMORY_FAILED
#define SN_ERR_SUCCESS SC_ERR_SUCCESS
#define SN_ERR_TIMEOUT SC_ERR_TIMEOUT
#define SN_ERR_TX_DMA_WAIT_FAILED SC_ERR_TX_DMA_WAIT_FAILED
#define SN_ERR_UNSPECIFIED SC_ERR_UNSPECIFIED
#define SN_FLUSH_NORMAL SC_FLUSH_NORMAL
#define SN_FLUSH_PRIORITY SC_FLUSH_PRIORITY
#define SN_FLUSH_WAIT_FOR_COMPLETION SC_FLUSH_WAIT_FOR_COMPLETION
#define SN_FLUSH_WAIT_FOR_PIPE_FREE SC_FLUSH_WAIT_FOR_PIPE_FREE
#define SN_Packet__ SC_Packet__
#define SN_SEND_DEFAULT SC_SEND_DEFAULT
#define SN_SEND_NONE SC_SEND_NONE
#define SN_SEND_PRIORITY SC_SEND_PRIORITY
#define SN_SEND_QUEUE_LIST SC_SEND_QUEUE_LIST
#define SN_SEND_SINGLE_LIST SC_SEND_SINGLE_LIST
#define SN_SEND_SINGLE_PACKET SC_SEND_SINGLE_PACKET
#define SN_SEND_WAIT_FOR_COMPLETION SC_SEND_WAIT_FOR_COMPLETION
#define SN_SEND_WAIT_FOR_PIPE_FREE SC_SEND_WAIT_FOR_PIPE_FREE
#define SN_TCP_CLOSED SC_TCP_CLOSED
#define SN_TCP_ESTABLISHED SC_TCP_ESTABLISHED
#define SN_TCP_FIN_WAIT SC_TCP_FIN_WAIT
#define SN_TIMEOUT_INFINITE SC_TIMEOUT_INFINITE
#define SN_TIMEOUT_NONE SC_TIMEOUT_NONE
#define SN_USER_LOGIC_INTERRUPT_MASK_ SC_USER_LOGIC_INTERRUPT_MASK_
#define SN_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS SC_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS
#define SN_USER_LOGIC_INTERRUPT_MASK_NONE SC_USER_LOGIC_INTERRUPT_MASK_NONE

#define SN_AclDisable SC_AclDisable
#define SN_AclEnable SC_AclEnable
#define SN_AclGet SC_AclGet
#define SN_AclId SC_AclId
#define SN_AclModify SC_AclModify
#define SN_AclReset SC_AclReset
#define SN_ActivateDemoDesign SC_ActivateDemoDesign
#define SN_AllocateMonitorChannel SC_AllocateMonitorChannel
#define SN_AllocateTcpChannel SC_AllocateTcpChannel
#define SN_AllocateUdpChannel SC_AllocateUdpChannel
#define SN_AllocateUserLogicChannel SC_AllocateUserLogicChannel
#define SN_Boolean SC_Boolean
#define SN_Bucket SC_Bucket
#define SN_CancelListen SC_CancelListen
#define SN_CardInfo SC_CardInfo
#define SN_ChannelContext SC_ChannelContext
#define SN_ChannelId SC_ChannelId
#define SN_ChannelOptions SC_ChannelOptions
#define SN_CloseDevice SC_CloseDevice
#define SN_CompilationOptions SC_CompilationOptions
#define SN_Connect SC_Connect
#define SN_ConnectOptions SC_ConnectOptions
#define SN_DeallocateChannel SC_DeallocateChannel
#define SN_DEMO SC_DEMO
#define SN_DemoDesign SC_DemoDesign
#define SN_DeviceContext SC_DeviceContext
#define SN_DeviceId SC_DeviceId
#define SN_DeviceOptions SC_DeviceOptions
#define SN_Disconnect SC_Disconnect
#define SN_DumpReceiveInfo SC_DumpReceiveInfo
#define SN_EnableUserLogicInterrupts SC_EnableUserLogicInterrupts
#define SN_ERR SC_ERR
#define SN_Error SC_Error
#define SN_ErrorHandler SC_ErrorHandler
#define SN_Exception SC_Exception
#define SN_ExceptionFactory SC_ExceptionFactory
#define SN_FillingLevels SC_FillingLevels
#define SN_Flush SC_Flush
#define SN_FLUSH SC_FLUSH
#define SN_FlushOptions SC_FlushOptions
#define SN_FpgaInfo SC_FpgaInfo
#define SN_GetBucket SC_GetBucket
#define SN_GetBucketPayload SC_GetBucketPayload
#define SN_GetCardInfo SC_GetCardInfo
#define SN_GetChannelNumber SC_GetChannelNumber
#define SN_GetChannelOptions SC_GetChannelOptions
#define SN_GetConnectOptions SC_GetConnectOptions
#define SN_GetDeviceId SC_GetDeviceId
#define SN_GetDeviceOptions SC_GetDeviceOptions
#define SN_GetDmaChannelNumber SC_GetDmaChannelNumber
#define SN_GetDriverLogMask SC_GetDriverLogMask
#define SN_GetFileDescriptor SC_GetFileDescriptor
#define SN_GetFpgaRegistersBase SC_GetFpgaRegistersBase
#define SN_GetFpgaTemperature SC_GetFpgaTemperature
#define SN_GetIgmpOptions SC_GetIgmpOptions
#define SN_GetLane SC_GetNetworkInterface
#define SN_GetLastErrorCode SC_GetLastErrorCode
#define SN_GetLastTcpAckNumber SC_GetLastTcpAckNumber
#define SN_GetLibraryAndDriverVersion SC_GetLibraryAndDriverVersion
#define SN_GetLibraryLogMask SC_GetLibraryLogMask
#define SN_GetLibraryOptions SC_GetLibraryOptions
#define SN_GetlinkStatus SC_GetlinkStatus
#define SN_GetLinkStatus SC_GetLinkStatus
#define SN_GetListenOptions SC_GetListenOptions
#define SN_GetLocalAddress SC_GetLocalAddress
#define SN_GetMaxBucketPayloadSize SC_GetMaxBucketPayloadSize
#define SN_GetMaxNumberOfBuckets SC_GetMaxNumberOfBuckets
#define SN_GetNextPacket SC_GetNextPacket
#define SN_GetNumberOfChannels SC_GetNumberOfChannels
#define SN_GetPacketPayload SC_GetPacketPayload
#define SN_GetPacketPayloadLength SC_GetPacketPayloadLength
#define SN_GetPacketRingBufferSize SC_GetPacketRingBufferSize
#define SN_GetPacketTimestamp SC_GetPacketTimestamp
#define SN_GetPointerListDMA SC_GetPointerListDMA
#define SN_GetRawNextPacket SC_GetRawNextPacket
#define SN_GetRawPayload SC_GetRawPayload
#define SN_GetRawPayloadLength SC_GetRawPayloadLength
#define SN_GetSendBucketDefaultOptions SC_GetSendBucketDefaultOptions
#define SN_GetStatistics SC_GetStatistics
#define SN_GetStatusInfo SC_GetStatusInfo
#define SN_GetTcpPayload SC_GetTcpPayload
#define SN_GetTcpPayloadLength SC_GetTcpPayloadLength
#define SN_GetTcpState SC_GetTcpState
#define SN_GetTcpStateAsString SC_GetTcpStateAsString
#define SN_GetTxDmaFifoFillLevels SC_GetTxDmaFifoFillLevels
#define SN_GetUdpPayload SC_GetUdpPayload
#define SN_GetUdpPayloadLength SC_GetUdpPayloadLength
#define SN_GetUserLogicInterruptOptions SC_GetUserLogicInterruptOptions
#define SN_GetUserLogicInterruptResults SC_GetUserLogicInterruptResults
#define SN_GetUserLogicPayload SC_GetUserLogicPayload
#define SN_GetUserLogicPayloadLength SC_GetUserLogicPayloadLength
#define SN_GetUserLogicRegistersBase SC_GetUserLogicRegistersBase
#define SN_IgmpJoin SC_IgmpJoin
#define SN_IgmpLeave SC_IgmpLeave
#define SN_IgmpOptions SC_IgmpOptions
#define SN_IsMacLane SC_IsMacNetworkInterface
#define SN_IsMonitorLane SC_IsMonitorNetworkInterface
#define SN_IsOobLane SC_IsOobNetworkInterface
#define SN_IsQsfpPort SC_IsQsfpPort
#define SN_IsTcpLane SC_IsTcpNetworkInterface
#define SN_IsUdpLane SC_IsUdpNetworkInterface
#define SN_LaneStatistics SC_NetworkInterfaceStatistics
#define SN_LibraryOptions SC_LibraryOptions
#define SN_LinkStatus SC_LinkStatus
#define SN_Listen SC_Listen
#define SN_ListenOptions SC_ListenOptions
#define SN_LogMask SC_LogMask
#define SN_OpenDevice SC_OpenDevice
#define SN_packet SC_packet
#define SN_Packet SC_Packet
#define SN_PacketStatistics SC_PacketStatistics
#define SN_PrintVersions SC_PrintVersions
#define SN_ReadUserLogicRegister SC_ReadUserLogicRegister
#define SN_RegisterErrorHandler SC_RegisterErrorHandler
#define SN_ResetFPGA SC_ResetFPGA
#define SN_SEND SC_SEND
#define SN_SendBucket SC_SendBucket
#define SN_SendOptions SC_SendOptions
#define SN_SetChannelOptions SC_SetChannelOptions
#define SN_SetConnectOptions SC_SetConnectOptions
#define SN_SetDeviceOptions SC_SetDeviceOptions
#define SN_SetDriverLogMask SC_SetDriverLogMask
#define SN_SetIgmpOptions SC_SetIgmpOptions
#define SN_SetLastErrorCode SC_SetLastErrorCode
#define SN_SetLibraryLogMask SC_SetLibraryLogMask
#define SN_SetLibraryOptions SC_SetLibraryOptions
#define SN_SetListenOptions SC_SetListenOptions
#define SN_SetLocalAddress SC_SetLocalAddress
#define SN_SetPointerListDMA SC_SetPointerListDMA
#define SN_SetSendBucketDefaultOptions SC_SetSendBucketDefaultOptions
#define SN_SetTimeStampingMode SC_SetTimeStampingMode
#define SN_SetUserLogicInterruptOptions SC_SetUserLogicInterruptOptions
#define SN_SharedMemoryFlowControlOptions SC_SharedMemoryFlowControlOptions
#define SN_StatusInfo SC_StatusInfo
#define SN_TCP SC_TCP
#define SN_TcpAbort SC_TcpAbort
#define SN_TcpState SC_TcpState
#define SN_TimeControl SC_TimeControl
#define SN_Timeout SC_Timeout
#define SN_TIMEOUT SC_TIMEOUT
#define SN_UpdateReceivePtr SC_UpdateReceivePtr
#define SN_USER SC_USER
#define SN_UserLogicInterruptMask SC_UserLogicInterruptMask
#define SN_UserLogicInterruptOptions SC_UserLogicInterruptOptions
#define SN_UserLogicInterruptResults SC_UserLogicInterruptResults
#define SN_Version SC_Version
#define SN_WriteUserLogicRegister SC_WriteUserLogicRegister

#endif /* SC_LEGACY_API */

/**! @endcond Doxygen_Suppress */

//#include PRODUCT_COMMON_H commented by ekaline
#include "smartnic_common.h"

/**! @cond Doxygen_Suppress */

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__) /* Microsoft or MingW compiler */
    #define CPP11               /* C++ 11 feature */
    #define __attribute__(x)
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <sys/stat.h>
#endif

/**! @endcond Doxygen_Suppress */

/**
 *  Define our own boolean because C and C++ might not have compatible implementations especially on older compiler versions.
 *  C++ bool is usually one byte, C bool might be an int.
 */
typedef int32_t SC_Boolean;

/** **************************************************************************
 * @defgroup ConstantsMacros Constants, macros, enumerations and data structures.
 * @{
 */

/* Do not pad extra bytes between structure members. Primary reason for this is to make access from Python test programs easier. */
#pragma pack(push,1)

/**
 *  Rx DMA ring buffer filling levels.
 */
typedef struct
{
    uint16_t    StructSize;             /**< Size of this structure. */
    uint64_t    Size;                   /**< Ring buffer size in bytes. */
    uint64_t    Filling;                /**< Ring buffer fill level in bytes. */
    uint64_t    PeakFilling;            /**< Ring buffer peak fill level in bytes. */
    float       FillingPercentage;      /**< Ring buffer fill level as percentage (0-100). */
    float       PeakFillingPercentage;  /**< Ring buffer peak fill level as percentage (0-100). */

} SC_FillingLevels;

/**
 *  Link status information. Link and SFP status are periodically checked
 *  in the driver.
 */
typedef struct
{
    uint16_t        StructSize;             /**< Size of this structure. */
    SC_Boolean      Link;                   /**< FPGA has detected Ethernet link established since last check.
                                                 True when link is present, false if there is no link. */
    SC_Boolean      LostLink;               /**< FPGA has detected Ethernet link lost since last check. 
                                                  True if link has been lost, false if link has not been lost.
                                                  Stays true until cleared with @ref SC_XXX. */
    SC_Boolean      SFP_Present;            /**< FPGA has detected inserted SFP since last check.
                                                 True when SFP module is present, false if SFP module is not present. */
    SC_Boolean      SFP_Removed;            /**< FPGA has detected removed SFP since last check.
                                                 True if SFP module has been removed, false if SFP module has not been removed.
                                                 Stays true until cleared with @ref SC_XXX. */

} SC_LinkStatus;

/**
 *  Various primarily DMA status information. These definitions are currently preliminary
 *  and subject to change in future releases.
 */
typedef struct
{
    uint16_t                    StructSize;                     /**< Size of this structure. */
    uint16_t                    NumberOfLinks;                  /**< Number of link status entries available in the @ref LinkStatus array. */
    const SC_LinkStatus *       LinkStatus;                     /**< Link status per link (one link per network interface). Upon return from
                                                                     @ref SC_GetStatusInfo this points to internal memory in the device context.*/
    SC_Boolean                  PPS_Missing;                    /**< PPS missing status, true if PPS is missing, false otherwise. */
    SC_Boolean                  PPS_Present;                    /**< PPS present status, true if PPS is present, false otherwise. */
#if 0 /* Not implemented */
    SC_Boolean                  StatisticsOverflow;             /**< TBD (not currently implemented) */
    uint32_t                    PacketDmaOverflow;              /**< TBD (not currently implemented) */
#endif
    uint16_t                    NumberOfTcpChannels;            /**< Number of TOE TCP channels availabe in the @ref TcpConnectionStatus array. */
    const SC_TcpState *         TcpConnectionStatus;            /**< TCP connection status or error code of all available TOE channels.
                                                                     Upon return from @ref SC_GetStatusInfo this points to internal memory
                                                                     in the device context.*/
    uint16_t                    NumberOfRxDmaFillings;          /**< Number of DMA filling entries available in the @ref RxDmaFillings array. */
    const SC_FillingLevels *    RxDmaFillings;                  /**< Rx DMA filling level info for all DMA channels. Upon return from
                                                                     @ref SC_GetStatusInfo this points to internal memory in the device context. */

} SC_StatusInfo;

/**
 *  Rx/Tx Ethernet frame statistics for a single network interface. Currently statistics are only supported for NIC network interfaces.
 */
typedef struct
{
    uint16_t                    StructSize;                     /**< Size of this structure. */
    uint32_t                    NumberOfOkFrames;               /**< Number of valid (not corrupted) frames including the range 1-63. */
    uint32_t                    NumberOfErrorFrames;            /**< Rx: number of corrupted frames.<br>Tx: number of frames sent to the MAC with an instruction to invalidate the packet. */
    uint32_t                    NumberOfFramesSize_1_63;        /**< Frames with length less than 64 bytes. */
    uint32_t                    NumberOfFramesSize_64;          /**< Frames with length equal to 64 bytes. */
    uint32_t                    NumberOfFramesSize_65_127;      /**< Frames with length between 65 and 127 bytes. */
    uint32_t                    NumberOfFramesSize_128_255;     /**< Frames with length between 128 and 255 bytes. */
    uint32_t                    NumberOfFramesSize_256_511;     /**< Frames with length between 256 and 511 bytes. */
    uint32_t                    NumberOfFramesSize_512_1023;    /**< Frames with length between 512 and 1023 bytes. */
    uint32_t                    NumberOfFramesSize_1024_1518;   /**< Frames with length between 1024 and 1518 bytes. */
    uint32_t                    NumberOfFramesSize_Above_1518;  /**< Frames with length of more than 1518 bytes. */
    uint64_t                    NumberOfOkBytes;                /**< Number of bytes in frames longer than 63 and not corrupted. */
    uint64_t                    NumberOfErrorBytes;             /**< Number of bytes in frames less than 64 in length or in corrupted frames. */
    uint64_t                    NumberOfRxOverflowFrames;       /**< Number of frames dropped by Rx DMA due to PRB overflow or other problems in Rx path. */

} SC_NetworkInterfaceStatistics;

/**
 *  Miscellaneous information about a single FPGA register. All registers are 64-bit wide.
 */
typedef struct
{
    uint16_t                    StructSize;                 /**< Size of this structure. */
    SC_FpgaRegister             FpgaRegister;               /**< Which register. */
    uint16_t                    PCIeBAR;                    /**< The PCIe BAR (Base Address Register or region in lspci output) where the register is located. */
    uint64_t                    ByteOffsetInPCIeBAR;        /**< Byte offset from the start of PCIe BAR. */
    uint64_t                    PhysicalAddress;            /**< Physical address of the register. */
    volatile uint64_t *         pKernelVirtualAddress;      /**< Kernel virtual address of the register. */
    volatile uint64_t *         pUserSpaceVirtualAddress;   /**< User space virtual address of the register. */
    SC_Boolean                  IsReadable;                 /**< True if the register can be read, false otherwise. */
    SC_Boolean                  IsWritable;                 /**< True if the register can be written to, false otherwise. */

} SC_FpgaRegisterInfo;


/**
 *  Statistics for all network interfaces.
 */
typedef struct
{
    uint16_t                                    StructSize;                         /**< Size of this structure. */
    struct
    {
        uint32_t                                Seconds;                            /**< Timestamp whole seconds part. */
        uint32_t                                Nanoseconds;                        /**< Timestamp nanoseconds part. */
    }                                           Timestamp;                          /**< Packet timestamp. */
#ifdef SC_LEGACY_API
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t                                    NumberOfNetworkInterfaces;          /**< Number of network interfaces available in @ref RxStatisticsOfNetworkInterfaces
                                                                                         and @ref TxStatisticsOfNetworkInterfaces arrays.
                                                                                         The value is filled by @ref SC_GetStatistics. */
#ifdef SC_LEGACY_API
        uint16_t                                NumberOfLanes;                      /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    const SC_NetworkInterfaceStatistics *       RxStatisticsOfNetworkInterfaces;    /**< Receive statistics for all network interfaces. Upon return from
                                                                                         @ref SC_GetStatistics this points to internal memory in the device context. */
#ifdef SC_LEGACY_API
        const SC_NetworkInterfaceStatistics *   RxStatisticsOfLanes;                /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    const SC_NetworkInterfaceStatistics *       TxStatisticsOfNetworkInterfaces;    /**< Transmit statistics for all network interfaces. Upon return from
                                                                                         @ref SC_GetStatistics this points to internal memory in the device context. */
#ifdef SC_LEGACY_API
        const SC_NetworkInterfaceStatistics *   TxStatisticsOfLanes;                /* Deprecated legacy name */
    };
#endif /* SC_LEGACY_API */

} SC_PacketStatistics;

/**
 *  FPGA version and revision information.
 */
typedef struct
{
    uint16_t    StructSize;             /**< Size of this structure. */
    uint16_t    Major;                  /**< Major revision number. */
    uint16_t    Minor;                  /**< Minor revision number. */
    uint16_t    Revision;               /**< Sub revision number. */
    uint32_t    Build;                  /**< Build number. */
    uint16_t    ImageType;              /**< FPGA image type. */
    uint16_t    Model;                  /**< FPGA model. */
    SC_Boolean  MultiplePlDmaGroups;    /**< True if FPGA supports multiple Pointer List DMA groups, false if FPGA supports only a single PL DMA. */
    SC_Boolean  MMU;                    /**< True if FPGA has a MMU (Memroy Management Unit) present, false otherwise.
                                             MMU is used to implement Rx Packet Ring Buffers (PRBs) bigger than 4 MB. */
    SC_Boolean  MMU_IsEnabled;          /**< True if MMU (Memory Manegement Unit) is enabled (in use), false otherwise. */

} SC_FpgaInfo;

/**
 *  Static information about a single network interface.
 */
typedef struct
{
    uint16_t        StructSize;                         /**< Size of this structure. */
    const char *    InterfaceName;                      /**< Name of network interface. */
    uint8_t         InterfaceNumber;                    /**< Interface number inside the card numbered from zero per card. For example
                                                             in a card with 8 network interfaces the interfaces are numbered 0-7. */
    uint8_t         MacAddress[SC_MAC_ADDRESS_LENGTH];  /**< Network interface MAC address. */
    uint32_t        Speed;                              /**< Network interface speed in megabits per second (Mbps). */

} SC_NetworkInterfaceInfo;

/**
 *  Standard network interface (NIC) specific information.
 */
typedef struct
{
    uint16_t        StructSize;                         /**< Size of this structure. */
    uint16_t        InterfaceNumber;                    /**< Interface number inside the card numbered from zero per card. For example
                                                             in a card with 8 network interfaces the interfaces are numbered 0-7. */
    uint32_t        InterfaceFlags;                     /**< Network interface SIOCGIFFLAGS flags. See for example bash command "man netdevice". */
    uint32_t        IpAddress;                          /**< IPV4 address in host byte order. */
    const char *    IpAddressString;                    /**< Network interface IP address as string. */
    uint32_t        IpNetMask;                          /**< IPV4 net mask in host byte order. */
    const char *    IpNetMaskString;                    /**< Network interface IP mask net as string. */
    uint32_t        IpBroadcastAddress;                 /**< IPV4 broadcast address in host byte order. */
    const char *    IpBroadcastAddressString;           /**< Network interface IP broadcast address as string. */

} SC_NicInfo;

/**
 *  Parameters of a continuous memory block which might be mapped in a FPGA Memory Management Unit (MMU).
 */
typedef struct
{
    uint16_t        StructSize;                     /**< Size of this structure. */
    uint64_t        Length;                         /**< Length of memory. */
    uint64_t        KernelVirtualAddress;           /**< Kernel virtual address of the memory. */
    uint64_t        UserSpaceVirtualAddress;        /**< User space virtual address of the memory. */
    uint64_t        PhysicalAddress;                /**< Physical address of the memory. */
    uint64_t        FpgaDmaStartAddress;            /**< FPGA DMA start address. This is a Memory Management Unit (MMU)
                                                         virtual address when MMU is present in the FPGA and is enabled
                                                         or a physical start address if MMY is not present or not enabled. */

} SC_Memory;

/**
 *  Parameters of memory that is mapped from kernel space to user space as continuous memory
 *  but is in kernel space constructed from a number of smaller memory blocks.
 */
typedef struct
{
    uint16_t            StructSize;                 /**< Size of this structure. */
    uint64_t            Length;                     /**< Total length of mapped memory in user space. */
    uint64_t            UserSpaceVirtualAddress;    /**< User space virtual address of the the memory. */
    uint16_t            NumberOfMemoryBlocks;       /**< Number of memory blocks that this user space memory is constructed of. */
    const SC_Memory *   MemoryBlocks;               /**< Pointer to an array of memory blocks. The number of elements
                                                         in the array is given by @ref SC_MappedMemory::NumberOfMemoryBlocks.
                                                         These memory blocks are allocated and mapped in the driver to form
                                                         the continuous user space memory starting with
                                                         @ref SC_MappedMemory::UserSpaceVirtualAddress and of length
                                                         @ref SC_MappedMemory::Length. */
} SC_MappedMemory;

/**
 *  Static information about a single DMA channel.
 */
typedef struct
{
    uint16_t            StructSize;                                 /**< Size of this structure. */
    uint8_t             DmaChannelNumber;                           /**< DMA channel number of this channel. */
    SC_ChannelType      DmaChannelType;                             /**< DMA channel type, i.e. if it is a NIC channel, user logic channel, etc. */
    uint16_t            MaximumSizeOfFpgaTxPacket;                  /**< Maximum size of Rx packet in the FPGA. */
    uint16_t            MaximumSizeOfTxBucketPayload;               /**< Maximum size of a payload in API Tx bucket. */
    uint16_t            MaximumNumberOfTxBuckets;                   /**< Maximum number of Tx buckets available on this channel. */
    SC_MappedMemory     PacketRingBuffer;                           /**< The Packet Ring Buffer as continuous user space memory. */

} SC_DmaChannelInfo;

/**
 *  Information that is common to all DMA channels.
 */
typedef struct
{
    uint16_t        StructSize;                                 /**< Size of this structure. */
    uint16_t        MaximumNumberOfBucketsInTxList;             /**< Maximum number of Tx buckets in a Tx list. */
    uint16_t        RecommendedMaximumNumberOfBytesInTxList;    /**< Recommended maximum number of total bytes in all Tx buckets in a Tx list.
                                                                     This is a soft limit. Theoretically a single packet list
                                                                     can contain @ref SC_DmaChannelCommonInfo::MaximumNumberOfBucketsInTxList *
                                                                     SC_DmaChannelInfo::MaximumSizeOfTxBucketPayload bytes but it is
                                                                     recommended not to exceed this limit to avoid PCIe bus congestion
                                                                     with other DMA channels. While a Tx list is being transmitted all
                                                                     other DMA channels are blocked. */
} SC_DmaChannelCommonInfo;

/**
 *  Miscellaneous static card information like network interface configuration masks, FPGA version number, board serial number, etc.
 */
typedef struct
{
    uint16_t        StructSize;                             /**< Size of this structure. */
    uint32_t        BoardSerialNumber;                      /**< Board serial number. */
    const char *    BoardSerialNumberString;                /**< Board serial number as a string (points to internal storage in device context). */
#ifdef SC_LEGACY_API
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        OobNetworkInterfacesEnabledMask;        /**< OOB (Standard NIC) network interfaces mask. A bit of one indicates OOB present. */
#ifdef SC_LEGACY_API
        union
        {
            uint16_t        OobNetworkInterfaceEnabledMask; /* Deprecated legacy name */
            uint16_t        OobLanesEnabledMask;            /* Deprecated legacy name */
        };
    };
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        TcpNetworkInterfacesEnabledMask;        /**< ETLM TCP network interfaces mask. A bit of one indicates ETLM TCP present. */
#ifdef SC_LEGACY_API
        uint16_t        TcpLanesEnabledMask;                /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        UdpNetworkInterfacesEnabledMask;        /**< UDP (Standard NIC) network interfaces mask. A bit of one indicates UDP present. */
#ifdef SC_LEGACY_API
        uint16_t        UdpLanesEnabledMask;                /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        MacNetworkInterfacesEnabledMask;        /**< MAC network interfaces mask. A bit of one indicates MAC present. */
#ifdef SC_LEGACY_API
        uint16_t        MacLanesEnabledMask;                /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        AclNetworkInterfacesEnabledMask;        /**< ACL network interfaces enabled mask. A bit of one indicates ACL present. */
#ifdef SC_LEGACY_API
        uint16_t        AclLanesEnabledMask;                /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        MonitorNetworkInterfacesEnabledMask;    /**< Monitor network interfaces enabled mask. A bit of one indicates monitor present. */
#ifdef SC_LEGACY_API
        uint16_t        MonitorLanesEnabledMask;            /* Deprecated legacy name */
    };
    union
    {
#endif /* SC_LEGACY_API */
    uint16_t        NumberOfNetworkInterfaces;              /**< Number of network interfaces in this card. */
#ifdef SC_LEGACY_API
        uint16_t        NumberOfNetworkLanes;               /* Deprecated legacy name */
    };
#endif /* SC_LEGACY_API */

    uint8_t         QsfpPortsEnabledMask;                   /**< QSFP ports enabled mask. A bit of one indicates QSFP present. */
    uint16_t        EvaluationEnabledMask;                  /**< Evaluation mode bits 24-28. */
    uint16_t        NumberOfNICs;                           /**< Number of network interfaces in this card
                                                                 that are configured and available in Linux as standard network interfaces (NIC). */
    uint16_t        NumberOfDmaChannels;                    /**< Number of DMA channels in this card. */
    uint16_t        NumberOfTcpChannels;                    /**< Number of available TOE TCP channels. */
    SC_Boolean      IsToeConnectedToUserLogic;              /**< True if TOE is connected to user logic, false otherwise. */
    SC_Boolean      StatisticsEnabled;                      /**< True if FPGA includes the statistics module, false otherwise. */
    const char *    PciDeviceName;                          /**< PCIe device name. This points to internal memory in the device context.*/
    uint16_t        MaximumTxDmaFifoEntries;                /**< Maximum number of bucket entries in Tx DMA FIFO. */
    uint16_t        MaximumTxDmaFifoEntriesUsedByDriver;    /**< Maximum number of bucket entries in Tx DMA FIFO used by the driver for NIC Tx. */
    const char *    DriverName;                             /**< Name of the Linux driver handling this card. */

    const SC_NetworkInterfaceInfo *   NetworkInterfaces;    /**< Information about network interfaces in this card. Length of table
                                                                 is determined by SC_CardInfo::NumberOfNetworkInterfaces. */
    const SC_NicInfo *                NICs;                 /**< Information about standard network interfaces (NICs) in this card.
                                                                 Length of table is determined by @ref SC_CardInfo::NumberOfNICs. */
    SC_DmaChannelCommonInfo           DmaChannelCommonInfo; /**< Information that is common to all DMA channels. */
    const SC_DmaChannelInfo *         DmaChannels;          /**< Information about DMA channels supported by this card. Length of table
                                                                 is determined by SC_CardInfo::NumberOfDmaChannels. */
    SC_FpgaInfo     FpgaInfo;                               /**< FPGA related information. */

} SC_CardInfo;

/**
 *  Information about a single Linux device.
 */
typedef struct
{
    uint16_t        StructSize;                             /**< Size of this structure. */
    uint8_t         MajorDeviceNumber;                      /**< Major device number of the device. */
    uint8_t         MinorDeviceNumber;                      /**< Minor device number of the device. */
    SC_Boolean      Available;                              /**< Device is available via the driver, i.e. this means that the device can be opened via a file handle.
                                                                 If this member is false then only the device file exists but no actual device. */
    const char *    FullDeviceName;                         /**< Full path Linux device name as /dev/DEVICENAME. Points to internal static storage in the library. */

} SC_DeviceInfo;

/**
 *  System information about all devices.
 */
typedef struct
{
    uint16_t        StructSize;                             /**< Size of this structure. */
    uint16_t        NumberOfDevices;                        /**< Total number of device files found in the /dev/ directory. */
    SC_DeviceInfo * Devices;                                /**< Pointer to an internal array in the library that contains device information about each device.
                                                                 @ref SC_SystemInfo::NumberOfDevices number of entries in the table. */

} SC_SystemInfo;

/**
 *  Options for ETLM TCP channels shared 16 KB buffer Tx flow control.
 *
 *  @brief You have to use shared memory flow control if you run multiple processes accessing ETLM TPC channels or
 *  if you create multiple device contexts in the same process for the same SmartNIC device.
 *  <br><br>
 *  Per default shared memory flow control is disabled, i.e. you should only create one device context
 *  in a single user process but multithreaded access to ETLM TCP channels will work.
 *  <br><br>
 *  If you don't use ETLM TCP channels then you don't need shared memory flow control and multithreaded/multiprocess
 *  access is safe with one thread for Tx and another thread for Rx per channel.
 *  <br><br>
 *  NB: Shared memory flow control is not currently supported.
 */
typedef struct
{
    uint16_t        StructSize;         /**< Size of this structure. */
    SC_Boolean      Enabled;            /**< Set to true if multiple processes are using ETLM TCP channels. Default is false.
                                             NB: current release does not support multiple process flow control of ETLM shared 16 KB buffer.
                                             Multiple process flow control support might be added in a future release if there is need for it. */
    const char *    FileName;           /**< Name of the shared memory file in directory /dev/shm/. Default is "SmartNicAPI". */
    mode_t          Permissions;        /**< Usual shared memory file Linux/Unix permission flags. Default value is 0666. */

} SC_SharedMemoryFlowControlOptions; 

/**
 *  Global options specific to whole API library and all device instances.
 */
typedef struct
{
    uint16_t            StructSize;                 /**< Size of this structure. */
    SC_Boolean          ThrowExceptions;            /**< For C++ clients; throw @ref SC_Exception exceptions instead of returning error codes. Default is false.
                                                         This is currently an experimental feature. */
    SC_Boolean          WriteErrorsToStdErr;        /**< True if library errors should always be written to stderr or false otherwise. Default value is false. */
    const char *        LogFileName;                /**< Name of a log file. If a file name is provided then all logging from the API library is written into the file.
                                                         Default file name is NULL, i.e. all logging is written either to stdout or stderr. */
    uint32_t            FpgaResetWaitInSeconds;     /**< How many seconds to wait after FPGA reset for everything start running normal again.
                                                         For all other tested systems 200 milliseconds delay in the driver is enough
                                                         but HP Proliant g9 servers DL360/DL380 running CentOS 7.4 seem to require at least 3 seconds
                                                         which is the default value now. If there are problems with FPGA reset then try
                                                         longer values. */
    SC_Boolean          EnforceBucketsSanity;       /**< This option affects only DEBUG builds of the library enforcing strict buckets sanity checks.
                                                         Some example applications (for example ul_dma_perf.c) violate the bucket life cycle described
                                                         in @ref SC_GetBucket causing debug assertions in the library. These asserts can be disabled
                                                         with this option. Release builds of the library do not do any sanity checks for performance reasons.
                                                         Default value is true.<br><br>
                                                         NB: Never use this option in production software and never violate the bucket life cycle. Doing
                                                         so results in undefined behaviour. This option is only for use by Silicom internal test applications. */
    SC_SharedMemoryFlowControlOptions   SharedMemoryFlowControlOptions;  /**< Options for ETLM TCP channels shared 16 KB buffer Tx flow control shared between user processes. */

} SC_LibraryOptions;

/**
 *  Some static information how the API library was compiled.
 */
typedef struct
{
    SC_Boolean      IsLoggingSupported;             /**< True if API library was compiled with logging support, false otherwise. */
    SC_Boolean      IsStatisticsSupported;          /**< True if API library was compiled with statistics support, false otherwise. */
    SC_Boolean      IsEtlmTxSpinlockUsed;           /**< True if API library was compiled with ETLM Tx flow control spin lock, false if compiled with a mutex lock. */

} SC_CompilationOptions;

/**
 *  Access to API library compilation options.
 */
extern const SC_CompilationOptions g_ApiLibraryCompilationOptions;

/**
 *  Optional device options to use with @ref SC_OpenDevice call.
 */
typedef struct
{
    uint16_t        StructSize;                             /**< Size of this structure. */
    /**
     *  @brief  Reserve some Tx DMA normal FIFO capacity for concurrent threads or processes.
     *
     *  Reserve some Tx DMA FIFO capacity for concurrent threads
     *  or processes that make a firmware fill level check at the same time
     *  that might lead to overbooking the queue capacity. This happens
     *  if 2 or more threads or processes read the fill level before
     *  each writes a new packet descriptor into the FIFO.
     *  The value of this parameter should be set
     *  to equal or greater than the maximum number of concurrent
     *  FIFO writes possible.
     *  <br><br>
     *  Of course, it is probably difficult to estimate a correct
     *  value for this but at least this provides some protection
     *  against several threads thinking that there is place
     *  in the FIFO but only one request succeeds and the others fail
     *  because the FIFO is full. Failing requests will result in lost packets.
     *  <br><br>
     *  In @ref SC_OpenDevice the value of this field is checked against
     *  the FIFO size of 512 entries
     *  including the number of NICs detected in the driver multiplied by
     *  the value of FB_FIFO_ENTRIES_PER_CHANNEL in ctls.h header file.
     *  This takes into account that standard NICs in the driver use
     *  the normal Tx FIFO too. Also the flow control check in the library
     *  takes into account the FIFO entries reserved for use by NICs
     *  in the driver.
     */
    uint16_t        MaxTxDmaNormalFifoConcurrency;
    uint16_t        MaxTxDmaPriorityFifoConcurrency;    /**< Same as @ref MaxTxDmaNormalFifoConcurrency above but for the Tx DMA priority FIFO. */
    uint32_t        ArpEntryTimeout;                    /**< Driver ARP table entry timeout in millisecons. Value of zero disables ARP timeouts,
                                                             i.e. entries in ARP table never time out and new entries overwrite the least recently
                                                             used entry. Default value is zero. */

} SC_DeviceOptions;

/**
 *  Optional channel options to use with the various allocate channel calls. The sum of
 *  software fill limits for all channels must not exceed the FIFO size of 512 entries.
 */
typedef struct
{
    uint16_t        StructSize;                                 /**< Size of this structure. */
    SC_Boolean      UseTxDmaNormalFifoFirmwareFillLevel;        /**< Check firmware fill level count when writing to the Tx DMA normal FIFO.
                                                                     This option is disabled (false) by default. */
    uint16_t        TxDmaNormalFifoSoftwareFillLevelLimit;      /**< Channel specific software fill level limit when writing to the Tx DMA normal FIFO.
                                                                     Default value is 4 which is a conservative value assuming that all channels are in use.
                                                                     An application can have up to this value of pending Tx DMA requests in the normal FIFO.
                                                                     A value of zero disables this flow control option. */
    SC_Boolean      UseTxDmaPriorityFifoFirmwareFillLevel;      /**< Check firmware fill level count when writing to the Tx DMA priority FIFO.
                                                                     This option is disabled (false) by default. */
    uint16_t        TxDmaPriorityFifoSoftwareFillLevelLimit;    /**< Channel specific software fill level limit when writing to the Tx DMA priority FIFO.
                                                                     Default value is 4 which is a conservative value assuming that all channels are in use.
                                                                     An application can have up to this value of pending Tx DMA requests in the priority FIFO.
                                                                     A value of zero disables this flow control option. */
    SC_Boolean      DoNotUseRxDMA;                              /**< Do not use Rx DMA functionality.
                                                                     This is currently supported only on UDP DMA channels
                                                                     which can be directed to user logic instead of to the host.
                                                                     Default value is false, i.e. Rx DMA is enabled. */
    SC_Boolean              UserSpaceRx;                        /**< True if this channel is doing user space Rx, false otherwise. Default is true. */
    SC_Boolean              UserSpaceTx;                        /**< True if this channel is doing user space Tx, false otherwise. Default is true. */

} SC_ChannelOptions;

/**
 *  TCP connection options used by @ref SC_Connect.
 */
typedef struct
{
    uint16_t        StructSize;                                 /**< Size of this structure. */
    const char *    LocalIpAddress;                             /**< Optional local IP address per connection. Default is NULL and the IP address of the configured network interface or VLAN is used. */
    uint16_t        LocalPort;                                  /**< Optional local IP port to use. Use 0 for automatic selection of local IP port. This is the default. */
    int16_t         VLanTag;                                    /**< VLAN tag. Valid values are from 1 to 4094 or default -1 when VLAN is not used. */
    uint8_t         RemoteMacAddress[MAC_ADDR_LEN];             /**< Remote MAC address to use. Default is NULL and remote MAC is resolved with ARP. */
    uint16_t        ConnectTimeout;                             /**< Connect timeout in milliseconds to wait for connection to establish. Default timeout is 30 seconds. */
                                                                /**< Zero timeout will return immediately without waiting connection to establish and */
                                                                /**< the user is self responsible to poll the TCP state until it is SC_TCP_ESTABLISHED by calling */
                                                                /**< API function @ref SC_GetTcpState. */
    uint16_t        DisconnectTimeout;                          /**< Disconnect timeout in milliseconds to wait for connection to close . Default timeout is 30 seconds. */
                                                                /**< Zero timeout will return immediately without waiting connection to close and */
                                                                /**< the user is self responsible to poll the TCP state until it is SC_TCP_CLOSED by calling */
                                                                /**< API function @ref SC_GetTcpState or aborting the ETLM with @ref SC_TcpAbort. */
    uint32_t        AcquireMacAddressViaArpTimeout;             /**< Timeout in seconds to acquire remote host MAC address via ARP in milliseconds. */
    SC_Boolean      ScanLinuxArpTable;                          /**< True if before sending any ARP request the Linux ARP table should be consulted, false otherwise.
                                                                     Default value is true. */

} SC_ConnectOptions;

/**
 *  TCP listen connection options used by @ref SC_Listen.
 */
typedef struct
{
    uint16_t        StructSize;                                 /**< Size of this structure. */
    const char *    LocalIpAddress;                             /**< Optional local IP address to listen. Default is NULL and the IP address of the configured network interface or VLAN is used. */
    int32_t         ListenTimeout;                              /**< Timeout in seconds or default -1 to wait indefinitely. */
    int16_t         VLanTag;                                    /**< VLAN tag. Valid values are from 1 to 4094 or default -1 when VLAN is not used. */
    const char *    RemoteIpAddress;                            /**< Remote IP address after successful remote connect, empty otherwise. */
    uint16_t        RemoteIpPort;                               /**< Remote IP port after successful remote connect, zero otherwise. */
    uint8_t         RemoteMacAddress[MAC_ADDR_LEN];             /**< Remote MAC address after successful remote connect, zero otherwise. */

} SC_ListenOptions;

/**
 *  UDP multicast join settings and options.
 */
typedef struct
{
    uint16_t        StructSize;                                 /**< Size of this structure. */
    int16_t         VLanTag;                                    /**< VLAN tag. Valid values are from 1 to 4094 and default -1 when VLAN is not used. */
                                                                /**< VLAN tag is reserved for future expansion. Use -1 or pass NULL as pointer to @ref SC_IgmpOptions. */
    SC_Boolean      EnableMulticastBypass;                      /**< Multicast bypass allows unsubscribed multicast traffic to be forwarded to standard NIC.
                                                                     Per default bypass is disabled. */

} SC_IgmpOptions;

/**
 *  User logic interrupt masks.
 */
typedef enum
{
    SC_USER_LOGIC_INTERRUPT_MASK_NONE           = 0x00,         /**< (0x00) Mask for disabling all user logic interrutps. */
    SC_USER_LOGIC_INTERRUPT_MASK_0              = 0x01,         /**< (0x01) Mask for enabling user logic interrupts on user logic channel 0. */
    SC_USER_LOGIC_INTERRUPT_MASK_1              = 0x02,         /**< (0x02) Mask for enabling user logic interrupts on user logic channel 1. */
    SC_USER_LOGIC_INTERRUPT_MASK_2              = 0x04,         /**< (0x04) Mask for enabling user logic interrupts on user logic channel 2. */
    SC_USER_LOGIC_INTERRUPT_MASK_3              = 0x08,         /**< (0x08) Mask for enabling user logic interrupts on user logic channel 3. */
    SC_USER_LOGIC_INTERRUPT_MASK_4              = 0x10,         /**< (0x10) Mask for enabling user logic interrupts on user logic channel 4. */
    SC_USER_LOGIC_INTERRUPT_MASK_5              = 0x20,         /**< (0x20) Mask for enabling user logic interrupts on user logic channel 5. */
    SC_USER_LOGIC_INTERRUPT_MASK_6              = 0x40,         /**< (0x40) Mask for enabling user logic interrupts on user logic channel 6. */
    SC_USER_LOGIC_INTERRUPT_MASK_7              = 0x80,         /**< (0x80) Mask for enabling user logic interrupts on user logic channel 7. */
    SC_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS   = 0xFF          /**< (0xFF) Enable user logic interrupts on all user logic channels 0-7. */

} SC_UserLogicInterruptMask;

/**
 *  User logic interrupt options.
 */
typedef struct
{
    uint16_t                        StructSize;     /**< Size of this structure. */
    pid_t                           Pid;            /**< Process id to which the interrupt signals are delivered. Default value zero
                                                         means the same process that calls @ref SC_EnableUserLogicInterrupts. */
    int                             SignalNumber;   /**< Signal number to use when user logic interrupt signals user space application. Default signal is SIGUSR1. */
    SC_UserLogicInterruptMask       Mask;           /**< Combined mask for user logic interrupts. Default mask is @ref SC_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS. */
    void *                          pContext;       /**< Optional user defined context which is passed to the user space application signal handler. Default is NULL. */

} SC_UserLogicInterruptOptions;

/**
 *  User logic interrupt results.
 */
typedef struct
{
    uint16_t            StructSize;             /**< Size of this structure. */
    uint8_t             DeviceMinorNumber;      /**< Device minor number (0-15) of the SmMartNIC device that the interrupt originated from. */
    uint8_t             UserLogicChannel;       /**< User logic channel (0-7) of the channel that the interrupt originated from. */
    void *              pContext;               /**< Optional user defined context that was passed to @ref SC_EnableUserLogicInterrupts. */
    uint64_t            TimestampSeconds;       /**< Timestamp seconds when interrupt was signalled. */
    uint64_t            TimestampNanoSeconds;   /**< Timestamp nanoseconds when interrupt was signalled. */

} SC_UserLogicInterruptResults;

#pragma pack(pop)

#ifdef __GNUC__
    /*
    #pragma GCC diagnostic push
    For enum SC_LogMask ignore warning: ISO C restricts enumerator values to range of 'int' [-Wpedantic]
    #pragma GCC diagnostic ignored "-Wpedantic"
    */
#endif

/**
 *  API library logging masks. Used by log mask accessor functions @ref SC_GetLibraryLogMask and @ref SC_SetLibraryLogMask.
 */
typedef enum
    #ifdef CPP11    /* C++ 11 or later */
        : uint64_t
    #endif
{
    LOG_NONE                                        = SC_MASK(0x0000000000000000),  /**< (0x0000000000000000) No logging enabled. */
    LOG_SETUP                                       = SC_MASK(0x0000000000000001),  /**< (0x0000000000000001) Log initial setup like
                                                                                          memory mapping and other initializations. */
    LOG_TX                                          = SC_MASK(0x0000000000000002),  /**< (0x0000000000000002) Log Tx related activity. */
    LOG_DMA                                         = SC_MASK(0x0000000000000004),  /**< (0x0000000000000004) Log DMA related activity. */
    LOG_REG                                         = SC_MASK(0x0000000000000008),  /**< (0x0000000000000008) Log register operationss */
    LOG_READ                                        = SC_MASK(0x0000000000000010),  /**< (0x0000000000000010) Log read operations */
    LOG_WRITE                                       = SC_MASK(0x0000000000000020),  /**< (0x0000000000000020) Log write operations */
    LOG_FLOW_CONTROL                                = SC_MASK(0x0000000000000040),  /**< (0x0000000000000040) Log activity related to flow control */
    LOG_TX_PIPE_FULL                                = SC_MASK(0x0000000000000080),  /**< (0x0000000000000080) Log Tx pipeline full conditions */
    LOG_COUNTER_SWAP_AROUND                         = SC_MASK(0x0000000000000100),  /**< (0x0000000000000100) Log consumed bytes counter swap arounds */
    LOG_BUCKET                                      = SC_MASK(0x0000000000000200),  /**< (0x0000000000000200) Log bucket related activity */
    LOG_TCP                                         = SC_MASK(0x0000000000000400),  /**< (0x0000000000000400) Log TCP related activity */
    LOG_UDP                                         = SC_MASK(0x0000000000000800),  /**< (0x0000000000000800) Log UDP related activity */
    LOG_ARP                                         = SC_MASK(0x0000000000001000),  /**< (0x0000000000001000) Log ARP related activity */
    LOG_SHARED_MEMORY                               = SC_MASK(0x0000000002000000),  /**< (0x0000000002000000) Log any activity related to shared memory access. */
    LOG_LOCKING                                     = SC_MASK(0x0000000004000000),  /**< (0x0000000004000000) Log activity related to locking */
    LOG_DETAIL                                      = SC_MASK(0x0000000008000000),  /**< (0x0000000008000000) Log more detail */
    LOG_UNKNOWN                                     = SC_MASK(0x0000000010000000),  /**< (0x0000000010000000) Log unknowns. */
    LOG_WARNING                                     = SC_MASK(0x0000000020000000),  /**< (0x0000000020000000) Log warnings. */
    LOG_ERROR                                       = SC_MASK(0x0000000040000000),  /**< (0x0000000040000000) Log errors. */

    /* Special log flag to dynamically choose LOGANY or LOGALL alternatives */
    LOG_IF_ALL_BITS_MATCH                           = SC_MASK(0x0000000080000000),  /**< (0x0000000080000000) Log if all bits match. */

    /* Combined flags for convenience */

    LOG_ALL                                         = SC_MASK(0x000000007FFFFFFF),  /**< (0x000000007FFFFFFF) Log everything! */

    LOG_ALWAYS                                      = LOG_ERROR | LOG_WARNING | LOG_UNKNOWN,   /**< Log always any of these. */

    LOG_REG_READ                                    = LOG_REG | LOG_READ,                       /**< Log register reads. */
    LOG_REG_WRITE                                   = LOG_REG | LOG_WRITE,                      /**< Log register writes. */
    LOG_TX_DMA                                      = LOG_TX | LOG_DMA,                         /**< Log Tx DMA related activity. */
    LOG_TX_COUNTER_SWAP_AROUND                      = LOG_TX | LOG_COUNTER_SWAP_AROUND,         /**< Log Tx flow control 32-bit counter swap around. */
    LOG_TX_FLOW_CONTROL                             = LOG_TX | LOG_FLOW_CONTROL,                /**< Log Tx flow control related activity. */
    LOG_TCP_FLOW_CONTROL                            = LOG_TCP | LOG_FLOW_CONTROL,               /**< Log TCP related flow control activity. */
    LOG_TCP_FLOW_CONTROL_LOCKING                    = LOG_TCP_FLOW_CONTROL | LOG_LOCKING        /**< Log TCP flow control locking. */

} SC_LogMask;

#ifdef __GNUC__
    /*
    #pragma GCC diagnostic pop
    */
#endif

/**
 *  Error codes returned by API calls and the @ref SC_GetLastErrorCode function.
 */
typedef enum
{
    SC_ERR_NOTHING_TO_FLUSH                             = 1,    /**< (1) Nothing to flush */

    SC_ERR_SUCCESS                                      = 0,    /**< (0) Successful completion. */
    SC_ERR_OUT_OF_MEMORY                                = -1,   /**< (-1) Memory allocation failed. */
    SC_ERR_FAILED_TO_OPEN_DEVICE                        = -2,   /**< (-2) Failed to open device. */
    SC_ERR_MMAP_OF_FPGA_REGISTERS_FAILED                = -3,   /**< (-3) Failed to mmap FPGA registers into user space. */
    SC_ERR_TX_DMA_WAIT_FAILED                           = -4,   /**< (-4) Wait on TxDMA list failed. */
    SC_ERR_CHANNEL_ALLOCATION_FAILED                    = -5,   /**< (-5) Allocation of a channel from the driver failed. */
    SC_ERR_CHANNEL_DEALLOCATION_FAILED                  = -6,   /**< (-6) Deallocation of a channel failed. */
    SC_ERR_BUCKET_PHYSICAL_ADDRESS_FAILED               = -7,   /**< (-7) Failed to get a physical bucket address from the driver. */
    SC_ERR_MMAP_OF_BUFFERS_FAILED                       = -8,   /**< (-8) Failed to mmap various buffers from the driver. */
    SC_ERR_RECEIVE_PHYSICAL_ADDRESS_FAILED              = -9,   /**< (-9) Failed to get receive physical address from the driver. */

    SC_ERR_INVALID_ARGUMENT                             = -10,  /**< (-10) Invalid function argument. */
    SC_ERR_FAILED_TO_SET_LOCAL_ADDRESS                  = -11,  /**< (-11) Failed to set local IP address, IP mask or MAC address. */
    SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS                  = -12,  /**< (-12) Failed to get local IP address, IP mask or MAC address. */
    SC_ERR_FAILED_TO_GET_LINK_STATUS                    = -13,  /**< (-13) Failed to get network interface link status. */
    SC_ERR_FAILED_TO_READ_FPGA_TEMPERATURE              = -14,  /**< (-14) Failed to read FPGA temperature. */
    SC_ERR_FAILED_TO_READ_FPGA_VERSION                  = -15,  /**< (-15) Failed to read FPGA version. */
    SC_ERR_FAILED_TO_GET_DRIVER_LOG_MASK                = -16,  /**< (-16) Failed to get driver log mask. */
    SC_ERR_FAILED_TO_SET_DRIVER_LOG_MASK                = -17,  /**< (-17) Failed to set driver log mask. */
    SC_ERR_FAILED_TO_GET_BOARD_SERIAL                   = -18,  /**< (-18) Failed to get static card information, deprecated, same as @ref SC_ERR_FAILED_TO_GET_CARD_INFO. */
    SC_ERR_FAILED_TO_GET_INFO                           = -18,  /**< (-18) Failed to get static card information, deprecated, same as @ref SC_ERR_FAILED_TO_GET_CARD_INFO. */
    SC_ERR_FAILED_TO_GET_CARD_INFO                      = -18,  /**< (-18) Failed to get static card information. */
    SC_ERR_FAILED_TO_GET_BOARD_MASKS                    = -19,  /**< (-19) Failed to get board network interface masks. */

    SC_ERR_FAILED_TO_SET_TIME_STAMPING_MODE             = -20,  /**< (-20) Failed to set time stamping mode. */
    SC_ERR_FAILED_TO_GET_FRAME_STATISTICS               = -21,  /**< (-21) Failed to get Ethernet frame statistics. */
    SC_ERR_FAILED_TO_JOIN_IGMP_GROUP                    = -22,  /**< (-22) Failed to join an IGMP group. */
    SC_ERR_FAILED_TO_LEAVE_IGMP_GROUP                   = -23,  /**< (-23) Failed to leave an IGMP group. */
    SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO                  = -24,  /**< (-24) Failed to dump receive info. */
    SC_ERR_ACL_OPERATION_FAILED                         = -25,  /**< (-25) ACL operation failed. */
    SC_ERR_FAILED_TO_GET_DRIVER_VERSION                 = -26,  /**< (-26) Failed to get device driver version. */
    SC_ERR_FAILED_TO_GET_TCP_STATUS                     = -27,  /**< (-27) Failed to get TCP status. */
    SC_ERR_FAILED_TO_READ_USER_LOGIC_REGISTER           = -28,  /**< (-28) Failed to read to user logic register. */
    SC_ERR_DMA_BUFFER_FAILED                            = -29,  /**< (-29) Failed to allocate/deallocate DMA buffer. */

    SC_ERR_FAILED_TO_UPDATE_RECEIVE_PTR                 = -30,  /**< (-30) Failed to update receive pointer. */
    SC_ERR_NO_MORE_BUCKETS_AVAILABLE                    = -31,  /**< (-31) There are no more buckets available. */
    SC_ERR_FAILED_TO_CONNECT                            = -32,  /**< (-32) Failed to connect. */
    SC_ERR_FAILED_TO_DISCONNECT                         = -33,  /**< (-33) Failed to disconnect. */
    SC_ERR_FAILED_TO_LISTEN_OR_CANCEL_LISTEN            = -34,  /**< (-34) Failed to listen or cancel listen. */
    SC_ERR_FAILED_TO_GET_LAST_TCP_ACK_NUMBER            = -35,  /**< (-35) Failed to get last TCP ack number. */
    SC_ERR_FAILED_TO_GET_NEXT_PACKET                    = -36,  /**< (-36) Failed to get next Rx packet. */
    SC_ERR_PIPE_FULL                                    = -37,  /**< (-37) Tx pipeline is full, retry the same send again at a later time
                                                                     or use @ref SC_SEND_WAIT_FOR_PIPE_FREE option in @ref SC_SendBucket. */
    SC_ERR_FAILED_TO_CLOSE_DEVICE                       = -38,  /**< (-38) Failed to close device. */
    SC_ERR_LOCKING_FAILED                               = -39,  /**< (-39) Spin lock or mutex failed to lock or unlock. */

    SC_ERR_TIMEOUT                                      = -40,  /**< (-40) Timeout occurred. */
    SC_ERR_FAILED_TO_GET_INTERFACE_NAME                 = -41,  /**< (-41) Failed to get network interface (aka logical port) interface name from the driver. */
    SC_ERR_SHARED_MEMORY_FAILED                         = -42,  /**< (-42) Shared memory failure. */
    SC_ERR_ACL_NOT_PRESENT                              = -43,  /**< (-43) ACL is not present in FPGA configuration on this network interface. */
    SC_ERR_DEMO_DESIGN_FAILED                           = -44,  /**< (-44) Failure in demo design. */
    SC_ERR_FAILED_TO_ENUMERATE_NETWORK_INTERFACES       = -45,  /**< (-45) Enumeration of existing network interfaces failed */
    SC_ERR_NOT_SUPPORTED_OR_NOT_IMPLEMENTED             = -46,  /**< (-46) Not supported or not implemented feature, option or setting. */
    SC_ERR_FAILED_TO_ABORT_ETLM_CHANNEL                 = -47,  /**< (-47) Failed to abort ETLM channel. */
    SC_ERR_FAILED_TO_GET_PCI_DEVICE_NAME                = -48,  /**< (-48) Failed to get PCIe device name. */
    SC_ERR_ARP_FAILED                                   = -49,  /**< (-49) Something went wrong in ARP processing. */

    SC_ERR_FAILED_TO_RESET_FPGA                         = -50,  /**< (-50) Failed to reset the FPGA. */
    SC_ERR_INVALID_TCP_STATE                            = -51,  /**< (-51) Invalid TCP state. */
    SC_ERR_FPGA_DOES_NOT_SUPPORT_FRAME_STATISTICS       = -52,  /**< (-52) FPGA does not support Ethernet frame statistics. */
    SC_ERR_LISTEN_CANCELLED_BY_USER                     = -53,  /**< (-53) Returned by @ref SC_Listen if cancelled by @ref SC_CancelListen. */
    SC_ERR_PARSE_ACL_EXPRESSION_FAILED                  = -54,  /**< (-54) Failed to parse an ACL expression. */
    SC_ERR_PARSE_ACL_NOT_ENOUGH_PLACE                   = -55,  /**< (-55) Not enough place in ACL return binary values array. */
    SC_ERR_FAILED_TO_GET_SYSTEM_INFO                    = -56,  /**< (-56) Failed to get system information about device files. */
    SC_ERR_FAILED_TO_CONVERT_IP_ADDRESS                 = -57,  /**< (-57) Failed to convert IP address or mask to a string. */
    SC_ERR_DMA_OPERATION_FAILED                         = -58,  /**< (-58) Rx, Tx or Pointer List DMA operation failed. */
    SC_ERR_REFERENCE_COUNT_FAILED                       = -59,  /**< (-59) Reference count increment or decrement failed. */

    SC_ERR_MMAP_OF_MEMORY_FAILED                        = -60,  /**< (-60) Failed to map kernel memory to user space. */
    SC_ERR_FAILED_TO_GET_CHANNEL_INFO                   = -61,  /**< (-61) Failed to get channel info from the driver. */

    SC_ERR_HANDLER_WRITE_TO_STDERR_AND_CONTINUE         = -96,  /**< (-96) Write an error message to standard error and continue from error handler, execution continues as if error handler has not been called. API function returns an error code. */
    SC_ERR_HANDLER_THROW_EXCEPTION                      = -97,  /**< (-97) API function that called the error handler will throw a C++ @ref SC_Exception regardless of @ref SC_LibraryOptions.ThrowExceptions setting. */
    SC_ERR_HANDLER_CONTINUE                             = -98,  /**< (-98) Continue from error handler, execution continues as if error handler has not been called. API function returns an error code. */
    SC_ERR_UNSPECIFIED                                  = -99   /**< (-99) Unspecified miscellaneous error. */

} SC_Error;


/**
 *  Send flags used in @ref SC_SendBucket function.
 */
typedef enum
{
    SC_SEND_NONE                = 0x00,     /**< (0x00) No options defined, this is an invalid value. */
    SC_SEND_DEFAULT             = 0x01,     /**< (0x01) Use current default options. Use function @ref SC_SetSendBucketDefaultOptions to
                                                 change the defaults. */
    SC_SEND_SINGLE_PACKET       = 0x02,     /**< (0x02) Use Tx DMA single packet functionality to immediately write the bucket
                                                 into the Tx DMA FIFO. */
    SC_SEND_SINGLE_LIST         = 0x04,     /**< (0x04) Use Tx DMA list of packets functionality to immediately write the bucket
                                                 into the Tx DMA FIFO using a list containing only one packet.
                                                 Not currently supported. */
    SC_SEND_QUEUE_LIST          = 0x08,     /**< (0x08) Use Tx DMA list of packets functionality to queue into a list.
                                                 The list has to be flushed with the @ref SC_Flush function.
                                                 However, the list will be flushed automatically if list length exceeds
                                                 a predefined limit.
                                                 Not currently supported. */
    SC_SEND_WAIT_FOR_COMPLETION = 0x10,     /**< (0x10) Wait for completion when used together with @ref SC_SEND_SINGLE_LIST or
                                                  @ref SC_SEND_SINGLE_PACKET option. Completion means the bucket payload has been
                                                  moved from host memory to internal buffer in the FPGA and is signalled by the DMA mechanism
                                                  by writing zero after bucket when a single packet is sent or
                                                  by writing zero to beginning of list when a list is sent. */
    SC_SEND_PRIORITY            = 0x20,     /**< (0x20) Use Tx DMA priority FIFO instead of the normal FIFO. */
    SC_SEND_WAIT_FOR_PIPE_FREE  = 0x40      /**< (0x40) Wait until the pipeline is free to send, without this flag @ref SC_SendBucket
                                                 returns @ref SC_ERR_PIPE_FULL if send cannot complete. */

} SC_SendOptions;

/**
 *  Flush flags used in @ref SC_Flush function.
 */
typedef enum
{
    SC_FLUSH_NORMAL                 = 0x00,             /**< (0x00) Flush into normal Tx DMA FIFO. */
    SC_FLUSH_PRIORITY               = 0x02,             /**< (0x02) Flush into priority Tx DMA FIFO. */
    SC_FLUSH_WAIT_FOR_COMPLETION    = 0x01,             /**< (0x01) Wait until completed. Not currently supported. */
    SC_FLUSH_WAIT_FOR_PIPE_FREE     = 0x04              /**< (0x04) Wait until pipe is free, otherwise @ref SC_Flush will return error @ref SC_ERR_PIPE_FULL. */

} SC_FlushOptions;

/**
 *  Symbolic names for infinite and no timeout for use with @ref SC_GetNextPacket.
 */
typedef enum
{
    SC_TIMEOUT_INFINITE = -1,               /**< (-1) Symbolic name for infinite timeout. */
    SC_TIMEOUT_NONE     = 0                 /**< (0) Symbolic name for no timeout. */

} SC_Timeout;

/**
 *  User logic demo options.
 */
typedef struct
{
    uint16_t        StructSize;             /**< Size of this structure. */
    uint32_t        RegisterIndex;          /**< User logic register index. */

} SC_DemoDesignOptions;

/**
 *  Enums selecting various user logic or other demo designs available in the FPGA.
 */
typedef enum
{
    SC_DEMO_USER_LOGIC_GENERIC          = 1,    /**< (1) Enable/disable user logic generic demo design (demo is selected by the register write value). */
    SC_DEMO_USER_LOGIC_RTT_LATENCY      = 2,    /**< (2) Perform a round trip time latency measurement. */
    SC_DEMO_USER_LOGIC_INTERRUPTS       = 3     /**< (3) User logic interrupts demo. */

} SC_DemoDesign;

/** @} ConstantsMacros */

/**
 *  Forward definitions of some internal data structures.
 *  The actual structure definition is private information and not publicly visible or accessible.
 *  Do not access any members of these types directly, that is private information
 *  that is subject to change from release to release without notice.
 *  There should be accessor functions to get all necessary information.
 *  If some needed information is missing then make a request to add
 *  an accessor function for it.<br><br>
 */
typedef struct SC_DeviceContext * SC_DeviceId;      /**< A device context. */
typedef struct SC_ChannelContext * SC_ChannelId;    /**< A channel context. */
typedef struct __SC_Bucket__ SC_Bucket;             /**< A bucket for sending data. */
typedef struct __SC_Packet__ SC_Packet;             /**< A packet for receiving data. */

/** **************************************************************************
 * @defgroup InitialisationCleanup Initialisation and cleanup.
 * @{
 * @brief Create and destroy a device context, configure IP adresses.
 */

/**
 *  Error handler callback function type that can be registered with the @ref SC_RegisterErrorHandler function.
 *
 *  @param  fileName        Name of file where the problem occurred.
 *  @param  lineNumber      Line number in the file where the problem occurred.
 *  @param  functionName    Name of the function where the problem occurred.
 *  @param  errorCode       SC_Error error code related to the problem.
 *  @param  errorMessage    Textual formatted error message describing the problem.
 *  @param  pUserContext    Pointer to an optional user defined context.
 *
 *  @return                 Return a code that tells the library how to continue error processing:
 *                          - SC_ERR_HANDLER_CONTINUE continues error processing in the library in a normal way,
 *                            i.e. the API function that failed returns the original error code like the error handler had not been called.
 *                          - SC_ERR_HANDLER_THROW_EXCEPTION throws a C++ @ref SC_Exception with the same parameters as the error handler was called with.
 *                          - Any other return code will be substituted as the error code returned by the failed API call.
 */
typedef SC_Error SC_ErrorHandler(const char * fileName, int lineNumber, const char * functionName, SC_Error errorCode, const char * errorMessage, void * pUserContext);

/**
 *  Register an error handler callback. Only one error handler can be registered replacing the previous one.
 *
 *  @param pErrorHandler    Error handler callback to be called when error occurs in API library. Use value of NULL to disable callbacks.
 *  @param pUserContext     Optional user context or NULL to pass along to the error handler. This can be used to pass some user specific
 *                          information to the error handler or to C++ @ref SC_Exception exceptions if throwing exceptions is enabled.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_RegisterErrorHandler(SC_ErrorHandler * pErrorHandler, void * pUserContext);

/**
 *  Get library options.
 *
 *  @param pLibraryOptions      Pointer to library options that receives the current library options. Cannot be NULL.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetLibraryOptions(SC_LibraryOptions * pLibraryOptions);

/**
 *  Set library options.
 *
 *  @param pLibraryOptions      Pointer to library options that receives the current library options.
 *                              Pass NULL to set default library options.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetLibraryOptions(SC_LibraryOptions * pLibraryOptions);

/**
 *  Open device for access.
 *
 *  @param deviceName       Name to device node in '/dev' for the card. If NULL then
 *                          "/dev/smartnic" is used.
 *  @param pDeviceOptions   Optional device parameters. Pass NULL to use default options.
 *
 *  @return             SC_DeviceId on success, NULL on error. Call function @ref SC_GetLastErrorCode
 *                      to get a more specific error code.
 */
SC_DeviceId SC_OpenDevice(const char * deviceName, SC_DeviceOptions * pDeviceOptions);

/**
 *  Close the device.
 *
 *  @brief              All channels will be closed if not already explicitly closed.
 *
 *  @param deviceId     Device id that shall be destroyed.
 *
 *  @return             SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_CloseDevice(SC_DeviceId deviceId);

/**
 *  Set the local parameters (IP data, MAC address) for the network interface.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the physical port.
 *  @param localIP              Local IP address
 *  @param netMask              Netmask for the interface
 *  @param gateway              Gateway address
 *  @param macAddress           MAC address of the network interface. If NULL the card
 *                              uses its factory MAC address for the interface.
 *
 *  @note Only IPv4 addresses are supported.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetLocalAddress(SC_DeviceId deviceId, uint8_t networkInterface, struct in_addr localIP,
                            struct in_addr netMask, struct in_addr gateway, const uint8_t macAddress[SC_MAC_ADDRESS_LENGTH]);

/**
 *  Get the local IP parameters for a network interface.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the card enumerated from zero.
 *  @param pLocalIP             Pointer to where the local IP address is delivered.
 *  @param pNetMask             Pointer to where the netmask for the interface is delivered.
 *  @param pBroadcastAddress    Pointer to where the broadcast address is delivered.
 *  @param macAddress           Pointer to where the MAC address of the
 *                              network interface is delivered.
 *
 *  @note Only IPv4 addresses are supported.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetLocalAddress(SC_DeviceId deviceId, uint16_t networkInterface, struct in_addr * pLocalIP,
                            struct in_addr * pNetMask, struct in_addr * pBroadcastAddress, uint8_t macAddress[SC_MAC_ADDRESS_LENGTH]);

/**
 *  Reset the FPGA without reloading the driver. Note that no other activity should be
 *  attempted on this device while the FPGA reset is in progress. Doing so will result in undefined behaviour.
 *
 *  @param deviceId             Device id
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_ResetFPGA(SC_DeviceId deviceId);

/**
 *  Enable FPGA user logic interrupts.
 *
 *  @param deviceId                         Device id
 *  @param pUserLogicInterruptOptions       Pointer to @ref SC_UserLogicInterruptOptions that define
 *                                          all parameters relevant to user logic interrupts.
 *                                          Pass NULL to use default options.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_EnableUserLogicInterrupts(SC_DeviceId deviceId, SC_UserLogicInterruptOptions * pUserLogicInterruptOptions);

/**
 *  Get the user logic interrupt results from a siginfo_t structure.
 *
 *  @param pSignalInfo                      Pointer to a siginfo_t structure received in a signal handler.
 *  @param pUserLogicInterruptResults       Pointer to @ref SC_UserLogicInterruptResults that returns
 *                                          all parameters relevant to a user logic interrupt.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetUserLogicInterruptResults(const siginfo_t * pSignalInfo, SC_UserLogicInterruptResults * pUserLogicInterruptResults);

/**
 *  Get user logic interrupt options for a given device.
 *
 *  @param deviceId                         Device id. Pass NULL to return library default user logic interrupt options.
 *  @param pUserLogicInterruptOptions       Pointer to @ref SC_UserLogicInterruptOptions that define
 *                                          all parameters relevant to user logic interrupts.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetUserLogicInterruptOptions(SC_DeviceId deviceId, SC_UserLogicInterruptOptions * pUserLogicInterruptOptions);

/**
 *  Set user logic interrupt options for a given device.
 *
 *  @param deviceId                         Device id. Pass NULL to set library default user logic interrupt options.
 *  @param pUserLogicInterruptOptions       Pointer to @ref SC_UserLogicInterruptOptions that returns
 *                                          all parameters relevant to user logic interrupts.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetUserLogicInterruptOptions(SC_DeviceId deviceId, SC_UserLogicInterruptOptions * pUserLogicInterruptOptions);

/** @} InitialisationCleanup */

/** **************************************************************************
 * @defgroup SystemInfo Miscellaneous system information.
 * @{
 * @brief Get various information about system state and configuration.
 */

/**
 *  Get library and driver version number.
 *
 *  Ask library and driver for a version string.
 *
 *  @param deviceId         Device id
 *  @param pLibraryVersion  Pointer to API library version structure.
 *  @param pDriverVersion   Pointer to driver version structure.
 *
 *  @note This function is for <b>INTERNAL USE ONLY</b>. It must not be
 *        used by user space programs.
 *
 *  @return 0 if version was succesfully retrieved, 0 otherwise.
 */
SC_Error SC_GetLibraryAndDriverVersion(SC_DeviceId deviceId, SC_Version * pLibraryVersion, SC_Version * pDriverVersion);

/**
 *  Get FPGA temperatures.
 *
 *  Temperatures are reported in degrees C
 *
 *  @param deviceId                 Device id
 *  @param pMinimumTemperature      Pointer to a value that receives the minimum FPGA temperature.
 *  @param pCurrentTemperature      Pointer to a value that receives the current FPGA temperature.
 *  @param pMaximumTemperature      Pointer to a value that receives the maximum FPGA temparature.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetFpgaTemperature(SC_DeviceId deviceId, float * pMinimumTemperature, float * pCurrentTemperature, float * pMaximumTemperature);

/*
 *  @name Link status bit masks.
 *  @brief Used when decoding the value returned by @ref SC_GetlinkStatus
 */
/* FIXME Those defines do not match the way fbconfig decodes the linkStatus value. who is right? */
#if 0
#define PORT_0_LINK_STATUS  (0x1LL << 0)         /**< Port 0 link status bit */
#define PORT_1_LINK_STATUS  (0x1LL << 7)         /**< Port 1 link status bit */
#define PORT_2_LINK_STATUS  (0x1LL << 15)        /**< Port 2 link status bit */
#define PORT_3_LINK_STATUS  (0x1LL << 23)        /**< Port 3 link status bit */
#define PORT_4_LINK_STATUS  (0x1LL << (0+32))    /**< Port 4 link status bit */
#define PORT_5_LINK_STATUS  (0x1LL << (7+32))    /**< Port 5 link status bit */
#define PORT_6_LINK_STATUS  (0x1LL << (15+32))   /**< Port 6 link status bit */
#define PORT_7_LINK_STATUS  (0x1LL << (23+32))   /**< Port 7 link status bit */
#endif

/**
 *  Get link status for network ports.
 *
 *  @param deviceId         Device id.
 *  @param pLinkStatus      Pointer to where bitmask for link status shall be
 *                          returned.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetLinkStatus(SC_DeviceId deviceId, uint64_t * pLinkStatus);

/**
 *  Get miscellaneous card information.
 *
 *  @brief  Returns miscellaneous static card and device info in the @ref SC_CardInfo structure.
 *
 *  @param deviceId         Device id
 *  @param pCardInfo        Pointer to where the card info is delivered.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetCardInfo(SC_DeviceId deviceId, SC_CardInfo * pCardInfo);

/**
 *  Get miscellaneous system information.
 *
 *  @brief  Returns miscellaneous static system information in the @ref SC_SystemInfo structure.
 *          If the system information changes with time then a new call to @ref SC_GetSystemInfo
 *          is required to update the information.
 *
 *  @param pSystemInfo      Pointer to where the system info is delivered.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetSystemInfo(SC_SystemInfo * pSystemInfo);

/**
 *  Get device options.
 *
 *  @brief  Returns device options for a device context.
 *
 *  @param deviceId         Device id. Pass NULL to get library default device options.
 *  @param pDeviceOptions   Pointer to device options.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetDeviceOptions(SC_DeviceId deviceId, SC_DeviceOptions * pDeviceOptions);

/**
 *  Set device options.
 *
 *  @brief  Sets device options for a device context.
 *
 *  @param deviceId         Device id. Pass NULL to set library default device options.
 *  @param pDeviceOptions   Pointer to device options.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetDeviceOptions(SC_DeviceId deviceId, SC_DeviceOptions * pDeviceOptions);

/**
 *  Get channel options.
 *
 *  @brief  Returns channel options for a channel context.
 *
 *  @param channelId         Channel id. Pass NULL to get library default channel options.
 *  @param pChannelOptions   Pointer to channel options.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetChannelOptions(SC_ChannelId channelId, SC_ChannelOptions * pChannelOptions);

/**
 *  Set channel options.
 *
 *  @brief  Sets channel options for a channel context.
 *
 *  @param channelId         Channel id. Pass NULL to set library default channel options.
 *  @param pChannelOptions   Pointer to channel options.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetChannelOptions(SC_ChannelId channelId, SC_ChannelOptions * pChannelOptions);

/**
 *  Get driver logging mask.
 *
 *  @param deviceId         Device id
 *  @param pLogMask         Pointer to where the logging mask will be be delivered.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetDriverLogMask(SC_DeviceId deviceId, uint64_t * pLogMask);

/**
 *  Set driver logging mask.
 *
 *  @param deviceId         Device id
 *  @param logMask          Logging mask
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetDriverLogMask(SC_DeviceId deviceId, uint64_t logMask);


/**
 *  Set timestamping mode.
 *
 *  Card timer can either be regulated by software or a by hardware via a pulse per second signal.
 *
 *  @param deviceId Device id
 *  @param mode Mode @ref SC_TimeControl. TIMECONTROL_SOFTWARE is the default mode.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetTimeStampingMode(SC_DeviceId deviceId, SC_TimeControl mode);


/**
 *  Get packet/Ethernet frame statistics for all network interfaces.
 *
 *  @param deviceId             Device id. NULL fills the statistics structure with default values.
 *  @param pPacketStatistics    Pointer to data structure where statistics data shall be
 *                              delivered.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetStatistics(SC_DeviceId deviceId, SC_PacketStatistics * pPacketStatistics);

/**
 *  Get miscellaneous status information.
 *
 *  @param deviceId             Device id. NULL fills status info with default values.
 *  @param pStatusInfo          Pointer to data structure where status information data shall be
 *                              delivered.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetStatusInfo(SC_DeviceId deviceId, SC_StatusInfo * pStatusInfo);

/**
 *  Get the last error code from the API library.
 *
 *  @return The last error code.
 */
SC_Error SC_GetLastErrorCode();

/**
 *  Set the last error code in the API library.
 *
 *  @param  errorCode       The error code to set.
 *
 *  @return The previous error code.
 */
SC_Error SC_SetLastErrorCode(SC_Error errorCode);

/**
 *  Get the number of channels available on this device.
 *
 *  @param deviceId         Device id
 *
 *  @return The number of available channels..
 */
int32_t SC_GetNumberOfChannels(SC_DeviceId deviceId);

/**
 *  Get the logging mask of this device.
 *
 *  @param deviceId         Device id
 *  @param pLogMask         Pointer to logging mask that is returned.
 *
 *  @return                 SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetLibraryLogMask(SC_DeviceId deviceId, SC_LogMask * pLogMask);

/**
 *  Set the logging mask of this device.
 *
 *  @param deviceId         Device id.
 *  @param logMask          The logging mask to set.
 *
 *  @return The previous device logging mask or negative error code @ref SC_Error.
 */
SC_Error SC_SetLibraryLogMask(SC_DeviceId deviceId, SC_LogMask logMask);

/**
 *  Check if given network interface has an OOB (standard NIC) configured.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *
 *  @return                     True if the given network interface has an OOB (standard NIC) configured and false if it doesn't.
 */
SC_Boolean SC_IsOobNetworkInterface(SC_DeviceId deviceId, uint8_t networkInterface);

/**
 *  Check if given network interface has a TCP (TOE) configured.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *
 *  @return                     True if the given network interface has a TCP (TOE) configured and false if it doesn't.
 */
SC_Boolean SC_IsTcpNetworkInterface(SC_DeviceId deviceId, uint8_t networkInterface);

/**
 *  Check if given network interface has an UDP configured.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *
 *  @return                     True if the given network interface has an UDP configured and false if it doesn't.
 */
SC_Boolean SC_IsUdpNetworkInterface(SC_DeviceId deviceId, uint8_t networkInterface);

/**
 *  Check if given network interface has a MAC configured.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *
 *  @return                     True if the given network interface has a MAC configured and false if it doesn't.
 */
SC_Boolean SC_IsMacNetworkInterface(SC_DeviceId deviceId, uint8_t networkInterface);

/**
 *  Check if given physical port has a QSFP configured.
 *
 *  @param deviceId             Device id.
 *  @param port                 Physical port.
 *
 *  @return                     True if the given port has a QSFP configured and false if it doesn't.
 */
SC_Boolean SC_IsQsfpPort(SC_DeviceId deviceId, uint8_t port);

/**
 *  Get FPGA register base that is mapped into user space.
 *
 *  @param  deviceId        Device id.
 *
 *  @return                 Return a volatile pointer to the beginning of the user space FPGA registers.
 */
volatile uint64_t * SC_GetFpgaRegistersBase(SC_DeviceId deviceId);

/**
 *  Get FPGA Tx DMA FIFO fill levels.
 *
 *  @param  deviceId                Device id.
 *  @param  pNormalFifoFillLevel    Pointer to a variable that receives the normal Tx DMA FIFO fill level or
 *                                  NULL if the value is not wanted.
 *  @param  pPriorityFifoFillLevel  Pointer to a variable that receives the priority Tx DMA FIFO fill level or
 *                                  NULL if the value is not wanted.
 *
 *  @return                         SC_ERR_SUCCESS on success, an error code otherwise. The FIFO fill level values 
 *                                  are returned via parameters @p pNormalFifoFillLevel and @p pPriorityFifoFillLevel.
 */
SC_Error SC_GetTxDmaFifoFillLevels(SC_DeviceId deviceId, uint16_t * pNormalFifoFillLevel, uint16_t * pPriorityFifoFillLevel);

/** @} SystemInfo */

/** **************************************************************************
 * @defgroup ChannelSetup Initialisation of TCP, UDP, user logic and monitoring channels.
 * @{
 */

/**
 *  Allocate a TCP channel.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *  @param channel              Instruct FPGA to use specific TCP channel. Use the value
 *                              -1 to let the driver assign the channel. In that case
 *                              the selected channel can be retrieved with @ref SC_GetChannelNumber function.
 *                              Note that channel numbering has changed beginning with release 2.5.0.
 *                              See  @ref SC_GetChannelNumber for details.
 *  @param pChannelOptions      Optional channel options. Pass NULL as value to use default values.
 *
 *  @return                     Channel id on success, NULL on error. Call function @ref SC_GetLastErrorCode
 *                              to get a more specific error code.
 */
SC_ChannelId SC_AllocateTcpChannel(SC_DeviceId deviceId, uint8_t networkInterface, int16_t channel, SC_ChannelOptions * pChannelOptions);

/**
 *  Allocate a UDP channel.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *  @param channel              Instruct FPGA to use specific UDP channel. Use the value
 *                              -1 to let the driver assign the channel. In that case
 *                              The selected channel can be retrieved with @ref SC_GetChannelNumber function.
 *                              Note that channel numbering has changed beginning with release 2.5.0.
 *                              See  @ref SC_GetChannelNumber for details.
 *  @param pChannelOptions      Optional channel options. Pass NULL as value to use default values.
 *
 *  @return                     Channel id on success, NULL on error. Call function @ref SC_GetLastErrorCode
 *                              to get a more specific error code.
 */
SC_ChannelId SC_AllocateUdpChannel(SC_DeviceId deviceId, uint8_t networkInterface, int16_t channel, SC_ChannelOptions * pChannelOptions);

/**
 *  Allocate a monitoring channel.
 *
 *  @param deviceId             Device id.
 *  @param networkInterface     Network interface in the physical port.
 *  @param pChannelOptions      Optional channel options. Pass NULL as value to use default values.
 *
 *  @return                     Channel id on success, NULL on error. Call function @ref SC_GetLastErrorCode
 *                              to get a more specific error code.
 *
 * @note <b>Monitor functionality is not supported in this release of SmartNIC.</b>
 */
SC_ChannelId SC_AllocateMonitorChannel(SC_DeviceId deviceId, uint8_t networkInterface, SC_ChannelOptions * pChannelOptions);

/**
 *  Allocate a user logic channel.
 *
 *  @param deviceId             Device id.
 *  @param channel              User logic channel number 0 .. 7.
 *  @param pChannelOptions      Optional channel options. Pass NULL as value to use default values.
 *
 *  @return                     Channel id on success, NULL on error. Call function @ref SC_GetLastErrorCode
 *                              to get a more specific error code.
 */
SC_ChannelId SC_AllocateUserLogicChannel(SC_DeviceId deviceId, int16_t channel, SC_ChannelOptions * pChannelOptions);

/**
 *  Deallocate a channel.
 *
 *  @param channelId            Channel id.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_DeallocateChannel(SC_ChannelId channelId);

/**
 *  Get number of user logic channels.
 *
 *  @return                     Positive number of user logic channels or a negative error code.
 */
int16_t SC_GetNumberOfUserLogicChannels();

/**
 *  Return the file descriptor for an allocated channel.
 *
 *  @brief  The file descriptor that this function provides is used with
 *          standard system calls. The channel must be allocated before
 *          calling this function.
 *
 *  @param deviceId             Device id.
 *  @param channelId            Channel id.
 *
 *  @return                     A positive file descriptor opened for this channel
 *                              a negative error code on error.
 */
int SC_GetChannelFileDescriptor(SC_DeviceId deviceId, SC_ChannelId channelId);

/** @} ChannelSetup */

/** **************************************************************************
 * @defgroup TCPConnections Handle TCP connections.
 * @{
 * @brief Make and break connections, listen on ports and get TCP status.
 *
 * @warning     You must have allocated a TCP channel using @ref SC_AllocateTcpChannel
 *              before calling any of the functions in this module.
 */

 /**
  *  Get channel TCP connect options.
  *
  *  @param channelId            Channel id. Pass NULL to get the library default connect options.
  *  @param pConnectOptions      Pointer to connect options that receives the current values.
  *
  *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
  */
SC_Error SC_GetConnectOptions(SC_ChannelId channelId, SC_ConnectOptions * pConnectOptions);

/**
 *  Set channel TCP connect options.
 *
 *  @param channelId            Channel id. Pass NULL to set the library default connect options.
 *  @param pConnectOptions      Pointer to connect options that receives the current values.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetConnectOptions(SC_ChannelId channelId, SC_ConnectOptions * pConnectOptions);

/**
 *  Get channel TCP listen options.
 *
 *  @param channelId            Channel id. Pass NULL to get library default listen options.
 *  @param pListenOptions       Pointer to listen options that receives the current values.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetListenOptions(SC_ChannelId channelId, SC_ListenOptions * pListenOptions);

/**
 *  Set channel TCP listen options.
 *
 *  @param channelId            Channel id. Pass NULL to set library default listen options.
 *  @param pListenOptions       Pointer to listen options that receives the current values.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetListenOptions(SC_ChannelId channelId, SC_ListenOptions * pListenOptions);

/**
 *  Listen for incoming connection.
 *
 *  @note This call blocks until a connection is obtained or optionally times out.
 *
 *  @param channelId            Channel id.
 *  @param localPort            Port to listen on
 *  @param pListenOptions       Pointer to optional listen options or NULL for defaults.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_Listen(SC_ChannelId channelId, uint16_t localPort, SC_ListenOptions * pListenOptions);

/**
 *  Cancel a pending listen.
 *
 *  @note   This cancels a blocking @ref SC_Listen call which returns an error code @ref SC_ERR_LISTEN_CANCELLED_BY_USER.
 *          Obviously this call has to be done from another thread than the blocking @ref SC_Listen call that it cancels.
 *          This call is currently experimental and might or might not work as expected.
 *
 *  @param channelId            Channel id.
 *  @param pReserved            Reserved for future use. Use NULL as value.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_CancelListen(SC_ChannelId channelId, void * pReserved);

/**
 *  Connect to remote machine.
 *
 *  @param channelId            Channel id
 *  @param remoteHost           Remote host IPV4 address.
 *  @param remotePort           Port on remote to connect to.
 *  @param pConnectOptions      Pointer to optional connection parameters structure @ref SC_ConnectOptions
 *                              or NULL for defaults.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_Connect(SC_ChannelId channelId, const char * remoteHost, uint16_t remotePort,
                    SC_ConnectOptions * pConnectOptions);

/**
 *  Disconnect the TCP connection
 *
 *  @param channelId            Channel id
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_Disconnect(SC_ChannelId channelId);

/**
 *  Get state of TCP channel. This only works on ETLM (TOE) channels.
 *
 *  @param channelId            Channel id
 *
 *  @return                     The TCP channel state or negative error code on failure.
 */
SC_TcpState SC_GetTcpState(SC_ChannelId channelId);

/**
 *  Get state of TCP channel as a string.
 *
 *  @param tcpState             TCP state as @ref SC_TcpState.
 *
 *  @return         TCP channel state as a string or "*** UNKNOWN ***" for unknown state.
 */
const char * SC_GetTcpStateAsString(SC_TcpState tcpState);

/**
 *  Abort ETLM TCP engine on the given channel.
 *
 *  @brief  Abort the ETLM channel and returns it to a SC_TCP_CLOSED state.
 *          A TCP reset returns the counterpart into a closed state too.
 *          Aborting the ETLM is the only way to get away from the SC_TCP_FIN_WAIT2 state.
 *          After abort has completed the ETLM will be in SC_TCP_CLOSED state
 *          and ready for new connections or listens.
 *
 *  @param channelId            Channel id
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_TcpAbort(SC_ChannelId channelId);

/**
 *  Get last TCP acknowledged bytes number (Tcb.SndUna). This is a 32-bit counter
 *  that counts the acknowledged sent bytes on an ETLM channel. You can use this
 *  value to see when all your TCP data has actually been transmitted and acknodledged
 *  by the counterpart. If you transmit more than 4 GiB then the counter wraps around.
 *
 *  @param channelId            Channel id
 *  @param pLastAckNumber       Pointer to where last acknowledge number shall be
 *                              delivered.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetLastTcpAckNumber(SC_ChannelId channelId, uint32_t * pLastAckNumber);

/** @} TCPConnections */

/** **************************************************************************
 * @defgroup PacketReception Handle incoming packets.
 * @{
 *
 * @warning You must have allocated a channel using one of the channel allocation
 * functions in section @ref ChannelSetup before calling any of the functions in this module.
 */

/**
 *  Return the file descriptor for the device.
 *
 *  @brief  The file descriptor that this function provides is used with
 *          standard system calls.
 *
 *  @brief Use the file descriptor and standard system calls to get incoming
 *  packets.
 *
 *  Due to the use of system calls that rely on interrupts and this method is
 *  probably slower than the @ref PacketReception "raw memory access method".
 *
 *  @param deviceId             Device id
 *
 *  @return                     File descriptor opened for this device.
 */
int SC_GetFileDescriptor(SC_DeviceId deviceId);

/**
 *  Get the next incoming packet from the packet ring buffer (PRB) based on previous
 *  incoming packet. There is only API access to packet payload and payload length.
 *  Packet header is not available and can change from release to release.
 *  Packets are padded up to a multiple of a CPU cache line length (64 bytes).
 *
 *  This function always returns immediately but it may not return a packet.
 *
 *  @param channelId                Channel id
 *  @param pPreviousPacket          Pointer to previous packet or NULL if this is the first call to this function.
 *  @param timeoutInMicroseconds    Timeout in microseconds, @ref SC_TIMEOUT_NONE for no timeout
 *                                  or @ref SC_TIMEOUT_INFINITE to wait forever for next packet.
 *
 *  @return     Pointer to received packet or NULL and @ref SC_GetLastErrorCode() is SC_ERR_SUCCESS if no new packets are available.
 *              Returns NULL and  @ref SC_GetLastErrorCode() is not SC_ERR_SUCCESS if an error occurred.
 *
 *  Usage example:
 *
 *  @code{.cpp}
 *
 *  const SC_Packet * pPreviousPacket = NULL;
 *  uint64_t packetCount = 0;
 *
 *  // Continuous receive loop:
 *  while (true)
 *  {
 *      const SC_Packet * pPacket = SC_GetNextPacket(channelId, pPreviousPacket, SC_TIMEOUT_INFINITE);
 *      if (pPacket == NULL && SC_GetLastErrorCode() != SC_ERR_SUCCESS)
 *      {
 *          // Error handling
 *          continue;
 *      }
 *
 *      const uint8_t * payload = SC_GetUserLogicPayload(pPacket);
 *      uint16_t payloadLength = SC_GetUserLogicPayloadLength(pPacket);
 *
 *      // Process payload here...
 *
 *      if (++packetCount % 1024 == 0)
 *      {
 *          // For every 1024th received packet acknowledge to the FPGA that
 *          // all packets up to the last received packet have been processed.
 *          // The value of constant 1024 is dependent on average packet length
 *          // and size of the packet ring buffer (PRB) which is 4 MB per default.
 *          // For example if all packets are 2 KB then 1024 * 2 KB = 2 MB or
 *          // half the size of the PRB so 1024 is a conservative value and
 *          // should not affect packet receive performance very much.
 *
 *          // Another way of deciding when to call SC_UpdateReceivePtr is
 *          // to sum together all received payload lengths and call SC_UpdateReceivePtr
 *          // when the total is approaching the 4 MB PRB size. Just beware
 *          // that if the Rx DMA totally fills the PRB then you start losing packets.
 *
 *          SC_Error errorCode = SC_UpdateReceivePtr(channelId, pPacket);
 *          if (errorCode != SC_ERR_SUCCESS)
 *              // Error handling
 *      }
 *  }
 *
 *  @endcode
 */
const SC_Packet * SC_GetNextPacket(SC_ChannelId channelId, const SC_Packet * pPreviousPacket,
                                   int64_t timeoutInMicroseconds);

/**
 *  Update read marker in FPGA receive buffer.
 *
 *  Call this function once in a while to free up space in the
 *  receive packet ring buffer (PRB). It is not necessary nor efficient to
 *  call it for every incoming packet.
 *
 *  @param channelId            Channel id
 *  @param pPacket              Pointer to most recently processed packet.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 *
 *  For usage example see @ref SC_GetNextPacket.
 */
SC_Error SC_UpdateReceivePtr(SC_ChannelId channelId, const SC_Packet * pPacket);

/**
 *  Get the time stamp from a packet.
 *
 *  @param pPacket          Pointer to the packet.
 *  @param pSeconds         Pointer to seconds part of timestamp.
 *  @param pNanoseconds     Pointer to nanoseconds part of timestamp.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetPacketTimestamp(const SC_Packet * pPacket, uint32_t * pSeconds, uint32_t * pNanoseconds);

/**
 *  Get pointer to TCP payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Pointer to payload
 */
const uint8_t * SC_GetTcpPayload(const SC_Packet * pPacket);

/**
 *  Get pointer to UDP payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Pointer to payload
 */
const uint8_t * SC_GetUdpPayload(const SC_Packet * pPacket);

/**
 *  Get length of TCP payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Length of payload
 */
uint16_t SC_GetTcpPayloadLength(const SC_Packet * pPacket);

/**
 *  Get length of UDP payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Length of payload
 */
uint16_t SC_GetUdpPayloadLength(const SC_Packet * pPacket);

/**
 *  Get pointer to raw payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Pointer to payload
 */
const uint8_t * SC_GetRawPayload(const SC_Packet * pPacket);

/**
 *  Get pointer to user logic payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Pointer to payload
 */
const uint8_t * SC_GetUserLogicPayload(const SC_Packet * pPacket);

/**
 *  Get pointer to packet payload.
 *
 *  @param channelId     Channel identifier or NULL to get the raw payload.
 *  @param pPacket       Pointer to packet
 *
 *  @return Pointer to payload or NULL in case of error. Call @ref SC_GetLastErrorCode to get the error code.
 */
const uint8_t * SC_GetPacketPayload(SC_ChannelId channelId, const SC_Packet * pPacket);

/**
 *  Get length of user logic payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Length of payload
 */
uint16_t SC_GetUserLogicPayloadLength(const SC_Packet * pPacket);

/**
 *  Get length of raw payload.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Length of payload
 */
uint16_t SC_GetRawPayloadLength(const SC_Packet * pPacket);

/**
 *  Get length of packet payload.
 *
 *  @param channelId     Channel identifier or NULL get get raw payload length.
 *  @param pPacket       Pointer to packet
 *
 *  @return Length of payload or negative error code.
 */
int16_t SC_GetPacketPayloadLength(SC_ChannelId channelId, const SC_Packet * pPacket);

/**
 *  Get the total size of available receive packet ring buffer (PRB) on this channel.
 *
 *  @param channelId            Channel id
 *
 *  @return         The total size of the packet receive buffer.
 */
int64_t SC_GetPacketRingBufferSize(SC_ChannelId channelId);

/**
 *  Get network interface of UDP packet. Returns the value of packet header status network interface field which
 *  is only valid for packets received on UDP channels. Network interface value returned is undefined
 *  for packets received on other than UDP channels.
 *
 *  @param pPacket Pointer to packet
 *
 *  @return Network interface of UDP packet.
 */
uint16_t SC_GetNetworkInterface(const SC_Packet * pPacket);

/** @} PacketReception */

/** **************************************************************************
 * @defgroup PacketTransmission Handle outgoing packets.
 * @{
 * @brief Functions for administrating transmission buckets.
 *
 * @warning You must have allocated a channel using channel allocation
 * functions in section @ref ChannelSetup before calling
 * any of the functions in this module.
 */

/**
 *  Get default @ref SC_SendBucket options on this channel.
 *
 *  Call this function to get the @ref SC_SendOptions options to use when @ref SC_SEND_DEFAULT
 *  is used as a parameter to @ref SC_SendBucket.
 *
 *  @param channelId            Channel id.
 *  @param pDefaultSendOptions  Pointer to a variable that receives the default send options.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetSendBucketDefaultOptions(SC_ChannelId channelId, SC_SendOptions * pDefaultSendOptions);

/**
 *  Set default @ref SC_SendBucket options on this channel.
 *
 *  Call this function to set the @ref SC_SendOptions options to use when @ref SC_SEND_DEFAULT
 *  is used as a parameter to @ref SC_SendBucket.
 *
 *  @param channelId            Channel id.
 *  @param defaultSendOptions   Default send options.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetSendBucketDefaultOptions(SC_ChannelId channelId, SC_SendOptions defaultSendOptions);

/**
 *  Send a bucket on this channel.
 *
 *  Call this function to send a bucket on this channel. The bucket might be sent at once
 *  or queued for later sending or flushing depending on the argument values.
 *
 *  @param channelId            Channel id.
 *  @param pBucket              Pointer to the bucket to send.
 *  @param payloadLength        Length of the payload of bucket.
 *  @param sendOptions          The options from @ref SC_SendOptions describing the send options to use.
 *  @param pReserved            Reserved for future use. Pass NULL as value.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SendBucket(SC_ChannelId channelId, SC_Bucket * pBucket, uint16_t payloadLength, SC_SendOptions sendOptions, void * pReserved);

/**
 *  Flush all pending buckets collected into a Tx DMA packet list on this channel.
 *
 *  Call this function once in a while to flush all pending buckets
 *  on this channel that were sent with @ref SC_SendBucket using the @ref SC_SEND_QUEUE_LIST option.
 *
 *  @param channelId            Channel id
 *  @param flushOptions         Flush options, a combination of @ref SC_FlushOptions.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_Flush(SC_ChannelId channelId, SC_FlushOptions flushOptions);

/**
 *  Get a pointer to the payload of a bucket.
 *
 *  @param pBucket      Pointer to a bucket.
 *
 *  @return Byte pointer to the payload of the bucket..
 */
uint8_t * SC_GetBucketPayload(SC_Bucket * pBucket);

/**
 *  Get the maximum size of a bucket payload on this channel.
 *
 *  @param channelId            Channel id
 *
 *  @return Maximum size of a bucket payload or a negative error code.
 */
int16_t SC_GetMaxBucketPayloadSize(SC_ChannelId channelId);

/**
 *  Get the maximum number of buckets available on this channel.
 *
 *  @param channelId            Channel id
 *
 *  @return Total number of available buckets, used and unused.
 */
int32_t SC_GetMaxNumberOfBuckets(SC_ChannelId channelId);

/**
 *  @brief  Get the virtual channel number of this channel.
 *          <br><br>
 *          Note that in releases before 2.5.0 this function returned
 *          the internal DMA channel number (0-127). From release 2.5.0
 *          and onward channel numbering follows this scheme:
 *          <br>
 *               Channel Type    | Old DMA | New Virtual | Comment
 *          ---------------------|---------|-------------|-------------------------------------------------------------
 *          User logic channels  |  0-7    |  0-7        | Unchanged
 *          OOB channels         | 8-15    |  N/A        | Not accessible from API library interface so nothing changed
 *          Monitor channels     | 24-31   |  N/A        | Not accessible from API library interface so nothing changed
 *          UDP channels         | 32-63   | 0-31        | Changed to virtual channel number with numbering starting with 0
 *          TCP channels         | 64-127  | 0-63        | Changed to virtual channel number with numbering starting with 0
 *
 *          ETLM internally numbers the channels from 0 to 63 so there is no change there.
 *          <br><br>
 *          Using -1 for automatic assignment of the channel number still works as before except
 *          the resulting channel number follows the above scheme. API function SC_GetChannelNumber
 *          returns the new virtual channel numbers starting from release 2.5.0.
 *          <br><br>
 *          This change has been made to make the API interface independent of internal DMA channel numbering
 *          and mapping, i.e. which DMA channels and how many belong to each channel type. The internal DMA
 *          channel numbering might change in the the future depending on FPGA configuration.
 *
 *  @param channelId            Channel id
 *
 *  @return The channel number of this channel or negative error code on error.
 */
int32_t SC_GetChannelNumber(SC_ChannelId channelId);

/**
 *  Get the DMA channel number of the channel.
 *  <br>
 *  See @ref SC_GetChannelNumber for the relationship between virtual and DMA channel numbers.
 *
 *  @param channelId            Channel id
 *
 *  @return The DMA channel number between 0 and @ref SC_CardInfo::NumberOfDmaChannels - 1 of this channel or negative error code on error.
 */
int32_t SC_GetDmaChannelNumber(SC_ChannelId channelId);

/**
 *  Get the DMA channel number from a virtual channel number.
 * <br>
 *  See @ref SC_GetChannelNumber for the relationship between virtual and DMA channel numbers.
 *
 *  @param  channelType             Channel type @ref SC_ChannelType.
 *  @param  virtualChannelNumber    Virtual channel id.
 *
 *  @return The DMA channel number between 0 and @ref SC_CardInfo::NumberOfDmaChannels - 1 of this channel or negative error code on error.
 */
int32_t SC_GetDmaChannelNumberFromVirtualChannelNumber(SC_ChannelType channelType, uint16_t virtualChannelNumber);

/**
 *  Get the device associated with this channel.
 *
 *  @param channelId            Channel id
 *
 *  @return The device of this channel.
 */
SC_DeviceId SC_GetDeviceId(SC_ChannelId channelId);

/**
 *  Get next available free bucket for this channel.
 *
 *  After a successful send by @ref SC_SendBucket and when the bucket is
 *  ready for reuse it is automatically returned into an internal bucket pool.
 *  A bucket is ready for reuse after the payload has been read from the host
 *  memory by the Tx DMA and the descriptor entry in the Tx DMA FIFO queue has been
 *  processed.
 *
 *  An exception to this bucket life cycle can be made by disabling internal
 *  bucket sanity checks with @ref SC_LibraryOptions::EnforceBucketsSanity option.
 *  Some example applications (for example ul_dma_perf) violate the bucket life cycle
 *  by potentially resending buckets before they have been acknowledged by the FPGA
 *  DMA. This is acceptable in ul_dma_perf where we are only interested in DMA throughput
 *  and not bucket data integrity.
 *
 *  @param channelId            Channel id
 *  @param reserved             Reserved for future use. Pass zero (0) as value.
 *
 *  @return                     Pointer to a SC_Bucket or NULL if no free buckets are available.
 */
SC_Bucket * SC_GetBucket(SC_ChannelId channelId, int64_t reserved);

/** @} PacketTransmission */


/** **************************************************************************
 * @defgroup MultiCast IGMP setup
 * @{
 * @brief Functions for joining and leaving a multicast group.
 */

/**
 *  Get channel IGMP options.
 *
 *  @param channelId            Channel id
 *  @param pIgmpOptions         Pointer to optional IGMP settings and options or NULL for
 *                              defaults.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetIgmpOptions(SC_ChannelId channelId, SC_IgmpOptions * pIgmpOptions);

/**
 *  Set channel IGMP options.
 *
 *  @param channelId            Channel id
 *  @param pIgmpOptions         Pointer to optional IGMP settings and options or NULL for
 *                              defaults.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetIgmpOptions(SC_ChannelId channelId, SC_IgmpOptions * pIgmpOptions);

/**
 *  Join a multicast group.
 *
 *  @param channelId            Channel id
 *  @param networkInterface     Network interface in the physical port.
 *  @param group                Multicast group to join
 *  @param port                 Select IP port number that shall receive multicast traffic.
 *                              Calling this function again with a different port number
 *                              but otherwise same parameters adds that port number to the
 *                              list of ports to receive IGMP traffic on. Port number 0
 *                              means receive on all ports.
 *  @param pIgmpOptions         Pointer to optional IGMP settings and options or NULL for
 *                              defaults.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_IgmpJoin(SC_ChannelId channelId, uint8_t networkInterface, const char * group, uint16_t port, SC_IgmpOptions * pIgmpOptions);

/**
 *  Leave a multicast group.
 *
 *  @param channelId            Channel id
 *  @param networkInterface     Network interface in the physical port.
 *  @param group                Multicast group to leave.
 *  @param port                 Select IP port number that shall no longer receive multicast
 *                              traffic. Port number 0 means stop all reception from this
 *                              IGMP group.
 *  @param pIgmpOptions         Pointer to optional IGMP settings and options or NULL for
 *                              defaults.
 *
 *  @return                     SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_IgmpLeave(SC_ChannelId channelId, uint8_t networkInterface, const char * group, uint16_t port, SC_IgmpOptions * pIgmpOptions);

/** @} MultiCast */


/** **************************************************************************
 * @defgroup ULogicFunc Access to user logic registers
 * @{
 * @brief Function to read and write user logic registers. There is 32 registers of 64 bits
 */

/**
 *  Get base address of FPGA user logic registers that are mapped into user space.
 *
 *  @brief
 *
 *  This allows direct access to memory mapped user logic registers.
 *  Note that the registers are all 64-bits wide and have to be accessed
 *  as such. Any other kind of addressing produces undefined results.
 *  Total size of the mapped register memory is 1 MB and accessing
 *  memory over this size causes undefined results.
 *
 *  This is an alternative more direct and faster way of accessing
 *  user logic registers instead of using @ref SC_WriteUserLogicRegister
 *  or @ref SC_ReadUserLogicRegister functions.
 *
 *  @param deviceId         Device id.
 *
 *  @return     Base address of user space mapped FPGA user logic registers.
 *              On error NULL is returned. Call the @ref SC_GetLastErrorCode
 *              function to find the error cause.
 */
volatile uint64_t * SC_GetUserLogicRegistersBase(SC_DeviceId deviceId);

volatile uint64_t * EkalineGetWcBase(SC_DeviceId deviceId);

/**
 *  Write to user logic register.
 *
 * @brief   Please note that this function addresses registers by index like an array
 *          of uint64_t elements and not by memory offset. So index 0 addresses register
 *          at byte offset 0, index 1 addresses register at byte offset 8, etc.
 *          There is no locking to protect against simultaneous access from multiple threads.
 *          If such protection is needed you have to implement yourself.
 *
 * @param deviceId         Device id.
 * @param index            User logic register index 0, 1, 2, ... , 131071.
 * @param value            The 64 bit value to be written.
 *
 */
SC_Error SC_WriteUserLogicRegister(SC_DeviceId deviceId, uint32_t index, uint64_t value);

/**
 *  Read from user logic register.
 *
 * @brief   Please note that this function addresses registers by index like an array
 *          of uint64_t elements and not by memory offset. So index 0 addresses register
 *          at byte offset 0, index 1 addresses register at byte offset 8, etc.
 *          There is no locking to protect against simultaneous access from multiple threads.
 *          If such protection is needed you have to implement yourself.
 *
 * @param deviceId         Device id
 * @param index            User logic register index 0, 1, 2, ... , 131071.
 * @param pValue           Pointer to a 64 bits value where the result will be returned.
 *
 * @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_ReadUserLogicRegister(SC_DeviceId deviceId, uint32_t index, uint64_t * pValue);

/** @} ULogicFunc */

/****************************************************************************
 * @defgroup DemoDesignFunc Demo design functions
 * @{
 * @brief Functions to activate the demo designs in the FPGA.
 */

/**
 *  Activate a demo design.
 *
 *  By default demo designs are deactivated.
 *
 *  @param deviceId             Device id
 *  @param demoDesign           Demo design selector.
 *  @param parameter            Value to use depends on the selected demo.
 *  @param pDemoDesignOptions   Pointer to demo design options. Pass NULL for defaults.
 */
SC_Error SC_ActivateDemoDesign(SC_DeviceId deviceId, SC_DemoDesign demoDesign, uint64_t parameter, SC_DemoDesignOptions * pDemoDesignOptions);

/** @} DemoDesignFunc */

/**
 *  **************************************************************************
 * @defgroup ACLFunc ACL control functions
 * @{
 * @brief Functions to control the ACL (Access Control List) tables in firmware (edit, reset, show, enable, disable).
 *        ACL functionality in FPGA is an optional feature which is not present in all releases.
 *        ACL functions will return an error code if FPGA ACL support is missing.
 */

/**
 *  Modify an ACL entry in firmware.
 *
 *  Modifies an ACL entry pointed to by index in the firmware.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the physical port.
 *  @param aclId                Identifies either Tx or Rx ACL.
 *  @param index                Index into ACL table
 *  @param ipAddress            IP address to white or black list.
 *  @param ipMask               IP mask to white or black list.
 *  @param minimumPort          Lowest IP port number to white or black list
 *  @param maximumPort          Highest IP port number to white or black list
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_AclModify(SC_DeviceId deviceId, uint8_t networkInterface, SC_AclId aclId,
                      uint16_t index, struct in_addr ipAddress, struct in_addr ipMask, uint16_t minimumPort, uint16_t maximumPort);

/**
 *  Reset an ACL list entry in firmware.
 *
 *  ACL entry pointed to by index is reset in the ACL list.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the physical port.
 *  @param aclId                Identifies either Tx or Rx ACL.
 *  @param index                Index into ACL table
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_AclReset(SC_DeviceId deviceId, uint8_t networkInterface, SC_AclId aclId, int16_t index);

/**
 *  Get an ACL entry from firmware.
 *
 *  An ACL entry is read from the index position into the ACL list.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the physical port.
 *  @param aclId                Identifies either Tx or Rx ACL or both.
 *  @param index                Index into ACL table. Value of -1 will reset the whole table.
 *  @param pIPAddress           Pointer to IP address to white or black list.
 *  @param pIPMask              Pointer to IP mask to white or black list.
 *  @param pMinimumPort         Pointer to lowest IP port number to white or black list
 *  @param pMaximumPort         Pointer to highest IP port number to white or black list
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_AclGet(SC_DeviceId deviceId, uint8_t networkInterface, SC_AclId aclId, uint16_t index,
                   struct in_addr * pIPAddress, struct in_addr * pIPMask, uint16_t * pMinimumPort, uint16_t * pMaximumPort);

/**
 *  Enable all ACL lists in a network interface.
 *
 *  All ACL lists in a network interface are enabled.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the physical port.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_AclEnable(SC_DeviceId deviceId, uint8_t networkInterface);

/**
 *  Disable all ACL lists in a network interface.
 *
 *  All ACL lists in a network interface are disabled.
 *
 *  @param deviceId             Device id
 *  @param networkInterface     Network interface in the physical port.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_AclDisable(SC_DeviceId deviceId, uint8_t networkInterface);


/** @} ACLFunc */

/**
 *  **************************************************************************
 * @defgroup DMA_Setup Tx and Rx DMA configuration and control functions.
 * @{
 * @brief   Functions to configure and control Tx and Rx DMA functionality. Primarily intended
 *          for use with external DMA, i.e. defining an external PRB that can be located for example
 *          in another NUMA node or in graphics card.
 *          <br><br>
 *          Also Tx DMA can be accomplished from outside the driver and API library as
 *          the user logic Tx send register physical address is exposed.
 */

 /**
  *  Stop Pointer List (PL) DMA.
  *
  *  @param deviceId                     Device id.
  *  @param dmaChannel                   DMA channel number that is stopped. The value must be
  *                                      a valid user logic channel number.
  *  @param fpgaRegister                 FPGA register identifier @ref SC_FpgaRegister.
  *  @param pFpgaRegisterInfo            Pointer to FPGA register info @ref SC_FpgaRegisterInfo.
  *
  *  @return SC_ERR_SUCCESS on success, an error code otherwise.
  */
SC_Error SC_GetFpgaRegisterInfo(SC_DeviceId deviceId, uint16_t dmaChannel, SC_FpgaRegister fpgaRegister, SC_FpgaRegisterInfo * pFpgaRegisterInfo);


/** @} DMA_Setup */


/** **************************************************************************
 * @defgroup InternalFunctions Internal functions used by internal Silicom tools. Not to be used in user or production code.
 * @{ SC_packet
 * @brief Functions for accessing incoming packets directly.
 *
 * @note The functions and definitions in this section are only used when
 *       the receive buffer is handled in user space.
 */

/** @cond PRIVATE_CODE */

/**
 *  Get the raw pointer to the next packet
 *
 *  @brief  This function traverses through the packet ring buffer (PRB) from one packet
 *          to the next just by using packet header information and regardless of
 *          whether packets have been marked already read or not. Requires valid
 *          packet data in the packet ring buffer.
 *
 *  @param channelId            Channel id
 *  @param pPacket              Pointer to a valid packet or NULL to start from beginning of PRB.
 *
 *  @note This function is for <b>INTERNAL USE ONLY</b>. It must not be
 *        used by user space programs.
 *
 *  @return Pointer to next raw packet.
 */
const SC_Packet * SC_GetRawNextPacket(SC_ChannelId channelId, const SC_Packet * pPacket);

/**
 *  Get the last received packet physical address from PL DMA.
 *
 *  @param deviceId                 Device id
 *  @param channelNumber            Channel number (index) of the channel to access.
 *  @param pLastPacket              Pointer to a value that receives the last packet pointer.
 *
 *  @note This function is for <b>INTERNAL USE ONLY</b>. It must not be
 *        used by user space programs.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_GetPointerListDMA(SC_DeviceId deviceId, int16_t channelNumber, uint64_t * pLastPacket);

/**
 *  Set the last received packet in the PL DMA.
 *
 *  @param deviceId                 Device id
 *  @param channelNumber            Channel number (index) of the channel to access.
 *  @param lastPacket               Value to set as the last packet pointer.
 *
 *  @note This function is for <b>INTERNAL USE ONLY</b>. It must not be
 *        used by user space programs.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_SetPointerListDMA(SC_DeviceId deviceId, int16_t channelNumber, uint64_t lastPacket);

/**
 *  Dump a received packet into a file.
 *
 *  Writes out two files:
 *  - A file containing a dump of the packet pointed to by @a lastpacket
 *    parameter. This file has the extension '.dump'.
 *  - A text file containing various information and the description
 *    pointed to by @a infostr parameter. This file has the extension
 *    '.txt'.
 *
 *  The name of the file is &lt;basename&gt;:&lt;pid&gt;.ext where '.ext'
 *  is one of the extensions mentioned above.
 *
 *  @param channelId        Channel id
 *  @param baseFileName     Base of file name for the two files written.
 *  @param message          Pointer to user defined message that will be included
 *                          in the text file
 *  @param pLastpacket      Pointer to the received packet that shall be dumped
 *                          in the dump file
 *
 *  @note This function is for <b>INTERNAL USE ONLY</b>. It must not be
 *        used by user space programs.
 *
 *  @return SC_ERR_SUCCESS on success, an error code otherwise.
 */
SC_Error SC_DumpReceiveInfo(SC_ChannelId channelId, const char * baseFileName, const char * message,
                            const SC_Packet * pLastpacket);

/**
 *  Print out application and library version numbers.
 *
 *  @param  deviceId            Device id.
 *  @param  pFile               File to print to.
 *  @param  applicationName     Application name.
 *
 *  @note This function is for <b>INTERNAL USE ONLY</b>. It must not be
 *        used by user space programs.
 */
void SC_PrintVersions(SC_DeviceId deviceId, FILE * pFile, const char * applicationName);

/** @endcond PRIVATE_CODE */

/** @} InternalFunctions */

#ifdef __cplusplus
}

/* C++ definitions. */

#include <cstdarg>
#include <memory>
#include <stdexcept>
#include <string>

/** **************************************************************************
 * @addtogroup CPlusPlus
 * @{
 */

/**
 *  Optional exception thrown by the library instead of returning @ref SC_Error error code
 *  if C++ clients enable exceptions with @ref SC_SetLibraryOptions. Please note that
 *  exception support is currently an experimental feature and not recommended in production code.
 */
class SC_Exception : public std::runtime_error
{
private:

    std::string                 _fileName;      /**< Name of the file where the problem occurred. */
    int                         _lineNumber;    /**< Line number in the file where the problem occurred. */
    std::string                 _functionName;  /**< Name of the function where the problem occurred. */
    SC_Error                    _errorCode;     /**< @ref SC_Error error code that indentifies the problem.
                                                     This is the same error code that the API call would return
                                                     if exceptions were not enabled. */
    std::shared_ptr<void>       _pContext;      /**< Optional user specified context registered by @ref SC_RegisterErrorHandler or nullptr. */

    friend class SC_ExceptionFactory; /* Only place to create exceptions! */

    /**
     *  Private exception constructor. Private to allow construction
     *  only inside the API library by SC_ExceptionFactory.
     *
     *  @param  fileName        Name of the file where the problem occurred.
     *  @param  lineNumber      Line number of the file where the problem occurred.
     *  @param  functionName    Name of the function where the problem occurred.
     *  @param  errorCode       SC_Error error code related to this exception.
     *  @param  message         Message string.
     *  @param  pContext        Optional user specified context registered by @ref SC_RegisterErrorHandler or nullptr.
     */
    explicit SC_Exception(const char * fileName, int lineNumber, const char * functionName, SC_Error errorCode, const char * message, void * pContext);

public:

    /**
     *  Exception destructor.
     */
    virtual ~SC_Exception() throw(); /* Some versions of std::runtime_error have this destructor signature so we have to emulate it here. */

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
     *  Get the @ref SC_Error error code related to this exception.
     *  To get the error message call the base class what() method.
     *
     *   @return Return the error code related to the problem.
     */
    SC_Error GetErrorCode() const;

    /**
     *  Get the user specified context passed to @ref SC_RegisterErrorHandler.
     *
     *   @return Return the optional user specified context.
     */
    void * GetContext() const;
};

/** @} CPlusPlus */

#endif /* __cplusplus */


#endif /* __public_api_h__ */
