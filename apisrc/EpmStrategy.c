#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "EpmStrategy.h"

/* ------------------------------------------------ */
EpmStrategy::EpmStrategy(EkaEpm* _parent, epm_strategyid_t _id, epm_actionid_t _baseActionIdx, const EpmStrategyParams *params) {
  epm = _parent;
  dev = epm->dev;
  id = _id;
  baseActionIdx = _baseActionIdx;

  const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(params->triggerAddr);
  ip   = addr->sin_addr.s_addr;
  port = be16toh(addr->sin_port);

  numActions = params->numActions;
  reportCb   = params->reportCb;
  cbCtx     =  params->cbCtx;

  /* if (! parent->alreadyJoined(id,ip,port)) */
  /*   parent->joinMc(ip,port,parent->vlanTag); */

  EKA_LOG("Created Strategy %u: baseActionIdx=%u, numActions=%u, UDP triggers: %s:%u",
	  id,baseActionIdx,numActions,EKA_IP2STR(ip),port);
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::setEnableBits(epm_enablebits_t _enable) {
  enable = _enable;
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::getEnableBits(epm_enablebits_t *_enable) {
  *_enable = enable;
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

bool EpmStrategy::hasSame(uint32_t _ip, uint16_t _port) {
  return ip == _ip && port == _port;
}
/* ------------------------------------------------ */

bool EpmStrategy::myAction(epm_actionid_t actionId) {
  return actionId < numActions;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::setAction(epm_actionid_t actionIdx, const EpmAction *epmAction) {
  if (action[actionIdx] != NULL) return EKA_OPRESULT__ERR_INVALID_ACTION;

  action[actionIdx] = static_cast<EpmAction*>(malloc(sizeof(EpmAction)));
  if (action[actionIdx] == NULL) on_error("failed on malloc(sizeof(EpmAction))");
  
  memcpy(action[actionIdx],epmAction,sizeof(EpmAction));
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::getAction(epm_actionid_t actionIdx, EpmAction *epmAction) {
  if (action[actionIdx] == NULL) return EKA_OPRESULT__ERR_INVALID_ACTION;

  memcpy(epmAction,action[actionIdx],sizeof(EpmAction));
  return EKA_OPRESULT__OK;
}
