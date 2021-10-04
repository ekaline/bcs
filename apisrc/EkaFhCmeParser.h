#ifndef _EKA_FH_CME_PARSER_H_
#define _EKA_FH_CME_PARSER_H_

#include "EkaFhTypes.h"
#include "eka_macros.h"
#include "EfhMsgs.h"

//#define EFH_CME_STRIKE_PRICE_SCALE 1
//#define EFH_CME_ORDER_PRICE_SCALE  1

#define EFH_CME_STRIKE_PRICE_SCALE 1
//#define EFH_CME_ORDER_PRICE_SCALE  100000000
#define EFH_CME_ORDER_PRICE_SCALE    1e9

namespace Cme {

  enum class MsgId : uint16_t {
			       ChannelReset4                             = 4,
			       AdminHeartbeat12                        = 12,
			       AdminLogin15                            = 15,
			       AdminLogout16                           = 16,
			       MDInstrumentDefinitionFuture27          = 27,
			       MDInstrumentDefinitionSpread29          = 29,
			       SecurityStatus30                        = 30,
			       MDIncrementalRefreshBook32              = 32,
			       MDIncrementalRefreshDailyStatistics33   = 33,
			       MDIncrementalRefreshLimitsBanding34     = 34,
			       MDIncrementalRefreshSessionStatistics35 = 35,
			       MDIncrementalRefreshVolume37            = 37,
			       SnapshotFullRefresh38                   = 38,
			       QuoteRequest39                          = 39,
			       MDInstrumentDefinitionOption41          = 41,
			       MDIncrementalRefreshTradeSummary42      = 42,
			       MDIncrementalRefreshOrderBook43         = 43,
			       SnapshotFullRefreshOrderBook44          = 44,
			       MDIncrementalRefreshBook46              = 46,
			       MDIncrementalRefreshOrderBook47         = 47,
			       MDIncrementalRefreshTradeSummary48      = 48,
			       MDIncrementalRefreshDailyStatistics49   = 49,
			       MDIncrementalRefreshLimitsBanding50     = 50,
			       MDIncrementalRefreshSessionStatistics51 = 51,
			       SnapshotFullRefresh52                   = 52,
			       SnapshotFullRefreshOrderBook53          = 53,
			       MDInstrumentDefinitionFuture54          = 54,
			       MDInstrumentDefinitionOption55          = 55,
			       MDInstrumentDefinitionSpread56          = 56,
  };

  typedef uint8_t  uInt8;
  typedef uint8_t  uint8;
  typedef uint16_t uint16;
  typedef uint32_t uint32;
  typedef uint32_t uInt32;
  typedef uint64_t uint64;

  typedef int8_t  int8;
  typedef int16_t int16;
  typedef int32_t int32;
  typedef int64_t int64;

  typedef char     	Asset_T[6];                     // length='6',   semanticType='String',       const='',  description='Asset'
  typedef char    	CFICode_T[6];                   // length='6',   semanticType='String',       const='',  description=''
  typedef char      	CHAR_T;                         // length='',    semanticType='',             const='',  description='char'
  typedef char    	Currency_T[3];                  // length='3',   semanticType='Currency',     const='',  description='Currency'
  typedef int8      	InstAttribType_CONST_T;         // length='',    semanticType='',             const='24',description='Eligibility'
  typedef int16     	Int16_T;                        // length='',    semanticType='',             const='',  description='int16'
  typedef int32     	Int32_T;                        // length='',    semanticType='',             const='',  description='int32'
  typedef int32     	Int32NULL_T;                    // length='',    semanticType='',             const='',  description=''
  typedef int8      	Int8_T;                         // length='',    semanticType='',             const='',  description='int8'
  typedef int8      	Int8NULL_T;                     // length='',    semanticType='',             const='',  description='int8'
  typedef uint16    	LocalMktDate_T;                 // length='',    semanticType='LocalMktDate', const='',  description=''
  typedef char      	MDEntryTypeChannelReset_CONST_T;// length='',    semanticType='',             const='J', description='Channel'
  typedef char      	MDEntryTypeLimits_CONST_T;      // length='',    semanticType='',             const='g', description='MDEntryTypeLimits'
  typedef char      	MDEntryTypeTrade_CONST_T;       // length='',    semanticType='',             const='2', description='MDEntryTypeTrade'
  typedef char      	MDEntryTypeVol_CONST_T;         // length='',    semanticType='',             const='e', description='MDEntryTypeVol'
  typedef char    	MDFeedType_T[3];                // length='3',   semanticType='String',       const='',  description=''
  typedef int8      	MDUpdateActionNew_CONST_T;      // length='',    semanticType='',             const='0', description='MDUpdateActionNew'
  typedef int8      	MDUpdateTypeNew_T;              // length='',    semanticType='',             const='0', description='MDUpdateTypeNew'
  typedef char    	QuoteReqId_T[23];               // length='23',  semanticType='String',       const='',  description=''
  typedef char    	SecurityExchange_T[4];          // length='4',   semanticType='Exchange',     const='',  description=''
  typedef char    	SecurityGroup_T[6];             // length='6',   semanticType='String',       const='',  description=''
  typedef char      	SecurityIDSource_CONST_T;       // length='1',   semanticType='char',         const='8', description='SecurityIDSource'
  typedef char    	SecuritySubType_T[5];           // length='5',   semanticType='String',       const='',  description=''
  typedef char    	SecurityType_T[6];              // length='6',   semanticType='String',       const='',  description='SecurityType'
  typedef char   	Symbol_T[20];                   // length='20',  semanticType='String',       const='',  description='Symbol'
  typedef char    	Text_T[180];                    // length='180', semanticType='String',       const='',  description='Text'
  typedef char    	UnderlyingSymbol_T[20];         // length='20',  semanticType='String',       const='',  description=''
  typedef char    	UnitOfMeasure_T[30];            // length='30',  semanticType='String',       const='',  description=''
  typedef char      	UserDefinedInstrument_T;        // length='1',   semanticType='char',         const='',  description=''
  typedef uint32    	uInt32_T;                       // length='',    semanticType='',             const='',  description='uInt32'
  typedef uint32    	uInt32NULL_T;                   // length='',    semanticType='',             const='',  description='uInt32'
  typedef uint64    	uInt64_T;                       // length='',    semanticType='',             const='',  description='uInt64'
  typedef uint64    	uInt64NULL_T;                   // length='',    semanticType='',             const='',  description='uInt64'
  typedef uint8     	uInt8_T;                        // length='',    semanticType='',             const='',  description='uInt8'
  typedef uint8     	uInt8NULL_T;                    // length='',    semanticType='',             const='',  description='uInt8NULL'


