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
FILE *g_ekaLogFile = stdout;

static volatile bool g_keepWork = true;

void getExampleFireReport(const void *p, size_t len,
                          void *ctx) {
  //  printf ("Some report came\n");
}


static void setUp() {

  char testLogFileName[256] = {};
  sprintf(testLogFileName, "test_logs/testLog.log");

  if ((g_ekaLogFile = fopen(testLogFileName, "w")) ==
      nullptr)
    on_error("Failed to open %s file for writing",
             testLogFileName);
  //  g_ekaLogCB = ekaDefaultLog;

#ifdef _VERILOG_SIM
  char verilogSimFileName[256] = {};
  sprintf(verilogSimFileName, "test_logs/verilogLog.log");

  if ((g_ekaVerilogSimFile =
           fopen(verilogSimFileName, "w")) == nullptr)
    on_error("Failed to open %s file for writing",
             verilogSimFileName);
#endif
  //  TMP PATCH !!!
  // g_ekaLogFile = stdout;

  return;
}


void INThandler(int sig) {
  signal(sig, SIG_IGN);
  g_keepWork = false;
  EKA_LOG("Ctrl-C detected: exitting...");
  fflush(stdout);
  return;
}


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

EkaLogCallback g_ekaLogCB = ekaDefaultLog;

int main(int argc, char *argv[]) {

  setUp();
  signal(SIGINT, INThandler);
  // ==============================================

  if (openDev() != OPRESULT__OK)
    on_error("openDev() failed");

  // ==============================================
  // Basic HW params
  HwEngInitCtx initCtx = {.report_only = true,
                          .watchdog_timeout_sec = 1000000};

  if (hwEngInit(&initCtx) != OPRESULT__OK)
    on_error("hwEngInit() failed");

  // ==============================================
  // MdRcvParams

  static const McGroupParams feedA[] = {
      {2, "239.195.1.16", 16016}};
  static const UdpMcParams mcParamsA = {feedA,
                                        std::size(feedA)};
  //sw
  // if (configureRcvMd_A(&mcParamsA, printMdPkt, stdout) !=
  //     OPRESULT__OK)
  //   on_error("setMdRcvParams() failed");

  // std::thread rcvA(startRcvMd_A);

  // ==============================================
  // Product List
  MoexSecurityId prodList_[16] = {};

  prodList_[0] = MoexSecurityId("EURRUB_TMS  ");
  prodList_[1] = MoexSecurityId("USDCNY_TOD  ");

  //hw (TBD check IGMP)
  initMoexStrategy(&mcParamsA);

  // Actions
  auto fireNewActionIdx = allocateNewAction(EkaBcsActionType::MoexFireNew);
  setActionTcpSock (fireNewActionIdx,(EkaSock)NULL);
  setActionNext (fireNewActionIdx,EKA_BC_LAST_ACTION);

  auto fireReplaceActionIdx = allocateNewAction(EkaBcsActionType::MoexFireReplace);
  setActionTcpSock (fireReplaceActionIdx,(EkaSock)NULL);
  setActionNext (fireReplaceActionIdx,EKA_BC_LAST_ACTION);

  const char fireNewMsg[] =
    "MOEX dummy pkt a b c d e f g h"
    "NEW";
  setActionPayload(fireNewActionIdx,&fireNewMsg,strlen(fireNewMsg));

  const char fireReplaceMsg[] =
    "MOEX dummy pkt a b c d e f g h"
    "Replace";
  setActionPayload(fireReplaceActionIdx,&fireReplaceMsg,strlen(fireReplaceMsg));

  // Static Product
  ProdPairInitParams prodPairInitParams;
  prodPairInitParams.secBase = prodList_[0];
  prodPairInitParams.secQuote = prodList_[1];
  prodPairInitParams.fireBaseNewIdx = fireNewActionIdx;
  prodPairInitParams.fireQuoteReplaceIdx = fireReplaceActionIdx;
  auto ret = initProdPair(0, &prodPairInitParams);


  EkaBcsRunCtx runCtx = {.onReportCb = getExampleFireReport,
    .cbCtx = NULL};
  EkaBcsMoexRun(&runCtx);


  //base 2500@97145000000 : 90000@97152500000
  //quot  100@ 7194400000 :    20@ 7207400000

  ProdPairDynamicParams prodPairDynamicParams;
  prodPairDynamicParams.markupBuy     = 0x2;
  prodPairDynamicParams.markupSell    = 0x3;
  prodPairDynamicParams.fixSpread     = 0x4;
  prodPairDynamicParams.tolerance     = 0x5;
  prodPairDynamicParams.quoteSize     = 0x6;
  prodPairDynamicParams.timeTolerance = 0x7;
  ret = setProdPairDynamicParams(0, &prodPairDynamicParams);

  EKA_LOG("Test Before Loop");

  // ClOrdID
  setClOrdId(123);
  ekaBcsResetReplaceCnt();
  ekaBcsSetReplaceThr(100);

  // Set SW order
  setOrderPricePair(EkaBcsOrderType::MY_ORDER,0,EkaBcsOrderSide::BUY,0x444);
  setOrderPricePair(EkaBcsOrderType::MY_ORDER,0,EkaBcsOrderSide::SELL,0x555);
  setOrderPricePair(EkaBcsOrderType::HEDGE_ORDER,0,EkaBcsOrderSide::BUY,0x666);
  setOrderPricePair(EkaBcsOrderType::HEDGE_ORDER,0,EkaBcsOrderSide::SELL,0x777);

  ekaBcsArmMoex(true,0);

#ifndef _VERILOG_SIM
  while (g_keepWork)
    std::this_thread::yield();

  //  stopRcvMd_A();
  //  rcvA.join();

  closeDev();
#endif

  return 0;
}
