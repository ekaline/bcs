#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <functional>

#include "eka_macros.h"

#include "Efc.h"
#include "Eka.h"
#include "EkaBcs.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmRegion.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpTxSess.h"

#include "EkaEurStrategy.h"
#include "EkaMoexStrategy.h"
#include "EkaMdRecvHandler.h"

extern EkaDev *g_ekaDev;
int ekaAddArpEntry(EkaDev *dev, EkaCoreId coreId,
                   const uint32_t *protocolAddr,
                   const uint8_t *hwAddr);

namespace EkaBcs {

OpResult openDev(const EkaBcAffinityConfig *affinityConf,
                 const EkaCallbacks *cb) {
  EkaDevInitCtx initCtx = {};
  if (cb) {
    // initCtx.logCallback = cb->logCb;
    initCtx.logContext = cb->cbCtx;
  }
  g_ekaDev = new EkaDev(&initCtx);
  if (!g_ekaDev)
    return OPRESULT__ERR_DEVICE_INIT;

  int tcpInternalCountersThreadCpuId = -1;
  if (affinityConf)
    memcpy(&g_ekaDev->affinityConf, affinityConf,
           sizeof(*affinityConf));

  return OPRESULT__OK;
}
/* ==================================================== */

OpResult closeDev() {
  if (!g_ekaDev)
    on_error("!g_ekaDev");

  EKA_LOG("Closing Ekaline device");

  delete g_ekaDev;
  g_ekaDev = NULL;

  return OPRESULT__OK;
}
/* ==================================================== */

OpResult addArpEntry(EkaBcLane lane, const uint32_t *ipAddr,
                     const uint8_t *macAddr) {

  auto rc = ekaAddArpEntry(g_ekaDev, lane, ipAddr, macAddr);

  if (rc)
    return OPRESULT__ERR_A;

  return OPRESULT__OK;
}

/* ==================================================== */

EkaSock tcpConnect(EkaBcLane coreId, const char *ip,
                   uint16_t port) {
  if (!g_ekaDev->checkAndSetEpmTx())
    on_error(
        "TX functionality is not available for this "
        "Ekaline SW instance - caught by another process");

  EkaCore *core = g_ekaDev->core[coreId];
  if (!core)
    on_error("Lane %u has no link or IP address", coreId);

  if (!core->pLwipNetIf)
    core->initTcp();

  const auto sessId = core->addTcpSess();
  auto hSocket = core->tcpSess[sessId]->sock;

  EkaTcpSess *const sess = g_ekaDev->findTcpSess(hSocket);
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
  g_ekaDev->snDev->set_fast_session(
      sess->coreId, sess->sessId, sess->srcIp,
      sess->srcPort, sess->dstIp, sess->dstPort);

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
int ekaBcSetBlocking(EkaSock hSock, bool blocking) {
  EkaTcpSess *const s = g_ekaDev->findTcpSess(
      static_cast<ExcSocketHandle>(hSock));
  if (s)
    return s->setBlocking(blocking);

  EKA_WARN("EkaSock %d not found", hSock);
  errno = EBADF;
  return -1;
}
/* ==================================================== */

ssize_t tcpSend(EkaSock ekaSock, const void *buf,
                size_t size) {
  EkaTcpSess *const s = g_ekaDev->findTcpSess(ekaSock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", ekaSock);
    return -1;
  }
  return s->sendPayload(s->sessId /* thrId */, (void *)buf,
                        size, 0);
}
/* ==================================================== */
#if 0
ssize_t appTcpSend(EkaDev *dev, EkaActionIdx actionIdx,
                     const void *buffer, size_t size) {
  if (!dev || !dev->epm)
    on_error("!dev or Epm is not initialized");

  if (!dev->epm->a_[actionIdx])
    on_error("Acion[%d] is not set", actionIdx);

  return dev->epm->a_[actionIdx]->fastSend(buffer, size);
}
#endif

/* ==================================================== */

OpResult setSessionCntr(EkaSock ekaSock, uint64_t cntr) {
  EkaTcpSess *const s = g_ekaDev->findTcpSess(ekaSock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", ekaSock);
    return OPRESULT__ERR_TCP_SOCKET;
  }

  s->updateFpgaCtx<EkaTcpSess::AppSeqBin>(cntr);
  s->updateFpgaCtx<EkaTcpSess::AppSeqAscii>(cntr);

  // in eurex, both use binary, ascii is used for clordid

  /* char cntrString[64] = {}; */
  /* sprintf(cntrString, "%08ju", cntr); */

  /* uint64_t cntrAscii = 0; */
  /* memcpy(&cntrAscii, cntrString, 8); */
  /* s->updateFpgaCtx<EkaTcpSess::AppSeqAscii>( */
  /*     be64toh(cntrAscii)); */

  return OPRESULT__OK;
}

/* ==================================================== */

ssize_t tcpRecv(EkaSock sock, void *buf, size_t size) {
  auto s = g_ekaDev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  return s->recv(buf, size, 0);
}

/* ==================================================== */

OpResult closeSock(EkaSock sock) {
  auto s = g_ekaDev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return OPRESULT__ERR_TCP_SOCKET;
  }
  g_ekaDev->core[s->coreId]->tcpSess[s->sessId] = nullptr;
  delete s;

  return OPRESULT__OK;
}
/* ==================================================== */

OpResult hwEngInit(const HwEngInitCtx *ekaBcInitCtx) {
  if (!g_ekaDev) {
    EKA_ERROR("!g_ekaDev");
    return OPRESULT__ERR_DEVICE_INIT;
  }
  EfcInitCtx initCtx = {
      .report_only = ekaBcInitCtx->report_only,
      .watchdog_timeout_sec =
          ekaBcInitCtx->watchdog_timeout_sec};

  efcInit(g_ekaDev, &initCtx);
  EKA_LOG("HW Engine initialized with "
          "report_only = %d, "
          "watchdog_timeout_sec = %ju",
          initCtx.report_only,
          initCtx.watchdog_timeout_sec);
  return OPRESULT__OK;
}

/* ==================================================== */

void swKeepAliveSend() { efcSwKeepAliveSend(g_ekaDev, 0); }

/* ==================================================== */

OpResult configurePort(EkaBcLane lane,
                       const PortAttrs *pPortAttrs) {
  if (!g_ekaDev)
    return OPRESULT__ERR_DEVICE_INIT;
  EkaCoreInitCtx ekaCoreInitCtx = {
      .coreId = lane,
      .attrs = {.host_ip = pPortAttrs->host_ip,
                .netmask = pPortAttrs->netmask,
                .gateway = pPortAttrs->gateway}};
  memcpy(&ekaCoreInitCtx.attrs.nexthop_mac,
         pPortAttrs->nexthop_mac, 6);
  memcpy(&ekaCoreInitCtx.attrs.src_mac_addr,
         pPortAttrs->src_mac_addr, 6);

  ekaDevConfigurePort(g_ekaDev, &ekaCoreInitCtx);
  return OPRESULT__OK;
}
/* ==================================================== */

EkaActionIdx allocateNewAction(EkaBcActionType type) {
  return efcAllocateNewAction(
      g_ekaDev, static_cast<EpmActionType>(type));
}

/* ==================================================== */

OpResult setActionPayload(EkaActionIdx actionIdx,
                          const void *payload, size_t len) {

  efcSetActionPayload(g_ekaDev, actionIdx, payload, len);
  return OPRESULT__OK;
}
/* ==================================================== */

OpResult ekaBcArmEur(EkaBcSecHandle prodHande, bool armBid,
                     bool armAsk, EkaBcArmVer ver) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  //  EKA_LOG("Prod Handle: %jd, "
  //          "armBid=%d, armAsk=%d, armVer=%d",
  //          prodHande, armBid, armAsk, ver);
  //  fflush(g_ekaLogFile);
  efc->armEur(prodHande, armBid, armAsk, ver);
  return OPRESULT__OK;
}
/* ==================================================== */

