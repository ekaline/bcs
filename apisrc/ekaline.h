/* Copyright (c) Ekaline, 2018. All rights reserved.
 * PROVIDED "AS IS" WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED.
 * See LICENSE file for details.
 */

#ifndef _EKALINE_H_
#define _EKALINE_H_
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <inttypes.h>

#include "smartnic.h"
#include "eka.h"
#include "eka_sn_addr_space.h"
#include "eka_hw_conf.h"
#include "eka_data_structs.h"
#include "eka_api_funcs.h"

#include "eka_fh.h"
#include "ctls.h"

#include "EfcCtxs.h"
#include "Efc.h"
#include "EfcMsgs.h"
#include "Efh.h"
#include "EfhMsgs.h"
#include "Eka.h"
#include "EkaMsgs.h"
#include "Exc.h"


#ifndef SC_LEGACY_API
#define SC_IOCTL_EKALINE_DATA SMARTNIC_EKALINE_DATA
#endif

#endif //_EKALINE_H_

