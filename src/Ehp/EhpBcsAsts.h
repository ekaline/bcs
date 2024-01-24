#ifndef _EHP_BC_CMEFC_H_
#define _EHP_BC_CMEFC_H_

#include "EhpProtocol.h"

enum class EhpBcCmeFCMsg : int { FastCancel = 0 };

class EhpBcCmeFC : public EhpProtocol {
public:
  EhpBcCmeFC(EkaStrategy *strat);
  virtual ~EhpBcCmeFC() {}

  int init();

  int createFastCancel();

public:
  static const int FastCancelMsg = 0;
};

#endif
