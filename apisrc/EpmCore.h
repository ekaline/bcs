#ifndef _EPM_CORE_H_
#define _EPM_CORE_H_

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "eka_dev.h"

class EpmCore {
 public:
  EkaOpResult setAction(epm_actionid_t idx, const EpmAction *epmAction) {
    if (idx >= MaxActions) return ERR_INVALID_ACTION;
      
      
  }
 private:
  uint32_t          stratNum = 0;

  EpmStrategyParams strat[MaxStrategies] = {};
  EpmAction         action[MaxActions];
  char              heap[PayloadMemorySize];
};


#endif
