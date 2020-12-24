#ifndef __EKA_CTXS_H__
#define __EKA_CTXS_H__

#include "Eka.h"
#include "Efh.h"

struct EkaCtx {
  EkaDev* dev;
};

struct ExcCtx {
  EkaDev* dev;
};

struct EfcCtx {
  EkaDev* dev;
};

struct EfhCtx {
  EkaDev* dev;
  //  EfhFeedVer      feed_ver; // not needed
  //  EkaCoreId       coreId = -1;
  uint8_t         fhId   = -1; // free running number for multi-FH solution

  bool            dontPrintTobUpd      = false;
  bool            dontPrintDefinition  = false;
  bool            printQStatistics     = false;
  bool            injectTestGaps       = false; // used for Gap recovery testing
  uint64_t        injectTestGapsPeriod = false; // spcifying how frequency of the the Gap injections. Measured in raw sequences.
};


#endif
