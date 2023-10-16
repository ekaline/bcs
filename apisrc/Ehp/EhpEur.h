#ifndef _EHP_CMEFC_H_
#define _EHP_CMEFC_H_

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
