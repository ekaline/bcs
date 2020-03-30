/*
 * cmd_shared.c
 *
 *  Created on: Mar 31, 2016
 *      Author: rdj
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#include "fpgaregs.h"
#include "flash.h"
#include "sysmon.h"
#include "printlog.h"
#include "actel.h"
#include "panic.h"
#include "actel_sf2.h"
#include "gecko.h"
#include "fbcapture_if.h"
#include "actel_gecko_flash.h"

#ifdef WIN32
    #include <windows.h>
    #define sleep(x)    Sleep(x)  /* sleep is defined by POSIX, not by C++ so we need Windows Sleep */
#endif

#include "FL_FlashLibrary.h"

#ifdef WIN32

extern int _printf(const char * format, ...);

#define printf _printf  /* Use _printf function to avoid compiler format warnings on mingw C++ compiler and -Wformat option */

#endif // WIN32

static int cmdPanic(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    int ret;
    ret = panicParse(pFlashLibraryContext->FlashInfo.pRegistersBaseAddress);
    return ret;
}

static int cmdFlowDebug(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = (uint32_t *)pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    int loops = 0;
#define COLUMNS 9
    double pct_val[COLUMNS];
    uint64_t val[COLUMNS];
    uint64_t diff;
    uint64_t prev_val[COLUMNS] = {0};
    char txtHdr[COLUMNS][8] = {
            "HdrCred",
            " PdCred",
            "  pFIFO",
            "    tsi",
            "    net",
            "    pci",
            "L0 err0",
            "L0 err1",
            "  txDMA",
    };
    int pct_mode = 0;
    int col_width = 10;
    int i, j;

    if((argc == 1) && (strcmp(argv[0], "pct")==0)){
        pct_mode = 1;
        col_width = 8;
    }

    // Print header
    printf("Idx ");// Add space for counter
    for(i=0; i<COLUMNS; i++){
        if(!pct_mode){
            for(j=0;j<col_width-8; j++){
                printf(" ");// Add leading space

            }
        }
        printf(" %s", txtHdr[i]);
    }
    printf("\n");
    printf("----");
    for(i=0; i<COLUMNS; i++){
        for(j=0; j<col_width; j++){
            printf("-");
        }
    }
    printf("\n");

    // Make initial read because we print a diff
    for(i=0; i<COLUMNS; i++){
        wr64(pRegistersBaseAddress, REG_PERF_MON, i);
        usleep(1);
        prev_val[i] = rd64(pRegistersBaseAddress, REG_PERF_MON);
    }
    sleep(1);
    while(1){
        printf("%3u ", loops);
        loops++;
        for(i=0; i<COLUMNS; i++){
            wr64(pRegistersBaseAddress, REG_PERF_MON, i);
            //usleep(1);
            val[i] = rd64(pRegistersBaseAddress, REG_PERF_MON);
        }
        for(i=0; i<COLUMNS; i++){
            diff = (val[i]-prev_val[i]) & 0xFFFFFFFF;
            if(pct_mode){
                pct_val[i] = (double)(diff)/2500000.;
                printf(" %7.2f", pct_val[i]);
            }else{
                printf(" %9lu", diff);
            }
            prev_val[i] = val[i];
        }
#if 0
        uint64_t curCredit = rd64(pRegistersBaseAddress, 0x68);
        if(pct_mode){
            printf(" %7lu %7lu\n", curCredit&0xFF, ((curCredit>>8)&0xFFF)*16);
        }else{
            printf(" %9lu %9lu\n", curCredit&0xFF, ((curCredit>>8)&0xFFF)*16);
        }
#else
        printf("\n");
#endif
        sleep(1);
    }
    return 0;
}

static int cmdReg(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = (uint32_t *)pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    uint64_t data;
    uint64_t reg;

    if( (argc == 2) && (strcmp("r", argv[0]) == 0) ) {
        reg = strtoull(argv[1], NULL, 0);
        printf("0x%llX\n", (unsigned long long)rd64(pRegistersBaseAddress, reg));
    }else if( (argc == 3) && (strcmp("w", argv[0]) == 0) ) {
        reg = strtoull(argv[1], NULL, 0);
        data = strtoull(argv[2], NULL, 0);
        printf("Writing 0x%llX to 0x%llX\n", (unsigned long long)data, (unsigned long long)reg);
        wr64(pRegistersBaseAddress, reg, data);
    }else{
        printf("Invalid reg command. Use reg r|w <reg> [data]\n");
        return -1;
    }
    return 0;
}

