#ifndef _EKA_STRATEGY_H_
#define _EKA_STRATEGY_H_

#include "EkaEpmRegion.h"
#include "eka_macros.h"

class EkaUdpSess;

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

public:
  static void clearAllHwUdpParams();

protected:
  EkaDev *dev_ = nullptr;
  EfhFeedVer hwFeedVer_ = EfhFeedVer::kInvalid;

  EkaUdpSess *udpSess_[MaxUdpMcGroups] = {};
  int numUdpSess_ = 0;

  OnReportCb reportCb_;
  void *cbCtx_;
};

#endif