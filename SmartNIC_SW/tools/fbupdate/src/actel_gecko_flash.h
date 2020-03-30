#ifndef ACTEL_GECKO_H_
#define ACTEL_GECKO_H_

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

#include <sys/types.h>

#include "FL_FlashLibrary.h"

/**
 * Type to define which kind of style is used for admin flash
 */
typedef enum
{
    ADMIN_STYLE_ACTEL,  /**< Uses the Actel admin style for flash of data like MAC address */
    ADMIN_STYLE_GECKO,  /**< Uses the Gecko admin style for flash of data like MAC address */
} AdminStyle;

/**
 * Context for admin flash
 */
typedef struct
{
    uint bufWriteNoBytes;   /**< How many bytes can we written in one operation for this flash interface */
    uint sectorSizeInWords; /**< Size of the sector used measured in words (2 bytes) */
    AdminStyle adminStyle;  /**< Basis for choosing function to call */
} AGFlashContext;

/**
 * Generic Init flash to determine cardType and handle Actel
 */
int AG_InitFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext);

/**
 * Generic read flash using the old uint16_t array
 */
int AG_ReadLenFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext,
                     const uint16_t offset, const uint16_t len, uint16_t *data);

/**
 * Generic read whole flash using the old uint16_t array
 */
int AG_ReadFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext, uint16_t *data);

/**
 * Generic erase and write flash using the old uint16_t array
 */
int AG_EraseWriteFlash(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext, uint16_t *data);

/**
 * Generic unprotect flash if protected
 */
int AG_Unprotect(const FL_FlashInfo * pFlashInfo, AGFlashContext * pAGFlashContext);

/**
 * Generic read serial
 */
int AG_ReadSerial(const FL_FlashInfo * pFlashInfo);

#endif /* ACTEL_GECKO_H_ */
