#ifndef _EHP_ITCHFS_H_
#define _EHP_ITCHFS_H_

#include "EhpProtocol.h"

enum class EhpItchfsMsg : int {
  OrderExecuted = 0,
  TradeNonCross = 1
};

class EhpItchfs : public EhpProtocol {
 public:
  EhpItchfs(EkaDev* dev);
  virtual ~EhpItchfs() {}

  int init();

  int createOrderExecuted();
  int createTradeNonCross();

 public:
  static const int OrderExecutedMsg = 0;
  static const int TradeNonCrossMsg = 1;

};

#endif