  typedef int64 Decimal9_T;      // Decimal9" description="Decimal with constant exponent -9
  typedef int64 Decimal9NULL_T;  // Optional Decimal with constant exponent -9

  typedef int32 DecimalQty_T;    // A number representing quantity" semanticType="Qty" constant exponent -4

  typedef int64 FLOAT_T;         // Decimal, constant exponent -7

  struct MaturityMonthYear_T {     // "Year, Month and Date" semanticType="MonthYear"
    uint16 year;  // description="YYYY"
    uint8  month; // description="MM"
    uint8  day;   // description="DD"
    uint8  week;  // description="WW"
  }  __attribute__((packed));

  typedef int64 PRICE_T;         // Decimal, constant exponent -7

  // Price with constant exponent -9
  typedef int64 PRICE9_T;        // Decimal, constant exponent -9

  // Price NULL
  typedef int64 PRICENULL_T;     // Decimal, constant exponent -7

  //Optional Price with constant exponent -9
  typedef int64 PRICENULL9_T;    // Decimal, constant exponent -9

  struct groupSize_T { // Repeating group dimensions
    uint16 blockLength;
    uint8  numInGroup;
  } __attribute__((packed));

  struct groupSize8Byte_T { // 8 Byte aligned repeating group dimensions
    uint16 blockLength;
    uint8  numInGroup; // offset="7"
  } __attribute__((packed));

  struct groupSizeEncoding_T { // Repeating group dimensions
    uint16 blockLength;
    uint16 numInGroup;
  } __attribute__((packed));

  typedef uint16_t MessageLen;

  struct messageHeader_T { // Template ID and length of message root. Always prepended by uin16 MessageLen!!!
    uint16       blockLength;
    uint16       templateId;
    uint16       schemaId;
    uint16       version;
  } __attribute__((packed));

  struct MsgHdr { // more convinient than "messageHeader"
    uint16_t       size;
    uint16_t       blockLen;
    MsgId          templateId;
    uint16_t       schemaId;
    uint16_t       version;
  } __attribute__((packed));


  enum class AggressorSide : uInt8NULL_T {
					  NoAggressor = 0,
					  Buy       = 1,
					  Sell      = 2
  };

  enum class AggressorSide_T : uInt8 {
				      Activation = 5,
				      LastEligibleTradeDate       = 7,
  };

  enum class HaltReason_T : uInt8 {
				   GroupSchedule = 0,
				   SurveillanceIntervention = 1,
				   MarketEvent = 2,
				   InstrumentActivation = 3,
				   InstrumentExpiration = 4,
				   Unknown = 5,
				   RecoveryInProcess = 6
  };

  enum class LegSide_T : uInt8 {
				BuySide = 1,
				SellSide = 2
  };

  enum class MDEntryType_T : CHAR_T {
				     Bid = '0',
				     Offer = '1',
				     Trade = '2',
				     OpenPrice = '4',
				     SettlementPrice = '6',
				     TradingSessionHighPrice = '7',
				     TradingSessionLowPrice = '8',
				     ClearedVolume = 'B',
				     OpenInterest = 'C',
				     ImpliedBid = 'E',
				     ImpliedOffer = 'F',
				     BookReset = 'J',
				     SessionHighBid = 'N',
				     SessionLowOffer = 'O',
				     FixingPrice = 'W',
				     ElectronicVolume = 'e',
				     ThresholdLimitsandPriceBandVariation = 'g'
  };

  enum class MDEntryTypeBook_T : CHAR_T {
					 Bid = '0',
					 Offer = '1',
					 ImpliedBid = 'E',
					 ImpliedOffer = 'F',
					 BookReset = 'J',
  };

  enum class MDEntryTypeDailyStatistics_T : CHAR_T {
						    SettlementPrice = '6',
						    ClearedVolume = 'B',
						    OpenInterest = 'C',
						    FixingPrice = 'W'
  };

  enum class MDEntryTypeStatistics_T : CHAR_T {
					       OpenPrice = '4',
					       HighTrade = '7',
					       LowTrade = '8',
					       HighestBid = 'N',
					       LowestOffer = 'O'
  };

  enum class MDUpdateAction_T : uInt8 {
				       New = 0,
				       Change = 1,
				       Delete = 2,
				       DeleteThru = 3,
				       DeleteFrom = 4,
				       Overlay = 5
  };

  enum class OpenCloseSettlFlag_T : uInt8NULL_T {
						 DailyOpenPrice = 0,
						 IndicativeOpeningPrice = 5
  };

  enum class OrderUpdateAction_T : uInt8 {
					  New = '0',
					  Update = '1',
					  Delete = '2',
  };

  enum class PutOrCall_T : uInt8 {
				  Put = 0,
				  Call = 1
  };

  inline bool operator==(const PutOrCall_T &lhs, const uint8_t &rhs) noexcept {
    return (uint8_t)lhs == rhs;
  }
  inline bool operator==(const uint8_t &lhs, const PutOrCall_T &rhs) noexcept {
    return lhs == (uint8_t)rhs;
  }
  inline bool operator!=(const PutOrCall_T &lhs, const uint8_t &rhs) noexcept {
    return !operator==(lhs, rhs);
  }
  inline bool operator!=(const uint8_t &lhs, const PutOrCall_T &rhs) noexcept {
    return !operator==(lhs, rhs);
  }
  enum class SecurityTradingEvent_T : uInt8 {
					     NoEvent = 0,
					     NoCancel = 1,
					     ResetStatistics = 4,
					     ImpliedMatchingON = 5,
					     ImpliedMatchingOFF = 6,
  };

