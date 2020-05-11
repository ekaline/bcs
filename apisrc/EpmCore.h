#ifndef _EPM_CORE_H_
#define _EPM_CORE_H_

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "eka_dev.h"
#include "EkaEpm.h"

class EpmCore {
 public:
  EkaOpResult setAction(epm_strategyid_t strategy,
			epm_actionid_t actionIdx, const EpmAction *epmAction) {
    if (actionIdx >= static_cast<epm_actionid_t>(EkaEpm::MaxActions)) return EKA_OPRESULT__ERR_INVALID_ACTION;
    if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
    if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

    memcpy (&action[actionIdx],epmAction,sizeof(EpmAction));
    return EKA_OPRESULT__OK;
  }

  EkaOpResult getAction(epm_strategyid_t strategy,
			epm_actionid_t actionIdx, EpmAction *epmAction) {
    if (actionIdx >= static_cast<epm_actionid_t>(EkaEpm::MaxActions)) return EKA_OPRESULT__ERR_INVALID_ACTION;
    if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
    if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

    memcpy (epmAction,&action[actionIdx],sizeof(EpmAction));
    return EKA_OPRESULT__OK;
  }

  EkaOpResult enableController(bool enable) {
    if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
    controllerEnabled = enable;
    return EKA_OPRESULT__OK;
  }

  EkaOpResult initStrategies(const EpmStrategyParams *params,
				epm_strategyid_t numStrategies) {
    for (auto i = 0; i < numStrategies; i++) {
      memcpy(&strat[i],&params[i],sizeof(EpmStrategyParams));
    }
    initialized = true;
    stratNum = numStrategies;
    return EKA_OPRESULT__OK;
  }

  EkaOpResult setStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t enable) {
    if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
    if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

    memcpy(&strat[strategy].enable,&enable,sizeof(epm_enablebits_t));

    return EKA_OPRESULT__OK;
  }

  EkaOpResult getStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t enable) {
    if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
    if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

    memcpy(&enable,&strat[strategy].enable,sizeof(epm_enablebits_t));

    return EKA_OPRESULT__OK;
  }

  EkaOpResult raiseTriggers(const EpmTrigger *trigger) {

    return EKA_OPRESULT__OK;
  }

  EkaOpResult payloadHeapCopy(epm_strategyid_t strategy, 
			      uint32_t offset,
			      uint32_t length, 
			      const void *contents) {
    if (offset % EkaEpm::PayloadAlignment != 0)
      return EKA_OPRESULT__ERR_INVALID_ALIGN;
       
    memcpy(heap + offset,contents,length);
    return EKA_OPRESULT__OK;

  }

  private:
  struct Strategy {
    EpmStrategyParams params;
    epm_enablebits_t  enable;
  };

  epm_actionid_t    stratNum = 0;
  
  Strategy          strat[EkaEpm::MaxStrategies] = {};
  EpmAction         action[EkaEpm::MaxActions] = {};
  char              heap[EkaEpm::PayloadMemorySize] = {};
  
  bool              controllerEnabled = false;
  bool              initialized = false;
};


#endif
