#ifndef _EPM_BOE_QUOTE_UPDATE_SHORT_TEMPLATE_H_
#define _EPM_BOE_QUOTE_UPDATE_SHORT_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmBoeQuoteUpdateShortTemplate : public EpmTemplate {
 public:
 EpmBoeQuoteUpdateShortTemplate(uint idx) : EpmTemplate(idx) {
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
      {"MessageType" ,                  1, HwField::IMMEDIATE,    false, false }, // 0x59
      {"MatchingUnit" ,                 1, HwField::IMMEDIATE,    false, false }, // always 0
      {"SequenceNumber" ,               4, HwField::IMMEDIATE,    false, false }, // 0
      {"QuoteUpdateIdtxt" ,             8, HwField::IMMEDIATE,    false, false }, // free text
      {"QuoteUpdateIdseq" ,             8, HwField::ASCII_CNT,    false, true  }, //
      
      {"ClearingFirm" ,                 4, HwField::IMMEDIATE,    false, false },  
      {"ClearingAccount" ,              4, HwField::IMMEDIATE,    false, false },  
      {"CustomGroupID" ,                2, HwField::IMMEDIATE,    false, false },  
      {"Capacity" ,                     1, HwField::IMMEDIATE,    false, false }, // 'M'  
      {"Reserved" ,                     3, HwField::IMMEDIATE,    false, false },
      {"SendTime" ,                     8, HwField::TIME,         true,  true  }, // 
      {"PostingInstruction" ,           1, HwField::IMMEDIATE,    false, false }, // I
      {"SessionEligibility" ,           1, HwField::IMMEDIATE,    false, false }, // R      
      {"QuoteCnt" ,                     1, HwField::IMMEDIATE,    false, false }, // 1

      /* --------------------------- */
      {"Symbol" ,                       6, HwField::IMMEDIATE,    false, false }, // free text
      {"Side" ,                         1, HwField::SIDE,         true,  true  }, // '1'-Bid, '2'-Ask
      {"OpenClose" ,                    1, HwField::IMMEDIATE,    false, false }, // 0
      {"Price" ,                        4, HwField::PRICE,        true,  true  },  
      {"OrderQty" ,                     2, HwField::SIZE,         true,  true  },
      {"Reserved" ,                     2, HwField::IMMEDIATE,    false, false },

    };

    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    strcpy(name,"EpmBoeQuoteUpdateShortTemplate");
    init();
  }

};

#endif
