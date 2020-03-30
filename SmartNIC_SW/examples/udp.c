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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <xmmintrin.h>

#include "smartnic.h"

#define MAX_NUMBER_OF_GROUPS  64

static volatile bool stopCondition = false;

static void SignalHandler(int signal)
{
    if (signal == SIGINT)
    {
        printf("\nCtrl-C detected!\n");
        if (stopCondition)
        {
            /* Second or more Ctrl-C, exit now */
            exit(1);
        }
        stopCondition = true;
    }
}

/** Print help text
*/
static void help_text(FILE * pStream, const char * defaultRemoteHost, uint16_t defaultRemotePort)
{
    fprintf(pStream, "Usage:\n\n");
    fprintf(pStream,
        "udp [-d/--device <device-name>] [--udplane <lane>] [--tpclane <lane>] [--host <remote-host>] [--port <remote-port>]\n"
        "    [--toechannel <toe-channel-number>] [--udpchannel <udp-channel-number>] [--vlan <vlan-tag>]\n");
    fprintf(pStream,
        "     Receive a number of UDP packets from one or more multicast group IP\n"
        "     addresses and echo them back to a remote host.\n");
    fprintf(pStream, "udp -h | --help\n"
        "     Show this text\n\n");
    fprintf(pStream, "Where:\n"
        "     -d/--device    SmartNIC device name. Default is /dev/smartnic\n"
        "     -g/--group     The ip address of the multicast group. Repeat this option\n"
        "                    for each desired multicast group. This address may have an\n"
        "                    optional port number attached in which case only broadcast\n"
        "                    packets sent to that port will get through.\n");
    fprintf(pStream,
        "     -n/--count     Echo to remote host every count'th incoming UDP packet. Default is 10.\n"
        "     --udplane      Use this specific lane to receive UDP data. Default lane is 1.\n");
    fprintf(pStream,
            "     --udpchannel   UDP channel number in range 0-31 or -1. Default UDP channel is -1 (driver assigns the number).\n"
        "     --toelane      Use this specific lane to send TCP data. Default lane is 0.\n"
        "     --toechannel   TOE channel number in range 0-63 or -1. Default TOE channel is -1 (driver assigns the number).\n"
        "     --vlan         VLAN tag value 1-4094 or -1 for no VLAN. Default is -1.\n");
    fprintf(pStream,
        "     --host         Echo received packets to this remote host. Default host is %s.\n"
        "     --port         Echo received packets to this remote port. Default port is %u.\n"
        "     -v/--verbose   Print received packet count and length when packets are received.\n",
        defaultRemoteHost, defaultRemotePort);
}

