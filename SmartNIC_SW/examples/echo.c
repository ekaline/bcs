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
 * @file echo.c
 *
 * @brief Example of simple echo test.
 *
 * Open a connection. Echo back received payload
 */

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "smartnic.h"
#include "smartnic_tools.h"

volatile bool s_Stop = false;

static void sig_handler(int signo)
{
    /*printf("signal tid %ld\n", gettid());*/

    if (signo == SIGINT)
    {
        s_Stop = true;
        printf("\nCtrl-C caught, exiting...\n");
    }
}

static SC_Error ErrorHandler(const char * fileName, int lineNumber, const char * functionName, SC_Error errorCode, const char * errorMessage, void * pContext)
{
    UNUSED_ALWAYS(pContext);

    fprintf(stderr, "Error at %s:%d function %s: error code %d, %s\n", fileName, lineNumber, functionName, errorCode, errorMessage);
    return SC_ERR_HANDLER_CONTINUE;
}

static void help(FILE * pStream, char * argv[])
{
    fprintf(pStream,
        "Echo TCP packets received from a remote host.\n"
        "\n"
        "Usage: %s [options] IP-address port mode method\n"
        "\n", argv[0]);
    fprintf(pStream,
        "where options are:\n"
        "    -h/--help          This help.\n"
        "    -d/--device        SmartNIC device name. Default device name is /dev/smartnic.\n"
        "    --toelane          TOE lane to use. Default is 0.\n"
        "    --toechannel       TOE channel to to use. Default is -1 (driver selects a free channel).\n");
    fprintf(pStream,
        "    --vlan             VLAN tag to use. Default is -1 for no VLAN.\n"
        "    IP-Address         Remote IP address.\n"
        "    port               Remote port.\n"
        "    mode               0=connect | 1=listen.\n"
        "    method             0=wait forever | 1=wait with timeout.\n");

}

