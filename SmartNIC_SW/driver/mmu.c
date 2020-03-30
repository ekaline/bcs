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
 *  MMU implementation.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/types.h>

#include "main.h"
#include "mmu.h"
#include "fpga.h"

 /*****************************************************************************/

  /**
  * MMU table block. Defines parameters of a single continuous memory block.
  */
typedef struct
{
    size_t          Length;                 /**< Length of allocated DMA memory block. */
    int16_t         DmaChannel;             /**< DMA channel that owns this memory block. Value is only valid if @ref mmu_memory_block_t::Used is true. */
    void *          HostVirtualAddress;     /**< Host virtual address of allocated DMA memory block. */
    dma_addr_t      HostPhysicalAddress;    /**< Host physical address of allocated DMA memory block. */
    uint64_t        MmuVirtualAddress;      /**< Virtual address in the MMU of this entry. */
    uint64_t        MmuBusAddress;          /**< Bus address in the MMU of this entry. */
    uint64_t        CumulativeOffset;       /**< This is the sum of all entry lengths before this one. */
    bool            Used;                   /**< True if memory block has been used, false otherwise. */

} mmu_memory_block_t;

/*****************************************************************************/

/**
 * MMU context data, all data and functions needed to support MMU functionality.
 */
struct __mmu_context__
{
    const device_context_t *    pDevExt;                        /**< Pointer to device context */
    struct pci_dev *            pPciDevice;                     /**<  Pointer to a PCIe device. */
    unsigned long               TotalBufferSize;                /**< Total buffer size requested. */
    unsigned long               NumberOfEntries;                /**< Number of entries in total buffer requested. */
    unsigned long               SingleEntrySize;                /**< Size of a single entry computed from the above values. */
    uint16_t                    MaximumNumberOfMmuMemoryBlocks; /**< Maximum number of mapping memory blocks in the MMU. */
    unsigned long               MinimumTotalBufferSize;         /**< Minimum total buffer size supported by the MMU. */
    unsigned long               MaximumTotalBufferSize;         /**< Maximum total buffer size supported by the MMU. */
    unsigned long               TotalBufferSizeIsMultipleOf;    /**< Total buffer size must be multiple of this value. */
    size_t                      StartMemorySizeOfAllocation;    /**< Start memory size of memory allocations. Allocated memory size
                                                                     halved until allocation succeeds or minimum value of @ref
                                                                     mmu_context_t::EndMemorySizeOfAllocation is breached. */
    size_t                      EndMemorySizeOfAllocation;      /**< End memory size of memory allocations. */

    size_t                      TotalSizeOfMmuEntriesInBytes;   /**< Total size of the @ref mmu_context_t::MmuMemoryBlocks table as bytes. */
    uint16_t                    NumberOfMmuMemoryBlocks;        /**< Number of entries in the @ref mmu_context_t::MmuMemoryBlocks table. */
    size_t                      TotalMemoryAllocatedInBytes;    /**< Size of total allocated memory in bytes */
    size_t                      NumberOfMemoryBlocksPerEntry;   /**< Number of physical memory blocks used for one requested entry. */
    mmu_memory_block_t *        MmuMemoryBlocks;                /**< Table of allocated MMU memory blocks. */
    bool                        MmuEnabled;                     /**< True if MMU has been enabled, false otherwise. */
};

/*****************************************************************************/

static uint64_t RoundToMultipleOf(uint64_t address, uint64_t multipleOf, bool roundUp)
{
    uint64_t multiples = address / multipleOf;
    uint64_t roundedUpValue = multiples * multipleOf;
    if (roundedUpValue != address && roundUp)
    {
        roundedUpValue += multipleOf;
    }
    return roundedUpValue;
}

/*****************************************************************************/

unsigned long mmu_validate_total_buffer_size(mmu_context_t * this, unsigned long totalBufferSize, const char * bufferName)
{
    if (totalBufferSize > this->MaximumTotalBufferSize)
    {
        printk(KERN_ERR MODULE_NAME " *** Maximum value of %s is %lu (MB)."
            " Value is set down to that from %lu\n", bufferName, this->MaximumTotalBufferSize, totalBufferSize);
        totalBufferSize = this->MaximumTotalBufferSize;
    }
    else if (totalBufferSize < this->MinimumTotalBufferSize)
    {
        printk(KERN_ERR MODULE_NAME " *** Minimum value of %s is %lu (MB) with MMU enabled."
            " Value is set up to that from %lu\n", bufferName, this->MinimumTotalBufferSize, totalBufferSize);
        totalBufferSize = this->MinimumTotalBufferSize;
    }
    else
    {
        unsigned long roundedTotalBufferSize = RoundToMultipleOf(totalBufferSize, this->TotalBufferSizeIsMultipleOf, false);
        if (totalBufferSize != roundedTotalBufferSize)
        {
            printk(KERN_ERR MODULE_NAME " *** Value %lu of %s has to be a multiple of %lu (MB)."
                " Value is rounded down to %lu\n", totalBufferSize, bufferName, this->TotalBufferSizeIsMultipleOf, roundedTotalBufferSize);
            totalBufferSize = roundedTotalBufferSize;
        }
    }

    return totalBufferSize;
}


