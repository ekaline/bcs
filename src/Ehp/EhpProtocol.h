#ifndef _EHP_PROTOCOL_H_
#define _EHP_PROTOCOL_H_

#include "EhpConf.h"
#include "eka_macros.h"

class EkaDev;
class EkaStrategy;

class EhpProtocol {
protected:
  EhpProtocol(EkaStrategy *strat);

public:
  virtual int init() = 0;
  int download2Hw(int coreId);

  /* ------------------------------------- */
public:
  volatile EhpProtocolConf conf = {};

protected:
  EkaDev *dev = NULL;
  EkaStrategy *strat_ = nullptr;
};
#endif
