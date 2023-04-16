#ifndef _EKA_FH_BOOK_H_
#define _EKA_FH_BOOK_H_

#include <inttypes.h>
#include "eka_macros.h"

#include "Efh.h"
#include "EkaDev.h"
#include "EkaFhSecurity.h"

class EkaFhGroup;

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
  virtual int invalidate() = 0;
  
/* ####################################################### */

  //----------------------------------------------------------

  EkaDev*          dev      = NULL;
  EkaLSI           grId     = -1; 
  EkaSource        exch     = EkaSource::kInvalid; 
  //----------------------------------------------------------


};

struct EkaFhNoopQuotePostProc {
  constexpr bool operator()(const EkaFhSecurity*, EfhQuoteMsg*) { return true; }
};

struct EkaFhInvertComplexAskQuotePostProc {
  constexpr bool operator()(const EkaFhSecurity* sec, EfhQuoteMsg* msg) {
    if (sec->type == EfhSecurityType::kComplex) {
      // Correct exchange's complex price conventions to match our own
      msg->askSide.price = -msg->askSide.price;
    }
    return true;
  }
};

#endif
