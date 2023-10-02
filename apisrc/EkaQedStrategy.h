#ifndef _EKA_QED_STRATEGY_H_
#define _EKA_QED_STRATEGY_H_

#include "Efc.h"
#include "Efh.h"
#include "EkaEpm.h"

#include "EhpQED.h"
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
#include "EkaStrategyEhp.h"
#include "eka_macros.h"

class EkaQedStrategy : public EkaStrategyEhp<EhpQED> {
public:
  EkaQedStrategy(const EfcUdpMcParams *mcParams,
                 const EfcQedParams *qedParams);
  void setFireAction(epm_actionid_t fireActionId,
                     int productId);

private:
  // void configureEhp();
  void configureTemplates();

public:
  /*   static const int MAX_CTX_THREADS = 16;

    EfhFeedVer feedVer_ = EfhFeedVer::kInvalid;

    int numSecurities_ = 0;
    uint64_t *secIdList_ =
        NULL; // array of SecIDs, index is handle
    int ctxWriteBank[MAX_CTX_THREADS] = {};

    uint32_t maxSize_ = 0;
    bool fireOnAllAddOrders_ = false; */

private:
  const int ConfHwAddr = 0x86000;

  volatile EfcQEDStrategyConf conf_ = {};
  int prodCnt_ = 0;

  int regionId_ = EkaEpmRegion::Regions::Efc;
};
#endif