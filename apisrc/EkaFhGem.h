#ifndef _EKA_FH_GEM_H_
#define _EKA_FH_GEM_H_

#include "EkaNasdaq.h"

class FhGem : public FhNasdaq{ // Gem & Ise
  static const uint QSIZE = 1 * 1024 * 1024;
 public:
  virtual ~FhGem() {};
 private:
};

#endif
