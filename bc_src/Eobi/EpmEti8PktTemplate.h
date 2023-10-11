#ifndef _EPM_ETI8_PKT_TEMPLATE_H_
#define _EPM_ETI8_PKT_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmEti8PktTemplate : public EpmTemplate {
 public:
 EpmEti8PktTemplate(uint idx, const char* _name) : EpmTemplate(idx,_name) {
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
      {"BodyLen" ,                                4, HwField::IMMEDIATE,    false, false},
      {"TemplateID",                              2, HwField::IMMEDIATE,    false, false},

      {"NetworkMsgID",                            8, HwField::IMMEDIATE,    false, false},
      {"Pad2",                                    2, HwField::IMMEDIATE,    false, false},

      {"AppSeq",                                  4, HwField::APPSEQ,       true,  true},

      {"SenderSubID",                             4, HwField::IMMEDIATE,    false, false},

      {"OrderID",                                 8, HwField::IMMEDIATE,    false, true},
      {"ClOrdID",                                 8, HwField::IMMEDIATE,    false, true},

      {"OrigClOrdID",                             8, HwField::IMMEDIATE,    false, false},
      {"PartyIdInvestmentDecisionMaker",          8, HwField::IMMEDIATE,    false, false},
      {"ExecutingTrader",                         8, HwField::IMMEDIATE,    false, false},
      {"MarketSegmentID",                         4, HwField::IMMEDIATE,    false, false},
      {"SimpleSecurityID",                        4, HwField::IMMEDIATE,    false, false},
      {"TargetPartyIDSessionID",                  4, HwField::IMMEDIATE,    false, false},
      {"OrderOrigination",                        1, HwField::IMMEDIATE,    false, false},
      {"PartyIdInvestmentDecisionMakerQualifier", 1, HwField::IMMEDIATE,    false, false},
      {"ExecutingTraderQualifier",                1, HwField::IMMEDIATE,    false, false},
      {"FIXClOrdID",                             20, HwField::IMMEDIATE,    false, false},
      {"ComplianceText",                         20, HwField::IMMEDIATE,    false, false},
      {"Pad1",                                    1, HwField::IMMEDIATE,    false, false},
        
      //    {"payload"  , EpmMaxRawTcpSize - sizeof(EkaEthHdr) - sizeof(EkaIpHdr) - sizeof(EkaTcpHdr) - sizeof(DeleteOrderSingleRequestT) , HwField::IMMEDIATE, false, false}
    };

    tSize = sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS) on_error ("tSize %u > MAX_FIELDS %u",tSize, MAX_FIELDS);
    memcpy(templateStruct,myTemplateStruct,sizeof(myTemplateStruct));
    init();
  }
  virtual uint16_t payloadSize() {
    return (uint16_t) sizeof(DeleteOrderSingleRequestT);
  }


};

#endif
