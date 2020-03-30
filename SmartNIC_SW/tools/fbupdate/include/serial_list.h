#ifndef SERIAL_LIST_H
#define SERIAL_LIST_H

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

typedef struct
{
    const char * name;
    int32_t      type;
} serial_list_t;

serial_list_t serialList[] =
{
    {"FB2XG@V5110",         1}, 
    {"FB4XG@V6240",         2},     /* Ancona */
    {"FB2XG@V5155",         3},     /* FB2022 */
    {"FB4G@V6130",          4},     /* Herculaneum */
    {"FB8XG@V7690",         5},     /* Livorno */
    {"FB8XG@V7693",         6},     /* Livorno SG3 */
    {"FB2XG@V6242",         7},     /* Genoa */
    {"FB4XG@V7693",         8},     /* Livigno */
    {"FB8XG@V7695",         9},     /* Livorno SG3 big QDR */
    {"FB4XG@V7330",         10},    /* Livigno */
    {"FB8XG@V7692",         11},    /* Livorno SG3 no QDR */
    {"FB8XGHH@V7693",       12},    /* Lucca */
    {"FB1CG@V7580",         13},    /* Milan 03 */
    {"FB8XG@V7691",         14},    /* Livorno SG2 big QDR */
    {"FB8XG@Z7045",         15},    /* Marsala */
    {"FB2CG@V7580",         16},    /* Monza */
    {"FB8XGHH@V7330",       17},    /* Lucca */
    {"FB8XG@V7696",         18},    /* Livorno SG2, no QDR, no RLD */
    {"FB2XGHH@V7330",       19},    /* Latina */
    {"FB8XG@KU115",         20},    /* Torino */
    {"FB4CGG3@VU125-2",     21},    /* Mango 4 port, VU125, SG-2 */
    {"FB2CGG3HL@VU125-2",   22},    /* Padua, SG-2 */
    {"FB2CGG3@VU125-2",     23},    /* Mango 2 port, VU125, SG-2 */
    {"FB2CGG3HL@VU080-2",   24},    /* Padua , SG-2 */
    {"FB4CGG3@VU09P-2",     25},    /* Mango 4 port, Ultrascale+, SG-2 */
    {"FB4CGG3@VU190-2",     26},    /* Mango 4 port, Ultrascale vu190, SG-2 */
    {"FB8XGHH@V7690",       27},    /* Lucca */
    {"FB4CGG3@VU080-2",     28},    /* Mango 4 port, Ultrascale vu080, SG-2 */
    {"FB2CGG3@VU080-2",     29},    /* Mango 2 port, Ultrascale vu080, SG-2 */
    {"FB4CGG3@VU07P-2",     30},    /* Mango 4 port, Ultrascale Plus vu07P, SG-2 */
    {"FB2CGG3@VU07P-2",     31},    /* Mango 2 port, Ultrascale Plus vu07P, SG-2 */
    {"FB2CGG3@VU190-2",     32},    /* Mango 2 port, Ultrascale vu190, SG-2 */
    {"FB2CGG3@VU09P-2",     33},    /* Mango 2 port, Ultrascale+, SG-2 */
    {"FB2CGG3@VU190-2",     34},    /* Mango 2 port, Ultrascale vu190, SG-2 */
    {"FB4CGG3@VU09P-2-R0",  35},    /* Mango 4 port, Ultrascale+, SG-2 with one IOS-A module */
    {"FB4CGG3@VU09P-3",     36},    /* Mango 4 port, VU09P SG3 */
    {"FB2CG@KU15P-2",       37},    /* Savona 2 port, KU15P */
    {"FB2CGHH@KU15P-2",     38},    /* Tivoli 2 port, KU15P */
};

#endif /* SERIAL_LIST_H */
