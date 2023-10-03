#ifndef _EKA_FIRE_REPORT_H
#define _EKA_FIRE_REPORT_H


/* typedef enum { */
/*   FIRE_REPORT = 1, */
/*   EXCEPTION_REPORT = 2 */
/* } EventType; */



#define _DELAYED_CAT( _0, _1 )          _DELAYED_CAT2( _0, _1 )
#define _DELAYED_CAT2( _0, _1 )         _0 ## _1

#define _FIELD_DEF( _type, _name )      _type _name;
#define _ENUM_DEF( _key, _val )         _DELAYED_CAT( k, _key ) = _val,

// this is the very first header in the report
// it is followed by a sequence of reports
typedef struct __attribute__((packed)) {
  #define ContainerGlobalHdr_FIELD_ITER( _x )                                                                           \
            _x( EventType, type )             /* type of the report event (today only FIRE or EXCEPTION) */             \
            _x( uint32_t,  num_of_reports ) //
    ContainerGlobalHdr_FIELD_ITER( _FIELD_DEF )
} ContainerGlobalHdr;

typedef enum {
  #define ReportType_ENUM_ITER( _x )                                                                                    \
            _x( ControllerState,    1000 )                                                                              \
            _x( MdReport,           2000 )                                                                              \
            _x( SecurityCtx,        3000 )                                                                              \
            _x( FireReport,         4000 )                                                                              \
            _x( MiaxSessionCtx,     5001 )                                                                              \
            _x( SqfSessionCtx,      5002 )                                                                              \
            _x( ExceptionReport,    6000 )
    ReportType_ENUM_ITER( _ENUM_DEF )
} ReportType;

// every report is pre-pended by this header
typedef struct __attribute__((packed)) {
  #define ReportHdr_FIELD_ITER( _x )                                                                                    \
            _x( ReportType,    type )                                                                                   \
            _x( uint8_t,       idx )  /* which report out of num_of_reports (counting from 1) */                        \
            _x( size_t,        size ) /* size of the body (= number of bytes after this field in the report) */
    ReportHdr_FIELD_ITER( _FIELD_DEF )
} ReportHdr;

typedef struct __attribute__((packed)) {
  #define ControllerState_FIELD_ITER( _x )                                                                              \
            _x( uint8_t, unarm_reason )
    ControllerState_FIELD_ITER( _FIELD_DEF )
} ControllerState; // single appearence

typedef struct __attribute__((packed)) {
  #define MdReport_FIELD_ITER( _x )                                                                                     \
            _x( uint64_t,    timestamp )                                                                                \
            _x( uint64_t,    sequence )                                                                                 \
            _x( uint8_t,     side )                                                                                     \
            _x( uint64_t,    price )                                                                                    \
            _x( uint64_t,    size )                                                                                     \
            _x( uint8_t,     group_id )                                                                                 \
            _x( uint8_t,     core_id )
    MdReport_FIELD_ITER( _FIELD_DEF )
} MdReport; // single appearence

typedef struct __attribute__((packed)) {
  #define SecurityCtx_FIELD_ITER( _x )                                                                                  \
            _x( uint8_t,  lower_bytes_of_sec_id )                                                                       \
            _x( uint8_t,  ver_num )                                                                                     \
            _x( uint8_t,  size )                                                                                        \
            _x( uint16_t, ask_max_price )                                                                               \
            _x( uint16_t, bid_min_price )
    SecurityCtx_FIELD_ITER( _FIELD_DEF )
} SecurityCtx; // single appearence

// one report per fire
// always followed by session_ctx_t
typedef struct __attribute__((packed)) {
  #define FireReport_FIELD_ITER( _x )                                                                                   \
            _x( uint8_t,  reason )                                                                                      \
            _x( uint64_t, size )                                                                                        \
            _x( uint64_t, price )                                                                                       \
            _x( uint64_t, security_id )                                                                                 \
            _x( uint16_t, session ) /* = core * 128 + sess */                                                           \
            _x( uint64_t, timestamp ) /* HW timestamp of the first fire */
    FireReport_FIELD_ITER( _FIELD_DEF )
} FireReport;

// one report per fire
// always comes after (in pair with) FireReport
typedef struct __attribute__((packed)) {
  #define CommonSessionCtx_FIELD_ITER( _x )                                                                            \
            _x( uint16_t, fired_session ) /* = core * 128 + sess */                                                    \
            _x( uint64_t, cl_ord_id )
    CommonSessionCtx_FIELD_ITER( _FIELD_DEF )
} CommonSessionCtx;

typedef struct __attribute__((packed)) {
  #define MiaxSessionCtx_FIELD_ITER( _x )                                                                              \
            CommonSessionCtx_FIELD_ITER( _x )
    MiaxSessionCtx_FIELD_ITER( _FIELD_DEF )
} MiaxSessionCtx;

typedef struct __attribute__((packed)) {
  #define SqfSessionCtx_FIELD_ITER( _x )                                                                              \
            CommonSessionCtx_FIELD_ITER( _x )
    SqfSessionCtx_FIELD_ITER( _FIELD_DEF )
} SqfSessionCtx;

typedef struct __attribute__((packed)) {
  #define ExceptionReport_FIELD_ITER( _x )                                                                             \
            _x( int32_t, error_code )
    ExceptionReport_FIELD_ITER( _FIELD_DEF )
} ExceptionReport;

#undef _FIELD_DEF
#undef _ENUM_DEF

#undef _DELAYED_CAT
#undef _DELAYED_CAT2

#endif
