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
  BcsSbeDecoder::printPkt(md);
  return 0;
}

EkaLogCallback g_ekaLogCB = ekaDefaultLog;

int main(int argc, char *argv[]) {

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
  // MdRcvParams

  static const McGroupParams feedA[] = {
      //{0, "239.195.1.24", 16024},
      {0, "239.195.1.16", 16016}};
  static const UdpMcParams mcParamsA = {feedA,
                                        std::size(feedA)};
  if (configureRcvMd_A(&mcParamsA, printMdPkt, stdout) !=
      OPRESULT__OK)
    on_error("setMdRcvParams() failed");

  std::thread rcvA(startRcvMd_A);

  while (g_keepWork)
    std::this_thread::yield();

  stopRcvMd_A();

  rcvA.join();

  closeDev();

  return 0;
}
