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
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <xmmintrin.h>

#include "smartnic.h"
#include "smartnic_tools.h"

/* Channels 6 and 7 are crossed in the demo design and channel 5 is another test so just allow channels 0-4 for now! */
#define MAX_CHANNELS    5
#define CLOCK_ID        CLOCK_MONOTONIC /* CLOCK_MONOTONIC seems to be several times faster than CLOCK_MONOTONIC_RAW on all tried systems */

const uint64_t ONE_SECOND_IN_NS = 1000000000UL;

typedef struct {
    size_t size;
    char *data;
} message;

/* print a message to stderr and abort the program */
static void quit(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);

    fflush(stderr);
    exit(EXIT_FAILURE);
}

/* print a message to stdout with a timestamp */
static void print(const char *format, ...)
{
    time_t now_time;
    struct tm *t;
    char t_str[32];
    struct timeval now;
    gettimeofday(&now, NULL);

    now_time = now.tv_sec;
    t = localtime(&now_time);
    strftime(t_str, sizeof t_str, "%H:%M:%S", t);
    snprintf(t_str + 8, sizeof t_str - 8, ".%06ld: ", now.tv_usec);
    fputs(t_str, stdout);

    {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }
    fputc('\n', stdout);

    fflush(stdout);
}

/* write an array of bytes to a file */
static void dump_to_file(const char *filename, const void *data, size_t size)
{
    FILE *f = fopen(filename, "wb");
    fwrite(data, 1, size, f);
    fclose(f);
}

static size_t generate_messages(message **data, int channelsStart, int channelsEnd, int buckets, size_t size)
{
    size_t total_size = 0;
    int i, j;
    size_t k;
    bool randomSize = size == 0;

    for (i= channelsStart; i<channelsEnd; ++i)
    {
        for (j=0; j<buckets; ++j)
        {
            if (randomSize)
            {
                size = (size_t)((rand() % 63) + 1) * 32;
            }
            total_size += size;
            data[i][j].size = size;
            data[i][j].data = (char *)malloc(size);
            for (k=0; k<(size / 4); ++k)
            {
                *((uint32_t*)(data[i][j].data + (k * 4))) = (uint32_t)rand();
            }
        }
    }

    return total_size;
}

static void free_messages(message **data, int channelsStart, int channelsEnd, int buckets)
{
    int i, j;

    for (i= channelsStart; i<channelsEnd; ++i)
    {
        for (j=0; j<buckets; ++j)
        {
            free(data[i][j].data);
        }
    }
}

static void print_usage(void)
{
    printf("ul_dma_loop - Hammer Test for the Silicom SmartNIC DMA infrastructure\n");
    printf("    -d/--device <device-name>           -- SmartNIC device name. Default name is /dev/smartnic.\n");
    printf("    -c/--channels <number-of-channels>  -- Number of channels to test simultaneously. Default is 1.\n");
    printf("    -s/--startchannel <start-channel>   -- Start channel. Default is 0.\n");
    printf("    -i/--iterations <iterations>        -- Number of iterations of the test to run. Default is 1.\n");
    printf("    -b/--nbuckets <number-of-buckets>   -- Number of buckets to fill. Default is max supported by driver.\n");
    printf("    -z/--size <packet-size>             -- Size of packets in bytes. Default is 0 which creates packets of random length.\n");
    printf("    -f/--filllevellimit <value>         -- Set Tx DMA FIFO software flow control fill level limit. Default is 0 (disabled).\n");
    printf("                                           Value of -1 disables both software and firmware FIFO flow control.\n");
    printf("                                           Value less than -1 enables both software and firmware FIFO flow control,\n");
    printf("                                           software fill level limit is set to abs(value).\n");
    printf("                                           Fill level value -1000 or 1000 uses defaults.\n");
    printf("    -o/--sendoptions <value>            -- Set send options. Default is SC_SEND_SINGLE_PACKET | SC_SEND_WAIT_FOR_PIPE_FREE.\n");
    printf("    -p/--pollfifo                       -- Poll Tx DMA FIFO fill level.\n");
    printf("    -l/--looptime                       -- Measure send loop time.\n");
    printf("    -v/--verbosity <level>              -- Show verbose output (level = 0, 1, or 2).\n");
    printf("    -h/--help                           -- Show help and usage.\n");
}

