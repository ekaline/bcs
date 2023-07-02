#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <linux/sockios.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "Efc.h"
#include "Efh.h"
#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"

#include "EkaEfcDataStructs.h"

int printSecCtx(EkaDev *dev, const SecCtx *msg);
int printMdReport(EkaDev *dev, const EfcMdReport *msg);
int printControllerStateReport(
    EkaDev *dev, const EfcControllerState *msg);
int printBoeFire(EkaDev *dev, const BoeNewOrderMsg *msg);

EkaOpResult efcInit(EfcCtx **ppEfcCtx, EkaDev *pEkaDev,
                    const EfcInitCtx *pEfcInitCtx) {
  auto dev = pEkaDev;
  if (!dev || !dev->ekaHwCaps)
    on_error("(!dev || !dev->ekaHwCaps");
  dev->ekaHwCaps->checkEfc();

  if (!dev->checkAndSetEpmTx())
    on_error(
        "TX functionality is not available for this "
        "Ekaline SW instance - caught by another process");
  if (!pEfcInitCtx)
    on_error("!pEfcInitCtx");
  *ppEfcCtx = new EfcCtx;
  if (!*ppEfcCtx)
    on_error("!*ppEfcCtx");

  (*ppEfcCtx)->dev = dev;

  if (dev->efc)
    on_error("Efc is already initialized");

  dev->efc = new EkaEfc(pEfcInitCtx);

  if (!dev->efc)
    on_error("Failed creating Efc");

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

epm_actionid_t
efcAllocateNewAction(const EkaDev *ekaDev,
                     EpmActionType actionType) {
  auto dev = ekaDev;
  if (!dev || !dev->epm || !dev->efc)
    on_error("!dev || !epm || !efc");
  auto epm = dev->epm;
  auto efc = dev->efc;

  return epm->allocateAction(actionType);
}
/* --------------------------------------------------- */

EkaOpResult efcSetAction(EkaDev *ekaDev,
                         epm_actionid_t globalIdx,
                         const EfcAction *efcAction) {
  on_error("FIX ME!");
#if 0
  auto dev = ekaDev;
  if (!dev || !dev->epm || !dev->efc)
    on_error("!dev || !epm || !efc");
  auto epm = dev->epm;
  auto efc = dev->efc;

  auto a = epm->a_[globalIdx];
  auto regionId = EkaEpmRegion::Regions::Efc;
  auto localIdx =
      globalIdx - EkaEpmRegion::getBaseActionIdx(regionId);

  auto actionType =
      efcAction->type != EpmActionType::INVALID
          ? efcAction->type
          : a->type;

  auto baseOffs =
      EkaEpmRegion::getActionHeapOffs(regionId, localIdx);

  auto dataOffs = isUdpAction(actionType)
                      ? EkaEpm::UdpDatagramOffset
                      : EkaEpm::TcpDatagramOffset;
  uint payloadOffs = baseOffs + dataOffs;

  const EpmAction epmAction = {
      .type = actionType,
      .token = efcAction->token,
      .hConn = efcAction->hConn,
      .offset = payloadOffs,
      .length = a->getPayloadLen(),
      .actionFlags = efcAction->actionFlags,
      .nextAction = efcAction->nextAction,
      .enable = efcAction->enable,
      .postLocalMask = efcAction->postLocalMask,
      .postStratMask = efcAction->postStratMask,
      .user = efcAction->user};

  return epm->setAction(EFC_STRATEGY, actionIdx,
                        &epmAction);
#endif
}
/* --------------------------------------------------- */

EkaOpResult efcSetActionPayload(EkaDev *ekaDev,
                                epm_actionid_t actionIdx,
                                const void *payload,
                                size_t len) {
  auto dev = ekaDev;
  if (!dev || !dev->epm)
    on_error("!dev || !epm");
  auto epm = dev->epm;
  auto efc = dev->efc;
  if (!efc)
    on_error("!efc");

  auto a = epm->a_[actionIdx];

  if (!a)
    on_error("action[%d] was not allocated", actionIdx);

  a->setPayload(payload, len);

  EKA_LOG("EFC Action[%d]: %ju bytes copied Heap",
          actionIdx, len);

  //	a->printHeap();

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcInitP4Strategy(EfcCtx *pEfcCtx,
                              const EfcP4Params *p4Params) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->initP4(p4Params);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcArmP4(EfcCtx *pEfcCtx, bool arm,
                     EfcArmVer ver) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  if (arm)
    efc->armP4(ver);
  else
    efc->disarmP4();
  return EKA_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaOpResult
efcEnableFiringOnSec(EfcCtx *pEfcCtx,
                     const uint64_t *pSecurityIds,
                     size_t numSecurityIds) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  if (!efc->p4_)
    on_error("P4 is not initialized: "
             "use efcInitP4Strategy()");

  auto p4 = efc->p4_;

  if (!pSecurityIds)
    on_error("!pSecurityIds");

  uint64_t *p = (uint64_t *)pSecurityIds;
  for (uint i = 0; i < numSecurityIds; i++) {
    //    EKA_LOG("Subscribing on 0x%jx",*p);
    p4->subscribeSec(*p);
    p++;
  }
  p4->downloadTable();

  uint64_t subscrCnt =
      (numSecurityIds << 32) | p4->numSecurities_;
  eka_write(dev, SCRPAD_EFC_SUBSCR_CNT, subscrCnt);

  EKA_LOG("Tried to subscribe on %u, succeeded on %d",
          (uint)numSecurityIds, p4->numSecurities_);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EfcSecCtxHandle getSecCtxHandle(EfcCtx *pEfcCtx,
                                uint64_t securityId) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  if (!efc->p4_)
    on_error("P4 is not initialized: "
             "use efcInitP4Strategy()");

  auto p4 = efc->p4_;
  return p4->getSubscriptionId(securityId);
}

/* --------------------------------------------------- */

EkaOpResult efcSetStaticSecCtx(EfcCtx *pEfcCtx,
                               EfcSecCtxHandle hSecCtx,
                               const SecCtx *pSecCtx,
                               uint16_t writeChan) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  if (!efc->p4_)
    on_error("P4 is not initialized: "
             "use efcInitP4Strategy()");

  auto p4 = efc->p4_;

  if (hSecCtx < 0)
    on_error("hSecCtx = %jd", hSecCtx);

  if (!pSecCtx)
    on_error("!pSecCtx");

  if (static_cast<uint8_t>(p4->secIdList_[hSecCtx] &
                           0xFF) !=
      pSecCtx->lowerBytesOfSecId)
    on_error("EFC_CTX_SANITY_CHECK error: "
             "secIdList[%jd] 0x%016jx & 0xFF != %02x",
             hSecCtx, p4->secIdList_[hSecCtx],
             pSecCtx->lowerBytesOfSecId);

  // This copy is needed because EkaHwSecCtx is packed and
  // SecCtx is not!
  const EkaHwSecCtx hwSecCtx = {
      .bidMinPrice = pSecCtx->bidMinPrice,
      .askMaxPrice = pSecCtx->askMaxPrice,
      .bidSize = pSecCtx->bidSize,
      .askSize = pSecCtx->askSize,
      .versionKey = pSecCtx->versionKey,
      .lowerBytesOfSecId = pSecCtx->lowerBytesOfSecId};

  p4->writeSecHwCtx(hSecCtx, &hwSecCtx, writeChan);

  return EKA_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaOpResult efcRun(EfcCtx *pEfcCtx,
                   const EfcRunCtx *pEfcRunCtx) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  dev->pEfcRunCtx = new EfcRunCtx;
  if (!dev->pEfcRunCtx)
    on_error("fauiled on new EfcRunCtx");

  memcpy(dev->pEfcRunCtx, pEfcRunCtx, sizeof(*pEfcRunCtx));

  efc->run(pEfcCtx, pEfcRunCtx);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcSwKeepAliveSend(EfcCtx *pEfcCtx,
                               int strategyId) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  eka_write(dev, 0xf0608, 0);
  return EKA_OPRESULT__OK;
}

EkaOpResult efcClose(EfcCtx *efcCtx) {
  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcSetSessionCntr(EkaDev *dev,
                              ExcConnHandle hConn,
                              uint64_t cntr) {
  auto coreId = excGetCoreId(hConn);
  auto sessId = excGetSessionId(hConn);
  if (!dev || !dev->core[coreId] ||
      !dev->core[coreId]->tcpSess[sessId])
    on_error("hConn 0x%x does not exist", hConn);

  auto tcpS = dev->core[coreId]->tcpSess[sessId];

  tcpS->updateFpgaCtx<EkaTcpSess::AppSeqBin>(cntr);

  char cntrString[64] = {};
  sprintf(cntrString, "%08ju", cntr);

  uint64_t cntrAscii = 0;
  memcpy(&cntrAscii, cntrString, 8);
  tcpS->updateFpgaCtx<EkaTcpSess::AppSeqAscii>(
      be64toh(cntrAscii));

  return EKA_OPRESULT__OK;
}