int main(int argc, char * argv[])
{
    const char *      deviceName = NULL;
    SC_DeviceId       deviceId;
    SC_ChannelId      channelId;
    SC_Error          errorCode;

    /* Default values */
    int mode = 0;   /* 0 for tcp connect, 1 for tcp listen */
    int meth = 0;   /* 0 for wait forever, 1 for wait with timeout */
    int rc = EXIT_SUCCESS;
    const char * ipAddress = "172.16.10.1";
    uint16_t port = 2000;

    const char * toeLaneArg = NULL;
    uint8_t toeLane = 0;
    int16_t toeChannel = -1;
    int16_t vlanTag = -1;

    const SC_Packet * pNextPacket = NULL;
    const SC_Packet * pPreviousPacket = NULL;
    int16_t bucketSize;

    int optionIndex = 0, argumentIndex = 0;
    bool exitAfterOptionsParsing = false;

    /* Register signal handler */
    if (signal(SIGINT, &sig_handler) == SIG_ERR)
    {
        fprintf(stderr, "Cannot register signal handler\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);

    SC_RegisterErrorHandler(&ErrorHandler, NULL);

    while (true)
    {
        static struct option longOptions[] =
        {
            { "device",     required_argument,  0, 'd' },
            { "help",       required_argument,  0, 'h' },
            { "toelane",    required_argument,  0, 0 },
            { "toechannel", required_argument,  0, 0 },
            { "vlan",       required_argument,  0, 0 },
            { 0,            0,                  0, 0 }
        };

        int optionChar = getopt_long(argc, argv, "d:h", longOptions, &optionIndex);

        if (optionChar == -1)
            break;

        switch (optionChar)
        {
            case 0:
                {
                    const char * name = longOptions[optionIndex].name;
                    if (strcmp(name, "toelane") == 0)
                    {
                        toeLaneArg = optarg;
                    }
                    else if (strcmp(name, "toechannel") == 0)
                    {
                        toeChannel = (int16_t)SCI_GetNumber(optarg, "TOE channel", 0, 63, true);
                    }
                    else if (strcmp(name, "vlan") == 0)
                    {
                        vlanTag = (int16_t)SCI_GetNumber(optarg, "VLAN tag", 1, 4094, true);
                    }
                    else
                    {
                        fprintf(stderr, "Unknown command line option: %s\n", name);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'd':
                deviceName = optarg;
                break;

            default:
            case '?':
                help(stdout, argv);
                exitAfterOptionsParsing = true;
                rc = EXIT_FAILURE;
                break;

            case 'h':
                help(stdout, argv);
                exitAfterOptionsParsing = true;
                rc = EXIT_SUCCESS;
                break;
        }
    }

    if (exitAfterOptionsParsing)
    {
        exit(rc);
    }

    while (optind < argc)
    {
        /* Non-option arguments */

        const char * argument = argv[optind++];
        if (argument != NULL)
        {
            switch (argumentIndex++)
            {
                case 0:     /* IP address */
                    {
                        struct in_addr    ip_addr_arg;

                        if (inet_aton(argument, &ip_addr_arg) == 0)
                        {
                            fprintf(stderr, "Invalid IP address '%s'\n", argument);
                            exit(EXIT_FAILURE);
                        }
                        ipAddress = argument;
                    }
                    break;

                case 1:     /* port */
                    port = (uint16_t)SCI_GetNumber(argument, "IP port", 0, 65535, false);
                    break;

                case 2:     /* mode */
                    mode = (int)SCI_GetNumber(argument, "mode", 0, 1, false);
                    break;

                case 3:     /* method */
                    meth = (int)SCI_GetNumber(argument, "method", 0, 1, false);
                    break;

                default:
                    fprintf(stderr, "Unknown non-option argument: %s\n", argument);
                    exit(EXIT_FAILURE);
                    break;
            }
        }
    }

    if (argumentIndex < 4)
    {
        switch (argumentIndex)
        {
            default:
            case 0: fprintf(stderr, "Missing or invalid IP address!\n"); break;
            case 1: fprintf(stderr, "Missing or invalid IP port!\n"); break;
            case 2: fprintf(stderr, "Missing or invalid mode!\n"); break;
            case 3: fprintf(stderr, "Missing or invalid method!\n"); break;
        }
        exit(EXIT_FAILURE);
    }

    /** [sn_init_call] */
    /* Open device, allocate and initialise internal data structures */
    deviceId = SC_OpenDevice(deviceName, NULL);
    if (deviceId == NULL)
    {
        fprintf(stderr, "Cannot open device '%s'\n", deviceName == NULL ? "/dev/smartnic" : deviceName);
        return EXIT_FAILURE;
    }
    /** [sn_init_call] */

    if (toeLaneArg != NULL)
    {
        toeLane = (uint8_t)SCI_GetNumber(toeLaneArg, "TOE lane", 0, 15, false);
        if (!SC_IsTcpNetworkInterface(deviceId, toeLane))
        {
            fprintf(stderr, "Lane %s is not configured as a TOE lane!\n", toeLaneArg);
            rc = EXIT_FAILURE;
            goto cleanup;
        }
    }

    /* Allocate a TCP channel on the card */
    channelId = SC_AllocateTcpChannel(deviceId, toeLane, toeChannel, NULL);
    if (channelId == NULL)
    {
        fprintf( stderr, "Cannot allocate TCP channel\n" );
        rc = EXIT_FAILURE;
        goto cleanup;
    }
    /** [sn_allocate_tcp_channel_call] */

    if (mode == 0)
    {
        SC_ConnectOptions connectOptions;
        if (SC_GetConnectOptions(channelId, &connectOptions) != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Failed to get default connect options.\n");
            rc = EXIT_FAILURE;
            goto cleanup;
        }
        connectOptions.VLanTag = vlanTag;

        /** [sn_connect_call]SC_GetNextPacket */
        /* Make the connection to 172.16.10.1 port 2000 */
        printf("TOE connecting to %s:%u ...\n", ipAddress, port);
        if (SC_Connect(channelId, ipAddress, port, &connectOptions) != SC_ERR_SUCCESS)
        {
            fprintf( stderr, "Cannot establish connection\n" );
            rc = EXIT_FAILURE;
            goto cleanup;
        }
        /** [sn_connect_call] */
    }
    else
    {
        SC_ListenOptions listenOptions;
        if (SC_GetListenOptions(channelId, &listenOptions) != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Failed to get default listen options.\n");
            rc = EXIT_FAILURE;
            goto cleanup;
        }
        listenOptions.VLanTag = vlanTag;

        printf("listening on TOE port %u...\n", port);
        if (SC_Listen(channelId, port, &listenOptions) != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "listening failed\n");
            rc = EXIT_FAILURE;
            goto cleanup;
        }
    }
    fprintf( stderr, "Connection established\n" );

#if 0
    /** [file_descriptor_method] */
    /* Example where we use the file descriptor to read incoming data. */

    char buffer[512];
    int readLength = 0;

    /* Get file descriptor */
    int fd = SC_GetFileDescriptor(deviceId);
    SC_Bucket * pBucket = SC_GetBucket(channelId);
    uint8_t * bucketPayload = SC_GetBucketPayload(pBucket);

    while (true)
    {
        do
        {
            readLength = read(fd, buffer, 512);
        } while (readLength == -1);

        /* Copy the received data into a bucket */
        memcpy(bucketPayload, buffer, readLength);

        /* Echo back the received data */
        errorCode = SC_SendBucket(channelId, pBucket, readLength, SC_SEND_DEFAULT, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "SC_SendBucket failed with error code %d\n", errorCode);
        }
    }
    /** [file_descriptor_method] */

#else

    /** [direct_access_method] */
    /* Example where we directly access the circular packet buffer where incoming data is delivered */

    bucketSize = SC_GetMaxBucketPayloadSize(channelId);

    while (!s_Stop)
    {
        uint16_t payloadLength;

        /* Wait for the receiving payload to arrive */
        if (meth == 0)
        {
            do
            {
                pNextPacket = SC_GetNextPacket(channelId, pPreviousPacket, SC_TIMEOUT_NONE);
                if (s_Stop)
                {
                    goto cleanup;
                }
            } while (pNextPacket == NULL);
        }
        else
        {
            pNextPacket = SC_GetNextPacket(channelId, pPreviousPacket, 1000 * 1000); /* Wait one second */
        }
        if (pNextPacket == NULL)
        {
            fprintf(stderr, "No packet received\n");
            rc = EXIT_FAILURE;
            goto cleanup;
        }

        /* Process the received */
        payloadLength = SC_GetTcpPayloadLength(pNextPacket);
        if (payloadLength > 0)
        {
            uint16_t i;
            SC_Bucket * pEchoBucket;
            const uint8_t * payload = SC_GetTcpPayload(pNextPacket);
            uint8_t * pEchoBucketPayload;

            printf("got packet of size %d:\n", payloadLength);
            for (i = 0; i < payloadLength; ++i)
            {
                printf( "%c", payload[i] );
            }
            printf("\n");

            if (payloadLength > bucketSize)
            {
                payloadLength = (uint16_t)bucketSize;
            }
            /* Copy the received data to bucket 0 */

            /* Get the bucket we will use for transmitting */
            pEchoBucket = SC_GetBucket(channelId, 0);
            pEchoBucketPayload = SC_GetBucketPayload(pEchoBucket);
            memcpy(pEchoBucketPayload, payload, payloadLength );
            /* Echo back the received data */
            /* [11:0] payload len ; [31:12] address for 4k boundary (PAGE_SIZE) */
            errorCode = SC_SendBucket(channelId, pEchoBucket, payloadLength, SC_SEND_DEFAULT, NULL);
            if (errorCode != SC_ERR_SUCCESS)
            {
                fprintf(stderr, "SC_SendBucket failed with error code %d\n", errorCode);
            }
            /* Tell the FPGA that this packet has been processed. */
            /* FPGA can reuse the packet ring buffer up to and including this packet. */
            SC_UpdateReceivePtr(channelId, pNextPacket);
        }

        usleep(100000); /* Wait 100 milliseconds */

        pPreviousPacket = pNextPacket;
    }
    /** [direct_access_method] */

#endif

cleanup:

    /* Clean up */
    SC_CloseDevice(deviceId);

    printf("Closed device\n");

    return rc;
}
