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
#include <filesystem>

#include "EkaBcs.h"

#include "testMacros.h"

#include "EkaFhBcsSbeDecoder.h"

using namespace EkaBcs;
FILE *g_ekaLogFile = stdout;

static volatile bool g_keepWork = true;

void printFireReport(uint8_t *p) {
  uint8_t *b = (uint8_t *)p;

  auto reportHdr = reinterpret_cast<ReportHdr *>(b);
  printf("reportHdr->idx = %d\n", reportHdr->idx);

  b += sizeof(*reportHdr);
  auto hwReport{
      reinterpret_cast<const FireReport *>(b)};
  printf ("\n---- Action Params ----\n");
  printf ("currentActionIdx = %ju\n",(uint64_t)hwReport->currentActionIdx);
  printf ("firstActionIdx = %ju\n",(uint64_t)hwReport->firstActionIdx);

  printf ("\n---- Fire Params ----\n");
  printf ("StratType = %ju\n",(uint64_t)hwReport->moexFireReport.StratType);
  printf ("PairID = %ju\n",(uint64_t)hwReport->moexFireReport.PairID);
  printf ("MDSecID = %ju\n",(uint64_t)hwReport->moexFireReport.MDSecID);
  printf ("MyOrderBuyPrice = %ju\n",(uint64_t)hwReport->moexFireReport.MyOrderBuyPrice);
  printf ("MyOrderSellPrice = %ju\n",(uint64_t)hwReport->moexFireReport.MyOrderSellPrice);
  printf ("MDBidPrice = %ju\n",(uint64_t)hwReport->moexFireReport.MDBidPrice);
  printf ("MDAskPrice = %ju\n",(uint64_t)hwReport->moexFireReport.MDAskPrice);
  printf ("GoodPrice = %ju\n",(uint64_t)hwReport->moexFireReport.GoodPrice);
  printf ("Delta = %ju\n",(uint64_t)hwReport->moexFireReport.Delta);
  printf ("OrderUpdateTime = %ju\n",(uint64_t)hwReport->moexFireReport.OrderUpdateTime);
  printf ("RTCounterInternal = %ju\n",(uint64_t)hwReport->moexFireReport.RTCounterInternal);
  printf ("ReplaceOrigClOrdID = %ju\n",(uint64_t)hwReport->moexFireReport.ReplaceOrigClOrdID);
  
}

void printPayloadReport(uint8_t *p) {
  uint8_t *b = (uint8_t *)p;
  auto firePktHdr{reinterpret_cast<ReportHdr *>(b)};

  auto length = firePktHdr->size;
  
  printf ("Length = %d, Type = %s \n",length,ReportType2STR((EkaEfcBcReportType)firePktHdr->type));
  
  b += sizeof(ReportHdr);
  b += 54; //skip l2-l3 headers

  hexDump("Data (without headers)",b,length-54);
}

void getExampleFireReport(const void *p, size_t len,
                          void *ctx) {
  uint8_t *b = (uint8_t *)p;
  auto containerHdr{
      reinterpret_cast<ContainerGlobalHdr *>(b)};

  switch (containerHdr->eventType) {
  case EventType::FireEvent:
    printf("Fire with %d reports...\n",
           containerHdr->nReports);
    // skip container header
    b += sizeof(*containerHdr);
    // print fire report
    printFireReport(b);
    // skip report hdr (of fire) and fire report
    b += sizeof(ReportHdr);
    b += sizeof(FireReport);
    // print payload
    printPayloadReport(b);
    break;
  case EventType::ExceptionEvent:
    //    printf ("Status...\n");
    break;

  default:
    break;
  }
}

static void openLogFiles(const char *name) {
  std::string fullPath = name;
  std::string filename =
      std::filesystem::path(fullPath).filename();

  std::string dirPath = "./test_log";

  if (!std::filesystem::exists(dirPath))
    if (!std::filesystem::create_directory(dirPath))
      on_error("Failed creating directory: %s",
               dirPath.c_str());

  char testLogFileName[256] = {};
  sprintf(testLogFileName, "%s/%s.log", dirPath.c_str(),
          filename.c_str());

  if ((g_ekaLogFile = fopen(testLogFileName, "w")) ==
      nullptr)
    on_error("Failed to open %s file for writing",
             testLogFileName);

#ifdef _VERILOG_SIM
  char verilogSimFileName[256] = {};
  sprintf(verilogSimFileName, "%s/verilogLog.log",
          dirPath.c_str());

  if ((g_ekaVerilogSimFile =
           fopen(verilogSimFileName, "w")) == nullptr)
    on_error("Failed to open %s file for writing",
             verilogSimFileName);
#endif

  return;
}

