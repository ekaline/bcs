#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "eka_macros.h"

#include "Efc.h"
#include "EfcCme.h"
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

int ekaBcCloseDev(EkaDev *dev) {
  if (!dev)
    return -1;
  delete dev;
  g_ekaDev = NULL;

  return 0;
}

int ekaBcTcpConnect(EkaDev *dev, int8_t coreId,
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

ssize_t ekaBcSend(EkaDev *dev, int sock, const void *buf,
                  size_t size) {
  EkaTcpSess *const s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  return s->sendPayload(s->sessId /* thrId */, (void *)buf,
                        size, 0);
}

ssize_t ekaBcCmeSendHB(EkaDev *dev, int sock,
                       const void *buffer, size_t size) {
  auto s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  return efcCmeSend(dev, s->getConnHandle(), buffer, size,
                    0, false);
}

ssize_t ekaBcRecv(EkaDev *dev, int sock, void *buf,
                  size_t size) {
  auto s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  return s->recv(buf, size, 0);
}

int ekaBcCmeSetILinkAppseq(EkaDev *dev, int sock,
                           int32_t appSequence) {
  auto s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  efcCmeSetILinkAppseq(dev, s->getConnHandle(),
                       appSequence);
  return 0;
}

int ekaBcCloseSock(EkaDev *dev, int sock) {
  auto s = dev->findTcpSess(sock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", sock);
    return -1;
  }
  dev->core[s->coreId]->tcpSess[s->sessId] = nullptr;
  delete s;

  return 0;
}

int ekaBcFcInit(EkaDev *dev) {
  EfcCtx efcCtx = {};
  EfcCtx *pEfcCtx = &efcCtx;

  EfcInitCtx initCtx = {.feedVer = EfhFeedVer::kCME};
  auto rc = efcInit(&pEfcCtx, dev, &initCtx);
  if (rc != EkaOpResult::EKA_OPRESULT__OK)
    return -1;
  return 0;
}

int ekaBcCmeFcMdInit(EkaDev *dev,
                     const struct EkaBcFcMdParams *params) {
  const EpmStrategyParams epmStrategyParams = {
      .numActions = 256,
      // EkaEpmRegion::getMaxActions(EkaEpmRegion::Regions::Efc),
      .triggerParams =
          (const EpmTriggerParams *)params->triggerParams,
      .numTriggers = params->numTriggers};

  auto rc = epmInitStrategies(dev, &epmStrategyParams, 1);
  if (rc != EkaOpResult::EKA_OPRESULT__OK)
    return -1;
  return 0;
}

int ekaBcCmeFcGlobalInit(
    EkaDev *dev,
    const struct EkaBcCmeFastCanceGlobalParams *params) {
  EfcCtx efcCtx = {.dev = dev};
  EfcStratGlobCtx efcStratGlobCtx = {
      .report_only = params->report_only,
      .watchdog_timeout_sec = params->watchdog_timeout_sec};

  efcInitStrategy(&efcCtx, &efcStratGlobCtx);
  return 0;
}

void ekaBcSwKeepAliveSend(EkaDev *dev) {
  EfcCtx efcCtx = {.dev = dev};
  efcSwKeepAliveSend(&efcCtx);
}

static const uint64_t DefaultToken = 0x1122334455667788;

int ekaBcCmeFcAlgoInit(
    EkaDev *dev,
    const struct EkaBcCmeFcAlgoParams *params) {
  if (!dev)
    on_error("! dev");
  if (!params)
    on_error("! params");

  volatile EfcBCCmeFastCancelStrategyConf conf = {
      //      .pad            = {},
      .minNoMDEntries = params->minNoMDEntries,
      .minTimeDiff = params->minTimeDiff,
      .token = DefaultToken,
      .fireActionId = (uint16_t)params->fireActionId,
      .strategyId = (uint8_t)EFC_STRATEGY};

  EKA_LOG("Configuring Cme Fast Cancel FPGA: "
          "minNoMDEntries=%d,minTimeDiff=%ju,"
          "fireActionId=%d",
          conf.minNoMDEntries, conf.minTimeDiff,
          conf.fireActionId);
  //  hexDump("EfcCmeFastCancelStrategyConf",&conf,
  //          sizeof(conf),stderr);
  copyBuf2Hw(dev, 0x84000, (uint64_t *)&conf, sizeof(conf));
  return 0;
}

void ekaBcConfigurePort(
    EkaDev *dev, int8_t lane,
    const struct EkaBcPortAttrs *pPortAttrs) {
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

/// @brief
/// @param dev
/// @return
int ekaBcAllocateFcAction(EkaDev *dev) {
  return efcAllocateNewAction(dev,
                              EpmActionType::CmeHwCancel);
}

int ekaBcSetActionParams(
    EkaDev *dev, int actionIdx,
    const struct EkaBcActionParams *params) {

  auto s = dev->findTcpSess(params->tcpSock);
  if (!s) {
    EKA_WARN("TCP sock %d not found", params->tcpSock);
    return -1;
  }

  EfcAction genericActionParams = {
      .type = EpmActionType::CmeHwCancel,
      .token = DefaultToken,
      .hConn = s->getConnHandle(),
      .actionFlags = AF_Valid,
      .nextAction = params->nextAction,
      .enable = EkaEpm::ALWAYS_ENABLE,
      .postLocalMask = EkaEpm::ALWAYS_ENABLE,
      .postStratMask = EkaEpm::ALWAYS_ENABLE,
      .user = 0};

  auto rc =
      efcSetAction(dev, actionIdx, &genericActionParams);
  if (rc != EKA_OPRESULT__OK)
    return -1;
  return 0;
}

int ekaBcSetActionPayload(EkaDev *dev, int actionIdx,
                          const void *payload, size_t len) {
  auto rc =
      efcSetActionPayload(dev, actionIdx, payload, len);
  if (rc != EKA_OPRESULT__OK)
    return -1;
  return 0;
}

void ekaBcEnableController(EkaDev *dev, bool enable,
                           uint32_t armVer) {
  EfcCtx efcCtx = {.dev = dev};
  efcEnableController(&efcCtx, enable ? 1 : -1, armVer);
}

void ekaBcFcRun(EkaDev *dev,
                struct EkaBcFcRunCtx *pEkaBcFcRunCtx) {
  struct EfcRunCtx efcRunCtx = {
      .onEfcFireReportCb = pEkaBcFcRunCtx->onReportCb};
  EfcCtx efcCtx = {.dev = dev};
  efcRun(&efcCtx, &efcRunCtx);
}
