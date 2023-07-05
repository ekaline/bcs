#ifndef _EKA_QED_STRATEGY_H_
#define _EKA_QED_STRATEGY_H_

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

class EkaQedStrategy : public EkaStrategy {
public:
  EkaQedStrategy(const EfcUdpMcParams *mcParams,
                 const EfcQedParams *p4Params);

  void arm(EfcArmVer ver);
  void disarm();

private:
  void configureEhp();
  void configureTemplates();

public:
  static const int MAX_CTX_THREADS = 16;

  EfhFeedVer feedVer_ = EfhFeedVer::kInvalid;

  int numSecurities_ = 0;
  uint64_t *secIdList_ =
      NULL; // array of SecIDs, index is handle
  int ctxWriteBank[MAX_CTX_THREADS] = {};

  uint32_t maxSize_ = 0;
  bool fireOnAllAddOrders_ = false;

private:
  EfhFeedVer hwFeedVer_ = EfhFeedVer::kInvalid;

  int regionId_ = EkaEpmRegion::Regions::Efc;
};
#endif