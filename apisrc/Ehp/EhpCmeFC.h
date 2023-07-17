#ifndef _EHP_CMEFC_H_
#define _EHP_CMEFC_H_

#include "EhpProtocol.h"

enum class EhpCmeFCMsg : int { FastCancel = 0 };

class EhpCmeFC : public EhpProtocol {
public:
  EhpCmeFC(EkaStrategy *strat);
  virtual ~EhpCmeFC() {}

  int init();

  int createFastCancel();

public:
  static const int FastCancelMsg = 0;
};

#endif
