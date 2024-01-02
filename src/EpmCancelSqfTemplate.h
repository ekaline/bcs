#ifndef _EPM_CANCEL_SQF_TEMPLATE_H_
#define _EPM_CANCEL_SQF_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmCancelSqfTemplate : public EpmTemplate {
 public:
 EpmCancelSqfTemplate(uint idx, const char* _name) : EpmTemplate(idx,_name) {
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
      {"seqno" , 4 , HwField::LOCAL_SEQ,    false, true},
      {"ackno" , 4 , HwField::REMOTE_SEQ,   false, true},
      {"hdrlen", 2 , HwField::IMMEDIATE,    false, false},
      {"wnd"   , 2 , HwField::TCP_WINDOW,   false, true},
      {"chksum", 2 , HwField::TCP_CHCK_SUM, false, true},
      {"urgp"  , 2 , HwField::IMMEDIATE,    false, false},

      /* --------------------------- */
      {"typeSub" ,                                2, HwField::IMMEDIATE,    false, false }, //swap,clear
      {"badge" ,                                  4, HwField::IMMEDIATE,    false, false },
      {"messageId" ,                              8, HwField::IMMEDIATE,    false, false },
      {"quoteCount" ,                             2, HwField::IMMEDIATE,    false, false },
      /* --------------------------- */
      {"optionId" ,                               4, HwField::SECURITY_ID,  false, true  },

      {"bidPrice",                                4, HwField::IMMEDIATE,    false, false },
      {"bidSize",                                 4, HwField::IMMEDIATE,    false, false },
      {"askPrice",                                4, HwField::IMMEDIATE,    false, false },
      {"askSize",                                 4, HwField::IMMEDIATE,    false, false },

      {"reentry",                                 1, HwField::IMMEDIATE,    false, false },
    };

    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    strcpy(name,"EpmCancelSqfTemplate");
    init();
  }

};

#endif
