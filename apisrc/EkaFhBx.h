#ifndef _EKA_FH_BX_H_
#define _EKA_FH_BX_H_

#include "EkaFhNasdaq.h"

class EkaFhBx : public EkaFhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
public:
  EkaFhGroup* addGroup();
  EkaOpResult runGroups( EfhCtx* pEfhCtx,
			 const EfhRunCtx* pEfhRunCtx,
			 uint8_t runGrId);
  
  virtual ~EkaFhBx() {};
private:
  
};

#endif
