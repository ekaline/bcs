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

    if (dev->print_parsed_messages) {
      std::string parsedMsgFileName = std::string(EKA_EXCH_DECODE(exch)) + std::to_string(grId) + std::string("_PARSED_MESSAGES.txt");
      if((parser_log = fopen(parsedMsgFileName.c_str(),"w")) == NULL) on_error ("Error %s",parsedMsgFileName.c_str());
      EKA_LOG("%s:%u created file %s",EKA_EXCH_DECODE(exch),grId,parsedMsgFileName.c_str());
    }
  }

 public:
  //  virtual void      init() = 0;

/* ####################################################### */

  //----------------------------------------------------------

  EkaDev*          dev      = NULL;
  EkaLSI           grId     = -1; 
  EkaSource        exch     = EkaSource::kInvalid; 
  //----------------------------------------------------------

  uint             numSecurities = 0;

  FILE*            parser_log = NULL; // used with PRINT_PARSED_MESSAGES define
};

#endif
