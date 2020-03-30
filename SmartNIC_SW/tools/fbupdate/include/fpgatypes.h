#ifndef __fpgatypes_h__
#define __fpgatypes_h__

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
 *  FPGA image types. This is the one and only place for this information.
 *
 *  @brief      Do not replicate or copy this information to other places but use this header file in your project!
 *
 *              These image types can also be seen in this google docs document:
 *              https://docs.google.com/spreadsheets/d/1eX_wTghsGHB_jARpdVWWQvOq_-jZ2q4vOlTQOgZRfn0/edit#gid=0
 *
 *              or My Drive > Fiberblaze > Teams > Firmware Team > Doc > FPGA_image_type
 */
typedef enum
{
    FPGA_TYPE_UNKNOWN                       = 0x00,
    FPGA_TYPE_LUXG_PROBE                    = 0x03,
    FPGA_TYPE_SAVONA_WOLFSBURG_2X100G       = 0x10,
    FPGA_TYPE_TIVOLI_WOLFSBURG_2X100G       = 0x11,
    FPGA_TYPE_MANGO_WOLFSBURG_4X100G        = 0x12,
    FPGA_TYPE_SAVONA_AUGSBURG_2X25          = 0x13,
    FPGA_TYPE_SAVONA_AUGSBURG_8X10          = 0x14,
    FPGA_TYPE_ESSEN                         = 0x45,
    FPGA_TYPE_ANCONA_CHEMNITZ               = 0x46,
    FPGA_TYPE_FIRENZE_CHEMNITZ              = 0x47,
    FPGA_TYPE_MANGO_ODESSA_40               = 0x48,
    FPGA_TYPE_LIVORNO_ODESSA_40             = 0x49,
    FPGA_TYPE_LIVORNO_ODESSA_CRYPTO_2X10G   = 0x4A,
    FPGA_TYPE_LIVORNO_ODESSA_REGEX_4X10G    = 0x4B,
    FPGA_TYPE_TIVOLI_AUGSBURG_2X40          = 0x4D,
    FPGA_TYPE_SAVONA_AUGSBURG_2X40          = 0x4E,
    FPGA_TYPE_SAVONA_AUGSBURG_2X100         = 0x4F,
    FPGA_TYPE_ANCONA_AUGSBURG               = 0x51,
    FPGA_TYPE_HERCULANEUM_AUGSBURG          = 0x52,
    FPGA_TYPE_LIVORNO_AUGSBURG              = 0x53,
    FPGA_TYPE_LIVORNO_FULDA                 = 0x54,
    FPGA_TYPE_GENOA_AUGSBURG                = 0x56,
    FPGA_TYPE_LIVIGNO_AUGSBURG              = 0x57,
    FPGA_TYPE_LIVORNO_AUGSBURG_40           = 0x58,
    FPGA_TYPE_MILAN_AUGSBURG                = 0x59,
    FPGA_TYPE_LIVIGNO_FULDA_V7330           = 0x5F,
    FPGA_TYPE_MONZA_AUGSBURG                = 0x60,
    FPGA_TYPE_LIVORNO_LEONBERG              = 0x61,
    FPGA_TYPE_LUCCA_LEONBERG_V7690          = 0x62,
    FPGA_TYPE_LIVIGNO_LEONBERG_V7330        = 0x63,
    FPGA_TYPE_LATINA_LEONBERG               = 0x64,
    FPGA_TYPE_SILIC100_AUGSBURG             = 0x65,
    FPGA_TYPE_LATINA_AUGSBURG               = 0x66,
    FPGA_TYPE_LUCCA_AUGSBURG                = 0x67,
    FPGA_TYPE_LUCCA_AUGSBURG_40             = 0x68,
    FPGA_TYPE_PADUA_AUGSBURG_40             = 0x69,
    FPGA_TYPE_PADUA_AUGSBURG_100            = 0x6A,
    FPGA_TYPE_MANGO_AUGSBURG_40             = 0x6B,
    FPGA_TYPE_MANGO_AUGSBURG_100            = 0x6C,
    FPGA_TYPE_MANGO_03_LEONBERG_VU9P_ES1    = 0x6D, /* Used to be FPGA_TYPE_PADUA_LEONBERG_VU9P */
    FPGA_TYPE_MANGO_LEONBERG_VU9P           = 0x6E,
    FPGA_TYPE_PADUA_LEONBERG_VU125          = 0x6F,
    FPGA_TYPE_MANGO_LEONBERG_VU125          = 0x70,
    FPGA_TYPE_LUCCA_LEONBERG_V7330          = 0x71,
    FPGA_TYPE_LIVIGNO_LEONBERG_V7690        = 0x72,
    FPGA_TYPE_PADUA_AUGSBURG_8X10           = 0x73,
    FPGA_TYPE_MANGO_AUGSBURG_16X10          = 0x74,
    FPGA_TYPE_LIVIGNO_AUGSBURG_4X1          = 0x75,
    FPGA_TYPE_PADUA_AUGSBURG_2X25           = 0x76,
    FPGA_TYPE_MANGO_04_LEONBERG_VU9P        = 0x77,
    FPGA_TYPE_MANGO_04_AUGSBURG_2X100       = 0x78,
    FPGA_TYPE_MANGO_04_AUGSBURG_2X40        = 0x79,
    FPGA_TYPE_MANGO_04_AUGSBURG_2X25        = 0x7A,
    FPGA_TYPE_MANGO_04_AUGSBURG_16X10       = 0x7B,
    FPGA_TYPE_MANGO_04_AUGSBURG_8X10        = 0x7C,
    FPGA_TYPE_MANGO_AUGSBURG_8X10           = 0x7D,
    FPGA_TYPE_MANGO_AUGSBURG_2X25           = 0x7E,
    FPGA_TYPE_MANGO_04_AUGSBURG_2X100_VU7P  = 0x7F,
    FPGA_TYPE_LUXG_TEST                     = 0x82,
    FPGA_TYPE_ANCONA_TEST                   = 0x83,
    FPGA_TYPE_ERCOLANO_TEST                 = 0x84,
    FPGA_TYPE_ERCOLANO_DEMO                 = 0x85,
    FPGA_TYPE_FIRENZE_TEST                  = 0x86,
    FPGA_TYPE_GENOA_TEST                    = 0x87,
    FPGA_TYPE_HERCULANEUM_TEST              = 0x88,
    FPGA_TYPE_LIVORNO_TEST                  = 0x89,
    FPGA_TYPE_MILAN_TEST                    = 0x8A,
    FPGA_TYPE_PISA_TEST                     = 0x8B,
    FPGA_TYPE_MARSALA_TEST                  = 0x8C,
    FPGA_TYPE_LIVIGNO_TEST                  = 0x8D,
    FPGA_TYPE_MONZA_TEST                    = 0x8E,
    FPGA_TYPE_LIVORNO_TEST_40               = 0x8F,
    FPGA_TYPE_BERGAMO_TEST                  = 0x93,
    FPGA_TYPE_COMO_TEST                     = 0x94,
    FPGA_TYPE_2022                          = 0x90,
    FPGA_TYPE_2015                          = 0x91,
    FPGA_TYPE_2010                          = 0x92,
    FPGA_TYPE_ATHEN_TEST                    = 0x95,
    FPGA_TYPE_LUCCA_TEST                    = 0x96,
    FPGA_TYPE_TORINO_TEST                   = 0x97,
    FPGA_TYPE_LATINA_TEST                   = 0x98,
    FPGA_TYPE_SILIC100_TEST                 = 0x99,
    FPGA_TYPE_MANGO_TEST                    = 0x9A,
    FPGA_TYPE_PADUA_TEST                    = 0x9B,
    FPGA_TYPE_HERCULANEUM_CLOCK_CALIBRATION = 0x9C,
    FPGA_TYPE_ANCONA_CLOCK_CALIBRATION      = 0x9D,
    FPGA_TYPE_CARDIFF_TEST                  = 0x9E,
    FPGA_TYPE_RIMINI_TEST                   = 0xA0,
    FPGA_TYPE_MANGO_TEST_QDR                = 0xA1,
    FPGA_TYPE_CARDIFF_TEST_4X40G            = 0xA2,
    FPGA_TYPE_VCU1525_TEST                  = 0xA3,
    FPGA_TYPE_SAVONA_TEST                   = 0xA4,
    FPGA_TYPE_TIVOLI_TEST                   = 0xA5,

} fpga_type_t;

#endif /* __fpgatypes_h__ */
