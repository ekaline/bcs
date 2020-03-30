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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smartnic.h"

#define SIGNAL_NUMBER   SIGUSR1 /* Signal to use for signalling user logic interrupts. */

/**
 *  Signal handler that handles signals produced by SmartNIC driver user logic interrupts.
 *
 *  @param  signalNumber        Signal number.
 *  @param  pSignalInfo         Pointer to signal info structure.
 *  @param  pUContext           Pointer to an ucontext (not used by this handler).
 */
void SignalAction(int signalNumber, siginfo_t * pSignalInfo, void * pUContext)
{
    pUContext = pUContext; /* Avoid unused parameter warning */

    if (signalNumber == SIGNAL_NUMBER)
    {
        static int count = 0;

        SC_UserLogicInterruptResults userLogicInterruptResults;
        SC_Error errorCode = SC_GetUserLogicInterruptResults(pSignalInfo, &userLogicInterruptResults);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "SC_GetUserLogicInterruptsContext failed with error code %d", errorCode);
        }

        fprintf(stdout, "%2d: received signal %d from /dev/smartnic%u user logic channel %u, optional context is '%s'\n", 
            ++count, signalNumber, userLogicInterruptResults.DeviceMinorNumber, userLogicInterruptResults.UserLogicChannel,
            (const char *)userLogicInterruptResults.pContext);
    }
}

/**
 *  This is a small example program of user logic interrupt handling with a signal.
 */
int main(int argc, char * argv[])
{
    SC_Error errorCode;
    SC_UserLogicInterruptOptions userLogicInterruptOptions;
    uint64_t channel;
    SC_DeviceId deviceId;
    struct sigaction new_sigaction, old_sigaction;

    /* Avoid unused parameter warnings */
    argc = argc;
    argv = argv;

    /* Register signal handler */
    memset(&new_sigaction, 0, sizeof(new_sigaction));
    new_sigaction.sa_sigaction = SignalAction;
    new_sigaction.sa_flags = SA_SIGINFO;
    sigemptyset(&new_sigaction.sa_mask);
    if (sigaction(SIGNAL_NUMBER, &new_sigaction, &old_sigaction) != 0)
    {
        fprintf(stderr, "Failed to register signal handler\n");
        exit(EXIT_FAILURE);
    }

    deviceId = SC_OpenDevice(NULL, NULL);
    if (deviceId == NULL)
    {
        fprintf(stderr, "SC_OpenDevice failed with error code %d\n", SC_GetLastErrorCode());
        exit(EXIT_FAILURE);
    }

    /* Fill userLogicInterruptOptions structure with default values. */
    errorCode = SC_GetUserLogicInterruptOptions(deviceId, &userLogicInterruptOptions);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "SC_GetUserLogicInterruptOptions failed with error code %d", errorCode);
        exit(EXIT_FAILURE);
    }
    userLogicInterruptOptions.Pid = 0;                                      /* Fill in the process id of the process that receives the signal.
                                                                               Default zero means that the current process receives the signal. */
    userLogicInterruptOptions.SignalNumber = SIGNAL_NUMBER;                 /* User selectable signal number to use. SIGUSR1 is the default. */
    userLogicInterruptOptions.pContext = "User defined optional context.";  /* Optional user context that is delivered into the signal handler. */

    /* Using the SmartNIC demo FPGA, create user logic interrupt on each channel 0-7: */
    for (channel = 0; channel < 8; ++channel)
    {
        userLogicInterruptOptions.Mask = (SC_UserLogicInterruptMask)(1 << channel); /* Create interrupt enable mask for channel */

        /* Enable the selected interrupt channel(s). */
        errorCode = SC_EnableUserLogicInterrupts(deviceId, &userLogicInterruptOptions);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "SC_EnableUserLogicInterrupts failed with error code %d", errorCode);
            exit(EXIT_FAILURE);
        }

        /* Trigger the FPGA demo interrupt */
        errorCode = SC_ActivateDemoDesign(deviceId, SC_DEMO_USER_LOGIC_INTERRUPTS, channel, NULL);
        if (errorCode != SC_ERR_SUCCESS)
        {
            fprintf(stderr, "SC_ActivateDemoDesign failed with error code %d", errorCode);
            exit(EXIT_FAILURE);
        }

        /* Wait a millisecond to allow exception to be processed and signal handler called. */
        usleep(1000);
    }

    sleep(1); /* Wait for last signal(s) to arrive to signal handler. */

    /* Disable user logic interrupts on all channels */
    userLogicInterruptOptions.Mask = SC_USER_LOGIC_INTERRUPT_MASK_NONE;
    errorCode = SC_EnableUserLogicInterrupts(deviceId, &userLogicInterruptOptions);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "SC_EnableUserLogicInterrupts failed with error code %d", errorCode);
        exit(EXIT_FAILURE);
    }

    errorCode =SC_CloseDevice(deviceId);
    if (errorCode != SC_ERR_SUCCESS)
    {
        fprintf(stderr, "SC_CloseDevice failed with error code %d\n", errorCode);
        exit(EXIT_FAILURE);
    }

    return 0;
}
