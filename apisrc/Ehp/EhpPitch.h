#ifndef _EHP_PITCH_H_
#define _EHP_PITCH_H_

#include "EhpProtocol.h"

enum class EhpPitchMsg : int {
  AddOrderShort = 0,
    AddOrderLong = 1
};

class EhpPitch : public EhpProtocol {
 public:
  EhpPitch(EkaDev* dev);
  virtual ~EhpPitch() {}

  int init();

  int createAddOrderShort();
  int createAddOrderLong();
  int createAddOrderExpanded();

 public:
  static const int AddOrderExpandedMsg  = 0;
  static const int AddOrderShortMsg = 1;
  static const int AddOrderLongMsg  = 2;

};

#endif
