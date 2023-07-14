#include <arpa/inet.h>
#include <iterator>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include <chrono>
#include <sys/time.h>

#include "eka_macros.h"

#include "Efc.h"
#include "Eka.h"
#include "Epm.h"
#include "Exc.h"

#include "EkaEpm.h"
#include "epmCaps.h"

#include "EkaEpmRegion.h"

uint64_t epmGetDeviceCapability(const EkaDev *dev,
                                EpmDeviceCapability cap) {
  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;
  switch (cap) {
  case EHC_PayloadMemorySize:
    // return dev->epm->getPayloadMemorySize();
    return EkaEpmRegion::getWritableHeapSize();

  case EHC_PayloadAlignment:
    return dev->epm->getPayloadAlignment();

  case EHC_DatagramOffset:
    return dev->epm->getDatagramOffset();

  case EHC_UdpDatagramOffset:
    return dev->epm->getUdpDatagramOffset();

  case EHC_RequiredTailPadding:
    return dev->epm->getRequiredTailPadding();

  case EHC_MaxStrategies:
    return dev->epm->getMaxStrategies();

  case EHC_MaxActions:
    return EkaEpmRegion::getTotalActions();

  default:
    on_error("Unsupported EPM Cap: %d", (int)cap);
  }
}

EkaOpResult epmSetAction(EkaDev *dev,
                         epm_strategyid_t strategy,
                         epm_actionid_t action,
                         const EpmAction *epmAction) {
  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;

  return dev->epm->setAction(strategy, action, epmAction);
}

EkaOpResult epmGetAction(const EkaDev *dev,
                         epm_strategyid_t strategy,
                         epm_actionid_t action,
                         EpmAction *epmAction) {
  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;

  if (strategy >= EkaEpmRegion::MaxStrategies)
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  if (action >= EkaEpmRegion::getMaxActionsPerRegion())
    return EKA_OPRESULT__ERR_INVALID_ACTION;

  return dev->epm->getAction(strategy, action, epmAction);
}

EkaOpResult epmEnableController(EkaDev *dev,
                                EkaCoreId coreId,
                                bool enable) {
  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;
  if (coreId >= EkaDev::MAX_CORES ||
      dev->core[coreId] == NULL)
    return EKA_OPRESULT__ERR_INVALID_CORE;

  return dev->epm->enableController(coreId, enable);
}

EkaOpResult
epmInitStrategies(EkaDev *dev,
                  const EpmStrategyParams *params,
                  epm_strategyid_t numStrategies) {
  if (!dev->checkAndSetEpmTx())
    on_error(
        "TX functionality is not available for this "
        "Ekaline SW instance - caught by another process");

  if (numStrategies > static_cast<epm_strategyid_t>(
                          dev->epm->getMaxStrategies()))
    return EKA_OPRESULT__ERR_MAX_STRATEGIES;

  EKA_LOG("initializing %u EPM strategies", numStrategies);
  return dev->epm->initStrategies(params, numStrategies);
}

EkaOpResult
epmSetStrategyEnableBits(EkaDev *dev,
                         epm_strategyid_t strategy,
                         epm_enablebits_t enable) {

  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;

  return dev->epm->setStrategyEnableBits(strategy, enable);
}

EkaOpResult
epmGetStrategyEnableBits(const EkaDev *dev,
                         epm_strategyid_t strategy,
                         epm_enablebits_t *enable) {

  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;

  return dev->epm->getStrategyEnableBits(strategy, enable);
}

EkaOpResult epmRaiseTriggers(EkaDev *dev,
                             const EpmTrigger *trigger) {

  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;

  if (trigger == NULL)
    on_error("trigger == NULL");
  if (trigger->strategy >= EkaEpmRegion::MaxStrategies) {
    EKA_WARN("EKA_OPRESULT__ERR_INVALID_STRATEGY");
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  }
  if (trigger->action >=
      EkaEpmRegion::getMaxActionsPerRegion()) {
    EKA_WARN("EKA_OPRESULT__ERR_INVALID_ACTION");
    return EKA_OPRESULT__ERR_INVALID_ACTION;
  }

  return dev->epm->raiseTriggers(trigger);
}

EkaOpResult epmPayloadHeapCopy(EkaDev *dev,
                               epm_strategyid_t strategy,
                               uint32_t offset,
                               uint32_t length,
                               const void *contents,
                               const bool isUdpDatagram) {
  if (dev == NULL)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;

  return dev->epm->payloadHeapCopy(strategy, offset, length,
                                   contents, isUdpDatagram);
}

/* ######################################### */
