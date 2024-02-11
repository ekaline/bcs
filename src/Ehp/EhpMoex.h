#ifndef _EHP_BCS_MOEX_H_
#define _EHP_BCS_MOEX_H_

#include "EhpProtocol.h"

enum class EhpBcsMoexMsg : int { FastCancel = 0 };

class EhpBcsMoex : public EhpProtocol {
public:
  EhpBcsMoex(EkaStrategy *strat);
  virtual ~EhpBcsMoex() {}

  int init();

  int createBestPrice();

public:
  static const int BestPriceMsg = 0;
};

#endif
