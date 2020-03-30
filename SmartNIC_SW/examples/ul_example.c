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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <xmmintrin.h>

#include "smartnic.h"

static volatile bool stopCondition = false;

/**
 *  Print error message.
 */
static void Error(const char * format, ...) __attribute__((format(printf, 1, 2)));

static void Error(const char * format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    fprintf(stderr, " *** ERROR: ");
    vfprintf(stderr, format, arguments);
    va_end(arguments);
}

/**
 *  API library error handler.
 */
static SC_Error ErrorHandler(
    const char *    fileName,
    int             lineNumber,
    const char *    functionName,
    SC_Error        errorCode,
    const char *    errorMessage,
    void *          pUserContext)
{
    Error("API library file %s line %d function %s error code %d: %s\n", fileName, lineNumber, functionName, errorCode, errorMessage);

    return errorCode;
}

/**
 *  Signal handler to handle Ctrl-C breaks.
 */
static void SignalHandler(int signal)
{
    if (signal == SIGINT)
    {
        if (stopCondition)
        {
            /* Second or more Ctrl-C, exit now */
            printf("\nSecond Ctrl-C detected. Exiting!\n");
            exit(EXIT_FAILURE);
        }
        printf("\nCtrl-C detected!\n");
        stopCondition = true;
    }
}

/**
 *  Print help text.
 */
static void HelpText(FILE * pStream, uint16_t defaultPayloadLength)
{
    fprintf(pStream, "\n%s user logic send/receive example.\n\n", PRODUCT);
    fprintf(pStream, "Usage:\n\n");
    fprintf(pStream,
        "ul_example [-d|--device <device-name>] [-c|--channel <user-logic-channel-number>] [-l--length <packet-length>]\n"
        "           [-n|--count <number-of-packets-to-send>] [-e|--echodemo] [-h|--help] [-v|--verbose]\n");
    fprintf(pStream,
        "     Send and receive a number of packets to FPGA user logic demo which echoes them back.\n\n");
    fprintf(pStream, "where:\n"
        "     -d|--device    %s device name. Default name is /dev/%s.\n"
        "     -n|--count     Number of packets to send. Default is 10.\n"
        "     -l|--length    Payload length to send. Default is %u.\n", PRODUCT, LOWER_PRODUCT, defaultPayloadLength);
    fprintf(pStream,
        "     -c|--channel   User logic channel number in range 0-7.\n"
        "                    Default channel number is 0.\n"
        "     -e|--echodemo  Enable FPGA echo demo that echoes all sent data.\n"
        "     -v|--verbose   Print received packet count and length when packets are received.\n"
        "     -h|--help      Show this help.\n");
}

/**
 *  Show progress.
 */
static void ShowProgress(const char * message, uint64_t packetCount, uint64_t packetCountLimit, uint64_t totalNumberOfBytes, const char * lineEnding)
{
    uint64_t percentage = 100UL * packetCount;
    percentage /= packetCountLimit;
    fprintf(stdout, "%s%lu%%, %lu packets of %lu, %lu bytes sent and received%s",
        message, percentage, packetCount, packetCountLimit, totalNumberOfBytes, lineEnding);
    fflush(stdout);
}

/**
 *  Program main.
 */
