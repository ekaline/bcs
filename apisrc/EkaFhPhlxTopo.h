#ifndef _EKA_FH_PHLX_TOPO_H_
#define _EKA_FH_PHLX_TOPO_H_

#include "EkaFhNasdaq.h"

class EkaFhPhlxTopo : public EkaFhNasdaq {
  static const uint QSIZE = 2 * 1024 * 1024;
public:
  virtual ~EkaFhPhlxTopo() {};


  EkaOpResult         runGroups(EfhCtx*          pEfhCtx, 
				const EfhRunCtx* pEfhRunCtx, 
				uint8_t          runGrId);
  
  EkaOpResult         getDefinitions (EfhCtx*          pEfhCtx, 
				      const EfhRunCtx* pEfhRunCtx, 
				      EkaGroup*        group);
  
 private:

};

#endif
