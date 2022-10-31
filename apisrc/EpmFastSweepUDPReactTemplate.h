#ifndef _EPM_FAST_SWEEP_UDP_REACT_TEMPLATE_H_
#define _EPM_FAST_SWEEP_UDP_REACT_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmFastSweepUDPReactTemplate : public EpmTemplate {
 public:
 EpmFastSweepUDPReactTemplate(uint idx) : EpmTemplate(idx) {
      EpmTemplateField myTemplateStruct[] = {
	//ethernet
	{"macDa"  , 6, HwField::IMMEDIATE,    false, false},
	{"macSa"  , 6, HwField::IMMEDIATE,    false, false},
	{"ethType", 2, HwField::IMMEDIATE,    false, false},
	//ipv4
	{"v_hl"  , 1 , HwField::IMMEDIATE,    false, false},
	{"tos"   , 1 , HwField::IMMEDIATE,    false, false},
	{"len"   , 2 , HwField::IMMEDIATE,    false, false},
	{"id"    , 2 , HwField::IMMEDIATE,    false, false},
	{"offset", 2 , HwField::IMMEDIATE,    false, false},
	{"ttl"   , 1 , HwField::IMMEDIATE,    false, false},
	{"proto" , 1 , HwField::IMMEDIATE,    false, false},
	{"chksum", 2 , HwField::IMMEDIATE,    false, false},
	{"src"   , 4 , HwField::IMMEDIATE,    false, false},
	{"dst"   , 4 , HwField::IMMEDIATE,    false, false},
	//udp
	{"udp_srcport"  , 2 , HwField::IMMEDIATE,    false, false},
	{"udp_dstport"  , 2 , HwField::IMMEDIATE,    false, false},
	{"udp_length"   , 2 , HwField::IMMEDIATE,    false, false},
	{"udp_cs"       , 2 , HwField::IMMEDIATE   , false, false},
	/* --------------------------- */
	//42 bytes until now
	/* --------------------------- */                     // swap, clear
	{"MDLocateID" ,                 2, HwField::SECURITY_ID, true,  true },
	{"MDUDPPayloadSize" ,           2, HwField::SIZE,        true,  true },
	{"MDLastMsgNumber" ,            2, HwField::PRICE,       true,  true },
	/* {"MDLocateID" ,                 2, HwField::SECURITY_ID, false,  false }, */
	/* {"MDUDPPayloadSize" ,           2, HwField::SIZE,        false,  false }, */
	/* {"MDLastMsgNumber" ,            2, HwField::PRICE,       false,  false }, */
	{"Padding60" ,                 12, HwField::IMMEDIATE,   false, false },
      };

    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    strcpy(name,"EpmFastSweepUDPReactTemplate");
    init();
 }
  
};

#endif
