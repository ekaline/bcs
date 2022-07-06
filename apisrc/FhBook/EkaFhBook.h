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

/* ####################################################### */

  //----------------------------------------------------------

  EkaDev*          dev      = NULL;
  EkaLSI           grId     = -1; 
  EkaSource        exch     = EkaSource::kInvalid; 
  //----------------------------------------------------------


};

struct EkaFhNoopQuotePostProc {
  inline bool operator()(const EkaFhSecurity*, EfhQuoteMsg*) { return true; }
};

#endif
