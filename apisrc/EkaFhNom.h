#ifndef _EKA_FH_NOM_H_
#define _EKA_FH_NOM_H_

#include "EkaNasdaq.h"

class FhNom : public FhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
 public:
  virtual ~FhNom() {};
 private:

};

#endif
