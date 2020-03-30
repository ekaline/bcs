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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "smartnic.h"

static const int ip_out_sz = 31;

typedef int BOOL;

static void help_text(FILE * pStream, const char * pn)
{
    fprintf(pStream,
        "Usage:\n" );
    fprintf(pStream,
        "    %s -l[f] <lane>\n"
        "        List current content of the ACL tables attached to a specific lane\n"
        "        Optional [f] lists also empty entries.\n\n", pn);
    fprintf(pStream,
        "    %s -r <lane> [<Rx/Tx/RxTx>] [<index>]\n"
        "        Reset all ACL, or all ACL for a lane or a specific entry for a specific lane in specific ACL.\n\n", pn);
    fprintf(pStream,
        "    %s -m[f] <lane> <Rx/Tx/RxTx> <index> <ip> <ip mask> <min port> <max port>\n"
        "        Modify/set a specific ACL table for a specific lane, to allow/deny <ip> masked with <ip mask>\n"
        "        from ports <min port> to <max port> (both inclusive).\n"
        "        Optional [f] forces the entry \n\n", pn);
    fprintf(pStream,
        "    %s -e <lane>\n"
        "        Enable ACL tables attached to a specific lane\n\n", pn);
    fprintf(pStream,
        "    %s -d <lane>\n"
        "        Disable ACL tables attached to a specific lane\n\n", pn);
    fprintf(pStream,
        "    %s -V\n"
        "        Show software/API library and device driver versions\n", pn);
}

static int my_assert( int truth, const char *errmsg )
{
    if( ! truth ) {
        fprintf( stderr, "%s\n", errmsg );
        return -1;
    }
    return 0;
}

#if 0
static BOOL validate_ip_mask( uint32_t ip_mask )
{
    /* Validate IP mask to be in format 11111...00000, a normal limitation for IP net masks */
    uint32_t invertedMask = ~ip_mask;
    return ((invertedMask + 1) & invertedMask) == 0;
}
#endif

static BOOL validate_ip_address_and_mask( uint32_t ip,  uint32_t ip_mask )
{
    /* Validate that IP address has 0 bits in same positions as the mask. */
    /* This quarantees that the mask will trigger with some IP addresses, */
    /* otherwise it will never trigger. */
    return (ip & ip_mask) == ip;
}

static void printf_ip( uint32_t ip, uint32_t ip_mask )
{
    int c = printf( "%d.%d.%d.%d", (uint8_t)(ip >> 24), (uint8_t)(ip >> 16), (uint8_t)(ip >> 8), (uint8_t)(ip >> 0));
    c += printf( "/%d.%d.%d.%d", (uint8_t)(ip_mask >> 24), (uint8_t)(ip_mask >> 16), (uint8_t)(ip_mask >> 8), (uint8_t)(ip_mask >> 0));
    while( c++ < ip_out_sz )
        printf( " ");
}


static int parse_int_base10( int argc, char *argv[], int argn, int *value )
{
    char *endptr;
    if( argn >= argc ) {
        fprintf( stderr, "missing argument: argument %d is needed, only %d provided\n", argn, argc-1 );
        return -1;
    }
    *value = (int)strtol( argv[argn], &endptr, 10/*base*/ );
    if( *endptr ) { /* If not all string has been parsed */
        fprintf( stderr, "argument '%s' contains unexpected character: '%c'\n", argv[argn], *endptr );
        return -1;
    }
    return 0;
}

