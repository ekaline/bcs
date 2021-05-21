#ifndef _EKA_FH_GEM_H_
#define _EKA_FH_GEM_H_

#include "EkaFhNasdaq.h"

class EkaFhGemGr;

class EkaFhGem : public EkaFhNasdaq{ // Gem & Ise
  static const uint QSIZE = 1 * 1024 * 1024;
 public:

  EkaFhGroup* addGroup();

  virtual ~EkaFhGem() {};
 private:
};

#endif