static double time_diff_ns( const struct timespec *t0, const struct timespec *t1 )
{
    double dt = (double)((t1->tv_sec*1000000000+t1->tv_nsec) - (t0->tv_sec*1000000000+t0->tv_nsec));
    return dt;
}

static double time_elapsed_ns( const struct timespec *t0 )
{
    struct timespec t1;
    double dt;
    clock_gettime(CLOCK_ID, &t1);
    dt = time_diff_ns( t0, &t1 );
    return dt;
}

/*****************************************************************************/

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
    fprintf(stderr, "API library file %s line %d function %s error code %d: %s\n", fileName, lineNumber, functionName, errorCode, errorMessage);

    return errorCode;
}

#include "../driver/regs.h"
int main(int argc, char * argv[])
{
    const char * deviceName = NULL;
    int channelsStart = 0;
    int numberOfChannels = 1;
    int channelsEnd = channelsStart + numberOfChannels;
    int64_t iterations = 1;
    int buckets = 1, maxNnumberOfBuckets;
    int verbosity = 0;
    bool pollFifoFillLevel = false;
    bool measureSendLoopTime = false;
    int16_t fillLevelLimit = 0;
    size_t size = 0;
    SC_SendOptions sendOptions = (SC_SendOptions)(SC_SEND_SINGLE_PACKET | SC_SEND_WAIT_FOR_PIPE_FREE);
    SC_Error errorCode;
    int i, j, iteration;
    SC_DeviceId deviceId;
    SC_ChannelId channelId[MAX_CHANNELS];
    SC_ChannelOptions channelOptions;
    size_t data_bytes_tx = 0;
    size_t data_bytes_rx = 0;
    const SC_Packet * prev_packet[MAX_CHANNELS];
    message **data;
    uint16_t maxFifoFillLevel = 0;
    int sendLoopCount = 0;
    double sendLoopTotalTime = 0.0;

    errorCode = SC_RegisterErrorHandler(ErrorHandler, NULL);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "SC_RegisterErrorHandler failed with error code %d\n", errorCode);
    }

    while (true)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            { "device",         required_argument,  0, 'd' },
            { "channels",       required_argument,  0, 'c' },
            { "startchannel",   required_argument,  0, 's' },
            { "iterations",     required_argument,  0, 'i' },
            { "nbuckets",       required_argument,  0, 'b' },
            { "size",           required_argument,  0, 'z' },
            { "filllevellimit", required_argument,  0, 'f' },
            { "sendoptions",    required_argument,  0, 'o' },
            { "verbosity",      required_argument,  0, 'v' },
            { "pollfifo",       no_argument,        0, 'p' },
            { "looptime",       no_argument,        0, 'l' },
            { "help",           no_argument,        0, 'h' },
            { 0,                0,                  0, 0 }
        };

        int option_chr = getopt_long(argc, argv, "d:c:s:i:b:z:f:o:v:plh", long_options, &option_index);

        if (option_chr == -1)
            break;

        switch (option_chr)
        {
            case 'd':
                deviceName = optarg;
                break;

            case 'c':
                numberOfChannels = atoi(optarg);
                break;

            case 's':
                channelsStart = atoi(optarg);
                break;

            case 'i':
                iterations = atol(optarg);
                break;

            case 'b':
                buckets = atoi(optarg);
                break;

            case 'z':
                {
                    const int MaxSize = 2000; /* Conservative max, depends on driver SC_BUCKET_SIZE */
                    int intSize = atoi(optarg);
                    if (intSize < 0 || intSize > MaxSize)
                    {
                        quit("Invalid packet size parameter '%s'. Valid range is [0-%d]\n", optarg, MaxSize);
                    }
                    size = (size_t)intSize;
                }
                break;

            case 'f':
                {
                    const int MaxFillLevelLimit = 512;
                    int intFillLevelLimit = atoi(optarg);
                    if (!((intFillLevelLimit >= -MaxFillLevelLimit && intFillLevelLimit <= MaxFillLevelLimit) ||
                          (intFillLevelLimit == -1000 || intFillLevelLimit == +1000)))
                    {
                        quit("Invalid packet size parameter '%s'. Valid range is [-%d-%d] or -1000 or +1000\n", optarg, MaxFillLevelLimit, MaxFillLevelLimit);
                    }
                    fillLevelLimit = (int16_t)intFillLevelLimit;
                }
                break;

            case 'p':
                pollFifoFillLevel = true;
                break;

            case 'l':
                measureSendLoopTime = true;
                break;

            case 'o':
                sendOptions = (SC_SendOptions)atoi(optarg);
                break;

            case 'v':
                verbosity = atoi(optarg);
                break;

            case 'h':
                print_usage();
                return(EXIT_SUCCESS);

            default:
                print_usage();
                return(EXIT_FAILURE);
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

    if (numberOfChannels < 1 || numberOfChannels > MAX_CHANNELS)
    {
        quit("Number of channels must be set to a value between 1 and %d\n", MAX_CHANNELS);
    }
    if (channelsStart + numberOfChannels < 0 || channelsStart + numberOfChannels > MAX_CHANNELS)
    {
        quit("Channel start must be set to a value between 0 and %d\n", MAX_CHANNELS - numberOfChannels);
    }
    if (iterations < 1)
    {
        quit("Number of iterations must be at least 1\n");
    }

    channelsEnd = channelsStart + numberOfChannels;

    printf("Channels start=%d, Channels end=%d, Iterations=%ld, Buckets=%d\n", channelsStart, channelsEnd, iterations, buckets);

    deviceId =  SC_OpenDevice(deviceName, NULL);
    if (deviceId == NULL)
    {
        quit("Cannot open the device\n");
    }

    {
        /* Allocate a channel to get a value for numberOfBuckets */
        SC_ChannelId ulChannelId = SC_AllocateUserLogicChannel(deviceId, (int16_t )channelsStart, NULL);
        if (ulChannelId == NULL)
        {
            quit("Cannot allocate user logic channel\n");
        }
        maxNnumberOfBuckets = SC_GetMaxNumberOfBuckets(ulChannelId);
        errorCode = SC_DeallocateChannel(ulChannelId);
        if (errorCode != SC_ERR_SUCCESS)
        {
            quit("SC_DeallocateChannel failed with error code %d\n", errorCode);
        }
    }
    if (buckets < 1 || buckets > maxNnumberOfBuckets)
    {
        quit("Number of buckets must be set to a value between 1 and %d\n", maxNnumberOfBuckets);
    }

    errorCode = SC_GetChannelOptions(NULL, &channelOptions); /* Get default channel options */
    if (errorCode != SC_ERR_SUCCESS)
    {
        quit("SC_GetChannelOptions failed with error code %d\n", errorCode);
    }

    for (i= channelsStart; i<channelsEnd; ++i)
    {
        const SC_Packet * pPacket;
        /* -1000 uses defaults by passing NULL to SC_AllocateUserLogicChannel */
        SC_ChannelOptions * pChannelOptions = fillLevelLimit == -1000 ? NULL : &channelOptions;

        if (fillLevelLimit == +1000)
        {
            /* +1000 gets default options with SC_GetChannelOptions (done above) and then passes them to SC_AllocateUserLogicChannel. */
            /* Result is the same as passing NULL to SC_AllocateUserLogicChannel. */
        }
        else if (fillLevelLimit != -1000)
        {
            if (fillLevelLimit > 0)
            {
                channelOptions.UseTxDmaNormalFifoFirmwareFillLevel = false; /* Don't use firmware FIFO flow control. */
                                                                            /* Aggressive value 90 assumes maximum 5 user logic channels are in use
                                                                            leaving 512 - 5 * 90 = 62 FIFO entries for use by NICs (each NIC uses max 4 entries in the driver) */
                channelOptions.TxDmaNormalFifoSoftwareFillLevelLimit = (uint16_t)fillLevelLimit;
            }
            else if (fillLevelLimit == 0)
            {
                channelOptions.UseTxDmaNormalFifoFirmwareFillLevel = true; /* Use firmware FIFO flow control. */
                channelOptions.TxDmaNormalFifoSoftwareFillLevelLimit = 0;
            }
            else if (fillLevelLimit == -1)
            {
                /* Disable both firmware and software Tx DMA FIFO flow control. */
                channelOptions.UseTxDmaNormalFifoFirmwareFillLevel = false;
                channelOptions.TxDmaNormalFifoSoftwareFillLevelLimit = 0;
            }
            else if (fillLevelLimit < -1)
            {
                /* Enable both firmware and software Tx DMA FIFO flow control. */
                channelOptions.UseTxDmaNormalFifoFirmwareFillLevel = true;
                channelOptions.TxDmaNormalFifoSoftwareFillLevelLimit = (uint16_t)abs(fillLevelLimit);
            }
        }

        channelId[i] = SC_AllocateUserLogicChannel(deviceId, (int16_t)i, pChannelOptions);
        if (channelId[i] == NULL)
        {
            quit("Can't allocate user logic channel %d\n", i);
        }

        if (verbosity >= 1)
        {
            static bool firstTime = true;

            if (firstTime)
            {
                SC_ChannelOptions currentChannelOptions;
                errorCode = SC_GetChannelOptions(channelId[i], &currentChannelOptions); /* Get current channel options */
                if (errorCode != SC_ERR_SUCCESS)
                {
                    quit("SC_GetChannelOptions failed with error code %d\n", errorCode);
                }
                printf("All channels: UseTxDmaNormalFifoFirmwareFillLevel = %s TxDmaNormalFifoSoftwareFillLevelLimit = %d\n",
                    currentChannelOptions.UseTxDmaNormalFifoFirmwareFillLevel ? "true" : "false", currentChannelOptions.TxDmaNormalFifoSoftwareFillLevelLimit);
                firstTime = false;
            }
        }

        /* Check that no packet has arrived on the channel yet */
        /* This helps troubleshooting the pldma value not beeing cleared correctly */
        pPacket = SC_GetNextPacket(channelId[i], NULL, SC_TIMEOUT_NONE);
        if (pPacket != NULL)
        {
            quit( "Packet is arriving on channel %d before any packet was sent: is the pldma clear not working?!!", i );
        }
    }

    srand(0);

    for (i= channelsStart; i<channelsEnd; ++i)
        prev_packet[i] = NULL;

    data = (message **)malloc((size_t)MAX_CHANNELS * sizeof(message*));
    for (i= channelsStart; i<channelsEnd; ++i)
        data[i] = (message *)malloc((size_t)buckets * sizeof(message));

    for (iteration = 0; iteration < iterations; ++iteration)
    {
        size_t bytes_to_send;
        if (verbosity >= 2)
        {
            print("iteration %d / %d", iteration + 1, iterations);
        }
        bytes_to_send = generate_messages(data, channelsStart, channelsEnd, buckets, size);
        data_bytes_tx += bytes_to_send;

        for (j=0; j<buckets; ++j)
        {
            for (i= channelsStart; i<channelsEnd; ++i)
            {
                struct timespec startTime;
                struct timespec endTime;
                SC_Bucket * pBucket;
                uint8_t * pBucketPayload;
                uint16_t payloadLength;

                if (measureSendLoopTime)
                {
                    clock_gettime(CLOCK_ID, &startTime);
                }

                while ((pBucket = SC_GetBucket(channelId[i], 0)) == NULL);
                pBucketPayload = SC_GetBucketPayload(pBucket);
                payloadLength = (uint16_t)data[i][j].size;
                memcpy(pBucketPayload, data[i][j].data, payloadLength);
                errorCode = SC_SendBucket(channelId[i], pBucket, payloadLength, sendOptions, NULL);

                if (measureSendLoopTime)
                {
                    clock_gettime(CLOCK_ID, &endTime);
                    sendLoopTotalTime += time_diff_ns(&startTime, &endTime);
                }

                if (errorCode != SC_ERR_SUCCESS)
                {
                    quit("SC_SendBucket failed with error code %d\n", errorCode);
                }
                else
                {
                    ++sendLoopCount;
                    /*_mm_pause();*/
                    /*usleep(100);*/
                }

                if (pollFifoFillLevel)
                {
                    /* Poll FIFO fill level */
                    uint16_t normalFifoFillLevel;
                    errorCode = SC_GetTxDmaFifoFillLevels(deviceId, &normalFifoFillLevel, NULL);
                    if (errorCode != SC_ERR_SUCCESS)
                    {
                        quit("SC_SendBucket failed with error code %d\n", errorCode);
                    }
                    if (normalFifoFillLevel > maxFifoFillLevel)
                    {
                        maxFifoFillLevel = normalFifoFillLevel;
                    }
                }
            }
        }

        if (verbosity >= 2)
        {
            print("sent %zu bytes", bytes_to_send);
        }

        for (j=0; j<buckets; ++j)
        {
            for (i= channelsStart; i<channelsEnd; ++i)
            {
                const SC_Packet * pNextPacket = NULL;
                const void * payload;
                size_t payload_len;
                const message * m;
                struct timespec startTime;
                clock_gettime(CLOCK_ID, &startTime);
                do
                {
                    pNextPacket = SC_GetNextPacket(channelId[i], prev_packet[i], SC_TIMEOUT_NONE);
                } while (pNextPacket == NULL && time_elapsed_ns(&startTime) < 1 * ONE_SECOND_IN_NS);
                if (pNextPacket == NULL)
                {
#ifdef DEBUG
                    const uint8_t * rawPayload = SC_GetRawPayload(prev_packet[i]);
                    ptrdiff_t headerSize = rawPayload - (const uint8_t *)prev_packet[i];
                    uint16_t lastPayloadLength = SC_GetUserLogicPayloadLength(prev_packet[i]);
                    printf("Last packet %p: header %lu (0x%lx) payload %u (0x%x):\n", (const void *)prev_packet[i], headerSize, headerSize, lastPayloadLength, lastPayloadLength);
                    printf("Next possible packet:\n");
                    SCI_HexDump(&printf, (const uint8_t *)prev_packet[i] + headerSize + lastPayloadLength, 200, 16, true);
#endif
                    quit("Did not receive any packets on channel %d within 1 second\n", SC_GetChannelNumber(channelId[i]));
                }

                payload = SC_GetUserLogicPayload(pNextPacket);
                payload_len = SC_GetUserLogicPayloadLength(pNextPacket);
                m = &data[i][j];

                if (payload_len != m->size)
                {
                    quit("incorrect payload length: expected %zu bytes, got %zu for bucket=%d channel=%d", m->size, payload_len, j, i);
                }
                if (memcmp(payload, m->data, payload_len) != 0)
                {
                    dump_to_file("good_payload.data", m->data, m->size);
                    dump_to_file("bad_payload.data", payload, payload_len);
                    quit("received incorrect payload for bucket=%d channel=%d", j, i);
                }

                prev_packet[i] = pNextPacket;
                data_bytes_rx += payload_len;
            }
        }

        /* To be more effective we only mark the last received packet for each channel */
        /* as processed. Alternatively each received packet can be marked as processed */
        /* but that is less effective if the packet ring buffer is big enough to buffer */
        /* many packets. */
        for (i= channelsStart; i<channelsEnd; ++i)
        {
            if (SC_UpdateReceivePtr(channelId[i], prev_packet[i]) != SC_ERR_SUCCESS)
            {
                quit("failed to update receive pointer");
            }
        }

        if (verbosity >= 2)
        {
            print("received all data for iteration %d / %d", iteration + 1, iterations);
        }
        free_messages(data, channelsStart, channelsEnd, buckets);
    }

    for (i= channelsStart; i<channelsEnd; ++i)
        free(data[i]);
    free(data);

    SC_CloseDevice(deviceId);

    printf("Data bytes sent:     %lu\n", data_bytes_tx);
    printf("Data bytes received: %lu\n", data_bytes_rx);
    if (pollFifoFillLevel)
    {
        printf("Max FIFO fill level: %u\n", maxFifoFillLevel);
    }
    if (measureSendLoopTime)
    {
        printf("Send loop time:      %.2f ns\n", sendLoopTotalTime / sendLoopCount);
    }

    if (data_bytes_tx != data_bytes_rx)
    {
        quit("ERROR: data received is different from data sent\n");
    }

    return 0;
}
