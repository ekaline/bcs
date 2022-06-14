/*
 * EfhMsgs.h
 *     This header file contains types for Ekaline feed handler specific messages.
 */

#ifndef __EFH_MSGS_H__
#define __EFH_MSGS_H__

#include <time.h>
#include "Eka.h"

#define EFH__PRICE_SCALE        10000
#define EFH__MAX_COMPLEX_LEGS   40u

/*
 * $$NOTE$$ - All of these messages are standalone and therefore arent contained in a kEkaContainerMsg.
 */
enum class EfhMsgType : uint16_t {
    #define EfhMsgType_ENUM_ITER( _x )                                      \
                _x( Invalid, 0        )                                     \
                _x( OptionDefinition  )                                     \
                _x( FutureDefinition  )                                     \
                _x( ComplexDefinition )                                     \
                _x( AuctionUpdate     )                                     \
                _x( Trade             )                                     \
                _x( Quote             )                                     \
                _x( Order             )                                     \
                _x( DoneStaticDefs    )                                     \
                _x( GroupStateChanged )
        EfhMsgType_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct {
    #define EfhMsgHeader_FIELD_ITER( _x )                                   \
                _x( EfhMsgType, msgType )                                   \
                _x( EkaGroup,   group )                                     \
                _x( uint32_t,   underlyingId )                              \
                _x( uint64_t,   securityId )                                \
                _x( uint64_t,   sequenceNumber )                            \
                _x( uint64_t,   timeStamp )                                 \
                _x( uint64_t,   deltaNs )                                   \
                /** Incremented each time a new gap is encountered so this 
                 * can be compared  with the latest from 
                 * EfhGroupStateChangedMsg::code. */                        \
                _x( uint32_t,   gapNum )
         EfhMsgHeader_FIELD_ITER( EKA__FIELD_DEF )
} EfhMsgHeader;

/*
 *
 */
enum class EfhSecurityType : uint8_t {
    #define EfhSecurityType_ENUM_ITER( _x )                                 \
                _x( Invalid, 0 )                                            \
                _x( Index      )                                            \
                _x( Stock      )                                            \
                _x( Option     )                                            \
                _x( Future     )                                            \
                _x( Rfq        )                                            \
                _x( Complex    )
        EfhSecurityType_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
enum class EfhOptionType : uint8_t {
    #define EfhOptionType_ENUM_ITER( _x )                                   \
                _x( Err,  2 )                                               \
                _x( Put,  0 )                                               \
                _x( Call, 1 )
        EfhOptionType_ENUM_ITER( EKA__ENUM_DEF )
};

// These characters are chosen to match the CME CFI code.
enum class EfhOptionStyle : char {
#define EfhOptionStyle_ENUM_ITER( _x )          \
  _x( Invalid,  char(0) )                       \
  _x( Unknown,  'X' )                           \
  _x( American, 'A' )                           \
  _x( European, 'E' )
EfhOptionStyle_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
enum class EfhExchange : char {
#define EfhExchange_ENUM_ITER( _x )		\
  _x( NOM,   'Q' )				\
    _x( PHLX,  'X' )				\
    _x( MIAX,  'M' )				\
    _x( GEM,   'S' )				\
    _x( ISE,   'Y' )				\
    _x( Cboe,  'W' )				\
    _x( One,   'O' )				\
    _x( BATS,  'Z' )				\
    _x( Cfe,   'F' )				\
    _x( Cboe2, 'D' )				\
    _x( AOE,   'A' )				\
    _x( PCX,   'P' )				\
    _x( BOX,   'B' )				\
    _x( CME,   'C' )				\
    _x( BX,    'T' )				\
    _x( EDGX,  'E' )				\
    _x( MCRY,  'K' )				\
    _x( PEARL, 'R' )				\
    _x( EMLD,  'G' )				\
    _x( Unknown, ' ' )
  EfhExchange_ENUM_ITER( EKA__ENUM_DEF )
    };

/**
 * Current state of the exchange system.
 *
 * The trading day is divided up into time periods (the upper-case letters on
 * top) and various events happen throughout the day (e.g., market closing)
 * that move us to the next period. Those events are represented by the
 * lower-case letters on the bottom. This enumeration describes the time
 * periods (the upper-case letters), *not* the events that cause the transition.
 * Nevertheless, it is easier to describe the time periods by describing the
 * events that cause their transition. The time periods are called the
 * "System State" after a message type having the same name in the NASDAQ
 * family of feed protocols.
 *
 *      I     P     O     T     L     C     A
 *   *-----*-----*-----*-----*-----*-----*-----*
 *   p     s     o     t     l     c     e     x
 *
 *   p - start of the process, occuring in this example before the exchange
 *       has communicated any messages. After this, we're in the 'I' (Initial)
 *       state. This state always exists if the process is started early
 *       enough.
 *
 *   s - Start of system hours, i.e., when the exchange sends its first
 *       message. After this, we are in the 'P' (Preopen) state. Preopen is the
 *       time period after which the exchange has started sending us messages,
 *       but before the exchange's official "opening" process. This state may
 *       not exist on an exchange, e.g., if it communicates nothing interesting
 *       before opening or trading.
 *
 *   o - Start of exchange opening process. After this, we're in the 'O'
 *       (Opening) state. This state may not exist on an exchange if it does
 *       not have an opening process.
 *
 *   t - Start of normal trading hours. After this, we're in the 'T' (Trading)
 *       state.
 *
 *   l - Start of late trading hours (also the close of normal trading). After
 *       this, we're in the 'L' (LateTrading) state.
 *
 *   c - Close of late trading. After this, we're in the 'C' (Closed) state.
 *       In most cases, some exchange services are still available for some time
 *       after trading has ended.
 *
 *   e - End of system hours. After this, the exchange will not send any more
 *       messages and we are in the 'A' (AfterHours) state.
 *
 *   x - Process exit.
 */
enum class EfhSystemState : char {
#define EfhSystemState_ENUM_ITER( _x ) \
  _x( Unknown,     'U' )               \
  _x( Initial,     'I' )               \
  _x( Preopen,     'P' )               \
  _x( Opening,     'O' )               \
  _x( Trading,     'T' )               \
  _x( LateTrading, 'L' )               \
  _x( Closed,      'C' )               \
  _x( AfterHours,  'A' )
  EfhSystemState_ENUM_ITER( EKA__ENUM_DEF )
};

/**
 * Current state of a feed group.
 *
 *   - 'I' (Initializing): the group is in this state initially, until the
 *         first full recovery or the first error.
 *
 *   - 'N' (Normal): the group is completely recovered and operating normally.
 *
 *   - 'P' (Progressing Feed Up): the group is in recovering from MC market data:
 *         Securities become valid after getting a TOB update
 *
 *   - 'G' (Gap): a gap was detected and recovery is in progress. If an error
 *         occurs that prevents the gap from closing, it will enter the error
 *         state.
 *
 *   - 'E' (Error): the group is in this state when an error occurs during any
 *         other state.
 */
enum class EfhGroupState : char {
#define EfhGroupState_ENUM_ITER( _x ) \
  _x( Initializing, 'I' )            \
  _x( Normal,       'N' )            \
  _x( Progressing,  'P' )            \
  _x( Gap,          'G' )            \
  _x( Closed,       'C' )            \
  _x( Warning,      'W' )            \
  _x( Error,        'E' )
  EfhGroupState_ENUM_ITER( EKA__ENUM_DEF )
};

/**
 * Describes the domain of the error code when the group is in the error
 * state.
 *
 *   - SocketError, OSError: the error code is in the POSIX errno domain.
 *
 *   - UpdateTimeout: reported when the device did not receive any network
 *     packets during normal operation. The "error code" field is used to
 *     report the number of nanoseconds the feed has been silent.
 *
 *   - CredentialError: reported when there was a credential/login problem
 *     connecting to an exchange service. The error code is taken directly
 *     from the exchange message.
 *
 *   - ExchangeError: a catch-all error category when the exchange reports
 *     a non-credential error. The error code is taken directly from the
 *     exchange message.
 *
 *   - DeviceError: an Ekaline device error. The error code is in the
 *     EkaOpResult domain.
 */
enum class EfhErrorDomain : char {
#define EfhErrorDomain_ENUM_ITER( _x ) \
  _x( NoError,         ' ' )              \
  _x( SocketError,     'S' )              \
  _x( OSError,         'O' )              \
  _x( UpdateTimeout,   'T' )              \
  _x( CredentialError, 'C' )              \
  _x( ExchangeError,   'E' )              \
  _x( BackInTime,      'B' )              \
  _x( DeviceError,     'D' )
  EfhErrorDomain_ENUM_ITER( EKA__ENUM_DEF )
};

enum class EfhDayMethod : uint8_t {
#define EfhDayMethod_ENUM_ITER( _x )           \
  _x( Invalid,            0   )                \
  /* Day specified directly */                 \
  _x( DirectDay,          1 )                  \
  /* (week, day of week) e.g. (3, Monday) */   \
  _x( WeekWeekday,        2 )                  \
  /* First business day of the month */        \
  _x( MonthFirstTradeDay, 3 )                  \
  /* Last business day of the month */         \
  _x( MonthLastTradeDay,  4 )
  EfhDayMethod_ENUM_ITER( EKA__ENUM_DEF )
};

typedef struct {
    #define EfhDateComponents_FIELD_ITER( _x )                              \
                _x( uint16_t,     year )                                    \
                _x( uint8_t,      month )                                   \
                _x( uint8_t,      day )                                     \
                _x( uint8_t,      week )                                    \
                _x( uint8_t,      weekday )                                 \
                _x( EfhDayMethod, dayMethod )                               \
                _x( uint8_t,      hour )                                    \
                _x( uint8_t,      minute )                                  \
                _x( uint8_t,      second )                                  \
                _x( int8_t,       holiday )                                 \
                /* IANA name of timezone where date/time specified */       \
                _x( const char *, tz )
        EfhDateComponents_FIELD_ITER( EKA__FIELD_DEF )
} EfhDateComponents;

typedef char EfhSymbol[8];
typedef char ExchSecurityName[16];

// Provide a dummy empty type to avoid confusing preprocessor macros that may
// try to access a type of this name because EfhMsgType::kInvalid exists.
// FIXME: this should be completely empty but we added a dummy field for now,
// to avoid confusing some of the preprocessor-based code-generation logic.
typedef struct {
    #define EfhInvalidMsg_FIELD_ITER( _x )                                  \
                _x( int, dummy )
        EfhInvalidMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhInvalidMsg;

// Common fields for all security types
typedef struct {
    #define EfhSecurityDef_FIELD_ITER( _x )                                 \
                _x( EfhSecurityType,  securityType )                        \
                _x( EfhExchange,      exchange )                            \
                /** If this is >= 0, then this is a second group that
                 *  security updates can come over (useful for exchanges
                 *  where we subscribe to both TOB and Orders. */           \
                _x( EkaGroup,         secondaryGroup )                      \
                _x( EfhSecurityType,  underlyingType )                      \
                _x( EfhSymbol,        classSymbol )                         \
                _x( EfhSymbol,        underlying )                          \
                _x( ExchSecurityName, exchSecurityName )                    \
                /** Full UNIX epoch time of expiry, can be 0 */             \
                _x( time_t,           expiryTime )                          \
                /** Formatted as YYYYMMDD if expiryTime not avail */        \
                _x( uint32_t,         expiryDate )                          \
                _x( uint32_t,         contractSize )                        \
                _x( uint64_t,         opaqueAttrA )                         \
                _x( uint64_t,         opaqueAttrB )
        EfhSecurityDef_FIELD_ITER( EKA__FIELD_DEF )
} EfhSecurityDef;

/*
 *
 */
typedef struct {
    #define EfhOptionDefinitionMsg_FIELD_ITER( _x )                         \
                _x( EfhMsgHeader,    header )                               \
                /* This must be the 2nd field in any definition message */  \
                _x( EfhSecurityDef,  commonDef )                            \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( int64_t,         strikePrice )                          \
                _x( EfhOptionType,   optionType )                           \
                _x( EfhOptionStyle,  optionStyle )                          \
                _x( uint32_t,        segmentId )
        EfhOptionDefinitionMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhOptionDefinitionMsg;

typedef struct {
    #define EfhFutureDefinitionMsg_FIELD_ITER( _x )                         \
                _x( EfhMsgHeader,    header )                               \
                _x( EfhSecurityDef,  commonDef )
        EfhFutureDefinitionMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhFutureDefinitionMsg;

enum class EfhOrderSide : int8_t {
    #define EfhOrderSide_ENUM_ITER( _x )                                    \
                _x( Other,  0 )                                             \
                _x( Cross,  3 )                                             \
                _x( Err,    2 )                                             \
                _x( Bid,    1 )                                             \
                _x( Ask,   -1 )
        EfhOrderSide_ENUM_ITER( EKA__ENUM_DEF )
};

typedef struct {
    #define EfhComplexLeg_FIELD_ITER( _x )                                  \
                _x( uint64_t,        securityId )                           \
                _x( EfhSecurityType, type )                                 \
                _x( EfhOrderSide,    side )                                 \
                _x( int32_t,         ratio )                                \
                _x( int32_t,         optionDelta )                          \
                _x( int64_t,         price )
        EfhComplexLeg_FIELD_ITER( EKA__FIELD_DEF )
} EfhComplexLeg;

typedef EfhComplexLeg EfhComplexLegs[EFH__MAX_COMPLEX_LEGS];

typedef struct {
    #define EfhComplexDefinitionMsg_FIELD_ITER( _x )                        \
                _x( EfhMsgHeader,   header )                                \
                _x( EfhSecurityDef, commonDef )                             \
                _x( uint8_t,        numLegs )                               \
                _x( EfhComplexLegs, legs )
        EfhComplexDefinitionMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhComplexDefinitionMsg;

enum class EfhAuctionType : char {
    #define EfhAuctionType_ENUM_ITER( _x )                                  \
                _x( Unknown, ' ' )                                          \
                /* Below encodings are based on the Java class */           \
                /* com.lehman.cmte.amm.exchange.QuoteRequestType */         \
                _x( AllOrNone,              'A' )                           \
                _x( Exposed,                'E' )                           \
                _x( Facilitation,           'F' )                           \
                _x( Solicitation,           'S' )                           \
                _x( Open,                   'o' )                           \
                _x( Reopen,                 'r' )                           \
                /* Improvement Period (PIP) is used on BOX */               \
                _x( PriceImprovementPeriod, 'Q' )                           \
                _x( Prism,                  'p' )                           \
                _x( Cube,                   'U' )                           \
                _x( StepUpMechanism,        'V' )                           \
                _x( BatsAuctionMechanism,   'K' )                           \
                _x( AIM,                    'M' )                           \
                _x( ComplexOrderAuction,    'O' )                           \
                _x( SpreadFacilitation,     'f' )                           \
                _x( SpreadSolicitation,     's' )

        EfhAuctionType_ENUM_ITER( EKA__ENUM_DEF )
};

enum class EfhAuctionUpdateType : char {
    #define EfhAuctionUpdateType_ENUM_ITER( _x )                            \
                _x( Unknown, 'U' )                                          \
                _x( New,     'N' )                                          \
                _x( Replace, 'R' )                                          \
                _x( Delete,  'D' )
        EfhAuctionUpdateType_ENUM_ITER( EKA__ENUM_DEF )
};

enum class EfhOrderCapacity : char {
    #define EfhOrderCapacity_ENUM_ITER( _x )                                \
                _x( Unknown, 'U' )                                          \
                _x( Customer, 'C' )                                         \
                _x( BrokerDealer, 'B' )                                     \
                _x( BrokerDealerAsCustomer, 'W' )                           \
                _x( AwayBrokerDealerAsCustomer, 'V' )                       \
                _x( AwayBrokerDealer, 'Y' )                                 \
                _x( AwayNotAffiliatedMarketMaker, 'Z' )                     \
                _x( Agency, 'A' )                                           \
                _x( Principal, 'P' )                                        \
                _x( MarketMaker, 'M' )                                      \
                _x( AwayMarketMaker, 'N' )                                  \
                _x( ProfessionalCustomer, 'D' )                             \
                _x( Proprietary, 'E' )
        EfhOrderCapacity_ENUM_ITER( EKA__ENUM_DEF )
};

typedef char EfhCounterparty[8];

typedef struct {
    #define EfhAuctionUpdateMsg_FIELD_ITER( _x )                            \
                _x( EfhMsgHeader,         header )                          \
                _x( uint64_t,             auctionId )                       \
                _x( EfhAuctionType,       auctionType )                     \
                _x( EfhAuctionUpdateType, updateType)                       \
                _x( EfhOrderSide,         side )                            \
                _x( EfhOrderCapacity,     capacity )                        \
                _x( EfhSecurityType,      securityType )                    \
                _x( uint32_t,             quantity )                        \
                _x( int64_t,              price )                           \
                _x( uint64_t,             endTimeNanos )                    \
                _x( EfhCounterparty,      firmId )
        EfhAuctionUpdateMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhAuctionUpdateMsg;

/*
 *
 */
enum class EfhTradeStatus : char {
    #define EfhTradeStatus_ENUM_ITER( _x )                                  \
                _x( GapRecover,        'G' )                                \
                _x( Uninit,            '_' )                                \
                _x( Halted,            'H' )                                \
                _x( Preopen,           'P' )                                \
                _x( OpeningRotation,   'O' )                                \
                _x( Normal,            'N' )                                \
                _x( Closed,            'C' )
        EfhTradeStatus_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct {
    #define EfhBookSide_FIELD_ITER( _x )                                    \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( int64_t,  price )                                       \
                _x( uint32_t, size )                                        \
                _x( uint32_t, aoNSize )                                     \
                _x( uint32_t, customerSize )                                \
                _x( uint32_t, customerAoNSize )                             \
                _x( uint32_t, bdSize )                                      \
                _x( uint32_t, bdAoNSize )
        EfhBookSide_FIELD_ITER( EKA__FIELD_DEF )
} EfhBookSide;

/*
 *
 */
typedef struct {
    #define EfhQuoteMsg_FIELD_ITER( _x )                                    \
                _x( EfhMsgHeader,   header )                                \
                _x( EfhTradeStatus, tradeStatus )                           \
                _x( EfhBookSide ,   bidSide )                               \
                _x( EfhBookSide ,   askSide )
        EfhQuoteMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhQuoteMsg;

/*
 * Trade condition is based on OPRA Last Sale "Message Type", from OPRA
 * binary specification 3.6a, section 5.01.
 */
enum class EfhTradeCond : char {
    #define EfhTradeCond_ENUM_ITER( _x )                                    \
                _x( Unmapped, '\0' )                                        \
                _x( REG,  ' ' )                                             \
                _x( CANC, 'A' )                                             \
                _x( OSEQ, 'B' )                                             \
                _x( CNCL, 'C' )                                             \
                _x( LATE, 'D' )                                             \
                _x( CNCO, 'E' )                                             \
                _x( OPEN, 'F' )                                             \
                _x( CNOL, 'G' )                                             \
                _x( OPNL, 'H' )                                             \
                _x( AUTO, 'I' )                                             \
                _x( REOP, 'J' )                                             \
                _x( ISOI, 'S' )                                             \
                _x( SLAN, 'a' )                                             \
                _x( SLAI, 'b' )                                             \
                _x( SLCN, 'c' )                                             \
                _x( SLCI, 'd' )                                             \
                _x( SLFT, 'e' )                                             \
                _x( MLET, 'f' )                                             \
                _x( MLAT, 'g' )                                             \
                _x( MLCT, 'h' )                                             \
                _x( MLFT, 'i' )                                             \
                _x( MESL, 'j' )                                             \
                _x( TLAT, 'k' )                                             \
                _x( MASL, 'l' )                                             \
                _x( MFSL, 'm' )                                             \
                _x( TLET, 'n' )                                             \
                _x( TLCT, 'o' )                                             \
                _x( TLFT, 'p' )                                             \
                _x( TESL, 'q' )                                             \
                _x( TASL, 'r' )                                             \
                _x( TFSL, 's' )                                             \
                _x( CMBO, 't' )                                             \
                _x( MCTP, 'u' )                                             \
                /* Not a real OPRA code, Outside-Hours trade on BOX */      \
                _x( OSHT, 'z' )
        EfhTradeCond_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct {
    #define EfhTradeMsg_FIELD_ITER( _x )                                    \
                _x( EfhMsgHeader,      header )                             \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( int64_t,     price )                                    \
                _x( uint32_t,     size )                                    \
                _x( EfhTradeStatus, tradeStatus )                           \
                _x( EfhTradeCond, tradeCond )
        EfhTradeMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhTradeMsg;

/*
 *
 */
typedef struct { 
    #define EfhOrderMsg_FIELD_ITER( _x )                                      \
                _x( EfhMsgHeader,   header )                                  \
                _x( EfhTradeStatus, tradeStatus )                             \
                _x( EfhOrderSide,   orderSide )                               \
                _x( EfhBookSide,    bookSide )
        EfhOrderMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhOrderMsg;

/**
 * This message is reported when either the EfhGroupState or the
 * EfhSystemState changes.
 *
 * When the groupState is EfhGroupState::Error:
 *
 *   - The error code is copied into "code" and "errorDomain" describes
 *     what kind of error code it is (see EfhErrorDomain).
 *
 *   - If the error is related to a particular service (e.g., a failure on
 *     a recovery service socket), the service that failed is reported
 *     in the "service" field. This will usually be EkaServiceType::FeedRecovery,
 *     EkaServiceType::FeedSnapshot, or EkaServiceType::Heartbeat.
 *
 * When the groupState is EfhGroupState::Gap, the gap number is reported
 * using the "code" field.
 */
typedef struct {
    #define EfhGroupStateChangedMsg_FIELD_ITER( _x )                        \
                _x( EfhMsgType,               msgType )                     \
                _x( EkaGroup,                 group )                       \
                _x( EfhGroupState,            groupState )                  \
                _x( EfhSystemState,           systemState )                 \
                _x( EfhErrorDomain,           errorDomain )                 \
                _x( EkaServiceType,           service )                     \
                _x( int64_t,                  code )
        EfhGroupStateChangedMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhGroupStateChangedMsg;

/**
 * This is returned when the last static definition has been replayed.
 */
typedef struct {
    #define EfhDoneStaticDefsMsg_FIELD_ITER( _x )                           \
                _x( uint8_t, padding )
        EfhDoneStaticDefsMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhDoneStaticDefsMsg;


enum class EfhExchangeErrorCode : int64_t {
  #define EfhExchangeErrorCode_ENUM_ITER(_x)       \
    _x ( NoError, 0 )                        \
    _x ( InvalidFieldFormat )                \
    _x ( InvalidUserPasswd )                 \
    _x ( InvalidSession )                    \
    _x ( InvalidSessionProtocolVersion )     \
    _x ( InvalidApplicationProtocolVersion ) \
    _x ( SessionInUse )                      \
    _x ( ServiceLimitExhausted )             \
    _x ( CountLimitExceeded )                \
    _x ( OperationAlreadyInProgress )        \
    _x ( InvalidSequenceRange )              \
    _x ( ServiceCurrentlyUnavailable )       \
    _x ( RequestNotServed )                  \
    _x ( UnexpectedResponse )                \
      /* BOX exchange doesnt send predefined error code, but rather explicit text description */ \
    _x ( ExplicitBoxError )                  \
    _x ( Unknown )
  EfhExchangeErrorCode_ENUM_ITER( EKA__ENUM_DEF )
};


#endif // __EFH_MSGS__H__
