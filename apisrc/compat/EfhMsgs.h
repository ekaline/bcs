/*
 * EfhMsgs.h
 *     This header file contains types for Ekaline feed handler specific messages.
 */

#ifndef __EFH_MSGS_H__
#define __EFH_MSGS_H__

#include "Eka.h"

#define EFH__PRICE_SCALE        10000

/*
 * $$NOTE$$ - All of these messages are standalone and therefore arent contained in a kEkaContainerMsg.
 */
enum class EfhMsgType : uint16_t {
    #define EfhMsgType_ENUM_ITER( _x )                                      \
                _x( Definition,        0   )                                \
                _x( Trade                  )                                \
                _x( Quote                  )                                \
                _x( Order                  )                                \
                _x( DoneStaticDefs,    50  )                                \
                _x( GroupStateChanged, 100 )
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
                _x( uint64_t,   queueSize )                                 \
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
                _x( Indx, 1 )                                               \
                _x( Mleg    )                                               \
                _x( Opt     )                                               \
                _x( Fut     )                                               \
                _x( Cs      )
        EfhSecurityType_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
enum class EfhOptionType : uint8_t {
    #define EfhOptionType_ENUM_ITER( _x )                                   \
                _x( Put,  0 )                                               \
                _x( Call, 1 )
        EfhOptionType_ENUM_ITER( EKA__ENUM_DEF )
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
  _x( Gap,          'G' )            \
  _x( Closed,       'C' )            \
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
enum class EfhGroupStateErrorDomain : char {
#define EfhGroupStateErrorDomain_ENUM_ITER( _x ) \
  _x( NoError,         ' ' )              \
  _x( SocketError,     'S' )              \
  _x( OSError,         'O' )              \
  _x( UpdateTimeout,   'T' )              \
  _x( CredentialError, 'C' )              \
  _x( ExchangeError,   'E' )              \
  _x( DeviceError,     'D' )
  EfhGroupStateErrorDomain_ENUM_ITER( EKA__ENUM_DEF )
};

typedef char EfhSymbol[8];

/*
 *
 */
typedef struct {
    #define EfhDefinitionMsg_FIELD_ITER( _x )                               \
                _x( EfhMsgHeader,    header )                               \
                /** If this is >= 0, then this is a second group that 
                 *  security updates can come over (useful for exchanges 
                 *  where we subscribe to both TOB and Orders. */           \
                _x( EkaGroup,        secondaryGroup )                       \
                _x( EfhSecurityType, securityType )                         \
                _x( EfhSecurityType, underlyingType )                       \
                _x( EfhSymbol,       classSymbol )                          \
                _x( EfhSymbol,       underlying )                           \
                /** Formatted as YYYYMMDD. */                               \
                _x( uint32_t,        expiryDate )                           \
                _x( uint32_t,        contractSize )                         \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( uint32_t,        strikePrice )                          \
                _x( EfhExchange,     exchange )				\
                _x( EfhOptionType,   optionType )			\
                _x( uint64_t,        opaqueAttrA )			\
                _x( uint64_t,        opaqueAttrB )
        EfhDefinitionMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhDefinitionMsg;

/*
 *
 */
enum class EfhTradeStatus : char {
    #define EfhTradeStatus_ENUM_ITER( _x )                              \
                _x( GapRecover,        'G' )                            \
		  _x( Uninit,            '_' )				\
		  _x( Halted,            'H' )				\
		  _x( Preopen,           'P' )				\
		  _x( OpeningRotation,   'O' )				\
		  _x( Normal,            'N' )				\
		  _x( Closed,            'C' )
  EfhTradeStatus_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct {
    #define EfhBookSide_FIELD_ITER( _x )                                    \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( uint32_t, price )                                       \
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
 *
 */
enum class EfhTradeCond {
    #define EfhTradeCond_ENUM_ITER( _x )                                    \
                _x( Unmapped, 0 )                                           \
                _x( Reg, 1 )                                                \
                _x( Canc   )                                                \
                _x( Oseq   )                                                \
                _x( Cncl   )                                                \
                _x( Late   )                                                \
                _x( Cnco   )                                                \
                _x( Open   )                                                \
                _x( Cnol   )                                                \
                _x( Opnl   )                                                \
                _x( Auto   )                                                \
                _x( Reop   )                                                \
                _x( Ajst   )                                                \
                _x( Sprd   )                                                \
                _x( Stdl   )                                                \
                _x( Stdp   )                                                \
                _x( Cstp   )                                                \
                _x( Cmbo   )                                                \
                _x( Spim   )                                                \
                _x( Isoi   )                                                \
                _x( Bnmt   )                                                \
                _x( Xmpt   )                                                \
                _x( Blkt   )                                                \
                _x( Exph   )                                                \
                _x( Cncp   )                                                \
                _x( Blkp   )                                                \
                _x( Expp   )   
        EfhTradeCond_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct {
    #define EfhTradeMsg_FIELD_ITER( _x )                                    \
                _x( EfhMsgHeader,      header )                             \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( uint32_t,     price )                                   \
                _x( uint32_t,     size )                                    \
                _x( EfhTradeCond, tradeCond )
        EfhTradeMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhTradeMsg;

/*
 *
 */
enum class EfhOrderSideType {
    #define EfhOrderSideType_ENUM_ITER( _x )                                \
                _x( Bid,  1  )                                              \
                _x( Ask,  -1 )
        EfhOrderSideType_ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct { 
    #define EfhOrderMsg_FIELD_ITER( _x )                                      \
                _x( EfhMsgHeader,     header )                                \
                _x( EfhTradeStatus,   tradeStatus )                           \
                _x( EfhOrderSideType, orderSide )                             \
                _x( EfhBookSide,      bookSide )
        EfhOrderMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhOrderMsg;

/**
 * This message is reported when either the EfhGroupState or the
 * EfhSystemState changes.
 *
 * When the groupState is EfhGroupState::Error:
 *
 *   - The error code is copied into "code" and "errorDomain" describes
 *     what kind of error code it is (see EfhGroupStateErrorDomain).
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
                _x( EfhGroupStateErrorDomain, errorDomain )                 \
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

#endif // __EFH_MSGS__H__
