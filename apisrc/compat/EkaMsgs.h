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
        _x( FireEvent,      1 )                                            \
        _x( EpmEvent,       2 )                                            \
        _x( ExceptionEvent, 3 )                                            \
        _x( FastCancelEvent,4 )                                            \
        _x( NewsEvent,      5 )
    EkaEventType__ENUM_ITER( EKA__ENUM_DEF )
};

#define EkaEventType2STR(x)				     \
      x == EkaEventType::kFireEvent         ? "FireEvent" :	\
	x == EkaEventType::kEpmEvent        ? "EpmEvent" :		\
	x == EkaEventType::kExceptionEvent  ? "ExceptionEvent" :	\
	x == EkaEventType::kFastCancelEvent ? "FastCancelEvent" :	\
	x == EkaEventType::kNewsEvent       ? "NewsEvent" :	\
	"UnknownReport"
      
      
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
  // exception vector from FPGA HW diagnostics
    #define EkaExceptionReport_FIELD_ITER( _x )                            \
            _x( uint64_t, error_code )
    EkaExceptionReport_FIELD_ITER( EKA__FIELD_DEF )
} EkaExceptionReport;

#ifdef __cplusplus
    }
#endif

#endif // __EKA_MSG__H__
