#include <algorithm>
#include <chrono>
#include <cstdio>
#include <errno.h>
#include <math.h>
#include <string>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <cstring>
#include <stdarg.h>
#include <thread>

#include "EkaBcs.h"

#include "testMacros.h"

#include "EkaFhBcsSbeDecoder.h"

using namespace EkaBcs;

static volatile bool g_keepWork = true;

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  g_keepWork = false;
  EKA_LOG("Ctrl-C detected: exitting...");
  fflush(stdout);
  return;
}

FILE *g_ekaLogFile = stdout;

int ekaDefaultLog(void *logFile /*unused*/,
                  const char *function, const char *file,
                  int line, int priority,
                  const char *format, ...) {
  va_list ap;
  const int rc1 = fprintf(
      g_ekaLogFile, "%s@%s:%u: ", function, file, line);
  va_start(ap, format);
  const int rc2 = vfprintf(g_ekaLogFile, format, ap);
  va_end(ap);
  const int rc3 = fprintf(g_ekaLogFile, "\n");
  return rc1 + rc2 + rc3;
}

static int printMdPkt(const void *md, size_t len,
                      void *ctx) {
  BcsSbe::printPkt(md);
  return 0;
}

static std::uint64_t getCurrentTime() {

  auto now = std::chrono::system_clock::now();

  auto duration = now.time_since_epoch();

  auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          duration);

  return nanoseconds.count();
}

EkaLogCallback g_ekaLogCB = ekaDefaultLog;

int main(int argc, char *argv[]) {
  const char *moexIp = "91.203.252.67";
  const uint16_t moexTcpPort = 9212;

  EkaBcLane moexLane = 1;

  signal(SIGINT, INThandler);
  // ==============================================

  if (openDev() != OPRESULT__OK)
    on_error("openDev() failed");

  // ==============================================
  // Basic HW params
  HwEngInitCtx initCtx = {.report_only = false,
                          .watchdog_timeout_sec = 1000000};

  if (hwEngInit(&initCtx) != OPRESULT__OK)
    on_error("hwEngInit() failed");

  // ==============================================
  // Configure feth1 I/F

  PortAttrs feth1Conf = {
      .host_ip = inet_addr("10.250.218.96"),
      .netmask = inet_addr("255.255.0.0"),
      .gateway = inet_addr(moexIp),
      .nexthop_mac = {0x30, 0xf7, 0x0d, 0x9c, 0x6b, 0x3c}};

  if (configurePort(moexLane, &feth1Conf) != OPRESULT__OK)
    on_error("configurePort() failed");

  // ==============================================
  // TCP
  auto sock = tcpConnect(moexLane, moexIp, moexTcpPort);
  if (sock < 0)
    on_error("tcpConnect() failed");

  // ==============================================
  // Preparing Establish Message

  struct TwimeEstablishMsg {
    BcsSbe::MsgHdr hdr;
    BcsSbe::UTCTimestamp_T SendingTime;
    uint16_t KeepAliveInterval;
    char Username[12];
    char Password[8];
  } __attribute__((packed));

  TwimeEstablishMsg establishMsg = {};

  establishMsg.hdr.blockLength =
      sizeof(establishMsg) - sizeof(establishMsg.hdr);
  establishMsg.hdr.templateId = 6; // Establish
  establishMsg.hdr.schemaId = 22343;
  establishMsg.hdr.version = 0;

  establishMsg.SendingTime = getCurrentTime();
  establishMsg.KeepAliveInterval = 10000;
  strncpy(establishMsg.Username, "ND025390676C",
          sizeof(establishMsg.Username));
  strncpy(establishMsg.Password, "password",
          sizeof(establishMsg.Password));

  // ==============================================
  // Preparing Terminate Message

  struct TwimeTerminateMsg {
    BcsSbe::MsgHdr hdr;
    BcsSbe::UTCTimestamp_T SendingTime;
    uint8_t TerminationCode;
  } __attribute__((packed));

  TwimeTerminateMsg terminateMsg = {};

  terminateMsg.hdr.blockLength =
      sizeof(terminateMsg) - sizeof(terminateMsg.hdr);
  terminateMsg.hdr.templateId = 4; // Terminate
  terminateMsg.hdr.schemaId = 22343;
  terminateMsg.hdr.version = 0;

  terminateMsg.SendingTime = getCurrentTime();
  terminateMsg.TerminationCode = 0; // Finished

  // ==============================================
  // Sending Establish
  EKA_LOG("Sending TwimeEstablishMsg");

  ssize_t rc;
  rc = tcpSend(sock, &establishMsg, sizeof(establishMsg));
  if (rc <= 0)
    on_error("tcpSend() failed");

  // ==============================================
  // Getting response
  BcsSbe::MsgHdr rxHdr = {};
  rc = tcpRecv(sock, &rxHdr, sizeof(rxHdr));
  if (rc <= 0)
    on_error("tcpRecv() failed");

  EKA_LOG("Received Msg %u", rxHdr.templateId);

  uint8_t payload[1536] = {};
  rc = tcpRecv(sock, payload, rxHdr.blockLength);
  if (rc <= 0)
    on_error("tcpRecv() failed");

  // ==============================================
  // Sending Terminate
  EKA_LOG("Sending TwimeEstablishMsg");

  rc = tcpSend(sock, &terminateMsg, sizeof(terminateMsg));
  if (rc <= 0)
    on_error("tcpSend() failed");

  // ==============================================
  // Getting response
  rc = tcpRecv(sock, &rxHdr, sizeof(rxHdr));
  if (rc <= 0)
    on_error("tcpRecv() failed");

  EKA_LOG("Received Msg %u", rxHdr.templateId);

  rc = tcpRecv(sock, payload, rxHdr.blockLength);
  if (rc <= 0)
    on_error("tcpRecv() failed");

  // ==============================================

  closeSock(sock);

  closeDev();

  return 0;
}
