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
efcAllocateNewAction(EkaDev *ekaDev,
                     EpmActionType actionType) {
  auto dev = ekaDev;
  if (!dev || !dev->epm || !dev->efc)
    on_error("!dev || !epm || !efc");
  auto epm = dev->epm;
  auto efc = dev->efc;

  return epm->allocateAction(actionType);
}
/* --------------------------------------------------- */

EkaOpResult setActionTcpSock(EkaDev *ekaDev,
                             epm_actionid_t globalIdx,
                             ExcSocketHandle excSock) {
  auto dev = ekaDev;
  if (!dev || !dev->epm || !dev->efc)
    on_error("!dev || !epm || !efc");
  auto epm = dev->epm;

  if (!epm->a_[globalIdx])
    on_error("Action[%d] does not exist", globalIdx);

  EkaTcpSess *const sess = dev->findTcpSess(excSock);

  if (!sess)
    on_error("excSock %d does not exist", excSock);

  epm->a_[globalIdx]->setTcpSess(sess);

  EKA_LOG("Action[%d]: set TCP socket %d  "
          "%s %s:%u -->  %s  %s:%u ",
          globalIdx, excSock, EKA_MAC2STR(sess->macSa),
          EKA_IP2STR(sess->srcIp), sess->srcPort,
          EKA_MAC2STR(sess->macDa), EKA_IP2STR(sess->dstIp),
          sess->dstPort);

  return EKA_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaOpResult
setActionNext(EkaDev *ekaDev, epm_actionid_t globalIdx,
              epm_actionid_t nextActionGlobalIdx) {
  auto dev = ekaDev;
  if (!dev || !dev->epm || !dev->efc)
    on_error("!dev || !epm || !efc");
  auto epm = dev->epm;

  if (!epm->a_[globalIdx])
    on_error("Action[%d] does not exist", globalIdx);

  epm->a_[globalIdx]->setNextAction(nextActionGlobalIdx);

  return EKA_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaOpResult setActionPhysicalLane(EkaDev *ekaDev,
                                  epm_actionid_t globalIdx,
                                  EkaCoreId lane) {
  auto dev = ekaDev;
  if (!dev || !dev->epm || !dev->efc)
    on_error("!dev || !epm || !efc");
  auto epm = dev->epm;

  if (!epm->a_[globalIdx])
    on_error("Action[%d] does not exist", globalIdx);

  epm->a_[globalIdx]->setCoreId(lane);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

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

  if (len > 1448)
    on_error("len %ju is too high", len);

  auto a = epm->a_[actionIdx];

  if (!a)
    on_error("action[%d] was not allocated", actionIdx);

  char bufStr[10000] = {};
  hexDump2str("Payload set", payload, len, bufStr,
              sizeof(bufStr));
  a->setPayload(payload, len);

  EKA_LOG("EFC Action[%d]: payload set: %s", actionIdx,
          bufStr);

  //	a->printHeap();

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult
efcInitP4Strategy(EfcCtx *pEfcCtx,
                  const EfcUdpMcParams *mcParams,
                  const EfcP4Params *p4Params) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->initP4(mcParams, p4Params);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcArmP4(EfcCtx *pEfcCtx, EfcArmVer ver) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->armP4(ver);

  return EKA_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaOpResult efcDisArmP4(EfcCtx *pEfcCtx) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

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
/* --------------------------------------------------- */

ssize_t efcAppSend(EkaDev *dev, epm_actionid_t actionId,
                   const void *buffer, size_t size) {
  if (!dev)
    on_error("! dev");

  if (!dev->epm)
    on_error("Epm is not initialized");

  if (!dev->epm->a_[actionId])
    on_error("Acion[%d] is not set", actionId);

  return dev->epm->a_[actionId]->fastSend(buffer, size);
}

/* --------------------------------------------------- */

EkaOpResult
efcInitQedStrategy(EfcCtx *pEfcCtx,
                   const EfcUdpMcParams *mcParams,
                   const EfcQedParams *qedParams) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->initQed(mcParams, qedParams);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcQedSetFireAction(EfcCtx *pEfcCtx,
                                epm_actionid_t fireActionId,
                                int productId) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;
  efc->qedSetFireAction(fireActionId, productId);
  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcArmQed(EfcCtx *pEfcCtx, EfcArmVer ver) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->armQed(ver);

  return EKA_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaOpResult efcDisArmQed(EfcCtx *pEfcCtx) {
  if (!pEfcCtx || !pEfcCtx->dev)
    on_error("!pEfcCtx || !pEfcCtx->dev");
  auto dev = pEfcCtx->dev;
  if (!dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->disarmQed();
  return EKA_OPRESULT__OK;
}