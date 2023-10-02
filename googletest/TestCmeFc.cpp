#include "TestCmeFc.h"

void TestCmeFc::configureStrat(const TestCaseConfig *t) {
  ASSERT_NE(t, nullptr);
  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  EKA_LOG("\n"
          "=========== Configuring CmeFc Test ===========");
  auto udpMcParams = &udpCtx_->udpConf_;

  FireStatisticsAddr_ = 0xf0800;
  armController_ = efcArmCmeFc;
  disArmController_ = efcDisArmCmeFc;

  static const uint16_t QEDTestPurgeDSID = 0x1234;
  static const uint8_t QEDTestMinNumLevel = 5;

  EfcCmeFcParams cmeParams = {.maxMsgSize = 97,
                              .minNoMDEntries = 0};

  int rc = efcInitCmeFcStrategy(g_ekaDev, udpMcParams,
                                &cmeParams);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitCmeFcStrategy returned %d", (int)rc);

  auto cmeFcHwAction =
      efcAllocateNewAction(dev, EpmActionType::CmeHwCancel);

  efcCmeFcSetFireAction(g_ekaDev, cmeFcHwAction);

  rc = setActionTcpSock(dev, cmeFcHwAction,
                        tcpCtx_->tcpSess_[0]->excSock_);

  if (rc != EKA_OPRESULT__OK)
    on_error("setActionTcpSock failed for Action %d",
             cmeFcHwAction);

  const char CmeTestFastCancelMsg[] =
      "CME Fast Cancel: Sequence = |____| With Dummy "
      "payload";

  rc = efcSetActionPayload(dev, cmeFcHwAction,
                           &CmeTestFastCancelMsg,
                           strlen(CmeTestFastCancelMsg));
  if (rc != EKA_OPRESULT__OK)
    on_error("efcSetActionPayload failed for Action %d",
             cmeFcHwAction);
}

/* ############################################# */

void TestCmeFc::generateMdDataPkts(
    const void *mdInjectParams) {
  auto md =
      reinterpret_cast<const TestCmeFcMd *>(mdInjectParams);
  // ==============================================
  insertedMd_.push_back(*md);
}
/* ############################################# */

void TestCmeFc::sendData() {
  for (const auto &md : insertedMd_) {
    efcSetSessionCntr(g_ekaDev, tcpCtx_->tcpSess_[0]->hCon_,
                      md.appSeq);

    sendPktToAll(md.preloadedPkt, md.pktLen,
                 md.expectedFire);
  }
}

/* ############################################# */
void TestCmeFc::checkAlgoCorrectness(
    const TestCaseConfig *tc) {

  int i = 0;
  for (const auto &fr : fireReports_) {

    auto injectedMd = reinterpret_cast<const TestCmeFcMd *>(
        tc->mdInjectParams);

    efcPrintFireReport(fr->buf, fr->len, g_ekaLogFile);

    getReportPtrs(fr->buf, fr->len);
    if (!firePkt_)
      FAIL() << "!firePkt_";

    auto msgP = firePkt_ + TcpDatagramOffset;

    auto payloadLen = echoedPkts_.at(i)->len;
    auto dummyFireMsg =
        reinterpret_cast<const DummyIlinkMsg *>(msgP);

    auto firedPayloadDiff =
        memcmp(msgP, echoedPkts_.at(i)->buf, payloadLen);
#if 0
    hexDump("FireReport Msg", msgP, payloadLen);
    hexDump("Echo Pkt", echoedPkts_.at(i)->buf,
            echoedPkts_.at(i)->len);
#endif
    EXPECT_EQ(firedPayloadDiff, 0);

    EXPECT_EQ(dummyFireMsg->appSeq, injectedMd->appSeq);
    i++;
  }
}
