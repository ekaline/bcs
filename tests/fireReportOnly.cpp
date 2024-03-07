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

void printFireReport(uint8_t *p) {
  uint8_t *b = (uint8_t *)p;

  auto reportHdr = reinterpret_cast<EkaBcReportHdr *>(b);
  printf("reportHdr->idx = %d\n", reportHdr->idx);

  b += sizeof(*reportHdr);
}

void printPayloadReport(uint8_t *p) {}

void getExampleFireReport(const void *p, size_t len,
                          void *ctx) {
  uint8_t *b = (uint8_t *)p;
  auto containerHdr{
      reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};

  switch (containerHdr->eventType) {
  case EkaBcEventType::FireEvent:
    printf("Fire with %d reports...\n",
           containerHdr->nReports);
    break;
    // skip container header
    b += sizeof(*containerHdr);
    // print fire report
    printFireReport(b);
    // skip report hdr (of fire) and fire report
    b += sizeof(EkaBcReportHdr);
    b += sizeof(EkaBcsFireReport);
    // print payload
    printPayloadReport(b);
    break;
  case EkaBcEventType::ExceptionEvent:
    //    printf ("Status...\n");
    break;

  default:
    break;
  }
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

static int dummyProcessPkt(const void *md, size_t len,
                           void *ctx) {
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
      {0, "239.195.1.16", 16016}};
  static const UdpMcParams mcParamsA = {feedA,
                                        std::size(feedA)};
  // sw
  if (configureRcvMd_A(&mcParamsA, dummyProcessPkt,
                       stdout) != OPRESULT__OK)
    on_error("setMdRcvParams() failed");

  std::thread rcvA(startRcvMd_A);

  // ==============================================
  // Product List
  MoexSecurityId prodList_[16] = {};

  prodList_[0] = MoexSecurityId("EURRUB_TMS  ");
  prodList_[1] = MoexSecurityId("USDCNY_TOD  ");

  // hw (TBD check IGMP)
  initMoexStrategy(&mcParamsA);

  // Actions
  auto fireNewActionIdx =
      allocateNewAction(EkaBcsActionType::MoexFireNew);
  setActionTcpSock(fireNewActionIdx, EkaDummySock);
  setActionNext(fireNewActionIdx, CHAIN_LAST_ACTION);

  auto fireReplaceActionIdx =
      allocateNewAction(EkaBcsActionType::MoexFireReplace);
  setActionTcpSock(fireReplaceActionIdx, EkaDummySock);
  setActionNext(fireReplaceActionIdx, CHAIN_LAST_ACTION);

  const char fireNewMsg[] = "MOEX dummy pkt a b c d e f g h"
                            "NEW";
  setActionPayload(fireNewActionIdx, &fireNewMsg,
                   strlen(fireNewMsg));

  const char fireReplaceMsg[] =
      "MOEX dummy pkt a b c d e f g h"
      "Replace";
  setActionPayload(fireReplaceActionIdx, &fireReplaceMsg,
                   strlen(fireReplaceMsg));

  // Static Product
  ProdPairInitParams prodPairInitParams = {};
  prodPairInitParams.secBase = prodList_[0];
  prodPairInitParams.secQuote = prodList_[1];
  prodPairInitParams.fireBaseNewIdx = fireNewActionIdx;
  prodPairInitParams.fireQuoteReplaceIdx =
      fireReplaceActionIdx;
  auto ret = initProdPair(0, &prodPairInitParams);

  EkaBcsRunCtx runCtx = {.onReportCb = getExampleFireReport,
                         .cbCtx = NULL};
  EkaBcsMoexRun(&runCtx);

  // base 2500@97145000000 : 90000@97152500000
  // quot  100@ 7194400000 :    20@ 7207400000

  ProdPairDynamicParams prodPairDynamicParams;
  prodPairDynamicParams.markupBuy = 0x2;
  prodPairDynamicParams.markupSell = 0x3;
  prodPairDynamicParams.fixSpread = 0x4;
  prodPairDynamicParams.tolerance = 0x5;
  prodPairDynamicParams.quoteSize = 0x6;
  prodPairDynamicParams.timeTolerance = 0x7;
  ret = setProdPairDynamicParams(0, &prodPairDynamicParams);

  EKA_LOG("Test Before Loop");

  // ClOrdID
  setClOrdId(123);
  ekaBcsResetReplaceCnt();
  ekaBcsSetReplaceThr(100);

  // Set SW order
  setOrderPricePair(EkaBcsOrderType::MY_ORDER, 0,
                    EkaBcsOrderSide::BUY, 444);
  setOrderPricePair(EkaBcsOrderType::MY_ORDER, 0,
                    EkaBcsOrderSide::SELL, 555);
  setOrderPricePair(EkaBcsOrderType::HEDGE_ORDER, 0,
                    EkaBcsOrderSide::BUY, 666);
  setOrderPricePair(EkaBcsOrderType::HEDGE_ORDER, 0,
                    EkaBcsOrderSide::SELL, 777);

  ekaBcsArmMoex(true, 0);

  while (g_keepWork)
    std::this_thread::yield();

  stopRcvMd_A();
  rcvA.join();

  closeDev();

  return 0;
}
