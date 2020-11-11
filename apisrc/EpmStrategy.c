#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "EpmStrategy.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "EkaEpmAction.h"

ExcSessionId excGetSessionId( ExcConnHandle hConn );
EkaCoreId excGetCoreId( ExcConnHandle hConn );
unsigned short csum(unsigned short *ptr,int nbytes);
uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);

/* ------------------------------------------------ */
EpmStrategy::EpmStrategy(EkaEpm* _parent, epm_strategyid_t _id, epm_actionid_t _baseActionIdx, const EpmStrategyParams *params) {
  epm = _parent;
  dev = epm->dev;
  id = _id;
  baseActionIdx = _baseActionIdx;

  setActionRegionBaseIdx(dev,id,baseActionIdx);

  const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(params->triggerAddr);
  ip   = addr->sin_addr.s_addr;
  port = be16toh(addr->sin_port);

  uint64_t tmp_ipport = (uint64_t) (id) << 56 | (uint64_t) port << 32 | (uint64_t) be32toh(ip);
  EKA_LOG("HW Port-IP register = 0x%016jx (%x : %x)",tmp_ipport,ip,port);
  eka_write (dev,FH_GROUP_IPPORT,tmp_ipport);

  numActions = params->numActions;
  reportCb   = params->reportCb;
  cbCtx     =  params->cbCtx;

  if (numActions > (int)EkaEpm::MaxActionsPerStrategy)
    on_error("numActions %d > EkaEpm::MaxActionsPerStrategy %d",numActions,EkaEpm::MaxActionsPerStrategy);

  for (epm_actionid_t i = 0; i < numActions; i++) {
    action[i] = dev->epm->addAction(EkaEpm::ActionType::UserAction, id, i, -1,-1,-1);
    if (action[i] == NULL) on_error("Failed addAction");
  }

  EKA_LOG("Created Strategy %u: baseActionIdx=%u, numActions=%u, UDP trigger: %s:%u",
	  id,baseActionIdx,numActions,EKA_IP2STR(ip),port);
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::setEnableBits(epm_enablebits_t _enable) {
  enable = _enable;
  eka_write(dev,strategyEnableAddr(id),enable);
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::getEnableBits(epm_enablebits_t *_enable) {
  enable = eka_read(dev,strategyEnableAddr(id));
  *_enable = enable;
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

bool EpmStrategy::hasSame(uint32_t _ip, uint16_t _port) {
  return ip == _ip && port == _port;
}
/* ------------------------------------------------ */

bool EpmStrategy::myAction(epm_actionid_t actionId) {
  if (actionId >= numActions) {
    EKA_WARN("actionId %d > numActions %d",actionId,numActions);
  }
  return actionId < numActions;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::setAction(epm_actionid_t actionIdx, const EpmAction *epmAction) {
  if (actionIdx >= (int)numActions) {
    EKA_WARN("actionIdx %d >= numActions %u",actionIdx, numActions);
    return EKA_OPRESULT__ERR_INVALID_ACTION;
  }
  //---------------------------------------------------------

  EkaEpmAction *ekaA = action[actionIdx];

  uint8_t coreId = excGetCoreId(epmAction->hConn);
  if (dev->core[coreId] == NULL) on_error("Wrong coreId %u",coreId);
  uint8_t sessId = excGetSessionId(epmAction->hConn);
  if (dev->core[coreId]->tcpSess[sessId] == NULL) on_error("Wrong sessId %u at core %u",sessId,coreId);
  EkaTcpSess* sess = dev->core[coreId]->tcpSess[sessId];
  //---------------------------------------------------------

  ekaA->updateAttrs(coreId,sessId,epmAction);

  ekaA->setNwHdrs(sess->macDa,sess->macSa,sess->srcIp,sess->dstIp,sess->srcPort,sess->dstPort);

  ekaA->setPktPayload(/* thrId,  */&epm->heap[epmAction->offset], epmAction->length);




  //---------------------------------------------------------
  //  if (id == 4 && actionIdx == 100)
  if (0) {
    EKA_LOG("Setting Action Idx %3d (Local Action Idx=%3d) for Strategy %2d: token=%ju, hConn=0x%x, offset=%5ju,length=%3d,actionFlags=0x%x,nextAction=%3d,enable=%d,postLocalMask=%x,postStratMask=%x, heapOffs=%7ju, length=%u",
  	    baseActionIdx + actionIdx,
	    actionIdx,
  	    id,
  	    epmAction->token,
  	    epmAction->hConn,
  	    epmAction->offset,
  	    epmAction->length,
  	    epmAction->actionFlags,
  	    epmAction->nextAction,
  	    epmAction->enable,
  	    epmAction->postLocalMask,
  	    epmAction->postStratMask,
  	    epmAction->offset, 
	    epmAction->length); 
    fflush(stderr);
  }
  //---------------------------------------------------------
  // Writing Action to FPGA (Action Memory)
  copyBuf2Hw(dev,EkaEpm::EpmActionBase, (uint64_t*)&ekaA->hwAction,sizeof(ekaA->hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,ekaA->idx,0);
    
  //---------------------------------------------------------

  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::getAction(epm_actionid_t actionIdx, EpmAction *epmAction) {
  if (actionIdx >= (int)EkaEpm::MaxActionsPerStrategy) return EKA_OPRESULT__ERR_INVALID_ACTION;

  memcpy(epmAction,&action[actionIdx]->epmActionLocalCopy,sizeof(EpmAction));
  return EKA_OPRESULT__OK;
}
