#ifndef _EHP_BCS_ASTS_H_
#define _EHP_BCS_ASTS_H_

#include "EhpProtocol.h"

enum class EhpBcsAstsMsg : int { FastCancel = 0 };

class EhpBcsAsts : public EhpProtocol {
public:
  EhpBcsAsts(EkaStrategy *strat);
  virtual ~EhpBcsAsts() {}

  int init();

  int createBestPrice();

public:
  static const int BestPriceMsg = 0;
};

#endif
