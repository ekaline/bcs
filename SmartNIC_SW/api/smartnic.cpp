/*
 ***************************************************************************
 *
 * Copyright (c) 2008-2019, Silicom Denmark A/S
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 *or without modification, are permitted provided that the
 *following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *copyright notice, this list of conditions and the
 *following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the
 *above copyright notice, this list of conditions and the
 *following disclaimer in the documentation and/or other
 *materials provided with the distribution.
 *
 * 3. Neither the name of the Silicom nor the names of its
 * contributors may be used to endorse or promote products
 *derived from this software without specific prior written
 *permission.
 *
 * 4. This software may only be redistributed and used in
 *connection with a Silicom network adapter product.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 *OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/**
 * @brief Silicom API library implementation.
 */

// --fmax-errors=5 -Werror
// -Wno-variadic-macros -fdiagnostics-show-option
// ${CMAKE_CXX_FLAGS}")

// C headers
#include <fcntl.h>
#include <unistd.h>

// C++ headers
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

// Linux headers
#include <dirent.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#if defined(_MSC_VER) || defined(__MINGW32__) ||           \
    defined(__MINGW64__) /* Microsoft or MingW compiler */
                         // TODO
#else
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <pthread.h>
#endif

// If not a release build then per default this is a DEBUG
// build
#ifndef NDEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif

#if defined(DEBUG) && (DEBUG == 1)
#define DEBUG_BUILD " (DEBUG)"
#else
#define DEBUG_BUILD ""
#endif

#include "release_version.h"

#include "CommonUtilities.h"
#include "SharedMemory.h"
#include "Statistics.h"
#include "Stopwatch.h"
#include "tinythread.h"
#include "fpgatypes.h"

#include "eka.h"

#include PRODUCT_H
#include PRODUCT_INTERNAL_H
#include PRODUCT_TOOLS_H

#ifndef nullptr
#define nullptr NULL
#endif

#define REG32_REF(registerOffset)                          \
  *reinterpret_cast<uint32_t *>(                           \
      reinterpret_cast<char *>(                            \
          reinterpret_cast<const DeviceContextBase *>(     \
              channelId->DeviceId)                         \
              ->pBar0Registers) +                          \
      (registerOffset))

#define SC_BUCKET_ACKNOWLEDGE_PENDING                      \
  0xFFFFFFFFFFFFFFFFUL ///< Bucket is waiting for send
                       ///< acknowledgement from FPGA.
#define SC_BUCKET_ACKNOWLEDGED                             \
  0x0000000000000000UL ///< Bucket  has been acknowledged by
                       ///< FPGA.

///////////////////////////////////////////////////////////////////////////////
/**
 *  C++ domain starts here. Everything here has C++ linkage
 * and name mangling!.
 */

static const char *ErrorPrologue =
    " *** "; ///< String that precedes an error message.

inline bool ReadableAddressCheck(const void *pAddress,
                                 ssize_t length) {
#ifdef DEBUG
  return ReadableAddress(pAddress, length);
#else
  UNUSED_ALWAYS(pAddress);
  UNUSED_ALWAYS(length);
  return true;
#endif
}

/**
 *  Log a message or error.
 *
 *  @param      pFile                               FILE
 * pointer, either stdout or stderr.
 *  @param      logTime                             True if
 * timestamp is to be logged, false otherwise.
 *  @param      logToFileIfFileNameIsAvailable      Logs to
 * file if file name is available if true, override file
 * logging if false. Makes it possible to override file
 * logging in some cases.
 *  @param      deviceId                            Device
 * identifier (not currently used).
 *  @param      format                              Format
 * string like in printf.
 *  @param      arguments                           Format
 * arguments list.
 */
static void BaseLog(FILE *pFile, bool logTime,
                    bool logToFileIfFileNameIsAvailable,
                    SC_DeviceId /*deviceId*/,
                    const char *format, va_list arguments) {
  std::vector<char> logMessage;
  logMessage.reserve(5000);

  vsnprintf(&logMessage[0], logMessage.capacity(), format,
            arguments);

  const char *logFileName = nullptr;
  if (logToFileIfFileNameIsAvailable) // This is a bit of a
                                      // hack but makes it
                                      // possible to
                                      // override logging to
                                      // file.
  {
    logFileName =
        SCI_LibraryOptions::LogFileNameString.c_str();
    if (logFileName != nullptr && logFileName[0] != '\0') {
      static bool openingErrorAlreadyLogged = false;

      pFile = fopen(logFileName, "aw");
      if (pFile == nullptr) {
        if (!openingErrorAlreadyLogged) {
          fprintf(
              stderr,
              "%sFailed to open log file %s, errno %d\n",
              ErrorPrologue, logFileName, errno);
          openingErrorAlreadyLogged = true;
        }
        logFileName = nullptr;
      }
    }
  }

  char timeBuffer[50];
  long int milliSeconds = 0;
  if (logTime) {
    timeval timeValue;

    // Create a timestamp:
    gettimeofday(&timeValue, NULL);
    tm *timeInfo = localtime(&timeValue.tv_sec);
    milliSeconds =
        timeValue.tv_usec / 1000; // To milliseconds
    // Round milliseconds up if necessary
    if (timeValue.tv_usec % 1000 >= 500) {
      // Round up 1 millisecond
      ++milliSeconds;
      if (milliSeconds >= 1000) {
        // Add 1 second, subtract 1000 milliseconds
        milliSeconds -= 1000;
        ++timeValue.tv_sec;
      }
    }
    // Format timestamp to second precision
    strftime(timeBuffer, sizeof(timeBuffer),
             "%Y-%m-%d %H:%M:%S", timeInfo);
  }

  static volatile bool s_loggingStart[2] = {true, true};
  const char *const LogStartString = "=== Log Start ===";
  int loggingStartIndex = logFileName == nullptr ? 0 : 1;

  if (logTime) {
    // printf("logTime = %d loggingStartIndex = %d
    // s_loggingStart[loggingStartIndex] = %d\n", logTime,
    // loggingStartIndex,
    // s_loggingStart[loggingStartIndex]);
    if (logFileName != nullptr &&
        s_loggingStart[loggingStartIndex]) {
      fprintf(pFile, "%s.%03ld: %s\n", timeBuffer,
              milliSeconds, LogStartString);
      s_loggingStart[loggingStartIndex] = false;
    }
    fprintf(pFile, "%s.%03ld: %s", timeBuffer, milliSeconds,
            &logMessage[0]);
  } else {
    // printf("logTime = %d loggingStartIndex = %d
    // s_loggingStart[loggingStartIndex] = %d\n", logTime,
    // loggingStartIndex,
    // s_loggingStart[loggingStartIndex]);
    if (logFileName != nullptr &&
        s_loggingStart[loggingStartIndex]) {
      fprintf(pFile, "%s\n", LogStartString);
      s_loggingStart[loggingStartIndex] = false;
    }
    fprintf(pFile, "%s", &logMessage[0]);
  }

  if (logFileName != nullptr) {
    fflush(pFile);
    fclose(pFile);
  }
}

/**
 *  Log a message.
 *
 *  @param      deviceId        Device identifier.
 *  @param      format          Format string like in
 * printf.
 */
void Log(SC_DeviceId deviceId, const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  BaseLog(stdout, true, true, deviceId, format, arguments);

  va_end(arguments);
}

/**
 *  Log an error.
 *
 *  @param      deviceId        Device identifier.
 *  @param      format          Format string like in
 * printf.
 */
void LogError(SC_DeviceId deviceId, const char *format,
              ...) {
  std::string errorFormat =
      std::string(ErrorPrologue) + format;

  va_list arguments;
  va_start(arguments, format);

  BaseLog(stderr, true, true, deviceId, errorFormat.c_str(),
          arguments);

  va_end(arguments);
}

/**
 *  Log a message without a timestamp.
 *
 *  @param      deviceId        Device identifier.
 *  @param      format          Format string like in
 * printf.
 */
void LogNoTime(SC_DeviceId deviceId, const char *format,
               ...) {
  va_list arguments;
  va_start(arguments, format);

  BaseLog(stdout, false, true, deviceId, format, arguments);

  va_end(arguments);
}

/**
 *  Log message to a file.
 *
 *  @param      pFile           Pointer to FILE, usually
 * stdout or stderr.
 *  @param      deviceId        Device identifier.
 *  @param      format          Format string like in
 * printf.
 */
void LogToFile(FILE *pFile, SC_DeviceId deviceId,
               const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  BaseLog(pFile, true, false, deviceId, format, arguments);

  va_end(arguments);
}

/**
 *  Log an error to a file.
 *
 *  @param      pFile           Pointer to FILE, usually
 * stdout or stderr.
 *  @param      deviceId        Device identifier.
 *  @param      format          Format string like in
 * printf.
 */
void LogErrorToFile(FILE *pFile, SC_DeviceId deviceId,
                    const char *format, ...) {
  std::string errorFormat =
      std::string(ErrorPrologue) + format;

  va_list arguments;
  va_start(arguments, format);

  BaseLog(pFile, true, false, deviceId, errorFormat.c_str(),
          arguments);

  va_end(arguments);
}

/**
 *  Log message to a file without timestamp.
 *
 *  @param      pFile           Pointer to FILE, usually
 * stdout or stderr.
 *  @param      deviceId        Device identifier.
 *  @param      format          Format string like in
 * printf.
 */
void LogToFileNoTime(FILE *pFile, SC_DeviceId deviceId,
                     const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  BaseLog(pFile, false, false, deviceId, format, arguments);

  va_end(arguments);
}

/**
 *  Flags for various resource bookings done by Tx flow
 * control.
 */
enum SendBookings {
  SEND_BOOKING_NONE = 0x000,  ///< No bookings taken
  SEND_BOOKING_TAKEN = 0x001, ///< A booking was taken
  SEND_BOOKING_CANCELLED =
      0x002, ///< All bookings were cancelled

  SEND_BOOKING_SINGLE_PACKET =
      0x004, ///< Booking was taken by single packet send (8
             ///< bytes after payload used for flow control)
  SEND_BOOKING_SW_TD_DMA_FIFO =
      0x010, ///< Booking was taken by software Tx DMA FIFO
             ///< flow control
  SEND_BOOKING_TAKEN_AND_LOCKED =
      0x020, ///< A booking and a lock was taken
  SEND_BOOKING_FW_TD_DMA_FIFO =
      0x040, ///< Booking was taken by firmware Tx DMA FIFO
             ///< flow control
  SEND_BOOKING_CONSUMED_BYTES =
      0x080, ///< Booking was taken by consumed bytes flow
             ///< control
  SEND_BOOKING_TCP_WINDOW =
      0x100 ///< Booking was taken by TCP window consumed
            ///< bytes flow control
};

static std::string
GetSendBookings(SendBookings sendBookings) {
  uint32_t value = sendBookings;
  uint32_t mask = 1;
  const char *separator = "";
  std::string bookings = "";

  while (value != 0) {
    bookings += separator;
    separator = " | ";

    switch (value & mask) {
    case SEND_BOOKING_NONE:
      separator = "";
      break;
    case SEND_BOOKING_TAKEN:
      bookings += "TAKEN";
      break;
    case SEND_BOOKING_CANCELLED:
      bookings += "CANCELLED";
      break;
    case SEND_BOOKING_SW_TD_DMA_FIFO:
      bookings += "SW_TD_DMA_FIFO";
      break;
    case SEND_BOOKING_SINGLE_PACKET:
      bookings += "SINGLE_PACKET";
      break;
    case SEND_BOOKING_TAKEN_AND_LOCKED:
      bookings += "TAKEN_AND_LOCKED";
      break;
    case SEND_BOOKING_FW_TD_DMA_FIFO:
      bookings += "FW_TD_DMA_FIFO";
      break;
    case SEND_BOOKING_CONSUMED_BYTES:
      bookings += "CONSUMED_BYTES";
      break;
    case SEND_BOOKING_TCP_WINDOW:
      bookings += "TCP_WINDOW";
      break;
    default: {
      bookings += "*** UNKNOWN *** ";
      std::stringstream ss;
      ss << "(value & mask = 0x" << std::hex
         << (value & mask) << ", value = 0x" << value
         << ", mask = 0x" << mask << ")";
      bookings += ss.str();
      break;
    }
    }

    value &= ~mask;
    mask <<= 1;
  }

  return bookings;
}

enum FlowControlOptions {
  FLOW_CONTROL_NONE = 0x00,
  FLOW_CONTROL_WAIT_FOR_PIPE_FREE = 0x01,
  FLOW_CONTROL_SW_TX_DMA_FIFO = 0x02,
  FLOW_CONTROL_FW_TX_DMA_FIFO = 0x04,
  FLOW_CONTROL_CONSUMED_BYTES = 0x08,

  FLOW_CONTROL_SINGLE_PACKET =
      /*FLOW_CONTROL_FW_TX_DMA_FIFO |*/
  FLOW_CONTROL_CONSUMED_BYTES, ///< Flow control with single
                               ///< packet
  FLOW_CONTROL_APPEND_LIST =
      FLOW_CONTROL_CONSUMED_BYTES ///< Only use consumed
                                  ///< bytes flow control
                                  ///< while appending to a
                                  ///< list
};

static std::string GetFlowControlOptions(
    FlowControlOptions flowControlOptions) {
  uint32_t value = flowControlOptions;
  uint32_t mask = 1;
  const char *separator = "";
  std::string options = "";

  while (value != 0) {
    options += separator;
    separator = " | ";

    switch (value & mask) {
    case FLOW_CONTROL_NONE:
      separator = "";
      break;
    case FLOW_CONTROL_WAIT_FOR_PIPE_FREE:
      options += "WAIT_FOR_PIPE_FREE";
      break;
    case FLOW_CONTROL_SW_TX_DMA_FIFO:
      options += "SW_TX_DMA_FIFO";
      break;
    // case FLOW_CONTROL_SINGLE_PACKET:
    // case FLOW_CONTROL_APPEND_LIST:
    case FLOW_CONTROL_FW_TX_DMA_FIFO:
      options += "FW_TX_DMA_FIFO";
      break;
    case FLOW_CONTROL_CONSUMED_BYTES:
      options += "CONSUMED_BYTES";
      break;
    default: {
      options += "*** UNKNOWN *** ";
      std::stringstream ss;
      ss << "(value & mask = 0x" << std::hex
         << (value & mask) << ", value = 0x" << value
         << ", mask = 0x" << mask << ")";
      options += ss.str();
      break;
    }
    }

    value &= ~mask;
    mask <<= 1;
  }

  return options;
}

std::string
SCI_GetSendReasonString(SCI_SendReason sendReason) {
  if (sendReason == SCI_SEND_REASON_NONE) {
    return "NONE";
  }

  uint32_t mask = 1;
  uint32_t value = sendReason;
  const char *separator = "";
  std::string sendReasonString = "";

  while (value != 0) {
    sendReasonString += separator;
    separator = " | ";

    switch (value & mask) {
    case SCI_SEND_REASON_NONE:
      separator = "";
      break;
    case SCI_SEND_REASON_NORMAL_FIFO:
      sendReasonString += "NORMAL_FIFO: ";
      separator = "";
      break;
    case SCI_SEND_REASON_PRIORITY_FIFO:
      sendReasonString += "PRIORITY_FIFO: ";
      separator = "";
      break;
    case SCI_SEND_REASON_FW_FIFO_SUCCESS:
      sendReasonString += "FW_FIFO_SUCCESS";
      break;
    case SCI_SEND_REASON_FW_FIFO_FULL:
      sendReasonString += "FW_FIFO_FULL";
      break;
    case SCI_SEND_REASON_SW_FIFO_SUCCESS:
      sendReasonString += "SW_FIFO_SUCCESS";
      break;
    case SCI_SEND_REASON_SW_FIFO_FULL:
      sendReasonString += "SW_FIFO_FULL";
      break;
    case SCI_SEND_REASON_CONSUMER_SUCCESS:
      sendReasonString += "CONSUMER_SUCCESS";
      break;
    case SCI_SEND_REASON_CONSUMER_FULL:
      sendReasonString += "CONSUMER_FULL";
      break;
    case SCI_SEND_REASON_TCP_WINDOW_SUCCESS:
      sendReasonString += "TCP_WINDOW_SUCCESS";
      break;
    case SCI_SEND_REASON_TCP_WINDOW_FULL:
      sendReasonString += "TCP_WINDOW_FULL";
      break;
    default: {
      sendReasonString += "*** UNKNOWN *** ";
      std::stringstream ss;
      ss << "(value & mask = 0x" << std::hex
         << (value & mask) << ", value = 0x" << value
         << ", mask = 0x" << mask << ")";
      sendReasonString += ss.str();
      break;
    }
    }

    value &= ~mask;
    mask <<= 1;
  }

  return sendReasonString;
}

DeviceContextBase::DeviceContextBase(int fileDescriptor)
    : CommonContextBase(fileDescriptor), LogMask(LOG_NONE),
      pStatusDma(nullptr), InternalDeviceOptions(),
      InternalCardInfo(SC_NO_NETWORK_INTERFACES,
                       SC_MAX_CHANNELS),
      InternalUserLogicInterruptOptions(), TxCapable(false),
      pBar0Registers(nullptr), pUserLogicRegisters(nullptr),
      pPointerListDMA(nullptr), Channels(),
      MaxNormalFifoConcurrency(0),
      MaxPriorityFifoConcurrency(0),
      pTxDmaRequestFifoFillLevelsRegister(nullptr),
      TcpTxConsumedBytesFlowControl() {}

DeviceContextBase::~DeviceContextBase() {
  // Initialize contents to zero to make this instance
  // unusable.

  LogMask = LOG_NONE;
  pStatusDma = nullptr;
  // DeviceOptions  zeroed by DeviceOptions destructor
  // CardInfo zeroed by SCI_CardInfo destructor
  //  UserLogicInterruptOptions zeroed by
  //  SCI_UserLogicInterruptOptions destructor
  TxCapable = false;
  pBar0Registers = nullptr;
  pUserLogicRegisters = nullptr;
  pPointerListDMA = nullptr;
  memset(Channels, 0, sizeof(Channels));
  MaxNormalFifoConcurrency = 0;
  MaxPriorityFifoConcurrency = 0;
  pTxDmaRequestFifoFillLevelsRegister = nullptr;
  memset(&TcpTxConsumedBytesFlowControl, 0,
         sizeof(TcpTxConsumedBytesFlowControl));
}

SC_DeviceContext::SC_DeviceContext(const char *deviceName,
                                   int fileDescriptor)
    : DeviceContextBase(fileDescriptor),
      DeviceName(deviceName), ChannelsLock(),
      TcpConsumedBytesLock(), SharedMemoryLogStream(),
      Arp(this), InternalStatusInfo(
                     SC_MAX_NUMBER_OF_NETWORK_INTERFACES,
                     SC_MAX_TCP_CHANNELS, SC_MAX_CHANNELS),
      InternalPacketStatistics(
          SC_MAX_NUMBER_OF_NETWORK_INTERFACES) {
  SC_LockInit(ChannelsLock, "SC_DeviceContext", nullptr);
  assert(!SC_IsLocked(ChannelsLock));
  SC_LockInit(TcpConsumedBytesLock, "SC_DeviceContext",
              nullptr);
  assert(!SC_IsLocked(TcpConsumedBytesLock));

  SetConstructorErrorCode(SC_GetDeviceOptions(
      nullptr, &InternalDeviceOptions.DeviceOptions));

#ifdef DEBUG
  //_logMask |= LOG_SETUP;
  //_logMask |= LOG_TX_FLOW_CONTROL;
  //_logMask |= LOG_TCP_FLOW_CONTROL;
  //_logMask |= LOG_TX_PIPE_FULL;
  //_logMask |= LOG_TCP_FLOW_CONTROL_LOCKING;
  //_logMask |= LOG_TX_DMA;
  //_logMask |= LOG_TX | LOG_BUCKET | LOG_DETAIL;
  //_logMask |= LOG_TX_COUNTER_SWAP_AROUND;
  //_logMask |= LOG_SHARED_MEMORY;
  // LogMask |= LOG_ARP;
#endif
}

SC_DeviceContext::~SC_DeviceContext() {
  SC_LockDestroy(TcpConsumedBytesLock, nullptr);
  SC_LockDestroy(ChannelsLock, nullptr);
}

ChannelContextBase::ChannelContextBase(int fileDescriptor)
    : CommonContextBase(fileDescriptor), DeviceId(nullptr),
      DmaChannelNumber(FIND_FREE_DMA_CHANNEL),
      ChannelType(SC_DMA_CHANNEL_TYPE_NONE),
      NetworkInterface(255), InternalChannelOptions(),
      InternalConnectOptions(), InternalListenOptions(),
      InternalIgmpOptions(), pTotalMemory(nullptr),
      PrbSize(0), pPRB(nullptr), PrbPhysicalAddress(0),
      PrbFpgaStartAddress(0), pReadMarkerRegister(nullptr),
      LastPacketPlDmaAddress(SC_INVALID_PLDMA_ADDRESS),
      pPointerListDMA(nullptr), PreviousPointerListDMA(),
      Buckets(), UsedBucketsCount(0),
      NextFreeBucketIndex(0), DefaultSendOptions(),
      TxDmaLists(), pCurrentTxDmaList(nullptr),
      TxDmaListsMappedSize(0), NumberOfTxDmaLists(0),
      TxDmaListSize(0), FreeTxDmaSearchStartIndex(0),
      FreeTxDmaListWaitTime(0),
      pTcbSndUnaAndConsumedBytesRegister(nullptr),
      TxDmaNormalFifoFlowControl(),
      TxDmaPriorityFifoFlowControl(),
      pTxConsumedBytesFlowControl(nullptr),
      TxConsumedBytesFlowControl(),
      pTxWindowFlowControl(nullptr), TxWindowFlowControl(),
      TxFlowControlStatistics(),
      ListBucketSequenceNumber(0), SentBucketsCount(0) {
  for (std::size_t i = 0;
       i < ARRAY_SIZE(PreviousPointerListDMA.lastPacket);
       ++i) {
    PreviousPointerListDMA.lastPacket[i] =
        0xDEADBEEFDEADBEEF;
  }
}

ChannelContextBase::~ChannelContextBase() {
  // Initialize contents to zero make this instance
  // unusable.

  DeviceId = nullptr;
  DmaChannelNumber = FIND_FREE_DMA_CHANNEL;
  ChannelType = SC_DMA_CHANNEL_TYPE_NONE;
  NetworkInterface = 255;
  // ChannelOptions is zero filled by destructor
  // ConnectOptions is zero filled by destructor
  // ListenOptions is zero filled by destructor
  // IgmpOptions is zero filled by destructor
  pTotalMemory = nullptr;
  PrbSize = 0;
  pPRB = nullptr;
  PrbPhysicalAddress = 0;
  PrbFpgaStartAddress = 0;
  pReadMarkerRegister = nullptr;
  LastPacketPlDmaAddress = SC_INVALID_PLDMA_ADDRESS;
  pPointerListDMA = nullptr;
  memset(&PreviousPointerListDMA, 0,
         sizeof(PreviousPointerListDMA));
  UsedBucketsCount = 0;
  NextFreeBucketIndex = 0;
  DefaultSendOptions = SC_SEND_NONE;
  memset(TxDmaLists, 0, sizeof(TxDmaLists));
  pCurrentTxDmaList = nullptr;
  TxDmaListsMappedSize = 0;
  NumberOfTxDmaLists = 0;
  TxDmaListSize = 0;
  FreeTxDmaSearchStartIndex = 0;
  FreeTxDmaListWaitTime = 0;
  pTcbSndUnaAndConsumedBytesRegister = nullptr;
  memset(&TxDmaNormalFifoFlowControl, 0,
         sizeof(TxDmaNormalFifoFlowControl));
  memset(&TxDmaPriorityFifoFlowControl, 0,
         sizeof(TxDmaPriorityFifoFlowControl));
  pTxConsumedBytesFlowControl = nullptr;
  memset(&TxConsumedBytesFlowControl, 0,
         sizeof(TxConsumedBytesFlowControl));
  pTxWindowFlowControl = nullptr;
  memset(&TxWindowFlowControl, 0,
         sizeof(TxWindowFlowControl));
  memset(&TxFlowControlStatistics, 0,
         sizeof(TxFlowControlStatistics));
  ListBucketSequenceNumber = 0;
  SentBucketsCount = 0;
}

/**
 *  Construct a channel context.
 */
SC_ChannelContext::SC_ChannelContext(int fileDescriptor)
    : ChannelContextBase(fileDescriptor), InterfaceName(""),
      RemoteAddress(""), RemotePort(0),
      ListenRemoteIpAddress("")
#if SUPPORT_TX_FLOW_CONTROL_STATISTICS == 1
      ,
      AllStatistics(4), FlowControlTime(AllStatistics[0]),
      SendPacketTime(AllStatistics[1]),
      SendBucketTime(AllStatistics[2]),
      GetNextPacketTime(AllStatistics[3]),
      PreviousSendBucketTime(0),
      PreviousGetNextPacketTime(0)
#endif
{
  COLLECT_STATISTICS(
      FlowControlTime.Name = "FlowControl";
      SendPacketTime.Name = "SendPacket ";
      SendBucketTime.Name = nameof(SC_SendBucket);
      GetNextPacketTime.Name = nameof(SC_GetNextPacket););
}

/**
 *  Cleanup and destroy a channel context.
 */
SC_ChannelContext::~SC_ChannelContext() {}

/**
 *   Data about available network interfaces.
 */
struct NetworkInterface {
  std::string Name; ///< Name of network interface
  unsigned int
      Flags; ///< SIOCGIFFLAGS flags:
             ///< http://man7.org/linux/man-pages/man7/netdevice.7.html
  uint32_t IpAddress; ///< IP address of the interface
  uint32_t IpNetMask; ///< IP net mask of the interface
  uint32_t BroadcastIpAddress; ///< Broadcast IP address

  explicit NetworkInterface()
      : Name(), Flags(0), IpAddress(0), IpNetMask(0),
        BroadcastIpAddress(0) {}
};

#define gettid() long(syscall(224))

/**
 *  Enumerate all available network interfaces.
 *
 *  For reference see for example
 * https://stackoverflow.com/questions/427517/finding-an-interface-name-from-an-ip-address
 */
SC_Error
EnumerateNetworkInterfaces(const SC_DeviceId deviceId) {
  UNUSED_ALWAYS(deviceId);

  std::vector<NetworkInterface> networkInterfaces;
  SC_Error errorCode = SC_ERR_SUCCESS;
  ifaddrs *pIfAddress = nullptr;

  int rc = getifaddrs(
      &pIfAddress); // http://man7.org/linux/man-pages/man3/getifaddrs.3.html
  if (rc == 0) {
    for (ifaddrs *pAddress = pIfAddress;
         pAddress != nullptr;
         pAddress = pAddress->ifa_next) {
      NetworkInterface networkInterface;
      networkInterface.Name = pAddress->ifa_name;
      networkInterface.Flags = pAddress->ifa_flags;

      if (LOGANY(deviceId, LOG_SETUP)) {
        Log(nullptr,
            "%10s    0x%08x  ifa_addr = %p ifa_netmask = "
            "%p\n",
            networkInterface.Name.c_str(),
            networkInterface.Flags,
            static_cast<void *>(pAddress->ifa_addr),
            static_cast<void *>(pAddress->ifa_netmask));
      }
    }
  } else {
    errorCode = CallErrorHandler(
        SC_ERR_FAILED_TO_ENUMERATE_NETWORK_INTERFACES,
        "failed to enumerate all network interfaces. errno "
        "= %d",
        errno);
  }

  if (pIfAddress != nullptr) {
    freeifaddrs(pIfAddress);
  }

  return errorCode;
}

#if 0
static void ResetConsumedBytes(SC_ChannelId channelId)
{
    // Just writing to this address will zero the consumed bytes but not touch TcbSndUna:
    *channelId->pTcbSndUnaAndConsumedBytesRegister = 0;
}
#endif

/**
 *  Read consumed bytes value(s) from register. TCP channel
 * counter includes both a common 16KB buffer and channel
 * specific TCP windows counter.
 *
 *  @param  channelId       Channel identifier.
 *  @param  consumedBytes   16KB buffer consumed bytes.
 *  @param  tcbSndUna       TCP window consumed bytes AKA
 * TcbSndUna.
 */
static inline void
ReadConsumedBytesAndTcbSndUna(SC_ChannelId channelId,
                              OUT uint32_t &consumedBytes,
                              OUT uint32_t &tcbSndUna) {
  uint64_t registerValue =
      *channelId->pTcbSndUnaAndConsumedBytesRegister;
  consumedBytes =
      uint32_t(registerValue); // Lower 32 bits is 16KB
                               // buffer consumed bytes
  tcbSndUna = uint32_t(registerValue >>
                       32); // Upper 32 bits is TCP window
                            // consumed bytes (TcbSndUna)
}

void SCI_ReadConsumedBytesAndTcbSndUna(
    SC_ChannelId channelId, OUT uint32_t &consumedBytes,
    OUT uint32_t &tcbSndUna) {
  ReadConsumedBytesAndTcbSndUna(channelId, consumedBytes,
                                tcbSndUna);
}

SharedMemory<SC_TxConsumedBytesFlowControl,
             SC_ConsumedBytesLock>
    g_EtlmFlowControlSharedMemory;

#define GetBucketsDump(channelId, context)                 \
  _GetBucketsDump(channelId, context, __FILE__, __LINE__)
static std::string _GetBucketsDump(SC_ChannelId channelId,
                                   const char *context,
                                   const char *fileName,
                                   int lineNumber);

#if SANITY_CHECKS == 1

#define SanityAssert(logical_expression, fileName,         \
                     lineNumber, functionName)             \
  do {                                                     \
    if (!(logical_expression)) {                           \
      fprintf(stderr,                                      \
              "Sanity assertion '%s' failed at %s#%d "     \
              "from %s#%d in function %s\n",               \
              #logical_expression, __FILE__, __LINE__,     \
              functionName, lineNumber, fileName);         \
      exit(123);                                           \
    }                                                      \
  } while (false)

/**
 *  Do some sanity check of channel data structures in a
 * DEBUG build.
 */
static void _SanityCheck(SC_ChannelId channelId,
                         const char *fileName,
                         int lineNumber,
                         const char *functionName) {
  SanityAssert(channelId != nullptr, fileName, lineNumber,
               functionName);
  if (channelId->DmaChannelNumber !=
          FIND_FREE_DMA_CHANNEL &&
      (channelId->DmaChannelNumber < 0 ||
       channelId->DmaChannelNumber >= SC_MAX_CHANNELS)) {
    Log(channelId->DeviceId,
        " *** SANITY ERROR: channelId->channelNumber(%u) "
        ">= SC_MAX_CHANNELS(%u) at line %d\n",
        channelId->DmaChannelNumber, SC_MAX_CHANNELS,
        lineNumber);
    SanityAssert(channelId->DmaChannelNumber <
                     SC_MAX_CHANNELS,
                 fileName, lineNumber, functionName);
  }
  SanityAssert(channelId->pTotalMemory !=
                   reinterpret_cast<void *>(~0UL),
               fileName, lineNumber, functionName);
  SanityAssert(
      (reinterpret_cast<uint64_t>(channelId->pTotalMemory) &
       0xFFF) == 0,
      fileName, lineNumber, functionName);
  SanityAssert(channelId->FileDescriptor >= 0 ||
                   channelId->FileDescriptor == -1,
               fileName, lineNumber, functionName);
}

/**
 *
 */
static void _SanityCheck(SC_DeviceId deviceId,
                         const char *fileName,
                         int lineNumber,
                         const char *functionName) {
  if (deviceId == reinterpret_cast<SC_DeviceId>(1)) {
    return;
  }

  SanityAssert(deviceId != nullptr, fileName, lineNumber,
               functionName);
  // TODO: do device context sanity check in DEBUG builds
  UNUSED_ALWAYS(fileName);
  UNUSED_ALWAYS(lineNumber);
  UNUSED_ALWAYS(functionName);
}

#define SanityCheck(contextId)                             \
  _SanityCheck(contextId, __FILE__, __LINE__, __func__)

class DumpTxDmaList {
  NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(DumpTxDmaList);

public:
  SC_ChannelId _channelId;
  bool _shortForm;

  static void Dump(FILE *pFile, SC_ChannelId channelId,
                   bool shortForm) {
    if (shortForm) {
      for (uint32_t i = 0;
           i < channelId->NumberOfTxDmaLists; ++i) {
        fprintf(pFile, "Tx%02u ", i);
      }
      fprintf(pFile, "\n");

      for (uint32_t i = 0;
           i < channelId->NumberOfTxDmaLists; ++i) {
        uint64_t firstListElement =
            *(channelId->TxDmaLists[i].pUserSpaceStart);
        fprintf(pFile, "%s ",
                firstListElement != 0 ? "WAIT" : "ACK ");
      }
      fprintf(pFile, "\n");
    } else {
      fprintf(pFile,
              "Tx DMA lists (pCurrentTxDmaList %p):\n",
              static_cast<void *>(
                  channelId->pCurrentTxDmaList));
      fprintf(pFile, " #  UserStart      KernelStart "
                     "Payload Length Flushed Ack\n");
      for (uint32_t i = 0;
           i < channelId->NumberOfTxDmaLists; ++i) {
        const TxDmaList &list = channelId->TxDmaLists[i];
        fprintf(
            pFile,
            "%2u: %p 0x%lx %8u %6u %7s %3s %s   %s\n", i,
            static_cast<volatile void *>(
                list.pUserSpaceStart),
            list.KernelSpaceStart,
            list.CumulatedPayloadLength, list.ListLength,
            list.ListHasBeenFlushed ? "Yes" : "No ",
            *list.pUserSpaceStart == 0 ? "Yes" : " No",
            channelId->pCurrentTxDmaList == &list
                ? "<-- current"
                : "",
            channelId->FreeTxDmaSearchStartIndex == i
                ? "  <-- next search start"
                : "");
      }
    }
  }

  DumpTxDmaList(SC_ChannelId channelId, bool shortForm)
      : _channelId(channelId), _shortForm(shortForm) {}
  ~DumpTxDmaList() { Dump(stderr, _channelId, _shortForm); }
};

static void Assert(SC_ChannelId channelId,
                   const SC_Bucket &bucket,
                   const char *context, bool expression,
                   const char *fileName, int lineNumber,
                   int assertLineNumber, const char *format,
                   ...) {
  SanityCheck(channelId);

  if (!expression) {
    char message[1000];
    va_list(arguments);
    va_start(arguments, format);
    vsprintf(message, format, arguments);
    va_end(arguments);
    // LogError(nullptr, "%s from %s for bucket %u failed at
    // line %s#%d(%d): %s\n", __func__, context,
    // bucket.BucketNumber, fileName, lineNumber,
    // assertLineNumber, message);
    fprintf(stderr,
            "%s from %s for bucket %u failed at line "
            "%s#%d(%d): %s\n",
            __func__, context, bucket.BucketNumber,
            fileName, lineNumber, assertLineNumber,
            message);

    std::cout << _GetBucketsDump(channelId, __func__,
                                 fileName, lineNumber)
              << std::flush;
  }
  assert(expression);
}

/**
 *  Do some sanity check of single bucket data in a DEBUG
 * build.
 */
static void _BucketSanityCheck(SC_ChannelId channelId,
                               const SC_Bucket &bucket,
                               const char *context,
                               const char *fileName,
                               int lineNumber) {
  SanityCheck(channelId);

  Assert(channelId, bucket, context,
         bucket.BucketPhysicalAddress != 0, fileName,
         lineNumber, __LINE__,
         "bucket._bucketPhysicalAddressInDriver=0x%lx",
         bucket.BucketPhysicalAddress);
  Assert(channelId, bucket, context,
         bucket.pUserSpaceBucket != nullptr, fileName,
         lineNumber, __LINE__,
         "bucket._pUserSpaceBucket=%p",
         bucket.pUserSpaceBucket);

  if (bucket.BucketIsUsed ||
      ApiLibraryStaticInitializer::GetConstInstance()
          .BucketViolationsAllowed) {
    if (bucket.SendCount > 0) {
      if (bucket.PayloadLength > 0 &&
          bucket.pTxDmaFifoFlowControl == nullptr &&
          bucket.pTxDmaAcknowledgement != nullptr) {
        // TODO: forbid this state!
        return; // Special case allowed just for ul_dma_loop
                // and ul_dma_perf, they misuse the API
      }

      Assert(channelId, bucket, context,
             bucket.PayloadLength > 0, fileName, lineNumber,
             __LINE__, "bucket._payloadLength=%u",
             bucket.PayloadLength);
      Assert(channelId, bucket, context,
             bucket.pTxDmaFifoFlowControl != nullptr,
             fileName, lineNumber, __LINE__,
             "bucket._pTxDmaFifoFlowControl=%p",
             bucket.pTxDmaFifoFlowControl);
      Assert(channelId, bucket, context,
             bucket.pTxDmaAcknowledgement != nullptr,
             fileName, lineNumber, __LINE__,
             "bucket._pTxDmaAcknowledgement=%p",
             bucket.pTxDmaAcknowledgement);
    }
    return;
  }

  // Bucket is unused.
  Assert(channelId, bucket, context, bucket.SendCount == 0,
         fileName, lineNumber, __LINE__,
         "bucket._sendCount=%u", bucket.SendCount);
  Assert(channelId, bucket, context,
         bucket.PayloadLength == 0, fileName, lineNumber,
         __LINE__, "bucket._payloadLength=%u",
         bucket.PayloadLength);
  Assert(channelId, bucket, context,
         bucket.pTxDmaFifoFlowControl == nullptr, fileName,
         lineNumber, __LINE__, "fillLevel/Limit=%u/%u",
         bucket.pTxDmaFifoFlowControl != nullptr
             ? bucket.pTxDmaFifoFlowControl->FillLevel
             : 0,
         bucket.pTxDmaFifoFlowControl != nullptr
             ? bucket.pTxDmaFifoFlowControl->FillLevelLimit
             : 0);
  Assert(channelId, bucket, context,
         bucket.pTxDmaAcknowledgement == nullptr, fileName,
         lineNumber, __LINE__,
         "*bucket._pTxDmaAcknowledgement(%p)=0x%lx",
         bucket.pTxDmaAcknowledgement,
         bucket.pTxDmaAcknowledgement != nullptr
             ? *bucket.pTxDmaAcknowledgement
             : uint64_t(-1));
}

/**
 *  Do some sanity check of buckets data structures in a
 * DEBUG build.
 */
static void _BucketsSanityCheck(SC_ChannelId channelId,
                                const char *context,
                                const char *fileName,
                                int lineNumber) {
  SanityCheck(channelId);

  if (ApiLibraryStaticInitializer::GetConstInstance()
          .LibraryOptions.LibraryOptions
          .EnforceBucketsSanity) {
    for (std::size_t i = 0;
         i < ARRAY_SIZE(channelId->Buckets); ++i) {
      const SC_Bucket &bucket = channelId->Buckets[i];

      _BucketSanityCheck(channelId, bucket, context,
                         fileName, lineNumber);
    }
  }
}

#define BucketsSanityCheck(channelId, context)             \
  _BucketsSanityCheck(channelId, context, __FILE__,        \
                      __LINE__)
#define BucketSanityCheck(bucket, context)                 \
  _BucketSanityCheck(bucket, context, __FILE__, __LINE__)
#else
/**
 *  No sanity checks of device and channel data structures
 * in a RELEASE build.
 */
#define SanityCheck(contextId) UNUSED_ALWAYS(contextId)
/**
 *  No sanity checks of buckets data structures in a RELEASE
 * build.
 */
#define BucketsSanityCheck(channelId, context)
/**
 *  No sanity checks of single bucket data in a RELEASE
 * build.
 */
#define BucketSanityCheck(bucket, context)
#endif

static inline std::string FillLevel(
    const TxDmaFifoFlowControl *pTxDmaFifoFlowControl) {
  if (pTxDmaFifoFlowControl != nullptr) {
    if (!ReadableAddressCheck(
            const_cast<TxDmaFifoFlowControl *>(
                pTxDmaFifoFlowControl),
            sizeof(*pTxDmaFifoFlowControl))) {
#ifdef DEBUG
      printf(
          "pTxDmaFifoFlowControl = %pd is not writable "
          "address!\n",
          static_cast<const void *>(pTxDmaFifoFlowControl));
#endif
      return "N/W";
    }

    char buffer[20];
    sprintf(buffer, "%u/%u",
            pTxDmaFifoFlowControl->FillLevel,
            pTxDmaFifoFlowControl->FillLevelLimit);
    return buffer;
  }

  return "N/A";
}

static inline std::string TxDmaAcknowledgement(
    volatile const uint64_t *pTxDmaAcknowledgement) {
  if (pTxDmaAcknowledgement != nullptr) {
    if (!ReadableAddressCheck(
            const_cast<const uint64_t *>(
                pTxDmaAcknowledgement),
            sizeof(*pTxDmaAcknowledgement))) {
      // printf("pTxDmaAcknowledgement = %pd is not writable
      // address!\n", static_cast<volatile const void
      // *>(pTxDmaAcknowledgement));
      return "N/R";
    }
    return *pTxDmaAcknowledgement == 0 ? " Ack" : " Nak";
  }

  return " Nul";
}

#define DumpBuckets(stream, channelId, context)            \
  _DumpBuckets(stream, channelId, context, __FILE__,       \
               __LINE__)

static void _DumpBuckets(std::ostream &stream,
                         SC_ChannelId channelId,
                         const char *context,
                         const char *fileName,
                         int lineNumber) {
  const std::size_t NumberOfBuckets =
      ARRAY_SIZE(channelId->Buckets);
  stream << "=== " << context << " at " << fileName << "#"
         << lineNumber << " BUCKETS ===" << std::endl;
  stream << " # Used SndCnt Payload FillLevel       "
            "FlowControl      UserSpace         "
            "KernelSpace  DMAAckAddr        DMAAckValue Ack"
         << std::endl;
  for (std::size_t i = 0; i < NumberOfBuckets; ++i) {
    const SC_Bucket &bucket = channelId->Buckets[i];

    const char *fifo =
        bucket.pTxDmaFifoFlowControl == nullptr
            ? " "
            : (bucket.pTxDmaFifoFlowControl ==
                       &channelId
                            ->TxDmaPriorityFifoFlowControl
                   ? "P"
                   : "N");

    bool isAckAddressReadable = ReadableAddressCheck(
        const_cast<const uint64_t *>(
            bucket.pTxDmaAcknowledgement),
        sizeof(*bucket.pTxDmaAcknowledgement));
    std::stringstream ackValue;
    if (isAckAddressReadable) {
      ackValue << "0x" << std::hex
               << *bucket.pTxDmaAcknowledgement;
    } else {
      ackValue << "Not Readable!";
    }

    stream << std::setw(2) << std::dec << i << std::setw(5)
           << (bucket.BucketIsUsed ? "Used" : "Free")
           << std::setw(7) << bucket.SendCount
           << std::setw(8) << bucket.PayloadLength
           << std::setw(10)
           << FillLevel(bucket.pTxDmaFifoFlowControl)
           << std::setw(15) << bucket.pTxDmaFifoFlowControl
           << "(" << fifo << ")" << std::setw(15)
           << bucket.pUserSpaceBucket << std::setw(11)
           << std::hex << "0x"
           << bucket.BucketPhysicalAddress << std::setw(11)
           << std::hex << "0x"
           << static_cast<volatile void *>(
                  bucket.pTxDmaAcknowledgement)
           << std::setw(19) << std::hex << ackValue.str()
           << std::setw(4)
           << TxDmaAcknowledgement(
                  bucket.pTxDmaAcknowledgement)
           << (channelId->NextFreeBucketIndex == i
                   ? " <-- next free bucket"
                   : "")
           << std::endl;
  }

  stream << "[(N) = Normal FIFO, (P) = Priority FIFO]"
         << std::endl;
  stream << "[Nul = NULL pointer, Ack = Acknowledged, Nak "
            "= Not Acknowledged, N/R = Unreadable Memory]"
         << std::endl;
  stream << std::flush;

#if SANITY_CHECKS == 1
  if (std::string(context) != "Assert")
    _BucketsSanityCheck(channelId, context, fileName,
                        lineNumber);
#endif
}

static std::string _GetBucketsDump(SC_ChannelId channelId,
                                   const char *context,
                                   const char *fileName,
                                   int lineNumber) {
  std::stringstream bucketDump;
  _DumpBuckets(bucketDump, channelId, context, fileName,
               lineNumber);
  return bucketDump.str();
}

#ifdef DEBUG

uint32_t PrintSndUna(FILE *pFile, const char *context,
                     SC_ChannelId channelId) {
  uint64_t registerValue =
      *channelId->pTcbSndUnaAndConsumedBytesRegister;
  uint32_t tcbSndUna = uint32_t(
      registerValue >> 32); // Upper 32 bits is TCP window
                            // consumed bytes (TcbSndUna)
  uint32_t consumedBytes = uint32_t(registerValue);

  assert(channelId->DmaChannelNumber !=
         FIND_FREE_DMA_CHANNEL);
  uint32_t etlmConnectionId =
      uint32_t(channelId->DmaChannelNumber -
               FIRST_TCP_CHANNEL); // (0-63)

  // Select ETLM TCP channel (0-63):
  REG32_REF(ETLM_SA_TCB_ACC_CMD) = etlmConnectionId;
  uint32_t sndNxt = REG32_REF(ETLM_SA_TCB_SNDNXT);

  REG32_REF(ETLM_SA_TCB_ACC_CMD) = etlmConnectionId;
  uint32_t tcbSndUna2 = REG32_REF(ETLM_SA_TCB_SNDUNA);

  REG32_REF(ETLM_SA_TCB_ACC_CMD) = etlmConnectionId;
  uint32_t sndToSeq = REG32_REF(ETLM_SA_TCB_SNDTOSEQ);

  uint32_t dbgAssertions = REG32_REF(ETLM_DBG_ASSERTIONS);

  fprintf(pFile,
          "%s: tcbSndUna = %u, consumedBytes = %u, sndNxt "
          "= %u, sndToSeq = %u, tcbSndUna = %u, "
          "dbgAssertions = %u\n",
          context, tcbSndUna, consumedBytes, sndNxt,
          sndToSeq, tcbSndUna2, dbgAssertions);

  uint32_t val = REG32_REF(ETLM_SA_DBG_PROC_CONN);
  val &= ~(0xfffU | (1 << 14));
  val |= (etlmConnectionId & 0xfff) |
         (1 << 14); // connection id and ONE_SHOT
  REG32_REF(ETLM_SA_DBG_PROC_CONN) = val;

  return tcbSndUna;
}

#endif

#ifdef DEBUG
// #define DEBUG_STATIC_INITIALIZATIONS
#endif

// These static initializations are done in order:
const char *const SCI_LibraryOptions::DefaultLogFileName = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
    printf("SCI_LibraryOptions::DefaultLogFileName "
           "initialized\n"),
#endif
    "/tmp/tester.log");
std::string SCI_LibraryOptions::LogFileNameString = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
    printf("SCI_LibraryOptions::LogFileNameString "
           "initialized\n"),
#endif
    SCI_LibraryOptions::DefaultLogFileName);
const char *const
    SCI_LibraryOptions::DefaultSharedMemoryFileName = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
        printf("SCI_LibraryOptions::"
               "DefaultSharedMemoryFileName initialized\n"),
#endif
        "/tmp/SmartnicFlowControl");
const mode_t
    SCI_LibraryOptions::DefaultSharedMemoryPermissions = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
        printf(
            "SCI_LibraryOptions::"
            "DefaultSharedMemoryPermissions initialized\n"),
#endif
        (S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) |
            (S_IROTH | S_IWOTH)); // 0666
volatile uint64_t ApiLibraryStaticInitializer::LogMask = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
    printf("ApiLibraryStaticInitializer::LogMask "
           "initialized\n"),
#endif
    LOG_NONE);
volatile SC_Error
    ApiLibraryStaticInitializer::LastErrorCode = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
        printf("ApiLibraryStaticInitializer::LastErrorCode "
               "initialized\n"),
#endif
        SC_ERR_SUCCESS);
volatile bool ApiLibraryStaticInitializer::
    StaticInitializationInProgress = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
        printf(
            "ApiLibraryStaticInitializer::"
            "StaticInitializationInProgress initialized\n"),
#endif
        false);
int DummyStaticInitializer = (
#ifdef DEBUG_STATIC_INITIALIZATIONS
    printf("ApiLibraryStaticInitializer::GetInstance() "
           "initialized\n"),
#endif
    ApiLibraryStaticInitializer::GetInstance(), 0);

ApiLibraryStaticInitializer::ApiLibraryStaticInitializer()
    : ErrorHandlingLock(), pErrorHandler(nullptr),
      pErrorHandlerUserContext(nullptr),
      DefaultDeviceOptions(), DefaultChannelOptions(),
      DefaultConnectOptions(), DefaultListenOptions(),
      DefaultIgmpOptions(), LibraryARP(false),
      DefaultUserLogicInterruptOptions(), LibraryOptions(),
      BucketViolationsAllowed(false) {
  LibraryOptions.LibraryOptions.ThrowExceptions = false;
  LibraryOptions.LibraryOptions.WriteErrorsToStdErr = false;
  LibraryOptions.LibraryOptions.LogFileName =
      SCI_LibraryOptions::LogFileNameString.c_str();
  LibraryOptions.LibraryOptions.FpgaResetWaitInSeconds = 3;
  LibraryOptions.LibraryOptions.EnforceBucketsSanity = true;
  LibraryOptions.LibraryOptions
      .SharedMemoryFlowControlOptions.Enabled = false;
  LibraryOptions.LibraryOptions
      .SharedMemoryFlowControlOptions.FileName =
      SCI_LibraryOptions::DefaultSharedMemoryFileName;
  LibraryOptions.LibraryOptions
      .SharedMemoryFlowControlOptions.Permissions =
      SCI_LibraryOptions::DefaultSharedMemoryPermissions;
  HandleSanityCheckError(
      SanityCheckOptions(&LibraryOptions.LibraryOptions));

  DefaultDeviceOptions.DeviceOptions
      .MaxTxDmaNormalFifoConcurrency = 10;
  DefaultDeviceOptions.DeviceOptions
      .MaxTxDmaPriorityFifoConcurrency = 10;
  DefaultDeviceOptions.DeviceOptions.ArpEntryTimeout = 0;
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultDeviceOptions.DeviceOptions));

  // These Tx DMA flow control defaults give the best
  // performance if any kind of flow control is enabled.
  // Even better performance can be achieved by setting the
  // fill level limits below to 0 instead of 4 thus
  // disabling both firmware and software flow control but
  // then you have to be certain that your usage does not
  // overflow the 512 entry firmware FIFOs.
  DefaultChannelOptions.ChannelOptions
      .UseTxDmaNormalFifoFirmwareFillLevel = false;
  DefaultChannelOptions.ChannelOptions
      .TxDmaNormalFifoSoftwareFillLevelLimit = 4;
  DefaultChannelOptions.ChannelOptions
      .UseTxDmaPriorityFifoFirmwareFillLevel = false;
  DefaultChannelOptions.ChannelOptions
      .TxDmaPriorityFifoSoftwareFillLevelLimit = 4;
  DefaultChannelOptions.ChannelOptions.DoNotUseRxDMA =
      false;
  DefaultChannelOptions.ChannelOptions.UserSpaceRx = true;
  DefaultChannelOptions.ChannelOptions.UserSpaceTx = true;
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultChannelOptions.ChannelOptions));

  DefaultConnectOptions.ConnectOptions.LocalIpAddress =
      nullptr;
  DefaultConnectOptions.ConnectOptions.LocalPort = 0;
  DefaultConnectOptions.ConnectOptions.VLanTag =
      NO_VLAN_TAG;
  memset(
      DefaultConnectOptions.ConnectOptions.RemoteMacAddress,
      0,
      sizeof(DefaultConnectOptions.ConnectOptions
                 .RemoteMacAddress));
  DefaultConnectOptions.ConnectOptions.ConnectTimeout =
      30000; // 30 seconds
  DefaultConnectOptions.ConnectOptions.DisconnectTimeout =
      30000; // 30 seconds
  DefaultConnectOptions.ConnectOptions
      .AcquireMacAddressViaArpTimeout = 10000; // 10 seconds
  DefaultConnectOptions.ConnectOptions.ScanLinuxArpTable =
      true;

  DefaultListenOptions.ListenOptions.LocalIpAddress =
      nullptr;
  DefaultListenOptions.ListenOptions.VLanTag = NO_VLAN_TAG;
  DefaultListenOptions.ListenOptions.ListenTimeout =
      SC_Timeout::SC_TIMEOUT_INFINITE; // -1 = listen
                                       // forever

  DefaultIgmpOptions.IgmpOptions.VLanTag = NO_VLAN_TAG;
  DefaultIgmpOptions.IgmpOptions.EnableMulticastBypass =
      false;

  DefaultUserLogicInterruptOptions.UserLogicInterruptOptions
      .Pid = 0;
  DefaultUserLogicInterruptOptions.UserLogicInterruptOptions
      .Mask = SC_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS;
  DefaultUserLogicInterruptOptions.UserLogicInterruptOptions
      .SignalNumber = SIGUSR1;
  DefaultUserLogicInterruptOptions.UserLogicInterruptOptions
      .pContext = nullptr;
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultUserLogicInterruptOptions
                    .UserLogicInterruptOptions));

  StaticInitializationInProgress =
      true; // Enable while running sanity checks below

  HandleSanityCheckError(
      SanityCheckOptions(&LibraryOptions.LibraryOptions));
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultDeviceOptions.DeviceOptions));
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultChannelOptions.ChannelOptions));
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultConnectOptions.ConnectOptions));
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultListenOptions.ListenOptions));
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultIgmpOptions.IgmpOptions));
  HandleSanityCheckError(SanityCheckOptions(
      nullptr, &DefaultUserLogicInterruptOptions
                    .UserLogicInterruptOptions));

#ifdef DEBUG_STATIC_INITIALIZATIONS
  printf("%s library static initialized!\n", PRODUCT);
#if 0
    Log(nullptr, "%s library static initialized\n", PRODUCT);
    printf("hello\n");
    Log(nullptr, "  sizeof(bool) = %lu, sizeof(int) = %lu, sizeof(long) = %lu, sizeof(long long) = %lu\n", sizeof(bool), sizeof(int), sizeof(long), sizeof(long long));
    printf("there\n");
    LogError(nullptr, "%s library static initialization!\n", PRODUCT);
    LogNoTime(nullptr, "No Time? %s library static initialization!\n", PRODUCT);
#endif // 0
#endif // DEBUG_STATIC_INITIALIZATIONS

  // Restore default value of false.
  StaticInitializationInProgress = false;
}

ApiLibraryStaticInitializer::
    ~ApiLibraryStaticInitializer() {
#ifdef DEBUG_STATIC_INITIALIZATIONS
  Log(nullptr, "%s library static cleanup\n", PRODUCT);
#endif // DEBUG_STATIC_INITIALIZATIONS
}

ApiLibraryStaticInitializer &
ApiLibraryStaticInitializer::GetInstance() {
  static ApiLibraryStaticInitializer s_StaticInitializer;

  return s_StaticInitializer;
}

void ApiLibraryStaticInitializer::_HandleSanityCheckError(
    SC_Error errorCode,
    const char *nameOfSanityCheckFunction) {
  if (errorCode != SC_ERR_SUCCESS &&
      ApiLibraryStaticInitializer::LastErrorCode ==
          SC_ERR_SUCCESS) {
    fprintf(stderr,
            "Option sanity check %s failed with error code "
            "%d\n",
            nameOfSanityCheckFunction, errorCode);
    ApiLibraryStaticInitializer::LastErrorCode = errorCode;
  }
}

#define name_of(x)                                         \
  (static_cast<void>(sizeof(typeid(x))), #x)
#define name_of_member(s, x)                               \
  (static_cast<void>(sizeof(typeid(s::x))), #x)

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_LibraryOptions *pLibraryOptions) {
  if (pLibraryOptions->StructSize !=
      sizeof(SC_LibraryOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_LibraryOptions),
        name_of_member(SC_LibraryOptions, StructSize),
        pLibraryOptions->StructSize,
        sizeof(SC_LibraryOptions));
  }
  if (pLibraryOptions->SharedMemoryFlowControlOptions
          .StructSize !=
      sizeof(SC_SharedMemoryFlowControlOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_LibraryOptions::
                    SharedMemoryFlowControlOptions),
        name_of_member(SC_SharedMemoryFlowControlOptions,
                       StructSize),
        pLibraryOptions->SharedMemoryFlowControlOptions
            .StructSize,
        sizeof(SC_SharedMemoryFlowControlOptions));
  }
  SC_Boolean enabled =
      pLibraryOptions->SharedMemoryFlowControlOptions
          .Enabled;
  if (enabled != 1 && enabled != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_LibraryOptions::
                    SharedMemoryFlowControlOptions),
        name_of_member(SC_SharedMemoryFlowControlOptions,
                       Enabled),
        enabled);
  }
  if (pLibraryOptions->SharedMemoryFlowControlOptions
              .FileName == nullptr ||
      pLibraryOptions->SharedMemoryFlowControlOptions
              .FileName[0] == '\0') {
    // Use default file name
    pLibraryOptions->SharedMemoryFlowControlOptions
        .FileName =
        ApiLibraryStaticInitializer::GetConstInstance()
            .LibraryOptions.LibraryOptions
            .SharedMemoryFlowControlOptions.FileName;
  }
  if (pLibraryOptions->SharedMemoryFlowControlOptions
          .Permissions == 0) {
    // Use default file permissions
    pLibraryOptions->SharedMemoryFlowControlOptions
        .Permissions =
        SCI_LibraryOptions::DefaultSharedMemoryPermissions;
  }
  if (pLibraryOptions->SharedMemoryFlowControlOptions
          .Permissions > 0777) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of octal 0%o, value less "
        "than or equal to octal 0%o expected",
        name_of(SC_LibraryOptions::
                    SharedMemoryFlowControlOptions),
        name_of_member(SC_SharedMemoryFlowControlOptions,
                       Permissions),
        pLibraryOptions->SharedMemoryFlowControlOptions
            .Permissions,
        0777);
  }
  SC_Boolean throwExceptions =
      pLibraryOptions->ThrowExceptions;
  if (throwExceptions != 1 && throwExceptions != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_LibraryOptions),
        name_of_member(SC_LibraryOptions, ThrowExceptions),
        throwExceptions);
  }
  SC_Boolean writeErrorsToStdErr =
      pLibraryOptions->WriteErrorsToStdErr;
  if (writeErrorsToStdErr != 1 &&
      writeErrorsToStdErr != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_LibraryOptions),
        name_of_member(SC_LibraryOptions,
                       WriteErrorsToStdErr),
        writeErrorsToStdErr);
  }
  // TODO: check LogFileName;
  // TODO: check FpgaResetWaitInSeconds
  SC_Boolean enforceBucketsSanity =
      pLibraryOptions->EnforceBucketsSanity;
  if (enforceBucketsSanity != 1 &&
      enforceBucketsSanity != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_LibraryOptions),
        name_of_member(SC_LibraryOptions,
                       EnforceBucketsSanity),
        enforceBucketsSanity);
  }
  // const char *    FileName;           /**< Name of the
  // shared memory file in directory /dev/shm/. Default is
  // "SmartNicAPI". */

  return SC_ERR_SUCCESS;
}

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_DeviceId deviceId,
    SC_DeviceOptions *pDeviceOptions) {
  if (pDeviceOptions->StructSize !=
      sizeof(SC_DeviceOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_DeviceOptions),
        name_of_member(SC_DeviceOptions, StructSize),
        pDeviceOptions->StructSize,
        sizeof(SC_DeviceOptions));
  }
  if (deviceId == nullptr) {
    // Device is not known; do a more generic check:

    if (pDeviceOptions->MaxTxDmaNormalFifoConcurrency >=
        MAX_FIFO_ENTRIES) {
      return CallErrorHandlerDontSetLastError(
          SC_ERR_INVALID_ARGUMENT,
          "%s::%s value %u is greater than maximum %u "
          "allowed",
          name_of(SC_DeviceOptions),
          name_of_member(SC_DeviceOptions,
                         MaxTxDmaNormalFifoConcurrency),
          pDeviceOptions->MaxTxDmaNormalFifoConcurrency,
          MAX_FIFO_ENTRIES - 1);
    }
    if (pDeviceOptions->MaxTxDmaPriorityFifoConcurrency >=
        MAX_FIFO_ENTRIES) {
      return CallErrorHandlerDontSetLastError(
          SC_ERR_INVALID_ARGUMENT,
          "%s::%s value %u is greater than maximum %u "
          "allowed",
          name_of(SC_DeviceOptions),
          name_of_member(SC_DeviceOptions,
                         MaxTxDmaPriorityFifoConcurrency),
          pDeviceOptions->MaxTxDmaPriorityFifoConcurrency,
          MAX_FIFO_ENTRIES - 1);
    }
  } else {
    // Device is known, do a more precise check:

    if (pDeviceOptions->MaxTxDmaNormalFifoConcurrency +
            deviceId->InternalCardInfo.CardInfo
                    .NumberOfNICs *
                FB_FIFO_ENTRIES_PER_CHANNEL >=
        MAX_FIFO_ENTRIES) {
      return CallErrorHandlerDontSetLastError(
          SC_ERR_INVALID_ARGUMENT,
          "%s::%s value %u (plus %u entries for %u NICs in "
          "driver) >= than FIFO size %u",
          name_of(SC_DeviceOptions),
          name_of_member(SC_DeviceOptions,
                         MaxTxDmaNormalFifoConcurrency),
          pDeviceOptions->MaxTxDmaNormalFifoConcurrency,
          deviceId->InternalCardInfo.CardInfo.NumberOfNICs *
              FB_FIFO_ENTRIES_PER_CHANNEL,
          deviceId->InternalCardInfo.CardInfo.NumberOfNICs,
          MAX_FIFO_ENTRIES);
    }
    if (pDeviceOptions->MaxTxDmaPriorityFifoConcurrency >=
        MAX_FIFO_ENTRIES) {
      return CallErrorHandlerDontSetLastError(
          SC_ERR_INVALID_ARGUMENT,
          "%s::%s value %u >= than FIFO size %u",
          name_of(SC_DeviceOptions),
          name_of_member(SC_DeviceOptions,
                         MaxTxDmaPriorityFifoConcurrency),
          pDeviceOptions->MaxTxDmaPriorityFifoConcurrency,
          MAX_FIFO_ENTRIES);
    }
  }

  // pDeviceOptions->ArpEntryTimeout; // Anything goes!

  return SC_ERR_SUCCESS;
}

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_DeviceId deviceId,
    const SC_ChannelOptions *pChannelOptions) {
  UNUSED_ALWAYS(deviceId);

  if (pChannelOptions->StructSize !=
      sizeof(SC_ChannelOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_ChannelOptions),
        name_of_member(SC_ChannelOptions, StructSize),
        pChannelOptions->StructSize,
        sizeof(SC_ChannelOptions));
  }
  int useTxDmaNormalFifoFirmwareFillLevel =
      pChannelOptions->UseTxDmaNormalFifoFirmwareFillLevel;
  if (useTxDmaNormalFifoFirmwareFillLevel != 1 &&
      useTxDmaNormalFifoFirmwareFillLevel != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_ChannelOptions),
        name_of_member(SC_ChannelOptions,
                       UseTxDmaNormalFifoFirmwareFillLevel),
        useTxDmaNormalFifoFirmwareFillLevel);
  }
  SC_Boolean useTxDmaPriorityFifoFirmwareFillLevel =
      pChannelOptions
          ->UseTxDmaPriorityFifoFirmwareFillLevel;
  if (useTxDmaPriorityFifoFirmwareFillLevel != 1 &&
      useTxDmaPriorityFifoFirmwareFillLevel != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_ChannelOptions),
        name_of_member(
            SC_ChannelOptions,
            UseTxDmaPriorityFifoFirmwareFillLevel),
        useTxDmaPriorityFifoFirmwareFillLevel);
  }
  if (pChannelOptions
          ->TxDmaPriorityFifoSoftwareFillLevelLimit >=
      MAX_FIFO_ENTRIES) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u that is not in the "
        "range [0-%u]",
        name_of(SC_ChannelOptions),
        name_of_member(
            SC_ChannelOptions,
            TxDmaPriorityFifoSoftwareFillLevelLimit),
        pChannelOptions
            ->TxDmaPriorityFifoSoftwareFillLevelLimit,
        MAX_FIFO_ENTRIES - 1);
  }
  if (pChannelOptions
          ->TxDmaNormalFifoSoftwareFillLevelLimit >=
      MAX_FIFO_ENTRIES) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u that is not in the "
        "range [0-%u]",
        name_of(SC_ChannelOptions),
        name_of_member(
            SC_ChannelOptions,
            TxDmaNormalFifoSoftwareFillLevelLimit),
        pChannelOptions
            ->UseTxDmaNormalFifoFirmwareFillLevel,
        MAX_FIFO_ENTRIES - 1);
  }

  // Anything goes for these:
  // pChannelOptions->DefaultConnectTimeout;
  // pChannelOptions->DefaultDisconnectTimeout;

  SC_Boolean doNotUseRxDMA = pChannelOptions->DoNotUseRxDMA;
  if (doNotUseRxDMA != 1 && doNotUseRxDMA != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_ChannelOptions),
        name_of_member(SC_ChannelOptions, DoNotUseRxDMA),
        doNotUseRxDMA);
  }

  return SC_ERR_SUCCESS;
}

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_DeviceId deviceId,
    SC_ConnectOptions *pConnectOptions) {
  UNUSED_ALWAYS(deviceId);

  if (pConnectOptions->StructSize !=
      sizeof(SC_ConnectOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_ConnectOptions),
        name_of_member(SC_ConnectOptions, StructSize),
        pConnectOptions->StructSize,
        sizeof(SC_ConnectOptions));
  }
  struct in_addr address;
  if (pConnectOptions->LocalIpAddress != nullptr &&
      pConnectOptions->LocalIpAddress[0] != '\0' &&
      inet_aton(pConnectOptions->LocalIpAddress,
                &address) == 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s '%s' is not null or a valid IP address, "
        "errno %d",
        name_of(SC_ConnectOptions),
        name_of_member(SC_ConnectOptions, LocalIpAddress),
        pConnectOptions->LocalIpAddress, errno);
  }
  if (pConnectOptions->VLanTag != -1 &&
      (pConnectOptions->VLanTag < 1 ||
       pConnectOptions->VLanTag > 4095)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s value %u is not -1 or between [1-4094]",
        name_of(SC_ConnectOptions),
        name_of_member(SC_ConnectOptions, VLanTag),
        pConnectOptions->VLanTag);
  }
  SC_Boolean scanLinuxArpTable =
      pConnectOptions->ScanLinuxArpTable;
  if (scanLinuxArpTable != 1 && scanLinuxArpTable != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_ConnectOptions),
        name_of_member(SC_ConnectOptions,
                       ScanLinuxArpTable),
        scanLinuxArpTable);
  }

  // Anything goes for these:
  // pConnectOptions->LocalPort
  // pConnectOptions->RemoteMacAddress
  // pConnectOptions->ConnectTimeout
  // pConnectOptions->DisconnectTimeout;
  // pConnectOptions->AcquireMacAddressViaArpTimeout;

  return SC_ERR_SUCCESS;
}

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_DeviceId deviceId,
    SC_ListenOptions *pListenOptions) {
  UNUSED_ALWAYS(deviceId);

  if (pListenOptions->StructSize !=
      sizeof(SC_ListenOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_ListenOptions),
        name_of_member(SC_ListenOptions, StructSize),
        pListenOptions->StructSize,
        sizeof(SC_ListenOptions));
  }
  struct in_addr address;
  if (pListenOptions->LocalIpAddress != nullptr &&
      pListenOptions->LocalIpAddress[0] != '\0' &&
      inet_aton(pListenOptions->LocalIpAddress, &address) ==
          0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s '%s' is not null or a valid IP address",
        name_of(SC_ListenOptions),
        name_of_member(SC_ListenOptions, LocalIpAddress),
        pListenOptions->LocalIpAddress);
  }
  if (pListenOptions->ListenTimeout !=
          SC_Timeout::SC_TIMEOUT_INFINITE &&
      (pListenOptions->ListenTimeout < 0)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s value %u is not -1 or greater than or "
        "equal to 0",
        name_of(SC_ListenOptions),
        name_of_member(SC_ListenOptions, ListenTimeout),
        pListenOptions->ListenTimeout);
  }
  if (pListenOptions->VLanTag != -1 &&
      (pListenOptions->VLanTag < 1 ||
       pListenOptions->VLanTag > 4095)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s value %u is not -1 or between [1-4094]",
        name_of(SC_ListenOptions),
        name_of_member(SC_ListenOptions, VLanTag),
        pListenOptions->VLanTag);
  }

  return SC_ERR_SUCCESS;
}

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_DeviceId deviceId, SC_IgmpOptions *pIgmpOptions) {
  UNUSED_ALWAYS(deviceId);

  if (pIgmpOptions->StructSize != sizeof(SC_IgmpOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_IgmpOptions),
        name_of_member(SC_IgmpOptions, StructSize),
        pIgmpOptions->StructSize, sizeof(SC_IgmpOptions));
  }
  if (pIgmpOptions->VLanTag != -1 &&
      (pIgmpOptions->VLanTag < 1 ||
       pIgmpOptions->VLanTag > 4095)) {
    assert(pIgmpOptions->VLanTag != -1 &&
           (pIgmpOptions->VLanTag < 1 ||
            pIgmpOptions->VLanTag > 4095));
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s value %u is not -1 or between [1-4094]",
        name_of(SC_IgmpOptions),
        name_of_member(SC_IgmpOptions, VLanTag),
        pIgmpOptions->VLanTag);
  }

  SC_Boolean enableMulticatBypass =
      pIgmpOptions->EnableMulticastBypass;
  if (enableMulticatBypass != 1 &&
      enableMulticatBypass != 0) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid boolean value of %d, true = 1 "
        "or false = 0 expected",
        name_of(SC_IgmpOptions),
        name_of_member(SC_IgmpOptions,
                       EnableMulticastBypass),
        enableMulticatBypass);
  }

  return SC_ERR_SUCCESS;
}

SC_Error ApiLibraryStaticInitializer::SanityCheckOptions(
    SC_DeviceId deviceId, SC_UserLogicInterruptOptions
                              *pUserLogicInterruptOptions) {
  UNUSED_ALWAYS(deviceId);

  if (pUserLogicInterruptOptions->StructSize !=
      sizeof(SC_UserLogicInterruptOptions)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s has invalid value of %u, %lu expected",
        name_of(SC_UserLogicInterruptOptions),
        name_of_member(SC_UserLogicInterruptOptions,
                       StructSize),
        pUserLogicInterruptOptions->StructSize,
        sizeof(SC_UserLogicInterruptOptions));
  }
  if (pUserLogicInterruptOptions->SignalNumber < 0 ||
      pUserLogicInterruptOptions->SignalNumber >=
          SIGRTMIN) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s invalid value 0x%X is not between [%d-%d]",
        name_of(SC_UserLogicInterruptOptions),
        name_of_member(SC_UserLogicInterruptOptions,
                       SignalNumber),
        pUserLogicInterruptOptions->SignalNumber, 0,
        SIGRTMIN - 1);
  }
  if (kill(pUserLogicInterruptOptions->Pid, 0) != 0 &&
      errno == ESRCH) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s process with PID %d does not exist",
        name_of(SC_UserLogicInterruptOptions),
        name_of_member(SC_UserLogicInterruptOptions, Pid),
        pUserLogicInterruptOptions->Pid);
  }
  int mask = pUserLogicInterruptOptions->Mask;
  if (mask < 0 ||
      mask > SC_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s invalid value 0x%X is not between "
        "[0x%02X-0x%02X]",
        name_of(SC_UserLogicInterruptOptions),
        name_of_member(SC_UserLogicInterruptOptions, Mask),
        pUserLogicInterruptOptions->Mask,
        SC_USER_LOGIC_INTERRUPT_MASK_NONE,
        SC_USER_LOGIC_INTERRUPT_MASK_ALL_CHANNELS);
  }
  if (pUserLogicInterruptOptions->pContext != nullptr &&
      !ReadableAddressCheck(
          pUserLogicInterruptOptions->pContext, 1)) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "%s::%s invalid value %p is not NULL or a readable "
        "memory address.",
        name_of(SC_UserLogicInterruptOptions),
        name_of_member(SC_UserLogicInterruptOptions,
                       pContext),
        pUserLogicInterruptOptions->pContext);
  }

  return SC_ERR_SUCCESS;
}

ApiLibraryThreadStaticInitializer &
ApiLibraryThreadStaticInitializer::GetThreadInstance() {
  static ApiLibraryThreadStaticInitializer
      s_ThreadStaticInitializer;

  return s_ThreadStaticInitializer;
}

volatile __thread SC_Error
    ApiLibraryThreadStaticInitializer::LastThreadErrorCode =
        SC_ERR_SUCCESS;

#ifdef DEBUG
/**
 *  Find the network interface name that connects to given
 * remote host IP address based on directly comparing all
 * found local interfaces to remote IP address after masking
 * with network mask. This method does not take into account
 * routing information.
 *
 *  @param      ipAddress   Remore host IP address in host
 * byte order.
 *
 *  @return     Return the
 */
std::string
GetInterfaceNameFromRemoteIpAddress(uint32_t ipAddress) {
  std::string interfaceName;
  struct ifaddrs *pIfAddresses = nullptr;

  int rc = getifaddrs(&pIfAddresses);
  if (rc == 0) {
    for (struct ifaddrs *pIfAddress = pIfAddresses;
         pIfAddress != NULL;
         pIfAddress = pIfAddress->ifa_next) {
      if (pIfAddress->ifa_addr &&
          (pIfAddress->ifa_flags & IFF_UP) &&
          pIfAddress->ifa_addr->sa_family == AF_INET) {
        struct sockaddr_in *pSocketAddress =
            reinterpret_cast<struct sockaddr_in *>(
                pIfAddress->ifa_addr);
        struct sockaddr_in *pMaskAddress =
            reinterpret_cast<struct sockaddr_in *>(
                pIfAddress->ifa_netmask);
        uint32_t mask = pMaskAddress->sin_addr.s_addr;
        printf("ipAddress = 0x%08x mask = 0x%08x\n",
               ipAddress, mask);

        if ((ipAddress & mask) ==
            (pSocketAddress->sin_addr.s_addr & mask)) {
          printf("%s\n", pIfAddress->ifa_name);
          interfaceName = pIfAddress->ifa_name;
          break;
        }
      }
    }
  }

  if (pIfAddresses != nullptr) {
    freeifaddrs(pIfAddresses);
  }

  return interfaceName;
}
#endif

///////////////////////////////////////////////////////////////////////////////
/**
 *  C domain starts here. Everything after this has C
 * linkage.
 */

extern "C" {
const uint32_t TxConsumedBytesBufferSize = 16 * 1024;

const SC_CompilationOptions g_ApiLibraryCompilationOptions =
    {SUPPORT_LOGGING == 1 ? true : false,
     SUPPORT_TX_FLOW_CONTROL_STATISTICS == 1 ? true : false,
     USE_SPINLOCK == 1 ? true : false};

volatile uint64_t *
SC_GetFpgaRegistersBase(SC_DeviceId deviceId) {
  SanityCheck(deviceId);

  return static_cast<volatile uint64_t *>(
      deviceId->pBar0Registers);
}

SC_Error SC_GetTxDmaFifoFillLevels(
    SC_DeviceId deviceId, uint16_t *pNormalFifoFillLevel,
    uint16_t *pPriorityFifoFillLevel) {
  SanityCheck(deviceId);

  uint64_t txDmaRequestFifoFillLevels =
      *(deviceId->pTxDmaRequestFifoFillLevelsRegister);

  if (pNormalFifoFillLevel != nullptr) {
    *pNormalFifoFillLevel =
        uint16_t(txDmaRequestFifoFillLevels);
  }
  if (pPriorityFifoFillLevel != nullptr) {
    *pPriorityFifoFillLevel =
        uint16_t(txDmaRequestFifoFillLevels >> 16);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/**
 *  Continue Tx DMA in spite of panic (no guarantees however
 * that anything useful happens!): Only used for debugging
 * purposes. DO NOT ENABLE IN PRODUCTION CODE!
 */
void SCI_SetGlobalPanicState(SC_DeviceId deviceId,
                             bool disableGlobalPanic) {
  SanityCheck(deviceId);

  *static_cast<volatile uint64_t *>(
      static_cast<uint64_t *>(deviceId->pBar0Registers) +
      FPGA_DISABLE_GLOBAL_PANIC / 8) =
      disableGlobalPanic ? 1 : 0;
}

static std::string FileBaseName(const char *fileName) {
  std::vector<char> fileBaseName;
  fileBaseName.reserve(1000);

  std::size_t fileNameLength = std::strlen(fileName);
  if (fileNameLength < fileBaseName.capacity()) {
    std::strcpy(&fileBaseName[0], fileName);
  } else {
    std::strcpy(&fileBaseName[0],
                &fileName[fileNameLength -
                          fileBaseName.capacity()]);
  }
  return basename(&fileBaseName[0]);
}

class SC_ExceptionFactory {
public:
  SC_Exception CreateException(const char *fileName,
                               int lineNumber,
                               const char *functionName,
                               SC_Error errorCode,
                               const char *message,
                               void *pContext) {
    return SC_Exception(fileName, lineNumber, functionName,
                        errorCode, message, pContext);
  }
};

static SC_Error _CallErrorHandler(const char *fileName,
                                  int lineNumber,
                                  const char *functionName,
                                  SC_Error errorCode,
                                  const char *message,
                                  bool setLastErrorCode) {
  std::string baseName;

  if (fileName != nullptr) {
    std::vector<char> fileBaseName;
    fileBaseName.reserve(1000);

    std::size_t fileNameLength = std::strlen(fileName);
    if (fileNameLength < fileBaseName.capacity()) {
      std::strcpy(&fileBaseName[0], fileName);
    } else {
      std::strcpy(&fileBaseName[0],
                  &fileName[fileNameLength -
                            fileBaseName.capacity()]);
    }

    baseName = FileBaseName(fileName);
  }

  SC_ErrorHandler *pErrorHandler = nullptr;
  void *pErrorHandlerUserContext = nullptr;
  bool throwException = false;

  if (!ApiLibraryStaticInitializer::
          IsStaticInitializationInProgress()) {
    // The real values are not available during static
    // initialization
    const ApiLibraryStaticInitializer &staticInitializer =
        ApiLibraryStaticInitializer::GetConstInstance();

    ScopedTinyThreadLock lock(
        staticInitializer.ErrorHandlingLock);

    pErrorHandler = staticInitializer.pErrorHandler;
    pErrorHandlerUserContext =
        staticInitializer.pErrorHandlerUserContext;
    throwException = staticInitializer.LibraryOptions
                         .LibraryOptions.ThrowExceptions;
  }

  if (pErrorHandler != nullptr) {
    SC_Error rc = (*pErrorHandler)(
        baseName.c_str(), lineNumber, functionName,
        errorCode, message, pErrorHandlerUserContext);
    if (rc == SC_ERR_HANDLER_CONTINUE) {
      // Continue to API function exit.
    } else if (rc == SC_ERR_HANDLER_THROW_EXCEPTION) {
      throwException = true;
    } else if (
        rc == SC_ERR_HANDLER_WRITE_TO_STDERR_AND_CONTINUE) {
      fprintf(stderr,
              "Error code %d at %s#%d# function %s: %s\n",
              errorCode, fileName, lineNumber, functionName,
              message);
      // Continue to API function exit.
    } else {
      // API function returns an error code from the error
      // handler.
      errorCode = rc;
    }
  }

  if (ApiLibraryStaticInitializer::
          IsStaticInitializationInProgress() ||
      ApiLibraryStaticInitializer::GetConstInstance()
          .LibraryOptions.LibraryOptions
          .WriteErrorsToStdErr) {
    LogError(nullptr,
             "Error code %d at %s#%d# function %s: %s\n",
             errorCode, fileName, lineNumber, functionName,
             message);
  }

  if (setLastErrorCode) {
    SetLastErrorCode(errorCode);
  }

  if (throwException) {
    SC_ExceptionFactory exceptionFactory;
    SC_Exception exception =
        exceptionFactory.CreateException(
            fileName, lineNumber, functionName, errorCode,
            message, pErrorHandlerUserContext);
    throw exception;
  }

  return errorCode;
}

SC_Error CallErrorHandlerFunctor::operator()(
    SC_Error errorCode, const char *format, ...) {
  std::vector<char> message;
  message.reserve(3000);

  va_list arguments;
  va_start(arguments, format);
  vsnprintf(&message[0], message.capacity(), format,
            arguments);
  va_end(arguments);

  SC_Error returnErrorCode = _CallErrorHandler(
      _fileName, _lineNumber, _functionName, errorCode,
      &message[0], _setLastErrorCode);

  return returnErrorCode;
}

/**
 *
 */
/*static*/ void
DumpMemoryLayout(FILE *pFile,
                 const SC_DeviceId /*deviceId*/,
                 const SC_ChannelId tcpChannelId) {
  // fprintf(f, "Mapped Regs Memory Start:   0x%p, Total
  // Size: 0x%X\n", deviceId->regs, REGS_FULL_SIZE);
  fprintf(pFile,
          "Mapped Memory Layout Start: 0x%p, Total Size: "
          "0x%X\n",
          tcpChannelId->pTotalMemory, MMAP_FULL_SIZE);
  fprintf(pFile,
          "    BAR 0 REGS: [0x%06X, 0x%06X) Size: 0x%06X "
          "(%u KB)\n",
          BAR0_REGS_OFFSET,
          BAR0_REGS_OFFSET + BAR0_REGS_SIZE, BAR0_REGS_SIZE,
          BAR0_REGS_SIZE / 1024);
  fprintf(pFile,
          "    BAR 2 REGS: [0x%06X, 0x%06X) Size: 0x%06X "
          "(%u KB)\n",
          BAR2_REGS_OFFSET,
          BAR2_REGS_OFFSET + BAR2_REGS_SIZE, BAR2_REGS_SIZE,
          BAR2_REGS_SIZE / 1024);
  fprintf(pFile,
          "    PL DMA:     [0x%06X, 0x%06X) Size: 0x%06X "
          "(%u KB)\n",
          PL_DMA_OFFSET, PL_DMA_OFFSET + PL_DMA_SIZE,
          PL_DMA_SIZE, PL_DMA_SIZE / 1024);
  fprintf(pFile,
          "    STATUS DMA: [0x%06X, 0x%06X) Size: 0x%06X "
          "(%u KB)\n",
          STATUS_DMA_OFFSET,
          STATUS_DMA_OFFSET + STATUS_DMA_SIZE,
          STATUS_DMA_SIZE, STATUS_DMA_SIZE / 1024);
  fprintf(pFile,
          "    BUCKETS:    [0x%06X, 0x%06X) Size: 0x%06X "
          "(%u KB)\n",
          BUCKETS_OFFSET, BUCKETS_OFFSET + BUCKETS_SIZE,
          BUCKETS_SIZE, BUCKETS_SIZE / 1024);
  fprintf(pFile,
          "    RECV DMA:   [0x%06X, 0x%06X) Size: 0x%06X "
          "(%u KB)\n",
          RECV_DMA_OFFSET, RECV_DMA_OFFSET + RECV_DMA_SIZE,
          RECV_DMA_SIZE, RECV_DMA_SIZE / 1024);
}

static const bool UseDeviceContextPlDMA = false;

SC_Exception::SC_Exception(const char *fileName,
                           int lineNumber,
                           const char *functionName,
                           SC_Error errorCode,
                           const char *message,
                           void *pContext)
    : std::runtime_error(message),
      _fileName(fileName == nullptr ? "" : fileName),
      _lineNumber(lineNumber), _functionName(functionName),
      _errorCode(errorCode),
      _pContext(pContext, &NullDeleter<void>) {}

#if 0
// Don't need this constructor right now but keep it around just in case...
SC_Exception::SC_Exception(const char * fileName, int lineNumber, SC_Error errorCode, const char * format, ...)
    : SC_Exception(fileName, lineNumber, errorCode, format, (va_start(_arguments, format), _arguments))
{
    va_end(_arguments);
}
#endif

SC_Exception::~SC_Exception() throw() {}

const std::string &SC_Exception::GetFileName() const {
  return _fileName;
}

int SC_Exception::GetLineNumber() const {
  return _lineNumber;
}

const std::string &SC_Exception::GetFunctionName() const {
  return _functionName;
}

SC_Error SC_Exception::GetErrorCode() const {
  return _errorCode;
}

void *SC_Exception::GetContext() const {
  return _pContext.get();
}

static inline uint64_t
_GetPointerListDMA(SC_ChannelId channelId,
                   int16_t channelNumber) {
  return channelId->pPointerListDMA
      ->lastPacket[channelNumber];
}

static SC_Error GetPointerListDMA(SC_DeviceId deviceId,
                                  int16_t channelNumber,
                                  uint64_t *pLastPacket) {
  if (UseDeviceContextPlDMA) {
    if (deviceId->pPointerListDMA != nullptr) {
      *pLastPacket = deviceId->pPointerListDMA
                         ->lastPacket[channelNumber];
      return SC_ERR_SUCCESS;
    }
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "channel number %u device pointer list is not "
        "initialized",
        channelNumber);
  }
  // Use channel's PL DMA:
  if (deviceId->Channels[channelNumber] != nullptr &&
      deviceId->Channels[channelNumber]->pPointerListDMA !=
          nullptr) {
    *pLastPacket = _GetPointerListDMA(
        deviceId->Channels[channelNumber], channelNumber);
    return SC_ERR_SUCCESS;
  }

  return CallErrorHandlerDontSetLastError(
      SC_ERR_INVALID_ARGUMENT,
      "channel number %u channel pointer list is not "
      "initialized",
      channelNumber);
}

static SC_Error SetPointerListDMA(SC_DeviceId deviceId,
                                  int16_t channelNumber,
                                  uint64_t lastPacket) {
  if (UseDeviceContextPlDMA) {
    if (deviceId->pPointerListDMA != nullptr) {
      deviceId->pPointerListDMA->lastPacket[channelNumber] =
          lastPacket;
      return SC_ERR_SUCCESS;
    }
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "channel number %u device pointer list is not "
        "initialized",
        channelNumber);
  }
  // Use channel's PL DMA:
  if (deviceId->Channels[channelNumber] != nullptr &&
      deviceId->Channels[channelNumber]->pPointerListDMA !=
          nullptr) {
    deviceId->Channels[channelNumber]
        ->pPointerListDMA->lastPacket[channelNumber] =
        lastPacket;
    return SC_ERR_SUCCESS;
  }
  return CallErrorHandlerDontSetLastError(
      SC_ERR_INVALID_ARGUMENT,
      "channel number %u channel pointer list is not "
      "initialized",
      channelNumber);
}

SC_Error SC_GetLastErrorCode() {
  const ApiLibraryStaticInitializer &staticInitializer =
      ApiLibraryStaticInitializer::GetConstInstance();

  SC_Error lastErrorCode = staticInitializer.LastErrorCode;
  return lastErrorCode;
}

SC_Error SC_SetLastErrorCode(SC_Error errorCode) {
  ApiLibraryStaticInitializer &staticInitializer =
      ApiLibraryStaticInitializer::GetInstance();

  SC_Error previousErrorCode =
      staticInitializer.LastErrorCode;
  staticInitializer.LastErrorCode = errorCode;
  return previousErrorCode;
}

SC_Error SCI_GetLastThreadErrorCode() {
  ApiLibraryThreadStaticInitializer
      &threadStaticInitializer =
          ApiLibraryThreadStaticInitializer::
              GetThreadInstance();

  SC_Error lastErrorCode =
      threadStaticInitializer.LastThreadErrorCode;
  return lastErrorCode;
}

SC_Error SCI_SetLastThreadErrorCode(SC_Error errorCode) {
  ApiLibraryThreadStaticInitializer
      &threadStaticInitializer =
          ApiLibraryThreadStaticInitializer::
              GetThreadInstance();

  SC_Error previousThreadErrorCode =
      threadStaticInitializer.LastThreadErrorCode;
  threadStaticInitializer.LastThreadErrorCode = errorCode;
  return previousThreadErrorCode;
}

SC_Error SC_GetLibraryLogMask(SC_DeviceId deviceId,
                              SC_LogMask *pLogMask) {
  if (deviceId == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "device id cannot be NULL!");
  }
  if (pLogMask == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "pLogMask cannot be NULL!");
  }

  SanityCheck(deviceId);

  *pLogMask = SC_LogMask(deviceId->LogMask);

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

SC_Error SC_SetLibraryLogMask(SC_DeviceId deviceId,
                              SC_LogMask logMask) {
  if (deviceId == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "device id cannot be NULL!");
  }

  SanityCheck(deviceId);

  deviceId->LogMask = logMask;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

SC_Boolean
SC_IsOobNetworkInterface(SC_DeviceId deviceId,
                         uint8_t networkInterface) {
  if (deviceId == nullptr) {
    return SC_Boolean(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "device id cannot be NULL!"));
  }

  SanityCheck(deviceId);

  if (networkInterface >=
      deviceId->InternalCardInfo.CardInfo
          .NumberOfNetworkInterfaces) {
    return false;
  }
  return (deviceId->InternalCardInfo.CardInfo
              .OobNetworkInterfacesEnabledMask &
          (1 << networkInterface)) != 0;
}

SC_Boolean
SC_IsTcpNetworkInterface(SC_DeviceId deviceId,
                         uint8_t networkInterface) {
  if (deviceId == nullptr) {
    return SC_Boolean(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "device id cannot be NULL!"));
  }

  SanityCheck(deviceId);

  if (networkInterface >=
      deviceId->InternalCardInfo.CardInfo
          .NumberOfNetworkInterfaces) {
    return false;
  }
  return (deviceId->InternalCardInfo.CardInfo
              .TcpNetworkInterfacesEnabledMask &
          (1 << networkInterface)) != 0;
  /* This function is only supported on Silicom SmartNIC.
   * Available  */
  UNUSED_ALWAYS(deviceId);
  UNUSED_ALWAYS(networkInterface);
  return false;
}

SC_Boolean
SC_IsUdpNetworkInterface(SC_DeviceId deviceId,
                         uint8_t networkInterface) {
  if (deviceId == nullptr) {
    return SC_Boolean(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "device id cannot be NULL!"));
  }

  SanityCheck(deviceId);

  if (networkInterface >=
      deviceId->InternalCardInfo.CardInfo
          .NumberOfNetworkInterfaces) {
    return false;
  }
  return (deviceId->InternalCardInfo.CardInfo
              .UdpNetworkInterfacesEnabledMask &
          (1 << networkInterface)) != 0;
  UNUSED_ALWAYS(deviceId);
  UNUSED_ALWAYS(networkInterface);
  return false;
}

SC_Boolean
SC_IsMacNetworkInterface(SC_DeviceId deviceId,
                         uint8_t networkInterface) {
  if (deviceId == nullptr) {
    return SC_Boolean(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "device id cannot be NULL!"));
  }

  SanityCheck(deviceId);

  if (networkInterface >=
      deviceId->InternalCardInfo.CardInfo
          .NumberOfNetworkInterfaces) {
    return false;
  }
  return (deviceId->InternalCardInfo.CardInfo
              .MacNetworkInterfacesEnabledMask &
          (1 << networkInterface)) != 0;
}

SC_Boolean
SC_IsMonitorNetworkInterface(SC_DeviceId deviceId,
                             uint8_t networkInterface) {
  if (deviceId == nullptr) {
    return SC_Boolean(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "device id cannot be NULL!"));
  }

  SanityCheck(deviceId);

  if (networkInterface >=
      deviceId->InternalCardInfo.CardInfo
          .NumberOfNetworkInterfaces) {
    return false;
  }
  return (deviceId->InternalCardInfo.CardInfo
              .MonitorNetworkInterfacesEnabledMask &
          (1 << networkInterface)) != 0;
}

SC_Boolean SC_IsQsfpPort(SC_DeviceId deviceId,
                         uint8_t port) {
  if (deviceId == nullptr) {
    return SC_Boolean(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "device id cannot be NULL!"));
  }

  SanityCheck(deviceId);

  if (port >= 8) {
    return false;
  }
  return (deviceId->InternalCardInfo.CardInfo
              .QsfpPortsEnabledMask &
          (1 << port)) != 0;
}

SC_Error
SC_RegisterErrorHandler(SC_ErrorHandler *pErrorHandler,
                        void *pUserContext) {
  ApiLibraryStaticInitializer &staticInitializer =
      ApiLibraryStaticInitializer::GetInstance();
  ScopedTinyThreadLock lock(
      staticInitializer.ErrorHandlingLock);

  staticInitializer.pErrorHandler = pErrorHandler;
  staticInitializer.pErrorHandlerUserContext = pUserContext;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

static SC_Error DeleteChannel(SC_ChannelId channelId) {
  if (channelId != nullptr) {
    int16_t channelNumber = channelId->DmaChannelNumber;

    if ((channelNumber >= 0 &&
         channelNumber < SC_MAX_CHANNELS)) {
      SC_DeviceId deviceId = channelId->DeviceId;

      ScopedTinyThreadLock lock(deviceId->ChannelsLock);

      if (deviceId->Channels[channelNumber] == nullptr) {
        return CallErrorHandlerDontSetLastError(
            SC_ERR_INVALID_ARGUMENT,
            "channel %p channel number %d does not exist.",
            static_cast<const void *>(channelId),
            channelNumber);
      }

      // Delete channel from device context
      if (deviceId->Channels[channelNumber]
                  ->DmaChannelNumber != channelNumber ||
          deviceId->Channels[channelNumber] != channelId) {
        return CallErrorHandlerDontSetLastError(
            SC_ERR_INVALID_ARGUMENT,
            "failed to delete channel %p channel number %d",
            static_cast<const void *>(channelId),
            channelNumber);
      }
      deviceId->Channels[channelNumber] = nullptr;
    } else if (channelNumber != FIND_FREE_DMA_CHANNEL) {
      return CallErrorHandlerDontSetLastError(
          SC_ERR_INVALID_ARGUMENT,
          "invalid channel %p channel number %d",
          static_cast<const void *>(channelId),
          channelNumber);
    }

    delete channelId;
  }

  return SC_ERR_SUCCESS;
}

/**
 *  Get the name of the device. If @p deviceName is null or
 * "smartnic" or "packetmover" then return the default
 * device name "/dev/smartnic" or  "/dev/packetmover".
 *
 *  @param  deviceName  Device name or null to get the
 * default device name.
 *
 *  @return Return a valid device name.
 */
static std::string GetDeviceName(const char *deviceName) {
  if (deviceName == nullptr ||
      strcmp(deviceName, LOWER_PRODUCT) == 0) {
    return "/dev/" LOWER_PRODUCT; // Default device name to
                                  // use.
  } else if (strstr(deviceName, "/") == NULL) {
    return std::string("/dev/") + deviceName;
  }
  return deviceName;
}

inline void
AddSendReason(SC_TxFlowControlStatistics &statistics,
              int newSendReason) {
  statistics.SendReason =
      SCI_SendReason(statistics.SendReason | newSendReason);
}

/**
 *  Update cached values of ETLM TcbSndUna and the single 16
 * kB consumer buffer consumed bytes. Also used to update
 * user logic consumed bytes because they are both in the
 * same register read.
 *
 *  @brief  Read ETLM TcbSndUna and the consumed bytes count
 * from a channel specific register.
 *
 *  @param  channelId                   Channel id.
 *  @param  txConsumedBytesFlowControl  Consumed bytes
 * parameters shared by all TCP channels or channel specific
 * for all other channels.
 *  @param  pTxWindowFlowControl        TCP window consumed
 * byte parameters or NULL for all other channels.
 *  @param  alreadyLocked               True if protected
 * resources are already locked, false otherwise.
 */
static void UpdateCachedConsumedBytes(
    SC_ChannelId channelId,
    volatile SC_TxConsumedBytesFlowControl
        &txConsumedBytesFlowControl,
    SC_TxWindowFlowControl *pTxWindowFlowControl,
    bool alreadyLocked) {
  ApiScopedTimer(API_TIMER_UPDATE_CACHED_CONSUMED_BYTES);

  const int64_t FOUR_GB =
      4UL * 1024UL * 1024UL *
      1024UL; // 4 gigabytes, a limit where 32-bit counter
              // wraps around

  // If the lock is present then this is a TCP channel
  // and access to consumed bytes needs to be
  // locked because it is shared by all TCP channels:
  bool lock = txConsumedBytesFlowControl.pLock != nullptr &&
              !alreadyLocked;

  if (lock) {
    assert(channelId->ChannelType ==
           SC_DMA_CHANNEL_TYPE_TCP);
    assert(channelId->pTxWindowFlowControl != nullptr);
    SC_Lock(*txConsumedBytesFlowControl.pLock);

    if (LOG(channelId->DeviceId,
            LOG_TCP_FLOW_CONTROL_LOCKING)) {
      Log(channelId->DeviceId,
          "TCP flow control of DMA channel %u locked\n",
          channelId->DmaChannelNumber);
    }
  }
#ifdef DEBUG
  else {
    // printf("channelType %u, _pLock %p,
    // _pTxWindowFlowControl %p\n", channelId->channelType,
    // txConsumedBytesFlowControl._pLock,
    // channelId->_pTxWindowFlowControl);
    if (channelId->pTxWindowFlowControl != nullptr) {
      assert(channelId->ChannelType ==
             SC_DMA_CHANNEL_TYPE_TCP);
    }
  }
#endif

  uint32_t consumedBytes, tcbSndUna;
  ReadConsumedBytesAndTcbSndUna(channelId, consumedBytes,
                                tcbSndUna);

  if (LOG(channelId->DeviceId,
          LOG_TX_FLOW_CONTROL | LOG_TX_PIPE_FULL)) {
    if (pTxWindowFlowControl != nullptr) {
      // TCP channel:
      Log(channelId->DeviceId,
          "Read from register: sent %lu, consumed %u, "
          "previous %u, cached %u, "
          "TcbSndUna %u, previous %u, cached %u\n",
          txConsumedBytesFlowControl.SentBytesCount,
          consumedBytes,
          txConsumedBytesFlowControl
              .PreviousConsumedBytesCount,
          txConsumedBytesFlowControl
              .CachedLastConsumedBytesCount,
          tcbSndUna,
          pTxWindowFlowControl->PreviousLastTcbSndUna,
          pTxWindowFlowControl->CachedLastTcbSndUna);
    } else {
      // All other channels:
      Log(channelId->DeviceId,
          "Read from register: sent %lu, consumed %u, "
          "previous %u, cached %u\n",
          txConsumedBytesFlowControl.SentBytesCount,
          consumedBytes,
          txConsumedBytesFlowControl
              .PreviousConsumedBytesCount,
          txConsumedBytesFlowControl
              .CachedLastConsumedBytesCount);
    }
  }

  txConsumedBytesFlowControl.PreviousConsumedBytesCount =
      txConsumedBytesFlowControl
          .CachedLastConsumedBytesCount;
  txConsumedBytesFlowControl.CachedLastConsumedBytesCount =
      consumedBytes;

  // Has 32-bit unsigned consumed bytes count wrapped
  // around?
  bool wrapAround =
      txConsumedBytesFlowControl
          .CachedLastConsumedBytesCount <
      txConsumedBytesFlowControl.PreviousConsumedBytesCount;
  if (wrapAround) {
    // Consumed bytes value has wrapped around 4 GB

    if (LOG(channelId->DeviceId,
            LOG_TX_COUNTER_SWAP_AROUND)) {
      Log(channelId->DeviceId,
          "Ch. %u wrap. Before: sent %lu, cached %u, "
          "previous %u.",
          channelId->DmaChannelNumber,
          txConsumedBytesFlowControl.SentBytesCount,
          txConsumedBytesFlowControl
              .CachedLastConsumedBytesCount,
          txConsumedBytesFlowControl
              .PreviousConsumedBytesCount);
    }

    // Consumed bytes just became less by 4 GB, subtract the
    // same from sent bytes so buffer fill count still works
    // correctly:
    txConsumedBytesFlowControl.SentBytesCount -= FOUR_GB;

    if (LOG(channelId->DeviceId,
            LOG_TX_COUNTER_SWAP_AROUND)) {
      Log(channelId->DeviceId,
          " After: sent %lu, cached %u, previous %u\n",
          txConsumedBytesFlowControl.SentBytesCount,
          txConsumedBytesFlowControl
              .CachedLastConsumedBytesCount,
          txConsumedBytesFlowControl
              .PreviousConsumedBytesCount);
    }
  }

  if (lock) {
    if (LOG(channelId->DeviceId,
            LOG_TCP_FLOW_CONTROL_LOCKING)) {
      Log(channelId->DeviceId,
          "TCP flow control of channel %u unlocking...\n",
          channelId->DmaChannelNumber);
    }

    SC_Unlock(*txConsumedBytesFlowControl.pLock);
  }

  if (pTxWindowFlowControl != nullptr) {
    // TCP window flow control is always per channel and
    // does not need lock protection.
    pTxWindowFlowControl->PreviousLastTcbSndUna =
        pTxWindowFlowControl->CachedLastTcbSndUna;
    pTxWindowFlowControl->CachedLastTcbSndUna = tcbSndUna;

    if (LOG(channelId->DeviceId,
            LOG_TX_COUNTER_SWAP_AROUND | LOG_DETAIL)) {
      Log(channelId->DeviceId,
          "TCP wrap. Consumed: %u, sent %lu, cached %u, "
          "previous %u\n",
          tcbSndUna, pTxWindowFlowControl->SentBytesCount,
          pTxWindowFlowControl->CachedLastTcbSndUna,
          pTxWindowFlowControl->PreviousLastTcbSndUna);
    }

    // Has 32-bit unsigned TCP window byte count (TcbSndUna)
    // wrapped around?
    wrapAround =
        pTxWindowFlowControl->CachedLastTcbSndUna <
        pTxWindowFlowControl->PreviousLastTcbSndUna;
    if (wrapAround) {
      // Window bytes count has wrapped around 4 GB

      if (LOG(channelId->DeviceId,
              LOG_TX_COUNTER_SWAP_AROUND)) {
        Log(channelId->DeviceId,
            "TCP wrap. Before: sent %lu, cached %u, "
            "previous %u.",
            pTxWindowFlowControl->SentBytesCount,
            pTxWindowFlowControl->CachedLastTcbSndUna,
            pTxWindowFlowControl->PreviousLastTcbSndUna);
      }

      // TCP window consumed bytes just became less by 4 GB,
      // subtract the same from sent bytes so buffer fill
      // count still works correctly:
      pTxWindowFlowControl->SentBytesCount -= FOUR_GB;
      assert(pTxWindowFlowControl->SentBytesCount >= 0);

      if (LOG(channelId->DeviceId,
              LOG_TX_COUNTER_SWAP_AROUND)) {
        Log(channelId->DeviceId,
            " After: sent %lu, cached %u, previous %u\n",
            pTxWindowFlowControl->SentBytesCount,
            pTxWindowFlowControl->CachedLastTcbSndUna,
            pTxWindowFlowControl->PreviousLastTcbSndUna);
      }
    }
  }
}

/**
 *  Initialize cached values of ETLM TcbSndUna and the
 * single 16 kB consumer buffer consumed bytes and align
 * with internal software counters. Also used to initialize
 * user logic consumed bytes.
 *
 *  @brief   Read the consumed bytes count from a channel
 * specific register and update internal software counters
 * to same values as a starting point for consumed bytes
 * flow control.
 *
 *  @param  channelId                   Channel id.
 *  @param  txConsumedBytesFlowControl  Consumed bytes
 * parameters shared by all TCP channels or channel specific
 * for all other channels.
 *  @param  pTxWindowFlowControl        TCP window consumed
 * byte parameters or NULL for all other channels.
 */
static void InitializeCachedConsumedBytes(
    SC_ChannelId channelId,
    volatile SC_TxConsumedBytesFlowControl
        &txConsumedBytesFlowControl,
    SC_TxWindowFlowControl *pTxWindowFlowControl) {
  // If the lock is present then this is a TCP channel
  // and access to consumed bytes needs to be
  // locked because it is shared by all TCP channels:
  bool lock = txConsumedBytesFlowControl.pLock != nullptr;

  if (lock) {
    assert(channelId->ChannelType ==
           SC_DMA_CHANNEL_TYPE_TCP);
    assert(channelId->pTxWindowFlowControl != nullptr);
    SC_Lock(*txConsumedBytesFlowControl.pLock);

    if (LOG(channelId->DeviceId,
            LOG_TCP_FLOW_CONTROL_LOCKING)) {
      Log(channelId->DeviceId,
          "Initialization of TCP flow control of channel "
          "%u locked\n",
          channelId->DmaChannelNumber);
    }
  }
#ifdef DEBUG
  else {
    // printf("channelType %u, _pLock %p,
    // _pTxWindowFlowControl %p\n", channelId->channelType,
    // txConsumedBytesFlowControl._pLock,
    // channelId->_pTxWindowFlowControl);
    if (channelId->pTxWindowFlowControl != nullptr) {
      assert(channelId->ChannelType ==
             SC_DMA_CHANNEL_TYPE_TCP);
    }
  }
#endif

  if (pTxWindowFlowControl != nullptr) {
    pTxWindowFlowControl->CachedLastTcbSndUna = 0;
  }

  // Zero cached consumed byte counts to avoid possible 4 GB
  // wrap around in UpdateCachedConsumedBytes when
  // InitializeCachedConsumedBytes is repeatedly called to
  // initialize already used channel.
  txConsumedBytesFlowControl.CachedLastConsumedBytesCount =
      0;

  UpdateCachedConsumedBytes(channelId,
                            txConsumedBytesFlowControl,
                            pTxWindowFlowControl, lock);

  // Also initialize internal software consumed bytes
  // counter to the cached value to have a common starting
  // point
  txConsumedBytesFlowControl.SentBytesCount =
      txConsumedBytesFlowControl
          .CachedLastConsumedBytesCount;

  if (lock) {
    if (LOG(channelId->DeviceId,
            LOG_TCP_FLOW_CONTROL_LOCKING)) {
      Log(channelId->DeviceId,
          "Initialization of TCP flow control of channel "
          "%u unlocking...\n",
          channelId->DmaChannelNumber);
    }

    SC_Unlock(*txConsumedBytesFlowControl.pLock);
  }

  if (pTxWindowFlowControl != nullptr) {
    // Also initialize internal sent bytes counter to the
    // cached TcbSndUna value to have a common starting
    // point
    pTxWindowFlowControl->SentBytesCount =
        pTxWindowFlowControl->CachedLastTcbSndUna;
  }
}

/**
 *  Common parameters used by Tx send functions.
 */
struct SendParameters {
  uint16_t DmaChannelNumber; ///< DMA channel number.
  uint16_t PayloadLength; ///< Length of the bucket payload.
  uint64_t
      PayloadPhysicalAddress; ///< Payload DMA address in
                              ///< kernel space, the
                              ///< physical address that is
                              ///< actually accessed by the
                              ///< FPGA DMA.
  uint16_t
      PayloadLengthRoundedUpTo32; ///< Payload length
                                  ///< rounded up to nearest
                                  ///< 32 byte boundary.
  bool
      Priority; ///< Select between normal or priority FIFO.
  ::TxDmaFifoFlowControl
      &TxDmaFifoFlowControl; ///< Reference to Tx DMA FIFO
                             ///< flow control parameters.

  inline explicit SendParameters(
      SC_ChannelId channelId, uint16_t payloadLength,
      uint64_t payloadPhysicalAddress, bool priority)
      : DmaChannelNumber(channelId->DmaChannelNumber),
        PayloadLength(payloadLength),
        PayloadPhysicalAddress(payloadPhysicalAddress),
        PayloadLengthRoundedUpTo32(payloadLength),
        Priority(priority),
        TxDmaFifoFlowControl(
            priority
                ? channelId->TxDmaPriorityFifoFlowControl
                : channelId->TxDmaNormalFifoFlowControl) {
    UNUSED_ALWAYS(priority);
  }
};

/**
 *  Tx DMA FIFO flow control based on reading the fill level
 * register.
 *
 *  @brief      There are 2 FIFOs, normal and priority of
 * the same size. Both fill levels can be read with a single
 * register read.
 *
 *  @param  channelId                   Channel id.
 *  @param  sendParameters              Send parameters.
 *  @param  bookings                    Booking flags if any
 * bookings were taken.
 *
 *  @return                             True if there is
 * place for a write request to the FIFO, false if the FIFO
 * is full.
 */
static inline bool TxDmaFirmwareFlowControl(
    SC_ChannelId channelId,
    const SendParameters &sendParameters,
    SendBookings &bookings) {
  ApiScopedTimer(API_TIMER_TX_DMA_FIRMWARE_FLOW_CONTROL);

  SC_DeviceId deviceId = channelId->DeviceId;
  bool priority = sendParameters.Priority;

  // Read normal and priority queue fill levels from the
  // register:
  uint64_t txDmaRequestFifoFillLevels =
      *(deviceId->pTxDmaRequestFifoFillLevelsRegister);
  uint16_t fillLevel =
      priority ? uint16_t(txDmaRequestFifoFillLevels >> 16)
               : uint16_t(txDmaRequestFifoFillLevels);

  // Max concurrency count leaves free room for the same
  // count of concurrent threads in separate processes to
  // get a "canSend = true" result without lock protection.
  // An alternative would be a shared memory lock but a max
  // concurrency count gives superior performance (no
  // locking!).
  uint16_t maxTxDmaFifoConcurrency =
      priority ? deviceId->MaxPriorityFifoConcurrency
               : deviceId->MaxNormalFifoConcurrency;

  bool canSend = fillLevel <
                 MAX_FIFO_ENTRIES - maxTxDmaFifoConcurrency;
  if (canSend) {
    // Book that Tx DMA FIFO hardware flow control allows
    // sending
    bookings = SendBookings(bookings |
                            SEND_BOOKING_FW_TD_DMA_FIFO);
  }

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics;
      SCI_TxDmaFifoStatistics &fifoStatistics =
          priority ? statistics.PriorityFifo
                   : statistics.NormalFifo;
      SCI_SendReason sendReasonFifo =
          priority ? SCI_SEND_REASON_PRIORITY_FIFO
                   : SCI_SEND_REASON_NORMAL_FIFO;

      if (fillLevel > fifoStatistics.FwMaximumFillLevel) {
        fifoStatistics.FwMaximumFillLevel = fillLevel;
      } if (canSend) {
        AddSendReason(statistics,
                      SCI_SEND_REASON_FW_FIFO_SUCCESS |
                          sendReasonFifo);
        ++fifoStatistics.FwSuccessCount;
      } else {
        AddSendReason(statistics,
                      SCI_SEND_REASON_FW_FIFO_FULL |
                          sendReasonFifo);
        ++fifoStatistics.FwFullCount;
      })

  if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
    Log(channelId->DeviceId,
        "TxDmaFirmwareFlowControl returns canSend = %u"
        " (fillLevel=%u >= MAX_FIFO_ENTRIES - "
        "maxTxDmaFifoConcurrency=%u, "
        "txDmaRequestFifoFillLevels=0x%lx)\n",
        canSend, fillLevel,
        MAX_FIFO_ENTRIES - maxTxDmaFifoConcurrency,
        txDmaRequestFifoFillLevels);
  }

  return canSend;
}

/**
 *  Helper function to Tx DMA FIFO statistics.
 */
static inline SCI_TxDmaFifoStatistics &
GetFifoStatistics(const SendParameters &sendParameters,
                  SC_TxFlowControlStatistics &statistics,
                  SCI_SendReason *pReasonFifo) {
  if (pReasonFifo != nullptr) {
    *pReasonFifo = sendParameters.Priority
                       ? SCI_SEND_REASON_PRIORITY_FIFO
                       : SCI_SEND_REASON_NORMAL_FIFO;
  }
  return sendParameters.Priority ? statistics.PriorityFifo
                                 : statistics.NormalFifo;
}

/**
 *  Tx DMA FIFO flow control based on channel specific
 * maximum limits.
 *
 *  @brief      There are 2 FIFOs, normal and priority of
 * the same size. The cumulated maximum limits of all
 * <b>used</b> channels should not exceed the Tx DMA FIFO
 * size of 512 entries.
 *
 *  @param  channelId                   Channel id.
 *  @param  sendParameters              Reference to Tx send
 * parameters.
 *  @param  bookings                    Booking flags if any
 * bookings were taken.
 *
 *  @return                             True if there is
 * place for a write request to the FIFO, false if the FIFO
 * is full.
 */
static inline bool TxDmaSoftwareFlowControl(
    SC_ChannelId channelId,
    const SendParameters &sendParameters,
    SendBookings &bookings) {
  ApiScopedTimer(API_TIMER_TX_DMA_SOFTWARE_FLOW_CONTROL);

  assert(
      sendParameters.TxDmaFifoFlowControl.FillLevelLimit >
      0);
  assert(
      sendParameters.TxDmaFifoFlowControl.FillLevel <=
      sendParameters.TxDmaFifoFlowControl.FillLevelLimit);
  assert((bookings & SEND_BOOKING_SW_TD_DMA_FIFO) == 0);

  // Check per channel FIFO limit:
  bool canSend =
      sendParameters.TxDmaFifoFlowControl.FillLevel <
      sendParameters.TxDmaFifoFlowControl.FillLevelLimit;
  if (canSend) {
    // Book this entry so nobody else can take it. If other
    // flow controls fail then this booking has to be
    // cancelled. Only a single thread per channel should do
    // Tx operations, otherwise the fill level count below
    // fails.
    ++sendParameters.TxDmaFifoFlowControl.FillLevel;
    bookings = SendBookings(bookings | SEND_BOOKING_TAKEN |
                            SEND_BOOKING_SW_TD_DMA_FIFO);
  }

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics;
      SCI_SendReason reasonFifo;
      SCI_TxDmaFifoStatistics &fifoStatistics =
          GetFifoStatistics(sendParameters, statistics,
                            &reasonFifo);

      if (canSend) {
        AddSendReason(statistics,
                      SCI_SEND_REASON_SW_FIFO_SUCCESS |
                          reasonFifo);
        ++fifoStatistics.SwSuccessCount;
      } else {
        AddSendReason(statistics,
                      SCI_SEND_REASON_SW_FIFO_FULL |
                          reasonFifo);
        ++fifoStatistics.SwFullCount;
      })

  if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
    Log(channelId->DeviceId,
        "%s returns canSend = %u, fill level = %u\n",
        __func__, canSend,
        sendParameters.TxDmaFifoFlowControl.FillLevel);
  }

  return canSend;
}

/**
 *  Tx flow control based on channel specific second level
 * buffers that report a consumed bytes count.
 *
 *  @brief      These buffers are currently all 16kB in size
 * but this library allows channel specific buffer sizes to
 * be specified. The consumed byte count is reported in 32
 * byte chunks, i.e. it is always rounded up to nearest
 *              multiple of 32 bytes because of internal
 * buffer implementation in the firmware. Consequently the
 * count of sent bytes follows the same scheme.
 *
 *  @param  channelId                       Channel id.
 *  @param  pTxConsumedBytesFlowControl     Pointer to a
 * channel specific consumed bytes data structure.
 *  @param  pTxWindowFlowControl            Pointer to a
 * channel specific TCP window buffer consumed bytes data
 * structure.
 *  @param  sendParameters                  Send parameters.
 *  @param  bookings                        Booking flags if
 * any bookings were taken.
 *
 *  @return                                 True if there is
 * place to write the packet in the buffer, false if the
 * buffer is full. Variable \p
 * SendParameters::PayloadLengthRoundedUpTo32 returns the
 * rounded up payload length.
 */
static bool FirmwareConsumedBytesFlowControl(
    SC_ChannelId channelId,
    volatile SC_TxConsumedBytesFlowControl
        &txConsumedBytesFlowControl,
    SC_TxWindowFlowControl *pTxWindowFlowControl,
    SendParameters &sendParameters,
    SendBookings &bookings) {
  ApiScopedTimer(
      API_TIMER_FIRMWARE_CONSUMED_BYTES_FLOW_CONTROL);

  assert((bookings & SEND_BOOKING_CONSUMED_BYTES) == 0);

  const uint32_t ConsumedBytesDelta =
      32; // Consumed bytes count increments in this size
          // steps

  bool canSend = false;

  // TCP channels share a common consumed bytes flow control
  // and need lock protection, all other channels have
  // channel specific consumed bytes flow control.
  bool lock = txConsumedBytesFlowControl.pLock != nullptr;

  if (lock) {
    assert(pTxWindowFlowControl != nullptr);
    SC_Lock(*txConsumedBytesFlowControl.pLock);

    if (LOG(channelId->DeviceId,
            LOG_TCP_FLOW_CONTROL_LOCKING)) {
      Log(channelId->DeviceId,
          "Firmware consumed bytes flow control of channel "
          "%u locked\n",
          channelId->DmaChannelNumber);
    }
  }

  // Remember the precise original payload length before it
  // is rounded up
  uint32_t payloadLength =
      sendParameters.PayloadLengthRoundedUpTo32;

  // Consumed byte count is always multiple of
  // ConsumedBytesDelta
  uint32_t payloadModuloDelta =
      payloadLength % ConsumedBytesDelta;
  sendParameters.PayloadLengthRoundedUpTo32 =
      static_cast<uint16_t>(
          payloadLength +
          (payloadModuloDelta == 0
               ? 0
               : ConsumedBytesDelta - payloadModuloDelta));

  assert(sendParameters.PayloadLengthRoundedUpTo32 %
             ConsumedBytesDelta ==
         0);
  assert(sendParameters.PayloadLengthRoundedUpTo32 >=
         payloadLength);

  // In first round check if cached last consumed bytes
  // count allows us to send. If not then in a second round
  // actually read and update the consumed bytes register.
  // All this to read the register only when it is
  // absolutely necessary because it is very slow (around
  // 500 ns).
  int i = 0;
  for (; i < 2;
       ++i) // on first round use the cached value, on the
            // second round update by reading the register
  {
    uint32_t consumedByteCount =
        txConsumedBytesFlowControl
            .CachedLastConsumedBytesCount;

    // Buffer fill byte count if we send this packet
    int64_t bufferFillByteCount =
        txConsumedBytesFlowControl.SentBytesCount -
        consumedByteCount;
    bufferFillByteCount +=
        sendParameters.PayloadLengthRoundedUpTo32;
    assert(bufferFillByteCount >= 0);

    canSend = bufferFillByteCount <=
              txConsumedBytesFlowControl.BufferSize;

    if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
      Log(channelId->DeviceId,
          "Tx consumer %s, ca%s send: payloadLength=%4u, "
          "payloadLengthRoundedUpTo32=%4u, "
          "consumedByteCount=%u, new "
          "bufferFillByteCount=%ld-%u+%u=%ld, "
          "bufferSize=%u\n",
          i == 0 ? "cached " : "updated",
          canSend ? "n" : "n't", payloadLength,
          sendParameters.PayloadLengthRoundedUpTo32,
          consumedByteCount,
          txConsumedBytesFlowControl.SentBytesCount,
          consumedByteCount,
          sendParameters.PayloadLengthRoundedUpTo32,
          bufferFillByteCount,
          txConsumedBytesFlowControl.BufferSize);
    }

    if (canSend) {
      break;
    }

    // Cannot send based on the cached consumed bytes count.
    // Now actually read the register and update the cached
    // value and retry:
    UpdateCachedConsumedBytes(channelId,
                              txConsumedBytesFlowControl,
                              pTxWindowFlowControl, lock);
  }

  if (canSend) {
    // Book the free place in the buffer so nobody else
    // can get it. If other flow checks after this one
    // should fail then this booking has to be cancelled.
    txConsumedBytesFlowControl.SentBytesCount +=
        sendParameters.PayloadLengthRoundedUpTo32;
    bookings =
        lock ? SendBookings(bookings |
                            SEND_BOOKING_TAKEN_AND_LOCKED |
                            SEND_BOOKING_CONSUMED_BYTES)
             : SendBookings(bookings | SEND_BOOKING_TAKEN |
                            SEND_BOOKING_CONSUMED_BYTES);
  }

  if (lock) {
    if (LOG(channelId->DeviceId,
            LOG_TCP_FLOW_CONTROL_LOCKING)) {
      Log(channelId->DeviceId,
          "Firmware consumed bytes flow control of channel "
          "%u unlocking...\n",
          channelId->DmaChannelNumber);
    }

    SC_Unlock(*txConsumedBytesFlowControl.pLock);
  }

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics;

      if (i == 0) {
        // Success on first round, cached value was used
        ++statistics.ConsumerChannel.CachedReads;
      } else {
        // Register read was necessary (i == 1 if can send
        // after reading register, i == 2 if can't send
        // after reading register)
        ++statistics.ConsumerChannel.RegisterReads;
      }

      if (canSend) {
        AddSendReason(statistics,
                      SCI_SEND_REASON_CONSUMER_SUCCESS);
        ++statistics.ConsumerChannel.SuccessCount;
      } else {
        AddSendReason(statistics,
                      SCI_SEND_REASON_CONSUMER_FULL);
        ++statistics.ConsumerChannel.FullCount;
      })

  if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
    Log(channelId->DeviceId, "%s returns canSend = %u\n",
        __func__, canSend);
  }

  return canSend;
}

/**
 *  Tx flow control based on channel specific TCP window
 * buffers that report a consumed bytes count (the value of
 * ETLM TcbSndUna).
 *
 *  @brief      These buffers are currently all 16kB in size
 * but this library allows channel specific buffer sizes to
 * be specified. The consumed byte count is reported with 1
 * byte precision.
 *
 *  @param  channelId                       Channel id.
 *  @param  pTxWindowFlowControl            Pointer to a
 * channel specific TCP window buffer consumed bytes data
 * structure.
 *  @param  pTxConsumedBytesFlowControl     Pointer to a
 * channel specific consumed bytes data structure. Needed
 * here because TCP window and second level packet buffer
 *                                          consumed bytes
 * are read with the same single register read.
 *  @param  payloadLength                   Payload length.
 *                                          if the packet is
 * actually sent.
 *  @param  bookings                        Booking flags if
 * any bookings were taken.
 *
 *  @return                                 True if there is
 * place in the TCP window buffer, false if the buffer is
 * full.
 */
static bool TcpWindowFlowControl(
    SC_ChannelId channelId,
    SC_TxWindowFlowControl *pTxWindowFlowControl,
    volatile SC_TxConsumedBytesFlowControl
        &txConsumedBytesFlowControl,
    uint32_t payloadLength, SendBookings &bookings) {
  ApiScopedTimer(API_TIMER_TCP_WINDOW_FLOW_CONTROL);

  assert((bookings & SEND_BOOKING_TCP_WINDOW) == 0);

  bool canSend = false;

  int i = 0;
  for (; i < 2;
       ++i) // on first round use the cached value, on the
            // second round update by reading the register
  {
#ifdef DEBUG
    if (pTxWindowFlowControl->SentBytesCount <
        pTxWindowFlowControl->CachedLastTcbSndUna) {
      printf(
          "1: pTxWindowFlowControl->_sentBytesCount = %lu, "
          "pTxWindowFlowControl->_cachedLastTcbSndUna = "
          "%u\n",
          pTxWindowFlowControl->SentBytesCount,
          pTxWindowFlowControl->CachedLastTcbSndUna);
      bool doAssert =
          pTxWindowFlowControl->SentBytesCount >=
          pTxWindowFlowControl->CachedLastTcbSndUna;
      UpdateCachedConsumedBytes(
          channelId, txConsumedBytesFlowControl,
          pTxWindowFlowControl, false);
      printf(
          "2: pTxWindowFlowControl->_sentBytesCount = %lu, "
          "pTxWindowFlowControl->_cachedLastTcbSndUna = "
          "%u\n",
          pTxWindowFlowControl->SentBytesCount,
          pTxWindowFlowControl->CachedLastTcbSndUna);
      assert(doAssert);
    }
#endif
    uint32_t inFlightBytes =
        uint32_t(pTxWindowFlowControl->SentBytesCount -
                 pTxWindowFlowControl->CachedLastTcbSndUna);

    assert(pTxWindowFlowControl->EtlmTxWindowSize >=
           inFlightBytes);
    uint32_t canSendBytes =
        pTxWindowFlowControl->EtlmTxWindowSize -
        inFlightBytes;
    canSend = canSendBytes >= payloadLength;

    if (LOG(channelId->DeviceId, LOG_TCP_FLOW_CONTROL)) {
      Log(channelId->DeviceId,
          "TCP window %s, ca%s send: payloadLength=%4u, "
          "inFlightBytes=%u, canSendBytes=%u-%u=%u, "
          "windowSize=%u\n",
          i == 0 ? "cached " : "updated",
          canSend ? "n" : "n't", payloadLength,
          inFlightBytes,
          pTxWindowFlowControl->EtlmTxWindowSize,
          inFlightBytes, canSendBytes,
          pTxWindowFlowControl->EtlmTxWindowSize);
    }

#ifdef DEBUG
    {
      // Alternative way of computing "canSend" (similar to
      // consumer flow control above):

      uint32_t consumedByteCount =
          pTxWindowFlowControl->CachedLastTcbSndUna;
      // Window fill byte count if we send this packet
      int64_t windowFillByteCount =
          pTxWindowFlowControl->SentBytesCount -
          consumedByteCount;
      windowFillByteCount += payloadLength;
      assert(windowFillByteCount >= 0);

      bool alternativeCanSend =
          windowFillByteCount <=
          pTxWindowFlowControl->EtlmTxWindowSize;

      if (LOG(channelId->DeviceId, LOG_TCP_FLOW_CONTROL)) {
        Log(channelId->DeviceId,
            "  TCP window %s, ca%s send: "
            "payloadLength=%4u, "
            "consumedByteCount=%u, new "
            "windowFillByteCount=%ld-%u+%u=%ld, "
            "windowSize=%u\n",
            i == 0 ? "cached " : "updated",
            alternativeCanSend ? "n" : "n't", payloadLength,
            consumedByteCount,
            txConsumedBytesFlowControl.SentBytesCount,
            consumedByteCount, payloadLength,
            windowFillByteCount,
            pTxWindowFlowControl->EtlmTxWindowSize);
      }

      assert(alternativeCanSend ==
             canSend); // Do they agree?
    }
#endif // DEBUG

    if (canSend) {
      break;
    }

    // Cannot send based on the cached TcbSndUna byte count
    // (no free room in TCP window). Now actually read the
    // register and update the cached value and retry:
    UpdateCachedConsumedBytes(channelId,
                              txConsumedBytesFlowControl,
                              pTxWindowFlowControl, false);
  }

  if (canSend) {
    // Book the free place in the buffer so nobody else
    // can get it. If other flow checks after this one
    // should fail then this bookings has to be cancelled.
    pTxWindowFlowControl->SentBytesCount += payloadLength;
    bookings = SendBookings(bookings | SEND_BOOKING_TAKEN |
                            SEND_BOOKING_TCP_WINDOW);
  }

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics;

      if (i == 0) {
        // Success on first round, cached value was used
        ++statistics.TcpWindow.CachedReads;
      } else {
        // Register read was necessary (i == 1 if can send
        // after reading register, i == 2 if can't send
        // after reading register)
        ++statistics.TcpWindow.RegisterReads;
      }

      if (canSend) {
        AddSendReason(statistics,
                      SCI_SEND_REASON_TCP_WINDOW_SUCCESS);
      } else {
        AddSendReason(statistics,
                      SCI_SEND_REASON_TCP_WINDOW_FULL);
      })

  if (LOG(channelId->DeviceId, LOG_TCP_FLOW_CONTROL)) {
    assert(pTxWindowFlowControl->SentBytesCount >=
           pTxWindowFlowControl->CachedLastTcbSndUna);
    uint32_t inFlightBytes =
        uint32_t(pTxWindowFlowControl->SentBytesCount -
                 pTxWindowFlowControl->CachedLastTcbSndUna);
    uint32_t canSendBytes =
        pTxWindowFlowControl->EtlmTxWindowSize -
        inFlightBytes;
    Log(channelId->DeviceId,
        "TCP TX flow control: canSend %s, inFlightBytes "
        "%u, canSendBytes %u\n",
        canSend ? "True" : "False", inFlightBytes,
        canSendBytes);
  }

  return canSend;
}

#if SUPPORT_TX_FLOW_CONTROL_STATISTICS == 1
static void
StatisticsSanityChecks(SC_ChannelId channelId,
                       const SendParameters &sendParameters, bool canSend /*, FlowControlOptions flowControlOptions*/) {
  SanityCheck(channelId);

  if (channelId != nullptr)
    return; // TODO

#ifdef DEBUG
  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics);
#endif

  if (channelId->ChannelType == SC_DMA_CHANNEL_TYPE_TCP) {
    if (sendParameters.Priority) {
      if (canSend) {
        assert(statistics.SendReason ==
               (SCI_SEND_REASON_TCP_SUCCESS |
                SCI_SEND_REASON_PRIORITY_FIFO));
      } else {
        assert(statistics.SendReason !=
               (SCI_SEND_REASON_TCP_SUCCESS |
                SCI_SEND_REASON_PRIORITY_FIFO));
      }
    } else {
      if (canSend) {
        assert(statistics.SendReason ==
               (SCI_SEND_REASON_TCP_SUCCESS |
                SCI_SEND_REASON_NORMAL_FIFO));
      } else {
        assert(statistics.SendReason !=
               (SCI_SEND_REASON_TCP_SUCCESS |
                SCI_SEND_REASON_NORMAL_FIFO));
      }
    }
  } else {
    // Sanity checks:
    if (sendParameters.Priority) {
      if (canSend) {
        assert(statistics.SendReason ==
               (SCI_SEND_REASON_SUCCESS |
                SCI_SEND_REASON_PRIORITY_FIFO));
      } else {
        assert(statistics.SendReason !=
               (SCI_SEND_REASON_SUCCESS |
                SCI_SEND_REASON_PRIORITY_FIFO));
      }
    } else {
      if (canSend) {
        assert(statistics.SendReason ==
               (SCI_SEND_REASON_SUCCESS |
                SCI_SEND_REASON_NORMAL_FIFO));
      } else {
        assert(statistics.SendReason !=
               (SCI_SEND_REASON_SUCCESS |
                SCI_SEND_REASON_NORMAL_FIFO));
      }
    }
  }
}
#endif // SUPPORT_TX_FLOW_CONTROL_STATISTICS == 1

static inline bool
CanSendOnThisChannel(SC_ChannelId channelId,
                     SendParameters &sendParameters,
                     FlowControlOptions flowControlOptions,
                     SendBookings &bookings) {
  ApiScopedTimer(API_TIMER_CAN_SEND_ON_THIS_CHANNEL);

#ifdef DEBUG
  UNUSED_ALWAYS(flowControlOptions);
  UNUSED_ALWAYS(bookings);
#endif

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics;
      statistics.SendReason = SCI_SEND_REASON_NONE;)

  bool canSend = true;

  // Check consumed bytes first because it is most likely be
  // the biggest bottle neck
  if ((flowControlOptions & FLOW_CONTROL_CONSUMED_BYTES) !=
          0
#ifdef DEBUG
      && (flowControlOptions &
          SC_SEND_DISABLE_CONSUMER_FLOW_CHECK) == 0
#endif
  ) {
    // Remember the precise original payload length before
    // it is rounded up
    uint32_t payloadLength =
        sendParameters.PayloadLengthRoundedUpTo32;

    canSend = FirmwareConsumedBytesFlowControl(
        channelId, *channelId->pTxConsumedBytesFlowControl,
        channelId->pTxWindowFlowControl, sendParameters,
        bookings);
    if (!canSend) {
      COLLECT_STATISTICS(StatisticsSanityChecks(
          channelId, sendParameters, canSend));
      return canSend; // Return immediately on failure
    }

    if (channelId->ChannelType == SC_DMA_CHANNEL_TYPE_TCP) {
      assert(channelId->pTxWindowFlowControl != nullptr);

      canSend = TcpWindowFlowControl(
          channelId, channelId->pTxWindowFlowControl,
          *channelId->pTxConsumedBytesFlowControl,
          payloadLength, bookings);
      if (!canSend) {
        COLLECT_STATISTICS(StatisticsSanityChecks(
            channelId, sendParameters, canSend));
        return canSend; // Return immediately on failure
      }
    }
  }

  // Next check software counters on Tx DMA FIFO, most
  // likely second bottle neck
  if ((flowControlOptions & FLOW_CONTROL_SW_TX_DMA_FIFO) !=
      0) {
    canSend = TxDmaSoftwareFlowControl(
        channelId, sendParameters, bookings);
    if (!canSend) {
      COLLECT_STATISTICS(StatisticsSanityChecks(
          channelId, sendParameters, canSend));
      return canSend; // Return immediately on failure
    }
  }

  // Lastly check firmware Tx DMA FIFO counter, least likely
  // to be a bottle neck
  if ((flowControlOptions & FLOW_CONTROL_FW_TX_DMA_FIFO) !=
      0) {
    canSend = TxDmaFirmwareFlowControl(
        channelId, sendParameters, bookings);
    if (!canSend) {
      COLLECT_STATISTICS(StatisticsSanityChecks(
          channelId, sendParameters, canSend));
      return canSend; // Return immediately on failure
    }
  }

  COLLECT_STATISTICS(StatisticsSanityChecks(
      channelId, sendParameters, canSend));

  if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
    Log(channelId->DeviceId,
        "CanSendOnThisChannel returns canSend = %s\n",
        canSend ? "true" : "false");
  }

  return canSend;
}

SC_Error SCI_GetTxFlowControlStatistics(
    SC_ChannelId channelId,
    SC_TxFlowControlStatistics *pTxFlowControlStatistics,
    void *pStatistics) {
  SanityCheck(channelId);

  if (pTxFlowControlStatistics != nullptr) {
    *pTxFlowControlStatistics =
        channelId->TxFlowControlStatistics;
  }

  UNUSED_ALWAYS(pStatistics);
  UNUSED_ALWAYS(channelId);
  COLLECT_STATISTICS(if (pStatistics != nullptr) {
    *reinterpret_cast<std::vector<Statistics> *>(
        pStatistics) = channelId->AllStatistics;
  });

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

static volatile int g_OpenDeviceCount = 0;

static SC_Error DeallocateChannel(SC_ChannelId channelId);

static SC_Error CloseDevice(SC_DeviceId deviceId) {
  SC_Error errorCode = SC_ERR_SUCCESS;
  if (deviceId == nullptr) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "cannot close null device context!");
  }

  SanityCheck(deviceId);

  const std::size_t numberOfChannels =
      ARRAY_SIZE(deviceId->Channels);
  for (std::size_t i = 0; i < numberOfChannels; ++i) {
    SC_ChannelId channelId = deviceId->Channels[i];
    if (channelId != nullptr) {
      SC_Error channelErrorCode =
          DeallocateChannel(channelId);
      if (channelErrorCode != SC_ERR_SUCCESS &&
          errorCode == SC_ERR_SUCCESS) {
        errorCode = channelErrorCode;
      }
    }
  }

  if (deviceId->pUserLogicRegisters != nullptr) {
    if (munmap(const_cast<uint64_t *>(
                   deviceId->pUserLogicRegisters),
               BAR2_REGS_SIZE) != 0) {
      errorCode = CallErrorHandlerDontSetLastError(
          SC_ERR_FAILED_TO_CLOSE_DEVICE,
          "failed to munmap device BAR 2 user logic "
          "registers with errno %d",
          errno);
    }
    deviceId->pUserLogicRegisters = nullptr;
  }

  if (deviceId->pBar0Registers != nullptr) {
    if (munmap(deviceId->pBar0Registers, REGS_FULL_SIZE) !=
        0) {
      errorCode = CallErrorHandlerDontSetLastError(
          SC_ERR_FAILED_TO_CLOSE_DEVICE,
          "failed to munmap BAR 0 device %p registers at "
          "%p with errno %d",
          static_cast<void *>(deviceId),
          deviceId->pBar0Registers, errno);
    }
    deviceId->pBar0Registers = nullptr;
  }

  if (deviceId->FileDescriptor >= 0) {
    if (close(deviceId->FileDescriptor) != 0) {
      errorCode = CallErrorHandlerDontSetLastError(
          SC_ERR_FAILED_TO_CLOSE_DEVICE,
          "failed to close device %p file descriptor %d "
          "with errno %d",
          static_cast<void *>(deviceId),
          deviceId->FileDescriptor, errno);
    }
    deviceId->FileDescriptor = -1;
  }
  if (__sync_sub_and_fetch(&g_OpenDeviceCount, 1) == 0) {
    // No more devices open in this process. Close shared
    // memory if it is open:
    if (g_EtlmFlowControlSharedMemory.IsOpen()) {
      try {
        g_EtlmFlowControlSharedMemory.Close();
      } catch (std::exception &ex) {
        errorCode = CallErrorHandlerDontSetLastError(
            SC_ERR_SHARED_MEMORY_FAILED,
            "sharedMemory::Close failed: %s", ex.what());
      }

      if (LOG(deviceId, LOG_SHARED_MEMORY)) {
        Log(deviceId,
            "ETLM Tx flow control shared memory close:\n%s",
            deviceId->SharedMemoryLogStream.str().c_str());
      }
    }
  }

  delete deviceId;

  return errorCode;
}

SC_Error SC_CloseDevice(SC_DeviceId deviceId) {
  return SetLastErrorCode(CloseDevice(deviceId));
}

static inline void RequestHasbeenProcessedByTxDmaFifo(
    TxDmaFifoFlowControl &flowControl) {
  // Update TX DMA flow control:
  assert(flowControl.FillLevel > 0);
  __sync_sub_and_fetch(&flowControl.FillLevel, 1);
}

static inline void InitializeTxDmaFlowControl(
    TxDmaFifoFlowControl *pFlowControl,
    uint16_t fillLevelLimit) {
  pFlowControl->FillLevel = 0;
  pFlowControl->FillLevelLimit = fillLevelLimit;
}

static void InitializeTxConsumedBytesFlowControl(
    volatile SC_TxConsumedBytesFlowControl
        *pTxConsumedBytesFlowControl) {
  pTxConsumedBytesFlowControl
      ->CachedLastConsumedBytesCount = 0;
  pTxConsumedBytesFlowControl->PreviousConsumedBytesCount =
      0;
  pTxConsumedBytesFlowControl->SentBytesCount = 0;

  pTxConsumedBytesFlowControl->BufferSize =
      TxConsumedBytesBufferSize;
  pTxConsumedBytesFlowControl->ListFlushLimit =
      TxConsumedBytesBufferSize / 2;

  // Per default per channel flow control does not need a
  // consumed bytes lock, only TCP channels share a common
  // consumed channels flow control and need a lock.
  pTxConsumedBytesFlowControl->pLock = nullptr;
}

static SC_DeviceOptions *
HandlDeviceOptions(SC_DeviceId deviceId,
                   SC_DeviceOptions *pDeviceOptions) {
  if (pDeviceOptions == nullptr) {
    pDeviceOptions =
        &ApiLibraryStaticInitializer::GetInstance()
             .DefaultDeviceOptions.DeviceOptions;
  }

  SC_Error errorCode =
      ApiLibraryStaticInitializer::GetInstance()
          .SanityCheckOptions(deviceId, pDeviceOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return nullptr;
  }

  return pDeviceOptions;
}

// Bitwise-SWAR algorithm:
static uint32_t NumberOfSetBits(uint32_t x) {
  uint32_t numberOfSetBits = 0;
  while (x != 0) {
    if ((x & 1) == 1) {
      ++numberOfSetBits;
    }
    x >>= 1;
  }
  return numberOfSetBits;
}

SC_DeviceId
SC_OpenDevice(const char *deviceName,
              SC_DeviceOptions *pDeviceOptions) {
  SC_DeviceId deviceId = nullptr;

  try {
    std::string deviceNameString =
        GetDeviceName(deviceName);
    deviceName = deviceNameString.c_str();

    int fd = open(deviceName, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
      CallErrorHandler(
          SC_ERR_FAILED_TO_OPEN_DEVICE,
          "failed to open device %s, errno = %d",
          deviceName, errno);
      return nullptr;
    }

    deviceId = new SC_DeviceContext(deviceName, fd);
    if (deviceId == nullptr) {
      close(fd);
      CallErrorHandler(
          SC_ERR_OUT_OF_MEMORY,
          "failed to allocate device context, errno = %d",
          errno);
      return nullptr;
    }
    if (deviceId->GetConstructorErrorCode() !=
        SC_ERR_SUCCESS) {
      CloseDevice(deviceId);
      CallErrorHandler(
          deviceId->GetConstructorErrorCode(),
          "failed to construct device context, errno = %d",
          errno);
      return nullptr;
    }

    SC_Error errorCode = SC_GetCardInfo(
        deviceId, &deviceId->InternalCardInfo.CardInfo);
    if (errorCode != SC_ERR_SUCCESS) {
      CloseDevice(deviceId);
      CallErrorHandler(
          SC_ERR_FAILED_TO_GET_CARD_INFO,
          "SC_GetCardInfo failed with error code %d",
          errorCode);
      return nullptr;
    }

    {
      sc_misc_t misc;
      misc.command = sc_misc_t::SC_MISC_SET_ARP_MODE;
      misc.value1 =
          ApiLibraryStaticInitializer::GetConstInstance()
                  .LibraryARP
              ? 1
              : 0;
      if (ioctl(fd, SC_IOCTL_MISC, &misc) != 0) {
        CloseDevice(deviceId);
        CallErrorHandler(SC_ERR_ARP_FAILED,
                         "failed to initialize driver ARP "
                         "mode for device %s, errno %d",
                         deviceName, errno);
        return nullptr;
      }
    }
    if (SC_GetUserLogicInterruptOptions(
            nullptr,
            &deviceId->InternalUserLogicInterruptOptions
                 .UserLogicInterruptOptions) !=
        SC_ERR_SUCCESS) {
      CloseDevice(deviceId);
      CallErrorHandler(
          SC_ERR_SHARED_MEMORY_FAILED,
          "SC_GetUserLogicInterruptOptions failed");
      return nullptr;
    }

    if ((pDeviceOptions = HandlDeviceOptions(
             deviceId, pDeviceOptions)) == nullptr) {
      CloseDevice(deviceId);
      return nullptr;
    }
    deviceId->InternalDeviceOptions.DeviceOptions =
        *pDeviceOptions;

    deviceId->pBar0Registers = mmap(
        nullptr, REGS_FULL_SIZE, PROT_READ | PROT_WRITE,
        MAP_SHARED, deviceId->FileDescriptor, 0);
    if (deviceId->pBar0Registers == MAP_FAILED) {
      deviceId->pBar0Registers = nullptr;
      CloseDevice(deviceId);
      CallErrorHandler(
          SC_ERR_MMAP_OF_FPGA_REGISTERS_FAILED,
          "failed to mmap FPGA BAR 0 registers to user "
          "space, errno = %d",
          errno);
      return nullptr;
    }
    deviceId->pPointerListDMA =
        reinterpret_cast<sc_pldma_t *>(
            static_cast<uint8_t *>(
                deviceId->pBar0Registers) +
            PL_DMA_OFFSET);
    deviceId->pStatusDma =
        reinterpret_cast<sc_status_dma_t *>(
            static_cast<uint8_t *>(
                deviceId->pBar0Registers) +
            STATUS_DMA_OFFSET);

    // This deviates a bit from the other mappings: use
    // length parameter 2 * BAR2_REGS_SIZE as a "code" that
    // is used in driver mmap function to signify that we
    // are mapping BAR 2. ELE TODO: should rewrite all
    // mapping code to use the same principle!
    deviceId->pUserLogicRegisters =
        static_cast<volatile uint64_t *>(
            mmap(nullptr, 2 * BAR2_REGS_SIZE,
                 PROT_READ | PROT_WRITE, MAP_SHARED,
                 deviceId->FileDescriptor, 0));
    if (deviceId->pUserLogicRegisters == MAP_FAILED) {
      deviceId->pUserLogicRegisters = nullptr;
      CloseDevice(deviceId);
      CallErrorHandler(
          SC_ERR_MMAP_OF_FPGA_REGISTERS_FAILED,
          "failed to mmap FPGA BAR 2 user logic registers "
          "to user space, errno = %d",
          errno);
      return nullptr;
    }
    // start of ekaline code
    deviceId->pEkalineWcBase =
        static_cast<volatile uint64_t *>(
            mmap(nullptr, EKA_WC_REGION_SIZE,
                 PROT_READ | PROT_WRITE, MAP_SHARED,
                 deviceId->FileDescriptor, 0));
    //        deviceId->pEkalineWcBase =
    //        static_cast<volatile uint64_t
    //        *>(mmap(deviceId->pBar0Registers + 0x8000,
    //        0x8000, PROT_READ | PROT_WRITE, MAP_SHARED,
    //        deviceId->FileDescriptor, 0));
    if (deviceId->pEkalineWcBase == MAP_FAILED) {
      deviceId->pEkalineWcBase = nullptr;
      CloseDevice(deviceId);
      CallErrorHandler(
          SC_ERR_MMAP_OF_FPGA_REGISTERS_FAILED,
          "failed to mmap FPGA BAR 0 EkalineWcBase to user "
          "space, errno = %d",
          errno);
      return nullptr;
    }
    //	fprintf (stderr,"deviceId->pEkalineWcBase =
    //%p\n",deviceId->pEkalineWcBase);
    // end of ekaline code

    // Continue Tx DMA in spite of panic (no guarantees
    // however that anything useful happens!): Only used for
    // debugging purposes. DO NOT ENABLE IN PRODUCTION CODE!
    // SCI_SetGlobalPanicState(deviceId,
    // /*disableGlobalPanic:*/true);

    if (LOG(deviceId, LOG_SETUP)) {
      const uint8_t *pRegs =
          reinterpret_cast<const uint8_t *>(
              deviceId->pBar0Registers);
      Log(deviceId,
          "FPGA BAR 0 registers mapped to:                 "
          "   [%p-%p), length 0x%x\n",
          static_cast<const void *>(pRegs),
          static_cast<const void *>(pRegs + BAR0_REGS_SIZE),
          BAR0_REGS_SIZE);

      const volatile uint8_t *pUserLogicRegisters =
          reinterpret_cast<volatile uint8_t *>(
              deviceId->pUserLogicRegisters);
      Log(deviceId,
          "FPGA BAR 2 user logic registers mapped to:      "
          "   [%p-%p), length 0x%x\n",
          static_cast<const volatile void *>(
              pUserLogicRegisters),
          static_cast<const volatile void *>(
              pUserLogicRegisters + BAR2_REGS_SIZE),
          BAR2_REGS_SIZE);

      const uint8_t *pPlDMA =
          reinterpret_cast<const uint8_t *>(
              deviceId->pPointerListDMA);
      Log(deviceId,
          "PL DMA mapped to:                               "
          "   [%p-%p), length 0x%x\n",
          static_cast<const void *>(pPlDMA),
          static_cast<const void *>(pPlDMA + PL_DMA_SIZE),
          PL_DMA_SIZE);

      const uint8_t *pStatusDMA =
          reinterpret_cast<const uint8_t *>(
              deviceId->pStatusDma);
      Log(deviceId,
          "Status DMA mapped to:                           "
          "   [%p-%p), length 0x%x\n",
          static_cast<const void *>(pStatusDMA),
          static_cast<const void *>(pStatusDMA +
                                    STATUS_DMA_SIZE),
          STATUS_DMA_SIZE);
    }

    // Initialize ETLM 16 KB consumed bytes buffer shared
    // flow control:
    InitializeTxConsumedBytesFlowControl(
        &deviceId->TcpTxConsumedBytesFlowControl);

    deviceId->pTxDmaRequestFifoFillLevelsRegister =
        SC_GetFpgaRegistersBase(deviceId) +
        TX_DMA_FIFO_FILL_LEVEL / 8;

    // Take into account that NICs in the driver use some
    // entries too
    deviceId->MaxNormalFifoConcurrency = static_cast<
        decltype(deviceId->MaxNormalFifoConcurrency)>(
        pDeviceOptions->MaxTxDmaNormalFifoConcurrency +
        deviceId->InternalCardInfo.CardInfo.NumberOfNICs *
            FB_FIFO_ENTRIES_PER_CHANNEL);
    deviceId->MaxPriorityFifoConcurrency =
        pDeviceOptions->MaxTxDmaPriorityFifoConcurrency;

    const ApiLibraryStaticInitializer &staticInitializer =
        ApiLibraryStaticInitializer::GetConstInstance();

    int openDeviceCount =
        __sync_add_and_fetch(&g_OpenDeviceCount, 1);
    if (openDeviceCount == 1 &&
        staticInitializer.LibraryOptions.LibraryOptions
            .SharedMemoryFlowControlOptions.Enabled) {
      // First call to SC_OpenDevice in this process
      // initializes the share memory:

      std::stringstream *pLog = nullptr;
      if (LOG(deviceId, LOG_SHARED_MEMORY)) {
        pLog = &deviceId->SharedMemoryLogStream;
      }

      try {
        g_EtlmFlowControlSharedMemory.Open(
            staticInitializer.LibraryOptions.LibraryOptions
                .SharedMemoryFlowControlOptions.FileName,
            staticInitializer.LibraryOptions.LibraryOptions
                .SharedMemoryFlowControlOptions.Permissions,
            pLog);
      } catch (std::exception &ex) {
        CloseDevice(deviceId);
        CallErrorHandler(SC_ERR_SHARED_MEMORY_FAILED,
                         "SharedMemory::Open failed: %s",
                         ex.what());
        return nullptr;
      }

      if (LOG(deviceId, LOG_SHARED_MEMORY)) {
        Log(deviceId,
            "ETLM Tx flow control shared memory open:\n%s",
            pLog->str().c_str());
        pLog->clear();
      }
    }

    errorCode = SC_GetCardInfo(
        deviceId, &deviceId->InternalCardInfo.CardInfo);
    if (errorCode != SC_ERR_SUCCESS) {
      CloseDevice(deviceId);
      CallErrorHandler(
          SC_ERR_FAILED_TO_GET_CARD_INFO,
          "SC_GetCardInfo failed with error code %d",
          errorCode);
      return nullptr;
    }

    if (LOG(deviceId, LOG_SETUP)) {
      Log(deviceId, "PCIe BAR0 = %p, BAR2 = %p\n",
          static_cast<volatile void *>(
              SC_GetFpgaRegistersBase(deviceId)),
          static_cast<volatile void *>(
              deviceId->pUserLogicRegisters));
      Log(deviceId, "TX DMA FIFO fills register: %p\n",
          static_cast<volatile void *>(
              deviceId
                  ->pTxDmaRequestFifoFillLevelsRegister));
    }

    SetLastErrorCode(
        SC_ERR_SUCCESS); // If we get here, all is well

    return deviceId;
  } catch (SC_Exception &) {
    if (deviceId != nullptr) {
      CloseDevice(deviceId);
    }
    throw;
  } catch (std::bad_alloc &ex) {
    if (deviceId != nullptr) {
      CloseDevice(deviceId);
    }
    CallErrorHandler(SC_ERR_OUT_OF_MEMORY,
                     "out of memory exception caught: %s",
                     ex.what());
    return nullptr;
  } catch (std::exception &ex) {
    if (deviceId != nullptr) {
      CloseDevice(deviceId);
    }
    CallErrorHandler(SC_ERR_FAILED_TO_OPEN_DEVICE,
                     "unexpected exception caught: %s",
                     ex.what());
    return nullptr;
  }
}

static sc_bucket *
GetUserSpaceMappedDriverBucket(SC_ChannelId channelId,
                               std::size_t bucketNumber) {
  const std::size_t numberOfBuckets =
      ARRAY_SIZE(channelId->Buckets);
  if (unlikely(bucketNumber >= numberOfBuckets)) {
    return nullptr;
  }
  return reinterpret_cast<sc_bucket *>(
      static_cast<uint8_t *>(channelId->pTotalMemory) +
      BUCKET_OFFSET(bucketNumber));
}

static bool
CheckCumulativeFifoLimitBookings(SC_DeviceId deviceId) {
  uint32_t cumulativeNormalFifoLimitBookings = 0;
  uint32_t cumulativePriorityFifoLimitBookings = 0;

  const std::size_t numberOfChannels =
      ARRAY_SIZE(deviceId->Channels);
  for (std::size_t i = 0; i < numberOfChannels; ++i) {
    SC_ChannelId channelId = deviceId->Channels[i];
    if (channelId != nullptr) {
      cumulativeNormalFifoLimitBookings +=
          channelId->TxDmaNormalFifoFlowControl
              .FillLevelLimit;
      cumulativePriorityFifoLimitBookings +=
          channelId->TxDmaPriorityFifoFlowControl
              .FillLevelLimit;
    }
  }

  return (cumulativeNormalFifoLimitBookings <=
          MAX_FIFO_ENTRIES) &&
         (cumulativePriorityFifoLimitBookings <=
          MAX_FIFO_ENTRIES);
}

static SC_Error DeallocateChannel(SC_ChannelId channelId) {
  SC_Error errorCode = SC_ERR_SUCCESS;

  if (channelId == nullptr) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "parameter '%s' cannot be NULL!",
        nameof(channelId));
  }

  SC_DeviceId deviceId = channelId->DeviceId;
  if (deviceId == nullptr) {
    errorCode = CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "channel device id cannot be NULL!");
  }

  if (channelId->pPRB != nullptr) {
    if (munmap(channelId->pPRB, channelId->PrbSize) != 0) {
      SC_Error munmapErrorCode =
          CallErrorHandlerDontSetLastError(
              SC_ERR_CHANNEL_DEALLOCATION_FAILED,
              "failed to munmap channel %u PRB, address %p "
              "length 0x%lX, errno %d",
              channelId->DmaChannelNumber, channelId->pPRB,
              channelId->PrbSize, errno);
      if (errorCode == SC_ERR_SUCCESS) {
        errorCode = munmapErrorCode;
      }
    }
    channelId->pPRB = nullptr;
  }

  if (channelId->pTotalMemory != nullptr) {
    if (munmap(channelId->pTotalMemory, MMAP_FULL_SIZE) !=
        0) {
      SC_Error munmapErrorCode =
          CallErrorHandlerDontSetLastError(
              SC_ERR_CHANNEL_DEALLOCATION_FAILED,
              "failed to munmap channel %u memory mapping, "
              "address %p, errno %d",
              channelId->DmaChannelNumber,
              channelId->pTotalMemory, errno);
      if (errorCode == SC_ERR_SUCCESS) {
        errorCode = munmapErrorCode;
      }
    }
    channelId->pTotalMemory = nullptr;
  }

  /*
     Current device driver does not support explicit
     deallocation of channels. Channel is
     released/deallocated when the corresponding file
     descriptor is closed.
  */

  if (channelId->FileDescriptor >= 0) {
    if (close(channelId->FileDescriptor) != 0) {
      SC_Error closeErrorCode =
          CallErrorHandlerDontSetLastError(
              SC_ERR_CHANNEL_DEALLOCATION_FAILED,
              "failed to close channel file, errno %d",
              errno);
      if (errorCode == SC_ERR_SUCCESS) {
        errorCode = closeErrorCode;
      }
    }
    channelId->FileDescriptor = -1;
  }

  // Delete channel from device context
  SC_Error deleteChannelErrorCode =
      DeleteChannel(channelId);

  if (errorCode != SC_ERR_SUCCESS) {
    return errorCode;
  }

  return deleteChannelErrorCode;
}

SC_Error SC_DeallocateChannel(SC_ChannelId channelId) {
  return SetLastErrorCode(DeallocateChannel(channelId));
}

int16_t SC_GetNumberOfUserLogicChannels() {
  SetLastErrorCode(SC_ERR_SUCCESS);

  return SC_MAX_ULOGIC_CHANNELS;
}

int SC_GetChannelFileDescriptor(SC_DeviceId deviceId,
                                SC_ChannelId channelId) {
  for (std::size_t channelIndex = 0;
       channelIndex < ARRAY_SIZE(deviceId->Channels);
       ++channelIndex) {
    if (deviceId->Channels[channelIndex] == channelId) {
      SetLastErrorCode(SC_ERR_SUCCESS);

      return channelId->FileDescriptor;
    }
  }

  return CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "channel id %p is not a valid allocated channel id "
      "on this device",
      static_cast<void *>(channelId));
}

ssize_t SCI_GetChannelMapSizeAndOffsets(
    SC_DeviceId deviceId, SC_ChannelId channelId,
    uint64_t *pPrbOffset, uint64_t *pPlDmaOffset,
    uint64_t *pBucketsOffset) {
  for (std::size_t channelIndex = 0;
       channelIndex < ARRAY_SIZE(deviceId->Channels);
       ++channelIndex) {
    if (deviceId->Channels[channelIndex] == channelId) {
      SetLastErrorCode(SC_ERR_SUCCESS);

      if (pPrbOffset != nullptr) {
        *pPrbOffset = RECV_DMA_OFFSET;
      }
      if (pPlDmaOffset != nullptr) {
        *pPlDmaOffset = PL_DMA_OFFSET;
      }
      if (pBucketsOffset != nullptr) {
        *pBucketsOffset = BUCKET_OFFSET(0);
      }
      return MMAP_FULL_SIZE;
    }
  }

  return CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "channel id %p is not a valid allocated channel id "
      "on this device",
      static_cast<void *>(channelId));
}

void SCI_DumpChannelOptions(
    const SCI_IPrint &print,
    const SC_ChannelOptions *pChannelOptions,
    const char *context) {
  print.Print("Channel options at %s:\n", context);
  print.Print(
      "    UseTxDmaNormalFifoFirmwareFillLevel     = %d\n",
      pChannelOptions->UseTxDmaNormalFifoFirmwareFillLevel);
  print.Print(
      "    TxDmaNormalFifoSoftwareFillLevelLimit   = %u\n",
      pChannelOptions
          ->TxDmaNormalFifoSoftwareFillLevelLimit);
  print.Print(
      "    UseTxDmaPriorityFifoFirmwareFillLevel   = %d\n",
      pChannelOptions
          ->UseTxDmaPriorityFifoFirmwareFillLevel);
  print.Print(
      "    TxDmaPriorityFifoSoftwareFillLevelLimit = %u\n",
      pChannelOptions
          ->TxDmaPriorityFifoSoftwareFillLevelLimit);
  print.Print(
      "    DoNotUseRxDMA                           = %d\n",
      pChannelOptions->DoNotUseRxDMA);
}

/** Common code for allocating channels
 *
 *  @param pDeviceContext       Device context
 *  @param networkInterface     Network interface in the
 * physical port.
 *  @param dmaChannelNumber     Instruct FPGA to use
 * specific TCP channel. Use the value -1 to let the driver
 * assign the channel.
 *  @param channelType          Type of channel to allocate
 *  @param enableDma            Should the host memory dma
 * be enabled.
 *  @param pChannelOptions      Pointer to channel options
 * or null to use default options.
 *
 *  @return nullptr on success, non-zero channel id
 * otherwise.
 */
static SC_ChannelId AllocateChannel(
    SC_DeviceId deviceId, uint8_t networkInterface,
    int16_t dmaChannelNumber, sc_channel_type_t channelType,
    uint8_t enableDma, SC_ChannelOptions *pChannelOptions) {
  SC_ChannelId channelId = nullptr;

  try {
    SanityCheck(deviceId);

    int fileDescriptor = open(deviceId->DeviceName.c_str(),
                              O_RDWR | O_NONBLOCK);
    if (fileDescriptor < 0) {
      CallErrorHandler(SC_ERR_CHANNEL_ALLOCATION_FAILED,
                       "failed to open device %s for a "
                       "channel, errno = %d",
                       deviceId->DeviceName.c_str(), errno);
      return nullptr;
    }

    channelId = new SC_ChannelContext(fileDescriptor);
    if (channelId == nullptr) {
      CallErrorHandler(
          SC_ERR_OUT_OF_MEMORY,
          "failed to allocate channel context, errno = %d",
          errno);
      return nullptr;
    }

    // These are needed in DeallocateChannel()
    channelId->DeviceId = deviceId;
    channelId->DmaChannelNumber = FIND_FREE_DMA_CHANNEL;

    if (channelId->GetConstructorErrorCode() !=
        SC_ERR_SUCCESS) {
      DeallocateChannel(channelId);
      CallErrorHandler(
          channelId->GetConstructorErrorCode(),
          "failed to construct channel context, errno = %d",
          errno);
      return nullptr;
    }

    if (pChannelOptions == nullptr) {
      SC_Error errorCode = SC_GetChannelOptions(
          nullptr, &channelId->InternalChannelOptions
                        .ChannelOptions);
      if (errorCode != SC_ERR_SUCCESS) {
        DeallocateChannel(channelId);
        // Error code set and error handler already called
        // by SC_GetChannelOptions!
        return nullptr;
      }
      pChannelOptions =
          &channelId->InternalChannelOptions
               .ChannelOptions; // In case pChannelOptions
                                // was nullptr; now it isn't
    } else {
      SC_Error errorCode =
          SC_SetChannelOptions(channelId, pChannelOptions);
      if (errorCode != SC_ERR_SUCCESS) {
        DeallocateChannel(channelId);
        // Error code set and error handler already called
        // by SC_SetChannelOptions!
        return nullptr;
      }
    }
    if (channelType == SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
      // For UDP channels use pChannelOptions->DoNotUseRxDMA
      enableDma = !pChannelOptions->DoNotUseRxDMA;
    } else if (channelType !=
                   SC_DMA_CHANNEL_TYPE_USER_LOGIC &&
               pChannelOptions->DoNotUseRxDMA) {
      DeallocateChannel(channelId);
      CallErrorHandler(
          SC_ERR_INVALID_ARGUMENT,
          "SC_ChannelOptions::DoNotUseRxDMA is only "
          "supported for UDP channels, "
          "it must be set to false for all other channel "
          "types.");
      return nullptr;
    }

    SC_Error errorCode = SC_GetConnectOptions(
        nullptr,
        &channelId->InternalConnectOptions.ConnectOptions);
    if (errorCode != SC_ERR_SUCCESS) {
      DeallocateChannel(channelId);
      // Error code set and error handler already called by
      // SC_GetConnectOptions!
      return nullptr;
    }
    errorCode = SC_GetListenOptions(
        nullptr,
        &channelId->InternalListenOptions.ListenOptions);
    if (errorCode != SC_ERR_SUCCESS) {
      DeallocateChannel(channelId);
      // Error code set and error handler already called by
      // SC_GetListenOptions!
      return nullptr;
    }
    errorCode = SC_GetIgmpOptions(
        nullptr,
        &channelId->InternalIgmpOptions.IgmpOptions);
    if (errorCode != SC_ERR_SUCCESS) {
      DeallocateChannel(channelId);
      // Error code set and error handler already called by
      // SC_GetIgmpOptions!
      return nullptr;
    }

    // Tell driver to allocate a channel
    sc_alloc_info_t alloc_info;
    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.type = channelType;
    alloc_info.dma_channel_no = dmaChannelNumber;
    alloc_info.network_interface = networkInterface;
    alloc_info.enableDma = enableDma;

    if (ioctl(channelId->FileDescriptor,
              SC_IOCTL_ALLOC_CHANNEL, &alloc_info) != 0) {
      DeallocateChannel(channelId);
      CallErrorHandler(
          SC_ERR_CHANNEL_ALLOCATION_FAILED,
          "ioctl(SC_IOCTL_ALLOC_CHANNEL, ...) failed for "
          "%s DMA channel %d with errno %d",
          SCI_GetChannelTypeName(channelType),
          dmaChannelNumber, errno);
      return nullptr;
    }

    channelId->PrbSize = alloc_info.prb_size;
    dmaChannelNumber = channelId
                           ->DmaChannelNumber = int16_t(
        alloc_info.dma_channel_no); // TODO: change
                                    // alloc_info.channel_no
                                    // to int16_t
    {
      ScopedTinyThreadLock lock(deviceId->ChannelsLock);

      if (deviceId->Channels[channelId->DmaChannelNumber] ==
          nullptr) {
        deviceId->Channels[channelId->DmaChannelNumber] =
            channelId;
      } else {
        DeallocateChannel(channelId);
        CallErrorHandler(
            SC_ERR_CHANNEL_ALLOCATION_FAILED,
            "channel number %u already occupied!",
            channelId->DmaChannelNumber);
        return nullptr;
      }
    }

    // Map pldma, statusdma, buckets and ring buffer
    channelId->pTotalMemory = mmap(
        nullptr, MMAP_FULL_SIZE, PROT_READ | PROT_WRITE,
        MAP_SHARED, channelId->FileDescriptor, 0);
    if (channelId->pTotalMemory == MAP_FAILED) {
      channelId->pTotalMemory = nullptr;
      DeallocateChannel(channelId);
      CallErrorHandler(SC_ERR_MMAP_OF_BUFFERS_FAILED,
                       "mmap failed with errno %d", errno);
      return nullptr;
    }

    if (deviceId->InternalCardInfo.CardInfo.FpgaInfo
            .MMU_IsEnabled) {
      channelId->pPRB = static_cast<uint8_t *>(
          mmap(nullptr, channelId->PrbSize,
               PROT_READ | PROT_WRITE, MAP_SHARED,
               channelId->FileDescriptor,
               SC_MMAP_ALLOCATE_PRB_OFFSET * PAGE_SIZE));
      if (channelId->pPRB == MAP_FAILED) {
        channelId->pPRB = nullptr;
        DeallocateChannel(channelId);
        CallErrorHandler(SC_ERR_MMAP_OF_BUFFERS_FAILED,
                         "mmap of PRB failed with errno %d",
                         errno);
        return nullptr;
      }
    } else {
      channelId->pPRB =
          static_cast<uint8_t *>(channelId->pTotalMemory) +
          RECV_DMA_OFFSET;
      channelId->PrbSize = RECV_DMA_OFFSET;
    }

    SC_DmaChannelInfo &dmaChannelInfo =
        const_cast<SC_DmaChannelInfo &>(
            deviceId->InternalCardInfo.CardInfo
                .DmaChannels[dmaChannelNumber]);
    dmaChannelInfo.PacketRingBuffer
        .UserSpaceVirtualAddress =
        reinterpret_cast<uint64_t>(channelId->pPRB);
    const SC_Memory *pMemoryBlocks =
        dmaChannelInfo.PacketRingBuffer.MemoryBlocks;
    if (pMemoryBlocks != nullptr &&
        dmaChannelInfo.PacketRingBuffer
                .NumberOfMemoryBlocks > 0) {
      channelId->PrbFpgaStartAddress =
          pMemoryBlocks[0].FpgaDmaStartAddress;
    } else {
      DeallocateChannel(channelId);
      CallErrorHandler(
          SC_ERR_MMAP_OF_BUFFERS_FAILED,
          "No FPGA DMA start address available for DMA "
          "channel %u, "
          "pMemoryBlocks = 0x%lX NumberOfMemoryBlocks = %u",
          dmaChannelNumber,
          reinterpret_cast<uint64_t>(pMemoryBlocks),
          dmaChannelInfo.PacketRingBuffer
              .NumberOfMemoryBlocks);
      return nullptr;
    }

    channelId->pReadMarkerRegister =
        reinterpret_cast<uint64_t *>(
            reinterpret_cast<uint8_t *>(
                deviceId->pBar0Registers) +
            FB_RXDMA_REG(channelId->DmaChannelNumber,
                         PKG_RXDMA_RD_MARK));

    // Why is this mapped into every channel context? There
    // is only one PL DMA!
    channelId->pPointerListDMA =
        reinterpret_cast<sc_pldma_t *>(
            static_cast<uint8_t *>(
                channelId->pTotalMemory) +
            PL_DMA_OFFSET);
    if (LOG(deviceId, LOG_SETUP)) {
      Log(deviceId,
          "  Channel %3u: mem mapped to:                   "
          "   [%p-%p), length 0x%x\n",
          channelId->DmaChannelNumber,
          channelId->pTotalMemory,
          static_cast<const void *>(
              reinterpret_cast<const uint8_t *>(
                  channelId->pTotalMemory) +
              MMAP_FULL_SIZE),
          MMAP_FULL_SIZE);

      const uint8_t *pPL_DMA =
          reinterpret_cast<const uint8_t *>(
              channelId->pPointerListDMA);
      Log(deviceId,
          "               PL DMA mapped to:                "
          "   [%p-%p), length 0x%x\n",
          static_cast<const void *>(pPL_DMA),
          static_cast<const void *>(pPL_DMA + PL_DMA_SIZE),
          PL_DMA_SIZE);

      const uint8_t *pBuckets =
          reinterpret_cast<const uint8_t *>(
              GetUserSpaceMappedDriverBucket(channelId, 0));
      Log(deviceId,
          "               Tx DMA Buckets mapped to:        "
          "   [%p-%p), length 0x%x\n",
          static_cast<const void *>(pBuckets),
          static_cast<const void *>(pBuckets +
                                    BUCKETS_SIZE),
          BUCKETS_SIZE);

      const uint8_t *pPRB = channelId->pPRB;
      Log(deviceId,
          "               Packet Ring Buffer (PRB) mapped "
          "to: [%p-%p), length 0x%x\n",
          static_cast<const void *>(pPRB),
          static_cast<const void *>(pPRB + RECV_DMA_SIZE),
          RECV_DMA_SIZE);
    }

    // Initialize transmit buckets
    const std::size_t numberOfBuckets =
        ARRAY_SIZE(channelId->Buckets);
    for (std::size_t i = 0; i < numberOfBuckets; ++i) {
      SC_Bucket &bucket = channelId->Buckets[i];
      bucket.BucketPhysicalAddress = i;
      if (ioctl(channelId->FileDescriptor,
                SC_IOCTL_GET_BUCKET_HWADDR,
                &bucket.BucketPhysicalAddress) != 0) {
        DeallocateChannel(channelId);
        CallErrorHandler(
            SC_ERR_BUCKET_PHYSICAL_ADDRESS_FAILED,
            "ioctl(SC_IOCTL_GET_BUCKET_HWADDR, ...) failed "
            "with errno %d",
            errno);
        return nullptr;
      }

      bucket.BucketNumber = uint8_t(i);
      bucket.BucketIsUsed = false;
      bucket.pUserSpaceBucket =
          GetUserSpaceMappedDriverBucket(channelId, i);
    }

    // Get receive hardware address
    if (ioctl(channelId->FileDescriptor,
              SC_IOCTL_GET_RECV_HWADDR,
              &channelId->PrbPhysicalAddress) != 0) {
      DeallocateChannel(channelId);
      CallErrorHandler(
          SC_ERR_RECEIVE_PHYSICAL_ADDRESS_FAILED,
          "ioctl(SC_IOCTL_GET_RECV_HWADDR, ...) failed "
          "with errno %d",
          errno);
      return nullptr;
    }

    channelId->NetworkInterface = networkInterface;
    channelId->ChannelType = channelType;
    channelId->DefaultSendOptions = SC_SendOptions(
        SC_SEND_SINGLE_PACKET | SC_SEND_WAIT_FOR_PIPE_FREE);

    // Get network interface name:
    {
      sc_misc_t misc;
      memset(&misc, 0, sizeof(misc));
      misc.command = sc_misc_t::SC_MISC_GET_INTERFACE_NAME;
      misc.value1 = channelId->NetworkInterface;
      if (ioctl(channelId->FileDescriptor, SC_IOCTL_MISC,
                &misc) != 0) {
        DeallocateChannel(channelId);
        CallErrorHandler(
            SC_ERR_FAILED_TO_GET_INTERFACE_NAME,
            "failed to get interface name of network "
            "interface %u with errno %d",
            networkInterface, errno);
        return nullptr;
      }
      channelId->InterfaceName = misc.name;
    }

    // Initialize TX DMA stuff:
    channelId->NumberOfTxDmaLists =
        uint32_t(ARRAY_SIZE(channelId->TxDmaLists));
    channelId->TxDmaListsMappedSize = SC_BUCKET_SIZE;
    if (channelId->TxDmaListsMappedSize %
                channelId->NumberOfTxDmaLists !=
            0 || // Sanity check of values
        channelId->TxDmaListsMappedSize %
                sizeof(uint64_t) !=
            0) {
      DeallocateChannel(channelId);
      CallErrorHandler(
          SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO,
          "channelId->_txDmaListsMappedSize %u not "
          "divisible by channelId->_numberOfTxDmaLists %u "
          "or sizeof(uint64_t) %lu",
          channelId->TxDmaListsMappedSize,
          channelId->NumberOfTxDmaLists, sizeof(uint64_t));
      return nullptr;
    }
    channelId->TxDmaListSize = uint32_t(
        channelId->TxDmaListsMappedSize /
        channelId->NumberOfTxDmaLists / sizeof(uint64_t));
    channelId->FreeTxDmaListWaitTime =
        ::ONE_BILLION_NS; // Wait a second for a free TX DMA
                          // list, fail after that

    uint64_t *pTxDmaListsUserSpaceBase =
        reinterpret_cast<uint64_t *>(
            GetUserSpaceMappedDriverBucket(
                channelId, numberOfBuckets - 1));
    uint64_t txDmaListsKernelSpaceBase =
        channelId->Buckets[numberOfBuckets - 1]
            .BucketPhysicalAddress;
    for (uint32_t i = 0; i < channelId->NumberOfTxDmaLists;
         ++i) {
      channelId->TxDmaLists[i].pUserSpaceStart =
          pTxDmaListsUserSpaceBase +
          i * channelId->TxDmaListSize;
      channelId->TxDmaLists[i].KernelSpaceStart =
          txDmaListsKernelSpaceBase +
          i * channelId->TxDmaListSize * 8;
    }
    channelId->pCurrentTxDmaList = nullptr;
    if (LOG(deviceId, LOG_SETUP)) {
      Log(deviceId,
          "TxDMA: user space base %p, mapped size %u, "
          "number of lists = %u, list size is %u entries\n",
          static_cast<void *>(pTxDmaListsUserSpaceBase),
          channelId->TxDmaListsMappedSize,
          channelId->NumberOfTxDmaLists,
          channelId->TxDmaListSize);
      for (uint32_t i = 0;
           i < channelId->NumberOfTxDmaLists; ++i) {
        Log(deviceId,
            "channelId->_txDmaLists[%2u] = %p, "
            "_listPhysicalStart = 0x%lx\n",
            i,
            static_cast<volatile void *>(
                channelId->TxDmaLists[i].pUserSpaceStart),
            channelId->TxDmaLists[i].KernelSpaceStart);
      }
    }

    if (!CheckCumulativeFifoLimitBookings(
            channelId->DeviceId)) {
      DeallocateChannel(channelId);
      CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                       "combined bookings of channel level "
                       "Tx DMA FIFO limits are bigger "
                       "than the Tx DMA FIFO size %u\n",
                       MAX_FIFO_ENTRIES);
      return nullptr;
    }

    // Per default there is consumed bytes flow control per
    // channel:
    channelId->pTxConsumedBytesFlowControl =
        &channelId->TxConsumedBytesFlowControl;

    // Per default channels don't have window flow control
    // which only applies to TCP.
    channelId->pTxWindowFlowControl = nullptr;

    // Initialize per channel consumed bytes buffer shared
    // flow control:
    InitializeTxConsumedBytesFlowControl(
        &channelId->TxConsumedBytesFlowControl);

    channelId->pTcbSndUnaAndConsumedBytesRegister =
        SC_GetFpgaRegistersBase(deviceId) +
        FLOWCTRL_BASE / sizeof(uint64_t) +
        channelId->DmaChannelNumber;

    if (channelType == SC_DMA_CHANNEL_TYPE_TCP) {
      sc_etlm_data_t etlm_data;
      etlm_data.operation = sc_etlm_data_t::ETLM_GET_SIZES;
      etlm_data.channel_number = uint16_t(
          channelId
              ->DmaChannelNumber); // TODO: change
                                   // etlm_data.channel_number
                                   // to int16_t
      if (ioctl(channelId->FileDescriptor,
                SC_IOCTL_ETLM_DATA, &etlm_data) != 0) {
        DeallocateChannel(channelId);
        CallErrorHandler(
            SC_ERR_CHANNEL_ALLOCATION_FAILED,
            "failed to get ETLM Tx Windows size, errno %d",
            errno);
        return nullptr;
      }
      channelId->TxWindowFlowControl.EtlmTxWindowSize =
          etlm_data.u.capabilities.tx_window_size;
      if (LOG(deviceId, LOG_SETUP)) {
        Log(deviceId,
            "Channel %u TCP Tx window size is %u bytes\n",
            channelId->DmaChannelNumber,
            channelId->TxWindowFlowControl
                .EtlmTxWindowSize);
      }

      // All TCP channels share a common consumed bytes flow
      // control data.
      if (g_EtlmFlowControlSharedMemory
              .IsOpen()) // Is shared memory flow control in
                         // use for ETLM/TOE Tx 16 KB shared
                         // buffer?
      {
        // Multithreaded/multiprocess access to ETLM TCP
        // channels flow control supported via shared
        // memory.
        channelId->pTxConsumedBytesFlowControl =
            &g_EtlmFlowControlSharedMemory.GetSharedData();

        // Initialize ETLM 16 KB consumed bytes buffer
        // shared memory flow control:
        InitializeTxConsumedBytesFlowControl(
            channelId->pTxConsumedBytesFlowControl);
        channelId->pTxConsumedBytesFlowControl->pLock =
            const_cast<SC_ConsumedBytesLock *>(
                g_EtlmFlowControlSharedMemory.GetLock());
        if (LOG(deviceId, LOG_SETUP)) {
          Log(deviceId,
              "Channel %u TCP Tx using 16 KB buffer shared "
              "memory flow control\n",
              channelId->DmaChannelNumber);
        }
      } else {
        // Multithreaded access to TOE TCP channels flow
        // control only supported in a single process.
        channelId->pTxConsumedBytesFlowControl =
            &deviceId->TcpTxConsumedBytesFlowControl;
        channelId->pTxConsumedBytesFlowControl->pLock =
            &deviceId->TcpConsumedBytesLock;

        if (LOG(deviceId, LOG_SETUP)) {
          Log(deviceId,
              "Channel %u TCP Tx using 16 KB buffer single "
              "process flow control\n",
              channelId->DmaChannelNumber);
        }
      }

      // Set list flush limit to lower than non TCP channels
      // so a single list cannot monopolize the whole buffer
      // (TODO: evaluate this policy!)
      channelId->pTxConsumedBytesFlowControl
          ->ListFlushLimit = TxConsumedBytesBufferSize / 8;

      // Only TCP channels have window flow control
      channelId->pTxWindowFlowControl =
          &channelId->TxWindowFlowControl;

      InitializeCachedConsumedBytes(
          channelId,
          deviceId->TcpTxConsumedBytesFlowControl,
          channelId->pTxWindowFlowControl);
    } else if (channelType ==
               SC_DMA_CHANNEL_TYPE_USER_LOGIC) {
      InitializeCachedConsumedBytes(
          channelId, channelId->TxConsumedBytesFlowControl,
          nullptr);
    }

    if (pChannelOptions->UserSpaceRx) {
      // Register that we will handle incoming traffic in
      // userspace
      int rc = ioctl(channelId->FileDescriptor,
                     SC_IOCTL_USERSPACE_RX, nullptr);
      if (unlikely(rc != 0)) {
        DeallocateChannel(channelId);
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "Failed to register channel Rx "
                         "ownership on DMA channel %d",
                         dmaChannelNumber);
        return NULL;
      }
    }
    if (pChannelOptions->UserSpaceTx) {
      // Register that we will handle incoming traffic in
      // userspace
      int rc = ioctl(channelId->FileDescriptor,
                     SC_IOCTL_USERSPACE_TX, nullptr);
      if (unlikely(rc != 0)) {
        DeallocateChannel(channelId);
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "Failed to register channel Tx "
                         "ownership on DMA channel %d",
                         dmaChannelNumber);
        return NULL;
      }
    }

    SetLastErrorCode(
        SC_ERR_SUCCESS); // If we get here, all is well

    return channelId;
  } catch (SC_Exception &) {
    if (channelId != nullptr) {
      DeallocateChannel(channelId);
    }
    throw;
  } catch (std::bad_alloc &ex) {
    if (channelId != nullptr) {
      DeallocateChannel(channelId);
    }
    CallErrorHandler(SC_ERR_OUT_OF_MEMORY,
                     "out of memory exception caught: %s",
                     ex.what());
    return nullptr;
  } catch (std::exception &ex) {
    if (channelId != nullptr) {
      DeallocateChannel(channelId);
    }
    CallErrorHandler(SC_ERR_CHANNEL_ALLOCATION_FAILED,
                     "unexpected exception caught: %s",
                     ex.what());
    return nullptr;
  }
}

SC_ChannelId
SC_AllocateTcpChannel(SC_DeviceId deviceId,
                      uint8_t networkInterface,
                      int16_t channelNumber,
                      SC_ChannelOptions *pChannelOptions) {
  if (deviceId == nullptr) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "deviceId must not be NULL!");
    return nullptr;
  }

  if (channelNumber == -1 ||
      (channelNumber >= 0 &&
       channelNumber < SC_MAX_TCP_CHANNELS)) {
    if (channelNumber != -1) {
      channelNumber = static_cast<decltype(channelNumber)>(
          channelNumber + FIRST_TCP_CHANNEL);
    }

    deviceId->TxCapable = true;
    SC_ChannelId channelId = AllocateChannel(
        deviceId, networkInterface, channelNumber,
        SC_DMA_CHANNEL_TYPE_TCP, /*enableDma:*/ true,
        pChannelOptions);
    if (channelId != nullptr) {
      const ApiLibraryStaticInitializer &staticInitializer =
          ApiLibraryStaticInitializer::GetConstInstance();

      channelId->InternalConnectOptions =
          staticInitializer.DefaultConnectOptions;
      channelId->InternalListenOptions =
          staticInitializer.DefaultListenOptions;
    }
    return channelId;
  }

  CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "channel number %d out expected range -1 or %u-%u",
      channelNumber, 0, SC_MAX_TCP_CHANNELS - 1);
  return nullptr;
}

SC_ChannelId
SC_AllocateUdpChannel(SC_DeviceId deviceId,
                      uint8_t networkInterface,
                      int16_t channelNumber,
                      SC_ChannelOptions *pChannelOptions) {
  if (deviceId == nullptr) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "deviceId must not be NULL!");
    return nullptr;
  }

  if (channelNumber == -1 ||
      (channelNumber >= 0 &&
       channelNumber < SC_MAX_UDP_CHANNELS)) {
    if (channelNumber != -1) {
      channelNumber = static_cast<decltype(channelNumber)>(
          channelNumber + FIRST_UDP_CHANNEL);
    }
    deviceId->TxCapable = false;
    SC_ChannelId channelId = AllocateChannel(
        deviceId, networkInterface, channelNumber,
        SC_DMA_CHANNEL_TYPE_UDP_MULTICAST, false,
        pChannelOptions);
    if (channelId != nullptr) {
      channelId->InternalIgmpOptions =
          ApiLibraryStaticInitializer::GetConstInstance()
              .DefaultIgmpOptions;
    }
    return channelId;
  }

  CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "channel number %d out expected range -1 or %u-%u",
      channelNumber, 0, SC_MAX_UDP_CHANNELS - 1);
  return nullptr;
}

SC_ChannelId SC_AllocateMonitorChannel(
    SC_DeviceId deviceId, uint8_t networkInterface,
    SC_ChannelOptions *pChannelOptions) {
  if (deviceId == nullptr) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "deviceId must not be NULL!");
    return nullptr;
  }

  if (networkInterface <
      SC_MAX_NUMBER_OF_NETWORK_INTERFACES) {
    deviceId->TxCapable = false;
    return AllocateChannel(deviceId, networkInterface, -1,
                           SC_DMA_CHANNEL_TYPE_MONITOR,
                           1 /* enable dma */,
                           pChannelOptions);
  }

  CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "network interface %u out of expected range 0-%u",
      networkInterface,
      SC_MAX_NUMBER_OF_NETWORK_INTERFACES);
  return nullptr;
}

SC_ChannelId SC_AllocateUserLogicChannel(
    SC_DeviceId deviceId, int16_t channelNumber,
    SC_ChannelOptions *pChannelOptions) {
  if (deviceId == nullptr) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "deviceId must not be NULL!");
    return nullptr;
  }

  if (channelNumber >= 0 &&
      channelNumber < SC_MAX_ULOGIC_CHANNELS) {
    int16_t dmaChannelNumber = static_cast<int16_t>(
        FIRST_UL_CHANNEL + channelNumber);
    deviceId->TxCapable = true;
    const bool enableDma = true;
    return AllocateChannel(deviceId, 0, dmaChannelNumber,
                           SC_DMA_CHANNEL_TYPE_USER_LOGIC,
                           enableDma, pChannelOptions);
  }

  CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "channel number %d out expected range [0-%u]",
      channelNumber, SC_MAX_ULOGIC_CHANNELS - 1);
  return nullptr;
}

SC_Error SC_SetLocalAddress(
    SC_DeviceId deviceId, uint8_t networkInterface,
    struct in_addr local_ip, struct in_addr netmask,
    struct in_addr gateway,
    const uint8_t mac_addr[SC_MAC_ADDRESS_LENGTH]) {
  SanityCheck(deviceId);

  sc_local_addr_t local;
  local.network_interface = networkInterface;
  local.local_ip_addr = ntohl(local_ip.s_addr);
  local.netmask = ntohl(netmask.s_addr);
  local.gateway = ntohl(gateway.s_addr);

  if (mac_addr != nullptr) {
    memcpy(local.mac_addr, mac_addr, MAC_ADDR_LEN);
  } else {
    memset(local.mac_addr, 0, MAC_ADDR_LEN);
  }

  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_SET_LOCAL_ADDR, &local) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_SET_LOCAL_ADDRESS,
        "failed to set device local address, errno = %d",
        errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

std::string SCI_DecodeSIOCGIFFLAGS(unsigned int flags) {
  std::string flagString;
  unsigned int mask = 1;
  const char *separator = "";

  while (flags != 0) {
    flagString += separator;
    separator = " | ";

    switch (flags & mask) {
    case 0:
      separator = "";
      break;
    case IFF_UP:
      flagString += "IFF_UP";
      break; /* Interface is up.  */
    case IFF_BROADCAST:
      flagString += "IFF_BROADCAST";
      break; /* Broadcast address valid.  */
    case IFF_DEBUG:
      flagString += "IFF_DEBUG";
      break; /* Turn on debugging.  */
    case IFF_LOOPBACK:
      flagString += "IFF_LOOPBACK";
      break; /* Is a loopback net.  */
    case IFF_POINTOPOINT:
      flagString += "IFF_POINTOPOINT";
      break; /* Interface is point-to-point link.  */
    case IFF_NOTRAILERS:
      flagString += "IFF_NOTRAILERS";
      break; /* Avoid use of trailers.  */
    case IFF_RUNNING:
      flagString += "IFF_RUNNING";
      break; /* Resources allocated.  */
    case IFF_NOARP:
      flagString += "IFF_NOARP";
      break; /* No address resolution protocol.  */
    case IFF_PROMISC:
      flagString += "IFF_PROMISC";
      break; /* Receive all packets.  */
    case IFF_ALLMULTI:
      flagString += "IFF_ALLMULTI";
      break; /* Receive all multicast packets.  */
    case IFF_MASTER:
      flagString += "IFF_MASTER";
      break; /* Master of a load balancer.  */
    case IFF_SLAVE:
      flagString += "IFF_SLAVE";
      break; /* Slave of a load balancer.  */
    case IFF_MULTICAST:
      flagString += "IFF_MULTICAST";
      break; /* Supports multicast.  */
    case IFF_PORTSEL:
      flagString += "IFF_PORTSEL";
      break; /* Can set media type.  */
    case IFF_AUTOMEDIA:
      flagString += "IFF_AUTOMEDIA";
      break; /* Auto media select active.  */
    case IFF_DYNAMIC:
      flagString += "IFF_DYNAMIC";
      break; /* Dialup device with changing addresses.  */

    /* These are defined in Linux kernel linux/if.h but not
     * in user space net/if.h*/
    /* See
     * https://elixir.bootlin.com/linux/v4.20/source/include/uapi/linux/if.h#L102
     */
    case 1U << 16:
      flagString += "IFF_LOWER_UP";
      break; /* Driver signals L1 up (since Linux 2.6.17).
              */
    case 1U << 17:
      flagString += "IFF_DORMANT";
      break; /* Driver signals dormant (since Linux 2.6.17).
              */
    case 1U << 18:
      flagString += "IFF_ECHO";
      break; /* Echo sent packets (since Linux 2.6.25). */
    default: {
      char hexString[40];
      snprintf(hexString, sizeof(hexString),
               " *** UNKNOWN: 0x%08X *** ", flags & mask);
      flagString += hexString;
    } break;
    }

    flags = flags & ~mask;
    mask <<= 1;
  }

  return flagString;
}

/*****************************************************************************/

static int32_t FindNetworkInterfaceByInterfaceNumber(
    const SC_DeviceId deviceId, uint16_t interfaceNumber) {
  for (int16_t nifIndex = 0;
       nifIndex < deviceId->InternalCardInfo.CardInfo
                      .NumberOfNetworkInterfaces;
       ++nifIndex) {
    const SC_NetworkInterfaceInfo &networkInterfaceInfo =
        deviceId->InternalCardInfo.CardInfo
            .NetworkInterfaces[nifIndex];
    if (networkInterfaceInfo.InterfaceNumber ==
        interfaceNumber) {
      return nifIndex;
    }
  }

  return -1;
}

/*****************************************************************************/

SC_Error SCI_GetNetworkInterfaceInfo(
    SC_DeviceId deviceId, uint16_t networkInterface,
    struct in_addr *pLocalIP, struct in_addr *pNetMask,
    struct in_addr *pBroadcastAddress, uint32_t *pFlags) {
  if (pLocalIP != nullptr) {
    pLocalIP->s_addr = 0;
  }
  if (pNetMask != nullptr) {
    pNetMask->s_addr = 0;
  }
  if (pBroadcastAddress != nullptr) {
    pBroadcastAddress->s_addr = 0;
  }
  if (pFlags != nullptr) {
    *pFlags = 0;
  }

  ifaddrs *pIfAddresses;
  if (getifaddrs(&pIfAddresses) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
        "getifaddrs failed to get device local addresses, "
        "errno = %d",
        errno);
  }

  bool localIpFound = false, netMaskFound = false,
       broadcastIpFound = false;

  for (ifaddrs *pIfAddress = pIfAddresses;
       pIfAddress != nullptr;
       pIfAddress = pIfAddress->ifa_next) {
    char host[NI_MAXHOST];
    int32_t nifIndex =
        FindNetworkInterfaceByInterfaceNumber(
            deviceId, networkInterface);

    if (LOG(deviceId, LOG_IP)) {
      Log(deviceId,
          "pIfAddress = %p, networkInterface = %u, "
          "nifIndex = %d\n",
          static_cast<void *>(pIfAddress), networkInterface,
          nifIndex);
    }

    const SC_NetworkInterfaceInfo &networkInterfaceInfo =
        deviceId->InternalCardInfo.CardInfo
            .NetworkInterfaces[nifIndex];
    if (nifIndex < 0 ||
        strcmp(networkInterfaceInfo.InterfaceName,
               pIfAddress->ifa_name) != 0 ||
        networkInterface != nifIndex) {
      continue;
    }

    if (LOG(deviceId, LOG_IP)) {
      std::string flags =
          SCI_DecodeSIOCGIFFLAGS(pIfAddress->ifa_flags);

      Log(deviceId, "ifa_name = %s ifa_flags = 0x%X (%s)",
          pIfAddress->ifa_name, pIfAddress->ifa_flags,
          flags.c_str());
    }

    if (pIfAddress->ifa_addr != nullptr &&
        pIfAddress->ifa_addr->sa_family == AF_INET) {
      if (LOG(deviceId, LOG_IP)) {
        Log(deviceId,
            " ifa_addr->sa_data = 0x%02X 0x%02X 0x%02X "
            "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X ...",
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[0]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[1]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[2]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[3]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[4]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[5]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[6]),
            static_cast<uint8_t>(
                pIfAddress->ifa_addr->sa_data[7]));
      }

      // from "man getifaddrs":
      socklen_t addressLength = static_cast<socklen_t>(
          pIfAddress->ifa_addr->sa_family == AF_INET
              ? sizeof(sockaddr_in)
              : sizeof(sockaddr_in6));
      socklen_t hostLength =
          static_cast<socklen_t>(sizeof(host));
      if (getnameinfo(pIfAddress->ifa_addr, addressLength,
                      host, hostLength, NULL, 0,
                      NI_NUMERICHOST) != 0) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
            "%s getnameinfo of ifa_addr failed, errno %d",
            pIfAddress->ifa_name, errno);
      }

      sockaddr_in socketAddress;
      if (inet_pton(AF_INET, host,
                    &socketAddress.sin_addr) != 1) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
            "%s inet_pton of ifa_addr failed, errno %d",
            pIfAddress->ifa_name, errno);
      }

      if (pLocalIP != nullptr) {
        // memcpy(&pLocalIP->s_addr,
        // pIfAddress->ifa_addr->sa_data,
        // sizeof(pLocalIP->s_addr));
        pLocalIP->s_addr = socketAddress.sin_addr.s_addr;

#define HUH 0
#if HUH
        char newHost[NI_MAXHOST];
        // if (inet_ntop(AF_INET,
        // &socketAddress.sin_addr.s_addr, newHost,
        // hostLength) == NULL) printf(" *** inet_ntop
        // failed, errno %d\n", errno);
        if (inet_ntop(AF_INET, &pLocalIP->s_addr, newHost,
                      hostLength) == NULL)
          printf(" *** inet_ntop failed, errno %d\n",
                 errno);
        printf(" ifa_addr->sa_data    = 0x%02X 0x%02X "
               "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
               "... '%s' '%s' 0x%08X 0x%08X\n",
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[0]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[1]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[2]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[3]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[4]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[5]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[6]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_addr->sa_data[7]),
               host, newHost, socketAddress.sin_addr.s_addr,
               pLocalIP->s_addr);
        union {
          uint32_t s_addr;
          uint8_t bytes[4];
        } huh;
        huh.s_addr = socketAddress.sin_addr.s_addr;
        printf("huh 0x%08X = 0x%02X 0x%02X 0x%02X 0x%02X",
               huh.s_addr, huh.bytes[0], huh.bytes[1],
               huh.bytes[2], huh.bytes[3]);
#endif
      }
      localIpFound = true;

      if (pFlags != nullptr) {
        if (*pFlags == 0) {
          *pFlags = pIfAddress->ifa_flags;
        } else if (*pFlags != pIfAddress->ifa_flags) {
          return CallErrorHandler(
              SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
              "network interface ifa_addr flags of "
              "interface '%s' do not match with previous "
              "flags: 0x%X != 0x%X",
              pIfAddress->ifa_name, *pFlags,
              pIfAddress->ifa_flags);
        }
      }
    }
    if (pIfAddress->ifa_netmask != nullptr &&
        pIfAddress->ifa_netmask->sa_family == AF_INET) {
      sockaddr_in ipv4SocketAddress;
      memcpy(&ipv4SocketAddress, pIfAddress->ifa_netmask,
             sizeof(ipv4SocketAddress));

      if (LOG(deviceId, LOG_IP)) {
        Log(deviceId, " ifa_netmask = 0x%08X",
            ipv4SocketAddress.sin_addr.s_addr);
      }
      if (pNetMask != nullptr) {
        pNetMask->s_addr =
            ipv4SocketAddress.sin_addr.s_addr;
#if HUH
        // from man getifaddrs:
        int rc = getnameinfo(
            pIfAddress->ifa_netmask,
            static_cast<socklen_t>(
                pIfAddress->ifa_netmask->sa_family ==
                        AF_INET
                    ? sizeof(sockaddr_in)
                    : sizeof(sockaddr_in6)),
            host, static_cast<socklen_t>(sizeof(host)),
            NULL, 0, NI_NUMERICHOST);
        if (rc != 0)
          fprintf(stderr,
                  "getnameinfo failed with errno %d\n",
                  errno);
        printf(
            " ifa_netmask->sa_data = 0x%02X 0x%02X 0x%02X "
            "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X ... '%s'\n",
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[0]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[1]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[2]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[3]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[4]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[5]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[6]),
            static_cast<uint8_t>(
                pIfAddress->ifa_netmask->sa_data[7]),
            host);
#endif
      }
      netMaskFound = true;

      if (pFlags != nullptr) {
        if (*pFlags == 0) {
          *pFlags = pIfAddress->ifa_flags;
        } else if (*pFlags != pIfAddress->ifa_flags) {
          return CallErrorHandler(
              SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
              "network interface ifa_netmask flags of "
              "interface '%s' do not match with previous "
              "flags: 0x%X != 0x%X",
              pIfAddress->ifa_name, *pFlags,
              pIfAddress->ifa_flags);
        }
      }
    }
    if (pIfAddress->ifa_ifu.ifu_broadaddr != nullptr &&
        pIfAddress->ifa_ifu.ifu_broadaddr->sa_family ==
            AF_INET) {
      sockaddr_in ipv4SocketAddress;
      memcpy(&ipv4SocketAddress,
             pIfAddress->ifa_ifu.ifu_broadaddr,
             sizeof(ipv4SocketAddress));

      if (LOG(deviceId, LOG_IP)) {
        Log(deviceId, " ifu_broadaddr = 0x%08X",
            ipv4SocketAddress.sin_addr.s_addr);
      }
      if (pBroadcastAddress != nullptr) {
        pBroadcastAddress->s_addr =
            ipv4SocketAddress.sin_addr.s_addr;
#if HUH
        int rc = getnameinfo(
            pIfAddress->ifa_ifu.ifu_broadaddr,
            static_cast<socklen_t>(
                pIfAddress->ifa_ifu.ifu_broadaddr
                            ->sa_family == AF_INET
                    ? sizeof(sockaddr_in)
                    : sizeof(sockaddr_in6)),
            host, static_cast<socklen_t>(sizeof(host)),
            NULL, 0, NI_NUMERICHOST);
        if (rc != 0)
          fprintf(stderr,
                  "getnameinfo failed with errno %d\n",
                  errno);
        printf(" ifa_ifu.ifu_broadaddr->sa_data = 0x%02X "
               "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
               "0x%02X ... '%s'\n",
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[0]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[1]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[2]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[3]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[4]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[5]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[6]),
               static_cast<uint8_t>(
                   pIfAddress->ifa_ifu.ifu_broadaddr
                       ->sa_data[7]),
               host);
#endif
      }
      broadcastIpFound = true;

      if (pFlags != nullptr) {
        if (*pFlags == 0) {
          *pFlags = pIfAddress->ifa_flags;
        } else if (*pFlags != pIfAddress->ifa_flags) {
          return CallErrorHandler(
              SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
              "network interface ifu_broadaddr flags of "
              "interface '%s' do not match with previous "
              "flags: 0x%X != 0x%X",
              pIfAddress->ifa_name, *pFlags,
              pIfAddress->ifa_flags);
        }
      }
    }
    if (LOG(deviceId, LOG_IP)) {
      Log(deviceId, "n");
    }

    if (localIpFound && netMaskFound && broadcastIpFound) {
      break;
    }
  }

  freeifaddrs(pIfAddresses);

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetLocalAddress(
    SC_DeviceId deviceId, uint16_t networkInterface,
    struct in_addr *pLocalIP, struct in_addr *pNetMask,
    struct in_addr *pBroadcastAddress,
    uint8_t macAddress[SC_MAC_ADDRESS_LENGTH]) {
  SanityCheck(deviceId);

  if (pLocalIP != nullptr) {
    pLocalIP->s_addr = 0;
  }
  if (pNetMask != nullptr) {
    pNetMask->s_addr = 0;
  }
  if (pBroadcastAddress != nullptr) {
    pBroadcastAddress->s_addr = 0;
  }
  if (macAddress != nullptr) {
    memset(macAddress, 0, MAC_ADDR_LEN);
  }

  uint32_t flags;
  SC_Error errorCode = SCI_GetNetworkInterfaceInfo(
      deviceId, networkInterface, pLocalIP, pNetMask,
      pBroadcastAddress, &flags);
  if (errorCode != SC_ERR_SUCCESS) {
    return errorCode;
  }

  sc_local_addr_t local;
  memset(&local, 0, sizeof(local));
  local.network_interface = networkInterface;

  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_GET_LOCAL_ADDR, &local) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_LOCAL_ADDRESS,
        "failed to get device local address, errno = %d",
        errno);
  }

#if 0
    // These seem to not work very well.
    if (pLocalIP != nullptr)
    {
        pLocalIP->s_addr = htonl(local.local_ip_addr);
    }
    if (pNetMask != nullptr)
    {
        pNetMask->s_addr = htonl(local.netmask);
    }
    if (pBroadcastAddress != nullptr)
    {
        pBroadcastAddress->s_addr = htonl(local.gateway);
    }
#endif
  if (macAddress != nullptr) {
    memcpy(macAddress, local.mac_addr, MAC_ADDR_LEN);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_ResetFPGA(SC_DeviceId deviceId) {
  SanityCheck(deviceId);

  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_FPGA_RESET) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_RESET_FPGA,
        "failed to reset the FPGA, errno = %d", errno);
  }

  // Wait for everything start running normal again.
  // For all other tested systems 200 milliseconds delay in
  // the driver is enough but HP Proliant g9 servers
  // DL360/DL380 running CentOS 7.4 seem to require at least
  // 3 seconds. Have not been able to determine why these HP
  // servers are different. This extra delay is implemented
  // in the user space API library after executing the
  // driver SC_IOCTL_FPGA_RESET function.
  sleep(ApiLibraryStaticInitializer::GetConstInstance()
            .LibraryOptions.LibraryOptions
            .FpgaResetWaitInSeconds); // Wait extra seconds

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

/**
 *  Macro to get options.
 *
 *  @param  T
 */
#define GetOptions(OPTION_NAME, contextId, pOptions)       \
  if (pOptions == nullptr) {                               \
    return CallErrorHandler(                               \
        SC_ERR_INVALID_ARGUMENT,                           \
        "options argument " nameof(                        \
            pOptions) "cannot be NULL");                   \
  }                                                        \
                                                           \
  if (contextId == nullptr) {                              \
    /* Get default options: */                             \
    *pOptions =                                            \
        ApiLibraryStaticInitializer::GetConstInstance()    \
            .Default##OPTION_NAME.OPTION_NAME;             \
  } else {                                                 \
    SanityCheck(contextId);                                \
                                                           \
    *pOptions =                                            \
        contextId->Internal##OPTION_NAME.OPTION_NAME;      \
  }                                                        \
                                                           \
  return SetLastErrorCode(SC_ERR_SUCCESS);

/*****************************************************************************/

#define SetOptions(OPTION_NAME, contextId, pOptions)       \
  if (pOptions == nullptr) {                               \
    return CallErrorHandler(                               \
        SC_ERR_INVALID_ARGUMENT,                           \
        "argument " nameof(pOptions) " cannot be NULL");   \
  }                                                        \
                                                           \
  SC_Error errorCode =                                     \
      ApiLibraryStaticInitializer::GetInstance()           \
          .SanityCheckOptions(deviceId, pOptions);         \
  if (errorCode != SC_ERR_SUCCESS) {                       \
    return SetLastErrorCode(errorCode);                    \
  }                                                        \
                                                           \
  if (contextId == nullptr) {                              \
    /* Set default options: */                             \
    ApiLibraryStaticInitializer::GetInstance()             \
        .Default##OPTION_NAME = *pOptions;                 \
  } else {                                                 \
    SanityCheck(contextId);                                \
                                                           \
    contextId->Internal##OPTION_NAME.OPTION_NAME =         \
        *pOptions;                                         \
  }                                                        \
                                                           \
  return SetLastErrorCode(SC_ERR_SUCCESS);

/*****************************************************************************/

SC_Error SC_GetLinkStatus(SC_DeviceId deviceId,
                          uint64_t *pLinkStatus) {
  SanityCheck(deviceId);

  // FIXME: why is this doing an ioctl when we have the
  // status DMA mapped?
  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_LINK_STATUS,
            pLinkStatus) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_LINK_STATUS,
        "failed to get device link status, errno = %d",
        errno);
  }
  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

/// @cond PRIVATE_CODE
static inline float conv_temp(uint16_t int_val) {
  return int_val * 503.975F / 65536 - 273.15F;
}
/// @endcond PRIVATE_CODE

/*****************************************************************************/

SC_Error SC_GetFpgaTemperature(SC_DeviceId deviceId,
                               float *pMinimumTemperature,
                               float *pCurrentTemperature,
                               float *pMaximumTemperature) {
  SanityCheck(deviceId);

  sc_temperature_t temp;
  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_READ_TEMPERATURE, &temp) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_READ_FPGA_TEMPERATURE,
        "failed to read FPGA temperature, errno = %d",
        errno);
  }
  if (pMinimumTemperature != nullptr) {
    *pMinimumTemperature = conv_temp(temp.minTemp);
  }
  if (pCurrentTemperature != nullptr) {
    *pCurrentTemperature = conv_temp(temp.curTemp);
  }
  if (pMaximumTemperature != nullptr) {
    *pMaximumTemperature = conv_temp(temp.maxTemp);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

static bool IsValidNicInterface(const SC_CardInfo &cardInfo,
                                int32_t nif) {
  if (nif < 0 ||
      nif >= cardInfo.NumberOfNetworkInterfaces) {
    return false;
  }
  return (cardInfo.OobNetworkInterfacesEnabledMask &
          (1 << nif)) != 0;
}

/*****************************************************************************/

SC_Error SC_GetCardInfo(SC_DeviceId deviceId,
                        SC_CardInfo *pCardInfo) {
  if (pCardInfo == nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "pCardInfo parameter cannot be NULL");
  }
  if (deviceId == nullptr) {
    memset(pCardInfo, 0, sizeof(*pCardInfo));
    pCardInfo->StructSize = sizeof(*pCardInfo);

    return SetLastErrorCode(SC_ERR_SUCCESS);
  }

  SanityCheck(deviceId);

  sc_misc_t misc;
  memset(&misc, 0, sizeof(misc));

  misc.command = sc_misc_t::SC_MISC_GET_FPGA_VERSION;
  misc.value1 = 0;
  misc.value2 = 0;

#ifdef DEBUG
  // DecodeIoCtlCmd(SC_IOCTL_MISC);
#endif
  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
            &misc) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_READ_FPGA_VERSION,
        "failed to read FPGA version, errno = %d", errno);
  }

  fpga_revision_t revision;
  revision.raw_rev = misc.value1;
  uint32_t fpgaExtendedBuildNumber = uint32_t(misc.value2);
  bool useOldBuildNumber = revision.parsed.build == 0 ||
                           fpgaExtendedBuildNumber == 0;

  SC_FpgaInfo &fpgaInfo =
      deviceId->InternalCardInfo.CardInfo.FpgaInfo;
  fpgaInfo.Major = revision.parsed.major;
  fpgaInfo.Minor = revision.parsed.minor;
  fpgaInfo.Revision = revision.parsed.sub;
  fpgaInfo.Build = useOldBuildNumber
                       ? revision.parsed.build
                       : fpgaExtendedBuildNumber;
  fpgaInfo.ImageType = revision.parsed.type;
  fpgaInfo.Model = revision.parsed.model;
  fpgaInfo.MultiplePlDmaGroups = revision.parsed.pldma == 1;
  fpgaInfo.MMU = revision.parsed.mmu == 1;
  fpgaInfo.MMU_IsEnabled = misc.mmu_is_enabled == 1;

  bool PLDMA = revision.parsed.pldma == 1 ? true : false;
  UNUSED_ALWAYS(PLDMA);

  memset(&misc, 0, sizeof(misc));
  misc.command = sc_misc_t::SC_MISC_GET_BOARD_SERIAL;
  misc.value1 = 0;
  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
            &misc) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_CARD_INFO,
        "failed to get board serial number, errno = %d",
        errno);
  }

  deviceId->InternalCardInfo.CardInfo.BoardSerialNumber =
      static_cast<uint32_t>(misc.value1);
  std::string boardSerialNumber = misc.name;

  memset(&misc, 0, sizeof(misc));
  misc.command = sc_misc_t::SC_MISC_GET_PORT_MASKS;
  misc.value1 = 0;
  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
            &misc) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_BOARD_MASKS,
        "failed to get port masks, errno = %d", errno);
  }

  deviceId->InternalCardInfo.CardInfo
      .MacNetworkInterfacesEnabledMask =
      uint16_t(misc.value1 >> 48);
  deviceId->InternalCardInfo.CardInfo
      .OobNetworkInterfacesEnabledMask =
      uint16_t(misc.value1 >> 32);
  deviceId->InternalCardInfo.CardInfo
      .TcpNetworkInterfacesEnabledMask =
      uint16_t(misc.value1 >> 16);
  deviceId->InternalCardInfo.CardInfo
      .UdpNetworkInterfacesEnabledMask =
      uint16_t(misc.value1);
  deviceId->InternalCardInfo.CardInfo.NumberOfTcpChannels =
      deviceId->InternalCardInfo.CardInfo
                  .TcpNetworkInterfacesEnabledMask == 0
          ? 0
          : misc.number_of_tcp_channels;
  deviceId->InternalCardInfo.CardInfo
      .IsToeConnectedToUserLogic =
      misc.is_toe_connected_to_user_logic != 0;
  deviceId->InternalCardInfo.CardInfo
      .MonitorNetworkInterfacesEnabledMask =
      uint16_t(misc.value2 >> 16);
  deviceId->InternalCardInfo.CardInfo
      .AclNetworkInterfacesEnabledMask =
      uint16_t(misc.value2 >> 48);
  deviceId->InternalCardInfo.CardInfo.QsfpPortsEnabledMask =
      uint8_t(misc.value2 >> 32);
  deviceId->InternalCardInfo.CardInfo
      .EvaluationEnabledMask =
      uint16_t(misc.value2 & 0xFFFF);
  deviceId->InternalCardInfo.CardInfo
      .NumberOfNetworkInterfaces = misc.number_of_nifs;
  deviceId->InternalCardInfo.CardInfo.NumberOfNICs =
      uint16_t(NumberOfSetBits(
          deviceId->InternalCardInfo.CardInfo
              .OobNetworkInterfacesEnabledMask));
  deviceId->InternalCardInfo.CardInfo.NumberOfDmaChannels =
      misc.number_of_dma_channels;
  deviceId->InternalCardInfo.CardInfo.StatisticsEnabled =
      misc.statistics_enabled != 0;
  for (uint16_t nic = 0;
       nic < deviceId->InternalCardInfo.CardInfo
                 .NumberOfNetworkInterfaces;
       ++nic) {
    deviceId->InternalCardInfo.SetValues(
        nic, misc.mac_addresses[nic], nullptr, nullptr,
        nullptr, nullptr);
  }

  for (uint16_t dmaChannel = 0;
       dmaChannel < deviceId->InternalCardInfo.CardInfo
                        .NumberOfDmaChannels;
       ++dmaChannel) {
    sc_dma_info_t dmaInfo;
    memset(&dmaInfo, 0, sizeof(dmaInfo));
    dmaInfo.cmd = sc_dma_info_t::SC_GET_CHANNEL_INFO;
    dmaInfo.dma_channel = dmaChannel;
    if (ioctl(deviceId->FileDescriptor, SC_IOCTL_DMA_INFO,
              &dmaInfo) != 0) {
      return CallErrorHandler(
          SC_ERR_FAILED_TO_GET_CHANNEL_INFO,
          "failed to get channel %d info, errno = %d",
          dmaChannel, errno);
    }

    SC_DmaChannelInfo &dmaChannelInfo =
        const_cast<SC_DmaChannelInfo &>(
            deviceId->InternalCardInfo.CardInfo
                .DmaChannels[dmaChannel]);

    dmaChannelInfo.PacketRingBuffer.Length =
        dmaInfo.prb_memory_info.length;
    SC_ChannelId pChannel = deviceId->Channels[dmaChannel];
    if (pChannel != nullptr) {
      dmaChannelInfo.PacketRingBuffer
          .UserSpaceVirtualAddress =
          reinterpret_cast<uint64_t>(pChannel->pPRB);
    } else {
      dmaChannelInfo.PacketRingBuffer
          .UserSpaceVirtualAddress = 0;
    }

    dmaChannelInfo.PacketRingBuffer.NumberOfMemoryBlocks =
        1; // TODO FIXME ask driver for real number of
           // memory blocks!
    SC_Memory *pMemoryBlocks = const_cast<SC_Memory *>(
        dmaChannelInfo.PacketRingBuffer.MemoryBlocks);
    if (pMemoryBlocks != nullptr) {
      if (dmaChannelInfo.PacketRingBuffer
              .NumberOfMemoryBlocks == 0) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_GET_PCI_DEVICE_NAME,
            "There is a memory blocks array but number of "
            "memory blocks is zero, errno = %d",
            errno);
      }

      for (uint16_t i = 0;
           i < dmaChannelInfo.PacketRingBuffer
                   .NumberOfMemoryBlocks;
           ++i) {
        if (i == 0 && pChannel != nullptr) {
          // Take this only from the first memory block!
          pChannel->PrbFpgaStartAddress =
              dmaInfo.prb_memory_info.fpga_start_address;
        }

        SC_Memory &memoryBlock = pMemoryBlocks[i];

        memoryBlock.StructSize /**/ =
            sizeof(*pMemoryBlocks);
        memoryBlock.Length /**/ =
            dmaInfo.prb_memory_info.length;
        memoryBlock.KernelVirtualAddress /**/ =
            dmaInfo.prb_memory_info.kernel_virtual_address;
        memoryBlock.PhysicalAddress /**/ =
            dmaInfo.prb_memory_info.physical_address;
        memoryBlock.FpgaDmaStartAddress /**/ =
            dmaInfo.prb_memory_info.fpga_start_address;
        if (pChannel != nullptr) {
          memoryBlock.UserSpaceVirtualAddress =
              reinterpret_cast<uint64_t>(pChannel->pPRB);
        } else {
          memoryBlock.UserSpaceVirtualAddress = 0;
        }
      }
    } else {
      if (dmaChannelInfo.PacketRingBuffer
              .NumberOfMemoryBlocks != 0) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_GET_PCI_DEVICE_NAME,
            "No memory blocks array but number of memory "
            "blocks %u is not zero, errno = %d",
            dmaChannelInfo.PacketRingBuffer
                .NumberOfMemoryBlocks,
            errno);
      }
    }
  }

  memset(&misc, 0, sizeof(misc));
  misc.command = sc_misc_t::SC_MISC_GET_PCI_DEVICE_NAME;
  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
            &misc) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_PCI_DEVICE_NAME,
        "failed to get PCIe device name, errno = %d",
        errno);
  }

  deviceId->InternalCardInfo.SetStringValues(
      misc.name, boardSerialNumber.c_str());

  deviceId->InternalCardInfo.CardInfo
      .MaximumTxDmaFifoEntries = SC_MAX_TX_DMA_FIFO_ENTRIES;
  deviceId->InternalCardInfo.CardInfo
      .MaximumTxDmaFifoEntriesUsedByDriver = uint16_t(
      deviceId->InternalCardInfo.CardInfo.NumberOfNICs *
      FB_FIFO_ENTRIES_PER_CHANNEL);
  deviceId->InternalCardInfo.CardInfo.DriverName =
      LOWER_PRODUCT;

  deviceId->InternalCardInfo.CardInfo.DmaChannelCommonInfo
      .StructSize =
      sizeof(deviceId->InternalCardInfo.CardInfo
                 .DmaChannelCommonInfo);
  deviceId->InternalCardInfo.CardInfo.DmaChannelCommonInfo
      .MaximumNumberOfBucketsInTxList = 1024;
  deviceId->InternalCardInfo.CardInfo.DmaChannelCommonInfo
      .RecommendedMaximumNumberOfBytesInTxList = 8 * 1024;

  std::size_t nicIndex = 0;
  for (uint16_t nic = 0;
       nic < deviceId->InternalCardInfo.CardInfo
                 .NumberOfNetworkInterfaces;
       ++nic) {
    if (IsValidNicInterface(
            deviceId->InternalCardInfo.CardInfo, nic)) {
      SC_NicInfo &nicInfo = const_cast<SC_NicInfo &>(
          deviceId->InternalCardInfo.CardInfo
              .NICs[nicIndex]);

      nicInfo.InterfaceNumber = nic;

      {
        memset(&misc, 0, sizeof(misc));
        misc.command =
            sc_misc_t::SC_MISC_GET_INTERFACE_NAME;
        misc.value1 = nic;
        if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
                  &misc) != 0) {
          return CallErrorHandler(
              SC_ERR_FAILED_TO_GET_INTERFACE_NAME,
              "failed to get interface name of network "
              "interface %u with errno %d",
              nic, errno);
        }
        deviceId->InternalCardInfo.SetValues(
            nic, nullptr, misc.name, nullptr, nullptr,
            nullptr);
      }

      in_addr ipAddress, ipNetMask, ipBroadcastAddress;
      uint32_t interfaceFlags = 0;
      SC_Error errorCode = SCI_GetNetworkInterfaceInfo(
          deviceId, nic, &ipAddress, &ipNetMask,
          &ipBroadcastAddress, &interfaceFlags);
      if (errorCode != SC_ERR_SUCCESS) {
        return errorCode;
      }
      // SCI_GetNetworkInterfaceInfo returns addresses in
      // network byte order but system info is in host byte
      // order:
      nicInfo.IpAddress = ntohl(ipAddress.s_addr);
      nicInfo.IpNetMask = ntohl(ipNetMask.s_addr);
      nicInfo.IpBroadcastAddress =
          ntohl(ipBroadcastAddress.s_addr);
      nicInfo.InterfaceFlags = interfaceFlags;

      char ipAddressString[30];
      if (inet_ntop(AF_INET, &ipAddress.s_addr,
                    ipAddressString,
                    sizeof(ipAddressString)) == nullptr) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_CONVERT_IP_ADDRESS,
            "failed to convert IP address 0x%08X to a "
            "string, errno = %d",
            nicInfo.IpAddress, errno);
      }

      char ipNetMaskString[30];
      if (inet_ntop(AF_INET, &ipNetMask.s_addr,
                    ipNetMaskString,
                    sizeof(ipNetMaskString)) == nullptr) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_CONVERT_IP_ADDRESS,
            "failed to convert IP mask 0x%08X to a string, "
            "errno = %d",
            nicInfo.IpNetMask, errno);
      }

      char ipBroadcastAddressString[30];
      if (inet_ntop(AF_INET, &ipBroadcastAddress.s_addr,
                    ipBroadcastAddressString,
                    sizeof(ipBroadcastAddressString)) ==
          nullptr) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_CONVERT_IP_ADDRESS,
            "failed to convert IP broadcast address 0x%08X "
            "to a string, errno = %d",
            nicInfo.IpBroadcastAddress, errno);
      }

      deviceId->InternalCardInfo.SetValues(
          nic, nullptr, nullptr, ipAddressString,
          ipNetMaskString, ipBroadcastAddressString);

      ++nicIndex;
    }
  }

  *pCardInfo = deviceId->InternalCardInfo.CardInfo;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetSystemInfo(SC_SystemInfo *pSystemInfo) {
  if (pSystemInfo == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "parameter %s cannot be NULL",
                            nameof(pSystemInfo));
  }

  DIR *pDirectory = opendir("/dev");
  if (pDirectory == nullptr) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_SYSTEM_INFO,
        "failed to open device directory /dev/, errno %d",
        errno);
  }

  static std::vector<SC_DeviceInfo> s_DeviceInfos;
  static std::vector<std::string> s_DeviceNames;

  s_DeviceInfos.clear();
  s_DeviceNames.clear();

  pSystemInfo->StructSize = sizeof(*pSystemInfo);

  dirent *pDirectoryEntry;
  while ((pDirectoryEntry = readdir(pDirectory)) !=
         nullptr) {
    if (strstr(pDirectoryEntry->d_name, LOWER_PRODUCT) !=
            nullptr &&
        pDirectoryEntry->d_type == DT_CHR) {
      SC_DeviceInfo deviceInfo;
      memset(&deviceInfo, 0, sizeof(deviceInfo));

      deviceInfo.StructSize = sizeof(deviceInfo);

      s_DeviceNames.push_back(std::string("/dev/") +
                              pDirectoryEntry->d_name);
      deviceInfo.FullDeviceName =
          s_DeviceNames.back().c_str();

      int fd = open(deviceInfo.FullDeviceName,
                    O_RDWR | O_NONBLOCK);
      if (fd < 0) {
        deviceInfo.Available = false;
      } else {
        deviceInfo.Available = true;
        if (close(fd) != 0) {
          return CallErrorHandler(
              SC_ERR_FAILED_TO_GET_SYSTEM_INFO,
              "failed to open device file %s, errno %d",
              deviceInfo.FullDeviceName, errno);
        }
      }

      struct stat fileStats;
      if (stat(deviceInfo.FullDeviceName, &fileStats) !=
          0) {
        return CallErrorHandler(
            SC_ERR_FAILED_TO_GET_SYSTEM_INFO,
            "failed to get device file %s stat, errno %d",
            deviceInfo.FullDeviceName, errno);
      }

      deviceInfo.MajorDeviceNumber =
          static_cast<uint8_t>(major(fileStats.st_rdev));
      deviceInfo.MinorDeviceNumber =
          static_cast<uint8_t>(minor(fileStats.st_rdev));

      s_DeviceInfos.push_back(deviceInfo);
    }
  }

  pSystemInfo->Devices = &s_DeviceInfos[0];
  pSystemInfo->NumberOfDevices =
      static_cast<uint16_t>(s_DeviceInfos.size());

  if (closedir(pDirectory) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_SYSTEM_INFO,
        "failed to close device directory /dev/, errno %d",
        errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error
SC_GetLibraryOptions(SC_LibraryOptions *pLibraryOptions) {
  if (pLibraryOptions == nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "pLibraryOptions parameter cannot be NULL");
  }

  *pLibraryOptions =
      ApiLibraryStaticInitializer::GetConstInstance()
          .LibraryOptions.LibraryOptions;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error
SC_SetLibraryOptions(SC_LibraryOptions *pLibraryOptions) {
  if (pLibraryOptions == nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "pLibraryOptions cannot be NULL");
  }

  ApiLibraryStaticInitializer &staticInitializer =
      ApiLibraryStaticInitializer::GetInstance();

  SC_Error errorCode =
      staticInitializer.SanityCheckOptions(pLibraryOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  staticInitializer.LibraryOptions = *pLibraryOptions;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error
SC_GetDeviceOptions(SC_DeviceId deviceId,
                    SC_DeviceOptions *pDeviceOptions) {
  GetOptions(DeviceOptions, deviceId, pDeviceOptions);
}

/*****************************************************************************/

SC_Error
SC_SetDeviceOptions(SC_DeviceId deviceId,
                    SC_DeviceOptions *pDeviceOptions) {
  SetOptions(DeviceOptions, deviceId, pDeviceOptions);
}

/*****************************************************************************/

static SC_Error
GetChannelOptions(SC_ChannelId channelId,
                  SC_ChannelOptions *pChannelOptions) {
  GetOptions(ChannelOptions, channelId, pChannelOptions);
}

/*****************************************************************************/

SC_Error
SC_GetChannelOptions(SC_ChannelId channelId,
                     SC_ChannelOptions *pChannelOptions) {
  SC_Error errorCode =
      GetChannelOptions(channelId, pChannelOptions);
  if (errorCode == SC_ERR_SUCCESS) {
    if (channelId != nullptr &&
        pChannelOptions != nullptr) {
      InitializeTxDmaFlowControl(
          &channelId->TxDmaNormalFifoFlowControl,
          pChannelOptions
              ->TxDmaNormalFifoSoftwareFillLevelLimit);
      InitializeTxDmaFlowControl(
          &channelId->TxDmaPriorityFifoFlowControl,
          pChannelOptions
              ->TxDmaPriorityFifoSoftwareFillLevelLimit);
    }
  }

  return errorCode; // Error handler called and last error
                    // code set already in
                    // GetChannelOptions.
}

/*****************************************************************************/

static SC_Error
SetChannelOptions(SC_ChannelId channelId,
                  SC_ChannelOptions *pChannelOptions) {
  SC_DeviceId deviceId =
      channelId == nullptr ? nullptr : channelId->DeviceId;

  SetOptions(ChannelOptions, channelId, pChannelOptions);
}

/*****************************************************************************/

SC_Error
SC_SetChannelOptions(SC_ChannelId channelId,
                     SC_ChannelOptions *pChannelOptions) {
  SC_Error errorCode =
      SetChannelOptions(channelId, pChannelOptions);
  if (errorCode == SC_ERR_SUCCESS) {
    if (channelId != nullptr &&
        pChannelOptions != nullptr) {
      InitializeTxDmaFlowControl(
          &channelId->TxDmaNormalFifoFlowControl,
          pChannelOptions
              ->TxDmaNormalFifoSoftwareFillLevelLimit);
      InitializeTxDmaFlowControl(
          &channelId->TxDmaPriorityFifoFlowControl,
          pChannelOptions
              ->TxDmaPriorityFifoSoftwareFillLevelLimit);
    }
  }

  return errorCode; // Error handler called and last error
                    // code set already in
                    // SetChannelOptions.
}

/*****************************************************************************/

SC_Error SC_GetDriverLogMask(SC_DeviceId deviceId,
                             uint64_t *pLogMask) {
  if (deviceId == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "device id cannot be NULL!");
  }
  if (pLogMask == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "pLogMask cannot be NULL!");
  }

  SanityCheck(deviceId);

  sc_misc_t misc;
  misc.command = sc_misc_t::SC_MISC_GET_DRIVER_LOG_MASK;
  misc.value1 = 0;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
            &misc) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_DRIVER_LOG_MASK,
        "failed to get driver log mask, errno = %d", errno);
  }
  *pLogMask = misc.value1;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_SetDriverLogMask(SC_DeviceId deviceId,
                             uint64_t logMask) {
  if (deviceId == nullptr) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "device id cannot be NULL!");
  }

  SanityCheck(deviceId);

  sc_misc_t misc;
  misc.command = sc_misc_t::SC_MISC_SET_DRIVER_LOG_MASK;
  misc.value1 = logMask;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_MISC,
            &misc) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_SET_DRIVER_LOG_MASK,
        "failed to set driver log mask, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_SetTimeStampingMode(SC_DeviceId deviceId,
                                SC_TimeControl mode) {
  uint32_t timemode = mode;

  if (mode != SC_TIMECONTROL_HARDWARE &&
      mode != SC_TIMECONTROL_SOFTWARE) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_SET_TIME_STAMPING_MODE,
        "failed to set time stamping mode, wrong mode = %u",
        mode);
  }

  SanityCheck(deviceId);

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_TIMERMODE,
            &timemode) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_SET_TIME_STAMPING_MODE,
        "failed to set time stamping mode, errno = %d",
        errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

/**
 *  Convert time mode value to seconds and nanoseconds.
 * Quants are time measure units of Silicom network cards
 * that depend on clock frequency used and can vary between
 * different card models.
 *
 *  @param  timestampMSDW       Timestamp most significant
 * doubleword.
 *  @param  timestampLSDW       Timestamp least significant
 * doubleword.
 *
 *  @return     Error code.
 */
static SC_Error
TimeModeToNanoseconds(uint32_t &timestampMSDW,
                      uint32_t &timestampLSDW) {
  UNUSED_ALWAYS(timestampMSDW);

  // Time mode currently hardcoded to @ref sc_time_mode_t
  // SC_TIME_MODE_SECONDS_NANOSECONDS in driver.
  sc_time_mode_t timeMode =
      SC_TIME_MODE_SECONDS_NANOSECONDS;

  switch (timeMode) {
  case SC_TIME_MODE_SECONDS_MICROSECONDS:
    timestampLSDW *=
        1000; // Convert microseconds to nanoseconds
    break;

  case SC_TIME_MODE_SECONDS_NANOSECONDS:
    break; // Nothing to do, timestamp is exactly as we want
           // it

  case SC_TIME_MODE_10_NANOSECOND_COUNT:
    return CallErrorHandlerDontSetLastError(
        SC_ERR_NOT_SUPPORTED_OR_NOT_IMPLEMENTED,
        "conversion of time mode "
        "SC_TIME_MODE_10_NANOSECOND_COUNT not implemented");

  case SC_TIME_MODE_100_NANOSECOND_COUNT:
    return CallErrorHandlerDontSetLastError(
        SC_ERR_NOT_SUPPORTED_OR_NOT_IMPLEMENTED,
        "conversion of time mode "
        "SC_TIME_MODE_100_NANOSECOND_COUNT not "
        "implemented");

  case SC_TIME_MODE_SECONDS_400_PICOSECONDS: {
    // Convert 400 picosecond quants to nanoseconds:
    // Factor is 400 / 1000 = 4 / 10
    uint64_t nanoseconds = 4 * uint64_t(timestampLSDW);
    uint32_t roundUp = nanoseconds % 10 >= 5 ? 1 : 0;
    timestampLSDW = uint32_t(nanoseconds / 10) + roundUp;
  } break;

  default:
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT, "uUnknown time mode %d",
        timeMode);
  }

  return SC_ERR_SUCCESS;
}

/*****************************************************************************/

SC_Error
SC_GetStatistics(SC_DeviceId deviceId,
                 SC_PacketStatistics *pPacketStatistics) {
  if (pPacketStatistics == nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "pPacketStatistics parameter cannot be NULL");
  }
  if (deviceId == nullptr) {
    memset(pPacketStatistics, 0,
           sizeof(*pPacketStatistics));
    pPacketStatistics->StructSize =
        sizeof(*pPacketStatistics);

    return SetLastErrorCode(SC_ERR_SUCCESS);
  }

  SanityCheck(deviceId);

  sc_stat_packet_payload_t packetStatistics;
  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_GET_STAT,
            &packetStatistics) != 0) {
    if (errno == ENODEV) {
      return CallErrorHandler(
          SC_ERR_FPGA_DOES_NOT_SUPPORT_FRAME_STATISTICS,
          "FPGA does not support frame statistics, errno = "
          "%d",
          errno);
    }
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_FRAME_STATISTICS,
        "failed to get Ethernet frame statistics, errno = "
        "%d",
        errno);
  }

  deviceId->InternalPacketStatistics.PacketStatistics
      .NumberOfNetworkInterfaces =
      deviceId->InternalCardInfo.CardInfo.NumberOfNICs;
  deviceId->InternalPacketStatistics.PacketStatistics
      .Timestamp.Seconds =
      packetStatistics.timestamp_seconds;
  deviceId->InternalPacketStatistics.PacketStatistics
      .Timestamp.Nanoseconds =
      packetStatistics.timestamp_nanoseconds;
  SC_Error errorCode = TimeModeToNanoseconds(
      deviceId->InternalPacketStatistics.PacketStatistics
          .Timestamp.Seconds,
      deviceId->InternalPacketStatistics.PacketStatistics
          .Timestamp.Nanoseconds);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  assert(SC_MAX_NUMBER_OF_NETWORK_INTERFACES == 8 ||
         SC_MAX_NUMBER_OF_NETWORK_INTERFACES == 16);
  for (std::size_t networkInterface = 0;
       networkInterface <
       deviceId->InternalCardInfo.CardInfo.NumberOfNICs;
       ++networkInterface) {
    deviceId->InternalPacketStatistics
        .NetworkInterfacesRxStatistics[networkInterface] =
        packetStatistics.nif_groups[0]
            .rx_nifs[networkInterface];
    deviceId->InternalPacketStatistics
        .NetworkInterfacesTxStatistics[networkInterface] =
        packetStatistics.nif_groups[0]
            .tx_nifs[networkInterface];
    if (SC_MAX_NUMBER_OF_NETWORK_INTERFACES == 16) {
      std::size_t networkInterfacePerGroup =
          networkInterface % SC_STAT_CHANNELS_PER_GROUP;

      deviceId->InternalPacketStatistics
          .NetworkInterfacesRxStatistics[networkInterface] =
          packetStatistics.nif_groups[1]
              .rx_nifs[networkInterfacePerGroup];
      deviceId->InternalPacketStatistics
          .NetworkInterfacesTxStatistics[networkInterface] =
          packetStatistics.nif_groups[1]
              .tx_nifs[networkInterfacePerGroup];
    }
  }

  *pPacketStatistics =
      deviceId->InternalPacketStatistics.PacketStatistics;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetStatusInfo(SC_DeviceId deviceId,
                          SC_StatusInfo *pStatusInfo) {
  if (pStatusInfo == nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "pStatusInfo parameter cannot be NULL");
  }
  if (deviceId == nullptr) {
    memset(pStatusInfo, 0, sizeof(*pStatusInfo));
    pStatusInfo->StructSize = sizeof(*pStatusInfo);

    return SetLastErrorCode(SC_ERR_SUCCESS);
  }

  SanityCheck(deviceId);

  SC_Error errorCode = SC_ERR_SUCCESS;

  // Status information is updated by FPGA DMA into host
  // memory that is mapped to user space:
  const sc_status_dma_t *pStatusDma = deviceId->pStatusDma;

  for (std::size_t i = 0; i < 4; ++i) {
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.Link =
        pStatusDma->input_stat[i].link == 1;
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.LostLink =
        pStatusDma->input_stat[i].lost_link == 1;
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.SFP_Present =
        pStatusDma->input_stat[i].sfp_present == 1;
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.SFP_Removed =
        pStatusDma->input_stat[i].sfp_removed == 1;
  }
  for (std::size_t i = 4; i < 8; ++i) {
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.Link =
        pStatusDma->input_stat_ext[i % 4].link == 1;
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.LostLink =
        pStatusDma->input_stat_ext[i % 4].lost_link == 1;
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.SFP_Present =
        pStatusDma->input_stat_ext[i % 4].sfp_present == 1;
    deviceId->InternalStatusInfo
        .NetworkInterfacesLinkStatus[i]
        .LinkStatus.SFP_Removed =
        pStatusDma->input_stat_ext[i % 4].sfp_removed == 1;
  }

  deviceId->InternalStatusInfo.StatusInfo.NumberOfLinks =
      deviceId->InternalCardInfo.CardInfo
          .NumberOfNetworkInterfaces;
  deviceId->InternalStatusInfo.StatusInfo.PPS_Missing =
      pStatusDma->pps_missing == 1;
  deviceId->InternalStatusInfo.StatusInfo.PPS_Present =
      pStatusDma->pps_present == 1;

  // The following 2 fields are currently not implemented in
  // the driver:
  // deviceId->InternalStatusInfo.StatusInfo.StatisticsOverflow
  // = pStatusDma->stat_overflow == 1;
  // deviceId->InternalStatusInfo.StatusInfo.PacketDmaOverflow
  // = pStatusDma->packet_dma_overflow == 1;

  deviceId->InternalStatusInfo.StatusInfo
      .NumberOfTcpChannels =
      deviceId->InternalCardInfo.CardInfo
          .NumberOfTcpChannels;
  for (uint16_t toeChannel = 0;
       toeChannel < SC_MAX_TCP_CHANNELS; ++toeChannel) {
    if (toeChannel < deviceId->InternalStatusInfo.StatusInfo
                         .NumberOfTcpChannels) {
      SC_TcpState tcpState =
          SCI_GetTcpState(deviceId, toeChannel);
      if (static_cast<SC_Error>(tcpState) < 0 &&
          errorCode == SC_ERR_SUCCESS) {
        errorCode = static_cast<SC_Error>(tcpState);
      } else {
        deviceId->InternalStatusInfo
            .TcpChannelsTcpConnectionStatus[toeChannel] =
            tcpState;
      }
    } else {
      deviceId->InternalStatusInfo
          .TcpChannelsTcpConnectionStatus[toeChannel] =
          SC_TCP_NONE;
    }
  }

  deviceId->InternalStatusInfo.StatusInfo
      .NumberOfRxDmaFillings =
      deviceId->InternalCardInfo.CardInfo
          .NumberOfDmaChannels;
  for (std::size_t dmaChannel = 0;
       dmaChannel < deviceId->InternalStatusInfo.StatusInfo
                        .NumberOfRxDmaFillings;
       ++dmaChannel) {
    uint64_t filling = 0;
    uint64_t peakFilling = 0;

    sc_filling_scale_t fillingScale =
        SC_FILLING_SCALE_1_KB; // TODO: get value from
                               // driver, currently hard
                               // coded as 0

    switch (fillingScale) {
    case SC_FILLING_SCALE_1_KB:
      filling =
          pStatusDma->dma_filling[dmaChannel].filling *
          1024;
      peakFilling =
          pStatusDma->dma_filling[dmaChannel].peak_filling *
          1024;
      break;
    case SC_FILLING_SCALE_16_KB:
      filling =
          pStatusDma->dma_filling[dmaChannel].filling * 16 *
          1024;
      peakFilling =
          pStatusDma->dma_filling[dmaChannel].peak_filling *
          16 * 1024;
      break;
    case SC_FILLING_SCALE_256_KB:
      filling =
          pStatusDma->dma_filling[dmaChannel].filling *
          256 * 1024;
      peakFilling =
          pStatusDma->dma_filling[dmaChannel].peak_filling *
          256 * 1024;
      break;
    case SC_FILLING_SCALE_4_MB:
      filling =
          pStatusDma->dma_filling[dmaChannel].filling *
          4UL * 1024ULL * 1024UL;
      peakFilling =
          pStatusDma->dma_filling[dmaChannel].peak_filling *
          4 * 1024 * 1024;
      break;
    default:
      return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                              "Unknown filling scale %d",
                              fillingScale);
    }

    SC_FillingLevels &fillingEntry =
        deviceId->InternalStatusInfo
            .ChannelsRxDmaFillings[dmaChannel]
            .FillingLevels;
    fillingEntry.Size = RECV_DMA_SIZE;

    fillingEntry.Filling = filling;
    fillingEntry.PeakFilling = peakFilling;

    double fillingPercentage =
        double(filling) * 100.0 / double(fillingEntry.Size);
    fillingEntry.FillingPercentage =
        float(fillingPercentage);

    double peakFillingPercentage =
        double(peakFilling) * 100.0 /
        double(fillingEntry.Size);
    fillingEntry.PeakFillingPercentage =
        float(peakFillingPercentage);
  }

  *pStatusInfo = deviceId->InternalStatusInfo.StatusInfo;

  return SetLastErrorCode(errorCode);
}

/*****************************************************************************/

SC_Error
SC_GetConnectOptions(SC_ChannelId channelId,
                     SC_ConnectOptions *pConnectOptions) {
  if (channelId != nullptr &&
      channelId->ChannelType != SC_DMA_CHANNEL_TYPE_TCP) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not TCP!");
  }

  GetOptions(ConnectOptions, channelId, pConnectOptions);
}

/*****************************************************************************/

SC_Error
SC_SetConnectOptions(SC_ChannelId channelId,
                     SC_ConnectOptions *pConnectOptions) {
  SC_DeviceId deviceId =
      channelId == nullptr ? nullptr : channelId->DeviceId;

  if (channelId != nullptr &&
      channelId->ChannelType != SC_DMA_CHANNEL_TYPE_TCP) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not TCP!");
  }

  SetOptions(ConnectOptions, channelId, pConnectOptions);
}

/*****************************************************************************/

SC_Error
SC_GetListenOptions(SC_ChannelId channelId,
                    SC_ListenOptions *pListenOptions) {
  if (channelId != nullptr &&
      channelId->ChannelType != SC_DMA_CHANNEL_TYPE_TCP) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not TCP!");
  }

  GetOptions(ListenOptions, channelId, pListenOptions);
}

/*****************************************************************************/

SC_Error
SC_SetListenOptions(SC_ChannelId channelId,
                    SC_ListenOptions *pListenOptions) {
  SC_DeviceId deviceId =
      channelId == nullptr ? nullptr : channelId->DeviceId;

  if (channelId != nullptr &&
      channelId->ChannelType != SC_DMA_CHANNEL_TYPE_TCP) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not TCP!");
  }

  SetOptions(ListenOptions, channelId, pListenOptions);
}

/*****************************************************************************/

/**
 *  Get MAC address from the driver's ARP table or if not
 * available then send an ARP request out and wait for the
 * result.
 *
 *  @param  channelId           Channel idenfifier.
 *  @param  pConnectOptions     Pointer to connect options
 *  @param  remoteHost          IP address of the remote
 * host as a string
 *  @param  ipAddress           IP address of the remote
 * host as 32-bit unsigned integer in host byte order.
 *
 *  @return                     SC_ERR_SUCCESS if
 * successful, a negative error code otherwise.
 */
static SC_Error GetMacAddressWithDriverARP(
    SC_ChannelId channelId,
    SC_ConnectOptions *pConnectOptions,
    const char *remoteHost, uint32_t ipAddress) {
  SC_DeviceId deviceId = channelId->DeviceId;

  int fd = SC_GetFileDescriptor(deviceId);

  int64_t acquireMacAddressViaArpTimeout =
      pConnectOptions->AcquireMacAddressViaArpTimeout;
  const int PauseTimeInMilliseconds = 10;

  while (acquireMacAddressViaArpTimeout > 0) {
    int rc;

    sc_arp_t arp;
    memset(&arp, 0, sizeof(arp));
    arp.command = sc_arp_t::ARP_COMMAND_POLL;
    arp.remote_ip = ipAddress;
    arp.vlanTag = pConnectOptions->VLanTag;
    arp.lane = channelId->NetworkInterface;
    arp.arp_entry_timeout =
        deviceId->InternalDeviceOptions.DeviceOptions
            .ArpEntryTimeout;
    if ((rc = ioctl(fd, SC_IOCTL_ARP, &arp)) != 0) {
      if (errno == ENXIO) {
        // IP address not registered or pending in driver
        // ARP table, send an ARP request via driver.
        if (LOG(deviceId, LOG_ARP)) {
          Log(deviceId, "No MAC address found for IP %s\n",
              remoteHost);
        }

        deviceId->Arp.MakeArpRequest(
            ARP::ArpMethod::
                ARP_METHOD_ARP_REQUEST_VIA_DRIVER,
            channelId->NetworkInterface,
            channelId->InterfaceName, remoteHost,
            pConnectOptions->VLanTag);
        continue;
      }
      return CallErrorHandlerDontSetLastError(
          SC_ERR_ARP_FAILED,
          "failed to acquire MAC address via ARP for "
          "remote host %s, errno %d",
          remoteHost, errno);
    } else if (MacAddressIsNotZero(
                   arp.remote_mac_address)) {
      // Driver ARP table provided a MAC address.
      memcpy(pConnectOptions->RemoteMacAddress,
             arp.remote_mac_address, MAC_ADDR_LEN);
      return SetLastErrorCode(SC_ERR_SUCCESS);
    }

    // IP address was registered in driver ARP table but MAC
    // is still zero, i.e. ARP reply has not yet arrived.
    usleep(PauseTimeInMilliseconds * 1000);
    acquireMacAddressViaArpTimeout -=
        PauseTimeInMilliseconds;
  }

  return CallErrorHandlerDontSetLastError(
      SC_ERR_ARP_FAILED,
      "failed to acquire MAC address via ARP for remote "
      "host %s within %u milliseconds",
      remoteHost,
      pConnectOptions->AcquireMacAddressViaArpTimeout);
}

/*****************************************************************************/

SC_Error SC_Connect(SC_ChannelId channelId,
                    const char *remoteHost,
                    uint16_t remotePort,
                    SC_ConnectOptions *pConnectOptions) {
  SanityCheck(channelId);

  SC_DeviceId deviceId = channelId->DeviceId;

  if (remoteHost == nullptr || remoteHost[0] == '\0') {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        nameof(SC_Connect) " " nameof(
            remoteHost) " argument may not be NULL or "
                        "empty!");
  }

  if (pConnectOptions == nullptr) {
    pConnectOptions =
        &channelId->InternalConnectOptions.ConnectOptions;
  }

  SC_Error errorCode =
      ApiLibraryStaticInitializer::GetConstInstance()
          .SanityCheckOptions(channelId->DeviceId,
                              pConnectOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  if (!(pConnectOptions->VLanTag == -1 ||
        (pConnectOptions->VLanTag > 0 &&
         pConnectOptions->VLanTag < 4095))) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "invalid VLAN tag %d (VLAN tag must be greater "
        "than 0 and less than 4095 or -1 for no VLAN).",
        pConnectOptions->VLanTag);
  }

  struct in_addr address;
  if (inet_aton(remoteHost, &address) == 0) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "invalid remote IP address %s. errno %d",
        remoteHost, errno);
  }

  if (MacAddressIsZero(pConnectOptions->RemoteMacAddress)) {
    if (pConnectOptions->ScanLinuxArpTable) {
      // Scan Linux ARP table for an existing or static MAC
      // entry
      errorCode = deviceId->Arp.UpdateARP();
      if (errorCode == SC_ERR_SUCCESS) {
        errorCode = deviceId->Arp.GetMacAddress(
            ARP::ARP_METHOD_ARP_TABLE_LOOKUP_ONLY,
            channelId->NetworkInterface,
            channelId->InterfaceName, remoteHost,
            pConnectOptions->VLanTag,
            pConnectOptions->RemoteMacAddress,
            pConnectOptions
                ->AcquireMacAddressViaArpTimeout);
        // Ignore errors here, we got a MAC address or we
        // didn't
      }
    }
  }

  if (MacAddressIsZero(pConnectOptions->RemoteMacAddress)) {
    if (ApiLibraryStaticInitializer::GetConstInstance()
            .LibraryARP) {
      errorCode = deviceId->Arp.GetMacAddress(
          ARP::ARP_METHOD_PING_UTILITY,
          channelId->NetworkInterface,
          channelId->InterfaceName, remoteHost,
          pConnectOptions->VLanTag,
          pConnectOptions->RemoteMacAddress,
          pConnectOptions->AcquireMacAddressViaArpTimeout);
    } else {
      errorCode = GetMacAddressWithDriverARP(
          channelId, pConnectOptions, remoteHost,
          ntohl(address.s_addr));
    }

    if (errorCode != SC_ERR_SUCCESS) {
      return SetLastErrorCode(errorCode);
    }
  }

  SC_TcpState tcpState = SC_GetTcpState(channelId);
  if (tcpState != SC_TCP_CLOSED) {
    std::stringstream ss;
    ss << "Connect to host " << remoteHost << " port "
       << remotePort << " on channel "
       << SC_GetChannelNumber(channelId)
       << " failed because TCP state is not closed but is "
       << int(tcpState);
    return CallErrorHandler(
        SC_ERR_FAILED_TO_CONNECT,
        "connect to host %s port %u on channel %d failed "
        "because RCP state is not closed but is %u",
        remoteHost, remotePort,
        SC_GetChannelNumber(channelId), tcpState);
  }

  channelId->RemoteAddress = remoteHost;
  channelId->RemotePort = remotePort;

  sc_connection_info_t connect_info;
  connect_info.remote_ip = ntohl(address.s_addr);
  connect_info.remote_port = remotePort;
  connect_info.local_port = pConnectOptions->LocalPort;
  memcpy(connect_info.remote_mac_address,
         pConnectOptions->RemoteMacAddress,
         sizeof(connect_info.remote_mac_address));

  connect_info.local_ip_address = 0;
  if (pConnectOptions->LocalIpAddress != nullptr) {
    if (pConnectOptions->LocalIpAddress[0] != '\0') {
      if (inet_aton(pConnectOptions->LocalIpAddress,
                    &address) == 0) {
        return CallErrorHandler(
            SC_ERR_INVALID_ARGUMENT,
            "invalid local IP address %s. errno %d",
            pConnectOptions->LocalIpAddress, errno);
      }
      connect_info.local_ip_address = ntohl(address.s_addr);
    }
  }
  connect_info.vlan_tag = pConnectOptions->VLanTag;
  if (connect_info.vlan_tag != -1 &&
      (MacAddressIsZero(connect_info.remote_mac_address) ||
       connect_info.local_ip_address == 0)) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "if VLAN is enabled then local IP and remote MAC "
        "address have to be defined.");
  }
  connect_info.timeout = pConnectOptions->ConnectTimeout;

  if (ioctl(channelId->FileDescriptor, SC_IOCTL_CONNECT,
            &connect_info) != 0) {
    return CallErrorHandler(SC_ERR_FAILED_TO_CONNECT,
                            "failed to connect to remote "
                            "host %s:%u, errno = %d",
                            remoteHost, remotePort, errno);
  }

  if (pConnectOptions != nullptr &&
      pConnectOptions->LocalPort == 0) {
    // Return automatically assigned IP port number.
    pConnectOptions->LocalPort = connect_info.local_port;
  }

  InitializeCachedConsumedBytes(
      channelId, deviceId->TcpTxConsumedBytesFlowControl,
      channelId->pTxWindowFlowControl);

  channelId->LastPacketPlDmaAddress =
      SC_INVALID_PLDMA_ADDRESS;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_Listen(SC_ChannelId channelId,
                   uint16_t localPort,
                   SC_ListenOptions *pListenOptions) {
  SanityCheck(channelId);

  if (pListenOptions == nullptr) {
    pListenOptions =
        &channelId->InternalListenOptions.ListenOptions;
  }

  SC_Error errorCode =
      ApiLibraryStaticInitializer::GetConstInstance()
          .SanityCheckOptions(channelId->DeviceId,
                              pListenOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  if (pListenOptions->VLanTag != -1) {
    return CallErrorHandler(
        SC_ERR_NOT_SUPPORTED_OR_NOT_IMPLEMENTED,
        "SC_Listen does not currently support VLAN!");
  }
  if (!(pListenOptions->VLanTag == -1 ||
        (pListenOptions->VLanTag > 0 &&
         pListenOptions->VLanTag < 4095))) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "invalid VLAN tag %d (VLAN tag must be greater "
        "than 0 and less than 4095 or -1 for no VLAN).",
        pListenOptions->VLanTag);
  }
  if (pListenOptions->ListenTimeout < -1) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "invalid listen timeout %d (timeout must be -1 for "
        "infinite or greater than 0 seconds).",
        pListenOptions->ListenTimeout);
  }

  sc_listen_info_t listen_info;
  memset(&listen_info, 0, sizeof(listen_info));

  listen_info.cancel = 0;
  listen_info.local_ip_addr = 0;
  if (pListenOptions->LocalIpAddress != nullptr) {
    if (pListenOptions->LocalIpAddress[0] != '\0') {
      struct in_addr address;
      if (inet_aton(pListenOptions->LocalIpAddress,
                    &address) == 0) {
        return CallErrorHandler(
            SC_ERR_INVALID_ARGUMENT,
            "invalid local IP address %s. errno %d",
            pListenOptions->LocalIpAddress, errno);
      }
      listen_info.local_ip_addr = ntohl(address.s_addr);
    }
  }

  listen_info.local_port = localPort;
  listen_info.timeout = uint32_t(
      pListenOptions->ListenTimeout); // TODO: change
                                      // listen_info.timeout
                                      // to int32_t
  listen_info.vlan_tag = pListenOptions->VLanTag;

  if (ioctl(channelId->FileDescriptor, SC_IOCTL_LISTEN,
            &listen_info) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_LISTEN_OR_CANCEL_LISTEN,
        "failed to listen, errno = %d", errno);
  }

  // TODO FixMe: optionally handle EINTR (=4, interrupted
  // system call) here so we can call SC_CancelListen and
  // get a nice return code SC_ERR_LISTEN_CANCELLED_BY_USER.

  InitializeCachedConsumedBytes(
      channelId,
      channelId->DeviceId->TcpTxConsumedBytesFlowControl,
      channelId->pTxWindowFlowControl);

  channelId->LastPacketPlDmaAddress =
      SC_INVALID_PLDMA_ADDRESS;

  if (listen_info.remote_ip_address == 0) {
    channelId->ListenRemoteIpAddress = "";
  } else {
    struct in_addr ipAddress;
    ipAddress.s_addr = listen_info.remote_ip_address;
    channelId->ListenRemoteIpAddress = inet_ntoa(ipAddress);
  }
  pListenOptions->RemoteIpAddress =
      channelId->ListenRemoteIpAddress.c_str();
  pListenOptions->RemoteIpPort = listen_info.remote_ip_port;
  memcpy(pListenOptions->RemoteMacAddress,
         listen_info.remote_mac_address,
         sizeof(pListenOptions->RemoteMacAddress));

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_CancelListen(SC_ChannelId channelId,
                         void *pReserved) {
  SanityCheck(channelId);

  if (pReserved != nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "pReserved argument must be NULL!");
  }

  sc_listen_info_t listen_info;
  memset(&listen_info, 0, sizeof(listen_info));

  listen_info.cancel = 1;

  if (ioctl(channelId->FileDescriptor, SC_IOCTL_LISTEN,
            &listen_info) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_LISTEN_OR_CANCEL_LISTEN,
        "failed to cancel listen, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_Disconnect(SC_ChannelId channelId) {
  SanityCheck(channelId);

  sc_disconnect_info_t disconnect_info;
  disconnect_info.timeout =
      channelId->InternalConnectOptions.ConnectOptions
          .DisconnectTimeout;

  if (ioctl(channelId->FileDescriptor, SC_IOCTL_DISCONNECT,
            &disconnect_info) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_DISCONNECT,
        "failed to disconnect, errno = %d", errno);
  }

  channelId->LastPacketPlDmaAddress =
      SC_INVALID_PLDMA_ADDRESS;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetLastTcpAckNumber(SC_ChannelId channelId,
                                uint32_t *pLastAckNumber) {
  SanityCheck(channelId);

  uint32_t consumedBytes;
  ReadConsumedBytesAndTcbSndUna(channelId, consumedBytes,
                                *pLastAckNumber);

#ifdef DEBUG
  // Sanity check: get the same value from another register
  // via driver:
  uint32_t last_ack_num;
  if (ioctl(channelId->FileDescriptor, SC_IOCTL_LAST_ACK_NO,
            &last_ack_num)) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_GET_LAST_TCP_ACK_NUMBER,
        "failed to get last TCP ACK number, errno = %d",
        errno);
  }
  // assert(*pLastAckNumber == last_ack_num);
  // printf("*pLastAckNumber = %u, last_ack_num = %u\n",
  // *pLastAckNumber, last_ack_num);
#endif

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetIgmpOptions(SC_ChannelId channelId,
                           SC_IgmpOptions *pIgmpOptions) {
  if (channelId != nullptr &&
      channelId->ChannelType !=
          SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not UDP!");
  }

  GetOptions(IgmpOptions, channelId, pIgmpOptions);
}

/*****************************************************************************/

SC_Error SC_SetIgmpOptions(SC_ChannelId channelId,
                           SC_IgmpOptions *pIgmpOptions) {
  SC_DeviceId deviceId =
      channelId == nullptr ? nullptr : channelId->DeviceId;

  if (channelId != nullptr &&
      channelId->ChannelType !=
          SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not UDP!");
  }

  SetOptions(IgmpOptions, channelId, pIgmpOptions);
}

/*****************************************************************************/

SC_Error SC_IgmpJoin(SC_ChannelId channelId,
                     uint8_t networkInterface,
                     const char *group, uint16_t port,
                     SC_IgmpOptions *pIgmpOptions) {
  SanityCheck(channelId);

  if (group == nullptr || *group == '\0') {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "IGMP group cannot be NULL or empty!");
  }
  if (pIgmpOptions == nullptr) {
    pIgmpOptions =
        &channelId->InternalIgmpOptions.IgmpOptions;
  }

  SC_Error errorCode =
      ApiLibraryStaticInitializer::GetConstInstance()
          .SanityCheckOptions(channelId->DeviceId,
                              pIgmpOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }
  if (channelId->ChannelType !=
      SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not UDP!");
  }

  struct in_addr groupAddress;
  if (inet_aton(group, &groupAddress) == 0) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "invalid IGMP group address %s. errno %d", group,
        errno);
  }

  sc_igmp_info_t info;

  info.action = IGMP_JOIN;
  info.lane = networkInterface;
  info.group_address = ntohl(groupAddress.s_addr);
  info.ip_port_number = port;
  info.vlan_tag = pIgmpOptions->VLanTag;
  info.enable_multicast_bypass =
      uint8_t(pIgmpOptions->EnableMulticastBypass);
  info.source_vlan_ip_address = 0;

  if (ioctl(channelId->FileDescriptor, SC_IOCTL_IGMPINFO,
            &info)) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_JOIN_IGMP_GROUP,
        "failed to joing IGMP group, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_IgmpLeave(SC_ChannelId channelId,
                      uint8_t networkInterface,
                      const char *group, uint16_t port,
                      SC_IgmpOptions *pIgmpOptions) {
  SanityCheck(channelId);

  if (group == nullptr || *group == '\0') {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "IGMP group cannot be NULL or empty!");
  }
  if (pIgmpOptions == nullptr) {
    pIgmpOptions =
        &ApiLibraryStaticInitializer::GetInstance()
             .DefaultIgmpOptions.IgmpOptions;
  }

  SC_Error errorCode =
      ApiLibraryStaticInitializer::GetConstInstance()
          .SanityCheckOptions(channelId->DeviceId,
                              pIgmpOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  if (channelId->ChannelType !=
      SC_DMA_CHANNEL_TYPE_UDP_MULTICAST) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel type is not UDP!");
  }

  struct in_addr groupAddress;

  if (inet_aton(group, &groupAddress) == 0) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "invalid UDP group address %s. errno %d", group,
        errno);
  }

  sc_igmp_info_t info;

  info.action = IGMP_LEAVE;
  info.lane = networkInterface;
  info.group_address = ntohl(groupAddress.s_addr);
  info.ip_port_number = port;
  info.vlan_tag = pIgmpOptions->VLanTag;
  info.enable_multicast_bypass =
      uint8_t(pIgmpOptions->EnableMulticastBypass);
  info.source_vlan_ip_address = 0;

  if (ioctl(channelId->FileDescriptor, SC_IOCTL_IGMPINFO,
            &info)) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_LEAVE_IGMP_GROUP,
        "failed to leave IGMP group, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_DumpReceiveInfo(SC_ChannelId channelId,
                            const char *basefilename,
                            const char *infostr,
                            const SC_Packet *pLastPacket) {
  SanityCheck(channelId);

  FILE *pFile;
  char outstr[256];

  snprintf(outstr, 256, "%s.%d.dump", basefilename,
           getpid());

  if ((pFile = fopen(outstr, "wb")) == nullptr) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO,
        "failed to open file '%s' for writing receive "
        "info, errno = %d",
        basefilename, errno);
  };

  if (fwrite(channelId->pPRB, RECV_DMA_SIZE, 1, pFile) !=
      1) {
    fclose(pFile);
    return CallErrorHandler(
        SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO,
        "failed write the full dump of the receive buffer, "
        "errno = %d",
        errno);
  }

  fclose(pFile);

  // Get time of day
  time_t t;
  struct tm *tmp;

  t = time(NULL);
  if ((tmp = localtime(&t)) == nullptr) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO,
        "failed to get local time, errno = %d", errno);
  }

  snprintf(outstr, 256, "%s.%d.txt", basefilename,
           getpid());
  if ((pFile = fopen(outstr, "w")) == nullptr) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_DUMP_RECEIVE_INFO,
        "failed to open file '%s' for writing receive "
        "info, errno = %d",
        outstr, errno);
  };

  fprintf(pFile, "Filename: %s\n", outstr);

  if (strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z",
               tmp) == 0) {
    fprintf(stderr, "strftime returned 0");
  }

  fprintf(pFile, "Time: %s\n", outstr);
  fprintf(pFile, "Info: %s\n", infostr);
  fprintf(pFile, "Dma channel no: %d\n",
          channelId->DmaChannelNumber);
  fprintf(pFile, "Receive buffer size: 0x%x\n",
          RECV_DMA_SIZE);
  fprintf(
      pFile,
      "LastPacket cached pldma: 0x%016lx offset: 0x%lx\n",
      channelId->LastPacketPlDmaAddress,
      channelId->LastPacketPlDmaAddress -
          channelId->PrbPhysicalAddress);
  uint64_t lastPacket =
      channelId->pPointerListDMA
          ->lastPacket[channelId->DmaChannelNumber];
  fprintf(pFile,
          "LastPacket according to pldma: 0x%016lx offset: "
          "0x%lx\n",
          lastPacket,
          lastPacket - channelId->PrbPhysicalAddress);
  fprintf(pFile,
          "Last packet offset according to argument: "
          "offset: 0x%lx\n",
          reinterpret_cast<const uint8_t *>(pLastPacket) -
              channelId->pPRB);

  fclose(pFile);

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error
SC_GetLibraryAndDriverVersion(SC_DeviceId deviceId,
                              SC_Version *pLibraryVersion,
                              SC_Version *pDriverVersion) {
  SanityCheck(deviceId);

  if (pLibraryVersion != nullptr) {
    strncpy(pLibraryVersion->version, release_version,
            sizeof(pLibraryVersion->version) /
                    sizeof(pLibraryVersion->version[0]) -
                1);
  }
  if (pDriverVersion != nullptr) {
    if (ioctl(deviceId->FileDescriptor,
              SC_IOCTL_GET_DRIVER_VERSION,
              pDriverVersion)) {
      pDriverVersion->version[0] = '\0';
      return CallErrorHandler(
          SC_ERR_FAILED_TO_GET_DRIVER_VERSION,
          "failed to get device driver version, errno = %d",
          errno);
    }
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

void SC_PrintVersions(SC_DeviceId deviceId, FILE *pFile,
                      const char *app) {
  SanityCheck(deviceId);

  SC_Version lib_version, driver_version;
  if (deviceId != nullptr) {
    if (SC_GetLibraryAndDriverVersion(
            deviceId, &lib_version, &driver_version)) {
      strcpy(driver_version.version,
             "error: unable to read driver version number");
    }
    fprintf(pFile, "%s version '%s', driver version '%s'\n",
            app, lib_version.version,
            driver_version.version);
  } else {
    fprintf(pFile, "%s version '%s'\n", app,
            release_version);
  }

  SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

// Rx functions:

/**
 *  Get pointer to next received packet from the Packet Ring
 * Buffer (PRB). If the current packet is the last one in
 * the PRB before wrap around (indicated by the packet
 * header status bit SC_PKT_STATUS_END_OF_BUFFER) then
 *  return the start of the PRB (wrap around), otherwise
 * calculate start of next received packet based on current
 * packet header length field and that payloads less than 64
 * bytes are padded with zeroes up to that length.
 */
const SC_Packet *
SC_GetRawNextPacket(SC_ChannelId channelId,
                    const SC_Packet *pPreviousPacket) {
  SanityCheck(channelId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  if (unlikely(!pPreviousPacket ||
               pPreviousPacket->status &
                   SC_PKT_STATUS_END_OF_BUFFER)) {

    // Wrap around to start of PRB
    return reinterpret_cast<const SC_Packet *>(
        channelId->pPRB);
  }

  const std::size_t headerSize = (sizeof(SC_Packet) - 2);
  // FIXME implement the rounding in a nicer way
  const SC_Packet *pPossibleNextPacket =
      reinterpret_cast<const SC_Packet *>(
          reinterpret_cast<const uint8_t *>(
              pPreviousPacket) +
          ((headerSize + pPreviousPacket->len + 63U) &
           ~0x3f));

  return pPossibleNextPacket;
}

/*****************************************************************************/

bool PrintPlDma(const char *what, SC_ChannelId channelId,
                const sc_pldma_t *pPlDma,
                sc_pldma_t *pPreviousPlDma) {
  bool changed = false;

  for (std::size_t i = 0;
       i < ARRAY_SIZE(pPlDma->lastPacket); ++i) {
    if (pPlDma
            ->lastPacket[i] != pPreviousPlDma->lastPacket[i] /*&& pPreviousPlDma->lastPacket[i] != 0xDEADBEEFDEADBEEF*/) {
      printf("PLDMA %3lu %s: previous = 0x%09lX current = "
             "0x%09lX, PRB va 0x%lX pa 0x%09lX PRB DMA "
             "start address 0x%09lX\n",
             i, what != NULL ? what : "",
             pPreviousPlDma->lastPacket[i],
             pPlDma->lastPacket[i],
             (uint64_t)channelId->pPRB,
             channelId->PrbPhysicalAddress,
             channelId->PrbFpgaStartAddress);

      pPreviousPlDma->lastPacket[i] = pPlDma->lastPacket[i];
      changed = true;
    }
  }

  return changed;
}

/*****************************************************************************/

SC_Error SC_GetPointerListDMA(SC_DeviceId deviceId,
                              int16_t channelNumber,
                              uint64_t *pLastPacket) {
  SanityCheck(deviceId);

  const std::size_t numberOfChannels =
      ARRAY_SIZE(deviceId->Channels);
  if (channelNumber < 0 ||
      std::size_t(channelNumber) >= numberOfChannels) {
    *pLastPacket = 0xDEADBEEFDEADBEEF;
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel number %d is greater "
                            "than maximum allowed %lu",
                            channelNumber,
                            numberOfChannels - 1);
  }

  return SetLastErrorCode(GetPointerListDMA(
      deviceId, channelNumber, pLastPacket));
}

/*****************************************************************************/

SC_Error SC_SetPointerListDMA(SC_DeviceId deviceId,
                              int16_t channelNumber,
                              uint64_t lastPacket) {
  SanityCheck(deviceId);

  const std::size_t numberOfChannels =
      ARRAY_SIZE(deviceId->Channels);
  if (channelNumber < 0 ||
      std::size_t(channelNumber) >= numberOfChannels) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "channel number %d is greater "
                            "than maximum allowed %lu",
                            channelNumber,
                            numberOfChannels - 1);
  }

  return SetLastErrorCode(SetPointerListDMA(
      deviceId, channelNumber, lastPacket));
}

/*****************************************************************************/

/**
 *  Enter a busy-poll loop while waiting for the next
 * packet.
 *
 *  @brief  This function does not return until a packet has
 * been received.
 *
 *  @param channelId            Channel id
 *  @param pLastPacketReceived  Pointer to last packet
 * received
 *
 *  @return Pointer to received packet
 */
static const SC_Packet *
BusyPollForNextPacket(SC_ChannelId channelId,
                      const SC_Packet *pPreviousPacket) {
  // Get a pointer to the (potential!) next packet.
  // We don't know yet if there is a packet at this position
  // or not.
  const SC_Packet *pNextPacket =
      SC_GetRawNextPacket(channelId, pPreviousPacket);

  // Remember the current last value of PL DMA last packet
  // pointer.
  const uint64_t currentLastPacketPa =
      channelId->LastPacketPlDmaAddress;

  // Wait until the last packet from PL DMA changes, that
  // means we have at least one or possibly more new packets
  // in the packet ring buffer:
  while (
      currentLastPacketPa ==
      (channelId->LastPacketPlDmaAddress =
           channelId->pPointerListDMA
               ->lastPacket[channelId->DmaChannelNumber])) {
    //__asm__ __volatile__("pause");
  }

  // The potential pointer is now confirmed to point to an
  // actual received packet. There might well be more new
  // packets after this one but we have to process them in
  // order.
  return pNextPacket;
}

/*****************************************************************************/

const SC_Packet *
SC_GetNextPacket(SC_ChannelId channelId,
                 const SC_Packet *pPreviousPacket,
                 int64_t timeoutInMicroseconds) {
  ApiScopedTimer(API_TIMER_GET_NEXT_PACKET);

  SanityCheck(channelId);

  if (channelId->DeviceId->InternalCardInfo.CardInfo
          .IsToeConnectedToUserLogic &&
      channelId->ChannelType == SC_DMA_CHANNEL_TYPE_TCP) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "cannot receive from ETLM when TOE is "
                     "connected to user logic.");
    return nullptr;
  }

  COLLECT_STATISTICS(
      // Measure time between 2 consecutive calls to
      // SC_GetNextPacket:
      uint64_t now = Stopwatch::GetTimestamp();
      if (channelId->PreviousGetNextPacketTime != 0) {
        double sendBucketTime = double(
            now - channelId->PreviousGetNextPacketTime);
        channelId->GetNextPacketTime.Add(sendBucketTime);
      } channelId->PreviousGetNextPacketTime = now;)

  if (timeoutInMicroseconds > 0) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "timeouts not currently supported.");
    return nullptr;
  }

  SetLastErrorCode(SC_ERR_SUCCESS); // Just set success to
                                    // last error code.

  // Our cached lastPacketPa is not set - then update it
  if (unlikely(
          channelId->LastPacketPlDmaAddress ==
          SC_INVALID_PLDMA_ADDRESS)) // Only true for the
                                     // first packet
  {
    // Register that we will handle incoming traffic in
    // userspace
    int rc = ioctl(channelId->FileDescriptor,
                   SC_IOCTL_USERSPACE_RX, nullptr);
    if (unlikely(rc != 0)) {
      CallErrorHandler(SC_ERR_FAILED_TO_GET_NEXT_PACKET,
                       "Failed to register channel Rx "
                       "ownership on channel %d",
                       channelId->DmaChannelNumber);
      return nullptr;
    }

    channelId->LastPacketPlDmaAddress =
        channelId->pPointerListDMA->lastPacket
            [channelId->DmaChannelNumber]; // Initialize the
                                           // cache PL DMA.

    if (unlikely(channelId->LastPacketPlDmaAddress ==
                 SC_INVALID_PLDMA_ADDRESS)) {
      // No change, should we busy poll?
      if (timeoutInMicroseconds < 0) {
        return BusyPollForNextPacket(channelId,
                                     pPreviousPacket);
      }
      return nullptr; // No packet yet
    }
  }

  if (unlikely(
          pPreviousPacket ==
          nullptr)) // If lastPacketPa and pPreviousPacket
                    // == null then the first packet is
                    // placed at the start of the buffer.
  {
    return reinterpret_cast<SC_Packet *>(channelId->pPRB);
  }

  // From here the variable pNextPacket is expected location
  // of next packet and should be checked is not above the
  // current the cache version of the next pointer
  const SC_Packet *pNextPacket = SC_GetRawNextPacket(
      channelId,
      pPreviousPacket); // FIXME cache the 'nextPacket'
                        // pointer to avoid recalculating
                        // everytime

  if (reinterpret_cast<uint64_t>(pPreviousPacket) -
          reinterpret_cast<uint64_t>(channelId->pPRB) +
          channelId->PrbFpgaStartAddress ==
      channelId->LastPacketPlDmaAddress) {
    channelId->LastPacketPlDmaAddress =
        channelId->pPointerListDMA->lastPacket
            [channelId->DmaChannelNumber]; // Update cache
                                           // value of last
                                           // packet
    if (reinterpret_cast<uint64_t>(pPreviousPacket) -
            reinterpret_cast<uint64_t>(channelId->pPRB) +
            channelId->PrbFpgaStartAddress ==
        channelId->LastPacketPlDmaAddress) // recheck
    {
      // No change, should we busy poll?
      if (timeoutInMicroseconds < 0) {
        return BusyPollForNextPacket(channelId,
                                     pPreviousPacket);
      }
      return nullptr; // No packet yet
    }
  }
  // FIXME check packet length is not 0

  return pNextPacket;
}

/*****************************************************************************/

#if 0 // Not supported in this release

/** Enter a busy-poll loop while waiting for the next packet, with timeout.
 *
 *  @param channelId            Channel id
 *  @param pLastPacketReceived  Pointer to last packet received
 *  @param maxSpinCount         Number of busy poll loops to perform (timeout)
 *
 *  This function spins until a new packet has arrived or the number of busyloop iterations has been reached,
 *  whichever come first.
 *
 *  @return                     Pointer to received packet, NULL otherwise.
 */
SC_Packet * SC_BusyPollForNextPacketTimeout(SC_ChannelId channelId, const SC_Packet * current, uint32_t maxSpinCount)
{
    SC_Packet * nextPacket;
    uint32_t i;

    // Our cached lastPacketPa is not set - then update it
    if (unlikely(!channelId->lastPacketPa)) // Only true for the first packet
    {
        // Register that we will handle incoming traffic in userspace
        int ret = ioctl(channelId->fd, SC_IOCTL_USERSPACE_RX , NULL);
        if (ret)
        {
            fprintf(stderr, "Failed to register channel rx ownership on phys chan %d\n", channelId->channelNumber);
        }

        channelId->lastPacketPa = channelId->pl->lastPacket[channelId->channelNumber]; // Initialize the cache pldma.
        if (unlikely(!channelId->lastPacketPa))
            return 0; // No packet yet.
    }

    if (unlikely(!current)) // If lastPacketPa and current == 0 then the first packet is placed in the start of the buffer.
    {
        return (SC_Packet*)((uint8_t*)channelId->mem + RECV_DMA_OFFSET);
    }

    // From here the variable nextPacket is expected location of next packet and should be check is not above the current the cache version of the next pointer
    nextPacket = SC_GetRawNextPacket(channelId, current);

    for(i = 0; i < maxSpinCount; ++i)
    {
        if ((uint64_t)current - (uint64_t)((uint8_t*)channelId->mem + RECV_DMA_OFFSET) + (uint64_t)channelId->recvHwAddr != channelId->lastPacketPa)
            // FIXME check packet length is not 0
            return nextPacket;

        channelId->lastPacketPa = channelId->pl->lastPacket[channelId->channelNumber]; // Update cache value of last packet
    }

    return NULL;
}
#endif

/*****************************************************************************/

SC_Error SC_UpdateReceivePtr(SC_ChannelId channelId,
                             const SC_Packet *pPacket) {
  SanityCheck(channelId);

#if 1
  uint64_t value =
      channelId->PrbFpgaStartAddress +
      (reinterpret_cast<const uint8_t *>(pPacket) -
       channelId->pPRB);
  *channelId->pReadMarkerRegister = value;
#else
  uint64_t offset =
      uint64_t(reinterpret_cast<const uint8_t *>(pPacket) -
               channelId->pPRB);

  if (ioctl(channelId->FileDescriptor,
            SC_IOCTL_SET_READMARK, &offset) !=
      0) // FIXME why this goes through driver everytime? It
         // looks like this was to alternate between reading
         // in userspace and reading through read(). But we
         // don't support alternating any longer.
  {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_UPDATE_RECEIVE_PTR,
        "failed to get TCP status, errno = %d", errno);
  }
#endif

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_TcpState SCI_GetTcpState(SC_DeviceId deviceId,
                            uint16_t toeChannel) {
  SanityCheck(deviceId);

  if (toeChannel >= SC_MAX_TCP_CHANNELS) {
    return SC_TcpState(
        CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                         "TOE channel number %u must be "
                         "less than maximum of %u",
                         toeChannel, SC_MAX_TCP_CHANNELS));
  }

  uint32_t status = toeChannel;
  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_GET_TCP_STATUS, &status) != 0) {
    return SC_TcpState(CallErrorHandler(
        SC_ERR_FAILED_TO_GET_TCP_STATUS,
        "failed to get TCP status, errno = %d", errno));
  }

  SetLastErrorCode(SC_ERR_SUCCESS);

  return SC_TcpState(status);
}

/*****************************************************************************/

static int ZERO =
    0; // For eliminating compiler warnings about comparing
       // an unsigned int with >= 0 which is always true.

SC_ChannelType
SCI_GetChannelType(SC_DeviceId deviceId,
                   uint16_t dmaChannelNumber) {
  SanityCheck(deviceId);

  if (dmaChannelNumber >= SC_MAX_CHANNELS) {
    return SC_ChannelType(CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "FPGA DMA channel number %u must be less than "
        "maximum of %u",
        dmaChannelNumber, SC_MAX_CHANNELS));
  }

  SetLastErrorCode(SC_ERR_SUCCESS);

  if (dmaChannelNumber + ZERO >= FIRST_UL_CHANNEL &&
      dmaChannelNumber <
          FIRST_UL_CHANNEL + SC_MAX_ULOGIC_CHANNELS) {
    return SC_DMA_CHANNEL_TYPE_USER_LOGIC;
  } else if (dmaChannelNumber + ZERO >= FIRST_OOB_CHANNEL &&
             dmaChannelNumber <
                 FIRST_OOB_CHANNEL + SC_MAX_OOB_CHANNELS) {
    return SC_DMA_CHANNEL_TYPE_STANDARD_NIC;
  } else if (dmaChannelNumber >= FIRST_MONITOR_CHANNEL &&
             dmaChannelNumber <
                 FIRST_MONITOR_CHANNEL +
                     SC_MAX_MONITOR_CHANNELS) {
    return SC_DMA_CHANNEL_TYPE_MONITOR;
  } else if (dmaChannelNumber >= FIRST_UDP_CHANNEL &&
             dmaChannelNumber <
                 FIRST_UDP_CHANNEL + SC_MAX_UDP_CHANNELS) {
    return SC_DMA_CHANNEL_TYPE_UDP_MULTICAST;
  } else if (dmaChannelNumber >= FIRST_TCP_CHANNEL &&
             dmaChannelNumber <
                 FIRST_TCP_CHANNEL + SC_MAX_TCP_CHANNELS) {
    return SC_DMA_CHANNEL_TYPE_TCP;
  } else if (dmaChannelNumber >= FIRST_UNUSED_CHANNEL &&
             dmaChannelNumber <
                 FIRST_UNUSED_CHANNEL +
                     SC_MAX_UNUSED_CHANNELS) {
    return SC_DMA_CHANNEL_TYPE_UNUSED;
  }

  return SC_ChannelType(
      CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                       "cannot determine channel type of "
                       "FPGA DMA channel %u",
                       dmaChannelNumber));
}

/*****************************************************************************/

const char *
SCI_GetChannelTypeName(SC_ChannelType channelType) {
  switch (channelType) {
  case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
    return "User Logic";
  case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
    return "NIC";
  case SC_DMA_CHANNEL_TYPE_MONITOR:
    return "Monitor";
  case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
    return "UDP";
  case SC_DMA_CHANNEL_TYPE_TCP:
    return "TCP";
  case SC_DMA_CHANNEL_TYPE_UNUSED:
    return "Unused";
  case SC_DMA_CHANNEL_TYPE_NONE:
    break;
  }

  CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                   "Invalid channel type %u", channelType);
  return " *** INVALID CHANNEL TYPE";
}

/*****************************************************************************/

SC_TcpState SC_GetTcpState(SC_ChannelId channelId) {
  SanityCheck(channelId);

  if (channelId->ChannelType != SC_DMA_CHANNEL_TYPE_TCP) {
    return SC_TcpState(CallErrorHandler(
        SC_ERR_FAILED_TO_GET_TCP_STATUS,
        "this is not a ETLM TCP channel!"));
  }

  return SCI_GetTcpState(
      channelId->DeviceId,
      uint16_t(channelId->DmaChannelNumber -
               FIRST_TCP_CHANNEL));
}

/*****************************************************************************/

const char *SC_GetTcpStateAsString(SC_TcpState tcpState) {
  SetLastErrorCode(SC_ERR_SUCCESS);

  switch (tcpState) {
  case SC_TCP_NONE:
    return "NONE";
  case SC_TCP_CLOSED:
    return "CLOSED";
  case SC_TCP_LISTEN:
    return "LISTEN";
  case SC_TCP_SYN_SENT:
    return "SYN_SENT";
  case SC_TCP_SYN_RECV:
    return "SYN_RECV";
  case SC_TCP_ESTABLISHED:
    return "ESTABLISHED";
  case SC_TCP_CLOSE_WAIT:
    return "CLOSE_WAIT";
  case SC_TCP_FIN_WAIT1:
    return "FIN_WAIT1";
  case SC_TCP_CLOSING:
    return "CLOSING";
  case SC_TCP_LAST_ACK:
    return "LAST_ACK";
  case SC_TCP_FIN_WAIT2:
    return "FIN_WAIT2";
  case SC_TCP_TIME_WAIT:
    return "TIME_WAIT";
  case SC_TCP_CLOSED_DATA_PENDING:
    return "CLOSED_DATA_PENDING";
  default:
    break;
  }

  SetLastErrorCode(SC_ERR_INVALID_TCP_STATE);

  return "*** UNKNOWN ***";
}

/*****************************************************************************/

SC_Error SC_TcpAbort(SC_ChannelId channelId) {
  if (ioctl(channelId->FileDescriptor,
            SC_IOCTL_ABORT_ETLM_CHANNEL) != 0) {
    return CallErrorHandler(
        SC_ERR_FAILED_TO_ABORT_ETLM_CHANNEL,
        "failed to abort ETLM channel %u, errno = %d",
        channelId->DmaChannelNumber, errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

int SC_GetFileDescriptor(SC_DeviceId deviceId) {
  SanityCheck(deviceId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  return deviceId->FileDescriptor;
}

/*****************************************************************************/

uint8_t *SC_GetBucketPayload(SC_Bucket *pBucket) {
  SetLastErrorCode(SC_ERR_SUCCESS);

  return pBucket->pUserSpaceBucket->data;
}

/*****************************************************************************/

int SC_GetBucketSize(SC_Bucket *pBucket) {
  SetLastErrorCode(SC_ERR_SUCCESS);

  return sizeof(pBucket->pUserSpaceBucket->data);
}

/*****************************************************************************/

int32_t SC_GetMaxNumberOfBuckets(SC_ChannelId channelId) {
  SanityCheck(channelId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  return int32_t(ARRAY_SIZE(channelId->Buckets) - 1);
}

/*****************************************************************************/

int16_t SC_GetMaxBucketPayloadSize(SC_ChannelId channelId) {
  if (channelId == nullptr) {
    SetLastErrorCode(SC_ERR_SUCCESS);

    return SC_MAX_MTU_SIZE;
  }

  SanityCheck(channelId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  // Possible future extension: variable max bucket size.
  return SC_MAX_MTU_SIZE;
}

/*****************************************************************************/

int32_t SC_GetNumberOfChannels(SC_DeviceId deviceId) {
  SanityCheck(deviceId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  return int32_t(ARRAY_SIZE(deviceId->Channels));
}

/*****************************************************************************/

volatile uint64_t *
SC_GetUserLogicRegistersBase(SC_DeviceId deviceId) {
  SanityCheck(deviceId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  return deviceId->pUserLogicRegisters;
}

/*****************************************************************************/

volatile uint64_t *EkalineGetWcBase(SC_DeviceId deviceId) {
  SanityCheck(deviceId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  return deviceId->pEkalineWcBase;
}

/*****************************************************************************/

// BAR 2 user logic 64-bit registers
static const uint32_t NumberOfUserLogicRegisters =
    uint32_t(BAR2_REGS_SIZE / sizeof(uint64_t));

/*****************************************************************************/

uint32_t SCI_GetNumberOfUserLogicRegisters() {
  SetLastErrorCode(SC_ERR_SUCCESS);

  return NumberOfUserLogicRegisters;
}

/*****************************************************************************/

SC_Error SC_WriteUserLogicRegister(SC_DeviceId deviceId,
                                   uint32_t index,
                                   uint64_t value) {
  SanityCheck(deviceId);

  if (index >= NumberOfUserLogicRegisters) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "user logic register index %u "
                            "is out of bounds of [0,%u)",
                            index,
                            NumberOfUserLogicRegisters);
  }

  *(deviceId->pUserLogicRegisters + index) = value;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_ReadUserLogicRegister(SC_DeviceId deviceId,
                                  uint32_t index,
                                  uint64_t *pValue) {
  SanityCheck(deviceId);

  if (index >= NumberOfUserLogicRegisters) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "user logic register index %u "
                            "is out of bounds of [0,%u)",
                            index,
                            NumberOfUserLogicRegisters);
  }

  *pValue = *(deviceId->pUserLogicRegisters + index);

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

int32_t SC_GetChannelNumber(SC_ChannelId channelId) {
  SanityCheck(channelId);

  if (channelId->DmaChannelNumber < 0 ||
      channelId->DmaChannelNumber >= SC_MAX_CHANNELS) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "no channel number available; "
                            "channel is not allocated!");
  }

  SetLastErrorCode(SC_ERR_SUCCESS);

  switch (channelId->ChannelType) {
  case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
    return channelId->DmaChannelNumber - FIRST_UL_CHANNEL;
  case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
    return channelId->DmaChannelNumber - FIRST_OOB_CHANNEL;
  case SC_DMA_CHANNEL_TYPE_TCP:
    return channelId->DmaChannelNumber - FIRST_TCP_CHANNEL;
  case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
    return channelId->DmaChannelNumber - FIRST_UDP_CHANNEL;
  case SC_DMA_CHANNEL_TYPE_MONITOR:
    return channelId->DmaChannelNumber -
           FIRST_MONITOR_CHANNEL;
  case SC_DMA_CHANNEL_TYPE_NONE:
  case SC_DMA_CHANNEL_TYPE_UNUSED:
    break;
  }

  return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                          "unsupported channel type %d",
                          channelId->ChannelType);
}

/*****************************************************************************/

int32_t SC_GetDmaChannelNumber(SC_ChannelId channelId) {
  SanityCheck(channelId);

  if (channelId->DmaChannelNumber < 0 ||
      channelId->DmaChannelNumber >= SC_MAX_CHANNELS) {
    return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                            "no channel number available; "
                            "channel is not allocated!");
  }

  SetLastErrorCode(SC_ERR_SUCCESS);

  return channelId->DmaChannelNumber;
}

/*****************************************************************************/

int32_t SC_GetDmaChannelNumberFromVirtualChannelNumber(
    SC_ChannelType channelType,
    uint16_t virtualChannelNumber) {
  switch (channelType) {
  case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
    if (virtualChannelNumber >= SC_MAX_ULOGIC_CHANNELS) {
      return CallErrorHandler(
          SC_ERR_INVALID_ARGUMENT,
          "invalid virtual user logic channel number %u",
          virtualChannelNumber);
    }
    return FIRST_UL_CHANNEL + virtualChannelNumber;
  case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
    if (virtualChannelNumber >= SC_MAX_OOB_CHANNELS) {
      return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                              "invalid virtual standard "
                              "NIC (OOB) channel number %u",
                              virtualChannelNumber);
    }
    return FIRST_OOB_CHANNEL + virtualChannelNumber;
  case SC_DMA_CHANNEL_TYPE_MONITOR:
    if (virtualChannelNumber >= SC_MAX_MONITOR_CHANNELS) {
      return CallErrorHandler(
          SC_ERR_INVALID_ARGUMENT,
          "invalid virtual monitor channel number %u",
          virtualChannelNumber);
    }
    return FIRST_MONITOR_CHANNEL + virtualChannelNumber;
  case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
    if (virtualChannelNumber >= SC_MAX_UDP_CHANNELS) {
      return CallErrorHandler(
          SC_ERR_INVALID_ARGUMENT,
          "invalid virtual UDP channel number %u",
          virtualChannelNumber);
    }
    return FIRST_UDP_CHANNEL + virtualChannelNumber;
  case SC_DMA_CHANNEL_TYPE_TCP:
    if (virtualChannelNumber >= SC_MAX_TCP_CHANNELS) {
      return CallErrorHandler(
          SC_ERR_INVALID_ARGUMENT,
          "invalid virtual TCP channel number %u",
          virtualChannelNumber);
    }
    return FIRST_TCP_CHANNEL + virtualChannelNumber;
  case SC_DMA_CHANNEL_TYPE_UNUSED:
  case SC_DMA_CHANNEL_TYPE_NONE:
    break;
  }

  return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                          "invalid channel type %d ",
                          channelType);
}

/*****************************************************************************/

int64_t SC_GetPacketRingBufferSize(SC_ChannelId channelId) {
  SanityCheck(channelId);

  return RECV_DMA_SIZE;
}

/*****************************************************************************/

uint16_t SC_GetNetworkInterface(const SC_Packet *pPacket) {
  return (pPacket->status >> 8) & 0xF;
}

/*****************************************************************************/

SC_DeviceId SC_GetDeviceId(SC_ChannelId channelId) {
  SanityCheck(channelId);

  return channelId->DeviceId;
}

/*****************************************************************************/

const uint8_t *SC_GetRawPayload(const SC_Packet *pPacket) {
  return sc_get_raw_payload(pPacket);
}

/*****************************************************************************/

const uint8_t *SC_GetTcpPayload(const SC_Packet *pPacket) {
  return sc_get_tcp_payload(pPacket);
}

/*****************************************************************************/

const uint8_t *SC_GetUdpPayload(const SC_Packet *pPacket) {
  return sc_get_udp_payload(pPacket);
}

/*****************************************************************************/

const uint8_t *
SC_GetUserLogicPayload(const SC_Packet *pPacket) {
  return sc_get_ulogic_payload(pPacket);
}

/*****************************************************************************/

const uint8_t *
SC_GetPacketPayload(SC_ChannelId channelId,
                    const SC_Packet *pPacket) {
  SanityCheck(channelId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  if (channelId == nullptr) {
    return sc_get_raw_payload(pPacket);
  }

  switch (channelId->ChannelType) {
  case SC_DMA_CHANNEL_TYPE_TCP:
    return sc_get_tcp_payload(pPacket);
  case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
    return sc_get_udp_payload(pPacket);
  case SC_DMA_CHANNEL_TYPE_MONITOR:
  case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
    return sc_get_ulogic_payload(pPacket);
  case SC_DMA_CHANNEL_TYPE_NONE:
  case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
  case SC_DMA_CHANNEL_TYPE_UNUSED:
    break;
  }

  CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                   "receive packets not supported or other "
                   "invalid argument on channel %u",
                   channelId->DmaChannelNumber);
  return nullptr;
}

/*****************************************************************************/

uint16_t SC_GetTcpPayloadLength(const SC_Packet *pPacket) {
  return sc_get_tcp_payload_length(pPacket);
}

/*****************************************************************************/

uint16_t SC_GetUdpPayloadLength(const SC_Packet *pPacket) {
  return sc_get_udp_payload_length(pPacket);
}

/*****************************************************************************/

uint16_t
SC_GetUserLogicPayloadLength(const SC_Packet *pPacket) {
  return sc_get_ulogic_payload_length(pPacket);
}

/*****************************************************************************/

uint16_t SC_GetRawPayloadLength(const SC_Packet *pPacket) {
  return sc_get_raw_payload_length(pPacket);
}

/*****************************************************************************/

int16_t
SC_GetPacketPayloadLength(SC_ChannelId channelId,
                          const SC_Packet *pPacket) {
  SanityCheck(channelId);

  SetLastErrorCode(SC_ERR_SUCCESS);

  if (channelId == nullptr) {
    return int16_t(sc_get_raw_payload_length(pPacket));
  }

  switch (channelId->ChannelType) {
  case SC_DMA_CHANNEL_TYPE_TCP:
    return int16_t(sc_get_tcp_payload_length(pPacket));
  case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
    return int16_t(sc_get_udp_payload_length(pPacket));
  case SC_DMA_CHANNEL_TYPE_MONITOR:
  case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
    return int16_t(sc_get_ulogic_payload_length(pPacket));
  case SC_DMA_CHANNEL_TYPE_NONE:
  case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
  case SC_DMA_CHANNEL_TYPE_UNUSED:
    break;
  }

  return CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "receive packets not supported or other invalid "
      "argument on channel %u",
      channelId->DmaChannelNumber);
}

/*****************************************************************************/

/**
 *  Build Tx DMA list descriptor and write it into Tx DMA
 * FIFO (normal or priority).
 *
 *  @param  channelId               Channel id to write to.
 *  @param  listPhysicalStart       Physical address of the
 * list for use by the FPGA DMA (address in kernel space).
 *  @param  numberOfPackets         Number of packets in the
 * list.
 *  @param  priority                If true select the
 * priority FIFO instead of the normal FIFO.
 *  @param  pCurrentList            Pointer to the Tx DMA
 * list information structure.
 */
static void SendListToFIFO(SC_ChannelId channelId,
                           uint64_t listPhysicalStart,
                           uint16_t numberOfPackets,
                           bool priority,
                           TxDmaList *pCurrentList) {
  SanityCheck(channelId);

  // Each channel has its own write register 16 (0x10) bytes
  // apart only contains packets for the same destination.
  // When we move to multiple channels per context, FW and
  // SW will figure a way to make it possible to send a list
  // of diverse destination packets.
  int channelNumber = channelId->DmaChannelNumber;

  uint64_t descriptor = listPhysicalStart;
  descriptor |= (uint64_t(numberOfPackets))
                << 40; // 16bits number of packets
  descriptor |= (uint64_t(channelId->DmaChannelNumber))
                << 56;               // channel id
  descriptor |= (uint64_t(0)) << 63; // 0=packet list

  uint64_t sendRegisterBase = priority
                                  ? SENDREG_PRIORITY_BASE
                                  : SENDREG_NORMAL_BASE;

  // Channel register address in kernel space
  volatile uint64_t *channelRegister =
      reinterpret_cast<uint64_t *>(
          static_cast<uint8_t *>(
              channelId->DeviceId->pBar0Registers) +
          sendRegisterBase + (channelNumber * 0x10));

  if (LOG(channelId->DeviceId,
          LOG_TX_DMA | LOG_REG_WRITE)) {
    Log(channelId->DeviceId,
        "%s: list of %u packets, writing 0x%016lX to "
        "register 0x%p on channel %u\n",
        __func__, numberOfPackets, descriptor,
        static_cast<volatile void *>(channelRegister),
        channelNumber);
#if 1
    uint32_t listTotalPayloadLength = 0;
    for (uint16_t i = 0; i < numberOfPackets; ++i) {
      volatile uint64_t *pUserSpaceIndexer =
          pCurrentList->pUserSpaceStart + i;
      descriptor = *pUserSpaceIndexer;
      uint16_t length = uint16_t(descriptor >> 40);
      listTotalPayloadLength += length;
      Log(channelId->DeviceId,
          "    %u, %p: 0x%lx, length %4u (0x%x), total "
          "payload %u\n",
          i,
          static_cast<volatile void *>(pUserSpaceIndexer),
          descriptor, length, length,
          listTotalPayloadLength);
    }
#endif
  }

  // usleep(1000);
  *channelRegister = descriptor;

#if 0
    if (LOGX(channelId->deviceId, LOG_TX_DMA | LOG_REG_WRITE))
    {
        do
        {
            sleep(1);
            Log(channelId->deviceId, "    %p: 0x%lx\n", pCurrentList->_pUserSpaceStart, *pCurrentList->_pUserSpaceStart);

        } while (*pCurrentList->_pUserSpaceStart != 0);
    }
#endif
}

/*****************************************************************************/

/**
 *  Make a Tx DMA descriptor for a single packet.
 *
 *  @param  sendParameters                      Reference to
 * send parameters @ref SendParameters.
 *
 *  @return     The packet descriptor that can be queued
 * into the Tx DMA FIFO.
 */
static inline uint64_t MakeSinglePacketDescriptor(
    const SendParameters &sendParameters) {
  uint64_t descriptor =
      sendParameters.PayloadPhysicalAddress;
  descriptor |= (uint64_t(sendParameters.PayloadLength))
                << 40; // 16bits length
  descriptor |= (uint64_t(sendParameters.DmaChannelNumber))
                << 56;               // channel number
  descriptor |= (uint64_t(1)) << 63; // 1=single packet
  return descriptor;
}

/*****************************************************************************/

/**
 *  Build Tx DMA single packet descriptor and write it into
 * Tx DMA FIFO.
 *
 *  @param  channelId                           Channel
 * identifier.
 *  @param  sendParameters                      Reference to
 * send parameters @ref SendParameters.
 */
static inline void
SendPacketToFIFO(SC_ChannelId channelId,
                 const SendParameters &sendParameters) {
  SanityCheck(channelId);

  // Each channel has its own write register 16 (0x10) bytes
  // apart only contains packets for the same destination.
  // When we move to multiple channels per context, FW and
  // SW will figure a way to make it possible to send a list
  // of diverse destination packets.
  uint64_t descriptor =
      MakeSinglePacketDescriptor(sendParameters);
  int16_t dmaChannelNumber = channelId->DmaChannelNumber;
  volatile uint64_t *channelRegister;
  size_t registerOffset;
  uint64_t sendRegisterBase = sendParameters.Priority
                                  ? SENDREG_PRIORITY_BASE
                                  : SENDREG_NORMAL_BASE;
  registerOffset =
      sendRegisterBase + (dmaChannelNumber * 0x10);
  channelRegister = reinterpret_cast<uint64_t *>(
      static_cast<uint8_t *>(
          channelId->DeviceId->pBar0Registers) +
      sendRegisterBase + (dmaChannelNumber * 0x10));
  *channelRegister = descriptor;

  if (LOG(channelId->DeviceId,
          LOG_TX_DMA | LOG_REG_WRITE)) {
    Log(channelId->DeviceId,
        "\n%s: writing 0x%016lX to register 0x%p (offset "
        "0x%lX) on DMA channel %d\n",
        __func__, descriptor,
        static_cast<volatile void *>(channelRegister),
        registerOffset, dmaChannelNumber);
  }
}

/*****************************************************************************/

static uint32_t ToLittleEndian(uint32_t bigEndian) {
  return ((bigEndian & 0x000000FF) << 24) |
         ((bigEndian & 0x0000FF00) << 8) |
         ((bigEndian & 0x00FF0000) >> 8) |
         ((bigEndian & 0xFF000000) >> 24);
}

/*****************************************************************************/

SC_Error
SCI_IncrementReferenceCount(SC_DeviceId deviceId, int which,
                            int64_t *pReferenceCount) {
  sc_reference_count_t referenceCount;
  referenceCount.which = static_cast<
      sc_reference_count_t::reference_count_which_t>(which);
  referenceCount.what =
      sc_reference_count_t::SC_REFERENCE_COUNT_INCREMENT;

  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_REFERENCE_COUNT,
            &referenceCount) != 0) {
    return CallErrorHandler(
        SC_ERR_REFERENCE_COUNT_FAILED,
        "Reference count increment of %d failed, errno %d",
        referenceCount.which, errno);
  }

  *pReferenceCount = referenceCount.reference_count;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error
SCI_DecrementReferenceCount(SC_DeviceId deviceId, int which,
                            int64_t *pReferenceCount) {
  sc_reference_count_t referenceCount;
  referenceCount.which = static_cast<
      sc_reference_count_t::reference_count_which_t>(which);
  referenceCount.what =
      sc_reference_count_t::SC_REFERENCE_COUNT_DECREMENT;

  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_REFERENCE_COUNT,
            &referenceCount) != 0) {
    return CallErrorHandler(
        SC_ERR_REFERENCE_COUNT_FAILED,
        "Reference count decrement of %d failed, errno %d",
        referenceCount.which, errno);
  }

  *pReferenceCount = referenceCount.reference_count;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_ActivateDemoDesign(
    SC_DeviceId deviceId, SC_DemoDesign demoDesign,
    uint64_t parameter,
    SC_DemoDesignOptions *pDemoDesignOptions) {
  SanityCheck(deviceId);

  if (pDemoDesignOptions != nullptr &&
      pDemoDesignOptions->StructSize !=
          sizeof(*pDemoDesignOptions)) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "%s struct size %u is wrong, expected %lu",
        nameof(pDemoDesignOptions),
        pDemoDesignOptions->StructSize,
        sizeof(*pDemoDesignOptions));
  }

  try {
    switch (demoDesign) {
    // User logic generic demo.
    //
    // Register 0 parameter activates/parameter=0
    // deactivates user logic generic demo design.
    //
    //  For available demos see internal Software Interfaces
    //  Chapter 12 User logic demos
    //
    case SC_DEMO_USER_LOGIC_GENERIC: {
      uint32_t registerIndex;
      int64_t referenceCount;

      if (parameter) {
        SC_Error errorCode = SCI_IncrementReferenceCount(
            deviceId,
            sc_reference_count_t::
                SC_REFERENCE_COUNT_USER_LOGIC_DEMO,
            &referenceCount);
        if (errorCode != SC_ERR_SUCCESS) {
          return errorCode;
        }
        if (referenceCount > 1) {
          // Already enabled, do not do anything.
          return SetLastErrorCode(SC_ERR_SUCCESS);
        }
      } else {
        SC_Error errorCode = SCI_DecrementReferenceCount(
            deviceId,
            sc_reference_count_t::
                SC_REFERENCE_COUNT_USER_LOGIC_DEMO,
            &referenceCount);
        if (errorCode != SC_ERR_SUCCESS) {
          return errorCode;
        }
        if (referenceCount > 0) {
          // Still in use, do not do anything
          return SetLastErrorCode(SC_ERR_SUCCESS);
        }
      }

      if (pDemoDesignOptions == nullptr) {
        registerIndex = 0;
      } else {
        registerIndex = pDemoDesignOptions->RegisterIndex;
      }
      return SetLastErrorCode(SC_WriteUserLogicRegister(
          deviceId, registerIndex, parameter));
    }

    // User logic latency test.
    //
    // - test is started by writing to user logic register 0
    // on channel 7
    // - user logic sends a packet of required payload size
    // into the PRB
    // - test software here echoes the received packet back
    // as fast as possible
    //   directly from the PRB (no copying to bucket etc.).
    // - user logic replies with a packet that has the round
    // trip time in units of 4 ns.
    //
    //  Warm up rounds are required in the beginning to
    //  settle things down, otherwise the first round trips
    //  will be slow.
    //
    //  Different payload lengths are tried and an average
    //  time reported for each length.
    //
    case SC_DEMO_USER_LOGIC_RTT_LATENCY: {
      if (pDemoDesignOptions != nullptr) {
        return CallErrorHandler(
            SC_ERR_INVALID_ARGUMENT,
            "%s argument expected to be NULL instead of %p",
            nameof(pDemoDesignOptions),
            static_cast<void *>(pDemoDesignOptions));
      }

      const int userChannelNumber = 7;
      SC_ChannelId channelId = SC_AllocateUserLogicChannel(
          deviceId, userChannelNumber, nullptr);
      if (channelId == nullptr) {
        return CallErrorHandler(
            ApiLibraryStaticInitializer::GetConstInstance()
                .LastErrorCode,
            "failed to allocate user channel %d",
            userChannelNumber);
      }

      const SC_Packet *pPreviousPacket = nullptr;
      const SC_Packet *pPrbStart =
          reinterpret_cast<const SC_Packet *>(
              channelId->pPRB);

      if (parameter < 10) {
        parameter = 10;
      }

      SC_LibraryOptions libraryOptions;
      SC_Error errorCode =
          SC_GetLibraryOptions(&libraryOptions);
      if (errorCode != SC_ERR_SUCCESS) {
        return errorCode;
      }
      libraryOptions.EnforceBucketsSanity = false;
      errorCode = SC_SetLibraryOptions(&libraryOptions);
      if (errorCode != SC_ERR_SUCCESS) {
        return errorCode;
      }

      const uint64_t NumberOfWarmUpRounds = 5;
      const uint64_t NumberOfIterations =
          NumberOfWarmUpRounds + parameter;
      std::map<uint16_t, Statistics> latencies;
      std::map<uint16_t, SC_Bucket *> buckets;

      const uint16_t LengthIncrement = 250;

      for (uint16_t length = 32; length <= 2032;
           length = uint16_t(length + LengthIncrement)) {
        latencies[length].Clear();
        SC_Bucket *pBucket = SC_GetBucket(channelId, 0);
        if (pBucket == nullptr) {
          return ApiLibraryStaticInitializer::
              GetConstInstance()
                  .LastErrorCode;
        }
        buckets[length] = pBucket;
      }

      const bool sendFromRingBuffer = false;

      int totalTestCount = 0;
      for (uint64_t iteration = 0;
           iteration < NumberOfIterations; ++iteration) {
        for (std::map<uint16_t, Statistics>::iterator
                 pLatency = latencies.begin();
             pLatency != latencies.end(); ++pLatency) {
          uint16_t length = pLatency->first;

          // Start latency measurement:
          errorCode = SC_WriteUserLogicRegister(
              deviceId, 0,
              (uint64_t(length) << 48) + 0x200);
          if (errorCode != SC_ERR_SUCCESS) {
            return errorCode;
          }

          // Wait for the 1st reply packet:
          const SC_Packet *pNextPacket = SC_GetNextPacket(
              channelId, pPreviousPacket,
              SC_Timeout::SC_TIMEOUT_INFINITE);
          pPreviousPacket = pNextPacket;

          const std::ptrdiff_t expectedPacketHeaderSize =
              16;
          const uint8_t *pPayload =
              reinterpret_cast<const uint8_t *>(
                  pNextPacket) +
              expectedPacketHeaderSize;
          if (length !=
              SC_GetUserLogicPayloadLength(pNextPacket)) {
            return CallErrorHandler(
                SC_ERR_DEMO_DESIGN_FAILED,
                "packet length %u received instead of "
                "expected length %u",
                SC_GetUserLogicPayloadLength(pNextPacket),
                length);
          }
          std::ptrdiff_t receivedPacketHeaderSize =
              SC_GetUserLogicPayload(pNextPacket) -
              reinterpret_cast<const uint8_t *>(
                  pNextPacket);
          if (receivedPacketHeaderSize !=
              expectedPacketHeaderSize) {
            return CallErrorHandler(
                SC_ERR_DEMO_DESIGN_FAILED,
                "packet header length %lu received instead "
                "of expected header length %lu",
                receivedPacketHeaderSize,
                expectedPacketHeaderSize);
          }

          if (sendFromRingBuffer) {
            // Echo the received packet back directly from
            // the PRB:

            // FIXME, TODO: there is currently a problem
            // with sending received packets directly from
            // the PRB: FPGA acknowledges sent packet by
            // writing 8 zero bytes directly after the
            // packet and at the same time the next received
            // packet might be writing at the same location.

            uint64_t offset =
                reinterpret_cast<uint64_t>(pPayload) -
                reinterpret_cast<uint64_t>(pPrbStart);
            uint64_t prbPhysicalAddress =
                channelId->PrbPhysicalAddress + offset;

            SendParameters sendParameters(
                channelId, length, prbPhysicalAddress,
                false);

            SendPacketToFIFO(channelId, sendParameters);
          } else {
            // Send with a bucket, payload content is
            // random:
            errorCode = SC_SendBucket(
                channelId, buckets[length], length,
                SC_SendOptions(SC_SEND_SINGLE_PACKET |
                               SC_SEND_WAIT_FOR_PIPE_FREE),
                nullptr);
            if (errorCode != SC_ERR_SUCCESS) {
              return errorCode;
            }
          }

          // Read for 4 byte time stamp in units of 4 ns:
          pNextPacket = SC_GetNextPacket(
              channelId, pPreviousPacket, -1);
          pPreviousPacket = pNextPacket;
          ++totalTestCount;

          const uint8_t *pNextPayload =
              SC_GetUserLogicPayload(pNextPacket);
          uint32_t latencyTime =
              *reinterpret_cast<const uint32_t *>(
                  pNextPayload);
          latencyTime = ToLittleEndian(latencyTime);
          uint16_t payloadLength =
              SC_GetUserLogicPayloadLength(pNextPacket);
          if (payloadLength != 4) {
            printf("%d: pNextPacket %p, "
                   "SC_GetUserLogicPayloadLength("
                   "pNextPacket) = %u %s\n",
                   totalTestCount,
                   static_cast<const void *>(pNextPacket),
                   payloadLength,
                   payloadLength == 4 ? "" : " ?????");
            SCI_HexDumpStdout(
                reinterpret_cast<const uint8_t *>(
                    pNextPacket) -
                    64,
                160, 16, true);
          }
          // assert(SC_GetUserLogicPayloadLength(pNextPacket)
          // == 4); Log(deviceId, "send length %lu, receive
          // length %u, value = %u ns\n", length,
          // SC_GetUserLogicPayloadLength(pNextPacket), 4 *
          // latencyTime);

          if (iteration >= NumberOfWarmUpRounds) {
            Statistics &statistics = pLatency->second;
            statistics.Add(latencyTime / 1000.0 *
                           4.0); // Timestamp unit is 4 ps
          }

          if (totalTestCount % 1000 == 0) {
            errorCode = SC_UpdateReceivePtr(
                channelId, pPreviousPacket);
            if (errorCode != SC_ERR_SUCCESS) {
              return errorCode;
            }
          }
        }
      }

      fprintf(stdout,
              "\nResults of User Logic latency test (%lu "
              "rounds):\n",
              parameter);
      for (std::map<uint16_t, Statistics>::iterator
               pLatency = latencies.begin();
           pLatency != latencies.end(); ++pLatency) {
        uint16_t length = pLatency->first;
        Statistics &statistics = pLatency->second;

        fprintf(stdout,
                "length %4u (0x%03x), time %.2f %s %.2f "
                "%ss (%s %.1f %%)\n",
                length, length, statistics.Mean,
                UTF_8_PLUS_MINUS, statistics.SD(),
                UTF_8_GREEK_MU, UTF_8_PLUS_MINUS,
                statistics.Percentage_SD());
      }

      return SetLastErrorCode(DeallocateChannel(channelId));
    }

    case SC_DEMO_USER_LOGIC_INTERRUPTS: {
      if (pDemoDesignOptions != nullptr) {
        return CallErrorHandler(
            SC_ERR_INVALID_ARGUMENT,
            "%s argument expected to be NULL instead of %p",
            nameof(pDemoDesignOptions),
            static_cast<void *>(pDemoDesignOptions));
      }

      unsigned channelNumber = unsigned(parameter);
      SC_Error errorCode = SC_WriteUserLogicRegister(
          deviceId, 0,
          (uint64_t(1) << (48 + channelNumber)) + 0x2000);
      if (errorCode != SC_ERR_SUCCESS) {
        return errorCode;
      }
      return SetLastErrorCode(SC_ERR_SUCCESS);
    } break;

      // ACL demo design.
      //
      // ACL demo design is controlled by user logic
      // register 1 (look into driver file acl.c). ACL demo
      // design is enabled if driver detects any ACL
      // features mask bits for any network interfaces. ACL
      // demo design not implemented!

    default:
      return CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                              "unknown demo design %u",
                              demoDesign);
    }
  } catch (std::exception &ex) {
    return CallErrorHandler(
        SC_ERR_DEMO_DESIGN_FAILED,
        "exception caught in demo design: %s", ex.what());
  }
}

/*****************************************************************************/

static inline bool IsBucketFree(SC_Bucket *pBucket) {
  if (pBucket->BucketIsUsed) {
    /*printf("_bucketIsUsed=%u, _payloadLsength=%u,
       _lastErrorCode=%d, _pTxDmaAcknowledgement=%p,
       _pTxDmaFifoFlowControl=%p\n", pBucket->_bucketIsUsed,
       pBucket->_payloadLength, pBucket->_lastErrorCode,
           pBucket->_pTxDmaAcknowledgement,
       pBucket->_pTxDmaFifoFlowControl);*/

    if (pBucket->PayloadLength > 0 ||
        pBucket->SendCount > 0 ||
        pBucket->LastErrorCode != SC_ERR_SUCCESS ||
        pBucket->pTxDmaAcknowledgement != nullptr ||
        pBucket->pTxDmaFifoFlowControl != nullptr) {
      return false;
    }
  }

  if (!pBucket->BucketIsUsed &&
      pBucket->PayloadLength == 0 &&
      pBucket->SendCount == 0 &&
      pBucket->LastErrorCode == SC_ERR_SUCCESS &&
      pBucket->pTxDmaAcknowledgement == nullptr &&
      pBucket->pTxDmaFifoFlowControl == nullptr) {
    return true;
  }

  return false;
}

/*****************************************************************************/

bool SC_IsBucketFree(SC_Bucket *pBucket) {
  return IsBucketFree(pBucket);
}

/*****************************************************************************/

static inline void FreeBucket(SC_Bucket *pBucket) {
  assert(!IsBucketFree(pBucket));

  pBucket->BucketIsUsed = false;
  pBucket->PayloadLength = 0;
  pBucket->LastErrorCode = SC_ERR_SUCCESS;
  pBucket->SendCount = 0;
  if (pBucket->pTxDmaAcknowledgement != nullptr) {
    pBucket->pTxDmaAcknowledgement = nullptr;
  }
  if (pBucket->pTxDmaFifoFlowControl != nullptr) {
    pBucket->pTxDmaFifoFlowControl = nullptr;
  }

  assert(IsBucketFree(pBucket));
}

/*****************************************************************************/

static void TxDMAListHasBeenSent(TxDmaList *pTxDmaList) {
  // Mark sent buckets as unused
  for (std::size_t i = 0; i < pTxDmaList->ListLength; ++i) {
    assert(i < ARRAY_SIZE(pTxDmaList->Buckets));
    SC_Bucket *pBucket = pTxDmaList->Buckets[i];
    assert(pBucket->pTxDmaAcknowledgement == nullptr);
    assert(pBucket->pTxDmaFifoFlowControl == nullptr);
    FreeBucket(pBucket);
  }

  // Clear the Tx DMA list:
  pTxDmaList->ListLength = 0;
  pTxDmaList->ListHasBeenFlushed = false;
  pTxDmaList->CumulatedPayloadLength = 0;
  if (pTxDmaList->pTxDmaFifoFlowControl != nullptr) {
    RequestHasbeenProcessedByTxDmaFifo(
        *pTxDmaList->pTxDmaFifoFlowControl);
  }
  pTxDmaList->pTxDmaFifoFlowControl = nullptr;
}

/*****************************************************************************/

#ifndef NDEBUG
static inline bool
IsTxDmaListFree(const SC_ChannelId channelId,
                const TxDmaList *pTxDmaList) {
  if (pTxDmaList->ListHasBeenFlushed ||
      pTxDmaList->CumulatedPayloadLength > 0 ||
      pTxDmaList->ListLength > 0 || pTxDmaList->Priority ||
      pTxDmaList->pTxDmaFifoFlowControl != nullptr ||
      pTxDmaList->LastErrorCode != SC_ERR_SUCCESS ||
      channelId->pCurrentTxDmaList != pTxDmaList) {
    return false;
  }
  return true;
}

/*****************************************************************************/

static inline bool
IsTxDmaListUsed(const TxDmaList *pTxDmaList) {
  if (pTxDmaList->CumulatedPayloadLength > 0 &&
      pTxDmaList->ListLength > 0 &&
      pTxDmaList->pTxDmaFifoFlowControl != nullptr &&
      (pTxDmaList->LastErrorCode == SC_ERR_SUCCESS ||
       (pTxDmaList->LastErrorCode == SC_ERR_PIPE_FULL &&
        pTxDmaList->ListHasBeenFlushed))) {
    return true;
  }
  return false;
}
#endif

/*****************************************************************************/

static inline void FreeTxDmaList(TxDmaList *pTxDmaList) {
  assert(IsTxDmaListUsed(pTxDmaList));

  for (std::size_t i = 0; i < pTxDmaList->ListLength; ++i) {
    assert(i < ARRAY_SIZE(pTxDmaList->Buckets));
    SC_Bucket *pBucket = pTxDmaList->Buckets[i];
    FreeBucket(pBucket);
    pTxDmaList->Buckets[i] = nullptr;
  }
  pTxDmaList->ListLength = 0;

  if (pTxDmaList->pTxDmaFifoFlowControl != nullptr) {
    assert(pTxDmaList->pTxDmaFifoFlowControl->FillLevel >
           0);
    --pTxDmaList->pTxDmaFifoFlowControl->FillLevel;
  }
  pTxDmaList->pTxDmaFifoFlowControl = nullptr;

  pTxDmaList->ListHasBeenFlushed = false;
  pTxDmaList->CumulatedPayloadLength = 0;
  pTxDmaList->Priority = false;
  pTxDmaList->LastErrorCode = SC_ERR_SUCCESS;
}

/*****************************************************************************/

static SC_Error WaitUntilCurrentTxDMAListHasBeenSent(
    SC_ChannelId channelId, TxDmaList *pTxDmaList) {
  SanityCheck(channelId);

  assert(IsTxDmaListUsed(pTxDmaList));

  // Wait until TxDMA list has been sent.
  uint64_t startTime = 0;
  while (true) {
    uint64_t count = 0;
    while (*pTxDmaList->pUserSpaceStart != 0 &&
           count < 1000000UL) {
      ++count;
      //_mm_pause();
    }
    // printf("\rWaited total of %lu counts
    // (*pLastBucketStart = 0x%lX)!   \n", count,
    // *pTxDMAListStart);
    if (*pTxDmaList->pUserSpaceStart == 0) {
      break;
    }

    if (startTime == 0) {
      startTime = Stopwatch::GetTimeNow();
    } else {
      uint64_t endTime = Stopwatch::GetTimeNow();
      if (endTime - startTime >
          channelId->FreeTxDmaListWaitTime) {
        return CallErrorHandlerDontSetLastError(
            SC_ERR_TX_DMA_WAIT_FAILED,
            "first entry in Tx DMA list was not cleared "
            "within %.1f seconds."
            " Something is wrong!",
            double(channelId->FreeTxDmaListWaitTime) /
                ::ONE_BILLION_NS);
      }
    }
  }

  FreeTxDmaList(pTxDmaList);

  return SC_ERR_SUCCESS;
}

/*****************************************************************************/

/**
 *  Cancel all resource bookings made during flow control
 * check. This is necessary if resources were already booked
 * when a subsequent flow control fails.
 *
 *  @param      channelId                   Channel
 * identifier.
 *  @param      sendParameters              Reference to Tx
 * send parameters.
 *  @param      bookings                    Bit flags
 * identifying which resource bookings were taken during
 * flow control check.
 *  @return                                 On return
 * bookings has the value of SEND_BOOKING_CANCELLED if
 * something was actually cancelled, unchanged otherwise.
 */
static inline void
CancelBookings(SC_ChannelId channelId,
               const SendParameters &sendParameters,
               SendBookings &bookings) {
  if ((bookings & SEND_BOOKING_CANCELLED) != 0) {
    if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
      Log(channelId->DeviceId,
          "Bookings already cancelled, bookings %s\n",
          GetSendBookings(bookings).c_str());
    }

    // Already cancelled, don't do anything
    return;
  }

  bool cancelled = false;

  if ((bookings & SEND_BOOKING_CONSUMED_BYTES) != 0) {
    channelId->pTxConsumedBytesFlowControl
        ->SentBytesCount -=
        sendParameters.PayloadLengthRoundedUpTo32;
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_FW_TD_DMA_FIFO) != 0) {
    // No resources booked, nothing actual to cancel or
    // backtrack
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_TAKEN_AND_LOCKED) != 0) {
    assert(channelId->pTxConsumedBytesFlowControl !=
           nullptr);

    SC_Unlock(
        *channelId->pTxConsumedBytesFlowControl->pLock);
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_SW_TD_DMA_FIFO) != 0) {
    assert(sendParameters.TxDmaFifoFlowControl.FillLevel >
           0);
    --sendParameters.TxDmaFifoFlowControl.FillLevel;
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_CONSUMED_BYTES) != 0) {
    bool lock =
        channelId->pTxConsumedBytesFlowControl->pLock !=
        nullptr;
    assert(lock ? channelId->pTxWindowFlowControl != nullptr
                : channelId->pTxWindowFlowControl ==
                      nullptr);

    if (lock) {
      SC_Lock(
          *channelId->pTxConsumedBytesFlowControl->pLock);
    }

    channelId->pTxConsumedBytesFlowControl
        ->SentBytesCount -=
        sendParameters.PayloadLengthRoundedUpTo32;

    if (lock) {
      SC_Unlock(
          *channelId->pTxConsumedBytesFlowControl->pLock);
    }
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_TCP_WINDOW) != 0) {
    channelId->pTxWindowFlowControl->SentBytesCount -=
        sendParameters.PayloadLength;
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_TAKEN_AND_LOCKED) != 0) {
    assert(channelId->pTxConsumedBytesFlowControl !=
           nullptr);

    SC_Unlock(
        *channelId->pTxConsumedBytesFlowControl->pLock);
    cancelled = true;
  }

  if ((bookings & SEND_BOOKING_SINGLE_PACKET) != 0) {
    cancelled = true;
  }

  if (cancelled) {
    bookings = SEND_BOOKING_CANCELLED;
    COLLECT_STATISTICS(++channelId->TxFlowControlStatistics
                             .BookingsCancelled);
  }

  if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
    Log(channelId->DeviceId, "%s: %s, bookings %s\n",
        __func__, cancelled ? "Cancelled" : "Not cancelled",
        GetSendBookings(bookings).c_str());
  }
}

/*****************************************************************************/

/**
 *  Check for free buckets in the bucket pool and if one is
 * found then mark it as free for reuse. Only buckets sent
 * as single packet are checked. Packet lists are not
 * currently supported but will be in a future release. Only
 * a single bucket if any is freed per call to guarantee
 *  fastest possible response when a free bucket is actually
 * available.
 *
 *  @param      channelId                   Channel
 * identifier.
 *  @param      context                     Call context
 * string just for debugging and logging purposes.
 *  @param      returnFirstFree             True if first
 * free bucket should be returned, false otherwise.
 *
 *  @return     A free bucket if one is available, null
 * pointer otherwise.
 */
static SC_Bucket *CheckFreeBuckets(SC_ChannelId channelId,
                                   const char *context,
                                   bool returnFirstFree) {
  SanityCheck(channelId);

  const std::size_t numberOfBuckets =
      ARRAY_SIZE(channelId->Buckets);
  for (std::size_t i = 0; i < numberOfBuckets - 1; ++i) {
    BucketsSanityCheck(channelId, context);

    assert(channelId->NextFreeBucketIndex <
           countof(channelId->Buckets));
    SC_Bucket *pBucket =
        &channelId
             ->Buckets[channelId->NextFreeBucketIndex++];
    if (channelId->NextFreeBucketIndex >=
        numberOfBuckets - 1) {
      channelId->NextFreeBucketIndex = 0;
    }

    if (LOG(channelId->DeviceId,
            LOG_TX | LOG_BUCKET | LOG_DETAIL)) {
      Log(channelId->DeviceId, "SC_GetBucket entry: %s",
          GetBucketsDump(channelId, context).c_str());
    }
    if (pBucket->pTxDmaAcknowledgement != nullptr) {
      if (*pBucket->pTxDmaAcknowledgement !=
          SC_BUCKET_ACKNOWLEDGED) {
        continue; // to next bucket
      }

      // This bucket was sent with SC_SEND_SINGLE_PACKET
      // functionality and has now been acknowledged by Tx
      // DMA nulling mechanism and has been sent forward
      // from the FIFO and can now be reused.
      if (pBucket->pTxDmaFifoFlowControl != nullptr) {
        // If Tx DMA software flow control is enabled then
        // decrement fill level by one, this packet is gone
        // from the Tx DMA FIFO.
        assert(pBucket->pTxDmaFifoFlowControl->FillLevel >
               0);
        --pBucket->pTxDmaFifoFlowControl->FillLevel;
      }

      FreeBucket(pBucket); // Mark bucket as free
    } else {
      if (pBucket->BucketIsUsed) {
        // This bucket is in use and not yet acknowledged
        // by the Tx DMA nulling mechanism, let's take a
        // look at the next bucket:
        continue; // to next bucket
      }
    }

#ifdef DEBUG
    // When calling CheckFreeBuckets from SC_GetBucket do
    // not allow used buckets here, it is an internal logic
    // error.

    // When calling CheckFreeBuckets from FlowControl then
    // allow used buckets here. This will be the case if
    // user sends with SC_SEND_SINGLE_PACKET the same bucket
    // again and again before it has been acknowledged by
    // the Tx DMA nulling mechanism.

    // For example test programs ul_dma_perf
    // and ul_dma_loop send the same bucket again and again.

    bool isBucketFree = IsBucketFree(pBucket);
    if (returnFirstFree && !isBucketFree) {
      Log(channelId->DeviceId, "AAAAAAAARGH:\n%s",
          GetBucketsDump(channelId, context).c_str());
      assert(isBucketFree);
    }
#endif

    if (LOG(channelId->DeviceId, LOG_TX | LOG_BUCKET)) {
      Log(channelId->DeviceId,
          "SC_GetBucket returns bucket #%u at %p [%p], "
          "nextFreeBucketIndex=%d, fillLevel=%s\n",
          pBucket->BucketNumber,
          static_cast<const void *>(pBucket),
          static_cast<const void *>(
              pBucket->pUserSpaceBucket->data),
          channelId->NextFreeBucketIndex,
          FillLevel(pBucket->pTxDmaFifoFlowControl)
              .c_str());
      if (LOG(channelId->DeviceId,
              LOG_TX | LOG_BUCKET | LOG_DETAIL)) {
        std::stringstream bucketsDump;
        DumpBuckets(bucketsDump, channelId, context);
        Log(channelId->DeviceId, "SC_GetBucket exit: %s",
            bucketsDump.str().c_str());
      }
    }

    BucketsSanityCheck(channelId, context);
    if (returnFirstFree) {
      return pBucket;
    }
  }

  return nullptr;
}

/*****************************************************************************/

static SC_Error
FlowControl(SC_ChannelId channelId,
            SendParameters &sendParameters,
            FlowControlOptions flowControlOptions,
            SendBookings &bookings) {
  ApiScopedTimer(API_TIMER_FLOW_CONTROL);

  assert(bookings == SEND_BOOKING_NONE ||
         bookings == SEND_BOOKING_SINGLE_PACKET);

  bool waitForPipeReady =
      (flowControlOptions &
       FLOW_CONTROL_WAIT_FOR_PIPE_FREE) != 0;

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics);

  const bool sendSinglePacket =
      (bookings & SEND_BOOKING_SINGLE_PACKET) != 0;

  while (true) {
    // TODO FIXME: this is not necessary, is it???!!! the
    // rounded up value should never change once computed
    sendParameters.PayloadLengthRoundedUpTo32 =
        sendParameters
            .PayloadLength; // Restore the original payload
                            // length
    bool canSend =
        CanSendOnThisChannel(channelId, sendParameters,
                             flowControlOptions, bookings);
    SendBookings failedBookings = bookings;
    if (!canSend) {
      if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
        Log(channelId->DeviceId,
            "%s cancelling, flowControlOptions %s, "
            "bookings %s\n",
            __func__,
            GetFlowControlOptions(flowControlOptions)
                .c_str(),
            GetSendBookings(bookings).c_str());
      }

      // Some flow control checks failed, cancel all partial
      // bookings we made on the way
      CancelBookings(channelId, sendParameters, bookings);
    }

    COLLECT_STATISTICS(
        SCI_TxDmaFifoStatistics &fifoStatistics =
            GetFifoStatistics(sendParameters, statistics,
                              nullptr);

        if (sendParameters.TxDmaFifoFlowControl.FillLevel >
            fifoStatistics.SwMaximumFillLevel) {
          fifoStatistics.SwMaximumFillLevel =
              sendParameters.TxDmaFifoFlowControl.FillLevel;
        })

    if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
      SCI_SendReason sendReason =
          channelId->TxFlowControlStatistics.SendReason;
      Log(channelId->DeviceId,
          "%s 1: Ca%s send, reason %s\n", __func__,
          canSend ? "n" : "n't",
          SCI_GetSendReasonString(sendReason).c_str());
    }

    SC_Error errorCode;

    if (canSend) {
      return SC_ERR_SUCCESS;
    } else {
      if (waitForPipeReady) {
        // We wait until we can send
        COLLECT_STATISTICS(
            ++channelId->TxFlowControlStatistics
                  .WaitPipeFreeCount);
        errorCode = SC_ERR_SUCCESS;
      } else {
        // Can't send and we are not to wait, return pipe
        // full error
        COLLECT_STATISTICS(
            ++channelId->TxFlowControlStatistics
                  .PipeFullCount);
        errorCode = SC_ERR_PIPE_FULL;
      }
    }

    if (LOG(channelId->DeviceId, LOG_TX_PIPE_FULL)) {
      SCI_SendReason sendReason =
          channelId->TxFlowControlStatistics.SendReason;
      Log(channelId->DeviceId,
          "%s 2: Ca%s send, reason %s%s\n", __func__,
          canSend ? "n" : "n't",
          SCI_GetSendReasonString(sendReason).c_str(),
          !canSend && waitForPipeReady
              ? ", waiting for free pipe"
              : "");
    }

    // When retrying start with no bookings, there should
    // not be any here
    assert(bookings == SEND_BOOKING_CANCELLED ||
           bookings == SEND_BOOKING_NONE);
    bookings = SEND_BOOKING_NONE;

    if ((failedBookings & (SEND_BOOKING_SW_TD_DMA_FIFO)) !=
            0 ||
        sendSinglePacket) {
      // Check if we can free resources when buckets have
      // been sent from Tx DMA FIFO buffer
      CheckFreeBuckets(channelId, __func__,
                       /*returnFirstFree:*/ false);
      COLLECT_STATISTICS(
          ++statistics.FlowControlCheckFreeBuckets);
    }

    if (errorCode != SC_ERR_SUCCESS) {
      return errorCode;
    }

    // Continue to wait for pipe ready
    assert(waitForPipeReady);
  }
}

/*****************************************************************************/

static SC_Error
FindFreeTxDmaList(SC_ChannelId channelId,
                  const SendParameters &sendParameters) {
  SanityCheck(channelId);

  uint64_t startTime = 0, endTime;

  uint32_t searchStartIndex =
      channelId->FreeTxDmaSearchStartIndex;
  // printf("FindFreeTxDmaList: searchStartIndex = %u\n",
  // searchStartIndex);

  while (true) {
    for (uint32_t i = 0; i < channelId->NumberOfTxDmaLists;
         ++i) {
      uint32_t index = (searchStartIndex + i) %
                       channelId->NumberOfTxDmaLists;
      TxDmaList *pCurrentList =
          &channelId->TxDmaLists[index];
      if (*pCurrentList->pUserSpaceStart == 0) {
        // This list is free, use it.

        channelId->pCurrentTxDmaList = pCurrentList;
        // Next free search starts from next TX DMA list
        searchStartIndex = (searchStartIndex + 1) %
                           channelId->NumberOfTxDmaLists;

        if (LOG(channelId->DeviceId, LOG_TX_DMA)) {
          Log(channelId->DeviceId,
              "%s: index = %u, next search index = %u\n",
              __func__, index, searchStartIndex);
        }

        TxDMAListHasBeenSent(pCurrentList);

        pCurrentList->pTxDmaFifoFlowControl =
            &sendParameters.TxDmaFifoFlowControl;
        channelId->FreeTxDmaSearchStartIndex =
            searchStartIndex;

        return SC_ERR_SUCCESS;
      }
    }

    // No free lists found, wait
    if (startTime == 0) {
      startTime = Stopwatch::GetTimeNow();
    } else {
      endTime = Stopwatch::GetTimeNow();
      if (endTime - startTime >
          channelId->FreeTxDmaListWaitTime) {
        return CallErrorHandlerDontSetLastError(
            SC_ERR_TX_DMA_WAIT_FAILED,
            "could not find free TX DMA list in %f seconds",
            double(channelId->FreeTxDmaListWaitTime) /
                double(::ONE_BILLION_NS));
      }
    }
  }
}

/*****************************************************************************/

static inline void SetTxDmaFlowControlOptions(
    SC_ChannelId channelId,
    const SendParameters &sendParameters,
    FlowControlOptions &flowControlOptions) {
  if (sendParameters.Priority) {
    if (channelId->InternalChannelOptions.ChannelOptions
            .UseTxDmaPriorityFifoFirmwareFillLevel) {
      flowControlOptions = FlowControlOptions(
          flowControlOptions | FLOW_CONTROL_FW_TX_DMA_FIFO);
    }
    if (channelId->TxDmaPriorityFifoFlowControl
            .FillLevelLimit > 0) {
      flowControlOptions = FlowControlOptions(
          flowControlOptions | FLOW_CONTROL_SW_TX_DMA_FIFO);
    }
  } else {
    if (channelId->InternalChannelOptions.ChannelOptions
            .UseTxDmaNormalFifoFirmwareFillLevel) {
      flowControlOptions = FlowControlOptions(
          flowControlOptions | FLOW_CONTROL_FW_TX_DMA_FIFO);
    }
    if (channelId->TxDmaNormalFifoFlowControl
            .FillLevelLimit > 0) {
      flowControlOptions = FlowControlOptions(
          flowControlOptions | FLOW_CONTROL_SW_TX_DMA_FIFO);
    }
  }
}

/*****************************************************************************/

SC_Error SC_Flush(SC_ChannelId channelId,
                  SC_FlushOptions flushOptions) {
  SanityCheck(channelId);

  if (channelId !=
      nullptr) // Just to disable compiler warning about
               // unreachable code!
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "send list options, including SC_Flush not "
        "currently supported. Please use "
        "SC_SEND_SINGLE_PACKET instead.");

  TxDmaList *pCurrentList = channelId->pCurrentTxDmaList;
  if (pCurrentList != nullptr &&
      !pCurrentList->ListHasBeenFlushed &&
      pCurrentList->ListLength > 0 &&
      pCurrentList->CumulatedPayloadLength > 0) {
    FlowControlOptions flowControlOptions =
        FLOW_CONTROL_NONE;
    if ((flushOptions & SC_FLUSH_WAIT_FOR_PIPE_FREE) != 0) {
      flowControlOptions = FlowControlOptions(
          flowControlOptions |
          FLOW_CONTROL_WAIT_FOR_PIPE_FREE);
    }

    const bool priority =
        ((flushOptions & SC_FLUSH_PRIORITY) != 0)
            ? true
            : pCurrentList->Priority;

    SendBookings bookings = SEND_BOOKING_NONE;

    // See if there is room in the Tx DMA FIFO to send the
    // collective list All flow checks related to consumed
    // bytes have already been done in SC_SendBucket and all
    // the necessary buffer resources have been reserved
    // (_sentBytesCount!)
    assert(pCurrentList->CumulatedPayloadLength <
           std::numeric_limits<uint16_t>::max());
    uint16_t payloadLength =
        uint16_t(pCurrentList->CumulatedPayloadLength);

    SendParameters sendParameters(
        channelId, payloadLength,
        0 /*TODO: payloadPhysicalAddress*/, priority);

    SetTxDmaFlowControlOptions(channelId, sendParameters,
                               flowControlOptions);

    SC_Error errorCode =
        FlowControl(channelId, sendParameters,
                    flowControlOptions, bookings);
    if (errorCode != SC_ERR_SUCCESS) {
      if (errorCode == SC_ERR_PIPE_FULL) {
        pCurrentList->LastErrorCode = SC_ERR_PIPE_FULL;
      }

      if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
        Log(channelId->DeviceId,
            "SC_Flush flow control failed, error code %d\n",
            errorCode);
      }

      CancelBookings(channelId, sendParameters, bookings);

      return SetLastErrorCode(errorCode);
    }

    assert(*pCurrentList->pUserSpaceStart != 0);
    SendListToFIFO(
        channelId, pCurrentList->KernelSpaceStart,
        pCurrentList->ListLength, priority, pCurrentList);

    if (LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
      Log(channelId->DeviceId, "SC_Flush list sent\n");
    }

    pCurrentList->ListHasBeenFlushed = true;
    channelId->pCurrentTxDmaList = nullptr;

    if ((flushOptions & SC_FLUSH_WAIT_FOR_COMPLETION) !=
        0) {
      return SetLastErrorCode(
          WaitUntilCurrentTxDMAListHasBeenSent(
              channelId, pCurrentList));
    }

    return SetLastErrorCode(SC_ERR_SUCCESS);
  }

  return SetLastErrorCode(SC_ERR_NOTHING_TO_FLUSH);
}

/*****************************************************************************/

SC_Error SC_GetSendBucketDefaultOptions(
    SC_ChannelId channelId,
    SC_SendOptions *pDefaultSendOptions) {
  SanityCheck(channelId);

  *pDefaultSendOptions = channelId->DefaultSendOptions;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_SetSendBucketDefaultOptions(
    SC_ChannelId channelId,
    SC_SendOptions defaultSendOptions) {
  SanityCheck(channelId);

  channelId->DefaultSendOptions = defaultSendOptions;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

static inline SC_Error
SendSinglePacket(SC_ChannelId channelId,
                 SendParameters &sendParameters,
                 volatile uint64_t *pTxDmaAcknowledgement,
                 SC_SendOptions &sendOptions) {
  ApiScopedTimer(API_TIMER_SEND_SINGLE_PACKET);

  SanityCheck(channelId);

  // Send single packet

  FlowControlOptions flowControlOptions =
      FLOW_CONTROL_SINGLE_PACKET;
  SetTxDmaFlowControlOptions(channelId, sendParameters,
                             flowControlOptions);

  if ((sendOptions & SC_SEND_WAIT_FOR_PIPE_FREE) != 0) {
    flowControlOptions =
        FlowControlOptions(flowControlOptions |
                           FLOW_CONTROL_WAIT_FOR_PIPE_FREE);
  }

  if (LOG(channelId->DeviceId,
          LOG_TX_FLOW_CONTROL | LOG_DETAIL)) {
    Log(channelId->DeviceId,
        "Entering channel %u flow control...\n",
        channelId->DmaChannelNumber);
  }

  SendBookings bookings = SEND_BOOKING_SINGLE_PACKET; //

  COLLECT_STATISTICS(uint64_t flowControlStartTime =
                         Stopwatch::GetTimestamp());

  SC_Error errorCode =
      FlowControl(channelId, sendParameters,
                  flowControlOptions, bookings);

  COLLECT_STATISTICS(channelId->FlowControlTime.Add(double(
      Stopwatch::GetTimestamp() - flowControlStartTime)));

  if (errorCode != SC_ERR_SUCCESS) {
    if (LOG(channelId->DeviceId,
            LOG_TX_FLOW_CONTROL | LOG_DETAIL)) {
      Log(channelId->DeviceId,
          "Exiting channel %u flow control with error code "
          "%u\n",
          channelId->DmaChannelNumber, errorCode);
    }

    return errorCode;
  }

  if (LOG(channelId->DeviceId,
          LOG_TX_FLOW_CONTROL | LOG_DETAIL)) {
    Log(channelId->DeviceId,
        "Successfully exiting channel %u flow control\n",
        channelId->DmaChannelNumber);
  }

  BucketsSanityCheck(channelId, __func__);

  COLLECT_STATISTICS(uint64_t sendPacketStartTime =
                         Stopwatch::GetTimestamp());

  SendPacketToFIFO(channelId, sendParameters);

  if ((sendOptions & SC_SEND_WAIT_FOR_COMPLETION) != 0) {
    int count = 0;
    while (*pTxDmaAcknowledgement != 0) {
      ++count;
      if (count % 10000 == 0 &&
          LOG(channelId->DeviceId, LOG_TX_FLOW_CONTROL)) {
        Log(channelId->DeviceId, "Wait count is %u\r",
            count);
      }
    }
    sendOptions = SC_SendOptions(
        sendOptions | SC_SEND_FREE_BUCKET_AFTER_ACK_WAIT);
  }

  COLLECT_STATISTICS(channelId->SendPacketTime.Add(double(
      Stopwatch::GetTimestamp() - sendPacketStartTime)));

  if (LOG(channelId->DeviceId, LOG_TX | LOG_BUCKET)) {
    Log(channelId->DeviceId, "%s",
        GetBucketsDump(channelId, __func__).c_str());
  }

  BucketsSanityCheck(channelId, __func__);

  return errorCode;
}

/*****************************************************************************/

static SC_Error
AppendBucketToList(SC_ChannelId channelId,
                   SC_Bucket *pBucket,
                   const SendParameters &sendParameters) {
  SanityCheck(channelId);

  TxDmaList *pCurrentList = channelId->pCurrentTxDmaList;
  assert(pCurrentList != nullptr);
  uint32_t listLength = pCurrentList->ListLength;
  volatile uint64_t *pTxDmaNextEntry =
      pCurrentList->pUserSpaceStart + listLength;
  if (listLength == 0) {
    SC_DeviceId deviceId = channelId->DeviceId;

    if (*pTxDmaNextEntry != 0 && LOG(deviceId, LOG_ERROR)) {
      Log(deviceId,
          "Tx DMA list start is not zeroed on channel %u "
          "at address 0x%p, values is 0x%lX",
          channelId->DmaChannelNumber,
          static_cast<volatile void *>(pTxDmaNextEntry),
          *pTxDmaNextEntry);
    }
  }
  pCurrentList->Buckets[listLength] = pBucket;

  ++pCurrentList->ListLength;
  pCurrentList->CumulatedPayloadLength +=
      sendParameters.PayloadLengthRoundedUpTo32;

  ++channelId->ListBucketSequenceNumber;

  uint64_t packetDescriptor =
      MakeSinglePacketDescriptor(sendParameters);
  *pTxDmaNextEntry = packetDescriptor;

  return SC_ERR_SUCCESS;
}

/*****************************************************************************/

static SC_Error SendList(SC_ChannelId channelId,
                         SendParameters &sendParameters,
                         SC_Bucket *pBucket,
                         SC_SendOptions sendOptions) {
  SanityCheck(channelId);

  TxDmaList *pCurrentList = channelId->pCurrentTxDmaList;
  if (pCurrentList == nullptr) {
    SC_Error errorCode =
        FindFreeTxDmaList(channelId, sendParameters);
    if (errorCode != SC_ERR_SUCCESS) {
      return errorCode;
    }
    pCurrentList = channelId->pCurrentTxDmaList;
    assert(pCurrentList != nullptr);
  }

  if (pCurrentList->ListLength == 0) {
    assert(IsTxDmaListFree(channelId, pCurrentList));
    pCurrentList->Priority = sendParameters.Priority;
  } else if (pCurrentList->Priority !=
             sendParameters.Priority) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "cannot build a mixed list of normal and priority "
        "FIFO entries, select one or the other.");
  }

  FlowControlOptions flowControlOptions =
      FLOW_CONTROL_APPEND_LIST;
  if ((sendOptions & SC_SEND_WAIT_FOR_PIPE_FREE) != 0) {
    flowControlOptions =
        FlowControlOptions(flowControlOptions |
                           FLOW_CONTROL_WAIT_FOR_PIPE_FREE);
  }

  // See if there is room to send the collective list, i.e.
  // all list payloads summed together (each possibly
  // rounded up)
  SendBookings bookings = SEND_BOOKING_NONE;
  SC_Error errorCode =
      FlowControl(channelId, sendParameters,
                  flowControlOptions, bookings);
  if (errorCode != SC_ERR_SUCCESS) {
    if (errorCode == SC_ERR_PIPE_FULL) {
      pBucket->LastErrorCode = SC_ERR_PIPE_FULL;
    }
    return errorCode;
  }

  errorCode = AppendBucketToList(channelId, pBucket,
                                 sendParameters);
  if (errorCode != SC_ERR_SUCCESS) {
    CancelBookings(channelId, sendParameters, bookings);
    return errorCode;
  }

  // Explicit flush request?
  bool flush = (sendOptions & SC_SEND_SINGLE_LIST) != 0;

  // Other conditions to flush:

  // List length limit exceeded?
  flush |= pCurrentList->ListLength >= 31;

  // List cumulated payload length exceeded?
  if (pCurrentList->CumulatedPayloadLength >=
      channelId->pTxConsumedBytesFlowControl
          ->ListFlushLimit) {
    flush |= true;
    COLLECT_STATISTICS(
        AddSendReason(channelId->TxFlowControlStatistics,
                      SCI_SEND_REASON_LIST_SIZE));
  }

  if (flush) {
    SC_FlushOptions flushOptions = SC_FLUSH_NORMAL;

    if (sendParameters.Priority) {
      flushOptions =
          SC_FlushOptions(flushOptions | SC_FLUSH_PRIORITY);
    }

    if ((sendOptions & SC_SEND_WAIT_FOR_COMPLETION) != 0) {
      flushOptions = SC_FlushOptions(
          flushOptions | SC_FLUSH_WAIT_FOR_COMPLETION);
    }

    errorCode = SC_Flush(channelId, flushOptions);

    assert(errorCode ==
           SC_ERR_SUCCESS); // Should always have something
                            // to flush here!

    return errorCode;
  }

  return SC_ERR_SUCCESS;
}

/*****************************************************************************/

static SC_Error SendCommonChecks(
    SC_ChannelId channelId, uint16_t payloadLength,
    SC_SendOptions &sendOptions, void *pReserved) {
  SanityCheck(channelId);

  BucketsSanityCheck(channelId, nameof(SendCommonChecks));
  if (pReserved != nullptr) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "reserved argument 'void * pReserved' expected to "
        "be NULL instead of %p",
        pReserved);
  }
  if (sendOptions == SC_SEND_DEFAULT ||
      sendOptions == SC_SEND_NONE) {
    sendOptions = channelId->DefaultSendOptions;
#ifdef DEBUG
    // sendOptions = SC_SendOptions(sendOptions |
    // SC_SEND_DISABLE_CONSUMER_FLOW_CHECK);
#endif
  }
  if (payloadLength > MAX_TX_PAYLOAD_LENGTH) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "payload length %u is greater than allowed maximum "
        "of %u",
        payloadLength, MAX_TX_PAYLOAD_LENGTH);
  }

  bool sendSingleList =
      (sendOptions & SC_SEND_SINGLE_LIST) != 0;
  bool sendQueueList =
      (sendOptions & SC_SEND_QUEUE_LIST) != 0;
  if (sendSingleList || sendQueueList) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "send list options, including SC_Flush not "
        "currently supported. Please use "
        "SC_SEND_SINGLE_PACKET instead.");
  }

  if (channelId->DeviceId->InternalCardInfo.CardInfo
          .IsToeConnectedToUserLogic &&
      channelId->ChannelType == SC_DMA_CHANNEL_TYPE_TCP) {
    return CallErrorHandlerDontSetLastError(
        SC_ERR_INVALID_ARGUMENT,
        "cannot send to ETLM when TOE is connected to user "
        "logic.");
  }

  return SC_ERR_SUCCESS;
}

/*****************************************************************************/

SC_Error SC_SendBucket(SC_ChannelId channelId,
                       SC_Bucket *pBucket,
                       uint16_t payloadLength,
                       SC_SendOptions sendOptions,
                       void *pReserved) {
  ApiScopedTimer(API_TIMER_SEND_BUCKET);

  SanityCheck(channelId);

  COLLECT_STATISTICS(
      // Measure time between 2 consecutive calls to
      // SC_SendBucket:
      uint64_t now = Stopwatch::GetTimestamp();
      if (channelId->PreviousSendBucketTime != 0) {
        double sendBucketTime =
            double(now - channelId->PreviousSendBucketTime);
        channelId->SendBucketTime.Add(sendBucketTime);
      } channelId->PreviousSendBucketTime = now;)

  SC_Error errorCode = SendCommonChecks(
      channelId, payloadLength, sendOptions, pReserved);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  pBucket->PayloadLength = payloadLength;
  pBucket->SendCount = ++channelId->SentBucketsCount;

  const bool priority =
      (sendOptions & SC_SEND_PRIORITY) != 0;
  SendParameters sendParameters(
      channelId, payloadLength,
      pBucket->BucketPhysicalAddress, priority);

  bool sendSinglePacket =
      (sendOptions & SC_SEND_SINGLE_PACKET) != 0;
  if (sendSinglePacket) {
    pBucket->pTxDmaFifoFlowControl =
        &sendParameters.TxDmaFifoFlowControl;
    pBucket->pTxDmaFifoFlowControl =
        pBucket->pTxDmaFifoFlowControl->FillLevelLimit > 0
            ? pBucket->pTxDmaFifoFlowControl
            : nullptr;
    pBucket->pTxDmaAcknowledgement =
        reinterpret_cast<uint64_t *>(
            reinterpret_cast<uint8_t *>(
                pBucket->pUserSpaceBucket->data) +
            payloadLength);
    assert(ReadableAddressCheck(
        const_cast<uint64_t *>(
            pBucket->pTxDmaAcknowledgement),
        sizeof(void *)));

    // Tx DMA nulling mechanism writes zeroes to this
    // location (8 consecutive bytes right after the
    // payload) when all payload has been read from host
    // memory and the packet descriptor has been removed
    // from the FIFO. This signals that the bucket can be
    // reused to send a new packet.
    *pBucket->pTxDmaAcknowledgement =
        SC_BUCKET_ACKNOWLEDGE_PENDING;

    errorCode = SendSinglePacket(
        channelId, sendParameters,
        pBucket->pTxDmaAcknowledgement, sendOptions);
    if (errorCode == SC_ERR_PIPE_FULL) {
      pBucket->LastErrorCode = errorCode;
    }
    if ((sendOptions &
         SC_SEND_FREE_BUCKET_AFTER_ACK_WAIT) != 0) {
      if (LOG(channelId->DeviceId, LOG_TX_DMA)) {
        Log(channelId->DeviceId,
            "Bucket %u Tx DMA acknowledgement cleared in "
            "while waiting: 0x%lu\n",
            pBucket->BucketNumber,
            *pBucket->pTxDmaAcknowledgement);
      }
      FreeBucket(pBucket);
    }
    return SetLastErrorCode(errorCode);
  }

  const bool sendSingleList =
      (sendOptions & SC_SEND_SINGLE_LIST) != 0;
  const bool sendQueueList =
      (sendOptions & SC_SEND_QUEUE_LIST) != 0;
  if (sendSingleList || sendQueueList) {
    return SetLastErrorCode(SendList(
        channelId, sendParameters, pBucket, sendOptions));
  }

  return CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "invalid argument sendOptions %s",
      GetSendOptionsString(channelId, sendOptions).c_str());
}

/*****************************************************************************/

SC_Error SCI_SendPacket(SC_ChannelId channelId,
                        SC_Packet *pPacket,
                        SC_SendOptions sendOptions,
                        void *pReserved) {
  SanityCheck(channelId);

  int16_t payloadLength =
      SC_GetPacketPayloadLength(channelId, pPacket);
  if (payloadLength < 0) {
    return SC_Error(payloadLength);
  }
  SC_Error errorCode =
      SendCommonChecks(channelId, uint16_t(payloadLength),
                       sendOptions, pReserved);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  bool sendSinglePacket =
      (sendOptions & SC_SEND_SINGLE_PACKET) != 0;
  if (sendSinglePacket) {
    // Send the packet back directly from the PRB:
    const uint8_t *pPayload =
        SC_GetPacketPayload(channelId, pPacket);
    SC_Packet *pPrbStart =
        reinterpret_cast<SC_Packet *>(channelId->pPRB);
    uint64_t offset = reinterpret_cast<uint64_t>(pPayload) -
                      reinterpret_cast<uint64_t>(pPrbStart);

    bool priority = (sendOptions & SC_SEND_PRIORITY) != 0;
    SendParameters sendParameters(
        channelId, payloadLength,
        channelId->PrbPhysicalAddress + offset, priority);

    return SetLastErrorCode(SendSinglePacket(
        channelId, sendParameters, nullptr, sendOptions));
  }

  return CallErrorHandler(
      SC_ERR_INVALID_ARGUMENT,
      "invalid argument sendOptions %s",
      GetSendOptionsString(channelId, sendOptions).c_str());
}

/*****************************************************************************/

SC_Bucket *SC_GetBucket(SC_ChannelId channelId,
                        int64_t reserved) {
  ApiScopedTimer(API_TIMER_GET_BUCKET);

  SanityCheck(channelId);

  BucketsSanityCheck(channelId, "SC_GetBucket");
  if (reserved != 0) {
    CallErrorHandler(SC_ERR_INVALID_ARGUMENT,
                     "reserved argument expected to be "
                     "zero instead of %lu",
                     reserved);
    return nullptr;
  }

  SC_Bucket *pBucket = CheckFreeBuckets(
      channelId, "SC_GetBucket", /*returnFirstFree:*/ true);

  COLLECT_STATISTICS(
      SC_TxFlowControlStatistics &statistics =
          channelId->TxFlowControlStatistics;
      ++statistics.GetBucketCheckFreeBuckets;)

  if (pBucket == nullptr) {
#ifdef DEBUG
    // Log(channelId->deviceId, "SC_GetBucket: no more
    // buckets available:\n%s\n", GetBucketsDump(channelId,
    // "SC_GetBucket").c_str());
#endif
    SetLastErrorCode(SC_ERR_NO_MORE_BUCKETS_AVAILABLE);
  } else {
    pBucket->BucketIsUsed = true;
    SetLastErrorCode(SC_ERR_SUCCESS);
  }
  BucketsSanityCheck(channelId, "SC_GetBucket");

  return pBucket;
}

/*****************************************************************************/

SC_Error SC_GetUserLogicInterruptOptions(
    SC_DeviceId deviceId, SC_UserLogicInterruptOptions
                              *pUserLogicInterruptOptions) {
  GetOptions(UserLogicInterruptOptions, deviceId,
             pUserLogicInterruptOptions);
}

/*****************************************************************************/

SC_Error SC_SetUserLogicInterruptOptions(
    SC_DeviceId deviceId, SC_UserLogicInterruptOptions
                              *pUserLogicInterruptOptions) {
  SetOptions(UserLogicInterruptOptions, deviceId,
             pUserLogicInterruptOptions);
}

/*****************************************************************************/

SC_Error SC_EnableUserLogicInterrupts(
    SC_DeviceId deviceId, SC_UserLogicInterruptOptions
                              *pUserLogicInterruptOptions) {
  SanityCheck(deviceId);

  if (pUserLogicInterruptOptions == nullptr) {
    pUserLogicInterruptOptions =
        &ApiLibraryStaticInitializer::GetInstance()
             .DefaultUserLogicInterruptOptions
             .UserLogicInterruptOptions;
  }

  pUserLogicInterruptOptions->Pid =
      pUserLogicInterruptOptions->Pid == 0
          ? getpid()
          : pUserLogicInterruptOptions->Pid;
  SC_Error errorCode =
      ApiLibraryStaticInitializer::SanityCheckOptions(
          deviceId, pUserLogicInterruptOptions);
  if (errorCode != SC_ERR_SUCCESS) {
    return errorCode;
  }

  sc_user_logic_interrupts user_logic_interrupts;
  memset(&user_logic_interrupts, 0,
         sizeof(user_logic_interrupts));
  user_logic_interrupts.mask =
      uint8_t(pUserLogicInterruptOptions->Mask);
  if (pUserLogicInterruptOptions->Mask !=
      SC_USER_LOGIC_INTERRUPT_MASK_NONE) {
    user_logic_interrupts.pid =
        pUserLogicInterruptOptions->Pid;
    user_logic_interrupts.signal =
        pUserLogicInterruptOptions->SignalNumber;
    user_logic_interrupts.pContext =
        pUserLogicInterruptOptions->pContext;
  }

  if (ioctl(deviceId->FileDescriptor,
            SC_IOCTL_ENABLE_USER_LOGIC_INTERRUPTS,
            &user_logic_interrupts) != 0) {
    return CallErrorHandler(SC_ERR_FAILED_TO_RESET_FPGA,
                            "failed to enable user logic "
                            "interrupts, errno = %d",
                            errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetUserLogicInterruptResults(
    const siginfo_t *pSignalInfo,
    SC_UserLogicInterruptResults
        *pUserLogicInterruptResults) {
  if (pUserLogicInterruptResults == nullptr) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "argument pUserLogicInterruptResults cannot be "
        "NULL!");
  }

  if (pSignalInfo->si_int != int(SC_MAGIC_SIGNAL_INT)) {
    return CallErrorHandler(
        SC_ERR_INVALID_ARGUMENT,
        "this signal was not sent by %s driver!", PRODUCT);
  }

  pUserLogicInterruptResults->StructSize =
      sizeof(SC_UserLogicInterruptResults);

  // Interrupt results are located at the last bytes of the
  // siginfo_t structure:
  const sc_user_logic_interrupt_results &interruptResults =
      *reinterpret_cast<
          const sc_user_logic_interrupt_results *>(
          reinterpret_cast<const uint8_t *>(pSignalInfo) +
          sizeof(*pSignalInfo) -
          sizeof(sc_user_logic_interrupt_results));
  pUserLogicInterruptResults->DeviceMinorNumber =
      interruptResults.minor;
  pUserLogicInterruptResults->UserLogicChannel =
      interruptResults.channel;
  pUserLogicInterruptResults->pContext =
      interruptResults.pContext;
  pUserLogicInterruptResults->TimestampSeconds =
      uint64_t(interruptResults.timestamp.tv_sec);
  pUserLogicInterruptResults->TimestampNanoSeconds =
      uint64_t(interruptResults.timestamp.tv_nsec);

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_GetPacketTimestamp(const SC_Packet *pPacket,
                               uint32_t *pSeconds,
                               uint32_t *pNanoseconds) {
  *pSeconds = pPacket->ts_seconds;
  *pNanoseconds = pPacket->ts_nanoseconds;
  SC_Error errorCode =
      TimeModeToNanoseconds(*pSeconds, *pNanoseconds);
  if (errorCode != SC_ERR_SUCCESS) {
    return SetLastErrorCode(errorCode);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error
SC_AclModify(SC_DeviceId deviceId, uint8_t networkInterface,
             SC_AclId aclId, uint16_t index,
             struct in_addr ip, struct in_addr ipMask,
             uint16_t minimumPort, uint16_t maximumPort) {
  SanityCheck(deviceId);

  sc_acl_config_t acl;
  memset(&acl, 0, sizeof(acl));
  acl.cmd = sc_acl_config_t::ACL_CMD_EDIT;
  acl.network_interface = networkInterface;
  acl.acl_id = aclId;
  acl.index = index;
  acl.ip = ntohl(ip.s_addr);
  acl.ip_mask = ntohl(ipMask.s_addr);
  acl.port_min = minimumPort;
  acl.port_max = maximumPort;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_ACL_CMD,
            &acl) != 0) {
    if (errno == ENODEV) {
      return CallErrorHandler(
          SC_ERR_ACL_NOT_PRESENT,
          "ACL not present in FPGA configuration on "
          "network interface %u",
          networkInterface);
    }
    return CallErrorHandler(
        SC_ERR_ACL_OPERATION_FAILED,
        "failed to modify ACL settings, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_AclReset(SC_DeviceId deviceId,
                     uint8_t networkInterface,
                     SC_AclId acl_id, int16_t index) {
  SanityCheck(deviceId);

  sc_acl_config_t acl;
  memset(&acl, 0, sizeof(acl));
  acl.cmd = sc_acl_config_t::ACL_CMD_RESET;
  acl.network_interface = networkInterface;
  acl.acl_id = acl_id;
  acl.index = index;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_ACL_CMD,
            &acl) != 0) {
    if (errno == ENODEV) {
      return CallErrorHandler(
          SC_ERR_ACL_NOT_PRESENT,
          "ACL not present in FPGA configuration on "
          "network interface %u",
          networkInterface);
    }
    return CallErrorHandler(
        SC_ERR_ACL_OPERATION_FAILED,
        "failed to reset ACL settings, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_AclGet(SC_DeviceId deviceId,
                   uint8_t networkInterface, SC_AclId aclId,
                   uint16_t index,
                   struct in_addr *pIpAddress,
                   struct in_addr *pIpMask,
                   uint16_t *pMinimumPort,
                   uint16_t *pMaximumPort) {
  SanityCheck(deviceId);

  sc_acl_config_t acl;
  memset(&acl, 0, sizeof(acl));
  acl.cmd = sc_acl_config_t::ACL_CMD_GET;
  acl.network_interface = networkInterface;
  acl.acl_id = aclId;
  acl.index = index;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_ACL_CMD,
            &acl) != 0) {
    if (errno == ENODEV) {
      return CallErrorHandler(
          SC_ERR_ACL_NOT_PRESENT,
          "ACL not present in FPGA configuration on "
          "network interface %u",
          networkInterface);
    }
    return CallErrorHandler(
        SC_ERR_ACL_OPERATION_FAILED,
        "failed to get ACL settings, errno = %d", errno);
  }

  pIpAddress->s_addr = htonl(acl.ip);
  pIpMask->s_addr = htonl(acl.ip_mask);
  *pMinimumPort = acl.port_min;
  *pMaximumPort = acl.port_max;

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_AclEnable(SC_DeviceId deviceId,
                      uint8_t networkInterface) {
  SanityCheck(deviceId);

  sc_acl_config_t acl;
  memset(&acl, 0, sizeof(acl));
  acl.cmd = sc_acl_config_t::ACL_CMD_ENABLE;
  acl.network_interface = networkInterface;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_ACL_CMD,
            &acl) != 0) {
    if (errno == ENODEV) {
      return CallErrorHandler(
          SC_ERR_ACL_NOT_PRESENT,
          "ACL not present in FPGA configuration on "
          "network interface %u",
          networkInterface);
    }
    return CallErrorHandler(
        SC_ERR_ACL_OPERATION_FAILED,
        "failed to enable ACL, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

/*****************************************************************************/

SC_Error SC_AclDisable(SC_DeviceId deviceId,
                       uint8_t networkInterface) {
  SanityCheck(deviceId);

  sc_acl_config_t acl;
  memset(&acl, 0, sizeof(acl));
  acl.cmd = sc_acl_config_t::ACL_CMD_DISABLE;
  acl.network_interface = networkInterface;

  if (ioctl(deviceId->FileDescriptor, SC_IOCTL_ACL_CMD,
            &acl) != 0) {
    if (errno == ENODEV) {
      return CallErrorHandler(
          SC_ERR_ACL_NOT_PRESENT,
          "ACL not present in FPGA configuration on "
          "network interface %u",
          networkInterface);
    }
    return CallErrorHandler(
        SC_ERR_ACL_OPERATION_FAILED,
        "failed to disable ACL, errno = %d", errno);
  }

  return SetLastErrorCode(SC_ERR_SUCCESS);
}

} // extern "C"
