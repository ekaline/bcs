/**
 * @file
 *
 * This header file covers the API for the CME Exchange connectivity (Efc) feature.
 */

#ifndef __EFC_CME_H__
#define __EFC_CME_H__

#include "Epm.h"

extern "C" {

/**
 *
 */
  ssize_t efcCmeSendUntouched(EkaDev* pEkaDev, ExcConnHandle hConn,
			      const void* buffer, size_t size, int flags );
  
  ssize_t efcCmeSendSameSeq(EkaDev* pEkaDev, ExcConnHandle hConn,
			    const void* buffer, size_t size, int flags );

  ssize_t efcCmeSendIncrSeq(EkaDev* pEkaDev, ExcConnHandle hConn,
			    const void* buffer, size_t size, int flags );
 
  EkaOpResult efcCmeSetILinkAppseq(EkaDev *ekaDev,
				   ExcConnHandle hCon,
				   int32_t appSequence);
  
} // End of extern "C"

#endif
