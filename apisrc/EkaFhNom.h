#ifndef _EKA_FH_NOM_H_
#define _EKA_FH_NOM_H_

#include "EkaFhNasdaq.h"

class EkaFhNomGr;

class EkaFhNom : public EkaFhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
 public:
  EkaFhGroup* addGroup();

  virtual ~EkaFhNom() {};
 private:

};

#endif