  enum class SecurityTradingStatus_T : uInt8NULL_T {
						    TradingHalt = 2,
						    Close = 4,
						    NewPriceIndication = 15,
						    ReadyToTrade = 17,
						    NotAvailableForTrading = 18,
						    UnknownorInvalid = 20,
						    PreOpen = 21,
						    PreCross = 24,
						    Cross = 25,
						    PostClose = 26,
						    NoChange = 103,
  };

#define SecurityTradingStatus2STR(x)					\
  x == SecurityTradingStatus_T::TradingHalt              ? "TradingHalt"	    : \
    x == SecurityTradingStatus_T::Close                  ? "Close"                  : \
    x == SecurityTradingStatus_T::NewPriceIndication     ? "NewPriceIndication"     : \
    x == SecurityTradingStatus_T::ReadyToTrade           ? "ReadyToTrade"           : \
    x == SecurityTradingStatus_T::NotAvailableForTrading ? "NotAvailableForTrading" : \
    x == SecurityTradingStatus_T::UnknownorInvalid       ? "UnknownorInvalid"       : \
    x == SecurityTradingStatus_T::PreOpen                ? "PreOpen"                : \
    x == SecurityTradingStatus_T::PreCross               ? "PreCross"               : \
    x == SecurityTradingStatus_T::Cross                  ? "Cross"	            : \
    x == SecurityTradingStatus_T::PostClose              ? "PostClose"	            : \
    x == SecurityTradingStatus_T::NoChange               ? "NoChange"	            : \
    "UNEXPECTED"


  enum class SecurityUpdateAction_T : CHAR_T {
					      Add = 'A',
					      Delete = 'D',
					      Modify = 'M',
  };

  // Bitmap. Fields are bit indxes
  enum class InstAttribValue_T : uInt32 {
					 ElectronicMatchEligible = 0,
					 OrderCrossEligible = 1,
					 BlockTradeEligible = 2,
					 EFPEligible = 3,
					 EBFEligible = 4,
					 EFSEligible = 5,
					 EFREligible = 6,
					 OTCEligible = 7,
					 iLinkIndicativeMassQuotingEligible = 8,
					 NegativeStrikeEligible = 9,
					 NegativePriceOutrightEligible = 10,
					 IsFractional = 11,
					 VolatilityQuotedOption = 12,
					 RFQCrossEligible = 13,
					 ZeroPriceOutrightEligible = 14,
					 DecayingProductEligibility = 15,
					 VariableProductEligibility = 16,
					 DailyProductEligibility = 17,
					 GTOrdersEligibility = 18,
					 ImpliedMatchingEligibility = 19,
					 TriangulationEligible = 20,
					 VariableCabEligible = 21
  };

  // Bitmap. Fields are bit indxes
  typedef uInt8 MatchEventIndicator_T;
  enum class MatchEventIndicator_Bit : uInt8 {
					      LastTradeMsg     = 0x01, // = 0
					      LastVolumeMsg  = 0x02, // = 1
					      LastQuoteMsg   = 0x04, // = 2
					      LastStatsMsg   = 0x08,  // = 3
					      LastImpliedMsg = 0x10, // = 4
					      RecoveryMsg    = 0x20, // = 5
					      Reserved       = 0x40, // = 6
					      EndOfEvent     = 0x80  // = 7
  };

  // Bitmap. Fields are bit indxes
  enum class SettlPriceType_T: uInt8 {
				      FinalDaily = 0,
				      Actual = 1,
				      Rounded = 2,
				      Intraday = 3,
				      ReservedBits = 4,
				      NullValue = 7,
  };


  struct Definition_commonMainBlock {
    MatchEventIndicator_T         	MatchEventIndicator;
    uInt32NULL_T                  	TotNumReports;
  };
  struct Refresh_commonMainBlock {
    uInt32_T                      	LastMsgSeqNumProcessed;
    uInt32_T                      	TotNumReports;
  };
  
  struct MDInstrumentDefinitionFuture27_mainBlock {
    MatchEventIndicator_T         	MatchEventIndicator;
    uInt32NULL_T                  	TotNumReports;
    SecurityUpdateAction_T        	SecurityUpdateAction;
    uInt64_T                      	LastUpdateTime;
    SecurityTradingStatus_T       	MDSecurityTradingStatus;
    Int16_T                       	ApplID;
    uInt8_T                       	MarketSegmentID;
    uInt8_T                       	UnderlyingProduct;
    SecurityExchange_T            	SecurityExchange;
    SecurityGroup_T               	SecurityGroup;
    Asset_T                       	Asset;
    Symbol_T                      	Symbol;
    Int32_T                       	SecurityID;
    //	SecurityIDSource_CONST_T            	SecurityIDSource;
    SecurityType_T                	SecurityType;
    CFICode_T                     	CFICode;
    MaturityMonthYear_T           	MaturityMonthYear;
    Currency_T                    	Currency;
    Currency_T                    	SettlCurrency;
    CHAR_T                        	MatchAlgorithm;
    uInt32_T                      	MinTradeVol;
    uInt32_T                      	MaxTradeVol;
    PRICE_T                       	MinPriceIncrement;
    FLOAT_T                       	DisplayFactor;
    uInt8NULL_T                   	MainFraction;
    uInt8NULL_T                   	SubFraction;
    uInt8NULL_T                   	PriceDisplayFormat;
    UnitOfMeasure_T               	UnitOfMeasure;
    PRICENULL_T                   	UnitOfMeasureQty;
    PRICENULL_T                   	TradingReferencePrice;
    SettlPriceType_T              	SettlPriceType;
    Int32NULL_T                   	OpenInterestQty;
    Int32NULL_T                   	ClearedVolume;
    PRICENULL_T                   	HighLimitPrice;
    PRICENULL_T                   	LowLimitPrice;
    PRICENULL_T                   	MaxPriceVariation;
    Int32NULL_T                   	DecayQuantity;
    LocalMktDate_T                	DecayStartDate;
    Int32NULL_T                   	OriginalContractSize;
    Int32NULL_T                   	ContractMultiplier;
    Int8NULL_T                    	ContractMultiplierUnit;
    Int8NULL_T                    	FlowScheduleType;
    PRICENULL_T                   	MinPriceIncrementAmount;
    UserDefinedInstrument_T       	UserDefinedInstrument;
    LocalMktDate_T                	TradingReferenceDate;

  } __attribute__((packed));


