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
#include <getopt.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "smartnic.h"

const uint16_t plen = 10;

struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

static int handleConnection(SC_ChannelId channelId)
{
    const SC_Packet *incoming_packet = NULL;
    static const SC_Packet * prev_packet = NULL;
    static unsigned int count = 0;
    SC_Error errorCode;
    SC_Bucket * pBucket;
    uint8_t * pBucketPayload;

    /* fprintf(stderr, "[%d] Next packet for channel %d\n", getpid(), ctx->phy_channel_no); */
    incoming_packet = SC_GetNextPacket(channelId , prev_packet, SC_TIMEOUT_INFINITE);

    /*fprintf(stderr, "[%d]  received %d %p %s\n", getpid(), channelId->phy_channel_no , incoming_packet, SC_GetTcpPayload(incoming_packet )); */

    if (memcmp("quit", SC_GetTcpPayload(incoming_packet), 4) == 0)
        return -1;

    /* Send something back: */
    pBucket = SC_GetBucket(channelId, 0);
    pBucketPayload = SC_GetBucketPayload(pBucket);
    memcpy(pBucketPayload, "012345678901234567890", plen);

    errorCode = SC_SendBucket(channelId, pBucket, plen, SC_SEND_DEFAULT, NULL);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "SC_SendBucket failed with error code %d\n", errorCode);
    }
    /*fprintf(stderr,"[%d] Sent: %u bytes Tcp_status: 0x%x\n", getpid(), plen, SC_GetTcpState(channelId)); */

    prev_packet = incoming_packet;

    if ((count++ % 1000) == 0)
    {
        SC_UpdateReceivePtr(channelId, prev_packet);
    }

    return 0;
}

static SC_Error ErrorHandler(const char * fileName, int lineNumber, const char * functionName, SC_Error errorCode, const char * errorMessage, void * context)
{
    UNUSED_ALWAYS(context);

    fprintf(stderr, "Error at %s#%d function %s: error code %d, %s\n", fileName, lineNumber, functionName, errorCode, errorMessage);
    return SC_ERR_HANDLER_CONTINUE;
}

