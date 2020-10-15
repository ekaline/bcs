#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "EpmStrategy.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "EkaEpmAction.h"

ExcSessionId excGetSessionId( ExcConnHandle hConn );
EkaCoreId excGetCoreId( ExcConnHandle hConn );
unsigned short csum(unsigned short *ptr,int nbytes);

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

  if (numActions > (int)EkaEpm::MaxActionsPerStrategy)
    on_error("numActions %d > EkaEpm::MaxActionsPerStrategy %d",numActions,EkaEpm::MaxActionsPerStrategy);
  for (int i = 0; i < numActions; i++) {
    action[i].epmAction = static_cast<EpmAction*>(malloc(sizeof(EpmAction)));
    if (action[i].epmAction == NULL) on_error("failed on malloc(sizeof(EpmAction))");

    action[i].ekaAction = dev->epm->addAction(EkaEpm::ActionType::UserAction,-1,-1,-1);
    if (action[i].ekaAction == NULL) on_error("Failed addAction");
    
    //    EKA_LOG("Preloaded Action %3d for Strategy %u",i,id);
  }

  EKA_LOG("Created Strategy %u: baseActionIdx=%u, numActions=%u, UDP trigger: %s:%u",
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
  EKA_LOG("Setting Action %3d for Strategy %2d: token=%ju, hConn=0x%x, offset=%3ju,length=%d,actionFlags=0x%x,nextAction=%3d,enable=%d,postLocalMask=%x,postStratMask=%x",
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
  	  epmAction->postStratMask); fflush(stderr);

  //---------------------------------------------------------
  memcpy(action[actionIdx].epmAction,epmAction,sizeof(EpmAction));

  uint coreId = excGetCoreId(epmAction->hConn);
  if (dev->core[coreId] == NULL) on_error("Wrong coreId %u",coreId);
  uint sessId = excGetSessionId(epmAction->hConn);
  if (dev->core[coreId]->tcpSess[sessId] == NULL) on_error("Wrong sessId %u at core %u",sessId,coreId);

  int pktStart = epmAction->offset - EkaEpm::DatagramOffset;
  if (pktStart < 0 || (pktStart % 8 != 0)) on_error("Wrong epmAction->offset %d",epmAction->offset);

  memcpy(&dev->epm->heap[pktStart],dev->core[coreId]->tcpSess[sessId]->pktBuf,EkaEpm::DatagramOffset);

  uint64_t heapAddr = EkaEpm::UserHeapBaseAddr + pktStart;
  action[actionIdx].ekaAction->update(coreId,sessId,(uint)epmAction->nextAction,heapAddr);

  //---------------------------------------------------------
  EkaIpHdr* ipHdr = (EkaIpHdr*) &dev->epm->heap[pktStart + sizeof(EkaEthHdr)];
  ipHdr->_len    = be16toh(sizeof(EkaIpHdr) + sizeof(EkaTcpHdr) + epmAction->length);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) &dev->epm->heap[pktStart], 6 /*TBD!!! thrId */, EkaEpm::DatagramOffset + epmAction->length);

  //---------------------------------------------------------
  // Writing Action to FPGA
  copyBuf2Hw(dev,EkaEpm::EpmActionBase, (uint64_t*)&action[actionIdx].ekaAction->hwAction,sizeof(action[actionIdx].ekaAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,action[actionIdx].ekaAction->idx,0);
    
  //---------------------------------------------------------

  if (actionIdx == 100) {
    EKA_LOG("Action 100: data_db_ptr = %ju",action[actionIdx].ekaAction->hwAction.data_db_ptr);
    hexDump("Pkt copied to Heap for Action 100",&epm->heap[pktStart],EkaEpm::DatagramOffset + epmAction->length);
  }
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::getAction(epm_actionid_t actionIdx, EpmAction *epmAction) {
  if (actionIdx >= (int)EkaEpm::MaxActionsPerStrategy) return EKA_OPRESULT__ERR_INVALID_ACTION;

  memcpy(epmAction,&action[actionIdx].epmAction,sizeof(EpmAction));
  return EKA_OPRESULT__OK;
}
