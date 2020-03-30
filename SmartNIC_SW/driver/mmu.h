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
 *  MMU related public definitions.
 */

#pragma once

#include <linux/kernel.h>
#include <linux/types.h>
#include <stdbool.h>

typedef struct __mmu_context__ mmu_context_t;

mmu_context_t * mmu_new(const device_context_t * pDevExt, struct pci_dev * pPciDevice, unsigned long totalBufferSize,
    unsigned long numberOfEntries, uint64_t * pAvailableEntrySize, const char * bufferName);
void mmu_delete(mmu_context_t * this);
void mmu_enable(mmu_context_t * this, bool enable);
const dma_addr_t * mmu_get_dma_channel_physical_addresses(const mmu_context_t * this, uint16_t dmaChannelNumber,
    size_t * pNumberOfPhysicalAddresses, size_t * pOffsetToNextPhysicalAddress, size_t * pLengthOfSingleAllocation);
void * mmu_allocate(mmu_context_t * this, uint16_t dmaChannel, size_t length, dma_addr_t * pPhysicalAddress, dma_addr_t * pVirtualStartAddress);
void mmu_free(mmu_context_t * this, uint16_t dmaChannel, size_t length, void * virtualAddress, dma_addr_t physicalAddress);
