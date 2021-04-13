#ifndef __EKA_CTXS_H__
#define __EKA_CTXS_H__

class EkaDev;

struct EkaCtx {
  EkaDev* dev;
};

struct ExcCtx {
  EkaDev* dev;
};

struct EfcCtx {
  EkaDev* dev;
  //  int     feedVer;  // set from EfcInitCtx
};

struct EfhCtx {
  EkaDev* dev;
  uint8_t         fhId   = -1; // free running number for multi-FH solution

  bool            dontPrintTobUpd       = false;
  bool            dontPrintDefinition   = false;
  bool            printQStatistics      = false;
  bool            injectTestGaps        = false; // used for Gap recovery testing
  uint64_t        injectTestGapsPeriod  = false; // frequency of the the Gap injections. Measured in raw sequences.
};


#endif