static void closeLogFiles() {
  fclose(g_ekaLogFile);
#ifdef _VERILOG_SIM
  fclose(g_ekaVerilogSimFile);
#endif
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
  int rc1, rc2, rc3;

  // Log to the file
  if (g_ekaLogFile) {
    rc1 = fprintf(g_ekaLogFile, "%s@%s:%u: ", function,
                  file, line);
    va_start(ap, format);
    rc2 = vfprintf(g_ekaLogFile, format, ap);
    va_end(ap);
    rc3 = fprintf(g_ekaLogFile, "\n");
  }

  // Log to stdout
  rc1 = printf("%s@%s:%u: ", function, file, line);
  va_start(ap, format);
  rc2 = vprintf(format, ap);
  va_end(ap);
  rc3 = printf("\n");

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
  signal(SIGINT, INThandler);

  openLogFiles(argv[0]);

  // ==============================================
  AffinityConfig affinityConfig = {
      -1, -1, -1, -1, -1,
  };
  EkaCallbacks cb = {.logCb = ekaDefaultLog,
                     .cbCtx = g_ekaLogFile};

  if (openDev(&affinityConfig, &cb) != OPRESULT__OK)
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
      allocateNewAction(ActionType::MoexFireNew);
  setActionTcpSock(fireNewActionIdx, EkaDummySock);
  setActionNext(fireNewActionIdx, CHAIN_LAST_ACTION);

  auto fireReplaceActionIdx =
      allocateNewAction(ActionType::MoexFireReplace);
  setActionTcpSock(fireReplaceActionIdx, EkaDummySock);
  setActionNext(fireReplaceActionIdx, CHAIN_LAST_ACTION);

  const char fireNewMsg[] = "MOEX dummy pkt a b c d e f g h"
                            "NEW";
  setActionPayload(fireNewActionIdx, &fireNewMsg,
                   strlen(fireNewMsg));

  const char fireReplaceMsg[] =
      "MOEX dummy pkt a b c d e f g h"
      "Replace";

  if (setActionPayload(
          fireReplaceActionIdx, &fireReplaceMsg,
          strlen(fireReplaceMsg)) != OPRESULT__OK)
    on_error("setActionPayload() failed");

  // Static Product
  ProdPairInitParams prodPairInitParams = {};
  prodPairInitParams.secBase = prodList_[0];
  prodPairInitParams.secQuote = prodList_[1];
  prodPairInitParams.fireBaseNewIdx = fireNewActionIdx;
  prodPairInitParams.fireQuoteReplaceIdx =
      fireReplaceActionIdx;

  PairIdx pairIdx = 0;

  if (initProdPair(pairIdx, &prodPairInitParams) !=
      OPRESULT__OK)
    on_error("initProdPair() failed");

  RunCtx runCtx = {.onReportCb = getExampleFireReport,
                         .cbCtx = NULL};
  runMoexStrategy(&runCtx);

  // base 2500@97145000000 : 90000@97152500000
  // quot  100@ 7194400000 :    20@ 7207400000

  ProdPairDynamicParams prodPairDynamicParams;
  prodPairDynamicParams.markupBuy = 0x2;
  prodPairDynamicParams.markupSell = 0x3;
  prodPairDynamicParams.fixSpread = 0x4;
  prodPairDynamicParams.tolerance = 0x5;
  prodPairDynamicParams.quoteSize = 0x6;
  prodPairDynamicParams.timeTolerance = 0x7;

  if (setProdPairDynamicParams(
          pairIdx, &prodPairDynamicParams) != OPRESULT__OK)
    on_error("setProdPairDynamicParams() failed");

  EKA_LOG("Test Before Loop");

  // ClOrdID
  if (setClOrdId(123) != OPRESULT__OK)
    on_error("setClOrdId() failed");
  if (resetReplaceCnt() != OPRESULT__OK)
    on_error("resetReplaceCnt() failed");
  if (setReplaceThreshold(100) != OPRESULT__OK)
    on_error("setReplaceThreshold() failed");

  // Set SW order
  if (setNewOrderPrice(pairIdx,
		       OrderSide::BUY,
		       444) != OPRESULT__OK)
    on_error("setNewOrderPrice() failed");
  if (setNewOrderPrice(pairIdx,
		       OrderSide::SELL,
		       555) != OPRESULT__OK)
    on_error("setNewOrderPrice() failed");
  
  if (setReplaceOrderParams(
                        pairIdx, OrderSide::BUY,
			    666,9090) != OPRESULT__OK)
    on_error("setReplaceOrderParams() failed");
  if (setReplaceOrderParams(
                        pairIdx, OrderSide::SELL,
                        777,8080) != OPRESULT__OK)
    on_error("setReplaceOrderParams() failed");

  if (armProductPair(pairIdx, true, 0) != OPRESULT__OK)
    on_error("armProductPair() failed");

#ifndef _VERILOG_SIM
  while (g_keepWork)
    std::this_thread::yield();

  stopRcvMd_A();
  rcvA.join();

  if (closeDev() != OPRESULT__OK)
    on_error("closeDev() failed");
#endif

  closeLogFiles();

  return 0;
}
