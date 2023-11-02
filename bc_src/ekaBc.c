#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <functional>

#include "eka_macros.h"

#include "Efc.h"
#include "Eka.h"
#include "EkaBc.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmRegion.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpTxSess.h"

#include "EkaEurStrategy.h"

extern EkaDev *g_ekaDev;

EkaDev *
ekaBcOpenDev(const EkaBcAffinityConfig *affinityConf,
             const EkaBcCallbacks *cb) {
  EkaDevInitCtx initCtx = {};
  if (cb) {
    // initCtx.logCallback = cb->logCb;
    initCtx.logContext = cb->cbCtx;
  }
  g_ekaDev = new EkaDev(&initCtx);
  if (!g_ekaDev)
    return NULL;

  auto dev = g_ekaDev;

  int tcpInternalCountersThreadCpuId = -1;
  if (affinityConf)
    memcpy(&dev->affinityConf, affinityConf,
           sizeof(*affinityConf));

  return g_ekaDev;
}
/* ==================================================== */

int ekaBcCloseDev(EkaDev *dev) {
  if (!dev)
    return -1;
  delete dev;
  g_ekaDev = NULL;

  return 0;
}
/* ==================================================== */

EkaBcSock ekaBcTcpConnect(EkaDev *dev, EkaBcLane coreId,
                          const char *ip, uint16_t port) {
  if (!dev->checkAndSetEpmTx())
    on_error(
        "TX functionality is not available for this "
        "Ekaline SW instance - caught by another process");

  EkaCore *core = dev->core[coreId];
  if (!core)
    on_error("Lane %u has no link or IP address", coreId);

  if (!core->pLwipNetIf)
    core->initTcp();

  const auto sessId = core->addTcpSess();
  auto hSocket = core->tcpSess[sessId]->sock;

  EkaTcpSess *const sess = dev->findTcpSess(hSocket);
  if (!sess) {
    EKA_WARN("ExcSocketHandle %d not found", hSocket);
    errno = EBADF;
    return -1;
  } else if (sess->isEstablished()) {
    errno = EISCONN;
    return -1;
  }

  struct sockaddr_in dst = {};
  dst.sin_addr.s_addr = inet_addr(ip);
  dst.sin_family = AF_INET;
  dst.sin_port = be16toh(port);

  sess->dstIp = ((sockaddr_in *)&dst)->sin_addr.s_addr;
  sess->dstPort = be16toh(((sockaddr_in *)&dst)->sin_port);

  sess->bind();
  dev->snDev->set_fast_session(sess->coreId, sess->sessId,
                               sess->srcIp, sess->srcPort,
                               sess->dstIp, sess->dstPort);

  /* EKA_LOG("on coreId=%u, sessId=%u, sock=%d, %s:%u -->
   * %s:%u", */
  /* 	  sess->coreId,sess->sessId,sess->sock, */
  /* 	  EKA_IP2STR(sess->srcIp),sess->srcPort, */
  /* 	  EKA_IP2STR(sess->dstIp),sess->dstPort); */

  if (sess->connect() == -1)
    return -1;

  sess->updateFpgaCtx<EkaTcpSess::AppSeqBin>(0);
  char asciiZero[8] = {'0', '0', '0', '0',
                       '0', '0', '0', '0'};
  uint64_t asciiZeroNum;
  memcpy(&asciiZeroNum, asciiZero, 8);
  sess->updateFpgaCtx<EkaTcpSess::AppSeqAscii>(
      be64toh(asciiZeroNum));

  return hSocket;
}
/* ==================================================== */
int ekaBcSetBlocking(EkaDev *dev, EkaBcSock hSock,
                     bool blocking) {
  EkaTcpSess *const s =
      dev->findTcpSess(static_cast<ExcSocketHandle>(hSock));
  if (s)
    return s->setBlocking(blocking);

  EKA_WARN("EkaBcSock %d not found", hSock);
  errno = EBADF;
  return -1;
}
/* ==================================================== */

ssize_t ekaBcSend(EkaDev *dev, EkaBcSock ekaSock,
                  const void *buf, size_t size) {
  EkaTcpSess *const s = dev->findTcpSess(ekaSock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", ekaSock);
    return -1;
  }
  return s->sendPayload(s->sessId /* thrId */, (void *)buf,
                        size, 0);
}
/* ==================================================== */
#if 0
ssize_t ekaBcAppSend(EkaDev *dev, EkaBcActionIdx actionIdx,
                     const void *buffer, size_t size) {
  if (!dev || !dev->epm)
    on_error("!dev or Epm is not initialized");

  if (!dev->epm->a_[actionIdx])
    on_error("Acion[%d] is not set", actionIdx);

  return dev->epm->a_[actionIdx]->fastSend(buffer, size);
}
#endif

