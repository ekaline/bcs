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
 *  @file panic.c
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <asm/io.h>

#include "main.h"
#include "regs.h"
#include "fpga.h"

static bool panicPrint(const device_context_t * pDevExt, int PanicNo, uint64_t index, uint64_t ts)
{
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

    // Convert from special timestamp to nanoseconds. Timestamp is: 31..0 400 ps quants
    // We lose a little precision but avoid overflow.
    ns = (ns / 10) * 4;

    PRINTK_ERR("PanicBit:%d  Index:%d  TS:0x%016llX  Time: %d:%02d:%02d,%09d\n", PanicNo, (unsigned int)index, ts, h, m, s, ns);
    if (ts == 0xDEADBEEFDEADBEEFULL)
    {
        PRINTK_ERR("*** Wrong register access!\n");
        return false;
    }
    return true;
}

/**
 *  Reset panic controller and clear all panics.
 */
void reset_and_clear_all_panics(const device_context_t * pDevExt)
{
    writeq(15, pDevExt->bar0_va + FPGA_PANIC);
}

/**
 *  Reset panic value from an index.
 */
static uint64_t read_panic(const device_context_t * pDevExt, uint64_t index)
{
    uint64_t value;

    writeq(index, pDevExt->bar0_va + FPGA_PANIC);
    if (index == 0) // TODO FIXME: is this really necessary?
    {
        udelay(30); // This usually takes a bit of time
    }
    value = readq(pDevExt->bar0_va + FPGA_PANIC);
    return value;
}

int panicParse(device_context_t * pDevExt, int forceDecode)
{
    uint64_t enabled[2];
    //    uint64_t panic_qword1;
    uint64_t index[4];
    uint64_t ts[4];
    int i;
    uint64_t cur_index = 0;

    if (pDevExt->tcp_lane_mask != 0)
    {
        // There are one or more ETLMs present. Check that there are no (new!) ETLM debug assertions.

        static uint32_t previousDebugAssertions = 0;
        uint32_t debugAssertions = etlm_read_debug_assertions(pDevExt);
        if (debugAssertions != 0 && debugAssertions != previousDebugAssertions)
        {
            PRINTK_ERR("ETLM debug assertions = 0x%x\n", debugAssertions);
            previousDebugAssertions = debugAssertions;
        }
    }

    if (pDevExt->panicOccured && !forceDecode)
        return 0;

    // Read Panic enable
    enabled[0] = read_panic(pDevExt, 0);
    if (enabled[0] != 0)
    {
        // We have panic
        pDevExt->panicOccured = 1;

        // If all f's is read, the device is not responding
        if (enabled[0] == 0xffffffffffffffffULL)
            return 0;

        // Read Panic enable
        enabled[1] = read_panic(pDevExt, 1);

        // Read Panic index
        index[0] = read_panic(pDevExt, 2);
        index[1] = read_panic(pDevExt, 3);
        index[2] = read_panic(pDevExt, 4);
        index[3] = read_panic(pDevExt, 5);

        // Read Panic timestamp
        ts[0] = read_panic(pDevExt, 8);
        ts[1] = read_panic(pDevExt, 9);
        ts[2] = read_panic(pDevExt, 10);
        ts[3] = read_panic(pDevExt, 11);

        // Decode panic bits
        for (i = 0; i < 63; i++)// Loop through panic 0 -> 62
        {
            if ((enabled[0] & (1LL << i)) != 0)// Single panics out
            {
                // Read index
                if (i < 32)
                {
                    cur_index = (index[0] >> (i * 2)) & 3;// Mask out the 2 index bits
                }
                else if ((i >= 32) && (i < 63))
                {
                    cur_index = (index[1] >> ((i - 32) * 2)) & 3;// Mask out the 2 index bits
                }

                if (!panicPrint(pDevExt, i, cur_index, ts[cur_index]))// Print panics to screen
                {
                    return -1;
                }
            }
        }
        for (i = 0; i < 64; i++)// Loop through panic 63 -> 126
        {
            if ((enabled[1] & (1LL << i)) != 0)// Single panics out
            {
                // Read index
                if (i == 0)
                {
                    cur_index = (index[1] >> 62) & 3;// Mask out the 2 index bits
                }

                if ((i > 0) && (i < 33))
                {
                    cur_index = (index[2] >> (i * 2)) & 3;// Mask out the 2 index bits
                }
                else if ((i >= 33) && (i < 64))
                {
                    cur_index = (index[3] >> ((i - 33) * 2)) & 3;// Mask out the 2 index bits
                }

                if (!panicPrint(pDevExt, i + 63, cur_index, ts[cur_index]))// Print panics to screen
                {
                    return -1;
                }

            }
        }
    }
    return 0;
}
