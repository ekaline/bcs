#ifndef _EPM_ETI8_PKT_TEMPLATE_H_
#define _EPM_ETI8_PKT_TEMPLATE_H_

#include "EpmTemplate.h"

class EpmEti8PktTemplate : public EpmTemplate {
public:
  EpmEti8PktTemplate(uint idx) : EpmTemplate(idx) {
    EpmTemplateField myTemplateStruct[] = {
        {"macDa", 6, HwField::IMMEDIATE, false, false},
        {"macSa", 6, HwField::IMMEDIATE, false, false},
        {"ethType", 2, HwField::IMMEDIATE, false, false},

        {"v_hl", 1, HwField::IMMEDIATE, false, false},
        {"tos", 1, HwField::IMMEDIATE, false, false},
        {"len", 2, HwField::IMMEDIATE, false, false},
        {"id", 2, HwField::IMMEDIATE, false, false},
        {"offset", 2, HwField::IMMEDIATE, false, false},
        {"ttl", 1, HwField::IMMEDIATE, false, false},
        {"proto", 1, HwField::IMMEDIATE, false, false},
        {"chksum", 2, HwField::IMMEDIATE, false, false},
        {"src", 4, HwField::IMMEDIATE, false, false},
        {"dst", 4, HwField::IMMEDIATE, false, false},

        {"src", 2, HwField::IMMEDIATE, false, false},
        {"dest", 2, HwField::IMMEDIATE, false, false},
        {"seqno", 4, HwField::LOCAL_SEQ, false, true},
        {"ackno", 4, HwField::REMOTE_SEQ, false, true},
        {"hdrlen", 2, HwField::IMMEDIATE, false, false},
        {"wnd", 2, HwField::TCP_WINDOW, false, true},
        {"chksum", 2, HwField::TCP_CHCK_SUM, false, true},
        {"urgp", 2, HwField::IMMEDIATE, false, false},

        /* --------------------------- */
        {"BodyLen", 4, HwField::IMMEDIATE, false, false},
        {"TemplateID", 2, HwField::IMMEDIATE, false, false},
        {"NetworkMsgID", 8, HwField::IMMEDIATE, false,
         false},
        {"Pad2", 2, HwField::IMMEDIATE, false, false},
        {"MsgSeqNum", 4, HwField::APPSEQ, true, true},
        {"SenderSubID", 4, HwField::IMMEDIATE, false,
         false},
        {"Price", 8, HwField::PRICE, true, true},
        {"OrderQty", 8, HwField::SIZE, true, true},
        {"ClOrdID", 8, HwField::IMMEDIATE, false, false},
        {"PartyIDClientID", 8, HwField::IMMEDIATE, false, false},
        {"PartyIdInvestmentDecisionMaker", 8, HwField::IMMEDIATE, false, false},
        {"ExecutingTrader", 8, HwField::IMMEDIATE, false, false},
        {"SimpleSecurityID", 4, HwField::SECURITY_ID, true, true},
        {"MatchInstCrossID", 4, HwField::IMMEDIATE, false, false},
        {"EnrichmentRuleID", 2, HwField::IMMEDIATE, false, false},
        {"Side", 1, HwField::SIDE, true, true},
        {"ApplSeqIndicator", 1, HwField::IMMEDIATE, false, false},
        {"ApplSeqIndicator", 1, HwField::IMMEDIATE, false, false},
        {"ValueCheckTypeValue", 1, HwField::IMMEDIATE, false, false},
        {"OrderAttributeLiquidityProvision", 1, HwField::IMMEDIATE, false, false},
        {"TimeInForce", 1, HwField::IMMEDIATE, false, false},
        {"ExecInst", 1, HwField::ASK_PRICE, true, true}, //WA, ==isBOC
        {"TradingCapacity", 1, HwField::IMMEDIATE, false, false},
        {"OrderOrigination", 1, HwField::IMMEDIATE, false, false},
        {"PartyIdInvestmentDecisionMakerQualifier", 1, HwField::IMMEDIATE, false, false},
        {"ExecutingTraderQualifier", 1, HwField::IMMEDIATE, false, false},
        {"ComplianceText", 20, HwField::IMMEDIATE, false, false},
        {"Pad7", 7, HwField::IMMEDIATE, false, false},
	
    };

    tSize =
        sizeof(myTemplateStruct) / sizeof(EpmTemplateField);
    if (tSize > MAX_FIELDS)
      on_error("tSize %u > MAX_FIELDS %u", tSize,
               MAX_FIELDS);
    memcpy(templateStruct, myTemplateStruct,
           sizeof(myTemplateStruct));
    strcpy(name, "EpmEti8PktTemplate");
    init();
  }
};

#endif
