#ifndef _EHP_PROTOCOL_H_
#define _EHP_PROTOCOL_H_

#include "EhpConf.h"
#include "eka_macros.h"

class EkaDev;

class EhpProtocol {
protected:
  EhpProtocol(EkaDev *dev);

public:
  virtual int init() = 0;
  int download2Hw(int coreId);

  /* ------------------------------------- */
public:
  volatile EhpProtocolConf conf = {};
  static const uint64_t EhpConfAddr = 0x10000;

  // private:
  EkaDev *dev = NULL;
};
#endif
