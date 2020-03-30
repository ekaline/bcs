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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include PRODUCT_H
#include PRODUCT_TOOLS_H

/** Print help text
 */
static void help_text(FILE * pStream)
{
    fprintf(pStream,
        "Usage:\n\n");
    fprintf(pStream,
        "fbconfig\n"
        "         Show list of current settings\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -I<network-interface> -i<IP-address> -n<net mask> [-g<gateway IP address>]\n"
        "         [-m<MAC address>]\n\n");
    fprintf(pStream,
        "fbconfig [--device <device-name>] --nif <network-interface> --ip <IP-address> --netmask <net mask>\n"
        "         [--gateway <gateway IP address>] [--mac <MAC address>]\n"
        "         Set IP, netmask and optionally gateway and/or MAC address for\n"
        "         the network card\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -s | --status\n"
        "         Show various status bits: link status, SFP status, etc.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -S | --xstatus\n"
        "         Show various status bits with extra state info: link status, SFP status, etc.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -t | --temp\n"
        "         Show minimum, current and maximum FPGA temperatures in Celcius separated by tab characters and exit.\n"
        "         Useful for being called in a loop to monitor FPGA temperatures.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -k | --config\n"
        "         Show device configuration masks for Std NIC etc.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -K | --decoded-config\n"
        "         Decode device configuration masks for Std NIC etc. into a table\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -P | --pciname\n"
        "         Show PCIe device name for this device.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] --resetfpga\n"
        "         Reset the device FPGA.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -r | --rlogbits\n"
        "         Read driver log mask.\n\n");
    fprintf(pStream,
        "fbconfig [-d <device-name>] -w <logmask> | --rlogbits <logmask>\n"
        "         Write driver log mask.\n\n");
    fprintf(pStream,
        "fbconfig -h | --help\n"
        "         Show this text\n\n");
    fprintf(pStream,
        "fbconfig -V\n"
        "         Show software/API library and device driver version.\n\n");
    fprintf(pStream,
        "Where:\n"
        "\n"
        "    -d/--device   %s device name. Default name is /dev/%s.\n"
        "    -I/--nif      Network interface number in physical port.\n"
        "    -i/--ip       IP address for network card.\n"
        "    -n/--netmask  Net mask.\n"
        "    -g/--gateway  Network gateway.\n"
        "    -m/--mac      Custom MAC address (optional).\n"
        "\n"
        "    IP addresses and the net mask are in IPv4 format.\n"
        "    The MAC address must be formatted as xx:xx:xx:xx:xx:xx where 'x' is a\n"
        "    hexadecimal digit.\n"
        "    Argument order is significant!\n\n", PRODUCT, LOWER_PRODUCT);
}

static uint64_t strtoi64(const char * argument)
{
    uint64_t value = 0;
    size_t length = strlen(argument);
    if (length >= 3 && argument[0] == '0' && argument[1] == 'x')
    {
        char c;

        argument += 2;
        while ((c = *argument++) != '\0')
        {
            if (!isxdigit(c))
            {
                fprintf(stderr, "Error: hexadecimal digits expected instead of '%c'", c);
                exit(EXIT_FAILURE);
            }
            if (c >= 'A' && c <= 'F')
            {
                value = 16 * value + (uint64_t)(10 + c - 'A');
            }
            else if (c >= 'a' && c <= 'f')
            {
                value = 16 * value + (uint64_t)(10 + c - 'a');
            }
            else /* digits 0-9 */
            {
                value = 16 * value + (uint64_t)(c - '0');
            }
        }
    }
    else
    {
        char c;

        while ((c = *argument++) != '\0')
        {
            if (!isdigit(c))
            {
                fprintf(stderr, "Error: decimal digits expected instead of '%c'", c);
                exit(EXIT_FAILURE);
            }
            value = 10 * value + (uint64_t)(c - '0');
        }
    }
    return value;
}

/**
 *  Open device
 */
static SC_DeviceId OpenDevice(const char * deviceName)
{
    SC_DeviceId deviceId = SC_OpenDevice(deviceName, NULL);
    if (deviceId == NULL)
    {
        fprintf(stderr, "Failed to open device %s\n", deviceName == NULL ? "/dev/" LOWER_PRODUCT : deviceName);
        exit(EXIT_FAILURE);
    }
    return deviceId;
}

