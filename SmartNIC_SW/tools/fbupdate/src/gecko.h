#ifndef GECKO_H_
#define GECKO_H_

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

/*
 * Interface to Gecko flash
 */

#include <sys/types.h>

/**
 * Type to chose Gecko flash area
 */
typedef enum
{
    GECKO_FLASH_PLL,            /**< Select PLL area */
    GECKO_FLASH_SILICOM_AREA_1, /**< Select Silicom area 1 */
    GECKO_FLASH_SILICOM_AREA_2, /**< Select Silicom area 2 */
    GECKO_FLASH_SILICOM_AREA_3, /**< Select Silicom area 3 */
    GECKO_FLASH_SILICOM_AREA_4, /**< Select Silicom area 4 */
} GeckoFlashArea;

static const uint32_t GeckoFlash1PageSize = 0x1000;

int GeckoWrite(uint32_t * pRegistersBaseAddress, const uint8_t cmd, uint16_t subCmd, uint32_t data);

int GeckoReadMacAddr(uint32_t *pRegistersBaseAddress, int maxPort);
int GeckoReadSerial(uint32_t *pRegistersBaseAddress);
int GeckoWriteMacAddr(uint32_t *pRegistersBaseAddress, const char* serialNo, int maxPort);
int GeckoReadPcbVer(uint32_t *pRegistersBaseAddress);
int GeckoWritePcbVer(uint32_t *pRegistersBaseAddress, int pcbVer);

/* Read len bytes from geckoFlashArea from offset*/
int GeckoReadLenFlash1Page(uint32_t *pRegistersBaseAddress, const GeckoFlashArea geckoFlashArea,
                           const uint16_t offset, const uint16_t len, uint8_t *data);

/* Read whole geckoFlashArea */
int GeckoReadFlash1Page(uint32_t *pRegistersBaseAddress, const GeckoFlashArea geckoFlashArea, uint8_t *data);

/* Erase and write whole geckoFlashArea */
int GeckoWriteFlash1Page(uint32_t *pRegistersBaseAddress, const GeckoFlashArea geckoFlashArea, uint8_t *data);

uint32_t GeckoRevision(uint32_t *pRegistersBaseAddress);

void GeckoRebootFPGA(uint32_t *pRegistersBaseAddress, int image_no);

/*
 * Parse Gecko shared input cmds
 */
int cmdGecko(FL_FlashLibraryContext * pFlashLibraryContext, int argc, char **argv);

#endif /* GECKO_H_ */
