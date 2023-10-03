/* Copyright (c) Ekaline, 2013-2018. All rights reserved.
 * PROVIDED "AS IS" WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED.
 * See LICENSE file for details.
 */

#ifndef EFH_MESSAGE_H
#define EFH_MESSAGE_H

#include <stdint.h>

#define EFH_MESSAGE_SIZE 128
#define EFH_PRICE_SCALE 10000

enum efhm_type
{
    EFHM_DEFINITION_CBOE = 0,
    EFHM_TRADE           = 1,
    EFHM_QUOTE           = 2,
    EFHM_ORDER           = 3,
    EFHM_DEFINITION_NOM  = 4,
    EFHM_DEFINITION_BATS = 5,
    EFHM_DEFINITION_MIAX = 6,
    EFHM_DEFINITION_XDP  = 8,
    EFHM_DEFINITION_XDPC = 10,
    EFHM_DEFINITION_GEM  = 11,
    EFHM_DEFINITION_TOPO = 12,
    EFHM_FEED_UP         = 100,
    EFHM_FEED_DOWN       = 101,
    EFHM_FEED_INITED     = 102,
    EFHM_NO_MESSAGE      = 200,
};

struct efhm_header
{
    uint8_t  type; /* see enum efhm_type */
    uint8_t  group;
    uint16_t pad_;
    uint32_t UnderlyingId;
    uint64_t SecurityId;
    uint64_t SequenceNumber;
    uint64_t TimeStamp;
}  __attribute__((packed));

struct efh_message
{
    uint8_t type;
    char data[EFH_MESSAGE_SIZE-1];
}  __attribute__((packed));

// see also "CBOE Streaming Current Market V1.3", table 7 - Security Types
enum efh_security_type
{
    EFH_SECTYPE_INDX = 1,
    EFH_SECTYPE_MLEG,
    EFH_SECTYPE_OPT,
    EFH_SECTYPE_FUT,
    EFH_SECTYPE_CS
};

// see also "CBOE Streaming Current Market V1.3", table 10 - Security Put/Call
// values are defined by CBOE
enum efh_security_put_call
{
    EFH_SECURITY_PUT  = 0,
    EFH_SECURITY_CALL = 1
};

// see also "CBOE Streaming Current Market V1.3", table 8 - Security Exchanges
// values are defined by CBOE
enum efh_security_exchange
{
    EFH_EXCHANGE_CBOE  = 'C',
    EFH_EXCHANGE_ONE   = 'O',
    EFH_EXCHANGE_CBSX  = 'W',
    EFH_EXCHANGE_CFE   = 'F',
    EFH_EXCHANGE_CBOE2 = '2'
};

struct efhm_def_leg
{
    uint8_t     LegValid;       // see enum efh_def_leg_valid
    uint8_t     LegSide;        // see enum efh_def_leg_side
    uint8_t     _pad[6];
    uint32_t    LegRatioQty;
    uint32_t    LegSecurityId;
};

enum efh_def_leg_valid
{
    EFH_DEF_LEG_VALID = 1,
    EFH_DEF_LEG_INVALID = 255,
};

enum efh_def_leg_side
{
    EFH_DEF_LEG_SIDE_BUY  = 'B',
    EFH_DEF_LEG_SIDE_SELL = 'S',
};

struct efhm_def_leg_xdp
{
    uint32_t  LegSecurityId;
    uint16_t  LegRatioQty;
    uint8_t   LegSide;          // see enum efh_def_leg_side
    uint8_t   LegSecurityType;  // O - Options Series, E - Equity stock
};

enum
{
    EFH_DEF_LEGS     = 8,
    EFH_DEF_LEGS_XDP = 5,
};

struct efhm_definition_cboe
{
    struct efhm_header h;
    uint8_t   Symbol[256];      // null-terminated
    uint64_t  MaturityDate;
    uint8_t   UnderlyingSymbol[8];
    uint8_t   UnderlyingType;   // see enum efh_security_type
    uint8_t   _pad[3];
    uint32_t  ContractSize;
    uint8_t   SecurityType;     // see enum efh_security_type
    uint8_t   SecurityExchange; // see enum efh_security_exchange
    uint8_t   TargetLocationId; // ASCII '0'..'9', see CBOE docs
    uint8_t   PutOrCall;        // see enum efh_security_put_call
    uint32_t  StrikePrice;      // divide by EFH_PRICE_SCALE
    struct efhm_def_leg Legs[EFH_DEF_LEGS];
};

struct efhm_definition_nom
{
    struct efhm_header h;
    uint8_t   Symbol[8];    // null-terminated
    uint64_t  MaturityDate;
    uint8_t   UnderlyingSymbol[16];
    uint32_t  StrikePrice;  // divide by EFH_PRICE_SCALE
    uint8_t   PutOrCall;    // see enum efh_security_put_call
};

struct efhm_definition_bats
{
    struct efhm_header h;
    uint8_t   OsiSymbol[22]; // null-terminated
};

struct efhm_definition_miax
{
    struct efhm_header h;
    uint8_t   Symbol[8];    // null-terminated
    uint64_t  MaturityDate; // byte 0 - YY, byte 1 - MM, byte 2 - DD (decimal)
    uint8_t   UnderlyingSymbol[16];
    uint32_t  StrikePrice;  // divide by EFH_PRICE_SCALE
    uint8_t   PutOrCall;    // see enum efh_security_put_call
};