/*****************************************************************************/

const uint64_t MMU_VIRTUAL_ADDRESS_START = 1;       /**< Start of MMU virtual addresses. Use 1 because 0 causes problems in PL DMA */

/*****************************************************************************/

static int AllocateMemory(mmu_context_t * this)
{
    int                         rc = 0;
    const device_context_t *    pDevExt;
    size_t                      totalMmuEntriesAllocatedInBytes = 0;
    mmu_memory_block_t          mmuMemoryBlock;
    uint64_t                    mmuVirtualAddress = MMU_VIRTUAL_ADDRESS_START;
    uint64_t                    cumulativeOffset = 0;

    if (this == NULL)
    {
        return -ENOMEM;
    }

    memset(&mmuMemoryBlock, 0, sizeof(mmuMemoryBlock));

    pDevExt = this->pDevExt;
    mmuMemoryBlock.Length = this->StartMemorySizeOfAllocation;

    while (pDevExt->mmuSupport)
    {
        if (mmuMemoryBlock.Length < this->EndMemorySizeOfAllocation)
        {
            PRINTK_WARN("Failed to allocate DMA memory bigger or equal to %lu bytes\n", this->EndMemorySizeOfAllocation);
            rc = -ENOMEM;
            break;
        }
        mmuMemoryBlock.HostVirtualAddress = pci_alloc_consistent(this->pPciDevice, mmuMemoryBlock.Length, &mmuMemoryBlock.HostPhysicalAddress);
        if (mmuMemoryBlock.HostVirtualAddress == NULL)
        {
            PRINTK_C(LOG(LOG_MMU), "Halving MMU allocation size from %lu to %lu\n", mmuMemoryBlock.Length, mmuMemoryBlock.Length / 2);

            mmuMemoryBlock.Length /= 2; /* Next time try half of the length */
        }
        else
        {
            /* Zero allocated memory block */
            memset(mmuMemoryBlock.HostVirtualAddress, 0, mmuMemoryBlock.Length);

            /* Initialize MMU memory block information */
            mmuMemoryBlock.MmuVirtualAddress = mmuVirtualAddress++;
            mmuMemoryBlock.MmuBusAddress = mmuMemoryBlock.HostPhysicalAddress / this->EndMemorySizeOfAllocation;
            mmuMemoryBlock.CumulativeOffset = cumulativeOffset;
            mmuMemoryBlock.Used = false;

            cumulativeOffset += mmuMemoryBlock.Length;

            PRINTK_C(LOG(LOG_MMU), "%3u: Allocated %lu bytes of MMU DMA memory, va 0x%llX pa 0x%09llX MMU va 0x%03llX ba 0x%llX\n",
                this->NumberOfMmuMemoryBlocks, mmuMemoryBlock.Length, (uint64_t)mmuMemoryBlock.HostVirtualAddress, mmuMemoryBlock.HostPhysicalAddress,
                mmuMemoryBlock.MmuVirtualAddress, mmuMemoryBlock.MmuBusAddress);

            this->TotalMemoryAllocatedInBytes += mmuMemoryBlock.Length;

            if (this->NumberOfMmuMemoryBlocks < this->MaximumNumberOfMmuMemoryBlocks - 1)
            {
                totalMmuEntriesAllocatedInBytes += sizeof(mmuMemoryBlock);
                if (totalMmuEntriesAllocatedInBytes >= this->TotalSizeOfMmuEntriesInBytes)
                {
                    size_t totalSizeOfMmuEntriesInBytes = this->TotalSizeOfMmuEntriesInBytes;
                    const mmu_memory_block_t * mmuEntries = this->MmuMemoryBlocks;
                    this->TotalSizeOfMmuEntriesInBytes *= 2; /* double the size */
                    this->MmuMemoryBlocks = (mmu_memory_block_t *)kmalloc(this->TotalSizeOfMmuEntriesInBytes, GFP_KERNEL);
                    memset(this->MmuMemoryBlocks, 0, this->NumberOfMmuMemoryBlocks * sizeof(this->MmuMemoryBlocks[0]));
                    memcpy(this->MmuMemoryBlocks, mmuEntries, totalSizeOfMmuEntriesInBytes);
                }

                if (this->NumberOfMmuMemoryBlocks >= this->MaximumNumberOfMmuMemoryBlocks - 1)
                {
                    PRINTK_WARN("Exceeded maximum number of MMU memory blocks %u, allocated memory is only %lu bytes (%lu KB)",
                        this->NumberOfMmuMemoryBlocks, this->TotalMemoryAllocatedInBytes, this->TotalMemoryAllocatedInBytes / 1024);
                    rc = -ENOMEM;
                    break;
                }

                this->MmuMemoryBlocks[this->NumberOfMmuMemoryBlocks++] = mmuMemoryBlock;

                if (this->TotalMemoryAllocatedInBytes >= this->TotalBufferSize)
                {
                    break;
                }
            }
            else
            {
                PRINTK_WARN("Exceeded maximum number of MMU memory blocks %u, allocated memory is only %lu bytes (%lu KB)",
                    this->MaximumNumberOfMmuMemoryBlocks, this->TotalMemoryAllocatedInBytes, this->TotalMemoryAllocatedInBytes / 1024);
                rc = -ENOMEM;
                break;
            }
        }
    }

    PRINTK_C(LOG(LOG_MMU), "Allocated total of %lu bytes MMU DMA memory\n", this->TotalMemoryAllocatedInBytes);

    return rc;
}

