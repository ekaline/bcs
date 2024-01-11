#ifndef __product_common_h__
#define __product_common_h__

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
 * @brief Common definitions that are shared by user mode code API, API library and the device driver.
 */

/** @cond PRIVATE_CODE */

#include <stdbool.h>

#ifdef __cplusplus
    #define SC_CAST(type, value)     type(value)
#else
    #define SC_CAST(type, value)     (type)(value)
#endif
#define SC_MASK(mask)   SC_CAST(int, mask)                          /**< Logging mask definition. */

/* Macro magic to find out if definition of x is empty or not */
#define SC_APPEND(x)    x ## 10
#define SC_IS_EMPTY(x)  (SC_APPEND(x) == 10)

#ifndef STRICT_MODE_OFF

#define STRICT_MODE_OK 0
#if __GNUC__ >= 4
    #if __GNUC_MINOR__ >= 4
        #if __GNUC_PATCHLEVEL__ > 7
            #undef STRICT_MODE_OK
            #define STRICT_MODE_OK 1
        #endif
    #endif
#endif

#define SC_RETURN_CASE(value)  case value: return #value

#if STRICT_MODE_OK == 1

    /**
     *  Turn off GNU C++ strict compilation options (CPPSTRICT in the Makefile).
     */
    #define STRICT_MODE_OFF                                             \
        _Pragma("GCC diagnostic push")                                  \
        _Pragma("GCC diagnostic ignored \"-Wreturn-type\"")             \
        _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")        \
        _Pragma("GCC diagnostic ignored \"-pedantic\"")                 \
        _Pragma("GCC diagnostic ignored \"-Wshadow\"")                  \
        _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")          \
        _Pragma("GCC diagnostic ignored \"-Wswitch-default\"")

        /* Addition options that can be enabled
        _Pragma("GCC diagnostic ignored \"-Wpedantic\"")                \
        _Pragma("GCC diagnostic ignored \"-Wformat=\"")                 \
        _Pragma("GCC diagnostic ignored \"-Werror\"")                   \
        _Pragma("GCC diagnostic ignored \"-Werror=\"")                  \
        _Pragma("GCC diagnostic ignored \"-Wunused-variable\"")         \
        _Pragma("GCC diagnostic ignored \"-Wdelete-non-virtual-dtor\"") \
        */

    /**
    *  Turn on GNU C++ strict compilation options (CPPSTRICT in the Makefile).
    */
    #define STRICT_MODE_ON                                              \
        _Pragma("GCC diagnostic pop")

#else
    /* GNU C/C++ 4.4.7 or less (CentOS 6 etc) */
    #define STRICT_MODE_OFF
    #define STRICT_MODE_ON
#endif /* STRICT_MODE_OK */

#endif /* #ifndef STRICT_MODE_OFF */

#define SC_LEGACY_API

#ifdef SC_LEGACY_API

/* Map Silicom old SmartNIC API to Silicom Generic API */

#ifdef __cplusplus
    #define SN_CAST(type, value)     type(value)
#else
    #define SN_CAST(type, value)     (type)(value)
#endif
#define SN_MASK(mask)   SN_CAST(int, mask)

#define SN_TCP_NONE SC_TCP_NONE
#define SN_TCP_CLOSED SC_TCP_CLOSED
#define SN_TCP_LISTEN SC_TCP_LISTEN
#define SN_TCP_SYN_SENT SC_TCP_SYN_SENT
#define SN_TCP_SYN_RECV SC_TCP_SYN_RECV
#define SN_TCP_ESTABLISHED SC_TCP_ESTABLISHED
#define SN_TCP_CLOSE_WAIT SC_TCP_CLOSE_WAIT
#define SN_TCP_FIN_WAIT1 SC_TCP_FIN_WAIT1
#define SN_TCP_CLOSING SC_TCP_CLOSING
#define SN_TCP_LAST_ACK SC_TCP_LAST_ACK
#define SN_TCP_FIN_WAIT2 SC_TCP_FIN_WAIT2
#define SN_TCP_TIME_WAIT SC_TCP_TIME_WAIT
#define SN_TCP_CLOSED_DATA_PENDING SC_TCP_CLOSED_DATA_PENDING