int main(int argc, char * argv[])
{
    const char *            deviceName = NULL;
    SC_DeviceId             deviceId;
    SC_ChannelId            userLogicChannelId;
    SC_Error                errorCode;
    int16_t                 userLogicChannelNumber = 0;
    bool                    enableEchoDemo = false;
    bool                    verbose = false;
    uint64_t                count = 10;
    const uint16_t          DefaultPayloadLength = 1500;
    uint16_t                sendPayloadLength = DefaultPayloadLength;
    int                     rc = EXIT_SUCCESS;
    uint64_t                packetCount = 0;
    uint8_t                 payloadData = 0;
    uint64_t                totalNumberOfBytes = 0;
    const SC_Packet *       pPreviousPacket = NULL;
    int64_t                 bytesReceivedSinceLastUpdate = 0;
    const SC_Packet *       pMinPacketAddress = (const SC_Packet *)~0;
    const SC_Packet *       pMaxPacketAddress = NULL;
    uint64_t                packetAddressRange;

    /* Register signal handler */
    if (signal(SIGINT, &SignalHandler) == SIG_ERR)
    {
        Error("registering signal handler failed with errno %d\n", errno);
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);

    errorCode = SC_RegisterErrorHandler(ErrorHandler, NULL);
    if (errorCode != SC_ERR_SUCCESS)
    {
        Error("SC_RegisterErrorHandler failed with error code %d\n", errorCode);
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        int optionIndex = 0;
        static struct option longOptions[] =
        {
            { "device",     required_argument,  0, 'd' },
            { "count",      required_argument,  0, 'n' },
            { "channel",    required_argument,  0, 'c' },
            { "length",     required_argument,  0, 'l' },
            { "echodemo",   no_argument,        0, 'e' },
            { "verbose",    no_argument,        0, 'v' },
            { "help",       no_argument,        0, 'h' },
            { 0,            0,                  0, 0 }
        };

        int optionChar = getopt_long(argc, argv, "d:n:c:l:evh", longOptions, &optionIndex);

        if (optionChar == -1)
            break;

        switch (optionChar)
        {
            case 0:
                {
                    const char * name = longOptions[optionIndex].name;
                    Error("unknown command line option: %s\n", name);
                    exit(EXIT_FAILURE);
                }
                break;

            case 'd':
                deviceName = optarg;
                break;

            case 'c':
                {
                    int64_t channel = atol(optarg);
                    int64_t numberOfUserLogicChannels = SC_GetNumberOfUserLogicChannels();
                    if (channel < 0 || channel >= numberOfUserLogicChannels)
                    {
                        Error("user logic channel number %s must be in range 0-%ld\n", optarg, numberOfUserLogicChannels - 1);
                        exit(EXIT_FAILURE);
                    }
                    userLogicChannelNumber = (int16_t)channel;
                }
                break;

            case 'n':
                {
                    long value = atol(optarg);
                    if (value > 0)
                    {
                        count = (uint64_t)value;
                    }
                    else
                    {
                        Error("-n/--count value '%s' has to be greater than 0\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'l':
                {
                    long value = atol(optarg);
                    if (value > 0)
                    {
                        int16_t maxMTU = SC_GetMaxBucketPayloadSize(NULL);
                        if (value > maxMTU)
                        {
                            Error("-l/--length value '%s' has to be less than maximum MTU %d\n", optarg, maxMTU);
                            exit(EXIT_FAILURE);
                        }
                        sendPayloadLength = (uint64_t)value;
                    }
                    else
                    {
                        Error("-l/--length value '%s' has to be greater than 0\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'e':
                enableEchoDemo = true;
                break;

            case 'v':
                verbose = true;
                break;

            case 'h':
                HelpText(stdout, DefaultPayloadLength);
                exit(EXIT_SUCCESS);

            default:
                Error("unrecognized command line option '%c'\n", optionChar);
                exit(EXIT_FAILURE);
        }
    }

    /** [sc_init_device_context] */
    deviceId = SC_OpenDevice(deviceName, NULL);
    if (deviceId == NULL)
    {
        Error("cannot initialize device context, error code %d\n", SC_GetLastErrorCode());
        exit(EXIT_FAILURE);
    }
    /** [sc_init_device_context] */

    /** [sc_init_demo_design] */
    /* This user logic demo echoes all sent packets back. */
    if (enableEchoDemo)
    {
        errorCode = SC_ActivateDemoDesign(deviceId, SC_DEMO_USER_LOGIC_GENERIC, 1, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            Error("SC_ActivateDemoDesign enable failed with error code %d\n", errorCode);
            rc = EXIT_FAILURE;
            goto exit;
        }
    }
    /** [sc_init_demo_design] */

    /** [sc_allocate_ul_example.channel_call] */
    /* Allocate user logic channel */
    userLogicChannelId = SC_AllocateUserLogicChannel(deviceId, userLogicChannelNumber, NULL);
    if (userLogicChannelId == NULL)
    {
        Error("cannot allocate user logic channel\n");
        rc = EXIT_FAILURE;
        goto exit;
    }

    /** [sc_allocate_ul_example.channel_call] */

    while (!stopCondition && packetCount < count)
    {
        uint8_t *           pBucketPayload;
        const SC_Packet *   pCurrentPacket;
        const uint8_t *     pReceivePayload;
        uint16_t            receivePayloadLength;
        uint16_t            i;

        /** [sc_get_free_bucket] */
        /* Copy the payload to a free bucket */
        SC_Bucket * pBucket = NULL;
        while ((pBucket = SC_GetBucket(userLogicChannelId, 0)) == NULL)
        {
            _mm_pause();
        }
        pBucketPayload = SC_GetBucketPayload(pBucket);

        /* Fill bucket payload */
        for (i = 0; i < sendPayloadLength; ++i)
        {
            pBucketPayload[i] = payloadData++;
        }
        /** [sc_get_free_bucket] */

        /** [sc_send_packet] */
        /* Deliver bucket to FPGA for transmission */
        errorCode = SC_SendBucket(userLogicChannelId, pBucket, sendPayloadLength, SC_SEND_DEFAULT, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            Error("SC_SendBucket failed with error code %d\n", errorCode);
            rc = EXIT_FAILURE;
            goto exit;
        }
        /** [sc_send_packet] */

        /** [sc_wait_for_incoming_packet] */
        /* Wait for the receiving payload to arrive */
        pCurrentPacket = SC_GetNextPacket(userLogicChannelId, pPreviousPacket, SC_TIMEOUT_INFINITE);
        receivePayloadLength = SC_GetUserLogicPayloadLength(pCurrentPacket);
        pReceivePayload = SC_GetUserLogicPayload(pCurrentPacket);
        /** [sc_wait_for_incoming_packet] */

        /* Count incoming packets */
        ++packetCount;

        /** [sc_release_incoming_packet] */
        /* Tell FPGA we have processed the packet and it can reuse the space */
        /* For efficiency wait until half of the Packet Ring Buffer has been filled */
        /* so that several packets are acknowledged with one call to SC_UpdateReceivePtr. */
        /* This is just a heuristic optimization, some other value or criteria could be used. */
        bytesReceivedSinceLastUpdate += 16 + ((receivePayloadLength + 63) & ~63);
        if (bytesReceivedSinceLastUpdate > 2 * 1024 * 1024)
        {
            errorCode = SC_UpdateReceivePtr(userLogicChannelId, pCurrentPacket);
            if (errorCode != SC_ERR_SUCCESS)
            {
                Error("SC_UpdateReceivePtr failed with error code %d\n", errorCode);
                rc = EXIT_FAILURE;
                goto exit;
            }
            bytesReceivedSinceLastUpdate = 0;
        }
        /** [sc_release_incoming_packet] */

        /* Check received payload matches what was sent. */
        if (sendPayloadLength != receivePayloadLength)
        {
            Error("sent %u bytes but received %u bytes\n", sendPayloadLength, receivePayloadLength);
            rc = EXIT_FAILURE;
            goto exit;
        }
        else
        {
            /* Lengths match, check contents */
            if (memcmp(pBucketPayload, pReceivePayload, sendPayloadLength) != 0)
            {
                /* Contents mismatch, find out where */
                int byteErrorCount = 0;

                for (i = 0; i < sendPayloadLength; ++i)
                {
                    if (pBucketPayload[i] != pReceivePayload[i])
                    {
                        Error("sent data[%u]=0x%02X != received data[%u]=0x%02X\n", i, pBucketPayload[i], i, pReceivePayload[i]);
                        rc = EXIT_FAILURE;
                        if (++byteErrorCount > 10)
                        {
                            goto exit;
                        }
                    }
                }
            }
        }

        /* Accumulate byte counts */
        totalNumberOfBytes += sendPayloadLength;

        /* Advance packet pointer, next packet is received after the current packet */
        pPreviousPacket = pCurrentPacket;

        if (verbose && (packetCount % 10000) == 0)
        {
            ShowProgress("", packetCount, count, totalNumberOfBytes, "  \r");
        }

        if (pCurrentPacket < pMinPacketAddress)
        {
            pMinPacketAddress = pCurrentPacket;
        }
        if (pCurrentPacket > pMaxPacketAddress)
        {
            pMaxPacketAddress = pCurrentPacket;
        }
    }

    ShowProgress("Total of ", packetCount, count, totalNumberOfBytes, "");
    if (verbose)
    {
        printf(" on DMA channel %d\n", SC_GetDmaChannelNumber(userLogicChannelId));
    }
    packetAddressRange = (uint64_t)pMaxPacketAddress - (uint64_t)pMinPacketAddress;
    printf("Received packet address range was 0x%lX (%ld) bytes, %.3f MB\n", packetAddressRange, packetAddressRange, packetAddressRange / 1024.0 / 1024.0);

exit:

    /** [sc_disable_demo_design] */
    if (enableEchoDemo)
    {
        /* Reuse previous demo design options */
        errorCode = SC_ActivateDemoDesign(deviceId, SC_DEMO_USER_LOGIC_GENERIC, 0, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            Error("user logic demo disable failed with error code %d\n", errorCode);
            rc = EXIT_FAILURE;
        }
    }
    /** [sc_disable_demo_design] */

    /** [sc_deallocate_contexts] */
    /* Deallocate device and channel contexts */
    errorCode = SC_CloseDevice(deviceId);
    if (errorCode != SC_ERR_SUCCESS)
    {
        Error("close device failed with error code %d\n", errorCode);
        rc = EXIT_FAILURE;
    }
    /** [sc_deallocate_contexts] */

    return rc;
}
