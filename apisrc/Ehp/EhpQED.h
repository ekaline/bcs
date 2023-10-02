#ifndef _EHP_QED_H_
#define _EHP_QED_H_

#include "EhpProtocol.h"

enum class EhpQEDMsg : int { QedTrigger = 0 };

class EhpQED : public EhpProtocol {
public:
  EhpQED(EkaStrategy *strat);
  virtual ~EhpQED() {}

  int init();

  int createQedTrigger();

public:
  static const int QedTriggerMsg = 0;
};

#endif
