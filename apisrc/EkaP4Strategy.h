#ifndef _EKA_P4_STRATEGY_H_
#define _EKA_P4_STRATEGY_H_

#include "EkaEpmRegion.h"
#include "EkaStrategy.h"
#include "eka_macros.h"

class EkaHwHashTableLine;

class EkaP4Strategy : public EkaStrategy {
public:
  EkaP4Strategy(EfhFeedVer feedVer,
                const EpmStrategyParams *params);

  void cleanSubscrHwTable();
  int subscribeSec(uint64_t secId);
  EfcSecCtxHandle getSubscriptionId(uint64_t secId);
  int downloadTable();

  void writeSecHwCtx(const EfcSecCtxHandle handle,
                     const EkaHwSecCtx *pHwSecCtx,
                     uint16_t writeChan);

public:
  static const int MAX_CTX_THREADS = 16;

  int numSecurities_ = 0;
  uint64_t *secIdList_ =
      NULL; // array of SecIDs, index is handle
  int ctxWriteBank[MAX_CTX_THREADS] = {};

private:
  void preallocateFireActions();
  void configureTemplates();

  bool isValidSecId(uint64_t secId);
  int initHwRoundTable();
  int cleanSecHwCtx();
  int normalizeId(uint64_t secId);
  int getLineIdx(uint64_t normSecId);

  EfhFeedVer feedVer_ = EfhFeedVer::kInvalid;

  EkaHwHashTableLine *hashLine[EFC_SUBSCR_TABLE_ROWS] = {};
};
#endif