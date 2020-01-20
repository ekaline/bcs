/*
 * EkaMsgs.h
 *     This header file contains the api for general ekaline messages.
 */

#ifndef __EKA_MSGS_H__
#define __EKA_MSGS_H__

#include "Eka.h"

#ifdef __cplusplus
    extern "C" {
#endif

/*
 *
 */
enum class EkaEventType {
    #define EkaEventType__ENUM_ITER( _x )                                   \
        _x( FireReport,      1 )                                            \
        _x( ExceptionReport, 2 )
    EkaEventType__ENUM_ITER( EKA__ENUM_DEF )
};

/*
 *
 */
typedef struct {
    #define EkaContainerGlobalHdr_FIELD_ITER( _x )                          \
                _x( EkaEventType, type )                                    \
                _x( uint32_t,  num_of_reports )
    EkaContainerGlobalHdr_FIELD_ITER( EKA__FIELD_DEF )
} EkaContainerGlobalHdr;

typedef struct {
    #define EkaExceptionReport_FIELD_ITER( _x )                            \
            _x( int32_t, error_code )
    EkaExceptionReport_FIELD_ITER( EKA__FIELD_DEF )
} EkaExceptionReport;

#ifdef __cplusplus
    }
#endif

#endif // __EKA_MSG__H__
