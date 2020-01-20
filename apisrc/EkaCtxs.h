#ifndef __EKA_CTXS_H__
#define __EKA_CTXS_H__

#include "Eka.h"

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
  EfhFeedVer      feed_ver; // not needed
  EkaCoreId       coreId;
  uint8_t         fhId; // free running number for multi-FH solution

  bool            dontPrintTobUpd;
  bool            dontPrintDefinition;
  bool            printQStatistics;
  bool            injectTestGaps; // used for Gap recovery testing
  uint64_t        injectTestGapsPeriod; // spcifying how frequency of the the Gap injections. Measured in raw sequences.
};


#endif
