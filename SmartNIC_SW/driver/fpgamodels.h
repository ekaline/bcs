#ifndef __fpgamodels_h__
#define __fpgamodels_h__

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
 *  FPGA models. This is the one and only place for this information.
 *
 *  @brief      Do not replicate or copy this information to other places but use this header file in your project!
 *
 *              These models can also be seen in this google docs document:
 *              https://docs.google.com/spreadsheets/d/1eX_wTghsGHB_jARpdVWWQvOq_-jZ2q4vOlTQOgZRfn0/edit#gid=0
 *
 *              or My Drive > Fiberblaze > Teams > Firmware Team > Doc > FPGA_image_type
 */
typedef enum
{
    FPGA_MODEL_UNKNOWN           = 0x00,    /**< Unknown FPGA model. */
    FPGA_MODEL_V5_LX110          = 0x01,
    FPGA_MODEL_V5_LX155          = 0x02,
    FPGA_MODEL_V6_LX240          = 0x03,
    FPGA_MODEL_V6_LX130          = 0x04,
    FPGA_MODEL_V6_SX315          = 0x05,
    FPGA_MODEL_V7_X690           = 0x06,
    FPGA_MODEL_STRATIX_5         = 0x07,
    FPGA_MODEL_V7_HXT580         = 0x08,
    FPGA_MODEL_ZYNQ_7000_45      = 0x09,
    FPGA_MODEL_V7_X330           = 0x0A,
    FPGA_MODEL_V7_LX690_FFG1927  = 0x0B,
    FPGA_MODEL_KU_X115           = 0x0C,
    FPGA_MODEL_US_V095_FFVE1924  = 0x0D,
    FPGA_MODEL_US_V080           = 0x0E,
    FPGA_MODEL_US_V125           = 0x0F,
    FPGA_MODEL_US_V9P_ES1        = 0x10,   /**< Virtex UltraScale+ XCVU9P, xcvu9p-flgb2104-2-i-es1, es9830 */
    FPGA_MODEL_US_V190           = 0x11,
/*     FPGA_MODEL_ARRIA10_GX_1150   = 0x12, */
    FPGA_MODEL_US_V9P_I          = 0x13,
    FPGA_MODEL_US_V7P            = 0x14,
    FPGA_MODEL_US_V9P_D          = 0x15,
    FPGA_MODEL_US_V9P_SG3        = 0x16,
    FPGA_MODEL_US_KU15P          = 0x17
} fpga_model_t;

#endif /* __fpgamodels_h__ */