/*****************************************************************************/

const dma_addr_t * mmu_get_dma_channel_physical_addresses(
    const mmu_context_t *       this,
    uint16_t                    dmaChannelNumber,
    size_t *                    pNumberOfPhysicalAddresses,
    size_t *                    pOffsetToNextPhysicalAddress,
    size_t *                    pLengthOfSingleAllocation)
{
    if (this != NULL)
    {
        const dma_addr_t * pPhysicalAddresses = &this->MmuMemoryBlocks[dmaChannelNumber * this->NumberOfMemoryBlocksPerEntry].HostPhysicalAddress;

        *pNumberOfPhysicalAddresses = this->NumberOfMemoryBlocksPerEntry;
        *pOffsetToNextPhysicalAddress = &this->MmuMemoryBlocks[1].HostPhysicalAddress - &this->MmuMemoryBlocks[0].HostPhysicalAddress;
        *pLengthOfSingleAllocation = this->EndMemorySizeOfAllocation;

        if (LOG(LOG_MMU))
        {



            const device_context_t *    pDevExt = this->pDevExt;
            size_t                      i;
            const char *                delimiter = "";

            PRINTK_C(LOG(LOG_MMU), "DMA channel %u numberOfPhysicalAddresses = %lu, offsetToNext = %lu, alloc length = %lu, physicalAddresses: ",
                dmaChannelNumber, *pNumberOfPhysicalAddresses, *pOffsetToNextPhysicalAddress, *pLengthOfSingleAllocation);
            for (i = 0; i < this->NumberOfMemoryBlocksPerEntry; ++i)
            {
                PRINTK_("%s0x%llX", delimiter, pPhysicalAddresses[i]);
                delimiter = ", ";
            }
            PRINTK_("\n");
        }

        return pPhysicalAddresses;
    }
    return NULL;
}

/*****************************************************************************/

/**
 *  Allocates and constructs a new MMU instance.
 *
 *  @param  pDevExt             Pointer to device context. Only things referred to is the minor member.
 *  @param  pPciDevice          Pointer to a Linux PCIe device for this device.
 *  @param  totalBufferSize     Total PRB buffer size requested for this device for all DMA channels.
 *  @param  numberOfEntries     Total number of entries requested, one for each DMA channel.
 *  @param  pAvailableEntrySize Pointer to a value which receives the actual single entry size.
 *  @param  bufferName          Name for this buffer space to use in logging.
 *
 *  @return                     Pointer to the new MMU instance.
 */