  struct MDInstrumentDefinitionOption41_mainBlock {
    MatchEventIndicator_T         	MatchEventIndicator;
    uInt32NULL_T                  	TotNumReports;
    SecurityUpdateAction_T        	SecurityUpdateAction;
    uInt64_T                      	LastUpdateTime;
    SecurityTradingStatus_T       	MDSecurityTradingStatus;
    Int16_T                       	ApplID;
    uInt8_T                       	MarketSegmentID;
    uInt8_T                       	UnderlyingProduct;
    SecurityExchange_T            	SecurityExchange;
    SecurityGroup_T               	SecurityGroup;
    Asset_T                       	Asset;
    Symbol_T                      	Symbol;
    Int32_T                       	SecurityID;
    //	SecurityIDSource_CONST_T            	SecurityIDSource;
    SecurityType_T                	SecurityType;
    CFICode_T                     	CFICode;
    PutOrCall_T                   	PutOrCall;
    MaturityMonthYear_T           	MaturityMonthYear;
    Currency_T                    	Currency;
    PRICENULL_T                   	StrikePrice;
    Currency_T                    	StrikeCurrency;
    Currency_T                    	SettlCurrency;
    PRICENULL_T                   	MinCabPrice;
    CHAR_T                        	MatchAlgorithm;
    uInt32_T                      	MinTradeVol;
    uInt32_T                      	MaxTradeVol;
    PRICENULL_T                   	MinPriceIncrement;
    PRICENULL_T                   	MinPriceIncrementAmount;
    FLOAT_T                       	DisplayFactor;
    Int8NULL_T                    	TickRule;
    uInt8NULL_T                   	MainFraction;
    uInt8NULL_T                   	SubFraction;
    uInt8NULL_T                   	PriceDisplayFormat;
    UnitOfMeasure_T               	UnitOfMeasure;
    PRICENULL_T                   	UnitOfMeasureQty;
    PRICENULL_T                   	TradingReferencePrice;
    SettlPriceType_T              	SettlPriceType;
    Int32NULL_T                   	ClearedVolume;
    Int32NULL_T                   	OpenInterestQty;
    PRICENULL_T                   	LowLimitPrice;
    PRICENULL_T                   	HighLimitPrice;
    UserDefinedInstrument_T       	UserDefinedInstrument;
    LocalMktDate_T                	TradingReferenceDate;
  } __attribute__((packed));

  struct MDInstrumentDefinitionFuture54_mainBlock {
    MatchEventIndicator_T         	MatchEventIndicator;
    uInt32NULL_T                  	TotNumReports;
    SecurityUpdateAction_T        	SecurityUpdateAction;
    uInt64_T                      	LastUpdateTime;
    SecurityTradingStatus_T       	MDSecurityTradingStatus;
    Int16_T                       	ApplID;
    uInt8_T                       	MarketSegmentID;
    uInt8_T                       	UnderlyingProduct;
    SecurityExchange_T            	SecurityExchange;
    SecurityGroup_T               	SecurityGroup;
    Asset_T                       	Asset;
    Symbol_T                      	Symbol;
    Int32_T                       	SecurityID;
    //	SecurityIDSource_CONST_T            	SecurityIDSource;
    SecurityType_T                	SecurityType;
    CFICode_T                     	CFICode;
    MaturityMonthYear_T           	MaturityMonthYear;
    Currency_T                    	Currency;
    Currency_T                    	SettlCurrency;
    CHAR_T                        	MatchAlgorithm;
    uInt32_T                      	MinTradeVol;
    uInt32_T                      	MaxTradeVol;
    PRICE9_T                      	MinPriceIncrement;
    Decimal9_T                    	DisplayFactor;
    uInt8NULL_T                   	MainFraction;
    uInt8NULL_T                   	SubFraction;
    uInt8NULL_T                   	PriceDisplayFormat;
    UnitOfMeasure_T               	UnitOfMeasure;
    Decimal9NULL_T                	UnitOfMeasureQty;
    PRICENULL9_T                  	TradingReferencePrice;
    SettlPriceType_T              	SettlPriceType;
    Int32NULL_T                   	OpenInterestQty;
    Int32NULL_T                   	ClearedVolume;
    PRICENULL9_T                  	HighLimitPrice;
    PRICENULL9_T                  	LowLimitPrice;
    PRICENULL9_T                  	MaxPriceVariation;
    Int32NULL_T                   	DecayQuantity;
    LocalMktDate_T                	DecayStartDate;
    Int32NULL_T                   	OriginalContractSize;
    Int32NULL_T                   	ContractMultiplier;
    Int8NULL_T                    	ContractMultiplierUnit;
    Int8NULL_T                    	FlowScheduleType;
    PRICENULL9_T                  	MinPriceIncrementAmount;
    UserDefinedInstrument_T       	UserDefinedInstrument;
    LocalMktDate_T                	TradingReferenceDate;

  } __attribute__((packed));

  struct MDInstrumentDefinitionOption55_mainBlock {
    MatchEventIndicator_T         	MatchEventIndicator;
    uInt32NULL_T                  	TotNumReports;
    SecurityUpdateAction_T        	SecurityUpdateAction;
    uInt64_T                      	LastUpdateTime;
    SecurityTradingStatus_T       	MDSecurityTradingStatus;
    Int16_T                       	ApplID;
    uInt8_T                       	MarketSegmentID;
    uInt8_T                       	UnderlyingProduct;
    SecurityExchange_T            	SecurityExchange;
    SecurityGroup_T               	SecurityGroup;
    Asset_T                       	Asset;
    Symbol_T                      	Symbol;
    Int32_T                       	SecurityID;
    //	SecurityIDSource_CONST_T            	SecurityIDSource;
    SecurityType_T                	SecurityType;
    CFICode_T                     	CFICode;
    PutOrCall_T                   	PutOrCall;
    MaturityMonthYear_T           	MaturityMonthYear;
    Currency_T                    	Currency;
    PRICENULL9_T                  	StrikePrice;
    Currency_T                    	StrikeCurrency;
    Currency_T                    	SettlCurrency;
    PRICENULL9_T                  	MinCabPrice;
    CHAR_T                        	MatchAlgorithm;
    uInt32_T                      	MinTradeVol;
    uInt32_T                      	MaxTradeVol;
    PRICENULL9_T                  	MinPriceIncrement;
    PRICENULL9_T                  	MinPriceIncrementAmount;
    Decimal9_T                    	DisplayFactor;
    Int8NULL_T                    	TickRule;
    uInt8NULL_T                   	MainFraction;
    uInt8NULL_T                   	SubFraction;
    uInt8NULL_T                   	PriceDisplayFormat;
    UnitOfMeasure_T               	UnitOfMeasure;
    Decimal9NULL_T                	UnitOfMeasureQty;
    PRICENULL9_T                  	TradingReferencePrice;
    SettlPriceType_T              	SettlPriceType;
    Int32NULL_T                   	ClearedVolume;
    Int32NULL_T                   	OpenInterestQty;
    PRICENULL9_T                  	LowLimitPrice;
    PRICENULL9_T                  	HighLimitPrice;
    UserDefinedInstrument_T       	UserDefinedInstrument;
    LocalMktDate_T                	TradingReferenceDate;
  } __attribute__((packed));

