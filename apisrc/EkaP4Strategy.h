#ifndef _EKA_P4_STRATEGY_H_
#define _EKA_P4_STRATEGY_H_

#include "Efc.h"
#include "Efh.h"
#include "EkaEpm.h"

#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"

#include "EkaEfcDataStructs.h"

#include "EkaEpmRegion.h"
#include "EkaStrategy.h"
#include "eka_macros.h"

class EkaHwHashTableLine;

class EkaP4Strategy : public EkaStrategy {
public:
  EkaP4Strategy(const EfcStrategyParams *efcParams,
                const EfcP4Params *p4Params);

  void cleanSubscrHwTable();
  int subscribeSec(uint64_t secId);
  EfcSecCtxHandle getSubscriptionId(uint64_t secId);
  int downloadTable();

  void writeSecHwCtx(const EfcSecCtxHandle handle,
                     const EkaHwSecCtx *pHwSecCtx,
                     uint16_t writeChan);

private:
  void configureEhp();
  void createSecHash();

public:
  static const int MAX_CTX_THREADS = 16;

  EfhFeedVer feedVer_ = EfhFeedVer::kInvalid;

  int numSecurities_ = 0;
  uint64_t *secIdList_ =
      NULL; // array of SecIDs, index is handle
  int ctxWriteBank[MAX_CTX_THREADS] = {};

  uint32_t maxSize_ = 0;

private:
  void preallocateFireActions();
  void configureTemplates();

  bool isValidSecId(uint64_t secId);
  void initHwRoundTable();
  void cleanSecHwCtx();
  int normalizeId(uint64_t secId);
  int getLineIdx(uint64_t normSecId);

  EkaHwHashTableLine *hashLine[EFC_SUBSCR_TABLE_ROWS] = {};

  int regionId_ = EkaEpmRegion::Regions::Efc;
};
#endif