#ifndef _EKA_FH_BX_H_
#define _EKA_FH_BX_H_

#include "EkaFhNom.h"

class EkaFhBx : public EkaFhNom {
  static const uint QSIZE = 2 * 1024 * 1024;
 public:
  EkaFhGroup* addGroup();

  virtual ~EkaFhBx() {};
 private:

};

#endif
