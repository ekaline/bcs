#ifndef _EKA_STRATEGY_EHP_H_
#define _EKA_STRATEGY_EHP_H_

#include "EkaStrategy.h"

template <class Ehp>
class EkaStrategyEhp : public EkaStrategy {
protected:
  EkaStrategyEhp(const EfcUdpMcParams *mcParams)
      : EkaStrategy(mcParams) {}
  //  void downloadEhp2Hw();

  void configureEhp() {
    ehp_ = new Ehp(dev_ /* , fireOnAllAddOrders_ */);

    if (!ehp_)
      on_error("!ehp_");

    ehp_->init();

    for (auto coreId = 0; coreId < EFC_MAX_CORES;
         coreId++) {
      if (coreIdBitmap_ & (1 << coreId)) {
        EKA_LOG("Downloading Ehp for %s on coreId %d",
                name_.c_str(), coreId);
        ehp_->download2Hw(coreId);
      }
    }
  }

protected:
  EhpProtocol *ehp_ = nullptr;
};

#endif