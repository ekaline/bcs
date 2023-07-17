#ifndef _EHP_PITCH_H_
#define _EHP_PITCH_H_

#include "EhpProtocol.h"

class EhpPitch : public EhpProtocol {
public:
  EhpPitch(EkaStrategy *strat);
  virtual ~EhpPitch() {}

  int init();

  int createAddOrderShort();
  int createAddOrderLong();
  int createAddOrderExpanded();

public:
  static const int AddOrderExpandedMsg = 0;
  static const int AddOrderShortMsg = 1;
  static const int AddOrderLongMsg = 2;
  static const int NumMsgs = 3;

  const char *msgName[NumMsgs] = {
      "AddOrderExpanded", "AddOrderShort", "AddOrderLong"};

  bool fireOnAllAddOrders_ = false;
  EfhFeedVer hwFeedVer_ = EfhFeedVer::kInvalid;
};

#endif
