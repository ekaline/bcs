/*
 * EfcMsgs.h
 *     This header file contains the api for Ekaline firing controller messages.
 */

#ifndef __EFC_MSGS_H__
#define __EFC_MSGS_H__

#include "EkaMsgs.h"

#ifdef __cplusplus
    extern "C" {
#endif

/*
 *
 */
enum EfcReportType {
    #define EfcReportType_ENUM_ITER( _x )                                   \
                _x( ControllerState,    1000 )                              \
                _x( MdReport,           2000 )                              \
                _x( SecurityCtx,        3000 )                              \
                _x( FireReport,         4000 )                              \
                _x( MiaxSessionCtx,     5001 )                              \
                _x( SqfSessionCtx,      5002 )                              \
                _x( ExceptionReport,    6000 )
    EfcReportType_ENUM_ITER( EKA__ENUM_DEF )
};

// every report is pre-pended by this header
typedef struct __attribute__((packed)) {
    #define EfcReportHdr_FIELD_ITER( _x )                                   \
                _x( EfcReportType, type )                                   \
                /* which report out of num_of_reports (counting from 1) */  \
                _x( uint8_t,    idx )                                       \
                /* size of the body (= number of bytes after this field in the report) */   \
                _x( size_t,     size ) 
    EfcReportHdr_FIELD_ITER( EKA__FIELD_DEF )
} EfcReportHdr;

typedef struct __attribute__((packed)) {
    #define EfcControllerState_FIELD_ITER( _x )                             \
                _x( uint8_t, unarm_reason )
    EfcControllerState_FIELD_ITER( EKA__FIELD_DEF )
} EfcControllerState; // single appearence

typedef struct __attribute__((packed)) {
    #define EfcMdReport_FIELD_ITER( _x )                                    \
                _x( uint64_t,    timestamp )                                \
                _x( uint64_t,    sequence )                                 \
                _x( uint8_t,     side )                                     \
                _x( uint64_t,    price )                                    \
                _x( uint64_t,    size )                                     \
                _x( uint8_t,     group_id )                                 \
                _x( uint8_t,     core_id )
    EfcMdReport_FIELD_ITER( EKA__FIELD_DEF )
} EfcMdReport; // single appearence

typedef struct __attribute__((packed)) {
    #define EfcSecurityCtx_FIELD_ITER( _x )                                 \
                _x( uint8_t,  lower_bytes_of_sec_id )                       \
                _x( uint8_t,  ver_num )                                     \
                _x( uint8_t,  size )                                        \
                _x( uint16_t, ask_max_price )                               \
                _x( uint16_t, bid_min_price )
    EfcSecurityCtx_FIELD_ITER( EKA__FIELD_DEF )
} EfcSecurityCtx; // single appearence

// one report per fire
// always followed by session_ctx_t
typedef struct __attribute__((packed)) {
    #define EfcFireReport_FIELD_ITER( _x )                                  \
                _x( uint8_t,  reason )                                      \
                _x( uint64_t, size )                                        \
                _x( uint64_t, price )                                       \
                _x( uint64_t, security_id )                                 \
                /** = core * 128 + sess */                                  \
                _x( uint16_t, session )                                     \
                /** HW timestamp of the first fire */                       \
                _x( uint64_t, timestamp ) 
    EfcFireReport_FIELD_ITER( EKA__FIELD_DEF )
} EfcFireReport;

// one report per fire
// always comes after (in pair with) FireReport
typedef struct __attribute__((packed)) {
    #define EfcCommonSessionCtx_FIELD_ITER( _x )                            \
            /* = core * 128 + sess */                                       \
            _x( uint16_t, fired_session )                                   \
            _x( uint64_t, cl_ord_id )
    EfcCommonSessionCtx_FIELD_ITER( EKA__FIELD_DEF )
} EfcCommonSessionCtx;

typedef struct __attribute__((packed)) {
    #define EfcMiaxSessionCtx_FIELD_ITER( _x )                              \
                EfcCommonSessionCtx_FIELD_ITER( _x )
    EfcMiaxSessionCtx_FIELD_ITER( EKA__FIELD_DEF )
} EfcMiaxSessionCtx;

typedef struct __attribute__((packed)) {
    #define EfcSqfSessionCtx_FIELD_ITER( _x )                               \
                EfcCommonSessionCtx_FIELD_ITER( _x )
    EfcSqfSessionCtx_FIELD_ITER( EKA__FIELD_DEF )
} EfcSqfSessionCtx;


#ifdef __cplusplus
    }
#endif

#endif // __Efc_MSGS_H__

