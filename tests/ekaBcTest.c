#include <arpa/inet.h>
#include <getopt.h>
#include <iterator>
#include <linux/sockios.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <net/if.h>
#include <sys/ioctl.h>

#include <chrono>
#include <sys/time.h>

#include "EkaDev.h"

#include "Efc.h"
#include "Efh.h"
#include "Eka.h"
#include "Epm.h"
#include "Exc.h"

#include "eka_macros.h"

#include "ekaNW.h"
#include <fcntl.h>

#include "../googletest/TestTcpCtx.h"
#include "Eobi/EOBILayouts.h"

#define MASK32 0xffffffff

/* --------------------------------------------- */
volatile bool keep_work = true;
volatile bool serverSet = false;
volatile bool rxClientReady = false;

struct EobiAddOrderPkt {
  PacketHeaderT pktHdr;
  OrderAddT orderAddMsg;
};

struct EobiExecSumPkt {
  PacketHeaderT pktHdr;
  ExecutionSummaryT execSumMsg;
};

/* --------------------------------------------- */

void getFireReport(const void *p, size_t len, void *ctx) {
  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(p)};
  if (containerHdr->type == EkaEventType::kExceptionEvent)
    return;

  return;
}
/* --------------------------------------------- */

void INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  TEST_LOG(
      "Ctrl-C detected: keep_work = false, exitting...");
  fflush(stdout);
  return;
}

/* --------------------------------------------- */
static ssize_t sendUdpPkt(int udpSock, const void *buf,
                          size_t len,
                          const sockaddr_in *dstAddr) {
  return sendto(udpSock, buf, len, 0,
                (const sockaddr *)dstAddr,
                sizeof(sockaddr));
}
/* ############################################# */

