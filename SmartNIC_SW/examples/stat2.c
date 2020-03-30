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

#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include PRODUCT_H

static bool stop_cond = false;

/* Signal handler */
void sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        if (stop_cond)
        {
            fprintf(stderr, "First ctrl-C did not stop stat. Aborting...\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            stop_cond = true;
        }
    }
}

static uint64_t log10n(uint64_t n)
{
    if (n == 0)
    {
        return 1;
    }
    else
    {
        uint64_t count = 0;
        while (n > 0)
        {
            n /= 10;
            ++count;
        }
        return count;
    }
}

typedef struct
{
    const char *    Header;                 /**< Column header. */
    bool            RightJustified;         /**< Is column right or left justified. */
    const char *    ColumnSeparator;        /**< Column sepator. */
    uint64_t        MaxValue;               /**< Maximum value seen on this column. */
    uint64_t        FieldWidth;             /**< Dynamic field width that grows as required. Never becomes less. */
    bool            ShowNonzeroValueSeen;   /**< Column has seen non-zero values in the past. */
    bool            OnlyRx;                 /**< Column is only available for Rx. Tx shows "  ". */
} FieldType;

#define MAX_NETWORK_INTERFACES 16

static FieldType fields[] =
{
    { "NetIf",          true,   " ",    0,      0,  false,  false },
    { "Seconds",        false,  ":",    0,      0,  false,  false },
    { "Nanosec",        true,   " ",    0,      0,  false,  false },
    { "OK",             false,  " ",    0,      0,  true,   false },
    { "Errors",         false,  " ",    0,      0,  true,   true  },
    { "1-63",           false,  " ",    0,      0,  true,   false },
    { "64",             false,  " ",    0,      0,  true,   false },
    { "65-127",         false,  " ",    0,      0,  true,   false },
    { "128-255",        false,  " ",    0,      0,  true,   false },
    { "256-511",        false,  " ",    0,      0,  true,   false },
    { "<=1023",         false,  " ",    0,      0,  true,   false },
    { "<=1518",         false,  " ",    0,      0,  true,   false },
    { ">1518",          false,  " ",    0,      0,  true,   false },
    { "OK-bytes",       false,  " ",    0,      0,  true,   false },
    { "Err-bytes",      false,  " ",    0,      0,  true,   true  },
    { "RxOvrflw",       false,  " ",    0,      0,  true,   true  },
};

typedef int SC_AclStatistics; // Just a dummy type; ACL statistics is not available in SmartNIC.

/* NB: Cannot add initializer here because of GNU compiler bug. Array is initialized in main. */
static bool s_NonzeroValueSeen[sizeof(fields) / sizeof(fields[0])][2 * MAX_NETWORK_INTERFACES];

typedef enum
{
    PRINT_TYPE_NONE,
    PRINT_TYPE_HEADER,
    PRINT_TYPE_VALUE
} PrintType;

typedef enum
{
    RX_PROLOGUE,
    TX_PROLOGUE,
    HEADER_PROLOGUE
} PrologueType;

static const char * s_Prologue[] = { "Rx", "Tx", "  "};

