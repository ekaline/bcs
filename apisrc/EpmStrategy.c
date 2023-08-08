#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "EkaCore.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaIgmp.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"
#include "EkaUdpTxSess.h"
#include "EkaWc.h"
#include "EpmStrategy.h"

#if 0
ExcSessionId excGetSessionId(ExcConnHandle hConn);
EkaCoreId excGetCoreId(ExcConnHandle hConn);
/* unsigned short csum(unsigned short *ptr,int nbytes); */
/* uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr,
 * void* payload, uint16_t payload_size); */

/* ------------------------------------------------ */
EpmStrategy::EpmStrategy(EkaEpm *_epm, epm_strategyid_t _id,
                         epm_actionid_t _baseActionIdx,
                         const EpmStrategyParams *params,
                         EfhFeedVer _hwFeedVer) {
  on_error("Obsolete!");
  epm = _epm;
  if (!epm)
    on_error("!epm");
  dev = epm->dev;
  if (!dev)
    on_error("!dev");
  id = _id;
  baseActionIdx = _baseActionIdx;
  hwFeedVer = _hwFeedVer;

  numActions = params->numActions;
  reportCb = params->reportCb;
  cbCtx = params->cbCtx;

  maxActions_ = EkaEpmRegion::getMaxActions(id);

  if (numActions > maxActions_)
    on_error("region %d: numActions %d > "
             "maxActions_ %d",
             id, numActions, maxActions_);

  for (epm_actionid_t i = 0; i < numActions; i++) {
    action[i] = dev->epm->addAction(
        EkaEpm::ActionType::UserAction, id, i, -1, -1, -1);

    if (!action[i])
      on_error("Failed addAction");
  }

  for (auto i = 0; i < (int)params->numTriggers; i++) {
    udpSess[i] = new EkaUdpSess(
        dev, i, params->triggerParams[i].coreId,
        inet_addr(params->triggerParams[i].mcIp),
        params->triggerParams[i].mcUdpPort);
    dev->ekaIgmp->mcJoin(EkaEpmRegion::Regions::EfcMc,
                         udpSess[i]->coreId, udpSess[i]->ip,
                         udpSess[i]->port,
                         0,     // VLAN
                         NULL); // pPktCnt
  }

  numUdpSess = params->numTriggers;

  EKA_LOG("Created Strategy "
          "%u:baseActionIdx=%u,numActions=%u,numUdpSess=%u",
          id, baseActionIdx, numActions, numUdpSess);

  //  eka_write(dev,strategyEnableAddr(id), ALWAYS_ENABLE);
}
/* ------------------------------------------------ */

EkaOpResult
EpmStrategy::setEnableBits(epm_enablebits_t _enable) {
  enable = _enable;
  eka_write(dev, strategyEnableAddr(id), enable);
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

EkaOpResult
EpmStrategy::getEnableBits(epm_enablebits_t *_enable) {
  enable = eka_read(dev, strategyEnableAddr(id));
  *_enable = enable;
  return EKA_OPRESULT__OK;
}
/* ------------------------------------------------ */

/* bool EpmStrategy::hasSame(uint32_t _ip, uint16_t _port) {
 */
/*   return ip == _ip && port == _port; */
/* } */
/* ------------------------------------------------ */

#if 0
bool EpmStrategy::myAction(epm_actionid_t actionId) {
  if (actionId >= numActions) {
    EKA_WARN("actionId %d > numActions %d", actionId,
             numActions);
  }
  return actionId < numActions;
}
/* ------------------------------------------------ */

EkaOpResult
EpmStrategy::setAction(epm_actionid_t actionIdx,
                       const EpmAction *epmAction) {
  on_error("Obsolete!");
  if (actionIdx >= (int)numActions) {
    EKA_WARN("actionIdx %d >= numActions %u", actionIdx,
             numActions);
    return EKA_OPRESULT__ERR_INVALID_ACTION;
  }
  //---------------------------------------------------------
  EKA_LOG("Setting Action %d epmAction->type=%d", actionIdx,
          (int)epmAction->type);

  EkaEpmAction *ekaA = action[actionIdx];

  uint8_t coreId = excGetCoreId(epmAction->hConn);
  if (!dev->core[coreId])
    on_error("Wrong coreId %u", coreId);
  uint8_t sessId = excGetSessionId(epmAction->hConn);

  ekaA->updateAttrs(coreId, sessId, epmAction);

  EKA_LOG("ekaA->actionName=%s,isTcp()=%d,isUdp()=%d",
          ekaA->name_.c_str(), ekaA->isTcp(),
          ekaA->isUdp());

  if (ekaA->isUdp()) {
    if (!dev->core[coreId]->udpTxSess[sessId])
      on_error("Wrong Udp Tx sessId %u at core %u", sessId,
               coreId);
    EkaUdpTxSess *sess =
        dev->core[coreId]->udpTxSess[sessId];
    ekaA->setNwHdrs(sess->macDa_, sess->macSa_,
                    sess->srcIp_, sess->dstIp_,
                    sess->srcPort_, sess->dstPort_);
  }

  if (ekaA->isTcp()) {
    if (!dev->core[coreId]->tcpSess[sessId])
      on_error(
          "Wrong sessId %u at core %u, ekaA->type = %d, %s",
          sessId, coreId, (int)ekaA->type,
          ekaA->actionName);
    EkaTcpSess *sess = dev->core[coreId]->tcpSess[sessId];

    ekaA->setTcpSess(sess);

    ekaA->setNwHdrs(sess->macDa, sess->macSa, sess->srcIp,
                    sess->dstIp, sess->srcPort,
                    sess->dstPort);
  }
  //---------------------------------------------------------
  ekaA->copyHeap2Fpga();

  ekaA->initialized = true;

  //---------------------------------------------------------
  if (1) {
    EKA_LOG("Setting Action Idx %3d (Local Action Idx=%3d) "
            "for Strategy %2d: token=%ju, hConn=0x%x, "
            "offset=%5u,length=%3d,actionFlags=0x%x,"
            "nextAction=%3d,enable=%jx,postLocalMask=%jx,"
            "postStratMask=%jx, heapOffs=%7u, length=%u",
            baseActionIdx + actionIdx, actionIdx, id,
            epmAction->token, epmAction->hConn,
            epmAction->offset, epmAction->length,
            epmAction->actionFlags, epmAction->nextAction,
            epmAction->enable, epmAction->postLocalMask,
            epmAction->postStratMask, epmAction->offset,
            epmAction->length);
    fflush(stderr);
  }
  //---------------------------------------------------------
  // Writing Action to FPGA (Action Memory)
  copyBuf2Hw(dev, EkaEpm::EpmActionBase,
             (uint64_t *)&ekaA->hwAction,
             sizeof(ekaA->hwAction)); // write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /*ActionAddr*/, 0, 0,
                         ekaA->idx, 0);

  //  ekaA->printHeap();
  /* ekaA->printHwAction(); */

  //---------------------------------------------------------

  return EKA_OPRESULT__OK;
}
#endif
/* ------------------------------------------------ */

EkaOpResult EpmStrategy::getAction(epm_actionid_t actionIdx,
                                   EpmAction *epmAction) {
  on_error("Obsolete!");
  if (actionIdx >= maxActions_)
    return EKA_OPRESULT__ERR_INVALID_ACTION;

  memcpy(epmAction, &action[actionIdx]->epmActionLocalCopy,
         sizeof(EpmAction));

  return EKA_OPRESULT__OK;
}
#endif
