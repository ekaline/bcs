#include "eka_fh_udp_channel.h"
#include "EpmCore.h"

#include "EkaCtxs.h"

EpmCore::EpmCore(EkaDev* _dev, EkaCoreId _coreId) {
  dev = _dev;
  coreId = _coreId;
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::setAction(epm_strategyid_t strategy,
			       epm_actionid_t actionIdx, 
			       const EpmAction *epmAction) {
  if (actionIdx >= static_cast<epm_actionid_t>(EkaEpm::MaxActions)) return EKA_OPRESULT__ERR_INVALID_ACTION;
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  
  memcpy (&action[actionIdx],epmAction,sizeof(EpmAction));
  return EKA_OPRESULT__OK;
}
/* ----------------------------------------------------- */
EkaOpResult EpmCore::getAction(epm_strategyid_t strategy,
			       epm_actionid_t actionIdx, 
			       EpmAction *epmAction) {
  if (actionIdx >= static_cast<epm_actionid_t>(EkaEpm::MaxActions)) return EKA_OPRESULT__ERR_INVALID_ACTION;
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  memcpy (epmAction,&action[actionIdx],sizeof(EpmAction));
  return EKA_OPRESULT__OK;
}

/* ----------------------------------------------------- */

EkaOpResult EpmCore::enableController(bool enable) {
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  controllerEnabled = enable;
  return EKA_OPRESULT__OK;
}

/* ----------------------------------------------------- */
void swEpmProcessor(EpmCore* epmCore) {
  EkaDev* dev = epmCore->dev;
  EKA_LOG("Launched");
  dev->epm->active = true;
  while (dev->epm->active) {
    if (! epmCore->epmCh->has_data()) continue;

    const uint8_t* pkt = epmCore->epmCh->get();
    if (pkt == NULL) on_error("pkt == NULL");
    const EpmTrigger* trigger = reinterpret_cast<const EpmTrigger*>(pkt);
    EKA_LOG("Received Trigger: token=%u, strategy=%u, action=%u",
	    trigger->token,trigger->strategy,trigger->action);

    
    epmCore->epmCh->next();
  }
}

/* ----------------------------------------------------- */

EkaOpResult EpmCore::initStrategies(const EpmStrategyParams *params,
				    epm_strategyid_t numStrategies) {
  for (auto i = 0; i < numStrategies; i++) {
    memcpy(&strat[i],&params[i],sizeof(EpmStrategyParams));
  }
  initialized = true;
  stratNum = numStrategies;

  EfhCtx dummyEfhCtx = {};
  dummyEfhCtx.dev = dev;
  dummyEfhCtx.coreId = coreId;
  epmCh = new fh_udp_channel(&dummyEfhCtx);
  if (epmCh == NULL) on_error ("epmCh == NULL");

  for (auto i = 0; i < stratNum; i++) {
    const sockaddr_in *triggerAddr = reinterpret_cast<const sockaddr_in*>(params[i].triggerAddr);
    if (triggerAddr == NULL) on_error("triggerAddr == NULL for strategy %d",i);
    uint32_t mcIp = triggerAddr->sin_addr.s_addr;
    uint16_t port = be16toh(triggerAddr->sin_port);
    bool alreadyJoined = false;
    for (auto j = 0; j < i; j++) {
      if (reinterpret_cast<const sockaddr_in*>(params[i].triggerAddr)->sin_addr.s_addr == mcIp && 
	  be16toh(reinterpret_cast<const sockaddr_in*>(params[i].triggerAddr)->sin_port) == port) {
	alreadyJoined = true;
      }
    }
    EKA_LOG("strategy %d: %s:%u -- %s",i,EKA_IP2STR(mcIp),port, alreadyJoined ? "skipping IGMP join" : "joining MC");
    if (! alreadyJoined) epmCh->igmp_mc_join(0,mcIp,be16toh(port));
    //    if (! alreadyJoined) epmCh->igmp_mc_join(0,mcIp,0);
  }

  swProcessor = std::thread(swEpmProcessor,this);

  return EKA_OPRESULT__OK;
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::setStrategyEnableBits(epm_strategyid_t strategy,
					   epm_enablebits_t enable) {
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  memcpy(&strat[strategy].enable,&enable,sizeof(epm_enablebits_t));

  return EKA_OPRESULT__OK;
}
/* ----------------------------------------------------- */

EkaOpResult EpmCore::getStrategyEnableBits(epm_strategyid_t strategy,
					   epm_enablebits_t enable) {
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (strategy >= stratNum) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  memcpy(&enable,&strat[strategy].enable,sizeof(epm_enablebits_t));

  return EKA_OPRESULT__OK;
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
       
  memcpy(heap + offset,contents,length);
  return EKA_OPRESULT__OK;

}
