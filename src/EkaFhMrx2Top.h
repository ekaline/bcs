#ifndef _EKA_FH_MRX2_TOP_H_
#define _EKA_FH_MRX2_TOP_H_

#include "EkaFhNasdaq.h"

class EkaFhMrx2TopGr;

class EkaFhMrx2Top : public EkaFhNasdaq{ // Mrx2_Top
  static const uint QSIZE = 1 * 1024 * 1024;
 public:

  EkaFhGroup* addGroup();

  virtual ~EkaFhMrx2Top() {};
 private:
};

#endif
