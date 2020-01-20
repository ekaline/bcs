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
enum class EfhMsgType {
    #define EfhMsgType_ENUM_ITER( _x )                                      \
                _x( Definition,        0   )                                \
                _x( Trade                  )                                \
                _x( Quote                  )                                \
                _x( Order                  )                                \
                _x( DoneStaticDefs,    50  )                                \
                _x( FeedDown,          100 )                                \
                _x( FeedUp,            101 )
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
                /** Incremented each time a new gap is encountered so this 
                 * can be compared  with the latest from 
                 * EfhFeedStatusMsg::gapNum. */                             \
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
    #define EfhExchange_ENUM_ITER( _x )                                     \
                _x( NOM,   'Q' )                                            \
                _x( PHLX,  'P' )                                            \
                _x( MIAX,  'M' )                                            \
                _x( GEM,   'G' )                                            \
                _x( Cboe,  'C' )                                            \
                _x( One,   'O' )                                            \
                _x( Cbsx,  'W' )                                            \
                _x( Cfe,   'F' )                                            \
                _x( Cboe2, '2' )
        EfhExchange_ENUM_ITER( EKA__ENUM_DEF )
};

typedef char eka_secid_t[8];

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
                _x( eka_secid_t,     underlying )                           \
                /** Formatted as YYYYMMDD. */                               \
                _x( uint32_t,        expiryDate )                           \
                _x( uint32_t,        contractSize )                         \
                /** Divide by EFH_PRICE_SCALE. */                           \
                _x( uint32_t,        strikePrice )                          \
                _x( EfhExchange,     exchange )                             \
                _x( EfhOptionType,   optionType )
        EfhDefinitionMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhDefinitionMsg;

/*
 *
 */
enum class EfhTradeStatus : char {
    #define EfhTradeStatus_ENUM_ITER( _x )                                  \
                _x( Uninit,  '_' )                                          \
                _x( Halted,  'H' )                                          \
                _x( Pending, 'P' )                                          \
                _x( Normal,  'N' )                                          \
                _x( Closed,  'C' )
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
                _x( Reg, 1 )                                                \
                _x( Canc   )                                                \
                _x( Oseq   )                                                \
                _x( Late   )                                                \
                _x( Cnco   )                                                \
                _x( Reop   )                                                \
                _x( Sprd   )                                                \
                _x( Cmbo   )                                                \
                _x( Spim   )                                                \
                _x( Isoi   )                                                \
                _x( Bnmt   )                                                \
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

/*
 *
 */
typedef struct {
    #define EfhFeedStatusMsg_FIELD_ITER( _x )                               \
                _x( EfhMsgType, msgType )                                   \
                _x( EkaGroup,   group )                                     \
                _x( uint64_t,   gapNum )
        EfhFeedStatusMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhFeedStatusMsg;

struct EfhFeedDownMsg : public EfhFeedStatusMsg {};
struct EfhFeedUpMsg   : public EfhFeedStatusMsg {};

/**
 * This is returned when the last static definition has been replayed.
 */
typedef struct {
    #define EfhDoneStaticDefsMsg_FIELD_ITER( _x )                           \
                _x( uint8_t, padding )
        EfhDoneStaticDefsMsg_FIELD_ITER( EKA__FIELD_DEF )
} EfhDoneStaticDefsMsg;

#endif // __EFH_MSGS__H__