static int cmdReg2(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = (uint32_t *)mmap(0, FL_DEFAULT_FPGA_BAR2_REGISTERS_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pFlashLibraryContext->DeviceFileDescriptor, 0);
    uint64_t data;
    uint64_t reg;

    if (pRegistersBaseAddress == MAP_FAILED)
    {
        return FL_Error(&pFlashLibraryContext->ErrorContext, FL_ERROR_MMAP_FAILED, "Failed to map, errno %d ", errno);
    }
    
    if( (argc == 2) && (strcmp("r", argv[0]) == 0) ) {
        reg = strtoull(argv[1], NULL, 0);
        printf("0x%llX\n", (unsigned long long)rd64(pRegistersBaseAddress, reg));
    }else if( (argc == 3) && (strcmp("w", argv[0]) == 0) ) {
        reg = strtoull(argv[1], NULL, 0);
        data = strtoull(argv[2], NULL, 0);
        printf("Writing 0x%llX to 0x%llX\n", (unsigned long long)data, (unsigned long long)reg);
        wr64(pRegistersBaseAddress, reg, data);
    }else{
        printf("Invalid reg command. Use reg r|w <reg> [data]\n");
        return -1;
    }

    if (munmap(pRegistersBaseAddress, FL_DEFAULT_FPGA_BAR2_REGISTERS_MEMORY_SIZE) != 0)
    {
        return FL_Error(&pFlashLibraryContext->ErrorContext, FL_ERROR_MUNMAP_FAILED, "munmap of address %p failed with errno %d", pRegistersBaseAddress, errno);
    }
  
    return 0;
}

static int cmdReg32(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = (uint32_t *)pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    uint32_t data;
    uint32_t reg;

    if( (argc == 2) && (strcmp("r", argv[0]) == 0) ) {
        reg = strtoul(argv[1], NULL, 0);
        printf("0x%lX\n", (unsigned long)rd32(pRegistersBaseAddress, reg));
    }else if( (argc == 3) && (strcmp("w", argv[0]) == 0) ) {
        reg = strtoul(argv[1], NULL, 0);
        data = strtoul(argv[2], NULL, 0);
        printf("Writing 0x%lX to 0x%lX\n", (unsigned long)data, (unsigned long)reg);
        wr32(pRegistersBaseAddress, reg, data);
    }else{
        printf("Invalid reg command. Use reg32 r|w <reg> [data]\n");
        return -1;
    }
    return 0;
}

// Serial dump
static int cmdSerial(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;

    return AG_ReadSerial(pFlashInfo);
}