  struct MDInstrumentDefinitionSpread56_mainBlock {
    MatchEventIndicator_T         	MatchEventIndicator;
    uInt32NULL_T                  	TotNumReports;
    SecurityUpdateAction_T        	SecurityUpdateAction;
    uInt64_T                      	LastUpdateTime;
    SecurityTradingStatus_T       	MDSecurityTradingStatus;
    Int16_T                       	ApplID;
    uInt8_T                       	MarketSegmentID;
    uInt8NULL_T                   	UnderlyingProduct;
    SecurityExchange_T            	SecurityExchange;
    SecurityGroup_T               	SecurityGroup;
    Asset_T                       	Asset;
    Symbol_T                      	Symbol;
    Int32_T                       	SecurityID;
    /* SecurityIDSource_CONST_T            	SecurityIDSource; */
    SecurityType_T                	SecurityType;
    CFICode_T                     	CFICode;
    MaturityMonthYear_T           	MaturityMonthYear;
    Currency_T                    	Currency;
    SecuritySubType_T             	SecuritySubType;
    UserDefinedInstrument_T       	UserDefinedInstrument;
    CHAR_T                        	MatchAlgorithm;
    uInt32_T                      	MinTradeVol;
    uInt32_T                      	MaxTradeVol;
    PRICENULL9_T                  	MinPriceIncrement;
    Decimal9_T                    	DisplayFactor;
    uInt8NULL_T                   	PriceDisplayFormat;
    PRICENULL9_T                  	PriceRatio;
    Int8NULL_T                    	TickRule;
    UnitOfMeasure_T               	UnitOfMeasure;
    PRICENULL9_T                  	TradingReferencePrice;
    SettlPriceType_T              	SettlPriceType;
    Int32NULL_T                   	OpenInterestQty;
    Int32NULL_T                   	ClearedVolume;
    PRICENULL9_T                  	HighLimitPrice;
    PRICENULL9_T                  	LowLimitPrice;
    PRICENULL9_T                  	MaxPriceVariation;
    uInt8NULL_T                   	MainFraction;
    uInt8NULL_T                   	SubFraction;
    LocalMktDate_T                	TradingReferenceDate;
  } __attribute__((packed));

  struct MDInstrumentDefinitionSpread56_legEntry {
    Int32_T                       	LegSecurityID;
    /* SecurityIDSource_CONST_T       	LegSecurityIDSource; */
    LegSide_T                     	LegSide;
    Int8_T                        	LegRatioQty;
    PRICENULL9_T                  	LegPrice;
    DecimalQty_T                  	LegOptionDelta;
  } __attribute__((packed));

  struct QuoteRequest39_mainBlock {
    uInt64_T                      	TransactTime;
    QuoteReqId_T                  	QuoteReqID;
    MatchEventIndicator_T         	MatchEventIndicator;
  } __attribute__((packed));

  struct QuoteRequest39_legEntry {
    Symbol_T                      	Symbol;
    Int32_T                       	SecurityID;
    Int32NULL_T                   	OrderQty;
    Int8_T                        	QuoteType;
    Int8NULL_T                    	Side;
  } __attribute__((packed));

  struct PktHdr {
    uint32_t seq;
    uint64_t time; // ns since epoch
  } __attribute__((packed));
  

#define MDpdateAction2STR(x)				\
  x ==  MDUpdateAction_T::New         ? "New"        :	\
    x == MDUpdateAction_T::Change     ? "Change"     :	\
    x == MDUpdateAction_T::Delete     ? "Delete"     :	\
    x == MDUpdateAction_T::DeleteThru ? "DeleteThru" :	\
    x == MDUpdateAction_T::DeleteFrom ? "DeleteFrom" :	\
    x == MDUpdateAction_T::Overlay    ? "Overlay"    :	\
    "UNEXPECTED_ACTION"

#define MDEntryTypeBook2STR(x)					\
  x == MDEntryTypeBook_T::Bid            ? "Bid"          :	\
    x == MDEntryTypeBook_T::Offer        ? "Offer"        :	\
    x == MDEntryTypeBook_T::ImpliedBid   ? "ImpliedBid"   :	\
    x == MDEntryTypeBook_T::ImpliedOffer ? "ImpliedOffer" :	\
    x == MDEntryTypeBook_T::BookReset    ? "BookReset"    :	\
    "UNEXPECTED_ENTRY_TYPE_BOOK"

#define MDEntryType2STR(x)						\
  x == MDEntryType_T::Bid                                     ? "Bid" :	\
    x == MDEntryType_T::Offer                                 ? "Offer" : \
    x == MDEntryType_T::Trade                                 ? "Trade" : \
    x == MDEntryType_T::OpenPrice                             ? "OpenPrice" : \
    x == MDEntryType_T::SettlementPrice                       ? "SettlementPrice" : \
    x == MDEntryType_T::TradingSessionHighPrice               ? "TradingSessionHighPrice" : \
    x == MDEntryType_T::TradingSessionLowPrice                ? "TradingSessionLowPrice" : \
    x == MDEntryType_T::ClearedVolume                         ? "ClearedVolume" : \
    x == MDEntryType_T::OpenInterest                          ? "OpenInterest" : \
    x == MDEntryType_T::ImpliedBid                            ? "ImpliedBid" : \
    x == MDEntryType_T::ImpliedOffer                          ? "ImpliedOffer" : \
    x == MDEntryType_T::BookReset                             ? "BookReset" : \
    x == MDEntryType_T::SessionHighBid                        ? "SessionHighBid" : \
    x == MDEntryType_T::SessionLowOffer                       ? "SessionLowOffer" : \
    x == MDEntryType_T::FixingPrice                           ? "FixingPrice" :	\
    x == MDEntryType_T::ElectronicVolume                      ? "ElectronicVolume" : \
    x == MDEntryType_T::ThresholdLimitsandPriceBandVariation  ? "ThresholdLimitsandPriceBandVariation" : \
    "UNEXPECTED_ENTRY_TYPE"

#define EKA_CME_ENTRY_TYPE(x)					\
  x == '0'   ? "Offer" :					\
    x == '1'   ? "Ask  " :					\
    x == '2'   ? "Trade" :					\
    x == '4'   ? "OpenPrice            " :			\
    x == '6'   ? "SettlementPrice      " :			\
    x == '7'   ? "TradingSessionHighPrice" :			\
    x == '8'   ? "TradingSessionLowPrice " :			\
    x == 'B'   ? "ClearedVolume       " :			\
    x == 'C'   ? "OpenInterest        " :			\
    x == 'E'   ? "ImpliedBid          " :			\
    x == 'F'   ? "ImpliedOffer        " :			\
    x == 'J'   ? "BookReset           " :			\
    x == 'N'   ? "SessionHighBid      " :			\
    x == 'O'   ? "SessionLowOffer     " :			\
    x == 'W'   ? "FixingPrice         " :			\
    x == 'e'   ? "ElectronicVolume    " :			\
    x == 'g'   ? "ThresholdLimitsandPriceBandVariation" :	\
    "UNEXPECTED_ENTRY_TYPE"