// for XDP all array fields are null-terminated
struct efhm_definition_xdp
{
    struct efhm_header h;
    uint8_t   StrikePrice[16];
    uint8_t   UnderlyingSymbol[16];
    uint8_t   SymbolRoot[8];
    uint8_t   MaturityDate[8];
    uint8_t   PutOrCall;
    uint8_t   PriceScale;
    uint16_t  StreamId;
};

struct efhm_definition_xdp_complex
{
    struct efhm_header h;
    uint8_t   ComplexSymbol[24];
    uint64_t  ComplexIndex;
    uint16_t  StreamId;
    uint8_t   _pad[6];
    struct efhm_def_leg_xdp Legs[EFH_DEF_LEGS_XDP];
};

struct efhm_definition_gem
{
    struct efhm_header h;
    uint8_t Symbol[8];
    uint64_t MaturityDate;
    uint32_t StrikePrice;
    uint8_t PutOrCall;
    uint8_t Source;
    uint8_t UnderlyingSymbol[16];
    uint8_t TradingType;
    uint16_t ContractSize;
    uint8_t ClosingType;
    uint8_t Tradable;
    uint8_t MPV;
    uint8_t ClosingOnly;
};

struct efhm_definition_topo
{
    struct efhm_header h;
    uint8_t Symbol[8];
    uint64_t MaturityDate;
    uint32_t StrikePrice;
    uint8_t PutOrCall;
    uint8_t Source;
    uint8_t UnderlyingSymbol[16];
    uint8_t TradingType;
    uint8_t ClosingType;
    uint8_t Tradable;
    uint8_t MPV;
};

enum efh_trade_status
{
    EFH_TRADE_UNINIT    = '_',
    EFH_TRADE_HALTED    = 'H',
    EFH_TRADE_PENDING   = 'P',
    EFH_TRADE_NORMAL    = 'N',
    EFH_TRADE_CLOSED    = 'C',
};

/* struct efhm_quote */
/* { */
/*     struct efhm_header h; */
/*     uint8_t   TradeStatus;  // see enum efh_trade_status */
/*     uint8_t   _pad[3]; */
/*     uint32_t  BidPrice;     // divide by EFH_PRICE_SCALE */
/*     uint32_t  BidSize; */
/*     uint32_t  BidOrderSize; */
/*     uint32_t  BidAoNSize; */
/*     uint32_t  BidCustomerSize; */
/*     uint32_t  BidCustomerAoNSize; */
/*     uint32_t  BidBDSize; */
/*     uint32_t  BidBDAoNSize; */
/*     uint32_t  AskPrice;     // divide by EFH_PRICE_SCALE */
/*     uint32_t  AskSize; */
/*     uint32_t  AskOrderSize; */
/*     uint32_t  AskAoNSize; */
/*     uint32_t  AskCustomerSize; */
/*     uint32_t  AskCustomerAoNSize; */
/*     uint32_t  AskBDSize; */
/*     uint32_t  AskBDAoNSize; */
/* }; */
struct efhm_quote {
    struct efhm_header h;
    uint8_t   TradeStatus;  // see enum efh_trade_status
    uint8_t   _pad[3];
    uint32_t  BidPrice;     // divide by EFH_PRICE_SCALE
    uint32_t  BidSize;
    uint32_t  AskPrice;     // divide by EFH_PRICE_SCALE
    uint32_t  AskSize;
};


// see also "CBOE Streaming Current Market V1.3.8", table 19 - Trade Conditions
enum efh_trade_condition
{
    EFH_TRADECOND_REG = 1,
    EFH_TRADECOND_CANC,
    EFH_TRADECOND_OSEQ,
    EFH_TRADECOND_LATE,
    EFH_TRADECOND_CNCO,
    EFH_TRADECOND_REOP,
    EFH_TRADECOND_SPRD,
    EFH_TRADECOND_CMBO,
    EFH_TRADECOND_SPIM,
    EFH_TRADECOND_ISOI,
    EFH_TRADECOND_BNMT,
    EFH_TRADECOND_BLKT,
    EFH_TRADECOND_EXPH,
    EFH_TRADECOND_CNCP,
    EFH_TRADECOND_BLKP,
    EFH_TRADECOND_EXPP
};

struct efhm_trade
{
    struct efhm_header h;
    uint32_t  Price;            // divide by EFH_PRICE_SCALE
    uint32_t  Size;
    uint8_t   TradeCondition;   // see enum efh_trade_condition
};

enum efh_order_side
{
    EFH_ORDER_BID = 1,
    EFH_ORDER_ASK = -1
};

struct efhm_order
{
    struct efhm_header h;
    uint8_t   TradeStatus;  // see enum efh_trade_status
    uint8_t   OrderType;    // always 1
    int8_t    OrderSide;    // see enum efh_order_side
    uint8_t   _pad;
    uint32_t  Price;        // divide by EFH_PRICE_SCALE
    uint32_t  Size;
    uint32_t  AoNSize;
    uint32_t  CustomerSize;
    uint32_t  CustomerAoNSize;
    uint32_t  BDSize;
    uint32_t  BDAoNSize;
}   __attribute__((packed));

struct efhm_feed
{
    uint8_t type;
    uint8_t group;
};

#endif /* EFH_MESSAGE_H */
