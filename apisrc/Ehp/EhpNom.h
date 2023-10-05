#ifndef _EHP_NOM_H_
#define _EHP_NOM_H_

#include "EhpProtocol.h"

enum class EhpNomMsg : int {
  AddOrderShort = 0,
  AddOrderLong = 1
};

class EhpNom : public EhpProtocol {
public:
  EhpNom(EkaStrategy *strat);
  virtual ~EhpNom() {}

  int init();

  int createAddOrderShort();
  int createAddOrderLong();

public:
  static const int AddOrderShortMsg = 0;
  static const int AddOrderLongMsg = 1;
};

#endif