  struct OptionDefinitionUnderlyingEntry {
    int32                                 UnderlyingSecurityID;
    UnderlyingSymbol_T                    UnderlyingSymbol;
  } __attribute__((packed));

  struct IncrementalRefreshMdEntry {
    PRICENULL_T                   	MDEntryPx;
    Int32NULL_T                   	MDEntrySize;
    Int32_T                       	SecurityID;
    uInt32_T                      	RptSeq;
    Int32NULL_T                   	NumberOfOrders;
    uInt8_T                       	MDPriceLevel;
    MDUpdateAction_T              	MDUpdateAction;
    MDEntryTypeBook_T             	MDEntryType;
  } __attribute__((packed));

  struct OptionDefinition55RelatedInstrumentEntry {
    Int32_T                       	SecurityID;
    //SecurityIDSource_CONST_T            	SecurityIDSource;
    Symbol_T                      	Symbol;
  } __attribute__((packed));


  struct MDSnapshotFullRefreshMdEntry {
    PRICENULL9_T                  	MDEntryPx;
    Int32NULL_T                   	MDEntrySize;
    Int32NULL_T                   	NumberOfOrders;
    Int8NULL_T                    	MDPriceLevel;
    LocalMktDate_T                	TradingReferenceDate;
    OpenCloseSettlFlag_T          	OpenCloseSettlFlag;
    SettlPriceType_T              	SettlPriceType;
    MDEntryType_T                 	MDEntryType;
  } __attribute__((packed));

  struct SnapshotFullRefresh52_mainBlock {
    uInt32_T                      	LastMsgSeqNumProcessed;
    uInt32_T                      	TotNumReports;
    Int32_T                       	SecurityID;
    uInt32_T                      	RptSeq;
    uInt64_T                      	TransactTime;
    uInt64_T                      	LastUpdateTime;
    LocalMktDate_T                	TradeDate;
    SecurityTradingStatus_T       	MDSecurityTradingStatus;
    PRICENULL9_T                  	HighLimitPrice;
    PRICENULL9_T                  	LowLimitPrice;
    PRICENULL9_T                  	MaxPriceVariation;
  } __attribute__((packed));

  struct MDIncrementalRefreshBook46_mainBlock {
    uint64_t              TransactTime;
    MatchEventIndicator_T MatchEventIndicator;
  } __attribute__((packed));


  struct MDIncrementalRefreshTradeSummary48_mainBlock {
    uint64_t              TransactTime;
    MatchEventIndicator_T MatchEventIndicator;
  } __attribute__((packed));

  struct MDIncrementalRefreshTradeSummary48_mdEntry {
    PRICE9_T                      	MDEntryPx;
    Int32_T                       	MDEntrySize;
    Int32_T                       	SecurityID;
    uInt32_T                      	RptSeq;
    Int32_T                       	NumberOfOrders;
    AggressorSide_T               	AggressorSide;
    MDUpdateAction_T              	MDUpdateAction;
    //    MDEntryTypeTrade_CONST_T            	MDEntryType;
    uInt32NULL_T                  	MDTradeEntryID;
  } __attribute__((packed));

  inline auto getSide39(int rfqSide) {
    switch (rfqSide) {
    case 1 : return EfhOrderSide::kBid;
    case 2 : return EfhOrderSide::kAsk;
    case 8 : return EfhOrderSide::kOther; //CROSS
    default: return EfhOrderSide::kErr;
    }
  }
  
  inline auto getSide46(MDEntryTypeBook_T mdEntryTypeBook) {
    switch (mdEntryTypeBook) {
    case MDEntryTypeBook_T::Bid          : return SideT::BID;
    case MDEntryTypeBook_T::Offer        : return SideT::ASK;

    case MDEntryTypeBook_T::BookReset    :
      /* TBD */
    case MDEntryTypeBook_T::ImpliedBid   :
    case MDEntryTypeBook_T::ImpliedOffer : return SideT::OTHER;
    default:
      on_error("Unexpected MDEntryType \'%c\'",(char)mdEntryTypeBook);
    }
    return SideT::OTHER;
  }