OpResult ekaBcArmCmeFc(EkaDev *dev, EkaBcArmVer ver) {
  if (!dev || !dev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = dev->efc;

  efc->armBcCmeFc(static_cast<EfcArmVer>(ver));
  return OPRESULT__OK;
}
/* ==================================================== */

OpResult ekaBcDisArmCmeFc(EkaDev *dev) {
  if (!dev || !dev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = dev->efc;

  efc->disarmBcCmeFc();
  return OPRESULT__OK;
}

/* ==================================================== */

void EkaBcsMoexRun(const EkaBcsRunCtx *pEkaBcsRunCtx) {

  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");

  auto moex = g_ekaDev->efc->moex_;
  if (!moex)
    on_error("Moex is not initialized: use "
             "ekaBcsInitMoexStrategy()");

  EKA_LOG("Joining UDP Channels");
  moex->joinUdpChannels();

  EKA_LOG("Updating Scratchpad");
  moex->downloadProdInfoDB();

  /* ----------------------------------------------- */
  if (!pEkaBcsRunCtx)
    on_error("!pEkaBcsRunCtx");
  if (!pEkaBcsRunCtx->onReportCb)
    on_error("!pEfcRunCtx->onReportCb");
  /* ----------------------------------------------- */

  EKA_LOG("Lounching "
          "EkaMoexStrategy::fireReportThreadLoop()");
  auto fireReportLoopFunc =
      std::bind(&EkaMoexStrategy::fireReportThreadLoop,
                moex, pEkaBcsRunCtx);
  moex->fireReportLoopThr_ =
      std::thread(fireReportLoopFunc);
  EKA_LOG("EkaMoexStrategy::fireReportThreadLoop() "
          "span off");
  fflush(g_ekaLogFile);
  /* ----------------------------------------------- */

  g_ekaDev->efc->setHwUdpParams();
  g_ekaDev->efc->enableRxFire();

  EKA_LOG("Lounching EkaMoexStrategy::runLoop()");
  auto loopFunc = std::bind(&EkaMoexStrategy::runLoop, moex,
                            pEkaBcsRunCtx);

  moex->runLoopThr_ = std::thread(loopFunc);

  EKA_LOG("EkaMoexStrategy::runLoop() span off");
  fflush(g_ekaLogFile);
}

/* ==================================================== */

OpResult setActionTcpSock(EkaActionIdx actionIdx,
                          EkaSock sock) {
  setActionTcpSock(g_ekaDev, actionIdx, sock);
  return OPRESULT__OK;
}
/* ==================================================== */

OpResult setActionNext(EkaActionIdx actionIdx,
                       EkaActionIdx nextActionIdx) {
  setActionNext(g_ekaDev, actionIdx, nextActionIdx);
  return OPRESULT__OK;
}
/* ==================================================== */

ssize_t appTcpSend(EkaActionIdx actionIdx,
                   const void *buffer, size_t size) {
  return efcAppSend(g_ekaDev, actionIdx, buffer, size);
}
/* ==================================================== */

OpResult initMoexStrategy(const UdpMcParams *mcParams) {
  if (!g_ekaDev)
    on_error("!g_ekaDev");
  if (!g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  efc->initMoex(
      reinterpret_cast<const EfcUdpMcParams *>(mcParams));
  EKA_LOG("Moex Strategy initialized");
  return OPRESULT__OK;
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

OpResult ekaBcInitCmeFcStrategy(
    EkaDev *dev, const UdpMcParams *mcParams,
    const EkaBcCmeFcAlgoParams *cmeFcParams) {

  if (!dev || !dev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = dev->efc;

  efc->initBcCmeFc(
      reinterpret_cast<const EfcUdpMcParams *>(mcParams),
      cmeFcParams);

  return OPRESULT__OK;
}
/* ==================================================== */

OpResult configureRcvMd(int idx,
                        const UdpMcParams *mcParams,
                        MdProcessCallback cb, void *ctx) {
  if (!g_ekaDev) {
    EKA_ERROR("!g_ekaDev");
    return OPRESULT__ERR_DEVICE_INIT;
  }

  if (g_ekaDev->mdRecvH[idx]) {
    EKA_ERROR("MdRecvHandler[%i] is already configured",
              idx);
    return OPRESULT__ALREADY_INITIALIZED;
  }

  auto lane = idx == 0 ? 0 : idx == 1 ? 2 : -1;

  g_ekaDev->mdRecvH[idx] =
      new EkaMdRecvHandler(lane, mcParams, cb, ctx);

  if (!g_ekaDev->mdRecvH[idx])
    on_error("Failed to create g_ekaDev->mdRecvH[%d]", idx);
  return OPRESULT__OK;
}
/* ==================================================== */

OpResult configureRcvMd_A(const UdpMcParams *mcParams,
                          MdProcessCallback cb, void *ctx) {
  return configureRcvMd(0, mcParams, cb, ctx);
}
/* ==================================================== */

OpResult configureRcvMd_B(const UdpMcParams *mcParams,
                          MdProcessCallback cb, void *ctx) {
  return configureRcvMd(1, mcParams, cb, ctx);
}
/* ==================================================== */

/* ==================================================== */

void startRcvMd(int idx) {
  if (!g_ekaDev)
    on_error("!g_ekaDev");

  if (!g_ekaDev->mdRecvH[idx])
    on_error("!g_ekaDev->mdRecvH[%d]", idx);

  g_ekaDev->mdRecvH[idx]->run();
}
/* ==================================================== */

void startRcvMd_A() { startRcvMd(0); }
/* ==================================================== */

void startRcvMd_B() { startRcvMd(1); }

/* ==================================================== */

/* ==================================================== */

OpResult stopRcvMd(int idx) {
  if (!g_ekaDev) {
    EKA_ERROR("!g_ekaDev");
    return OPRESULT__ERR_DEVICE_INIT;
  }

  if (!g_ekaDev->mdRecvH[idx])
    on_error("!g_ekaDev->mdRecvH[%d]", idx);

  g_ekaDev->mdRecvH[idx]->stop();

  return OPRESULT__OK;
}
/* ==================================================== */

OpResult stopRcvMd_A() { return stopRcvMd(0); }
/* ==================================================== */

OpResult stopRcvMd_B() { return stopRcvMd(1); }

/* ==================================================== */

/* ==================================================== */

EkaBcSecHandle ekaBcsGetSecHandle(EkaBcsMoexSecId secId) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  auto moex = efc->moex_;
  if (!moex)
    on_error("Moex is not initialized: use "
             "initMoexStrategy()");

  // tbd subscription
  //   return eur->getSubscriptionId(secId);
  return OPRESULT__OK;
}

OpResult ekaBcsSetProducts(const EkaBcsMoexSecId *prodList,
                           size_t nProducts) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  auto moex = efc->moex_;
  if (!moex)
    on_error("Moex is not initialized: use "
             "initMoexStrategy()");

  if (!prodList)
    on_error("!prodList");

  // TBD moex products
  //  if (nProducts > EKA_BC_EUR_MAX_PRODS) {
  //    EKA_ERROR("nProducts %ju > EKA_BC_EUR_MAX_PRODS %d",
  //              nProducts, EKA_BC_EUR_MAX_PRODS);
  //    return OPRESULT__ERR_MAX_PRODUCTS_EXCEEDED;
  //  }

  // return eur->subscribeSecList(prodList, nProducts);
  return OPRESULT__OK;
}

/* ==================================================== */
OpResult
ekaBcInitEurProd(EkaBcSecHandle prodHande,
                 const EkaBcEurProductInitParams *params) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");

  return eur->initProd(prodHande, params);
}
/* ==================================================== */
OpResult ekaBcSetEurProdDynamicParams(
    EkaBcSecHandle prodHande,
    const EkaBcProductDynamicParams *params) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");
  //  EKA_LOG("Setting Product[%jd] Dynamic Params",
  //  prodHande); fflush(g_ekaLogFile);
  return eur->setProdDynamicParams(prodHande, params);
}

/* ==================================================== */
OpResult
ekaBcEurSetJumpParams(EkaDev *dev, EkaBcSecHandle prodHande,
                      const EkaBcEurJumpParams *params) {
  if (!dev || !dev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");
  //  EKA_LOG("Setting EkaBcEurJumpParams");
  //  fflush(g_ekaLogFile);
  return eur->setProdJumpParams(prodHande, params);
}
/* ==================================================== */

OpResult ekaBcEurSetReferenceJumpParams(
    EkaDev *dev, EkaBcSecHandle triggerProd,
    EkaBcSecHandle fireProd,
    const EkaBcEurReferenceJumpParams *params) {
  if (!dev || !dev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = dev->efc;

  auto eur = efc->eur_;
  if (!eur)
    on_error("Eurex is not initialized: use "
             "ekaBcInitEurStrategy()");
  //  EKA_LOG("Setting EkaBcEurReferenceJumpParams");

  return eur->setProdReferenceJumpParams(triggerProd,
                                         fireProd, params);
}
} // End of namespace EkaBcs