static void P(size_t index, PrologueType prologueIndex, uint8_t networkInterface, uint64_t value, PrintType printType, bool showNonzeroValueSeen)
{
    char format[100];
    bool nonzeroValueSeen = false;
    const FieldType * pField = &fields[index];

    assert(index < sizeof(fields) / sizeof(fields[0]));
    assert(prologueIndex >= RX_PROLOGUE && prologueIndex <= HEADER_PROLOGUE);

    if (pField->FieldWidth == 0)
    {
        fields[index].FieldWidth = strlen(pField->Header);
    }
    if (value > pField->MaxValue)
    {
        uint64_t numberOfValueDigits = log10n(value);
        fields[index].MaxValue = value;
        if (numberOfValueDigits > pField->FieldWidth)
        {
            fields[index].FieldWidth = numberOfValueDigits;
        }
    }
    if (prologueIndex != HEADER_PROLOGUE)
    {
        nonzeroValueSeen = s_NonzeroValueSeen[index][MAX_NETWORK_INTERFACES * prologueIndex + networkInterface];
        if (!nonzeroValueSeen && value > 0)
        {
            if (pField->ShowNonzeroValueSeen && showNonzeroValueSeen)
            {
                s_NonzeroValueSeen[index][MAX_NETWORK_INTERFACES * prologueIndex + networkInterface] = true;
                nonzeroValueSeen = true;
            }
        }
    }

    switch (printType)
    {
        const char * marker;

        case PRINT_TYPE_NONE:
            break;
        case PRINT_TYPE_HEADER:
            snprintf(format, sizeof(format), "%%%lus%s%s", pField->FieldWidth, pField->ShowNonzeroValueSeen ? " " : "", pField->ColumnSeparator);
            fprintf(stdout, format, fields[index].Header);
            break;
        case PRINT_TYPE_VALUE:
            if (pField->OnlyRx && prologueIndex == TX_PROLOGUE)
            {
                snprintf(format, sizeof(format), "%%%s%lus%s%s", pField->RightJustified ? "-" : "", pField->FieldWidth, " ", pField->ColumnSeparator);
                fprintf(stdout, format, "  ");
            }
            else
            {
                const char * markerSymbol = nonzeroValueSeen && value == 0 ? "^" : " ";
                marker = pField->ShowNonzeroValueSeen ? markerSymbol : "";
                snprintf(format, sizeof(format), "%%%s%lulu%s%s", pField->RightJustified ? "-" : "", pField->FieldWidth, marker, pField->ColumnSeparator);
                fprintf(stdout, format, value);
            }
            break;
        default:
            fprintf(stderr, "Unknown print type %d<n", printType);
            exit(EXIT_FAILURE);
            break;
    }
}

void print_stat_port(
    PrologueType                            prologueIndex,
    uint8_t                                 networkInterface,
    SC_PacketStatistics *                   p,
    const SC_NetworkInterfaceStatistics *   s,
    const SC_AclStatistics *                pACL,
    PrintType                               printType,
    bool                                    showNonzeroValueSeen)
{
    size_t index = 0;

    if (printType != PRINT_TYPE_NONE)
    {
        fprintf(stdout, "%s", s_Prologue[prologueIndex]);
    }

    P(index++, prologueIndex, networkInterface, networkInterface, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, p->Timestamp.Seconds, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, p->Timestamp.Nanoseconds, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfOkFrames, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfErrorFrames, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_1_63, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_64, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_65_127, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_128_255, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_256_511, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_512_1023, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_1024_1518, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfFramesSize_Above_1518, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfOkBytes, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfErrorBytes, printType, showNonzeroValueSeen);
    P(index++, prologueIndex, networkInterface, s->NumberOfRxOverflowFrames, printType, showNonzeroValueSeen);
    if (pACL != NULL)
    {
    }

    if (printType != PRINT_TYPE_NONE)
    {
        fprintf(stdout, "\n");
    }
}

static void help()
{
    fprintf(stderr,
        "Show device statistics\n"
        "\n"
        "usage: stat [-h|--help] [-d|--device <device-name>] [-s|--sum] [-z|--zero] [-|--once]\n");
    fprintf(stderr,
        "\n"
        "    -s|--sum   = Cumulate (sum) column values. Default is to show delta counts from previous sample.\n"
        "    -z|--zero  = Show columns without ^ markers, i.e. don't remember nonzero entries.\n"
        "    -o|--once  = Show only one sample and exit. Default is to show new sample every second.\n"
        "\n");
    fprintf(stderr,
        "Statistics column explanations:\n"
        "    Rx         = Receive.\n"
        "    Tx         = Transmit.\n"
        "    NetIf      = Physical network interface as 0, 1, ...\n"
        "    Seconds    = Seconds from some arbitrary start point.\n"
        "    Nanosec    = Nanoseconds.\n");
    fprintf(stderr,
        "    OK         = Number of frames that are valid (not corrupted) including range 1-63.\n"
        "    Errors     = Rx: number of corrupted frames.\n"
        "                 Tx: number of frames sent to the MAC with an instruction to invalidate the packet.\n"
        "    1-63       = Number of valid frames in length range [1-63].\n"
        "    64         = Number of valid frames of length 64.\n");
    fprintf(stderr,
        "    65-127     = Number of valid frames in length range [65-127].\n"
        "    128-255    = Number of valid frames in length range [128-255].\n"
        "    256-511    = Number of valid frames in length range [256-511].\n"
        "    <=1023     = Number of valid frames in length range [512-1023].\n"
        "    <=1518     = Number of valid frames in length range [1024-1518].\n");
    fprintf(stderr,
        "    >1518      = Number of valid frames in above 1518 in length.\n");
    fprintf(stderr,
        "    OK-bytes   = Number of bytes in valid packets of length >= 64.\n"
        "    Err-bytes  = Number of bytes in packets of length <= 63 or corrupted.\n"
        "    RxOvrflw   = Number of packets dropped in the Rx path.\n");
    fprintf(stderr,
        "\n"
        "    ^ after a column means that non-zero value(s) have been seen earlier.\n"
        "    Not available columns are shown as empty.\n");
}

