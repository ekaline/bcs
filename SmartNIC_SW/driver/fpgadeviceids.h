#ifndef __fpgadeviceids_h__
#define __fpgadeviceids_h__

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
 *  FPGA PCIe device ids. This is the one and only place for this information.
 *
 *  @brief      Do not replicate or copy this information to other places but use this header file in your project!
 *
 *              These ids can also be seen in this google docs document:
 *              https://docs.google.com/spreadsheets/d/1eX_wTghsGHB_jARpdVWWQvOq_-jZ2q4vOlTQOgZRfn0/edit#gid=0
 *
 *              or My Drive > Fiberblaze > Teams > Firmware Team > Doc > FPGA_image_type
 */

/**
 *  PCIe vendor ids.
 */
typedef enum
{
    VENDOR_ID_XILINX                        = 0x10ee,   /**< Vendor is Xilinx. */
    VENDOR_ID_ANRITSU                       = 0x1465,   /**< Vendor is Anritsu. */
    VENDOR_ID_FIBERBLAZE                    = 0x1c2c,   /**< Vendor is Silicom Denmark (previous name was Fiberblaze). DEPRECATED! */
    VENDOR_ID_SILICOM_DENMARK               = 0x1c2c    /**< Vendor is Silicom Denmark. */

} pcie_vendor_id_t;

/**
 *  PCIe device ids. Please keep the alphabetical order.
 */
typedef enum
{
    DEVICE_ID_UNKNOWN                       = 0x0000,   /**< Unknown device id. */

    DEVICE_ID_ANCONA_AUGSBURG               = 0x00A1,

    DEVICE_ID_BERLIN                        = 0x000B,

    DEVICE_ID_CHEMNITZ                      = 0x000C,

    DEVICE_ID_DEMO                          = 0x0002,

    DEVICE_ID_ESSEN                         = 0x000E,

    DEVICE_ID_FB2022_AUGSBURG               = 0x000A,
    DEVICE_ID_FBSLAVE                       = 0x00AF,

    DEVICE_ID_GENOA_AUGSBURG                = 0x00A3,

    DEVICE_ID_HERCULANEUM_AUGSBURG          = 0x00A0,

    DEVICE_ID_LATINA_AUGSBURG               = 0x00A9,
    DEVICE_ID_LATINA_LEONBERG               = 0x00C3,

    DEVICE_ID_LIVIGNO_AUGSBURG              = 0x00A4,
    DEVICE_ID_LIVIGNO_AUGSBURG_4X1          = 0xA004,
    DEVICE_ID_LIVIGNO_FULDA_V7330           = 0x00F2,
    DEVICE_ID_LIVIGNO_LEONBERG_V7330        = 0x00C2,
    DEVICE_ID_LIVIGNO_LEONBERG_V7690        = 0x00C9,

    DEVICE_ID_LIVORNO_FULDA                 = 0x000F,
    DEVICE_ID_LIVORNO_AUGSBURG              = 0x00A2,
    DEVICE_ID_LIVORNO_AUGSBURG_40           = 0x00A5,
    DEVICE_ID_LIVORNO_JENA                  = 0x00B1,
    DEVICE_ID_LIVORNO_JENA_40               = 0x00B2,
    DEVICE_ID_LIVORNO_LEONBERG              = 0x00C0,

    DEVICE_ID_LUCCA_AUGSBURG                = 0x00AA,
    DEVICE_ID_LUCCA_AUGSBURG_40             = 0x00AB,
    DEVICE_ID_LUCCA_LEONBERG_V7330          = 0x00C8,
    DEVICE_ID_LUCCA_LEONBERG_V7690          = 0x00C1,

    DEVICE_ID_LU50                          = 0x0005,
    DEVICE_ID_LUXG                          = 0x0006,

    DEVICE_ID_MANGO_AUGSBURG_8X10           = 0xa00B,
    DEVICE_ID_MANGO_AUGSBURG_2X25           = 0xa00C,
    DEVICE_ID_MANGO_AUGSBURG_40             = 0xA000,
    DEVICE_ID_MANGO_AUGSBURG_100            = 0xA001,
    DEVICE_ID_MANGO_AUGSBURG_16X10          = 0xA003,
    DEVICE_ID_MANGO_LEONBERG_VU125          = 0x00C7,
    DEVICE_ID_MANGO_LEONBERG_VU9P           = 0x00C5,
    DEVICE_ID_MANGO_WOLFSBURG_4X100G        = 0x00E2,
    DEVICE_ID_MANGO_03_LEONBERG_VU9P_ES1    = 0x00C4,
    DEVICE_ID_MANGO_04_AUGSBURG_2X100       = 0xA006,
    DEVICE_ID_MANGO_04_AUGSBURG_2X40        = 0xA007,
    DEVICE_ID_MANGO_04_AUGSBURG_2X25        = 0xA008,
    DEVICE_ID_MANGO_04_AUGSBURG_16X10       = 0xA009,
    DEVICE_ID_MANGO_04_AUGSBURG_8X10        = 0xA00A,
    DEVICE_ID_MANGO_04_AUGSBURG_2X100_VU7P  = 0xA00D,
    DEVICE_ID_MANGO_04_LEONBERG_VU9P        = 0x00CA,

    DEVICE_ID_MILAN_AUGSBURG                = 0x00A6,

    DEVICE_ID_MONZA_AUGSBURG                = 0x00A7,

    DEVICE_ID_PADUA_AUGSBURG_8X10           = 0xA002,
    DEVICE_ID_PADUA_AUGSBURG_2X25           = 0xA005,
    DEVICE_ID_PADUA_AUGSBURG_40             = 0x00AC,
    DEVICE_ID_PADUA_AUGSBURG_100            = 0x00AD,
    DEVICE_ID_PADUA_LEONBERG_VU125          = 0x00C6,

    DEVICE_ID_SAVONA_AUGSBURG_8X10          = 0xA012,
    DEVICE_ID_SAVONA_AUGSBURG_2X25          = 0xA011,
    DEVICE_ID_SAVONA_AUGSBURG_2X100         = 0xA00E,
    DEVICE_ID_SAVONA_AUGSBURG_2X40          = 0xA00F,
    DEVICE_ID_SAVONA_WOLFSBURG_2X100G       = 0x00E0,

    DEVICE_ID_SILIC100_AUGSBURG             = 0x00A8,

    DEVICE_ID_TEST_FPGA                     = 0x0001,
    DEVICE_ID_TEST_FPGA_SLAVE               = 0x0003,

    DEVICE_ID_TIVOLI_AUGSBURG_2X40          = 0xA010,
    DEVICE_ID_TIVOLI_WOLFSBURG_2X100G       = 0x00E1,

    DEVICE_ID_XILINX                        = 0x0007,

    DEVICE_ID_MANGO_ODESSA_CRYPTO_2X40      = 0x0021,
    DEVICE_ID_LIVORNO_ODESSA_CRYPTO_2X40    = 0x0022,
    DEVICE_ID_LIVORNO_ODESSA_CRYPTO_2X10    = 0x0023,
    DEVICE_ID_LIVORNO_ODESSA_REGEX_4X10     = 0x0024,

} pcie_device_id_t;

#endif /* __fpgadeviceids_h__ */
