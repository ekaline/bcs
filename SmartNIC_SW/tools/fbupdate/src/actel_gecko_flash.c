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

#include <stdint.h>
#include <unistd.h>
#include "actel.h"
#include "actel_gecko_flash.h"
#include "fpgaregs.h"
#include "gecko.h"


int AG_InitFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext)
{
    uint64_t revision;
    uint8_t fpgaType;
    // Write zero to revision register to read revision
    FL_Write64Bits(pFlashInfo->pRegistersBaseAddress, REG_VERSION, 0);
    usleep(1);
    // Read revision
    revision = FL_Read64Bits(pFlashInfo->pRegistersBaseAddress, REG_VERSION);
    fpgaType = (revision >> 32LL) & 0xFF;
    FL_CardType cardType = FL_GetCardTypeFromFpgaType(pFlashInfo->pErrorContext, fpgaType);

    if (cardType != FL_CARD_TYPE_SAVONA && cardType != FL_CARD_TYPE_TIVOLI)
    {
        pAGFlashContext->adminStyle = ADMIN_STYLE_ACTEL;
    }
    else
    {
        pAGFlashContext->adminStyle = ADMIN_STYLE_GECKO;
    }

    switch (pAGFlashContext->adminStyle)
    {
    case ADMIN_STYLE_ACTEL:
        pAGFlashContext->sectorSizeInWords = SECTOR0DATA_SIZE;
        pAGFlashContext->bufWriteNoBytes = pFlashInfo->BufferCount;// Read flash status
        break;
    case ADMIN_STYLE_GECKO:
        pAGFlashContext->sectorSizeInWords = GeckoFlash1PageSize / 2;
        pAGFlashContext->bufWriteNoBytes = 0; //not useed
        break;
    }
    return 0;
}

int AG_ReadLenFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext,
                     const uint16_t offset, const uint16_t len, uint16_t * data)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pAGFlashContext->adminStyle)
    {
    case ADMIN_STYLE_ACTEL:
    {
        int i;
        for (i = 0; i < len; i++) {
            FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, i + offset, &data[i]));
        }
        break;
    }
    case ADMIN_STYLE_GECKO:
        GeckoReadLenFlash1Page(pFlashInfo->pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, offset * 2, len * 2, (uint8_t *)data);
        break;
    }
    return 0;
}

int AG_ReadFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext, uint16_t * data)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pAGFlashContext->adminStyle)
    {
    case ADMIN_STYLE_ACTEL:
    {
        int i;
        for (i = 0; i < SECTOR0DATA_SIZE; i++) {
            FL_ExitOnError(pLogContext, FL_AdminReadWord(pFlashInfo, i, &data[i]));
        }
        break;
    }
    case ADMIN_STYLE_GECKO:
        GeckoReadFlash1Page(pFlashInfo->pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, (uint8_t *)data);
        break;
    }
    return 0;
}

int AG_EraseWriteFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext, uint16_t * data)
{
    const FL_LogContext * pLogContext = pFlashInfo->pLogContext;

    switch (pAGFlashContext->adminStyle)
    {
    case ADMIN_STYLE_ACTEL:
    {
        int i;
        //Erase sector 0
        FL_ExitOnError(pLogContext, FL_FlashEraseSector(pFlashInfo, 0));

        //Write updated data back to flash memory
        for(i = 0; i < SECTOR0DATA_SIZE; i += pAGFlashContext->bufWriteNoBytes)
        {
            FL_ExitOnError(pLogContext, FL_WriteFlashWords(pFlashInfo, i, &data[i], pAGFlashContext->bufWriteNoBytes));
        }
        break;
    }
    case ADMIN_STYLE_GECKO:
        GeckoWriteFlash1Page(pFlashInfo->pRegistersBaseAddress, GECKO_FLASH_SILICOM_AREA_1, (uint8_t *)data);
        break;
    }
    return 0;
}

int AG_Unprotect(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext)
{
    switch (pAGFlashContext->adminStyle)
    {
        case ADMIN_STYLE_ACTEL:
            {
                FL_Boolean sectorIsLocked;

                FL_ExitOnError(pFlashInfo->pLogContext, FL_FlashSectorIsLocked(pFlashInfo, 0, &sectorIsLocked));
                if (sectorIsLocked) {
                    // Unlock sector 0
                    ActelUnprotectHcmImage(pFlashInfo);
                }
            }
            break;
        case ADMIN_STYLE_GECKO:
            break;
    }
    return 0;
}

// Serial
int AG_ReadSerial(const FL_FlashInfo * pFlashInfo)
{
    AGFlashContext FlashContext;
    AG_InitFlash(pFlashInfo, &FlashContext);

    switch (FlashContext.adminStyle)
    {
        case ADMIN_STYLE_ACTEL:
            {
                char serial[30];
                //ActelInitFlash(pRegistersBaseAddress);
                ActelMac2Serial(pFlashInfo, serial);
                break;
            }
        case ADMIN_STYLE_GECKO:
            GeckoReadSerial(pFlashInfo->pRegistersBaseAddress);
            break;
    }
    return 0;
}