mmu_context_t * mmu_new(
    const device_context_t *    pDevExt,
    struct pci_dev *            pPciDevice,
    unsigned long               totalBufferSize,
    unsigned long               numberOfEntries,
    uint64_t *                  pAvailableEntrySize,
    const char *                bufferName)
{
    const unsigned MaxNumberOfChannels = SC_MAX_CHANNELS/* - SC_MAX_UNUSED_CHANNELS - SC_MAX_MONITOR_CHANNELS*/;

    mmu_context_t * this = (mmu_context_t *)kmalloc(sizeof(*this), GFP_KERNEL);
    if (this == NULL)
    {
        PRINTK_ERR("Failed to allocate MMU context, out of memory?\n");
        return NULL;
    }

    memset(this, 0, sizeof(*this));

    this->pDevExt = pDevExt;
    this->pPciDevice = pPciDevice;
    this->TotalBufferSize = totalBufferSize;
    this->NumberOfEntries = numberOfEntries;
    if (numberOfEntries > 0)
    {
        this->SingleEntrySize = totalBufferSize / numberOfEntries;
    }
    else
    {
        this->SingleEntrySize = 0;
    }
    this->TotalBufferSize *= 1024 * 1024;
    this->SingleEntrySize *= 1024 * 1024;

    this->MaximumNumberOfMmuMemoryBlocks    = 8 * 1024;
    this->MinimumTotalBufferSize            = 4 * MaxNumberOfChannels; /* Have to allocate at least a minimum of 4 MB per channel */
    this->MaximumTotalBufferSize            = 32 * 1024;
    this->TotalBufferSizeIsMultipleOf       = 512;
    this->StartMemorySizeOfAllocation       = RECV_DMA_SIZE;
    this->EndMemorySizeOfAllocation         = RECV_DMA_SIZE;
    this->TotalSizeOfMmuEntriesInBytes      = MaxNumberOfChannels * sizeof(this->MmuMemoryBlocks[0]);
    this->NumberOfMmuMemoryBlocks           = 0;
    this->TotalMemoryAllocatedInBytes       = 0;
    this->MmuMemoryBlocks                   = (mmu_memory_block_t *)kmalloc(this->TotalSizeOfMmuEntriesInBytes, GFP_KERNEL);
    memset(this->MmuMemoryBlocks, 0, this->TotalSizeOfMmuEntriesInBytes);

    if (pDevExt != NULL)
    {
        AllocateMemory(this);

        this->NumberOfMemoryBlocksPerEntry = this->NumberOfMmuMemoryBlocks / SC_MAX_CHANNELS;

        PRINTK_C(LOG(LOG_MMU), "%lu MMU memory blocks of length 0x%lX per entry\n", this->NumberOfMemoryBlocksPerEntry, this->EndMemorySizeOfAllocation);

        if (pAvailableEntrySize != NULL)
        {
            *pAvailableEntrySize = this->NumberOfMemoryBlocksPerEntry * this->EndMemorySizeOfAllocation;
        }

        PRINTK_C(LOG(LOG_MMU), "MMU initialized: 0x%lX bytes total memory allocated for %u MMU entries, each mapped entry is 0x%llX bytes and %lu memory blocks\n",
            this->TotalMemoryAllocatedInBytes, this->NumberOfMmuMemoryBlocks, pAvailableEntrySize != NULL ? *pAvailableEntrySize : 0UL, this->NumberOfMemoryBlocksPerEntry);
    }

    this->TotalBufferSize = mmu_validate_total_buffer_size(this, totalBufferSize, bufferName);

    return this;
}

/*****************************************************************************/

/**
 *  Destructs and deletes an MMU instance.
 *
 *  @param  this        Pointer to MMU instance to be deleted.
 */
void mmu_delete(mmu_context_t * this)
{
    const device_context_t *    pDevExt = this->pDevExt;

    if (this == NULL)
    {
        PRINTK_ERR("Cannot delete nonexistent MMU context!\n");
        return;
    }

    enable_mmu(pDevExt, false);

    if (this->MmuMemoryBlocks != NULL)
    {
        size_t                      i;
        size_t                      totalMemoryFreed = 0;

        for (i = 0; i < this->NumberOfMmuMemoryBlocks; ++i)
        {
            const mmu_memory_block_t * pMmuMemoryBlock = &this->MmuMemoryBlocks[i];

            if (pMmuMemoryBlock->Used)
            {
                PRINTK_ERR("Freeing used MMU memory block: entry %lu, length 0x%lX va 0x%llX pa 0x%llX\n",
                    i, pMmuMemoryBlock->Length, (uint64_t)pMmuMemoryBlock->HostVirtualAddress, pMmuMemoryBlock->HostPhysicalAddress);
            }

            pci_free_consistent(this->pPciDevice, pMmuMemoryBlock->Length, pMmuMemoryBlock->HostVirtualAddress, pMmuMemoryBlock->HostPhysicalAddress);
            totalMemoryFreed += pMmuMemoryBlock->Length;

            PRINTK_C(LOG(LOG_MMU), "%3lu: Freed %lu bytes of MMU DMA memory, va 0x%llX pa 0x%09llX MMU va 0x%03llX ba 0x%llX\n",
                i, pMmuMemoryBlock->Length, (uint64_t)pMmuMemoryBlock->HostVirtualAddress, pMmuMemoryBlock->HostPhysicalAddress,
                pMmuMemoryBlock->MmuVirtualAddress, pMmuMemoryBlock->MmuBusAddress);
        }

        kfree(this->MmuMemoryBlocks);

        PRINTK_C(LOG(LOG_MMU), "Freed total of %lu bytes MMU DMA memory\n", totalMemoryFreed);
    }

    memset(this, 0, sizeof(*this));

    kfree(this);
}

