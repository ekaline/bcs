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
 *  ACL related public definitions.
 *
 */

#pragma once

#include <linux/kernel.h>
#include <linux/types.h>

#define SMARTNIC_DRIVER

#include "ctls.h"           // Definition of SC_NO_NETWORK_INTERFACES

/**
 * ACL table entry
 */
typedef struct
{
    uint32_t                    ip;         ///< IP address white/black list (filter)
    uint32_t                    ip_mask;    ///< IP mask white/black list (filter)
    int                         port_min;   ///< UDP/TCP port lower limit (inclusive)
    int                         port_max;   ///< UDP/TCP port upper limit (inclusive)
} acl_entry;

#define ACL_ID_COUNT        2   // Total number of ACL identifiers (ACL_ID_TX and ACL_ID_RX)
#define ACL_TABLE_SZ        128 // Number of entries in the ACL table in FW

/**
 * ACL context data, all data needed to support ACL functionality.
 */
typedef struct
{
    const device_context_t *    pDevExt;                                                    ///< Pointer to device context
    uint16_t                    acl_nif_mask;                                               ///< Which network interfaces (0-15) have ACL supported in FPGA (bit mask).
    uint16_t                    enabled;                                                    ///< Which network interfaces (0-15) actually have ACL enabled.
    acl_entry                   table[ACL_ID_COUNT][SC_NO_NETWORK_INTERFACES][ACL_TABLE_SZ];     ///< Table containing both Rx and Tx ACL data for each network interface.
} acl_context_t;

int acl_reset_all(acl_context_t * this);
int sc_acl_cmd(acl_context_t * this, unsigned long data);