  inline auto getSide52(MDEntryType_T mdEntryType) {
    switch (mdEntryType) {
    case MDEntryType_T::Bid   : return SideT::BID;
    case MDEntryType_T::Offer : return SideT::ASK;

    case MDEntryType_T::Trade :
    case MDEntryType_T::OpenPrice :
    case MDEntryType_T::SettlementPrice :
    case MDEntryType_T::TradingSessionHighPrice :
    case MDEntryType_T::TradingSessionLowPrice :
    case MDEntryType_T::ClearedVolume :
    case MDEntryType_T::OpenInterest :
    case MDEntryType_T::ImpliedBid :
    case MDEntryType_T::ImpliedOffer :
    case MDEntryType_T::SessionHighBid :
    case MDEntryType_T::SessionLowOffer :
    case MDEntryType_T::FixingPrice :
    case MDEntryType_T::ElectronicVolume :
    case MDEntryType_T::ThresholdLimitsandPriceBandVariation :
    case MDEntryType_T::BookReset : return SideT::OTHER;

    default:
      on_error("Unexpected MDEntryType \'%c\'",(char)mdEntryType);
    } //switch mdEntryType
    return SideT::OTHER;
  }
  

  inline auto getEfhOptionType(PutOrCall_T type) {
    switch (type) {
    case PutOrCall_T::Put  : return EfhOptionType::kPut;
    case PutOrCall_T::Call : return EfhOptionType::kCall;
    default                : return EfhOptionType::kErr;
    }
  }

