#ifndef _EPM_CANCEL_BOE_TEMPLATE_H_
#define _EPM_CANCEL_BOE_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmCancelBoeTemplate : public EpmTemplate {
 public:
 EpmCancelBoeTemplate(uint idx) : EpmTemplate(idx) {
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

      /* --------------------------- */                         // swap, clear
      {"StartOfMessage" ,               2, HwField::IMMEDIATE,    false, false }, // 0xBABA
      {"MessageLength" ,                2, HwField::IMMEDIATE,    false, false }, // 45
      {"MessageType" ,                  1, HwField::IMMEDIATE,    false, false }, // 0x38 NewOrder
      {"MatchingUnit" ,                 1, HwField::IMMEDIATE,    false, false }, // always 0
      {"SequenceNumber" ,               4, HwField::IMMEDIATE,    false, false }, // 0
      {"ClOrdIDtxt" ,                   11,HwField::IMMEDIATE,    false, false }, // free text
      {"ClOrdIDseq" ,                   8, HwField::ASCII_CNT,    false, true  }, //
      {"ClOrdIDtail" ,                  1, HwField::IMMEDIATE,    false, false }, // free text
      
      /* --------------------------- */
      {"NumberOfBitfields" ,            1, HwField::IMMEDIATE,    false, false }, // 2
      {"NewOrderBitfield1" ,            1, HwField::IMMEDIATE,    false, false }, // ClearingFirm   (1) OR MassCancelId (16)
      {"NewOrderBitfield2" ,            1, HwField::IMMEDIATE,    false, false }, // MassCancelInst (1) OR SendTime     (8 )

      /* --------------------------- */
      {"ClearingFirm" ,                 4, HwField::IMMEDIATE,    false, false },
      
      {"MassCancelIdtxt" ,             11, HwField::IMMEDIATE,    false, false }, // free text
      {"MassCancelIdseq" ,              8, HwField::ASCII_CNT,    false, true  }, //
      {"MassCancelIdtail" ,             1, HwField::IMMEDIATE,    false, false }, // free text
      
      
      {"MassCancelInst" ,              16, HwField::IMMEDIATE,    false, false }, // Text
      {"SendTime" ,                     8, HwField::TIME,         false, false },
    };

    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    strcpy(name,"EpmCancelBoeTemplate");
    init();
  }

};

#endif
