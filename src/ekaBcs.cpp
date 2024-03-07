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

#include "EkaMoexStrategy.h"
#include "EkaMdRecvHandler.h"
void ekaFireReportThread(EkaDev *dev);
extern EkaDev *g_ekaDev;

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

OpResult setClOrdId(uint64_t cntr) {

  // 0x76000 - base
  //[3 +: 5] - pair id, NA for cntr
  //[8 +: 4] - conf id, ==4

  eka_write(g_ekaDev, 0x76400, cntr);

  return OPRESULT__OK;
}

/* ==================================================== */

OpResult setOrderPricePair(EkaBcsOrderType type,
                           PairIdx idx,
                           EkaBcsOrderSide side,
                           EkaBcsMoexPrice price) {

  //[3 +: 5] - pair id
  //[8 +: 4] - conf id
  // conf:
  // 0 -my buy
  // 1 -my sell
  // 2 -hedge buy
  // 3 -hedge sell
  uint64_t base_addr = 0x76000;
  base_addr |= (idx << 3); // Correct Pair
  switch (type) {
  case EkaBcsOrderType::MY_ORDER:
    if (side == EkaBcsOrderSide::BUY)
      eka_write(g_ekaDev, base_addr + 0x000, price);
    else
      eka_write(g_ekaDev, base_addr + 0x100, price);
    break;
  case EkaBcsOrderType::HEDGE_ORDER:
    if (side == EkaBcsOrderSide::BUY)
      eka_write(g_ekaDev, base_addr + 0x200, price);
    else
      eka_write(g_ekaDev, base_addr + 0x300, price);
    break;
  default:
    on_error("Unknown type %d", (int)type);
    break;
  }

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

OpResult hwEngInit(const HwEngInitCtx *hwEngInitCtx) {
  if (!g_ekaDev) {
    EKA_ERROR("!g_ekaDev");
    return OPRESULT__ERR_DEVICE_INIT;
  }
  EfcInitCtx initCtx = {
      .report_only = hwEngInitCtx->report_only,
      .watchdog_timeout_sec =
          hwEngInitCtx->watchdog_timeout_sec};

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

EkaActionIdx allocateNewAction(EkaBcsActionType type) {
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
  g_ekaDev->pEfcRunCtx->onEfcFireReportCb =
      pEkaBcsRunCtx->onReportCb;
  g_ekaDev->pEfcRunCtx->cbCtx = pEkaBcsRunCtx->cbCtx;
  /* ----------------------------------------------- */

  // TBD fire report loop
  //  EKA_LOG("Lounching "
  //          "EkaEurStrategy::fireReportThreadLoop()");
  //  auto fireReportLoopFunc =
  //      std::bind(&EkaEurStrategy::fireReportThreadLoop,
  //      eur,
  //                pEkaBcRunCtx);
  //  eur->fireReportLoopThr_ =
  //  std::thread(fireReportLoopFunc);
  //  EKA_LOG("EkaEurStrategy::fireReportThreadLoop() "
  //          "span off");
  //  fflush(g_ekaLogFile);

  /* ----------------------------------------------- */

  if (g_ekaDev->fireReportThreadActive) {
    on_error("fireReportThread already active");
  }

  g_ekaDev->fireReportThread =
      std::thread(ekaFireReportThread, g_ekaDev);
  g_ekaDev->fireReportThread.detach();
  while (!g_ekaDev->fireReportThreadActive)
    sleep(0);

  EKA_LOG("fireReportThread activated");

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

OpResult initProdPair(PairIdx idx,
                      const ProdPairInitParams *params) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  auto moex = efc->moex_;
  if (!moex)
    on_error("Moex is not initialized: use "
             "initMoexStrategy()");

  return moex->initPair(idx, params);
}
/* ==================================================== */

OpResult setProdPairDynamicParams(
    PairIdx idx, const ProdPairDynamicParams *params) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");
  auto efc = g_ekaDev->efc;

  auto moex = efc->moex_;
  if (!moex)
    on_error("Moex is not initialized: use "
             "initMoexStrategy()");

  return moex->setPairDynamicParams(idx, params);
}
/* ==================================================== */
/* ==================================================== */
OpResult ekaBcsArmMoex(bool arm, EkaBcsArmVer ver) {
  if (!g_ekaDev || !g_ekaDev->efc)
    on_error("HW Eng is not initialized: use hwEngInit()");

  auto efc = g_ekaDev->efc;
  efc->armMoex(arm, ver);
  return OPRESULT__OK;
}
/* ==================================================== */

/* ==================================================== */
OpResult ekaBcsResetReplaceCnt() {
  // 0x76000 - base
  //[3 +: 5] - pair id, NA for cntr
  //[8 +: 4] - conf id, ==5

  eka_write(g_ekaDev, 0x76500, 0x0);

  return OPRESULT__OK;
}
/* ==================================================== */

/* ==================================================== */
OpResult ekaBcsSetReplaceThr(uint32_t threshold) {
  // 0x76000 - base
  //[3 +: 5] - pair id, NA for cntr
  //[8 +: 4] - conf id, ==6

  eka_write(g_ekaDev, 0x76600, (uint64_t)threshold);

  return OPRESULT__OK;
}
/* ==================================================== */

} // End of namespace EkaBcs
