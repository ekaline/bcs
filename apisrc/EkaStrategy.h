#ifndef _EKA_STRATEGY_H_
#define _EKA_STRATEGY_H_

#include "EkaEpmRegion.h"
#include "eka_macros.h"

class EkaUdpSess;
class EkaEpmAction;

class EkaStrategy {
protected:
  static const size_t MaxUdpMcGroups = 64;

  EkaStrategy(const EfcUdpMcParams *mcParams);

public:
  virtual ~EkaStrategy();

  void setHwUdpParams();
  void enableRxFire();
  void disableRxFire();
  virtual void arm(EfcArmVer ver);
  virtual void disarm();

  EkaUdpSess *findUdpSess(EkaCoreId coreId, uint32_t mcAddr,
                          uint16_t mcPort);

  uint8_t getCoreBitmap();

public:
  static void clearAllHwUdpParams();
  uint8_t coreIdBitmap_ = 0x00;

protected:
  EkaDev *dev_ = nullptr;
  EkaEpm *epm_ = nullptr;

  const int ArmDisarmNonP4Addr = 0xf07d0;

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