/**
 *  Cleanup by closing device context.
 */
static void CloseDevice(SC_DeviceId deviceId)
{
    if (deviceId != NULL)
    {
        SC_Error errorCode = SC_CloseDevice(deviceId);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Error: Unable to close device.\n");
            exit(EXIT_FAILURE);
        }
    }
}

static const char * Truth(bool b)
{
    return b ? "Y" : "-";
}

/**
 *  Round float to unsigned integer.
 */
static unsigned Round(float x)
{
    return (unsigned)floor(x + 0.5);
}

static SC_Error ErrorHandler(const char * fileName, int lineNumber, const char * functionName, SC_Error errorCode, const char * errorMessage, void * pUserContext)
{
    UNUSED_ALWAYS(pUserContext);

    fprintf(stderr, "%s library error at %s#%d function %s: error code %d, %s\n", PRODUCT, fileName, lineNumber, functionName, errorCode, errorMessage);
    return SC_ERR_HANDLER_CONTINUE;
}

/** Main entry point and where the action is.
 *
 * @param argc Number of arguments
 * @param argv Table of arguments
 *
 * @return int 0 on success, non-zero otherwise.
 */
int main(int argc, char * argv[])
{
    int rc = EXIT_SUCCESS;

    struct in_addr      ip = { 0 };
    struct in_addr      netmask = { 0 };
    struct in_addr      gateway = { 0 };
    uint8_t             mac[MAC_ADDR_LEN] = { 0 };
    const char *        deviceName = NULL;
    SC_DeviceId         deviceId = NULL;
    SC_Error            errorCode;
    float               min_temp, cur_temp, max_temp;
    bool                showTcpState = false;
    const char *        networkInterfaceArg = NULL;
    uint8_t             useNetworkInterface = 0;
    SC_CardInfo         cardInfo;
    int                 verbosity = 0;

    memset(&ip,      0, sizeof(struct in_addr));
    memset(&netmask, 0, sizeof(struct in_addr));
    memset(&gateway, 0, sizeof(struct in_addr));

    SC_RegisterErrorHandler(ErrorHandler, NULL);

    while (true)
    {
        int option_index = 0;
        static struct option long_options[]=
        {
            { "device",     required_argument, 0, 'd' },
            { "port",       required_argument, 0, 'p' },
            { "lane",       required_argument, 0, 'l' }, /* Backward compatibility, deprecated */
            { "nif",        required_argument, 0, 'I' },
            { "ip",         required_argument, 0, 'i' },
            { "netmask",    required_argument, 0, 'n' },
            { "gateway",    required_argument, 0, 'g' },
            { "mac",        required_argument, 0, 'm' },
            { "temp",       no_argument      , 0, 't' },
            { "status",     no_argument      , 0, 's' },
            { "xstatus",    no_argument      , 0, 'S' },
            { "help",       no_argument      , 0, 'h' },
            { "rlogbits",   no_argument      , 0, 'r' },
            { "wlogbits",   required_argument, 0, 'w' },
            { "config",     no_argument      , 0, 'k' },
            { "decoded-config", no_argument  , 0, 'K' },
            { "pciname",    no_argument      , 0, 'P' },
            { "resetfpga",  no_argument      , 0, 0   },
            { "version",    no_argument      , 0, 'V' },
            { "verbosity",  required_argument, 0, 'v' },
            { 0,            0                , 0, 0   }
        };

        int option_chr = getopt_long(argc, argv, "d:p:l:I:i:n:g:m:w:v:sStkKpPhrV", long_options, &option_index);

        if (option_chr == -1)
            break;

        switch (option_chr)
        {
        case 0:
            {
                const char * name = long_options[option_index].name;
                if (strcmp(name, "resetfpga") == 0)
                {
                    deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                    errorCode = SC_ResetFPGA(deviceId);
                    CloseDevice(deviceId);
                    if (errorCode != SC_ERR_SUCCESS)
                    {
                        fprintf(stderr, "Failed to reset the FPGA, error code %d, errno %d", errorCode, errno);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        fprintf(stdout, "FPGA successfully reset\n");
                        exit(EXIT_SUCCESS);
                    }
                }
                else
                {
                    printf("option %s", name);
                    if (optarg != NULL)
                    {
                        printf(" with arg %s", optarg);
                    }
                    printf("\n");
                }
            }
            break;

        case 'd':
            if (networkInterfaceArg != NULL)
            {
                fprintf(stderr, "Please specify device name before -I/--nif option.\n");
                exit(EXIT_FAILURE);
            }
            deviceName = optarg;
            break;

        case 'p':
            fprintf(stderr, "-p/--port option is deprecated, use -I/--nif instead\n");
            /* fall through */

        case 'l':
        case 'I':
            {
                networkInterfaceArg = optarg;
                /* Sanity check of TOE and UDP network interface parameters */
                if (networkInterfaceArg != NULL)
                {
                    uint64_t networkInterface = (uint64_t)atol(networkInterfaceArg);
                    deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                    if (networkInterface > 255 || !SC_IsTcpNetworkInterface(deviceId, (uint8_t)networkInterface))
                    {
                        fprintf(stderr, "Network interface %s is not configured to have a TOE!\n", networkInterfaceArg);
                        rc = EXIT_FAILURE;
                        goto exit;
                    }
                    useNetworkInterface = (uint8_t)networkInterface;
                    CloseDevice(deviceId);
                    deviceId = NULL;
                }
            }
            break;

        case 'i':
            inet_aton(optarg, &ip);
            break;

        case 'n':
            inet_aton(optarg, &netmask);
            break;

        case 'g':
            inet_aton(optarg, &gateway);
            break;

        case 'm':
            {
                unsigned int bytes[MAC_ADDR_LEN];
                unsigned int i = 0;
                if (MAC_ADDR_LEN != sscanf(optarg, "%x:%x:%x:%x:%x:%x",
                                &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]))
                {
                    fprintf(stderr, "Error: Cannot parse MAC address\n");
                    exit(EXIT_FAILURE);
                }
                for (i = 0; i < MAC_ADDR_LEN; i++)
                {
                    if (bytes[i] > 0xff)
                    {
                        fprintf(stderr, "Error: MAC address parser error\n");
                        exit(EXIT_FAILURE);
                    }
                    mac[i] = (unsigned char)bytes[i];
                }
            }
            break;

        case 'S':
            showTcpState = true;
            /* fall through */

        case 's':
            {
                int16_t i;
                SC_StatusInfo statusInfo;

                deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;

                rc = SC_GetStatusInfo(deviceId, &statusInfo);
                if (rc != SC_ERR_SUCCESS)
                {
                    fprintf(stderr, "Failed to get status info, error code %d\n", rc);
                }

                printf("\n");

                printf("  nif #  | link | link lost | sfp | sfp removed |\n");
                printf(" --------|------|-----------|-----|-------------|\n");
                for (i = 0; i < statusInfo.NumberOfLinks; i++)
                {
                    const SC_LinkStatus *s = &statusInfo.LinkStatus[i];
                    printf("  nif %2d |   %s  |     %s     |  %s  |      %s      |\n", i,
                        Truth(s->Link), Truth(s->LostLink),
                        Truth(s->SFP_Present), Truth(s->SFP_Removed));
                }
                printf("\n");
                printf("PPS present    %s |  PPS missing     %s\n",
                    Truth(statusInfo.PPS_Present), Truth(statusInfo.PPS_Missing));
                /*printf( "fifo overflow  %s |  stats overflow  %s\n", */
                /*        TRUTH(statusInfo.FifoOverflow), TRUTH(statusInfo.StatisticsOverflow) ); */
                /*printf("Packet DMA overflow:  %6d\n", statusInfo.PacketDmaOverflow);*/
                /*printf( "fifo filling       :  %6d\n", statusInfo.FifoFilling ); */
                printf("\n");
                printf(" Channel # | Filling | Peak filling | Channel usage\n");
                printf(" ----------|---------|--------------|-----------------------\n");
                for (i = 0; i < statusInfo.NumberOfRxDmaFillings; ++i)
                {
                    SC_ChannelType channelType = SCI_GetChannelType(deviceId, (uint16_t)i);

                    const SC_FillingLevels * f = &statusInfo.RxDmaFillings[i];

                    switch (channelType)
                    {
                        case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
                        case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
                        case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
                        case SC_DMA_CHANNEL_TYPE_TCP:
                            {
                                unsigned percentage = Round(f->FillingPercentage);
                                unsigned peakPercentage = Round(f->PeakFillingPercentage);
                                printf("  DMA %3d  | %6u%% | %11u%% |", i, percentage, peakPercentage);
                            }
                            break;
                        default:
                            break;
                    }

                    switch (channelType)
                    {
                        case SC_DMA_CHANNEL_TYPE_NONE:
                        case SC_DMA_CHANNEL_TYPE_UNUSED:
                        case SC_DMA_CHANNEL_TYPE_USER_LOGIC:
                            printf(" - User Logic Channel %d\n", i);
                            break;
                        case SC_DMA_CHANNEL_TYPE_STANDARD_NIC:
                            printf(" - Std NIC port %d\n", i - 8);
                            break;
                        case SC_DMA_CHANNEL_TYPE_UDP_MULTICAST:
                            printf(" - UDP Multicast stream %d\n", i - 32);
                            break;
                        case SC_DMA_CHANNEL_TYPE_TCP:
                            {
                                uint16_t toeChannel = (uint16_t)(i - 64);
                                printf(" - TCP Connection %u", toeChannel);
                                if (showTcpState)
                                {
                                    const char * tcpStateString = "*** UNKNOWN ***";

                                    const char * filler = NULL;
                                    if (toeChannel >= 100)
                                    {
                                        filler = "";
                                    }
                                    else if (toeChannel >= 10)
                                    {
                                        filler = " ";
                                    }
                                    else
                                    {
                                        filler = "  ";
                                    }

                                    if (i - 64 < statusInfo.NumberOfTcpChannels)
                                    {
                                        SC_TcpState tcpState = SCI_GetTcpState(deviceId, toeChannel);
                                        if ((int)tcpState >= 0)
                                        {
                                            tcpStateString = SC_GetTcpStateAsString(tcpState);
                                        }
                                        else
                                        {
                                            char errorMessage[100];
                                            snprintf(errorMessage, sizeof(errorMessage), "error code %d", (int)tcpState);
                                            tcpStateString = errorMessage;
                                        }
                                        printf("%s  TCP state %s\n", filler, tcpStateString);
                                    }
                                    else
                                    {
                                        printf("%s  Not available\n", filler);
                                    }
                                }
                                else
                                {
                                    printf("\n");
                                }
                            }
                            break;
                        default:
                            fprintf(stderr, "\n *** ERROR: unknown FPGA DMA channel type %d\n", channelType);
                            break;
                    }
                }
                CloseDevice(deviceId);
                exit(EXIT_SUCCESS);
            }
            break;

        case 't':
            /* Attempt to open device */
            deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
            SC_GetFpgaTemperature(deviceId, &min_temp, &cur_temp, &max_temp);
            printf( "%.1f\t%.1f\t%.1f\n", min_temp, cur_temp, max_temp );

            /* Clean up */
            CloseDevice(deviceId);
            /* No more to do here */
            exit( EXIT_SUCCESS );

        case 'k':
            {
                deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                errorCode = SC_GetCardInfo(deviceId, &cardInfo);
                if (errorCode != SC_ERR_SUCCESS)
                {
                    fprintf(stderr, "SC_GetCardInfo failed with error code %d\n", errorCode);
                    rc = EXIT_FAILURE;
                    goto exit;
                }
                printf("OOB network interfaces mask 0x%04x\n"
                    "TCP network interfaces mask 0x%04x\n"
                    "UDP network interfaces mask 0x%04x\n"
                    "MAC network interfaces mask 0x%04x\n"
                    "ACL network interfaces mask 0x%04x\n"
                    "QSFP ports mask 0x%02x\n"
                    "Evaluation mask 0x%04x\n"
                    "TOE is connected to user logic: %s\n"
                    "Statistics module is present : %s\n",
                    cardInfo.OobNetworkInterfacesEnabledMask,
                    cardInfo.TcpNetworkInterfacesEnabledMask,
                    cardInfo.UdpNetworkInterfacesEnabledMask,
                    cardInfo.MacNetworkInterfacesEnabledMask,
                    cardInfo.AclNetworkInterfacesEnabledMask,
                    cardInfo.QsfpPortsEnabledMask,
                    cardInfo.EvaluationEnabledMask,
                    cardInfo.IsToeConnectedToUserLogic ? "yes" : "no",
                    cardInfo.StatisticsEnabled ? "yes" : "no");
                /* Clean up */
                goto exit;
            }
            break;

        case 'K':
            {
                int i;
                deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                errorCode = SC_GetCardInfo(deviceId, &cardInfo);
                if (errorCode != SC_ERR_SUCCESS)
                {
                    fprintf(stderr, "SC_GetCardInfo failed with error code %d\n", errorCode);
                    CloseDevice(deviceId);
                    exit(EXIT_FAILURE);
                }
                printf("  nif  # |  Std NIC  ");
                printf("|  UDP  |  TCP  ");
                printf("| MAC IF ena | \n");
                printf(" --------|-----------|-------|-------|------------|\n");
                for (i = 0; i < cardInfo.NumberOfNetworkInterfaces; ++i)
                {
                    int stdnic_present = (cardInfo.OobNetworkInterfacesEnabledMask & 1 << i) == 0 ? 0 : 1;
                    int tcp_present    = (cardInfo.TcpNetworkInterfacesEnabledMask & 1 << i) == 0 ? 0 : 1;
                    int udp_present    = (cardInfo.UdpNetworkInterfacesEnabledMask & 1 << i) == 0 ? 0 : 1;
                    int macif_present  = (cardInfo.MacNetworkInterfacesEnabledMask & 1 << i) == 0 ? 0 : 1;

                    printf("  nif %2d |     %s     ", i, Truth(stdnic_present));
                    printf("| %s | %s ", Truth(udp_present), Truth(tcp_present));
                    printf("| %s | \n", Truth(macif_present));
                }
                printf("\n");
                /* Clean up */
                CloseDevice(deviceId);
            }
            /* No more to do here */
            exit(EXIT_SUCCESS);

        case 'P':
            {
                deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                errorCode = SC_GetCardInfo(deviceId, &cardInfo);
                if (errorCode != SC_ERR_SUCCESS)
                {
                    fprintf(stderr, "SC_GetCardInfo failed with error code %d\n", errorCode);
                    CloseDevice(deviceId);
                    exit(EXIT_FAILURE);
                }
                printf("PCIe device name: %s\n", cardInfo.PciDeviceName);
                /* Clean up */
                CloseDevice(deviceId);
            }
            /* No more to do here */
            exit(EXIT_SUCCESS);

        case 'h':
            help_text(stdout);
            /* No more to do here */
            exit(EXIT_SUCCESS);

        case 'V':
            deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
            SC_PrintVersions(deviceId, stdout, "fbconfig");
            /* Clean up */
            CloseDevice(deviceId);
            /* No more to do here */
            exit(EXIT_SUCCESS);

        case 'v':
            verbosity = atoi(optarg);
            break;

        case 'r':
            {
                uint64_t logMask;

                deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                errorCode = SC_GetDriverLogMask(deviceId, &logMask);
                if (errorCode != SC_ERR_SUCCESS)
                {
                    fprintf(stderr, "Error: Unable to read log flags.\n");
                    CloseDevice(deviceId);
                    errorCode = EXIT_FAILURE;
                }
                fprintf(stdout, "0x%lX\n", logMask);
                /* Clean up */
                CloseDevice(deviceId);
                /* No more to do here */
                exit(errorCode);
            }

        case 'w':
            {
                uint64_t logMask = strtoi64(optarg);

                deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;
                errorCode = SC_SetDriverLogMask(deviceId, logMask);
                if (errorCode != SC_ERR_SUCCESS)
                {
                    fprintf(stderr, "Error: Unable to write log flags.\n");
                    CloseDevice(deviceId);
                    errorCode = EXIT_FAILURE;
                }
                fprintf(stdout, "0x%lX\n", logMask);
                /* Clean up */
                CloseDevice(deviceId);
                /* No more to do here */
                exit(errorCode);
            }

        default:
            /* Return error code for unrecognized options */
            fprintf(stderr, "Unknown option '%s'\n", argv[option_index]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind < argc)
    {
        fprintf( stderr, "Error: Non-option elements in argument list: ");
        while (optind <argc) {
            fprintf( stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n\n");
        exit(EXIT_FAILURE);
    }

    if (argc > (deviceName == NULL ? 1 : 3))
    {
        /* Check that all mandatory parameters are present */
        if (!ip.s_addr)
        {
            fprintf( stderr, "Error: IP address (option --ip) must be supplied.\n" );
            exit( EXIT_FAILURE );
        }
#ifdef THIS_IS_NOT_MANDATORY
        if( !netmask.s_addr )
        {
            fprintf( stderr, "Error: Net mask (option --netmask) must be supplied.\n" );
            exit( EXIT_FAILURE );
        }
        if( !gateway.s_addr )
        {
            fprintf( stderr, "Error: Gateway address (option --gateway) must be supplied.\n" );
            exit( EXIT_FAILURE );
        }
#endif
    }

    /* Attempt to open device */
    deviceId = deviceId == NULL ? OpenDevice(deviceName) : deviceId;

    errorCode = SC_GetCardInfo(deviceId, &cardInfo);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to read card info with error code %d\n", errorCode);
        rc = EXIT_FAILURE;
        goto exit;
    }

    /* Just show network interface parameters? */
    if (argc == (deviceName == NULL ? 1 : 3))
    {
        uint64_t linkStatus = 0;
        uint8_t networkInterface;

        if (SC_GetLinkStatus(deviceId, &linkStatus))
        {
            fprintf(stderr, "Cannot access link status - continue anyway\n");
        }

        printf("\n");
        printf("FPGA version: %u.%u.%u.%u Type: 0x%02x Model: 0x%02x\n", cardInfo.FpgaInfo.Major,
                cardInfo.FpgaInfo.Minor, cardInfo.FpgaInfo.Revision, cardInfo.FpgaInfo.Build,
                cardInfo.FpgaInfo.ImageType, cardInfo.FpgaInfo.Model);

        printf("\n");
        for (networkInterface = 0; networkInterface < cardInfo.NumberOfNetworkInterfaces; ++networkInterface)
        {
            bool stdnic_present = SC_IsOobNetworkInterface(deviceId, networkInterface);
            bool tcp_present    = SC_IsTcpNetworkInterface(deviceId, networkInterface);
            bool udp_present    = SC_IsUdpNetworkInterface(deviceId, networkInterface);

            if (SC_GetLocalAddress(deviceId, networkInterface, &ip, &netmask, &gateway, mac) == SC_ERR_SUCCESS)
            {
                printf("Settings for network interface %u:\n", networkInterface);
                printf("    feature: ");
                if (tcp_present) printf("TOE, ");
                if (udp_present) printf("UDP, ");
                if (stdnic_present) printf("Std NIC.");
                if (!tcp_present && !stdnic_present)
                {
                    printf(" N/A.\n");
                }
                else
                {
                    printf("\n");
                    if (tcp_present)
                    {
                        printf("    ip:      %s\n", inet_ntoa(ip));
                    }
                }
                /* we do not need these? printf("    netmask: %s\n", inet_ntoa(netmask)); */
                /* we do not need these? printf("    gateway: %s\n", inet_ntoa(gateway)); */
                printf("    mac:     %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                printf("    link:    %s\n", (linkStatus & ( (uint64_t)1 << (networkInterface * 8))) ? "Yes" : "No");
            }
            else
            {
                fprintf( stderr, "Error: Unable to get IP data from device.\n" );
                rc = EXIT_FAILURE;
                goto exit;
            }
        }
    }
    else
    {
        /* Use custom MAC address? */
        if (mac[0] || mac[1] || mac[2] || mac[3] || mac[4] || mac[5])
        {
            errorCode = SC_SetLocalAddress(deviceId, useNetworkInterface, ip, netmask, gateway, mac);
            if (verbosity > 0)
            {
                printf("Setting MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }
        }
        else
        {
            errorCode = SC_SetLocalAddress(deviceId, useNetworkInterface, ip, netmask, gateway, NULL);
            if (verbosity > 0)
            {
                printf("Setting MAC address: 00:00:00:00:00:00\n");
            }
        }
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf( stderr, "Error: Unable to configure device. Error code %d\n", errorCode);
            rc = EXIT_FAILURE;
            goto exit;
        }
    }

exit:

    /* Clean up */
    CloseDevice(deviceId);

    return rc;
}