/*****************************************************************************/

void mmu_enable(mmu_context_t * this, bool enable)
{
    if (this != NULL)
    {
        const device_context_t *    pDevExt = this->pDevExt;

        if (enable)
        {
            uint16_t                    i;

            for (i = 0; i < this->NumberOfMmuMemoryBlocks; ++i)
            {
                set_mmu(pDevExt, this->MmuMemoryBlocks[i].MmuVirtualAddress, this->MmuMemoryBlocks[i].MmuBusAddress);
            }
        }

        enable_mmu(pDevExt, enable);
        this->MmuEnabled = enable;
    }
}

/*****************************************************************************/

void * mmu_allocate(mmu_context_t * this, uint16_t dmaChannel, size_t length, dma_addr_t * pPhysicalAddress, dma_addr_t * pMmuStartAddress)
{
    const device_context_t *    pDevExt = this->pDevExt;
    void *                      virtualAddress;
    size_t                      i;
    mmu_memory_block_t *        pMmuMemoryBlocks = &this->MmuMemoryBlocks[this->NumberOfMemoryBlocksPerEntry * dmaChannel];

    if (this == NULL)
    {
        PRINTK_ERR("Cannot allocate from nonexistent MMU context!\n");
        return NULL;
    }

    *pPhysicalAddress = pMmuMemoryBlocks->HostPhysicalAddress;
    *pMmuStartAddress = pMmuMemoryBlocks->MmuVirtualAddress * this->EndMemorySizeOfAllocation;
    //virtualAddress = (void *)pMmuMemoryBlocks->MmuVirtualAddress;
    virtualAddress = pMmuMemoryBlocks->HostVirtualAddress;

    for (i = 0; i < this->NumberOfMemoryBlocksPerEntry; ++i)
    {
        if (pMmuMemoryBlocks[i].Used)
        {
            PRINTK_ERR("Memory block for DMA channel %u has already been used!\n", dmaChannel);
            return NULL;
        }

        pMmuMemoryBlocks[i].DmaChannel = dmaChannel;
        pMmuMemoryBlocks[i].Used = true;
    }

    PRINTK_C(LOG(LOG_MMU), "%s: %s, returns DMA #%u 0x%llX length 0x%lX va 0x%llX pa 0x%llX MMU VA 0x%llX\n", __func__,
        this->MmuEnabled ? "enabled" : "disabled", dmaChannel, (uint64_t)virtualAddress,
        pMmuMemoryBlocks->Length, (uint64_t)pMmuMemoryBlocks->HostVirtualAddress, pMmuMemoryBlocks->HostPhysicalAddress, *pMmuStartAddress);

    return virtualAddress;
}

/*****************************************************************************/

void mmu_free(mmu_context_t * this, uint16_t dmaChannel, size_t length, void * virtualAddress, dma_addr_t physicalAddress)
{
    mmu_memory_block_t *        pMmuMemoryBlocks = &this->MmuMemoryBlocks[this->NumberOfMemoryBlocksPerEntry * dmaChannel];
    const device_context_t *    pDevExt = this->pDevExt;
    size_t                      i;

    if (this == NULL)
    {
        PRINTK_ERR("Cannot allocate from nonexistent MMU context!\n");
        return;
    }

    for (i = 0; i < this->NumberOfMemoryBlocksPerEntry; ++i)
    {
        if (pMmuMemoryBlocks[i].Used)
        {
            pMmuMemoryBlocks[i].Used = false;
            pMmuMemoryBlocks[i].DmaChannel = -1;

            PRINTK_C(LOG(LOG_MMU), "%s: length 0x%lX va 0x%llX pa 0x%llX\n", __func__,
                pMmuMemoryBlocks[i].Length, (uint64_t)pMmuMemoryBlocks[i].HostVirtualAddress, pMmuMemoryBlocks[i].HostPhysicalAddress);
        }
        else
        {
            PRINTK_ERR("Cannot free unused DMA channel %u memory block!\n", dmaChannel);
        }
    }
}
