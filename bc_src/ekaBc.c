#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

extern EkaDev *g_ekaDev;

EkaDev *
ekaBcOpenDev(const EkaBcAffinityConfig *affinityConf) {
  EkaDevInitCtx initCtx = {};
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
EkaBCOpResult excSetBlocking(EkaDev *dev, EkaBcSock hSock,
                             bool blocking) {
  if (!checkDevice(dev))
    return -1;
  else if (EkaTcpSess *const s = dev->findTcpSess(
               static_cast<ExcSocketHandle>(hSock)))
    return s->setBlocking(blocking);
  EKA_WARN("ExcSocketHandle %d not found", hSock);
  errno = EBADF;
  return -1;
}
/* ==================================================== */

ssize_t ekaBcSend(EkaDev *pEkaDev, EkaBcSock ekaSock,
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

ssize_t ekaBcAppSend(EkaDev *pEkaDev,
                     EkaBcActionIdx actionIdx,
                     const void *buffer, size_t size) {
  if (!dev || !dev->epm)
    on_error("!dev or Epm is not initialized");

  if (!dev->epm->a_[actionIdx])
    on_error("Acion[%d] is not set", actionIdx);

  return dev->epm->a_[actionId]->fastSend(buffer, size);
}
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

  return EKA_OPRESULT__OK;
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

  return 0;
}
/* ==================================================== */

EkaBCOpResult ekaBcInit(EkaDev *ekaDev,
                        const EkaBcInitCtx *ekaBcInitCtx) {
  EfcCtx efcCtx = {};
  EfcCtx *pEfcCtx = &efcCtx;

  EfcInitCtx initCtx = {
      .report_only = ekaBcInitCtx->report_only,
      .watchdog_timeout_sec =
          ekaBcInitCtx->watchdog_timeout_sec};

  return efcInit(&pEfcCtx, dev, &initCtx);
}

/* ==================================================== */

void ekaBcSwKeepAliveSend(EkaDev *dev) {
  EfcCtx efcCtx = {.dev = dev};
  efcSwKeepAliveSend(&efcCtx);
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
ekaBcAllocateNewAction(EkaDev *pEkaDev,
                       EkaBcActionType type) {
  return efcAllocateNewAction(
      dev, static_cast<EpmActionType>(type));
}

/* ==================================================== */

EkaBCOpResult
ekaBcSetActionPayload(EkaDev *ekaDev,
                      EkaBcActionIdx actionIdx,
                      const void *payload, size_t len) {

  return efcSetActionPayload(dev, actionIdx, payload, len);
}
/* ==================================================== */

void ekaBcEnableController(EkaDev *dev, bool enable,
                           uint32_t armVer) {
  EfcCtx efcCtx = {.dev = dev};
  efcEnableController(&efcCtx, enable ? 1 : -1, armVer);
}
/* ==================================================== */

void ekaBcFcRun(EkaDev *dev,
                struct EkaBcFcRunCtx *pEkaBcFcRunCtx) {
  struct EfcRunCtx efcRunCtx = {
      .onEfcFireReportCb = pEkaBcFcRunCtx->onReportCb};
  EfcCtx efcCtx = {.dev = dev};
  efcRun(&efcCtx, &efcRunCtx);
}
/* ==================================================== */

void ekaBcRun(EkaDev *dev,
              struct EkaBcRunCtx *pEkaBcRunCtx) {
  struct EfcRunCtx efcRunCtx = {
      .onEfcFireReportCb = pEkaBcRunCtx->onReportCb};
  EfcCtx efcCtx = {.dev = dev};
  efcRun(&efcCtx, &efcRunCtx);
}

/* ==================================================== */

EkaBCOpResult
ekaBcSetActionTcpSock(EkaDev *ekaDev,
                      EkaBcActionIdx actionIdx,
                      EkaBcSock sock) {
  return setActionTcpSock(ekaDev, actionIdx, sock);
}
/* ==================================================== */

EkaBCOpResult
ekaBcSetActionNext(EkaDev *ekaDev, EkaBcActionIdx actionIdx,
                   EkaBcActionIdx nextActionIdx) {
  return setActionNext(ekaDev, actionIdx, nextActionIdx);
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
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->initEur(
      reinerpret_cast<const EfcUdpMcParams *>(mcParams));

  return EKABC_OPRESULT__OK;
}
/* ==================================================== */
//!!!
void ekaBcInitEurStrategyDone(EkaDev *dev) {
  ekaDownloadConf2hw(dev);
  eka_join_mc_groups(dev);
  sleep(1);               // to allow book to get filled
  eka_read(dev, 0xf0728); // Clearing Interrupts
  eka_read(dev, 0xf0798); // Clearing Interrupts

  //------------------------------------

  runBook(dev->exch->fireLogic);
}

/* ==================================================== */

EkaBCOpResult ekaBcInitCmeFcStrategy(
    EkaDev *pEkaDev, const EkaBcUdpMcParams *mcParams,
    const EkaBcCmeFcAlgoParams *cmeFcParams) {

  if (!dev || !dev->efc)
    on_error("Efc is not initialized: use efcInit()");
  auto efc = dev->efc;

  efc->initBcCmeFc(
      reinerpret_cast<const EfcUdpMcParams *>(mcParams),
      cmeFcParams);

  return EKABC_OPRESULT__OK;
}