// Length Monitor
static int cmdLengthMonitor(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = (uint32_t *)pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    int i = 0;
    int j;
    uint64_t port = 0;
    //uint64_t packets = 0; // ELE comment: was written into but never used for anything
    uint64_t rd_data = 0;
    uint64_t rd_data2 = 0;
    uint64_t wr_addr = 0;
    uint64_t rd_addr = 0;
    uint64_t read_cnt = 0;
    uint64_t time = 0;
    uint64_t time_diff = 0;
    //unused! uint64_t last_time = 0; // ele: unused variable
    uint64_t rd_array[0x800][2];

    if(argc == 1){
        if( strcmp("clr", argv[0]) == 0 ){
            printf("Clear to be ready for new trigger\n");
            wr64(pRegistersBaseAddress, REG_LEN_MONITOR, (0xf<<28) | 17);
            return 0;
        }else if( strcmp("trig", argv[0]) == 0 ){
            printf("Forcing trigger\n");
            wr64(pRegistersBaseAddress, REG_LEN_MONITOR, (0xf<<28) | 16);
            return 0;
        } else if( strcmp("read", argv[0]) == 0 ){
            for(port=0;port<1;port++)
            {
                // Get number of packets
                wr64(pRegistersBaseAddress, REG_LEN_MONITOR, (0xf<<28) | (port<<1) | 1);
                usleep(1);
                /*packets =*/ rd64(pRegistersBaseAddress, REG_LEN_MONITOR); // ELE comment: was written into but never used for anything

                // Get write addr to be used next time
                wr64(pRegistersBaseAddress, REG_LEN_MONITOR, (0xf<<28) | (port<<1) | 0);
                usleep(1);
                wr_addr = rd64(pRegistersBaseAddress, REG_LEN_MONITOR);

                //printf("Port %lu packets:%lu  LW:0x%lx\n", port, packets, wr_addr);
                rd_addr = wr_addr;
                read_cnt = 0x800;

                // Read out all the data
                i=0;
                while(read_cnt--){
                    //printf("reads left %lu\n", read_cnt);
                    // Set read addr
                    wr64(pRegistersBaseAddress, REG_LEN_MONITOR, (0x1<<31) | (port<<28) | ((rd_addr+i) & 0x7ff));
                    usleep(1);
                    rd_array[i][0] = rd64(pRegistersBaseAddress, REG_LEN_MONITOR);
                    rd_array[i][1] = rd64(pRegistersBaseAddress, REG_LEN_MONITOR2);
                    i++;
                }

                for(i=0;i<0x800;i++){
                    rd_data = rd_array[i][0];
                    rd_data2 = rd_array[i][1];
                    time = rd_data>>32;
                    if(i<0x7ff){
                        time_diff = ((rd_array[i+1][0]>>32) - time)&0xFFFFFFFF;
                    } else {
                        time_diff = 0;
                    }
                    printf("%4u : %11lu %11lu - ", i, time, time_diff);
                    if(rd_data2 != 0xDEADBEEFDEADBEEF){
                        // Print packet status and length
                        //printf("%04lx %04lx ", (rd_data2>>48)&0xffff, (rd_data2>>32)&0xffff);
                        printf("%02lx ", (rd_data>>24)&0x3f);
                        printf("%02lx ", (rd_data>>16)&0x3f);
                        // Print extra bits
                        for(j=11;j>=0;j--){
                            printf("%lu ", (rd_data>>j)&1);
                            if(j%4==0)
                                printf(" ");
                            if(j==0 && (rd_data&1))
                                printf(" Tick");
                        }
                    }
#if 0
                    printf(" ");
                    for(j=31;j>=28;j--){
                        printf("%lu ", (rd_data>>j)&1);
                    }
                    printf("%4lu %4lu %2lu ", (rd_data>>14)&0x3fff, (rd_data>>5)&0x1ff, rd_data&0x1f);
                    switch(rd_data&0x1f){
                        case 0  : printf("IDLE"); break;
                        case 1  : printf("HEADER"); break;
                        case 2  : printf("NETWORK"); break;
                        case 3  : printf("MODE0_HDR"); break;
                        case 4  : printf("MODE0"); break;
                        case 5  : printf("MODE1"); break;
                        case 6  : printf("MODE2"); break;
                        case 7  : printf("MODE3"); break;
                        case 8  : printf("MODE4"); break;
                        case 9  : printf("TOVF0"); break;
                        case 10 : printf("TOVF1"); break;
                        case 11 : printf("GO_TO_REPEAT"); break;
                        case 12 : printf("REPEAT"); break;
                        case 13 : printf("DISCARD"); break;
                        case 14 : printf("RLS_AND_GO_TO_IDLE"); break;
                        case 15 : printf("GO_TO_IDLE"); break;
                        default : printf("UNKNOWN"); break;
                    }
#endif
                    printf("\n");
                    rd_addr = (rd_addr+1) & 0x7ff;
                }
            }
            return 0;
        }
    }
    printf("Missing arguments. Valid arguments are:\n");
    printf("clr  : Clear and be ready for new trigger.\n");
    printf("trig : Force a trigger instead of waiting for a panic.\n");
    printf("read : Read out the lengths. Only valid after a panic or a forced trigger.\n");

    return 0;
}

/*
 * Print Atmel Revision
 */
static int cmdAtmelRevision(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    const FL_FlashInfo * pFlashInfo = &pFlashLibraryContext->FlashInfo;
    char rev_str[20];
    uint32_t rev;
    rev = ActelScratchpadRevision(pFlashInfo, rev_str);
    if(rev){
        printf("Atmel Revision %s\n", rev_str);
        return 0;
    } else {
        printf("Atmel Revision UNKNOWN\n");
        return -1;
    }
}

static int cmdAdmin(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    return cmdSF2(pFlashLibraryContext, argc, argv);
}

