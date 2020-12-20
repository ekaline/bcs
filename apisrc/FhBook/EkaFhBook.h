#ifndef _EKA_FH_BOOK_H_
#define _EKA_FH_BOOK_H_

#include <inttypes.h>
#include "eka_macros.h"

#include "Efh.h"
#include "EkaDev.h"

class EkaFhGroup;
class EkaFhSecurity;

class EkaFhBook {
 protected:
  EkaFhBook (EkaDev*     _dev,
  	     uint        _grId,
  	     EkaSource   _exch) {
    dev  = _dev;
    grId = _grId;
    exch = _exch;
  }

 public:
  //  virtual void      init() = 0;


  virtual int      generateOnQuote(const EfhRunCtx* pEfhRunCtx, 
				   EkaFhSecurity*   pFhSecurity, 
				   uint64_t         sequence,
				   uint64_t         timestamp,
				   uint             gapNum) {return 0;};
  
  //----------------------------------------------------------

  EkaDev*          dev      = NULL;
  EkaLSI           grId     = -1; 
  EkaSource        exch     = EkaSource::kInvalid; 
  //----------------------------------------------------------

  uint             numSecurities = 0;

  FILE*            parser_log = NULL; // used with PRINT_PARSED_MESSAGES define
};

#endif