/* ==================================================== */

EkaBCOpResult ekaBcSetSessionCntr(EkaDev *dev,
                                  EkaBcSock ekaSock,
                                  uint64_t cntr) {
  EkaTcpSess *const s = dev->findTcpSess(ekaSock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", ekaSock);
    return EKABC_OPRESULT__ERR_TCP_SOCKET;
  }

  s->updateFpgaCtx<EkaTcpSess::AppSeqBin>(cntr);

  char cntrString[64] = {};
  sprintf(cntrString, "%08ju", cntr);

  uint64_t cntrAscii = 0;
  memcpy(&cntrAscii, cntrString, 8);
  s->updateFpgaCtx<EkaTcpSess::AppSeqAscii>(
      be64toh(cntrAscii));

  return EKABC_OPRESULT__OK;
}

/* ==================================================== */

ssize_t ekaBcRecv(EkaDev *dev, EkaBcSock sock, void *buf,
                  size_t size) {
  auto s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  return s->recv(buf, size, 0);
}

/* ==================================================== */

EkaBCOpResult ekaBcCloseSock(EkaDev *dev, EkaBcSock sock) {
  auto s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return EKABC_OPRESULT__ERR_TCP_SOCKET;
  }
  dev->core[s->coreId]->tcpSess[s->sessId] = nullptr;
  delete s;

  return EKABC_OPRESULT__OK;
}
/* ==================================================== */

EkaBCOpResult ekaBcInit(EkaDev *dev,
                        const EkaBcInitCtx *ekaBcInitCtx) {
  EfcInitCtx initCtx = {
      .report_only = ekaBcInitCtx->report_only,
      .watchdog_timeout_sec =
          ekaBcInitCtx->watchdog_timeout_sec};

  efcInit(dev, &initCtx);
  EKA_LOG("Efc initialized with "
          "report_only = %d, "
          "watchdog_timeout_sec = %ju",
          initCtx.report_only,
          initCtx.watchdog_timeout_sec);
  return EKABC_OPRESULT__OK;
}

/* ==================================================== */

void ekaBcSwKeepAliveSend(EkaDev *dev) {
  efcSwKeepAliveSend(dev, 0);
}

/* ==================================================== */

void ekaBcConfigurePort(EkaDev *dev, EkaBcLane lane,
                        const EkaBcPortAttrs *pPortAttrs) {
  EkaCoreInitCtx ekaCoreInitCtx = {
      .coreId = lane,
      .attrs = {.host_ip = pPortAttrs->host_ip,
                .netmask = pPortAttrs->netmask,
                .gateway = pPortAttrs->gateway}};
  memcpy(&ekaCoreInitCtx.attrs.nexthop_mac,
         pPortAttrs->nexthop_mac, 6);
  memcpy(&ekaCoreInitCtx.attrs.src_mac_addr,
         pPortAttrs->src_mac_addr, 6);

  ekaDevConfigurePort(dev, &ekaCoreInitCtx);
}
/* ==================================================== */

EkaBcActionIdx
ekaBcAllocateNewAction(EkaDev *dev, EkaBcActionType type) {
  return efcAllocateNewAction(
      dev, static_cast<EpmActionType>(type));
}

/* ==================================================== */

EkaBCOpResult
ekaBcSetActionPayload(EkaDev *dev, EkaBcActionIdx actionIdx,
                      const void *payload, size_t len) {

  efcSetActionPayload(dev, actionIdx, payload, len);
  return EKABC_OPRESULT__OK;
}
/* ==================================================== */

EkaBCOpResult ekaBcArmEur(EkaDev *dev,
                          EkaBcSecHandle prodHande,
                          bool armBid, bool armAsk,
                          EkaBcArmVer ver) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  EKA_LOG("Prod Handle: %d, "
          "armBid=%d, armAsk=%d, armVer=%d",
          prodHande, armBid, armAsk, ver);
  fflush(g_ekaLogFile);
  efc->armEur(prodHande, armBid, armAsk, ver);
  return EKABC_OPRESULT__OK;
}
/* ==================================================== */

EkaBCOpResult ekaBcArmCmeFc(EkaDev *dev, EkaBcArmVer ver) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  efc->armBcCmeFc(static_cast<EfcArmVer>(ver));
  return EKABC_OPRESULT__OK;
}
/* ==================================================== */

EkaBCOpResult ekaBcDisArmCmeFc(EkaDev *dev) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  efc->disarmBcCmeFc();
  return EKABC_OPRESULT__OK;
}

/* ==================================================== */

