#ifndef __EHP_EHP_EUR_H__
#define __EHP_EHP_EUR_H__

#include "EhpProtocol.h"

enum class EhpEurMsg : int { ExecutionSummary = 0 };

class EhpEur : public EhpProtocol {
public:
  EhpEur(EkaStrategy *strat);
  virtual ~EhpEur() {}

  int init();

  int createExecutionSummary();

public:
  static const int ExecutionSummaryMsg = 0;
};

#endif
