/*
 * EfcCtxs.h
 *     This header file contains various core macros and routines.
 */
 
#ifndef __EFC_Ctxs_H__
#define __EFC_Ctxs_H__

#include "Exc.h"     /* For ExcSessionId.  */

/** This is the price in fixed format.  It should be divided by 100 to convert
 *  back to floating point. */
typedef uint16_t FixedPrice;

/*
 * 
 */
typedef struct { 
    #define SecCtx_FIELD_ITER( _x )                                     \
                _x( FixedPrice, bidMinPrice    )                        \
                _x( FixedPrice, askMaxPrice    )                        \
                _x( uint8_t, bidSize           )                        \
                _x( uint8_t, askSize           )                        \
                _x( uint8_t, versionKey        )                        \
                _x( uint8_t, lowerBytesOfSecId )
        SecCtx_FIELD_ITER( EKA__FIELD_DEF ) 
} SecCtx; 

/*
 *
 */
typedef struct { 
    #define SesCtx_FIELD_ITER( _x )                                     \
                _x( uint64_t,     clOrdId )                             \
                _x( ExcSessionId, nextSessionId )                       \
                _x( uint32_t, equote_mpid_sqf_badge )
        SesCtx_FIELD_ITER( EKA__FIELD_DEF ) 
} SesCtx; 

typedef struct { 
    #define EfcStratGlobCtx_FIELD_ITER( _x )                                     \
                _x( uint8_t, enable_strategy                   )        \
                _x( uint8_t, report_only                       )	\
                _x( uint8_t, debug_always_fire_on_unsubscribed )        \
                _x( uint8_t, debug_always_fire                 )        \
                _x( uint32_t, max_size                         )        \
                _x( uint64_t, watchdog_timeout_sec             )
        EfcStratGlobCtx_FIELD_ITER( EKA__FIELD_DEF ) 
} EfcStratGlobCtx;

#include "EkaCtxsExt.inl"  /* Exchange specific ctxs. */

#endif // __EKA_CORE_H__

