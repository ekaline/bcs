#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <iterator>
#include <thread>

#include <sys/time.h>
#include <chrono>

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "epmCaps.h"
#include "EkaEpm.h"

uint64_t epmGetDeviceCapability(const EkaDev *dev, EpmDeviceCapability cap) {
  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  switch (cap) {
    case EHC_PayloadMemorySize :
      return dev->epm->getPayloadMemorySize();

    case EHC_PayloadAlignment :
      return dev->epm->getPayloadAlignment();

    case EHC_DatagramOffset :
      return dev->epm->getDatagramOffset();

    case EHC_RequiredTailPadding :
      return dev->epm->getRequiredTailPadding();

    case EHC_MaxStrategies :
      return dev->epm->getMaxStrategies();

    case EHC_MaxActions :
      return dev->epm->getMaxActions();

    default:
      on_error("Unsupported EPM Cap: %d",(int)cap);
    }
}

EkaOpResult epmSetAction(EkaDev* dev, EkaCoreId coreId,
                         epm_strategyid_t strategy, epm_actionid_t action,
                         const EpmAction *epmAction) {
  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  return dev->epm->setAction(coreId,strategy,action,epmAction);
}

EkaOpResult epmGetAction(const EkaDev *dev, EkaCoreId coreId,
                         epm_strategyid_t strategy, epm_actionid_t action,
                         EpmAction *epmAction) {
  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  if (strategy >= (int)EkaEpm::MaxStrategies) return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  if (action   >= (int)EkaEpm::MaxActionsPerStrategy) return EKA_OPRESULT__ERR_INVALID_ACTION;

  return dev->epm->getAction(coreId,strategy,action,epmAction);

}


EkaOpResult epmEnableController(EkaDev *dev, EkaCoreId coreId, bool enable) {
  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  return dev->epm->enableController(coreId,enable);
}

EkaOpResult epmInitStrategies(EkaDev *dev, EkaCoreId coreId,
                              const EpmStrategyParams *params,
                              epm_strategyid_t numStrategies) {

  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;
  if (numStrategies > static_cast<epm_strategyid_t>(dev->epm->getMaxStrategies()))
    return EKA_OPRESULT__ERR_MAX_STRATEGIES;

  EKA_LOG("initializing %u EPM strategies for core %u",numStrategies,coreId);
  return dev->epm->initStrategies(coreId,params,numStrategies);
}

EkaOpResult epmSetStrategyEnableBits(EkaDev *dev, EkaCoreId coreId,
                                     epm_strategyid_t strategy,
                                     epm_enablebits_t enable) {

  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  return dev->epm->setStrategyEnableBits(coreId,strategy,enable);
}

EkaOpResult epmGetStrategyEnableBits(EkaDev *dev, EkaCoreId coreId,
                                     epm_strategyid_t strategy,
                                     epm_enablebits_t *enable) {

  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  return dev->epm->getStrategyEnableBits(coreId,strategy,enable);
}

EkaOpResult epmRaiseTriggers(EkaDev *dev, EkaCoreId coreId,
                             const EpmTrigger *trigger) {

  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  if (trigger == NULL) on_error("trigger == NULL");
  if (trigger->strategy >= (int)EkaEpm::MaxStrategies) return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  if (trigger->action   >= (int)EkaEpm::MaxActionsPerStrategy) return EKA_OPRESULT__ERR_INVALID_ACTION;

  return dev->epm->raiseTriggers(trigger);
}

EkaOpResult epmPayloadHeapCopy(EkaDev *dev, EkaCoreId coreId,
                               epm_strategyid_t strategy, uint32_t offset,
                               uint32_t length, const void *contents) {
  if (dev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES || dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  return dev->epm->payloadHeapCopy(coreId,strategy,offset,length,contents);
}


/* ######################################### */