int main(int argc, char * argv[])
{
    const char * deviceName = NULL;
    SC_DeviceId statDeviceId;
    uint64_t lastSeconds = 0;
    PrintType printType = PRINT_TYPE_NONE, headerPrintType = PRINT_TYPE_NONE;
    bool sum = false;
    bool showNonzeroValueSeen = true;
    bool once = false;
    uint64_t missedStatistics = 0;
    SC_NetworkInterfaceStatistics summedNetworkInterfaceStatisticsRx[16], summedNetworkInterfaceStatisticsTx[16];
    const SC_AclStatistics * pACL = NULL;

    memset(s_NonzeroValueSeen, 0, sizeof(s_NonzeroValueSeen));

    while (true)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            { "device",     required_argument,  0, 'd'  },
            { "help",       no_argument,        0, 'h'  },
            { "sum",        no_argument,        0, 's'  },
            { "zero",       no_argument,        0, 'z'  },
            { "once",       no_argument,        0, 'o'  },
            { 0,            0,                  0, '\0' }
        };

        int opt_chr = getopt_long(argc, argv, "d:hszo", long_options, &option_index);

        if (opt_chr == -1)
            break;

        switch (opt_chr)
        {
            case 'd':
                deviceName = optarg;
                break;

            case 'h':
                help();
                exit(EXIT_SUCCESS);
                break;

            case 's':
                sum = true;
                break;

            case 'z':
                showNonzeroValueSeen = false;
                break;

            case 'o':
                once = true;
                break;

            default:
                fprintf(stderr, "Unknown command line option %s\n", argv[option_index]);
                exit(EXIT_FAILURE);
                break;
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

    if (once)
    {
        if (sum)
        {
            fprintf(stderr, " *** ERROR: Cannot cumulate column sums when '-o|--once' option is used\n");
        }
        sum = false;
        showNonzeroValueSeen = false;
    }
    if (sum)
    {
        showNonzeroValueSeen = false;
    }

    /* Register signal handler */
    if (signal(SIGINT, &sig_handler) == SIG_ERR)
    {
        fprintf(stderr, "Cannot register signal handler\n");
        exit(EXIT_FAILURE);
    }

    statDeviceId = SC_OpenDevice(deviceName, NULL);
    if (statDeviceId == NULL)
    {
        fprintf(stderr, "Cannot open device '%s'. Is driver loaded?\n", deviceName == NULL ? "/dev/" LOWER_PRODUCT : deviceName);
        exit(EXIT_FAILURE);
    }

    memset(&summedNetworkInterfaceStatisticsRx, 0, sizeof(summedNetworkInterfaceStatisticsRx));
    memset(&summedNetworkInterfaceStatisticsTx, 0, sizeof(summedNetworkInterfaceStatisticsTx));


    while (!stop_cond)
    {
        SC_PacketStatistics statistics;
        const size_t ExpectedStructSize = sizeof(SC_PacketStatistics);

        SC_Error errorCode = SC_GetStatistics(statDeviceId, &statistics);
        if (errorCode != SC_ERR_SUCCESS)
        {
            if (errorCode == SC_ERR_FPGA_DOES_NOT_SUPPORT_FRAME_STATISTICS)
            {
                fprintf(stderr, "Sorry, FPGA on this card does not include statistics!\n");
            }
            else
            {
                fprintf(stderr, "SC_GetStatistics failed with error code %d\n", errorCode);
            }
            exit(EXIT_FAILURE);
        }

        if (statistics.StructSize != ExpectedStructSize)
        {
            fprintf(stderr, " *** ERROR: statistics.StructSize is %u instead of expected %lu,"
                " did something change in statistics fields?\n", statistics.StructSize, ExpectedStructSize);
        }

        if (statistics.Timestamp.Seconds != lastSeconds)
        {
            uint8_t networkInterface = 0;
            SC_NetworkInterfaceStatistics dummy;
            SC_AclStatistics dummyACL;
            SC_AclStatistics * pDummyACL = &dummyACL;
            memset(&dummyACL, 0, sizeof(dummyACL));

            if (sum)
            {
                for (networkInterface = 0; networkInterface < statistics.NumberOfNetworkInterfaces; ++networkInterface)
                {
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfOkFrames               /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfOkFrames;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfErrorFrames            /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfErrorFrames;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_1_63        /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_1_63;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_64          /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_64;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_65_127      /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_65_127;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_128_255     /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_128_255;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_256_511     /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_256_511;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_512_1023    /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_512_1023;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_1024_1518   /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_1024_1518;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfFramesSize_Above_1518  /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_Above_1518;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfOkBytes                /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfOkBytes;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfErrorBytes             /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfErrorBytes;
                    summedNetworkInterfaceStatisticsRx[networkInterface].NumberOfRxOverflowFrames       /**/ += statistics.RxStatisticsOfNetworkInterfaces[networkInterface].NumberOfRxOverflowFrames;

                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfOkFrames               /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfOkFrames;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfErrorFrames            /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfErrorFrames;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_1_63        /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_1_63;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_64          /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_64;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_65_127      /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_65_127;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_128_255     /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_128_255;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_256_511     /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_256_511;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_512_1023    /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_512_1023;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_1024_1518   /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_1024_1518;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfFramesSize_Above_1518  /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfFramesSize_Above_1518;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfOkBytes                /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfOkBytes;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfErrorBytes             /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfErrorBytes;
                    summedNetworkInterfaceStatisticsTx[networkInterface].NumberOfRxOverflowFrames       /**/ += statistics.TxStatisticsOfNetworkInterfaces[networkInterface].NumberOfRxOverflowFrames;
                }

                statistics.RxStatisticsOfNetworkInterfaces = &summedNetworkInterfaceStatisticsRx[0];
                statistics.TxStatisticsOfNetworkInterfaces = &summedNetworkInterfaceStatisticsTx[0];
            }

            memset(&dummy, 0, sizeof(dummy));
 	    system("clear");
 	    print_stat_port(HEADER_PROLOGUE, networkInterface, &statistics, &dummy, pDummyACL, headerPrintType, showNonzeroValueSeen);

            for (networkInterface = 0; networkInterface < statistics.NumberOfNetworkInterfaces; ++networkInterface)
            {
                const SC_NetworkInterfaceStatistics * s = &statistics.RxStatisticsOfNetworkInterfaces[networkInterface];
                print_stat_port(RX_PROLOGUE, networkInterface, &statistics, s, pACL, printType, showNonzeroValueSeen);

                s = &statistics.TxStatisticsOfNetworkInterfaces[networkInterface];
                print_stat_port(TX_PROLOGUE, networkInterface, &statistics, s, pACL, printType, showNonzeroValueSeen);
            }
            if (statistics.NumberOfNetworkInterfaces > 4)
            {
                for (networkInterface = 4; networkInterface < statistics.NumberOfNetworkInterfaces; ++networkInterface)
                {
                    const SC_NetworkInterfaceStatistics *s = &statistics.RxStatisticsOfNetworkInterfaces[networkInterface];
                    print_stat_port(RX_PROLOGUE, networkInterface, &statistics, s, pACL, printType, showNonzeroValueSeen);

                    s = &statistics.TxStatisticsOfNetworkInterfaces[networkInterface];
                    print_stat_port(TX_PROLOGUE, networkInterface, &statistics, s, pACL, printType, showNonzeroValueSeen);

                }
            }
            fflush(stdout);
            lastSeconds = statistics.Timestamp.Seconds;

            if (printType == PRINT_TYPE_VALUE)
            {
                if (statistics.Timestamp.Seconds - lastSeconds > 1)
                {
                    missedStatistics += statistics.Timestamp.Seconds - lastSeconds - 1;
                }
                if (missedStatistics > 0)
                {
                    fprintf(stderr, " *** ERROR: missed %lu statistics samples\n", missedStatistics);
                }

                if (once)
                {
                    stop_cond = once;
                }
            }

            /* First round runs with PRINT_TYPE_NONE so we just set initial field widths and maximum values. */
            headerPrintType = PRINT_TYPE_HEADER;
            printType = PRINT_TYPE_VALUE;
        }

        usleep(250 * 1000);
    }

    SC_CloseDevice(statDeviceId);

    return 0;
}