int main(int argc, char * argv[])
{
    pid_t cpid;
    int status;
    int w;
    unsigned long i= 0;
    SC_Error errorCode;
    struct timespec start, end, res;
    SC_Bucket * pBucket;
    uint8_t * pBucketPayload;
    uint8_t toeLane = 0;

    const char * IP_ADDR = "10.0.0.5";
    const uint16_t PORT = 2000;
    const int16_t CHANNEL = -1;

    UNUSED_ALWAYS(argc); UNUSED_ALWAYS(argv);

    SC_RegisterErrorHandler(&ErrorHandler, NULL);

    fprintf(stdout, "This test measure the time it takes to ping/pong of 15 Million tcp packets between two SmartNIC channels via a loopback module. ( estimated time < 1 min )\n");

    if ( ( cpid = fork() ) )
    {
        /* Parent */
        SC_DeviceId deviceId;
        SC_ChannelId channelId;
        struct in_addr ip;
        struct in_addr netmask;
        struct in_addr gateway = { 0 };

        deviceId = SC_OpenDevice(NULL, NULL);
        if (deviceId == NULL)
        {
            fprintf(stderr, "Can't initaliaze card! Is the driver loaded?\n");
            exit(EXIT_FAILURE);
        }

        inet_aton(IP_ADDR, &ip);
        inet_aton("255.255.255.0", &netmask);

        if (SC_SetLocalAddress(deviceId, toeLane, ip, netmask, gateway, NULL))
        {
            fprintf(stderr, "Can't allocate tcp channel, parent SC_SetLocalAddress failed, errno %d\n", errno);
            exit(EXIT_FAILURE);
        }

        channelId = SC_AllocateTcpChannel(deviceId, toeLane, CHANNEL, NULL);
        if (channelId == NULL)
        {
            fprintf(stderr, "Can't allocate tcp channel, parent SC_AllocateTcpChannel failed\n");
            exit(EXIT_FAILURE);
        }

        if (SC_Listen(channelId, PORT, NULL) != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Listen returned an error\n");
            exit(EXIT_FAILURE);
        }

        /*fprintf(stderr, "[%d] Listen tcp_status %p %x\n", getpid(), lctx, sn_get_tcp_status( lctx ) ); */

        pBucket = SC_GetBucket(channelId, 0);
        pBucketPayload = SC_GetBucketPayload(pBucket);

        memcpy(pBucketPayload, "11111111111111111111", plen);

        errorCode = SC_SendBucket(channelId, pBucket, plen, SC_SEND_DEFAULT, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Parent SC_SendBucket failed with error code %d\n", errorCode);
        }

        while (SC_GetTcpState( channelId) != SC_TCP_CLOSED )
        {
            if (handleConnection(channelId)) break;

            if (SC_GetTcpState( channelId ) == SC_TCP_CLOSE_WAIT) {
                break;
            }
        }

        /*fprintf(stderr, "Close connection\n"); */
        SC_Disconnect(channelId);

        SC_CloseDevice(deviceId);

        /* Wait for pid to finish */
        do {
            w = waitpid(cpid, &status, WUNTRACED | WCONTINUED);
            if (w == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

            if (WIFEXITED(status)) {
                /*printf("exited, status=%d\n", WEXITSTATUS(status)); */
            } else if (WIFSIGNALED(status)) {
                printf("killed by signal %d\n", WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                printf("stopped by signal %d\n", WSTOPSIG(status));
            } else if (WIFCONTINUED(status)) {
                printf("continued\n");
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        exit(EXIT_SUCCESS);
    }
    else
    {
        /* Child */
        SC_DeviceId sctx;
        SC_ChannelId channelId;

        sctx = SC_OpenDevice(NULL, NULL);
        if (sctx == NULL)
        {
            fprintf(stderr, "Can't initaliaze card! Is the driver loaded?\n");
            exit(EXIT_FAILURE);
        }

        sleep(1);

        channelId = SC_AllocateTcpChannel( sctx, toeLane, CHANNEL, NULL);
        if (channelId == NULL)
        {
            fprintf(stderr, "Can't allocate tcp channel, child sn_allocate_tcp_channel failed\n");
            exit(EXIT_FAILURE);
        }

        if (SC_Connect( channelId, IP_ADDR, PORT, /*SC_ConnectOptions *=*/NULL) != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Can't connect to port PORT. Are the loopback module inserted?\n");
            exit(EXIT_FAILURE);
        }

        /*fprintf(stderr, "[%d] Connect tcp_status %p %x\n", getpid(), sctx, sn_get_tcp_status( sctx ) ); */

        pBucket = SC_GetBucket(channelId, 0);
        pBucketPayload = SC_GetBucketPayload(pBucket);
        memcpy(pBucketPayload, "quit", 4);

#if 0
        /* Old code: send commented out? do we send here or not? */
        errorCode = SC_SendBucket(channelId, pBucket, 4, SC_SEND_DEFAULT, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "Parent SC_SendBucket failed with error code %d\n", errorCode);
        }
#endif

        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        while(i++ < 15000000)  {
            i++;
            /*fprintf(stderr, "[%d] tcp_status %p %x\n", getpid(), sctx, sn_get_tcp_status( sctx ) ); */
            handleConnection(channelId);
/*
            if (( i % 100000 ) == 0 ) {
                fprintf(stderr, "%lu %d\n", i, *p);
            }
*/
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        res = diff(start, end);
        fprintf(stdout, "Time: %lu sec: %lu ns\n", res.tv_sec, res.tv_nsec);

        /*fprintf(stderr, "Child finish\n"); */

        SC_Disconnect(channelId);

        SC_CloseDevice(sctx);

        exit(EXIT_SUCCESS);
    }
}

