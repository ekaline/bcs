#ifndef _EPM_CORE_H_
#define _EPM_CORE_H_

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "eka_dev.h"
#include "EkaEpm.h"

class fh_udp_channel;

class EpmCore {
 public:
  EpmCore(EkaDev* dev, EkaCoreId _coreId);
  EkaOpResult setAction(epm_strategyid_t strategy,
			epm_actionid_t actionIdx, 
			const EpmAction *epmAction);
  
  EkaOpResult getAction(epm_strategyid_t strategy,
			epm_actionid_t actionIdx, 
			EpmAction *epmAction);
  
  EkaOpResult enableController(bool enable);

  EkaOpResult initStrategies(const EpmStrategyParams *params,
			     epm_strategyid_t numStrategies);

  EkaOpResult setStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t enable);

  EkaOpResult getStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t enable);

  EkaOpResult raiseTriggers(const EpmTrigger *trigger);

  EkaOpResult payloadHeapCopy(epm_strategyid_t strategy, 
			      uint32_t offset,
			      uint32_t length, 
			      const void *contents);

  struct Strategy {
    EpmStrategyParams params;
    epm_enablebits_t  enable;
  };

  // private:
  EkaDev* dev;

  epm_actionid_t    stratNum = 0;
  
  Strategy          strat[EkaEpm::MaxStrategies] = {};
  EpmAction         action[EkaEpm::MaxActions] = {};
  char              heap[EkaEpm::PayloadMemorySize] = {};
  
  bool              controllerEnabled = false;
  bool              initialized = false;

  fh_udp_channel*   epmCh       = NULL;
  EkaCoreId         coreId;

  std::thread       swProcessor;

 private:
};


#endif
