#ifndef _EKA_FH_PHLX_TOPO_H_
#define _EKA_FH_PHLX_TOPO_H_

#include "EkaNasdaq.h"

class FhPhlxTopo : public FhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
public:
  virtual ~FhPhlxTopo() {};
private:

};

#endif