void pciegt_color_print(float error_float){
    if(error_float==0){
        // Blue
        printf("\x1b[44m - ");
    }else if(error_float > 0){
        if(error_float<0.00001){
            // Light blue
            printf("\x1b[104m 6 ");
        }else if(error_float<0.0001){
            // Light green
            printf("\x1b[102m 5 ");
        }else if(error_float<0.001){
            // Yellow
            printf("\x1b[43m 4 ");
        }else if(error_float<0.01){
            // Light yellow
            printf("\x1b[103m 3 ");
        }else if(error_float<0.1){
            // Light red
            printf("\x1b[101m 2 ");
        }else{
            printf("\x1b[41m 1 ");
        }
    } else {
        // Red
        printf("\x1b[41m   ");
    }
    // No color
    printf("\x1b[0m");
}
/**
 * PCIe GT scan that is called recursively
 */
int pciegt_scan(uint32_t *pRegistersBaseAddress, float* ber_map, float max_error, int post, int min_post, int max_post, int pre, int min_pre, int max_pre, int dir, float last_error_float, int cursor_res, int reg_style, int stack_depth){
    float error_float;
    uint64_t error, last_error;
    int i;
    int ret = 0;
    int next_pre, next_post;

    prn(LOG_SCR | LEVEL_DEBUG, "Stack:%2u  ", stack_depth);
    //prn(LOG_SCR | LEVEL_DEBUG, "get_error:%2u, %2u\n", post, pre);
    //if(*(ber_map+pre*32+post) < 0){
    if((*(ber_map+pre*32+post) < 0) && (stack_depth<1000)){
        // Print current scan location
        // Go to print location
        printf("\033[%d;%dH", 0, 65);
        printf("%02u, %02u", post, pre);

        // Set pre cursor and post cursor with a timeout
        if(reg_style == 0){
            wr64(pRegistersBaseAddress, 0x28, 0x87ff0000|(post<<8) | pre | 0x8080);
        } else {
            wr32(pRegistersBaseAddress, 0x28, (1<<31) | (1<<30) | (1<<16) | (post<<8) | pre | 0x8080);
        }
        // Wait for settings to take effect.
        usleep(1000);
        // Read current error count
        last_error = rd64(pRegistersBaseAddress, REG_PERF_MON);
        usleep(cursor_res);
        error = rd64(pRegistersBaseAddress, REG_PERF_MON) - last_error;
        // Calculate error percentage between 0 and 1
        error_float = (float)error/(250*cursor_res);

        // Go to print location
        printf("\033[%d;%dH", pre+4, post*3+5);
        pciegt_color_print(error_float);


        fflush(stdout);

        // Quit if not running PCIe gen 3
        if((rd32(pRegistersBaseAddress, REG_PCIE_STAT) & 0xF) != 4){
            return -1;
        }

        // Update ber_map with new value
        *(ber_map+pre*32+post) = error_float;

        // If the error level is not rising go to the next level
        prn(LOG_SCR | LEVEL_DEBUG, "%2u, %2u: error:%1.8f\n", post, pre, error_float);
        if( (error_float <= last_error_float) || (error_float < max_error)){
            for(i=0; i<4; i++){
                next_post = post;
                next_pre = pre;
                switch (i) {
                    case 0: next_post++; break;
                    case 1: next_pre++;  break;
                    case 2: next_post--; break;
                    case 3: next_pre--;  break;
                    default:
                        break;
                }
                //Test out of bounds and that we don't go back to where we come from
                if((next_pre<min_pre) || (next_pre>max_pre) || (next_post<min_post) || (next_post>max_post) || (*(ber_map+next_pre*32+next_post) >= 0)){
                    //prn(LOG_SCR | LEVEL_DEBUG, "Pre(%d)/post(%d) out of bounds.\n", pre, post);
                }else{
                    //usleep(10000);

                    switch (i) {
                        case 0: prn(LOG_SCR | LEVEL_DEBUG, " go > \n"); break;
                        case 1: prn(LOG_SCR | LEVEL_DEBUG, " go v \n"); break;
                        case 2: prn(LOG_SCR | LEVEL_DEBUG, " go < \n"); break;
                        case 3: prn(LOG_SCR | LEVEL_DEBUG, " go ^ \n"); break;
                        default:
                            break;
                    }
                    // Recursive call
                    ret = pciegt_scan(pRegistersBaseAddress, ber_map, max_error, next_post, min_post, max_post, next_pre, min_pre, max_pre, i, error_float, cursor_res, reg_style, stack_depth+1);
                }
                // Return early on error
                if(ret){
                    return ret;
                }
            }
        } else {
            prn(LOG_SCR | LEVEL_DEBUG, " go back\n");
        }
    } else {
        prn(LOG_SCR | LEVEL_DEBUG, "%2u, %2u: Already scanned\n", post, pre);
    }
    return 0;
}

