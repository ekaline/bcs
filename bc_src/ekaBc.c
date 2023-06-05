#include "eka_macros.h"

#include "Efc.h"
#include "EkaBc.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpTxSess.h"

EkaDev *ekaBcOpenDev() {
  EkaDevInitCtx initCtx = {};
  g_ekaDev = EkaDev(&initCtx);
  return g_ekaDev;
}

int ekaBcCloseDev(EkaDev *pEkaDev) {
  if (!pEkaDev)
    return -1;
  delete pEkaDev;
  g_ekaDev = NULL;

  return 0;
}

int ekaBcTcpConnect(EkaDev *dev, int coreId, const char *ip,
                    uint16_t port) {
  if (!dev->checkAndSetEpmTx())
    on_error(
        "TX functionality is not available for this "
        "Ekaline SW instance - caught by another process");

  if (!dev->core[coreId])
    on_error("Lane %u has no link or IP address", coreId);

  if (!dev->core[coreId]->pLwipNetIf)
    dev->core[coreId]->initTcp();

  EkaCore *const core = getEkaCore(dev, coreId);
  if (!core)
    return -1;

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
  dst.sin_port = htons(port);

  sess->dstIp = ((sockaddr_in *)dst)->sin_addr.s_addr;
  sess->dstPort = be16toh(((sockaddr_in *)dst)->sin_port);

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
  if (!s)
    return -1;

  return s->sendPayload(s->sessId /* thrId */, buf, size,
                        0);
}

ssize_t ekaBcRecv(EkaDev *dev, int sock, void *buf,
                  size_t size) {
  EkaTcpSess *const s = dev->findTcpSess(sock);
  if (!s)
    return -1;
  return s->recv(buf, size, 0);
}

int ekaBcCloseSock(EkaDev *dev, int sock) {
  EkaTcpSess *s = dev->findTcpSess(sock);
  if (!s)
    return -1;

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
  const EpmStrategyParams params = {
      .numActions = 0,
      .triggerParams = params->triggerParams,
      .numTriggers = params->numTriggers};

  auto rc = epmInitStrategies(dev, &params, 1);
  if (rc != EkaOpResult::EKA_OPRESULT__OK)
    return -1;
  return 0;
}

int ekaBcCmeFcGlobalInit(
    EkaDev *dev,
    const struct EkaBcCmeFastCanceGlobalParams *params) {
  EfcCtx efcCtx = {.dev = dev};
  EfcStratGlobCtx params = {
      .report_only = params->report_only,
      .watchdog_timeout_sec = watchdog_timeout_sec};

  efcInitStrategy(&efcCtx, &params);
  return 0;
}

static const uint64_t DefaultToken = 0x1122334455667788;

int ekaBcCmeFcAlgoInit(
    EkaDev *dev,
    const struct EkaBcCmeFcAlgoParams *params) {
  if (!dev)
    on_error("! dev");
  if (!params)
    on_error("! params");

  volatile EfcCmeFastCancelStrategyConf conf = {
      //      .pad            = {},
      .minNoMDEntries = params->minNoMDEntries,
      .maxMsgSize = params->maxMsgSize,
      .token = DefaultToken,
      .fireActionId = (uint16_t)params->fireActionId,
      .strategyId = (uint8_t)EFC_STRATEGY};

  EKA_LOG("Configuring Cme Fast Cancel FPGA: "
          "minNoMDEntries=%d,maxMsgSize=%u,"
          "fireActionId=%d",
          conf.minNoMDEntries, conf.maxMsgSize,
          conf.fireActionId);
  //  hexDump("EfcCmeFastCancelStrategyConf",&conf,
  //          sizeof(conf),stderr);
  copyBuf2Hw(dev, 0x84000, (uint64_t *)&conf, sizeof(conf));
  return 0;
}
