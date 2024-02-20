#ifndef __EKA_MOEX_TEST_STRUCTS_H__
#define __EKA_MOEX_TEST_STRUCTS_H__

#include <gtest/gtest.h>
#include <iostream>

typedef struct {
  uint32_t BodyLen;
  uint16_t TemplateID;
  char NetworkMsgID[8];
  char Pad2[2];
} MessageHeaderInCompT;

// Structure: RequestHeader

typedef struct {
  uint32_t MsgSeqNum;
  uint32_t SenderSubID;
} RequestHeaderCompT;

typedef struct {
  MessageHeaderInCompT MessageHeaderIn;
  RequestHeaderCompT RequestHeader;
  int64_t Price;
  int64_t OrderQty;
  uint64_t ClOrdID;
  uint64_t PartyIDClientID;
  uint64_t PartyIdInvestmentDecisionMaker;
  uint64_t ExecutingTrader;
  uint32_t SimpleSecurityID;
  uint32_t MatchInstCrossID;
  uint16_t EnrichmentRuleID;
  uint8_t Side;
  uint8_t ApplSeqIndicator;
  uint8_t PriceValidityCheckType;
  uint8_t ValueCheckTypeValue;
  uint8_t OrderAttributeLiquidityProvision;
  uint8_t TimeInForce;
  uint8_t ExecInst;
  uint8_t TradingCapacity;
  uint8_t OrderOrigination;
  uint8_t PartyIdInvestmentDecisionMakerQualifier;
  uint8_t ExecutingTraderQualifier;
  char ComplianceText[20];
  char Pad7[7];
} NewOrderSingleShortRequestT;

#endif
