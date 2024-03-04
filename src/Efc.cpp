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

using namespace EkaBcs;

int printSecCtx(EkaDev *dev, const SecCtx *msg);
int printMdReport(EkaDev *dev, const EfcMdReport *msg);
int printControllerStateReport(
    EkaDev *dev, const EfcControllerState *msg);
int printBoeFire(EkaDev *dev, const BoeNewOrderMsg *msg);

EkaOpResult efcInit(EkaDev *dev,
                    const EfcInitCtx *pEfcInitCtx) {
  if (!dev || !dev->ekaHwCaps)
    on_error("(!dev || !dev->ekaHwCaps");
  dev->ekaHwCaps->checkEfc();

  if (!dev->checkAndSetEpmTx())
    on_error(
        "TX functionality is not available for this "
        "Ekaline SW instance - caught by another process");
  if (!pEfcInitCtx)
    on_error("!pEfcInitCtx");

  if (dev->efc)
    on_error("Efc is already initialized");

  if (!pEfcInitCtx->watchdog_timeout_sec)
    on_error("pEfcInitCtx->watchdog_timeout_sec == 0");

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
  auto efc = dev->efc;
  
  if (!epm->a_[globalIdx])
    on_error("Action[%d] does not exist", globalIdx);

  if (!efc->isReportOnly()) {
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
  } else {
    EKA_LOG("Action[%d]: set to dummy TCP socket",
	    globalIdx);
  }

  epm->a_[globalIdx]->copyHeap2Fpga();

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

/* --------------------------------------------------- */

/* --------------------------------------------------- */

/* --------------------------------------------------- */

EkaOpResult efcRun(EkaDev *dev,
                   const EfcRunCtx *pEfcRunCtx) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  dev->pEfcRunCtx = new EfcRunCtx;
  if (!dev->pEfcRunCtx)
    on_error("fauiled on new EfcRunCtx");

  memcpy(dev->pEfcRunCtx, pEfcRunCtx, sizeof(*pEfcRunCtx));

  efc->run(pEfcRunCtx);

  return EKA_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaOpResult efcSwKeepAliveSend(EkaDev *dev,
                               int strategyId) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  eka_write(dev, 0xf0608, 0);
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
  if (!dev || !dev->epm)
    on_error("!dev or Epm is not initialized");

  if (!dev->epm->a_[actionId])
    on_error("Acion[%d] is not set", actionId);

  return dev->epm->a_[actionId]->fastSend(buffer, size);
}

/* --------------------------------------------------- */
