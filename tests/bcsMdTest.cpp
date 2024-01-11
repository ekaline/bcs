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

#include "EkaBcs.h"

#include "testMacros.h"

#include "EkaFhBcsSbeDecoder.h"

static volatile bool g_keepWork = true;

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  g_keepWork = false;
  EKA_LOG("Ctrl-C detected: exitting...");
  fflush(stdout);
  return;
}

using namespace EkaBcs;

static void printMdPkt(const void *md, size_t len,
                       void *ctx) {
  BcsSbeDecoder::printPkt(md);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, INThandler);
  // ==============================================
  // NW Config
  static const McGroupParams mc0[] = {0, "224.0.0.0",
                                      30300};
  static const UdpMcParams core0_1mc = {mc0,
                                        std::size(mc0)};

  const char *laneIpAddr[] = {"1.1.1.1", "2.2.2.2",
                              "3.3.3.3", "4.4.4.4"};
  const char *laneGwAddr[] = {"1.1.1.1", "2.2.2.2",
                              "3.3.3.3", "4.4.4.4"};
  // ==============================================

  if (openDev() != OPRESULT__OK)
    on_error("openDev() failed");

  // ==============================================
  // Configuring Physical Port
  EkaBcLane lane = 0;
  EKA_LOG("Initializing FPGA lane %d to %s", lane,
          laneIpAddr[lane]);

  const PortAttrs laneAttr = {
      .host_ip = inet_addr(laneIpAddr[lane]),
      .netmask = inet_addr("255.255.255.0"),
      .gateway = inet_addr(laneGwAddr[lane]),
      .nexthop_mac = {}, // resolved by our internal ARP
      .src_mac_addr = {} // taken from system config
  };
  if (configurePort(lane, &laneAttr) != OPRESULT__OK)
    on_error("configurePort() failed");
  // ==============================================
  // Basic HW params
  HwEngInitCtx initCtx = {.report_only = false,
                          .watchdog_timeout_sec = 1000000};

  if (hwEngInit(&initCtx) != OPRESULT__OK)
    on_error("hwEngInit() failed");

  // ==============================================
  // Strategy params

  return 0;
}
