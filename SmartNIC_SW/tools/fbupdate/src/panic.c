/*
 * panic.c
 *
 *  Created on: Sep 26, 2012
 *      Author: rdj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include "printlog.h"
#include "fpgaregs.h"

void panicPrint(int PanicNo, int index, uint64_t ts) {
    uint32_t ns;// Note that this is actually 400 ps quants
    uint32_t s = ts >> 32;
    uint32_t m = s / 60;
    uint32_t h = m / 60;

    s = ts >> 32;
    m = s / 60;
    h = m / 60;

    s %= 60;
    m %= 60;

    ns = ts & 0xFFFFFFFFLL;
    ns = ns/2.5;//Convert from special timestamp to nanoseconds. Timestamp is: 31..0 400 ps quants
    prn(LOG_SCR | LOG_FILE | LEVEL_ALL, "PanicBit:%d  Index:%d  TS:0x%016lX  Time: %d:%02d:%02d,%09d\n", PanicNo, index, ts, h, m, s, ns);
    return;
}

int panicParse(uint32_t *pRegistersBaseAddress) {
    uint64_t enabled[2];
    //    uint64_t panic_qword1;
    uint64_t index[4];
    uint64_t ts[4];
    int i;
    uint64_t cur_index;

    //Read Panic enable
    wr64(pRegistersBaseAddress, REG_PANIC, 0);
    enabled[0] = rd64(pRegistersBaseAddress, REG_PANIC);
    if (enabled[0] != 0) {
        //Read Panic enable
        wr64(pRegistersBaseAddress, REG_PANIC, 1);
        enabled[1] = rd64(pRegistersBaseAddress, REG_PANIC);

        //Read Panic index
        wr64(pRegistersBaseAddress, REG_PANIC, 2);
        index[0] = rd64(pRegistersBaseAddress, REG_PANIC);
        wr64(pRegistersBaseAddress, REG_PANIC, 3);
        index[1] = rd64(pRegistersBaseAddress, REG_PANIC);
        wr64(pRegistersBaseAddress, REG_PANIC, 4);
        index[2] = rd64(pRegistersBaseAddress, REG_PANIC);
        wr64(pRegistersBaseAddress, REG_PANIC, 5);
        index[3] = rd64(pRegistersBaseAddress, REG_PANIC);

        //Read Panic timestamp
        wr64(pRegistersBaseAddress, REG_PANIC, 8);
        ts[0] = rd64(pRegistersBaseAddress, REG_PANIC);
        wr64(pRegistersBaseAddress, REG_PANIC, 9);
        ts[1] = rd64(pRegistersBaseAddress, REG_PANIC);
        wr64(pRegistersBaseAddress, REG_PANIC, 10);
        ts[2] = rd64(pRegistersBaseAddress, REG_PANIC);
        wr64(pRegistersBaseAddress, REG_PANIC, 11);
        ts[3] = rd64(pRegistersBaseAddress, REG_PANIC);

        //Decode panic bits
        for (i = 0; i < 63; i++)//Loop through panic 0 -> 62
        {
            if ((enabled[0] & (1LL << i)) != 0)//Single panics out
            {
                //Read index
                if (i < 32){
                    cur_index = (index[0] >> (i * 2)) & 3;//Mask out the 2 index bits
                }else if ((i >= 32) && (i < 63)){
                    cur_index = (index[1] >> ((i - 32) * 2)) & 3;//Mask out the 2 index bits
                }
                panicPrint(i, cur_index, ts[cur_index]);// Print panics to screen
            }
        }
        for (i = 0; i < 64; i++)//Loop through panic 63 -> 126
        {
            if ((enabled[1] & (1LL << i)) != 0)//Single panics out
            {
                //Read index
                if (i == 0){
                    cur_index = (index[1] >> 62) & 3;//Mask out the 2 index bits
                }
                if ((i > 0) && (i < 33)){
                    cur_index = (index[2] >> (i * 2)) & 3;//Mask out the 2 index bits
                }else if ((i >= 33) && (i < 64)){
                    cur_index = (index[3] >> ((i - 33) * 2)) & 3;//Mask out the 2 index bits
                }
                panicPrint(i+63, cur_index, ts[cur_index]);// Print panics to screen

            }
        }
    } else {
        prn(LOG_SCR | LOG_FILE | LEVEL_ALL, "No panics detected\n");
    }
    return 0;
}

