#ifndef _EKA_STRATEGY_H_
#define _EKA_STRATEGY_H_

#include "EkaEpmRegion.h"
#include "eka_macros.h"

class EkaUdpSess;
class EkaEpmAction;

class EkaStrategy {
protected:
  static const size_t MaxUdpMcGroups = 64;

  EkaStrategy(EfhFeedVer feedVer,
              const EpmStrategyParams *params);

public:
  virtual ~EkaStrategy();

  void setHwUdpParams();
  void enableRxFire();
  void disableRxFire();
  void disArmController();
  void armController(EfcArmVer ver);

  int allocateAction(EpmActionType type);

  EkaUdpSess *findUdpSess(EkaCoreId coreId, uint32_t mcAddr,
                          uint16_t mcPort);

public:
  static void clearAllHwUdpParams();

protected:
  EkaDev *dev_ = nullptr;
  EkaEpm *epm_ = nullptr;
  EfhFeedVer hwFeedVer_ = EfhFeedVer::kInvalid;

public:
  EkaUdpSess *udpSess_[MaxUdpMcGroups] = {};
  int numUdpSess_ = 0;

  OnReportCb reportCb_;
  void *cbCtx_;

  std::string name_ = "Uninitialized";
  EkaEpmAction *a_[EkaEpmRegion::NumEfcActions] = {};

  int regionId_ = EkaEpmRegion::Regions::Efc;

  // EhpProtocol *ehp = NULL;
};

#endif