// Enforce integer range
int clamp( int value, int min, int max )
{
    return (value < min) ? min : (value > max) ? max : value;
}

static int cmdPciegt(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t *pRegistersBaseAddress = (uint32_t *)pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    uint64_t val;
    int base_pre, pre;
    int base_post, post;
    int min_pre = 0;
    int max_pre = 20;
    int min_post = 0;
    int max_post = 31;
    int cnt_good;
    float cnt_good_float;
    int cursor_res = 50000;//us
    float error_array[21][32];
    float error_float;
    float max_error = 0.0001;
    int argp=0;
    int plot = 0;
    int ret = 0;
    uint32_t rev;

    // Select what kind of register interface we have to the GTs
    int reg_style = 0;


    while(argc>argp) {
        if(strcmp("plot", argv[argp]) == 0){
            plot = 1;
        }
        else if(strcmp("--res", argv[argp]) == 0){
            argp++;
            if(argc>argp) {
                cursor_res = clamp(strtoul(argv[argp], NULL, 0)*1000, 1, 1000000);
            }
        }
        else if(strcmp("--min-pre", argv[argp]) == 0){
            argp++;
            if(argc>argp) {
                min_pre = clamp(strtoul(argv[argp], NULL, 0), 0, 20);
            }
        }
        else if(strcmp("--max-pre", argv[argp]) == 0){
            argp++;
            if(argc>argp) {
                max_pre = clamp(strtoul(argv[argp], NULL, 0), 0, 20);
            }
        }
        else if(strcmp("--min-post", argv[argp]) == 0){
            argp++;
            if(argc>argp) {
                min_post = clamp(strtoul(argv[argp], NULL, 0), 0, 31);
            }
        }
        else if(strcmp("--max-post", argv[argp]) == 0){
            argp++;
            if(argc>argp) {
                max_post = clamp(strtoul(argv[argp], NULL, 0), 0, 31);
            }
        }
        else if(strcmp("--max-error", argv[argp]) == 0){
            argp++;
            if(argc>argp) {
                max_error = strtof(argv[argp], NULL);
            }
        }
        else{
            printf("Wrong argument %s. Valid arguments are:\n", argv[argp]);
            printf("--res:       Set time(ms) at each sampling point\n");
            return 1;
        }
        argp++;
    }

    if(plot){
        // Decide on the register layout
        wr32(pRegistersBaseAddress, REG_VERSION, 0);
        usleep(1);
        rev = rd32(pRegistersBaseAddress, REG_VERSION);
        if(rev > 0x02091600){
            // Revision > 2.9.21.0
            reg_style = 1;
        } else {
            reg_style = 0;
        }

        // Quit if not running PCIe gen 3
        if((rd32(pRegistersBaseAddress, REG_PCIE_STAT) & 0xF) != 4){
            printf("ERROR: Scan only works when board is running PCIe gen 3\n");
            return -1;
        }

        // Clear error array
        for(pre=0;pre<21;pre++){
            for(post=0;post<32;post++){
                error_array[pre][post] = -1.0;
            }
        }
        // Write read command pcie GT
        if(reg_style == 0){
            wr64(pRegistersBaseAddress, 0x28, 0x07ff0000);
            usleep(1);
            val = rd64(pRegistersBaseAddress, 0x28);
        } else {
            wr32(pRegistersBaseAddress, 0x28, 0x40000000);
            usleep(1);
            val = rd32(pRegistersBaseAddress, 0x28);
        }


        base_pre = val&0xff;
        base_post = (val>>8)&0xff;

        printf("\033[2J"); // Clear screen
        printf("\033[%d;%dH", 0, 1);
        prn(LOG_SCR | LEVEL_INFO, "Silicom DK PCIe GT tweak from pre:%2u post:%2u   Scanning...\n", base_pre, base_post);

        // Print header for dynamic updated map
        prn(LOG_SCR | LEVEL_INFO, "Post");
        for(post=0; post<=31; post++){
            prn(LOG_SCR | LEVEL_INFO, "%2u ",post);
        }
        prn(LOG_SCR | LEVEL_INFO, "\n");
        prn(LOG_SCR | LEVEL_INFO, "Pre+");
        for(post=0; post<=31; post++){
            prn(LOG_SCR | LEVEL_INFO, "---");
        }
        prn(LOG_SCR | LEVEL_INFO, "\n");
        for(pre=0; pre<=20; pre++){
            prn(LOG_SCR | LEVEL_INFO, "%2u |\n", pre);
        }
        // Flush to make sure that the header is always visible even when we die during print.
        fflush(stdout);
        usleep(10);

        // Start recursive scan
        ret = pciegt_scan(pRegistersBaseAddress, &error_array[0][0], max_error, base_post, min_post, max_post, base_pre, min_pre, max_pre, 0, 0.5, cursor_res, reg_style, 0);

        // Restore Precursor and post cursor
        if(reg_style == 0){
            wr64(pRegistersBaseAddress, 0x28, 0x87ff0000);
        } else {
            wr32(pRegistersBaseAddress, 0x28, (1<<31) | (1<<30) | 0);
        }

        if(ret){
            printf("\033[%d;%dH", 24, 1);
            printf("ERROR: PCIe speed dropped from gen 3   \n");
                    //Reset fbcapture drivers copy of cursors if driver /proc file exists
                    fbcapture_cursor_reset();
            return -1;
        }
        printf("\033[%d;%dH", 2, 1);
        // Print header for static map
        printf("\033[2J"); // Clear screen
        printf("\033[%d;%dH", 0, 1);
        prn(LOG_SCR | LEVEL_INFO, "Silicom DK PCIe GT tweak from pre:%2u post:%2u\n", base_pre, base_post);
        // Print Post header
        prn(LOG_SCR | LEVEL_INFO, "Post");
        for(post=0; post<=31; post++){
            prn(LOG_SCR | LEVEL_INFO, "%2u ",post);
        }
        prn(LOG_SCR | LEVEL_INFO, "Width Float\n");
        prn(LOG_SCR | LEVEL_INFO, "Pre+");
        for(post=0; post<=31; post++){
            prn(LOG_SCR | LEVEL_INFO, "---");
        }
        prn(LOG_SCR | LEVEL_INFO, "\n");
        for(pre=0; pre<=20; pre++){
            prn(LOG_SCR | LEVEL_INFO, "%2u |", pre);
            cnt_good = 0;
            cnt_good_float = 0.0;
            for(post=0; post<=31; post++){
                error_float = error_array[pre][post];
                // Count good
                if(error_float>=0){
                    cnt_good_float += 1-error_float;
                }
                if(error_float==0){
                    cnt_good++;
                }

                pciegt_color_print(error_float);
            }
            // No color
            printf("\x1b[0m");
            printf(" %2u   %1.1f\n", cnt_good, cnt_good_float);
        }
        // Print nomenclature
        pciegt_color_print(-1.0);
        printf(" no scan\n");
        // Blue
        pciegt_color_print(0.0);
        printf(" no error\n");

        pciegt_color_print(0.2);
        printf(" error > 1E-1\n");

        pciegt_color_print(0.02);
        printf(" error > 1E-2\n");

        pciegt_color_print(0.002);
        printf(" error > 1E-3\n");

        pciegt_color_print(0.0002);
        printf(" error > 1E-4\n");

        pciegt_color_print(0.00002);
        printf(" error > 1E-5\n");

        pciegt_color_print(0.000002);
        printf(" error > 1E-6\n");
                
        //Reset fbcapture drivers copy of cursors if driver /proc file exists
        fbcapture_cursor_reset();
    }
    return 0;
}

