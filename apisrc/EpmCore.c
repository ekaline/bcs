#include "EkaUdpChannel.h"
#include "EpmCore.h"
#include "EpmStrategy.h"

#include "EkaCtxs.h"
#include "EkaCore.h"

EpmCore::EpmCore(EkaEpm* _parent, EkaCoreId _coreId) {
  parent = _parent;
  dev = parent->dev;
  coreId = _coreId;
  vlanTag = dev->core[coreId]->vlanTag;
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::setAction(epm_strategyid_t strategyIdx,
			       epm_actionid_t actionIdx, 
			       const EpmAction *epmAction) {
  if (! initialized) {
    EKA_WARN ("EKA_OPRESULT__ERR_EPM_UNINITALIZED");
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  }
  if (! validStrategyIdx(strategyIdx)) {
    EKA_WARN ("EKA_OPRESULT__ERR_INVALID_STRATEGY");
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  }
  if (! strategy[strategyIdx]->myAction(actionIdx)){
    EKA_WARN ("EKA_OPRESULT__ERR_INVALID_ACTION");
    return EKA_OPRESULT__ERR_INVALID_ACTION;
  }

  return strategy[strategyIdx]->setAction(actionIdx,epmAction);
}
/* ----------------------------------------------------- */
EkaOpResult EpmCore::getAction(epm_strategyid_t strategyIdx,
			       epm_actionid_t actionIdx, 
			       EpmAction *epmAction) {

  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  if (! strategy[strategyIdx]->myAction(actionIdx)) return EKA_OPRESULT__ERR_INVALID_ACTION;

  return strategy[strategyIdx]->getAction(actionIdx,epmAction);
}

/* ----------------------------------------------------- */

EkaOpResult EpmCore::enableController(bool enable) {
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  controllerEnabled = enable;
  return EKA_OPRESULT__OK;
}

/* ----------------------------------------------------- */
void EpmCore::swEpmProcessor() {
  while (parent->active) {
    if (! epmCh->has_data()) continue;

    const uint8_t* pkt = epmCh->get();
    if (pkt == NULL) on_error("pkt == NULL");
    const EpmTrigger* trigger = reinterpret_cast<const EpmTrigger*>(pkt);
    EKA_LOG("Received Trigger: token=%u, strategy=%u, action=%u",
  	    trigger->token,trigger->strategy,trigger->action);

    epm_strategyid_t strategyIdx = trigger->strategy;
    epm_actionid_t actionIdx = trigger->action;

    if (! validStrategyIdx(strategyIdx))
      on_error("INVALID Strategy %u",strategyIdx);
    if (! strategy[strategyIdx]->myAction(actionIdx))
      on_error("INVALID Action %u",actionIdx);

    while (actionIdx != EPM_LAST_ACTION) {
      EpmAction epmAction = {};
      if (strategy[strategyIdx]->getAction(actionIdx,&epmAction) != EKA_OPRESULT__OK)
	on_error("getAction failed for strategyIdx=%u, actionIdx=%u",strategyIdx,actionIdx);
      if (epmAction.offset + epmAction.length >= EkaEpm::PayloadMemorySize)
	on_error("epmAction.offset %u + epmAction.length %u >= EkaEpm::PayloadMemorySize %ju",
		 epmAction.offset,epmAction.length, EkaEpm::PayloadMemorySize);

      int bytes2send = epmAction.length;
      char* sndPtr = &heap[epmAction.offset];
      do {
	int bytesSent = excSend (dev, epmAction.hConn, sndPtr, bytes2send);
	bytes2send -= bytesSent;
	sndPtr     += bytesSent;
      } while (bytes2send > 0);
      //      EKA_LOG("strategy=%u, ActionId=%u, nextAction=%u",strategyIdx,epmAction.user,epmAction.nextAction);
      actionIdx = epmAction.nextAction;
    }
    
    epmCh->next();
  }
}

/* ----------------------------------------------------- */
int EpmCore::openUdpChannel() {
  epmCh              = new EkaUdpChannel(dev,coreId);
  if (epmCh == NULL) on_error ("epmCh == NULL");
  return 0;
}
/* ----------------------------------------------------- */
bool EpmCore::alreadyJoined(epm_strategyid_t prevStrats,uint32_t ip, uint16_t port) {
  for (auto i = 0; i < prevStrats; i++) {
    if (strategy[i]->hasSame(ip,port)) return true;
  }
  return false;
}

/* ----------------------------------------------------- */

int EpmCore::joinMc(uint32_t ip, uint16_t port, int16_t vlanTag) {
  epmCh->igmp_mc_join(dev->core[coreId]->srcIp,ip,be16toh(port), vlanTag);
  return 0;
}

/* ----------------------------------------------------- */

EkaOpResult EpmCore::initStrategies(const EpmStrategyParams *params,
				    epm_strategyid_t numStrategies) {
  EKA_LOG("Initializing Core %u, numStrategies =  %u",coreId,numStrategies);

  initialized = true;
  stratNum = numStrategies;
  openUdpChannel();

  epm_actionid_t baseActionIdx = 0;
  for (auto i = 0; i < stratNum; i++) {
    strategy[i] = new EpmStrategy(this,i,baseActionIdx, &params[i]);
    baseActionIdx += params[i].numActions;
  }

  swProcessor = std::thread(&EpmCore::swEpmProcessor,this);

  return EKA_OPRESULT__OK;
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::setStrategyEnableBits(epm_strategyid_t strategyIdx,
					   epm_enablebits_t enable) {
  if (! initialized) 
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;

  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->setEnableBits(enable);
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::getStrategyEnableBits(epm_strategyid_t strategyIdx,
					   epm_enablebits_t *enable) {
  if (! initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->getEnableBits(enable);
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::raiseTriggers(const EpmTrigger *trigger) {

  return EKA_OPRESULT__OK;
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::payloadHeapCopy(epm_strategyid_t strategy, 
				     uint32_t offset,
				     uint32_t length, 
				     const void *contents) {
  if (offset % EkaEpm::PayloadAlignment != 0)
    return EKA_OPRESULT__ERR_INVALID_ALIGN;
       
  if (offset + length > EkaEpm::PayloadMemorySize) 
    return EKA_OPRESULT__ERR_INVALID_OFFSET;
  memcpy(heap + offset,contents,length);
  return EKA_OPRESULT__OK;

}
