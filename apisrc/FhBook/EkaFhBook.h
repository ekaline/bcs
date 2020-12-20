#ifndef _EKA_FH_BOOK_H_
#define _EKA_FH_BOOK_H_

#include <inttypes.h>
#include "eka_macros.h"

#include "Efh.h"

class EkaFhGroup;
class EkaFhSecurity;
class EkaDev;


class EkaFhBook {
 protected:
  EkaFhBook (EkaDev*     _dev,
  	     EkaFhGroup* _gr,
  	     EkaSource   _exch) {
    dev  = _dev;
    gr   = _gr;
    exch = _exch;
  }

 public:
  //  virtual int      init() {return 0;}


  virtual int      generateOnQuote(const EfhRunCtx* pEfhRunCtx, 
				   EkaFhSecurity*   pFhSecurity, 
				   uint64_t         sequence,
				   uint64_t         timestamp,
				   uint             gapNum) {return 0;};
  
  //----------------------------------------------------------

  EkaDev*          dev      = NULL;
  EkaFhGroup*      gr       = NULL; 
  EkaSource        exch     = EkaSource::kInvalid; 
  //----------------------------------------------------------

  uint             numSecurities = 0;

  FILE*            parser_log = NULL; // used with PRINT_PARSED_MESSAGES define
};

#endif