#define ACL_ID_NONE SC_ACL_ID_NONE
#define ACL_ID_TX SC_ACL_ID_TX
#define ACL_ID_RX SC_ACL_ID_RX
#define ACL_ID_BOTH SC_ACL_ID_BOTH
#define ACL_ID_FAILED SC_ACL_ID_FAILED

#define TIMECONTROL_INVALID SC_TIMECONTROL_INVALID
#define TIMECONTROL_HARDWARE SC_TIMECONTROL_HARDWARE
#define TIMECONTROL_SOFTWARE SC_TIMECONTROL_SOFTWARE
#define TIMECONTROL_SOFTWARE_FPGA_RESET SC_TIMECONTROL_SOFTWARE_FPGA_RESET

#define SN_ChannelType SC_ChannelType

#define SN_DMA_CHANNEL_TYPE_NONE SC_DMA_CHANNEL_TYPE_NONE
#define SN_DMA_CHANNEL_TYPE_USER_LOGIC SC_DMA_CHANNEL_TYPE_USER_LOGIC
#define SN_DMA_CHANNEL_TYPE_STANDARD_NIC SC_DMA_CHANNEL_TYPE_STANDARD_NIC
#define SN_DMA_CHANNEL_TYPE_UNUSED SC_DMA_CHANNEL_TYPE_UNUSED
#define SN_DMA_CHANNEL_TYPE_MONITOR SC_DMA_CHANNEL_TYPE_MONITOR
#define SN_DMA_CHANNEL_TYPE_UDP_MULTICAST SC_DMA_CHANNEL_TYPE_UDP_MULTICAST
#define SN_DMA_CHANNEL_TYPE_TCP SC_DMA_CHANNEL_TYPE_TCP

#endif /* SC_LEGACY_API */

/** @endcond */

/** **************************************************************************
 * @addtogroup ConstantsMacros Constants, macros, enumerations and data structures.
 * @{
 */

#define MAC_ADDR_LEN                        6                       /**< Size of MAC address in bytes. */
#define SC_MAC_ADDRESS_LENGTH               6                       /**< Size of MAC address in bytes. */
#define MAX_TX_PAYLOAD_LENGTH               (SC_BUCKET_SIZE - 16)   /**< Maximum length of a Tx payload. */

#define UNUSED_ALWAYS(x)    do { (void)(x); } while (0) /**< Disable compiler warning for unused variable. */

/**
 * Possible TCP connection status values returned by @ref SC_GetTcpState.
 */
typedef enum
{
    SC_TCP_NONE                 = 0xc,  /**< Not used */
    SC_TCP_CLOSED               = 0x0,  /**< Socket is closed */
    SC_TCP_LISTEN               = 0x1,  /**< Listen for incoming connection requests. */
    SC_TCP_SYN_SENT             = 0x2,  /**< Sent a connection request, waiting for ack. */
    SC_TCP_SYN_RECV             = 0x3,  /**< Received a connection request, sent ack, waiting for final ack in three-way handshake. */
    SC_TCP_ESTABLISHED          = 0x4,  /**< Connection established. */
    SC_TCP_CLOSE_WAIT           = 0x5,  /**< Remote side has shutdown and is waiting for us to finish writing our data and to shutdown (we have to close() to move on to LAST_ACK). */
    SC_TCP_FIN_WAIT1            = 0x6,  /**< Our side has shutdown. */
    SC_TCP_CLOSING              = 0x7,  /**< Both sides have shutdown but we still wait for ack. */
    SC_TCP_LAST_ACK             = 0x8,  /**< Our side has shutdown after remote has shutdown. */
    SC_TCP_FIN_WAIT2            = 0x9,  /**< Waiting for remote to shutdown. */
    SC_TCP_TIME_WAIT            = 0xa,  /**< Waiting for enough time to pass to be sure the remote end has received the ack of the connection termination request. */
    SC_TCP_CLOSED_DATA_PENDING  = 0xd   /**< Waiting to be closed but Rx data is still pending. */
} SC_TcpState;

/**
 *  ACL identifier specifying either Tx or Rx ACL or both.
 *  Used as a parameter to ACL related function calls.
 */
