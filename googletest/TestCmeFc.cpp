#include "TestCmeFc.h"

void TestCmeFc::configure(const TestCaseConfig *t) {
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

void TestCmeFc::sendData(const void *mdInjectParams) {
  int cmeFcExpectedFires = 0;
  int TotalInjects = 4;
  EfcArmVer cmeFcArmVer = 0;

  uint64_t appSeq = 3000;
  efcSetSessionCntr(g_ekaDev, tcpCtx_->tcpSess_[0]->hCon_,
                    appSeq);

  for (auto i = 0; i < TotalInjects; i++) {
    // efcArmCmeFc(g_ekaDev, cmeFcArmVer++); // arm and
    // promote
    cmeFcExpectedFires++;

    const uint8_t pktBuf[] = {
        0x22, 0xa5, 0x0d, 0x02, 0xa5, 0x6f, 0x01, 0x38,
        0xca, 0x42, 0xdc, 0x16, 0x60, 0x00, 0x0b, 0x00,
        0x30, 0x00, 0x01, 0x00, 0x09, 0x00, 0x41, 0x23,
        0xff, 0x37, 0xca, 0x42, 0xdc, 0x16, 0x01, 0x00,
        0x00, 0x20, 0x00, 0x01, 0x00, 0xfc, 0x2f, 0x9c,
        0x9d, 0xb2, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x5b, 0x33, 0x00, 0x00, 0x83, 0x88, 0x26, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0xd9, 0x7a,
        0x6d, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x0e, 0x19, 0x84, 0x8e,
        0x36, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x7f, 0x8e,
        0x36, 0x06, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00};

    auto pktLen = sizeof(pktBuf);

    cmeFcArmVer = sendPktToAll(pktBuf, pktLen, cmeFcArmVer);

    // usleep(300000);
  }
  // ==============================================

  return;
}
