#ifndef __EHP_EHP_EUR_H__
#define __EHP_EHP_EUR_H__

#include "EhpProtocol.h"

enum class EhpEurMsg : int { FastCancel = 0 };

class EhpEur : public EhpProtocol {
public:
  EhpEur(EkaStrategy *strat);
  virtual ~EhpEur() {}

  int init();

  int createFastCancel();

public:
  static const int FastCancelMsg = 0;
};

#endif
