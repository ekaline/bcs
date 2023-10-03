#ifndef _EPM_CME_ILINK_TEMPLATE_H_
#define _EPM_CME_ILINK_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmCmeILinkTemplate : public EpmTemplate {
 public:
 EpmCmeILinkTemplate(uint idx) : EpmTemplate(idx) {
      EpmTemplateField myTemplateStruct[] = {
      {"macDa"  , 6, HwField::IMMEDIATE,    false, false},
      {"macSa"  , 6, HwField::IMMEDIATE,    false, false},
      {"ethType", 2, HwField::IMMEDIATE,    false, false},

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

      {"src"   , 2 , HwField::IMMEDIATE,    false, false},
      {"dest"  , 2 , HwField::IMMEDIATE,    false, false},
      {"seqno" , 4 , HwField::LOCAL_SEQ,    false, true },
      {"ackno" , 4 , HwField::REMOTE_SEQ,   false, true },
      {"hdrlen", 2 , HwField::IMMEDIATE,    false, false},
      {"wnd"   , 2 , HwField::TCP_WINDOW,   false, true },
      {"chksum", 2 , HwField::TCP_CHCK_SUM, false, true },
      {"urgp"  , 2 , HwField::IMMEDIATE,    false, false},
      /* --------------------------- */
      
      /* --------------------------- */                        // swap, clear
      {"MessageSize" ,                 2, HwField::IMMEDIATE,    false, false },
      {"EncodingType" ,                2, HwField::IMMEDIATE,    false, false },
      {"BlockLength" ,                 2, HwField::IMMEDIATE,    false, false },
      {"TemplateID" ,                  2, HwField::IMMEDIATE,    false, false },
      {"SchemaID" ,                    2, HwField::IMMEDIATE,    false, false },
      {"Version" ,                     2, HwField::IMMEDIATE,    false, false },
      /* --------------------------- */
      {"PartyDetailsListReqID" ,       8, HwField::IMMEDIATE,    false, false }, 
      {"SendingTimeEpoch" ,            8, HwField::TIME,         false, true  }, 
      {"ManualOrderIndicator" ,        1, HwField::IMMEDIATE,    false, false }, 
      {"SeqNum",                       4, HwField::APPSEQ,       true,  true  },
      {"Trailing" ,                  1000, HwField::IMMEDIATE,    false, false }, // common trailing at all ILink msgs

    };

    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    strcpy(name,"EpmCmeILinkTemplate");
    init();
  }

};

#endif
