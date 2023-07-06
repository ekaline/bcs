#ifndef _EKA_CME_FC_STRATEGY_H_
#define _EKA_CME_FC_STRATEGY_H_

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

class EkaCmeFcStrategy : public EkaStrategy {
public:
  EkaCmeFcStrategy(const EfcUdpMcParams *mcParams,
                   const EfcCmeFcParams *cmeParams);
  void setFireAction(epm_actionid_t fireActionId,
                     int productId);

private:
  void configureEhp();
  void configureTemplates();

public:
private:
  const int ConfHwAddr = 0x84000;
  volatile EfcCmeFastCancelStrategyConf conf_ = {};

  int regionId_ = EkaEpmRegion::Regions::Efc;
};
#endif