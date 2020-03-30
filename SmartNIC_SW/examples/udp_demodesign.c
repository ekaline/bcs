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
#include <unistd.h>
#include <time.h>

#include "smartnic.h"

static volatile bool stop_cond = false;

static SC_Error ErrorHandler(const char * fileName, int lineNumber, const char * functionName, SC_Error errorCode, const char * errorMessage, void * pUserContext)
{
    fprintf(stderr, "%s at %s#%d in %s context %p\n", errorMessage, fileName, lineNumber, functionName, pUserContext);

    return errorCode;
}


/* Signal handler */
void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        if (stop_cond)
        {
            exit(EXIT_FAILURE);
        }
        stop_cond = true;
    }
}

static void help(FILE * pStream, const char * defaultRemoteHost, uint16_t defaultRemotePort, const char * defaultGroup)
{
    fprintf(pStream,
        "UDP demo design\n"
        "\n"
        "Usage: udp_demodesign -h|--help\n"
        "       udp_demodesign [-d|--device <device-name>] [-c/--host <remote-host>] [-p/--port <remote-port>]\n"
        "                      [-g/--group <multicast-group>] [--toelane <toe-lane>] [--toechannel <toe-channel-number>]\n"
        "                      [--udplane <udp-lane>] [--udpchannel <udp-channel-number>] [--vlan <vlan-tag>]\n"
        "\n");
    fprintf(pStream,
        "       where\n"
        "\n"
        "       -d/--device     SmartNIC device name. Default name is /dev/smartnic.\n"
        "       -c/--host       Remote host to connect to. Default is %s.\n"
        "       -p/--port       Remote port to connect to. Default is %u.\n"
        "       -g/--group      Multicast group to join. Default is %s.\n", defaultRemoteHost, defaultRemotePort, defaultGroup);
    fprintf(pStream,
            "       --toelane       TOE lane to use. Default TOE lane is 0.\n"
        "       --toechannel    TOE channel number in range 0-63 or -1. Default TOE channel is -1 (driver assigns the number).\n"
        "       --udplane       UDP lane to use. Default UDP lane is 1.\n"
        "       --udpchannel    UDP channel number in range 0-31 or -1. Default UDP channel is -1 (driver assigns the number).\n"
        "       --vlan          VLAN tag value 1-4094 or -1 for no VLAN. Default is -1.\n"
        "       -h/--help       This help\n");
}