  inline uint32_t printPkt(const uint8_t* pkt, const int payloadLen, uint64_t pktNum) {
    auto p {pkt};
    auto pktHdr {reinterpret_cast<const PktHdr*>(p)};

    auto ts        {pktHdr->time};
    auto sequence  {pktHdr->seq};
    printf ("pktNum=%ju,pktTime=%ju,pktSeq=%u\n",
	    pktNum,ts,sequence);

    p += sizeof(*pktHdr);

    while (p - pkt < payloadLen) {
      auto m {p};
      auto msgHdr {reinterpret_cast<const MsgHdr*>(m)};
      printf ("\tMsgId=%d,size=%u,blockLen=%u\n",
	      (int)msgHdr->templateId,
	      msgHdr->size,
	      msgHdr->blockLen);
      m += sizeof(*msgHdr);

      switch (msgHdr->templateId) {
	/* ##################################################################### */
      case MsgId::QuoteRequest39 : {
	auto rootBlock {reinterpret_cast<const QuoteRequest39_mainBlock*>(m)};
	auto quoteReqID {std::string(rootBlock->QuoteReqID,sizeof(rootBlock->QuoteReqID))};

	printf ("\t\tQuoteRequest39: TransactTime=%jx, MatchEventIndicator=0x%x,quoteReqID=\'%s\'\n",
		rootBlock->TransactTime,rootBlock->MatchEventIndicator,quoteReqID.c_str());
	m += msgHdr->blockLen;
	/* ------------------------------- */
	auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize);
	/* ------------------------------- */
	for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	  auto e {reinterpret_cast<const QuoteRequest39_legEntry*>(m)};
	  auto symbol {std::string(e->Symbol,sizeof(e->Symbol))};

	  printf ("\t\t\tSymbol=\'%s\',secId=%8d,QuoteType=0x%x,side=0x%x,size=0x%x\n",
		  symbol.c_str(),
		  e->SecurityID,
		  e->QuoteType,
		  e->Side,
		  e->OrderQty);

	  m += pGroupSize->blockLength;
	}
      }
	break;
	/* ##################################################################### */
      case MsgId::SnapshotFullRefresh52 : {
	auto rootBlock {reinterpret_cast<const SnapshotFullRefresh52_mainBlock*>(m)};
	/* ------------------------------- */
	printf ("\t\tSnapshotFullRefresh52: secId=%8d,LastMsgSeqNumProcessed=%u,TotNumReports=%u,%s,%s\n",
		rootBlock->SecurityID,
		rootBlock->LastMsgSeqNumProcessed,
		rootBlock->TotNumReports,
		SecurityTradingStatus2STR(rootBlock->MDSecurityTradingStatus),
		ts_ns2str(rootBlock->LastUpdateTime).c_str());
		
      }
	break;
	/* ##################################################################### */
      case MsgId::MDIncrementalRefreshBook46 : {
	/* ------------------------------- */
	auto rootBlock {reinterpret_cast<const MDIncrementalRefreshBook46_mainBlock*>(m)};
	printf ("\t\tIncrementalRefreshBook46: TransactTime=%jx, MatchEventIndicator=0x%x\n",
		rootBlock->TransactTime,rootBlock->MatchEventIndicator);
	m += msgHdr->blockLen;
	/* ------------------------------- */
	auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize);
	/* ------------------------------- */
	for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	  auto e {reinterpret_cast<const IncrementalRefreshMdEntry*>(m)};
	  printf ("\t\t\tsecId=%8d,%6s,%5s,plvl=%u,p=%16jd,s=%d\n",
		  e->SecurityID,
		  MDpdateAction2STR(e->MDUpdateAction),
		  MDEntryTypeBook2STR(e->MDEntryType),
		  e->MDPriceLevel,
		  (int64_t) (e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE),
		  e->MDEntrySize);

	  m += pGroupSize->blockLength;
	}
      }
	break;
	/* ##################################################################### */
      case MsgId::MDIncrementalRefreshTradeSummary48 : {
	/* ------------------------------- */
	auto rootBlock {reinterpret_cast<const MDIncrementalRefreshTradeSummary48_mainBlock*>(m)};
	printf ("\t\tIncrementalRefreshTradeSummary48: TransactTime=%jx, MatchEventIndicator=0x%x\n",
		rootBlock->TransactTime,rootBlock->MatchEventIndicator);
	m += msgHdr->blockLen;
	/* ------------------------------- */
	auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize);
	/* ------------------------------- */
	// if (pGroupSize->numInGroup != 1)
	//   printf(YEL "WARNING: MDIncrementalRefreshTradeSummary48: numInGroup = %d\n" RESET,
	// 	 pGroupSize->numInGroup);
	for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	  auto e {reinterpret_cast<const MDIncrementalRefreshTradeSummary48_mdEntry*>(m)};
	  printf ("\t\t\t");
	  printf ("secId=%8d,",e->SecurityID);
	  printf ("%16jd,",(int64_t) (e->MDEntryPx / EFH_CME_ORDER_PRICE_SCALE));
	  printf ("%d,",e->MDEntrySize);
	  printf ("%s",MDpdateAction2STR(e->MDUpdateAction));
	  printf ("\n");
	  m += pGroupSize->blockLength;
	}
      }
	break;	
	/* ##################################################################### */
      case MsgId::MDInstrumentDefinitionFuture54 : {
	auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionFuture54_mainBlock*>(m)};
	/* ------------------------------- */
	auto symbol           {std::string(rootBlock->Symbol,          sizeof(rootBlock->Symbol))};
	auto cfiCode          {std::string(rootBlock->CFICode,         sizeof(rootBlock->CFICode))};
	auto securityExchange {std::string(rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange))};
	auto asset            {std::string(rootBlock->Asset,           sizeof(rootBlock->Asset))};
	auto securityType     {std::string(rootBlock->SecurityType,    sizeof(rootBlock->SecurityType))};
	auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

	printf ("\t\tDefinitionFuture54: \'%s\',\'%s\',\'%s\',\'%s\',%d,\'%s\',%04u-%02u-%02u--%02u\n",
		securityExchange.c_str(),
		asset.c_str(),
		symbol.c_str(),
		securityType.c_str(),
		rootBlock->SecurityID,
		cfiCode.c_str(),
		pMaturity->year,pMaturity->month,pMaturity->day,pMaturity->week
		);
      }
	break;
	/* ##################################################################### */
      case MsgId::MDInstrumentDefinitionOption55 : {
	auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionOption55_mainBlock*>(m)};
	/* ------------------------------- */
	auto symbol           {std::string(rootBlock->Symbol,	       sizeof(rootBlock->Symbol))};
	auto cfiCode          {std::string(rootBlock->CFICode,	       sizeof(rootBlock->CFICode))};
	auto securityExchange {std::string(rootBlock->SecurityExchange,sizeof(rootBlock->SecurityExchange))};
	auto asset            {std::string(rootBlock->Asset,	       sizeof(rootBlock->Asset))};
	auto securityType     {std::string(rootBlock->SecurityType,    sizeof(rootBlock->SecurityType))};
	auto pMaturity {reinterpret_cast<const MaturityMonthYear_T*>(&rootBlock->MaturityMonthYear)};

	auto putOrCall {getEfhOptionType(rootBlock->PutOrCall)};

	if (putOrCall == EfhOptionType::kErr) {
	  hexDump("DefinitionOption55",msgHdr,msgHdr->size);	  
	  on_error("Unexpected rootBlock->PutOrCall = 0x%x",(int)rootBlock->PutOrCall);
	}
	printf("\t\tDefinitionOption55: \'%s\',\'%s\',\'%s\',%s,\'%s\',%d,\'%s\',%04u-%02u-%02u--%02u, %ju (%jd)\n",
	       securityExchange.c_str(),
	       asset.c_str(),
	       symbol.c_str(),
	       putOrCall == EfhOptionType::kPut ? "PUT" : putOrCall == EfhOptionType::kCall ? "CALL" : "ERR",
	       securityType.c_str(),
	       rootBlock->SecurityID,
	       cfiCode.c_str(),
	       pMaturity->year,pMaturity->month,pMaturity->day,pMaturity->week,
	       rootBlock->StrikePrice,
	       (int64_t)(rootBlock->StrikePrice / EFH_CME_ORDER_PRICE_SCALE / 1e9 * rootBlock->DisplayFactor)
	       );
      }
	break;
	/* ##################################################################### */
      case MsgId::MDInstrumentDefinitionSpread56 : {
	auto rootBlock {reinterpret_cast<const MDInstrumentDefinitionOption55_mainBlock*>(m)};
	printf ("\t\tMDInstrumentDefinitionSpread56: MatchEventIndicator=0x%x\n",
		rootBlock->MatchEventIndicator);
	m += msgHdr->blockLen;

	/* ------------------------------- */
	auto pGroupSize_EventType {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize_EventType);
	for (uint i = 0; i < pGroupSize_EventType->numInGroup; i++) 
	  m += pGroupSize_EventType->blockLength;
	/* ------------------------------- */		
	auto pGroupSize_MDFeedType {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize_MDFeedType);
	for (uint i = 0; i < pGroupSize_MDFeedType->numInGroup; i++) 
	  m += pGroupSize_MDFeedType->blockLength;
	/* ------------------------------- */
	auto pGroupSize_InstAttribType {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize_InstAttribType);
	for (uint i = 0; i < pGroupSize_InstAttribType->numInGroup; i++)
	  m += pGroupSize_InstAttribType->blockLength;
	/* ------------------------------- */
	auto pGroupSize_LotTypeRules {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize_LotTypeRules);
	for (uint i = 0; i < pGroupSize_LotTypeRules->numInGroup; i++)
	  m += pGroupSize_LotTypeRules->blockLength;
	/* ------------------------------- */
	auto pGroupSize {reinterpret_cast<const groupSize_T*>(m)};
	m += sizeof(*pGroupSize);
	for (uint i = 0; i < pGroupSize->numInGroup; i++) {
	  auto e {reinterpret_cast<const MDInstrumentDefinitionSpread56_legEntry*>(m)};
	  printf ("\t\t\tsecId=%8d,side=%d,LegRatioQty=%d\n",
		  e->LegSecurityID,
		  (int)e->LegSide,
		  e->LegRatioQty);
	  m += pGroupSize->blockLength;
	}
      }
	break;
	/* ##################################################################### */
		
      default:
	break;
		
      } // switch()
      /* ----------------------------- */

      p += msgHdr->size;
    } // while()
	
    return sequence;
  } // printPkt()

  inline bool isDefinitionMsg(MsgId msgId) {
    switch (msgId) {
    case MsgId::MDInstrumentDefinitionFuture27 :
    case MsgId::MDInstrumentDefinitionSpread29 :
    case MsgId::MDInstrumentDefinitionOption41 :
    case MsgId::MDInstrumentDefinitionFuture54 :
    case MsgId::MDInstrumentDefinitionOption55 :
    case MsgId::MDInstrumentDefinitionSpread56 :
      return true;
    default:
      return false;
    }
  }

  inline bool isSnapshotMsg(MsgId msgId) {
    switch (msgId) {
    case MsgId::SnapshotFullRefresh38 :
    case MsgId::SnapshotFullRefreshOrderBook44 :
    case MsgId::SnapshotFullRefresh52 :
    case MsgId::SnapshotFullRefreshOrderBook53 :
      return true;
    default:
      return false;
    }
  }

} //name space Cme


#endif