//Print the licenses from card with fbcapture image
//Code is from cardstat.cpp and physicalcard.cpp
static int cmdPrintLicense(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv)
{
    uint32_t * baseAddr = pFlashLibraryContext->FlashInfo.pRegistersBaseAddress;
    const int FB_LICENSE_LEN = 12;
    const int FPGA_REVISION = 0; //register address
    typedef uint8_t FBLicense[FB_LICENSE_LEN];
    typedef union
    {
        struct
        {
            uint32_t    licenseMember[3];   // 32 license chuncks as read from fw
        } Split;
        FBLicense licenseOut;
    } LicenseCombi;
    LicenseCombi License;

    //read licenses
    memset(&License, 0, FB_LICENSE_LEN);
    wr64(baseAddr, FPGA_REVISION, 8); // SubAddr for License status
    usleep(1);
    uint64_t licenseReg = rd64(baseAddr, FPGA_REVISION);
    if ((licenseReg & 0x6ULL) == 0x6ULL)
    {
        //new license, ready and valid
        License.Split.licenseMember[0] = licenseReg >> 32;
        wr64(baseAddr, FPGA_REVISION, 9); // SubAddr for License 2
        usleep(1);
        licenseReg = rd64(baseAddr, FPGA_REVISION);
        License.Split.licenseMember[1] = licenseReg & 0xffffffff;
        License.Split.licenseMember[2] = licenseReg >> 32;
    }
    else
    {
        //old license maybe, return only bit 0
        License.licenseOut[0] = licenseReg & 0x1;
    }
    wr64(baseAddr, FPGA_REVISION, 0); // SubAddr back

    //present licenses
    uint32_t lic_no = 0;
    uint32_t i;
    for (i = 0; i < FB_LICENSE_LEN; i++)
    {
        uint8_t lic_byte = License.licenseOut[i];
        uint32_t j;
        for (j = 0; j < 8; j++)
        {
            if ((lic_byte & 1) == 1)
            {
                switch (lic_no) {
                    case 0:
                        printf("   Session based GTP Distribution\n");
                        break;
                    case 1:
                        printf("   FBGTPD_20\n");
                        break;
                    case 2:
                        printf("   FBGTPD_40\n");
                        break;
                    case 3:
                        printf("   FBGTPD_80\n");
                        break;
                    case 4:
                        printf("   FBGTPD_120\n");
                        break;
                    case 5:
                        printf("   FBGTPD_160\n");
                        break;
                    default:
                        printf("   License %d\n", lic_no);
                        break;
                }
            }
            lic_byte = lic_byte >> 1;
            ++lic_no;
        }
    }
    return 0;
}

