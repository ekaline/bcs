#ifndef _EKA_BC_CME_STRATEGY_H_
#define _EKA_BC_CME_STRATEGY_H_

#include "EkaStrategyEhp.h"

class EkaBcCmeStrategy : public EkaStrategyEhp<EhpCmeFC> {
public:
  EkaBcCmeStrategy(const EfcUdpMcParams *mcParams,
                   const EkaBcCmeFcAlgoParams *cmeParams);
  void setFireAction(epm_actionid_t fireActionId);

private:
  // void configureEhp();
  void configureTemplates();

public:
private:
  const int ConfHwAddr = 0x84000;
  volatile EfcBCCmeFastCancelStrategyConf conf_ = {};

  int regionId_ = EkaEpmRegion::Regions::Efc;
};
#endif