typedef enum
{
    SC_ACL_ID_NONE      = 0,    /**< ACL functionality is not supported. */
    SC_ACL_ID_TX        = 1,    /**< Identifies outgoing (Tx) ACL list. */
    SC_ACL_ID_RX        = 2,    /**< Identifies incoming (Rx) ACL list. */
    SC_ACL_ID_BOTH      = 3,    /**< Identifies both outgoing (Tx) and incoming (Rx) ACL list. */
    SC_ACL_ID_FAILED    = -1    /**< Error encountered in ACL functions. */
} SC_AclId;

/**
 *  Version structure for returning API and driver software versions.
 */
typedef struct
{
    char version[100];          /**< Version string. */
} SC_Version;

/**
 *  Time control selection modes
 */
typedef enum
{
    SC_TIMECONTROL_INVALID              = 0,    /**< Invalid setting for time control mode */
    SC_TIMECONTROL_HARDWARE             = 1,    /**< Time controlled through HW (the default) */
    SC_TIMECONTROL_SOFTWARE             = 2,    /**< Time controlled throug SW */
    SC_TIMECONTROL_SOFTWARE_FPGA_RESET  = 3     /**< This value is for the device driver internal use. Don't use it. */
} SC_TimeControl;

/**
 *  FPGA DMA channel types.
 */
typedef enum
{
    SC_DMA_CHANNEL_TYPE_NONE            = 0,    /**< (0) No or unknown channel type */
    SC_DMA_CHANNEL_TYPE_USER_LOGIC      = 1,    /**< (1) User logic channel type. */
    SC_DMA_CHANNEL_TYPE_STANDARD_NIC    = 2,    /**< (2) Standard NIC, also known as OOB (Out Of Band) */
    SC_DMA_CHANNEL_TYPE_UNUSED          = 3,    /**< (3) Unsed channel type. */
    SC_DMA_CHANNEL_TYPE_MONITOR         = 4,    /**< (4) Monitor channel type. */
    SC_DMA_CHANNEL_TYPE_UDP_MULTICAST   = 5,    /**< (5) UDP multicast channel type. */
    SC_DMA_CHANNEL_TYPE_TCP             = 6     /**< (6) ETLM TCP channel type. */

} SC_ChannelType;

/** @} ConstantsMacros */

/** @cond PRIVATE_CODE */

static inline bool MacAddressIsZero(const uint8_t macAddress[MAC_ADDR_LEN])
{
    if (macAddress == NULL)
    {
        return true;
    }
    if (macAddress[0] == 0 && macAddress[1] == 0 && macAddress[2] == 0 &&
        macAddress[3] == 0 && macAddress[4] == 0 && macAddress[5] == 0)
    {
        return true;
    }
    return false;
}

static inline bool MacAddressIsNotZero(const uint8_t macAddress[MAC_ADDR_LEN])
{
    if (macAddress == NULL)
    {
        return false;
    }
    if (macAddress[0] != 0 || macAddress[1] != 0 || macAddress[2] != 0 ||
        macAddress[3] != 0 || macAddress[4] != 0 || macAddress[5] != 0)
    {
        return true;
    }
    return false;
}

static inline bool MacAddressesAreEqual(const uint8_t macAddress1[MAC_ADDR_LEN], const uint8_t macAddress2[MAC_ADDR_LEN])
{
    if (macAddress1[0] == macAddress2[0] && macAddress1[1] == macAddress2[1] && macAddress1[2] == macAddress2[2] &&
        macAddress1[3] == macAddress2[3] && macAddress1[4] == macAddress2[4] && macAddress1[5] == macAddress2[5])
    {
        return true;
    }
    return false;
}

/* @endcond */

/** **************************************************************************
 * @defgroup ConstantsMacros Constants, macros, enumerations and data structures.
 * @{
 */

/**
 *  Register ids that have information available with @ref SC_GetFpgaRegisterInfo.
 */
typedef enum
{
    SC_FPGA_REGISTER_NONE                       = 0,    /**< (0) No register. */
    SC_FPGA_REGISTER_USER_LOGIC_TX_SEND         = 1,    /**< (1) User logic Tx send register. */
    SC_FPGA_REGISTER_RX_READ_MARKER             = 2,    /**< (2) Rx read marker register. */

} SC_FpgaRegister;

/** @} ConstantsMacros */

#endif /* __product_common_h__ */
