#include "TestEur.h"

#if 0
void TestEur::configureStrat(const TestCaseConfig *t) {
  ASSERT_NE(t, nullptr);
  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  EKA_LOG("\n"
          "=========== Configuring Eur Test ===========");
  auto udpMcParams = &udpCtx_->udpConf_;

  FireStatisticsAddr_ = 0xf0800;
  armController_ = ekaBcArmEur;
  disArmController_ = ekaBcDisArmEur;

  auto rc = ekaBcInitEurStrategy(dev, udpMcParams);
  if (rc != OPRESULT__OK)
    on_error("ekaBcInitEurStrategy returned %d", (int)rc);

  auto eurHwAction = ekaBcAllocateNewAction(
      dev, EpmActionType::CmeHwCancel);

  ekaBcEurSetFireAction(dev, eurHwAction);

  rc = setActionTcpSock(dev, eurHwAction,
                        tcpCtx_->tcpSess_[0]->excSock_);

  if (rc != OPRESULT__OK)
    on_error("setActionTcpSock failed for Action %d",
             eurHwAction);

  const char CmeTestFastCancelMsg[] =
      "CME Fast Cancel: Sequence = |____| With Dummy "
      "payload";

  rc = ekaBcSetActionPayload(dev, eurHwAction,
                             &CmeTestFastCancelMsg,
                             strlen(CmeTestFastCancelMsg));
  if (rc != OPRESULT__OK)
    on_error("ekaBcSetActionPayload failed for Action %d",
             eurHwAction);
}
#endif
/* ############################################# */

void TestEur::generateMdDataPkts(
    const void *mdInjectParams) {
#if 0
  auto md =
      reinterpret_cast<const TestEurMd *>(mdInjectParams);
  // ==============================================
  insertedMd_.push_back(*md);
#endif
}
/* ############################################# */

void TestEur::sendData() {
#if 0
  for (const auto &md : insertedMd_) {
    ekaBcSetSessionCntr(dev_, tcpCtx_->tcpSess_[0]->hCon_,
                        md.appSeq);

    sendPktToAll(md.preloadedPkt, md.pktLen,
                 md.expectedFire);
  }
#endif
}

/* ############################################# */