void ekaBcEurRun(EkaDev *dev,
                 const EkaBcRunCtx *pEkaBcRunCtx) {

  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");

  auto eur = dev->efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");

  EKA_LOG("Running EkaEurStrategy::runLoop()");
  fflush(g_ekaLogFile);

  auto loopFunc = std::bind(&EkaEurStrategy::runLoop, eur,
                            pEkaBcRunCtx);

  eur->runLoopThr_ = std::thread(loopFunc);
  EKA_LOG("EkaEurStrategy::runLoop() span off");
  fflush(g_ekaLogFile);
}

/* ==================================================== */

EkaBCOpResult
ekaBcSetActionTcpSock(EkaDev *ekaDev,
                      EkaBcActionIdx actionIdx,
                      EkaBcSock sock) {
  setActionTcpSock(ekaDev, actionIdx, sock);
  return EKABC_OPRESULT__OK;
}
/* ==================================================== */

EkaBCOpResult
ekaBcSetActionNext(EkaDev *ekaDev, EkaBcActionIdx actionIdx,
                   EkaBcActionIdx nextActionIdx) {
  setActionNext(ekaDev, actionIdx, nextActionIdx);
  return EKABC_OPRESULT__OK;
}
/* ==================================================== */

ssize_t ekaBcAppSend(EkaDev *pEkaDev,
                     EkaBcActionIdx actionIdx,
                     const void *buffer, size_t size) {
  return efcAppSend(pEkaDev, actionIdx, buffer, size);
}
/* ==================================================== */

EkaBCOpResult
ekaBcInitEurStrategy(EkaDev *dev,
                     const EkaBcUdpMcParams *mcParams) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  efc->initEur(
      reinterpret_cast<const EfcUdpMcParams *>(mcParams));
  EKA_LOG("EurStrategy initialized");
  return EKABC_OPRESULT__OK;
}
/* ==================================================== */
#if 0
void ekaBcInitEurStrategyDone(EkaDev *dev) {
  ekaDownloadConf2hw(dev);
  eka_join_mc_groups(dev);
  sleep(1);               // to allow book to get filled
  eka_read(dev, 0xf0728); // Clearing Interrupts
  eka_read(dev, 0xf0798); // Clearing Interrupts

  //------------------------------------

  runBook(dev->exch->fireLogic);
}
#endif
/* ==================================================== */

EkaBCOpResult ekaBcInitCmeFcStrategy(
    EkaDev *dev, const EkaBcUdpMcParams *mcParams,
    const EkaBcCmeFcAlgoParams *cmeFcParams) {

  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  efc->initBcCmeFc(
      reinterpret_cast<const EfcUdpMcParams *>(mcParams),
      cmeFcParams);

  return EKABC_OPRESULT__OK;
}

/* ==================================================== */

EkaBcSecHandle ekaBcGetSecHandle(EkaDev *dev,
                                 EkaBcEurSecId secId) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");

  return eur->getSubscriptionId(secId);
}

EkaBCOpResult
ekaBcSetProducts(EkaDev *dev, const EkaBcEurSecId *prodList,
                 size_t nProducts) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");

  if (!prodList)
    on_error("!prodList");

  if (nProducts > EKA_BC_EUR_MAX_PRODS) {
    EKA_ERROR("nProducts %ju > EKA_BC_EUR_MAX_PRODS %d",
              nProducts, EKA_BC_EUR_MAX_PRODS);
    return EKABC_OPRESULT__ERR_MAX_PRODUCTS_EXCEEDED;
  }

  return eur->subscribeSecList(prodList, nProducts);
}

/* ==================================================== */
EkaBCOpResult
ekaBcInitEurProd(EkaDev *dev, EkaBcSecHandle prodHande,
                 const EkaBcEurProductInitParams *params) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");

  return eur->initProd(prodHande, params);
}

/* ==================================================== */
EkaBCOpResult
ekaBcEurSetJumpParams(EkaDev *dev, EkaBcSecHandle prodHande,
                      const EkaBcEurJumpParams *params) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");
  EKA_LOG("Setting EkaBcEurJumpParams");
  fflush(g_ekaLogFile);
  return eur->setProdJumpParams(prodHande, params);
}
/* ==================================================== */

EkaBCOpResult ekaBcEurSetReferenceJumpParams(
    EkaDev *dev, EkaBcSecHandle triggerProd,
    EkaBcSecHandle fireProd,
    const EkaBcEurReferenceJumpParams *params) {
  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use ekaBcInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");
  EKA_LOG("Setting EkaBcEurReferenceJumpParams");

  return eur->setProdReferenceJumpParams(triggerProd,
                                         fireProd, params);
}