int main(int argc, char * argv[])
{
    const char * deviceName = NULL;
    SC_DeviceId deviceId;
    SC_ChannelId udpChannelId, tcpChannelId;
    SC_Error errorCode;
    const char * toeLaneArg = NULL;
    uint8_t toeLane = 0;
    int16_t toeChannel = -1;
    const char * udpLaneArg = NULL;
    uint8_t udpLane = 1;
    int16_t udpChannel = -1;
    int16_t vlanTag = -1;
    bool verbose = false;
    int i = 0;
    const char * defaultRemoteHost = "172.16.10.1";
    const char * remoteHost = defaultRemoteHost;
    const uint16_t defaultRemotePort = 2000;
    uint16_t remotePort = defaultRemotePort;
    uint32_t count = 10;
    int rc = EXIT_SUCCESS;
    SC_ConnectOptions connectOptions;

    const SC_Packet * pPreviousUdpPacket = NULL;
    uint32_t packetCount = 0;

    int group_count = -1;
    struct
    {
        const char *    ip;
        uint16_t        port;
    } groups[MAX_NUMBER_OF_GROUPS] =
    {
        { "239.2.0.2", 0 },
        { NULL, 0 }
    };

    /* Register signal handler */
    if (signal(SIGINT, &SignalHandler) == SIG_ERR)
    {
        fprintf(stderr, "Cannot register signal handler\n");
        exit(-1);
    }

    signal(SIGPIPE, SIG_IGN);

    while (true)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            { "device",      required_argument,  0, 'd' },
            { "group",       required_argument,  0, 'g' },
            { "count",       required_argument,  0, 'n' },
            { "host",        required_argument,  0, 0 },
            { "port",        required_argument,  0, 0 },
            { "udplane",     required_argument,  0, 0 },
            { "toelane",     required_argument,  0, 0 },
            { "toechannel",  required_argument,  0, 0 },
            { "udpchannel",  required_argument,  0, 0 },
            { "vlan",        required_argument,  0, 0 },
            { "verbose",     no_argument,        0, 'v' },
          /*{ "version",     no_argument,        0, 'V' },*/
            { "help",        no_argument,        0, 'h' },
            { 0,            0,                  0, 0 }
        };

        int option_chr = getopt_long(argc, argv, "d:g:n:vh", long_options, &option_index);

        if (option_chr == -1)
            break;

        switch (option_chr)
        {
            case 0:
                {
                    const char * name = long_options[option_index].name;
                    if (strcmp(name, "udplane") == 0)
                    {
                        udpLaneArg = optarg;
                    }
                    else if (strcmp(name, "toelane") == 0)
                    {
                        toeLaneArg = optarg;
                    }
                    else if (strcmp(name, "host") == 0)
                    {
                        remoteHost = optarg;
                    }
                    else if (strcmp(name, "port") == 0)
                    {
                        remotePort = (uint16_t)atoi(optarg);
                    }
                    else if (strcmp(name, "toechannel") == 0)
                    {
                        int64_t channel = atol(optarg);
                        if (channel != -1 && (channel < 0 || channel > 63))
                        {
                            fprintf(stderr, "TOE channel number %s must be -1 or 0-63\n", optarg);
                            exit(EXIT_FAILURE);
                        }
                        toeChannel = (int16_t)channel;
                    }
                    else if (strcmp(name, "udpchannel") == 0)
                    {
                        int64_t channel = atol(optarg);
                        if (channel != -1 && (channel < 0 || channel > 63))
                        {
                            fprintf(stderr, "UDP channel number %s must be -1 or 0-31\n", optarg);
                            exit(EXIT_FAILURE);
                        }
                        udpChannel = (int16_t)channel;
                    }
                    else if (strcmp(name, "vlan") == 0)
                    {
                        int64_t vlanTagValue = atol(optarg);
                        if (vlanTagValue != -1 && (vlanTagValue < 1 || vlanTagValue > 4094))
                        {
                            fprintf(stderr, "VLAN tag %s must be -1 or 1-4094\n", optarg);
                            exit(EXIT_FAILURE);
                        }
                        vlanTag = (int16_t)vlanTagValue;
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

            case 'n':
                i = atoi(optarg);
                if (i >= 1)
                {
                    count = (unsigned int)i;
                }
                else
                {
                    fprintf(stderr, "-n/--count value '%s' has to be greater than 0\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

            case 'g':
                {
                    char* opt_ip;
                    char* opt_port;

                    if (group_count == -1)
                    {
                        group_count = 0;
                    }
                    if (group_count >= MAX_NUMBER_OF_GROUPS)
                    {
                        fprintf(stderr, "The maximum number of groups ( %d ) is reached\n", MAX_NUMBER_OF_GROUPS);
                        exit(EXIT_FAILURE);
                    }

                    /* Get group IP address */
                    opt_ip = strtok(optarg, ":");
                    groups[group_count].ip = opt_ip;

                    /* Get optional port number in group address */
                    opt_port = strtok(NULL, ":");
                    if (opt_port)
                    {
                        int port = atoi(opt_port);
                        if (port < 0 || port > 65535)
                        {
                            fprintf(stderr, "Port number (%s) has to be between 0 and 65535\n", opt_port);
                            exit(EXIT_FAILURE);
                        }
                        groups[group_count].port = (uint16_t)port;
                    }
                    else
                    {
                        groups[group_count].port = 0;
                    }
                    ++group_count;
                }
                break;

            case 'v':
                verbose = true;
                break;

            /*case 'V':
                break;*/

            case 'h':
                help_text(stdout, defaultRemoteHost, defaultRemotePort);
                exit(EXIT_SUCCESS);

            default:
                fprintf(stderr, "Unrecognized option '%c'\n", option_chr);
                exit(EXIT_FAILURE);
        }
    }

    if (group_count == -1)
    {
        group_count = 1; /* Use default values */
    }

    if (verbose)
    {
        printf("IGMP groups:\n");
        for (i = 0; i < group_count; ++i)
        {
            printf("%d: %s:%u\n", i + 1, groups[i].ip, groups[i].port);
        }
    }

    /** [sn_init_device_context] */
    deviceId = SC_OpenDevice(deviceName, NULL);
    if (deviceId == NULL)
    {
        fprintf(stderr, "Cannot initialize device context, error code %d\n", SC_GetLastErrorCode());
        exit(EXIT_FAILURE);
    }
    /** [sn_init_device_context] */

    /* Sanity check of TOE and UDP lane paramaters */
    if (toeLaneArg != NULL)
    {
        uint64_t lane = (uint64_t)atol(toeLaneArg);
        if (lane > 255 || !SC_IsTcpNetworkInterface(deviceId, (uint8_t)lane))
        {
            fprintf(stderr, "Lane %s is not configured as a TOE lane!\n", toeLaneArg);
            rc = EXIT_FAILURE;
            goto exit;
        }
        toeLane = (uint8_t)lane;
    }
    if (udpLaneArg != NULL)
    {
        uint64_t lane = (uint64_t)atol(udpLaneArg);
        if (lane > 255 || !SC_IsUdpNetworkInterface(deviceId, (uint8_t)lane))
        {
            fprintf(stderr, "Lane %s is not configured as an UDP lane!\n", udpLaneArg);
            rc = EXIT_FAILURE;
            goto exit;
        }
        udpLane = (uint8_t)lane;
    }

    /** [sn_allocate_udp_channel_call] */
    /* Allocate UDP channel */
    udpChannelId = SC_AllocateUdpChannel(deviceId, udpLane, udpChannel, NULL);
    if (udpChannelId == NULL)
    {
        fprintf(stderr, "Cannot allocate UDP channel\n");
        rc = EXIT_FAILURE;
        goto exit;
    }
    /** [sn_allocate_udp_channel_call] */

    /** [sn_allocate_tcp_channel_call] */
    /* Allocate TCP channel */
    tcpChannelId = SC_AllocateTcpChannel(deviceId, toeLane, toeChannel, NULL);
    if (tcpChannelId == NULL)
    {
        fprintf(stderr, "Cannot allocate TCP channel\n");
        rc = EXIT_FAILURE;
        goto exit;
    }
    /** [sn_allocate_tcp_channel_call] */

    /** [sn_connect_call] */
    /* Connect to server at 172.16.10.1:2000 */
    if ((errorCode = SC_GetConnectOptions(tcpChannelId, &connectOptions)) != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to get default connect options, error coce %d\n", errorCode);
        rc = EXIT_FAILURE;
        goto exit;
    }
    connectOptions.VLanTag = vlanTag;

    errorCode = SC_Connect(tcpChannelId, remoteHost, remotePort, &connectOptions);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Cannot open connection to %s:%u, error code %d - is your local interface setup with 'fbconfig'?\n", remoteHost, remotePort, errorCode);
        rc = EXIT_FAILURE;
        goto exit;
    }
    /** [sn_connect_call] */

    printf("Connection is established\n");

    /** [sn_join_multicast_group] */
    for (i = 0; i < group_count; ++i)
    {
        errorCode = SC_IgmpJoin(udpChannelId, udpLane, groups[i].ip, groups[i].port, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Failed to join to a multicast group, error code %d\n", errorCode);
            rc = EXIT_FAILURE;
            goto exit;
        }
    }
    /** [sn_join_multicast_group] */

    while (!stopCondition)
    {
        /** [sn_wait_for_incoming_packet] */
        /* Wait for the receiving payload to arrive */
        const SC_Packet * pIncomingUdpPacket = SC_GetNextPacket(udpChannelId, pPreviousUdpPacket, SC_TIMEOUT_INFINITE);
        /** [sn_wait_for_incoming_packet] */

        /* We do something for every count'th incoming UDP packet */
        if (++packetCount % count == 0)
        {
            uint8_t * pBucketPayload;
            uint16_t payloadLength;

            /** [sn_send_incoming_packet] */
            /* Copy the payload to a free bucket */
            SC_Bucket * pBucket = NULL;
            while ((pBucket = SC_GetBucket(tcpChannelId, 0)) == NULL)
            {
                _mm_pause();
            }
            pBucketPayload = SC_GetBucketPayload(pBucket);
            payloadLength = SC_GetUdpPayloadLength(pIncomingUdpPacket);
            memcpy(pBucketPayload, SC_GetUdpPayload(pIncomingUdpPacket), payloadLength);

            /* Deliver bucket to FPGA for transmission */
            errorCode = SC_SendBucket(tcpChannelId, pBucket, payloadLength, SC_SEND_DEFAULT, NULL);
            if (errorCode != SC_ERR_SUCCESS)
            {
                fprintf(stderr, "SC_SendBucket failed with error code %d\n", errorCode);
            }
            /** [sn_send_incoming_packet] */
        }

        /** [sn_release_incoming_packet] */
        /* Tell FPGA we have processed the packet and it can reuse the space */
        SC_UpdateReceivePtr(udpChannelId, pIncomingUdpPacket);
        /** [sn_release_incoming_packet] */

        pPreviousUdpPacket = pIncomingUdpPacket;

        if (verbose)
        {
            printf("%u packets\r", packetCount);
        }
    }

    /** [sn_leave_multicast_group] */
    /* leave multicast group */
    for (i = 0; i < group_count; ++i)
    {
        errorCode = SC_IgmpLeave(udpChannelId, udpLane, groups[i].ip, groups[i].port, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Failed to leave a multicast group, error code %d\n", errorCode);
            rc = EXIT_FAILURE;
            goto exit;
        }
    }
    /** [sn_leave_multicast_group] */

    errorCode = SC_Disconnect(tcpChannelId);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Cannot close connection!\n");
        rc = EXIT_FAILURE;
    }

exit:

    /** [sn_deallocate_contexts] */
    /* Deallocate contexts */
    SC_CloseDevice(deviceId);
    /** [sn_deallocate_contexts] */

    printf("Closed device\n");

    return rc;
}
