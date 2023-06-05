#ifndef _EKA_BC_H_
#define _EKA_BC_H_

extern "C" {
struct EkaDev;

EkaDev *ekaBcOpenDev();
int ekaBcCloseDev(EkaDev *pEkaDev);

int ekaBcTcpConnect(EkaDev *pEkaDev, int lane,
                    const char *ip, uint16_t port);

ssize_t ekaBcSend(EkaDev *pEkaDev, int sock,
                  const void *buf, size_t size);

ssize_t ekaBcRecv(EkaDev *pEkaDev, int sock, void *buf,
                  size_t size);

int ekaBcCloseSock(EkaDev *pEkaDev, int sock);

int ekaBcFcInit(EkaDev *pEkaDev);

struct EkaBcTriggerParams {
  int8_t lane;      ///< 10G port to receive UDP MC trigger
  const char *mcIp; ///< MC IP address
  uint16_t mcUdpPort; ///< MC UDP Port
};

struct EkaBcFcMdParams {
  ///< list of triggers
  ///< belonging to this strategy
  const EkaBcTriggerParams *triggerParams;
  size_t numTriggers;
};

int ekaBcCmeFcMdInit(
    EkaDev *pEkaDev,
    const struct EkaBcFcMdParams *params);

struct EkaBcCmeFastCanceGlobalParams {
    uint8_t     report_only;
    uint64_t    watchdog_timeout_sec;
};

int ekaBcCmeFcGlobalInit(
    EkaDev *pEkaDev,
    const struct EkaBcCmeFastCanceGlobalParams *params);

struct EkaBcCmeFcAlgoParams {
  int fireActionId; ///< 1st Action fired
  uint16_t maxMsgSize;
  uint8_t minNoMDEntries;
};

int ekaBcCmeFcAlgoInit(
    EkaDev *pEkaDev,
    const struct EkaBcCmeFcAlgoParams *params);

typedef void (*onEkaBcFcReportCb)(const void *report,
                                  size_t len, void *ctx);

struct EkaBcFcRunCtx {
  onEkaBcFcReportCb OnReportCb;
  void *cbCtx; ///< optional opaque field looped back to CB
};

void ekaBcFcRun(EkaDev *pEkaDev,
                struct EkaBcFcRunCtx *pEkaBcFcRunCtx);

ssize_t ekaBcCmeSendHB(EkaDev *pEkaDev, int sock,
                       const void *buffer, size_t size);

void ekaBcCmeSetILinkAppseq(EkaDev *ekaDev, int sock,
                            int32_t appSequence);

struct EkaBcAction {
  int sock;       ///< TCP connection
  int nextAction; ///< Next action in sequence
};

int ekaBcAllocateCmeFireAction(EkaDev *pEkaDev);

int ekaBcSetAction(EkaDev *pEkaDev, int actionIdx,
                   const EkaBcAction *ekaBcAction);

int ekaBcSetActionPayload(EkaDev *ekaDev, int actionIdx,
                          const void *payload, size_t len);

} // End of extern "C"
#endif