int main(int argc, char * argv[])
{
    int rc = 0;
    SC_DeviceId deviceId;
    SC_ChannelId udpChannelId, tcpChannelId;
    SC_ChannelOptions udpChannelOptions;
    const char * deviceName = NULL;
    const char * toeLaneArg = NULL;
    const char * udpLaneArg = NULL;
    uint8_t toeLane = 0;
    int16_t toeChannel = -1;
    uint8_t udpLane = 1;
    int16_t udpChannel = -1;
    int16_t vlanTag = -1;
    const char * const DefaultRemoteHost = "172.16.10.1";
    const char * remoteHost = DefaultRemoteHost;
    const char * const DefaultGroup = "239.2.0.2";
    const uint16_t DefaultRemotePort = 2000;
    uint16_t remotePort = DefaultRemotePort;
    const char * group1 = DefaultGroup;
    SC_Error errorCode;
    SC_ConnectOptions connectOptions;

    /* Register signal handler */
    if (signal(SIGINT, &sig_handler) == SIG_ERR)
    {
        fprintf(stderr, "Cannot register signal handler\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN);

    errorCode = SC_RegisterErrorHandler(ErrorHandler, NULL);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Cannot register error handler, error code %d\n", errorCode);
        exit(EXIT_FAILURE);
    }

    /* Process options */
    while (true)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            { "device",     required_argument,  0, 'd' },
            { "host",       required_argument,  0, 'c' },
            { "port",       required_argument,  0, 'p' },
            { "group",      required_argument,  0, 'g' },
            { "help",       no_argument,        0, 'h' },
            { "toelane",    required_argument,  0, 0 },
            { "udplane",    required_argument,  0, 0 },
            { "toechannel", required_argument,  0, 0 },
            { "udpchannel", required_argument,  0, 0 },
            { "vlan",       required_argument,  0, 0 },
            { 0,            0,                  0, 0 }
        };

        int option_chr = getopt_long(argc, argv, "d:c:p:g:h", long_options, &option_index);

        if (option_chr == -1)
            break;

        switch (option_chr)
        {
            case 0:
                {
                    const char * name = long_options[option_index].name;
                    if (strcmp(name, "toelane") == 0)
                    {
                        toeLaneArg = optarg;
                        break;
                    }
                    else if (strcmp(name, "udplane") == 0)
                    {
                        udpLaneArg = optarg;
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

            case 'c':
                remoteHost = optarg;
                break;

            case 'p':
                {
                    uint64_t port = (uint64_t)atol(optarg);
                    if (port > 65535)
                    {
                        fprintf(stderr, "Remote port %s has to be between 0 and 65535\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                    remotePort = (uint16_t)port;
                }
                break;

            case 'g':
                group1 = optarg;
                break;

            case 'h':
                help(stdout, DefaultRemoteHost, DefaultRemotePort, DefaultGroup);
                exit(EXIT_SUCCESS);

            default:
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Error: Non-option elements in argument list: ");
        while (optind < argc)
        {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n\n");
        exit(EXIT_FAILURE);
    }

    /** [sn_init_device_context] */
    deviceId = SC_OpenDevice(deviceName, NULL);
    if (deviceId == NULL)
    {
        fprintf(stderr, "Cannot initialize device context for device '%s'. Is driver loaded?\n", deviceName == NULL ? "/dev/smartnic" : deviceName);
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

    SC_ActivateDemoDesign(deviceId, SC_DEMO_USER_LOGIC_GENERIC, 0x100, NULL);

    /** [sn_allocate_udp_channel_call] */
    /* Allocate UDP channel */
    /* Udp channel is used for the demo design - the demo design takes care of incomming packet so we don't need tom in user memory */
    errorCode = SC_GetChannelOptions(NULL, &udpChannelOptions);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to get channel default options, error code %d\n", errorCode);
        exit(EXIT_FAILURE);
    }
    udpChannelOptions.UseTxDmaNormalFifoFirmwareFillLevel = true;
    udpChannelOptions.DoNotUseRxDMA = true;
    udpChannelId = SC_AllocateUdpChannel(deviceId, udpLane, udpChannel, /*SC_ChannelOptions *:*/ &udpChannelOptions);
    if (udpChannelId == NULL)
    {
        fprintf(stderr, "Cannot allocate UDP channel, error code %d\n", SC_GetLastErrorCode());
        rc = EXIT_FAILURE;
        goto exit;
    }
    /** [sn_allocate_udp_channel_call] */

    /** [sn_allocate_tcp_channel_call] */
    /* Allocate TCP channel */
    tcpChannelId = SC_AllocateTcpChannel(deviceId, toeLane, toeChannel, /*SC_ChannelOptions *:*/ NULL);
    if (tcpChannelId == NULL)
    {
        fprintf(stderr, "Cannot allocate TCP channel, error code %d\n", SC_GetLastErrorCode());
        rc = EXIT_FAILURE;
        goto exit;
    }
    /** [sn_allocate_tcp_channel_call] */

    if ((errorCode = SC_GetConnectOptions(tcpChannelId, &connectOptions)) != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to get default connect options, error coce %d\n", errorCode);
        rc = EXIT_FAILURE;
        goto exit;
    }
    connectOptions.VLanTag = vlanTag;

    /** [sn_connect_call] */
    /* Connect to server at remoteHostt:remotePort */
    if (SC_Connect(tcpChannelId, remoteHost, remotePort, /*SC_ConnectOptions *:*/ NULL) != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Cannot open connection to remote host %s port %u\n", remoteHost, remotePort);
        rc = EXIT_FAILURE;
        goto exit;
    }
    /** [sn_connect_call] */

    printf("Connection is established\n");

    /** [sn_join_multicast_group] */
    /* Join multicast group */
    SC_IgmpJoin(udpChannelId, udpLane, group1, 0, NULL);
    /** [sn_join_multicast_group] */

    while (!stop_cond)
    {
        usleep(50000);
    }

    /** [sn_leave_multicast_group] */
    /* leave multicast group */
    SC_IgmpLeave(udpChannelId, udpLane, group1, 0, NULL);
    /** [sn_leave_multicast_group] */

    SC_ActivateDemoDesign(deviceId, SC_DEMO_USER_LOGIC_GENERIC, 0, NULL);

exit:

    /** [sn_deallocate_contexts] */
    /* Deallocate contexts */
    SC_CloseDevice(deviceId);
    /** [sn_deallocate_contexts] */

    return rc;
}