int main(int argc, char *argv[]) {

  signal(SIGINT, INThandler);
  // ==============================================
  static const char *udpSrcIp[] = {
      "10.0.0.10",
      "10.0.0.11",
      "10.0.0.12",
      "10.0.0.13",
  };

  static const EkaBcMcGroupParams mc0[] = {0, "224.0.0.0",
                                           30300};
  static const EkaBcUdpMcParams core0_1mc = {
      mc0, std::size(mc0)};
  static const TestTcpSess tcp0 = {0, "100.0.0.0",
                                   "10.0.0.10", 22000};
  // ==============================================

  auto dev = ekaBcOpenDev();
  EKA_LOG("Initializing FPGA port %d to %s", tcp0.coreId,
          tcp0.srcIp);
  const EkaBcPortAttrs laneAttr = {
      .host_ip = inet_addr(tcp0.srcIp),
      .netmask = inet_addr("255.255.255.0"),
      .gateway = inet_addr(tcp0.dstIp),
      .nexthop_mac = {}, // resolved by our internal ARP
      .src_mac_addr = {} // taken from system config
  };
  auto lane = static_cast<EkaBcLane>(tcp0.coreId);
  ekaBcConfigurePort(dev, lane, &laneAttr);
  // ==============================================

  EkaBcInitCtx initCtx = {.report_only = false,
                          .watchdog_timeout_sec = 1000000};
  auto rc = ekaBcInit(dev, &initCtx);
  // ==============================================
  rc = ekaBcInitEurStrategy(dev, &core0_1mc);
  // ==============================================
  EkaBcEurSecId prodList[] = {123, 456, 789};

  rc = ekaBcSetProducts(dev, prodList, std::size(prodList));

  auto h = ekaBcGetSecHandle(dev, prodList[0]);

  auto eurHwAction =
      ekaBcAllocateNewAction(dev, EkaBcActionType::EurFire);

#if 0
  rc = ekaBcSetActionTcpSock(
      dev, eurHwAction, 0 /* FIXME!!! */);
#endif
  // ==============================================

  uint8_t activeJumpAtBestSet = 2;
  uint8_t activeJumpBetterBestSet = 4;
  EkaBcEurMdSize sizeMultiplier = 10000;
  uint8_t AggressorSide = ENUM_AGGRESSOR_SIDE_BUY;
  /////////////// General

  /////////////// TOB
  EkaBcEurMdSize rawTobBidSize = 5;
  EkaBcEurPrice tobBidPrice = 50000;

  EkaBcEurMdSize rawTobAskSize = 8;
  EkaBcEurPrice tobAskPrice = 50006;
  /////////////// TOB

  /////////////// Trade
  EkaBcEurMdSize rawTradeSize = 2;
  EkaBcEurPrice tradePrice = 60000;
  /////////////// Trade

  EkaBcEurMdSize tobBidSize =
      rawTobBidSize * sizeMultiplier;
  EkaBcEurMdSize tobAskSize =
      rawTobAskSize * sizeMultiplier;
  EkaBcEurMdSize tradeSize = rawTradeSize * sizeMultiplier;
  // ==============================================
  EkaBcEurProductInitParams prodParams = {};
  prodParams.fireActionIdx = eurHwAction;
  prodParams.secId = prodList[0];
  prodParams.step = 1;
  prodParams.isBook = 1;
  prodParams.maxBidSize = sizeMultiplier;     // TBD
  prodParams.maxAskSize = sizeMultiplier * 2; // TBD
  prodParams.maxBookSpread = tobAskPrice - tobBidPrice;
  prodParams.midPoint =
      (tobAskPrice - tobBidPrice) / 2 + tobBidPrice;

  rc = ekaBcInitEurProd(dev, h, &prodParams);

  const char EurFireMsg[] = "EurFireMsg with Dummy "
                            "payload";
  rc = ekaBcSetActionPayload(dev, eurHwAction, &EurFireMsg,
                             strlen(EurFireMsg));

  EkaBcEurJumpParams jumpParams = {};
  jumpParams.atBest[activeJumpAtBestSet].max_tob_size =
      (tobBidSize > tobAskSize) ? tobBidSize : tobAskSize;
  jumpParams.atBest[activeJumpAtBestSet].min_tob_size =
      (tobBidSize > tobAskSize) ? tobAskSize : tobBidSize;
  jumpParams.atBest[activeJumpAtBestSet].max_post_size =
      tobBidSize - tradeSize; // TBD assume BUY ticker
  jumpParams.atBest[activeJumpAtBestSet].min_ticker_size =
      tradeSize;
  jumpParams.atBest[activeJumpAtBestSet].min_price_delta =
      tradePrice - tobBidPrice; // TBD assume BUY ticker
  jumpParams.atBest[activeJumpAtBestSet].buy_size =
      sizeMultiplier;
  jumpParams.atBest[activeJumpAtBestSet].sell_size =
      sizeMultiplier * 2;
  jumpParams.atBest[activeJumpAtBestSet].strat_en = 0;
  jumpParams.atBest[activeJumpAtBestSet].boc = 1;

  rc = ekaBcEurSetJumpParams(dev, h, &jumpParams);

  EkaBcArmVer armVer = 0;

  rc = ekaBcArmEur(dev, h, true /* armBid */,
                   true /* armAsk */, armVer);
  // ==============================================
  EkaBcRunCtx runCtx = {.onReportCb = getFireReport};
  ekaBcEurRun(dev, &runCtx);
  sleep(1);
  // ==============================================
  EobiAddOrderPkt addOrderBidPkt = {};
  addOrderBidPkt.pktHdr.TransactTime = 0; // TBD
  addOrderBidPkt.orderAddMsg.MessageHeader.BodyLen =
      sizeof(MessageHeaderCompT);
  addOrderBidPkt.orderAddMsg.MessageHeader.TemplateID =
      TID_ORDER_ADD;
  addOrderBidPkt.orderAddMsg.RequestTime = 0; // TBD
  addOrderBidPkt.orderAddMsg.SecurityID = prodList[0];
  addOrderBidPkt.orderAddMsg.OrderDetails.DisplayQty =
      tobBidSize;
  addOrderBidPkt.orderAddMsg.OrderDetails.Side =
      ENUM_SIDE_BUY;
  addOrderBidPkt.orderAddMsg.OrderDetails.Price =
      tobBidPrice;

  EobiAddOrderPkt addOrderAskPkt = {};
  addOrderAskPkt.pktHdr.TransactTime = 0; // TBD
  addOrderAskPkt.orderAddMsg.MessageHeader.BodyLen =
      sizeof(MessageHeaderCompT);
  addOrderAskPkt.orderAddMsg.MessageHeader.TemplateID =
      TID_ORDER_ADD;
  addOrderAskPkt.orderAddMsg.RequestTime = 0; // TBD
  addOrderAskPkt.orderAddMsg.SecurityID = prodList[0];
  addOrderAskPkt.orderAddMsg.OrderDetails.DisplayQty =
      tobAskSize;
  addOrderAskPkt.orderAddMsg.OrderDetails.Side =
      ENUM_SIDE_SELL;
  addOrderAskPkt.orderAddMsg.OrderDetails.Price =
      tobAskPrice;

  EobiExecSumPkt execSumPkt = {};
  execSumPkt.pktHdr.TransactTime = 0; // TBD
  execSumPkt.execSumMsg.MessageHeader.BodyLen =
      sizeof(MessageHeaderCompT);
  execSumPkt.execSumMsg.MessageHeader.TemplateID =
      TID_EXECUTION_SUMMARY;
  execSumPkt.execSumMsg.SecurityID = prodList[0];
  execSumPkt.execSumMsg.RequestTime = 0; // TBD
  execSumPkt.execSumMsg.LastQty = tradeSize;
  execSumPkt.execSumMsg.AggressorSide = AggressorSide;
  execSumPkt.execSumMsg.LastPx = tradePrice;

  // ==============================================
  auto udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udpSock < 0)
    on_error("failed to open UDP sock");

  struct sockaddr_in mcSrc = {};
  struct sockaddr_in mcDst = {};

  mcSrc.sin_addr.s_addr = inet_addr(udpSrcIp[mc0[0].lane]);
  mcSrc.sin_family = AF_INET;
  mcSrc.sin_port = 0;

  mcDst.sin_family = AF_INET;
  mcDst.sin_addr.s_addr = inet_addr(mc0[0].mcIp);
  mcDst.sin_port = be16toh(mc0[0].mcUdpPort);

  int r;
  // ==============================================

  EKA_LOG("Sending addOrderBidPkt to %s:%u",
          EKA_IP2STR(mcDst.sin_addr.s_addr),
          be16toh(mcDst.sin_port));
  r = sendto(udpSock, &addOrderBidPkt,
             sizeof(addOrderBidPkt), 0,
             (const sockaddr *)&mcDst, sizeof(sockaddr));

  if (rc < 0)
    on_error("MC trigger send failed");
  sleep(1);
  // ==============================================

  EKA_LOG("Sending addOrderAskPkt to %s:%u",
          EKA_IP2STR(mcDst.sin_addr.s_addr),
          be16toh(mcDst.sin_port));
  r = sendto(udpSock, &addOrderAskPkt,
             sizeof(addOrderAskPkt), 0,
             (const sockaddr *)&mcDst, sizeof(sockaddr));

  if (rc < 0)
    on_error("MC trigger send failed");
  sleep(1);
  // ==============================================

  EKA_LOG("Sending execSumPkt to %s:%u",
          EKA_IP2STR(mcDst.sin_addr.s_addr),
          be16toh(mcDst.sin_port));
  r = sendto(udpSock, &execSumPkt, sizeof(execSumPkt), 0,
             (const sockaddr *)&mcDst, sizeof(sockaddr));

  if (rc < 0)
    on_error("MC trigger send failed");
  sleep(1);
  // ==============================================
  ekaBcCloseDev(dev);

  return 0;
}
