#ifndef _EKA_EFC_H_
#define _EKA_EFC_H_

#include "Efh.h"
#include "Efc.h"
#include "eka_macros.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"

class EkaHwHashTableLine;
class EkaIgmp;

class EkaEfc {
 public:
  EkaEfc(EkaDev* dev, EfhFeedVer hwFeedVer, const EfcInitCtx* pEfcInitCtx);

  int downloadTable();
  int subscribeSec(uint64_t secId);
  int cleanSubscrHwTable();
  int getSubscriptionId(uint64_t secId);

 private:
  bool isValidSecId(uint64_t secId);
  int  setMcParams(const EfcInitCtx* pEfcInitCtx);
  int  confParse(const char *key, const char *value);
  int  initHwRoundTable();
  int  normalizeId(uint64_t secId);
  int  getLineIdx(uint64_t normSecId);

  EkaIgmp*   ekaIgmp    = NULL;
  uint8_t    mdCoreId = -1;
  EkaDev*    dev = NULL;
  EfhFeedVer hwFeedVer;
  EkaHwHashTableLine* hashLine[EKA_SUBSCR_TABLE_ROWS] = {};
  int        numSecurities = 0;
};

#endif
