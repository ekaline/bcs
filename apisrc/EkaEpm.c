#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "EkaDev.h"
#include "EpmCore.h"

EkaOpResult EkaEpm::epmSetAction(EkaCoreId coreId,
				 epm_strategyid_t strategy,
				 epm_actionid_t action,
				 const EpmAction *epmAction) {
  return core[coreId]->setAction(strategy,action,epmAction);
}

EkaOpResult EkaEpm::epmGetAction(EkaCoreId coreId,
				 epm_strategyid_t strategy,
				 epm_actionid_t action,
				 EpmAction *epmAction) {
  return core[coreId]->getAction(strategy,action,epmAction);
}

EkaOpResult EkaEpm::epmEnableController(EkaCoreId coreId, bool enable) {
  return core[coreId]->enableController(enable);
}

EkaOpResult EkaEpm::epmInitStrategies(EkaCoreId coreId,
			      const EpmStrategyParams *params,
			      epm_strategyid_t numStrategies) {
  if (core[coreId] != NULL) on_error("core[%u] != NULL",coreId);
  core[coreId] = new EpmCore(this, coreId);
  if (core[coreId] == NULL) on_error("core[%u] == NULL",coreId);
  active = true;
  return core[coreId]->initStrategies(params,numStrategies);
}

EkaOpResult EkaEpm::epmSetStrategyEnableBits(EkaCoreId coreId,
                                     epm_strategyid_t strategy,
                                     epm_enablebits_t enable) {
  return core[coreId]->setStrategyEnableBits(strategy,enable);
}

EkaOpResult EkaEpm::epmGetStrategyEnableBits(EkaCoreId coreId,
                                     epm_strategyid_t strategy,
                                     epm_enablebits_t *enable) {
  return core[coreId]->getStrategyEnableBits(strategy,enable);
}

  
EkaOpResult EkaEpm::epmRaiseTriggers(EkaCoreId coreId,
				     const EpmTrigger *trigger) {
  return core[coreId]->raiseTriggers(trigger);
}

EkaOpResult EkaEpm::epmPayloadHeapCopy(EkaCoreId coreId,
				       epm_strategyid_t strategy, 
				       uint32_t offset,
				       uint32_t length, 
				       const void *contents) {
  return core[coreId]->payloadHeapCopy(strategy,offset,length,contents); 
}