/* Array of commands */
static const FL_CommandEntry s_SharedCommandList[] =
{
  //  CommandName           pCommandFunction        InitializationLevel              HelpText
    { "bf",                 cmdActel,               FL_INITIALIZATION_LEVEL_FULL,    "Boot flash commands.\n" },
    { "adm",                cmdAdmin,               FL_INITIALIZATION_LEVEL_FULL,    "\nSynonym for sf2.\n" },
    { "admin",              cmdAdmin,               FL_INITIALIZATION_LEVEL_FULL,    "\nSynonym for sf2.\n" },
    { "sf2",                cmdAdmin,               FL_INITIALIZATION_LEVEL_FULL,    "Deprecated command, use adm or admin instead.\n" },
    { "gecko",              cmdGecko,               FL_INITIALIZATION_LEVEL_FULL,    "\n" },
    { "reg",                cmdReg,                 FL_INITIALIZATION_LEVEL_MAPPING, "Read or write to a 64-bit FPGA register.\n" },
    { "reg2",               cmdReg2,                FL_INITIALIZATION_LEVEL_MAPPING, "Read or write to a 64-bit FPGA register in BAR2 (if no BAR2, BAR0 is used).\n" },
    { "reg32",              cmdReg32,               FL_INITIALIZATION_LEVEL_MAPPING, "Read or write to a 32-bit FPGA register.\n" },
    { "panic",              cmdPanic,               FL_INITIALIZATION_LEVEL_MAPPING, "Show card panic status.\n" },
    { "flowdebug",          cmdFlowDebug,           FL_INITIALIZATION_LEVEL_MAPPING, "\n" },
    { "serial",             cmdSerial,              FL_INITIALIZATION_LEVEL_FULL,    "Show card serial number.\n" },
    { "pcie-gt",            cmdPciegt,              FL_INITIALIZATION_LEVEL_MAPPING, "\n" },
    { "atmelrevision",      cmdAtmelRevision,       FL_INITIALIZATION_LEVEL_FULL,    "\n" },
    { "lenmon",             cmdLengthMonitor,       FL_INITIALIZATION_LEVEL_MAPPING, "\n" },
    { "license",            cmdPrintLicense,        FL_INITIALIZATION_LEVEL_MAPPING, "\n" },
    { NULL,                 NULL,                   FL_INITIALIZATION_LEVEL_NONE, NULL } /* Needed as terminating record */
};

FL_Error AddSharedCommands(FL_FlashLibraryContext * pFlashLibraryContext)
{
    return FL_AddCommands(pFlashLibraryContext, s_SharedCommandList);
}