static int parse_ip_base10( int argc, char *argv[], int argn, uint32_t *value )
{
    char *endptr;
    int byte1, byte2, byte3, byte4;
    if( argn >= argc ) {
        fprintf( stderr, "missing argument: argument %d is needed, only %d provided\n", argn, argc-1 );
        return -1;
    }

    byte1 = (int)strtol( argv[argn], &endptr, 10/*base*/ );
    if( *endptr != '.' ) { /* If not all string has been parsed */
        fprintf( stderr, "argument '%s' contains unexpected character: '%c' (0x%02x)\n", argv[argn], *endptr, (unsigned char)*endptr );
        return -1;
    }
    if( my_assert( byte1 >= 0 && byte1 <= 0xff, "1st byte of IP address out of range!" ) )
        return -1;

    byte2 = (int)strtol( endptr+1, &endptr, 10/*base*/ );
    if( *endptr != '.' ) { /* If not all string has been parsed */
        fprintf( stderr, "argument '%s' contains unexpected character: '%c' (0x%02x)\n", argv[argn], *endptr, (unsigned char)*endptr );
        return -1;
    }
    if( my_assert( byte2 >= 0 && byte2 <= 0xff, "2nd byte of IP address out of range!" ) )
        return -1;

    byte3 = (int)strtol( endptr+1, &endptr, 10/*base*/ );
    if( *endptr != '.' ) { /* If not all string has been parsed */
        fprintf( stderr, "argument '%s' contains unexpected character: '%c' (0x%02x)\n", argv[argn], *endptr, (unsigned char)*endptr );
        return -1;
    }
    if( my_assert( byte3 >= 0 && byte3 <= 0xff, "3rd byte of IP address out of range!" ) )
        return -1;

    byte4 = (int)strtol( endptr+1, &endptr, 10/*base*/ );
    if( *endptr != '\0' ) { /* If not all string has been parsed */
        fprintf( stderr, "argument '%s' contains unexpected character: '%c' (0x%02x)\n", argv[argn], *endptr, (unsigned char)*endptr );
        return -1;
    }
    if( my_assert( byte4 >= 0 && byte4 <= 0xff, "4th byte of IP address out of range!" ) )
        return -1;

    *value = (uint32_t)((byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4);
    return 0;
}

static char * ToLower(char * string)
{
    char * originalString = string;
    for (; *string; ++string)
    {
        *string = (char)tolower(*string);
    }
    return originalString;
}

static int ParseAclId(char * aclIdString)
{
    aclIdString = ToLower(aclIdString);
    if (strcmp(aclIdString, "rx") == 0)
    {
        return ACL_ID_RX;
    }
    else if (strcmp(aclIdString, "tx") == 0)
    {
        return ACL_ID_TX;
    }
    if (strcmp(aclIdString, "rxtx") == 0 || strcmp(aclIdString, "txrx") == 0 ||
        strcmp(aclIdString, "rx/tx") == 0 || strcmp(aclIdString, "tx/rx") == 0 ||
        strcmp(aclIdString, "rx,tx") == 0 || strcmp(aclIdString, "tx,rx") == 0)
    {
        return ACL_ID_BOTH;
    }
    else
    {
        fprintf(stderr, "Expected one of Rx, Tx or RxTx as second argument instead of '%s'\n", aclIdString);
        return ACL_ID_FAILED;
    }
}

int main(int argc, char * argv[])
{
    const int           CMD_NULL    = -1;
    const int           CMD_LIST    =  0;
    const int           CMD_RESET   =  1;
    const int           CMD_MOD     =  2;
    const int           CMD_ENABLE  =  3;
    const int           CMD_DISABLE =  4;
    const int           CMD_VERSION =  5;

    SC_DeviceId         deviceId = NULL;
    int                 cmd = CMD_NULL;
    int                 acl_id = ACL_ID_NONE;
    int                 index = -1;
    int                 phys_port = -1;
    uint32_t            ip = 0, ip_mask = 0xFFFFFFFF;
    uint16_t            port_min = 0, port_max = 0;
    int                 returnCode = EXIT_SUCCESS;
    SC_Error            errorCode = SC_ERR_SUCCESS;
    bool                listAll = false;

    const char * aclTables[] = { "***", "Tx", "Rx" };

    struct in_addr ipNetworkOrder, ipMaskNetworkOrder;

    if( my_assert(argc > 1, "no argument specified!"))
    {
        help_text(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    listAll = strcmp(argv[1], "-lf") == 0;
    if (strcmp(argv[1], "-l") == 0 || listAll)
    {
        if( my_assert( argc == 3, "option -l expects 1 value!" ) )
            return EXIT_FAILURE;

        if( parse_int_base10( argc, argv, 2, &phys_port ) )
            return EXIT_FAILURE;

        cmd = CMD_LIST;
    }
    else if( strcmp( argv[1], "-r" ) == 0 ) {
        acl_id = ACL_ID_BOTH; /* Default: resets both Tx and Rx */
        if( my_assert( argc > 2 && argc <= 5, "option -r expects at least 1 and at most 3 values!" ) )
            return EXIT_FAILURE;

        if( argc >= 3 )
            if( parse_int_base10( argc, argv, 2, &phys_port ) )
                return EXIT_FAILURE;
        if (argc >= 4)
        {
            if ((acl_id = ParseAclId(argv[3])) == ACL_ID_FAILED)
            {
                return EXIT_FAILURE;
            }
        }
        if ( acl_id != ACL_ID_TX && acl_id != ACL_ID_RX && acl_id != ACL_ID_BOTH) {
            fprintf( stderr, "ACL id must be Rx or Tx or RxTx\n");
            return EXIT_FAILURE;
        }
        if( argc >= 5 )
            if( parse_int_base10( argc, argv, 4, &index ) )
                return EXIT_FAILURE;
        cmd = CMD_RESET;
    }
    else if( strcmp( argv[1], "-m" ) == 0 || strcmp( argv[1], "-mf" ) == 0)
    {
        int value;
        bool forceMask;

        if( my_assert( argc == 9, "option -m expects 7 values!" ) )
            return EXIT_FAILURE;

        forceMask = strcmp( argv[1], "-mf" ) == 0;
        if( parse_int_base10( argc, argv, 2, &phys_port ) )
            return EXIT_FAILURE;
        if ((acl_id = ParseAclId(argv[3])) == ACL_ID_FAILED)
        {
            return EXIT_FAILURE;
        }
        if ( acl_id != ACL_ID_TX && acl_id != ACL_ID_RX) {
            fprintf( stderr, "ACL id must be Rx or Tx\n");
            return EXIT_FAILURE;
        }
        if( parse_int_base10( argc, argv, 4, &index ) )
            return EXIT_FAILURE;
        if( parse_ip_base10( argc, argv, 5, &ip ) )
            return EXIT_FAILURE;
        if( parse_ip_base10( argc, argv, 6, &ip_mask ) )
            return EXIT_FAILURE;
        if (!validate_ip_address_and_mask( ip, ip_mask ) && !forceMask)
        {
            fprintf( stderr, "IP mask %s will never trigger with IP address %s,\n"
                             "if you are sure anyway you can force it with option -mf\n", argv[6], argv[5]);
            return EXIT_FAILURE;
        }
        if( parse_int_base10( argc, argv, 7, &value ) )
            return EXIT_FAILURE;
        port_min = (uint16_t)value;
        if( parse_int_base10( argc, argv, 8, &value ) )
            return EXIT_FAILURE;
        port_max = (uint16_t)value;

        /*if( my_assert( port_min >= 0 && port_min <= 0xffff, "port_min out of range!" ) )
            return EXIT_FAILURE;
        if( my_assert( port_max >= 0 && port_max <= 0xffff, "port_max out of range!" ) )
            return EXIT_FAILURE;*/
        if( my_assert( port_min <= port_max, "port_min must be smaller than port_max !" ) )
            return EXIT_FAILURE;
        cmd = CMD_MOD;
    }
    else if( strcmp( argv[1], "-e" ) == 0 ) {
        if( my_assert( argc == 3, "option -e expects 1 value!" ) )
            return EXIT_FAILURE;

        if( parse_int_base10( argc, argv, 2, &phys_port ) )
            return EXIT_FAILURE;
        cmd = CMD_ENABLE;
    }
    else if( strcmp( argv[1], "-d" ) == 0 ) {
        if( my_assert( argc == 3, "option -d expects 1 value!" ) )
            return EXIT_FAILURE;

        if( parse_int_base10( argc, argv, 2, &phys_port ) )
            return EXIT_FAILURE;
        cmd = CMD_DISABLE;
    }
    else if( strcmp( argv[1], "-v" ) == 0 || strcmp( argv[1], "-V" ) == 0 ) {
        if( my_assert( argc == 2, "option -V expects no values!" ) )
            return EXIT_FAILURE;
        cmd = CMD_VERSION;
    }
    else
    {
        help_text(stdout, argv[0]);
        return EXIT_FAILURE;
    }

    /* Attempt to open device */
    deviceId = SC_OpenDevice(NULL, NULL);
    if (deviceId == NULL)
    {
        if (cmd != CMD_VERSION)
        {
            fprintf( stderr, "Error: Unable to open device.\n" );
            return EXIT_FAILURE;
        }
        deviceId = NULL; /* Continue if cmd is CMD_VERSION */
    }

    if (deviceId != NULL)
    {
        SC_CardInfo cardInfo;
        errorCode = SC_GetCardInfo(deviceId, &cardInfo);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Failed to get card information with error code %d\n", errorCode);
            SC_CloseDevice(deviceId);
            return EXIT_FAILURE;
        }
        if (cardInfo.AclLanesEnabledMask == 0)
        {
            fprintf(stderr, "Driver and FPGA firmware do not support ACL functionality!\n");
            SC_CloseDevice(deviceId);
            return EXIT_FAILURE;
        }
    }

    if (cmd == CMD_LIST)
    {
        int headerPrinted = 0;
        bool allEntriesEmpty = true;

        for (index = 0; index < 128; ++index)
        {
            uint32_t ipRx = 0, ipRxMask = 0xFFFFFFFF;
            uint32_t ipTx = 0, ipTxMask = 0xFFFFFFFF;
            struct in_addr ipRxNetworkOrder, ipRxMaskNetworkOrder;
            struct in_addr ipTxNetworkOrder, ipTxMaskNetworkOrder;
            uint16_t rxPortMin, rxPortMax, txPortMin, txPortMax;
            bool EmptyEntry, showEntry;

            errorCode = SC_AclGet(deviceId, (uint8_t)phys_port, ACL_ID_RX, (uint16_t)index, &ipRxNetworkOrder, &ipRxMaskNetworkOrder, &rxPortMin, &rxPortMax);
            if (errorCode != SC_ERR_SUCCESS)
            {
                returnCode = EXIT_FAILURE;
                break;
            }
            ipRx = ntohl(ipRxNetworkOrder.s_addr);
            ipRxMask = ntohl(ipRxMaskNetworkOrder.s_addr);

            errorCode = SC_AclGet(deviceId, (uint8_t)phys_port, ACL_ID_TX, (uint16_t)index, &ipTxNetworkOrder, &ipTxMaskNetworkOrder, &txPortMin, &txPortMax);
            if (errorCode != SC_ERR_SUCCESS)
            {
                returnCode = EXIT_FAILURE;
                break;
            }
            ipTx = ntohl(ipTxNetworkOrder.s_addr);
            ipTxMask = ntohl(ipTxMaskNetworkOrder.s_addr);

            EmptyEntry = ipRx == 0 && ipRxMask == 0xFFFFFFFF && ipTx == 0 && ipTxMask == 0xFFFFFFFF &&
                rxPortMin == 0 && rxPortMax == 0 && txPortMin == 0 && txPortMax == 0;
            showEntry = !EmptyEntry || listAll;
            if (showEntry)
            {
                if (!headerPrinted)
                {
                    printf("port %d\t\tReceive ACL\t\t\t\tTransmit ACL\n", phys_port);
                    printf("%4s: %-33s %5s %5s\t%-33s %5s %5s\n", "idx", "IP addr/mask", "pmin", "pmax", "IP addr/mask", "pmin", "pmax");
                    headerPrinted = 1;
                }

                printf("%4d: ", index);
                printf_ip(ipRx, ipRxMask);
                printf(" %5d %5d\t", rxPortMin, rxPortMax);

                printf_ip(ipTx, ipTxMask);
                printf(" %5d %5d\n", txPortMin, txPortMax);

                allEntriesEmpty = false;
            }
        }
        if (returnCode == EXIT_FAILURE && errorCode != SC_ERR_ACL_NOT_PRESENT)
        {
            fprintf(stderr, "Failed to read ACL entries at %d with error code %d\n", index, errorCode);
        }
        else if (allEntriesEmpty)
        {
            fprintf(stdout, "No active ACL Rx or Tx entries found!\n");
        }
    }
    else if (cmd == CMD_RESET)
    {
        errorCode = SC_AclReset(deviceId, (uint8_t)phys_port, acl_id, (int16_t)index);
        if (errorCode != SC_ERR_SUCCESS && errorCode != SC_ERR_ACL_NOT_PRESENT)
        {
            fprintf( stderr, "Error: Unable to reset acl on port %d, %s ACL, index %d.\n", phys_port, aclTables[acl_id], index );
            returnCode = EXIT_FAILURE;
        }
    }
    else if (cmd == CMD_MOD)
    {
        ipNetworkOrder.s_addr = htonl(ip);
        ipMaskNetworkOrder.s_addr = htonl(ip_mask);
        errorCode = SC_AclModify(deviceId, (uint8_t)phys_port, acl_id, (uint16_t)index, ipNetworkOrder, ipMaskNetworkOrder, port_min, port_max);
        if (errorCode != SC_ERR_SUCCESS && errorCode != SC_ERR_ACL_NOT_PRESENT)
        {
            fprintf( stderr, "Error: Unable to modify acl on port %d, %s ACL, index %d.\n", phys_port, aclTables[acl_id], index );
            returnCode = EXIT_FAILURE;
        }

        printf( "%4d: %s ", index, aclTables[acl_id] );
        printf_ip( ip, ip_mask );
        printf( " %5d %5d\n", port_min, port_max );
    }
    else if (cmd == CMD_ENABLE)
    {
        errorCode = SC_AclEnable(deviceId, (uint8_t)phys_port);
        if (errorCode != SC_ERR_SUCCESS && errorCode != SC_ERR_ACL_NOT_PRESENT)
        {
            fprintf( stderr, "Error: Unable to enable acl on port %d.\n", phys_port );
            returnCode = EXIT_FAILURE;
        }
    }
    else if (cmd == CMD_DISABLE)
    {
        errorCode = SC_AclDisable(deviceId, (uint8_t)phys_port);
        if (errorCode != SC_ERR_SUCCESS && errorCode != SC_ERR_ACL_NOT_PRESENT)
        {
            fprintf( stderr, "Error: Unable to disable acl on port %d.\n", phys_port );
            returnCode = EXIT_FAILURE;
        }
    }
    else if (cmd == CMD_VERSION)
    {
        SC_PrintVersions(deviceId, stdout, "fbacl");
    }

    if (errorCode == SC_ERR_ACL_NOT_PRESENT)
    {
        fprintf(stderr, "ACL is not present in FPGA configuration on lane %u (error code %d)\n", phys_port, errorCode);
    }

    errorCode = SC_CloseDevice(deviceId);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to close device with error code %d\n", errorCode);
        returnCode = EXIT_FAILURE;
    }

    return returnCode;
}
