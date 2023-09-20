#include "TestP4.h"
#include "EkaHwInternalStructs.h"

/* ############################################# */

void TestP4::initializeAllCtxs(
    const TestCaseConfig *unused) {
  int i = 0;
  for (auto &sec : allSecs_) {
    if (!sec.valid)
      continue;
    sec.handle = getSecCtxHandle(g_ekaDev, sec.binId);
    if (sec.handle < 0) {
      EKA_WARN("Security[%d] \'%s\' 0x%016jx was not "
               "fit into FPGA hash: handle = %jd",
               i, sec.strId.c_str(), sec.binId, sec.handle);
    } else {
      SecCtx secCtx = {};
      setSecCtx(&sec, &secCtx);

      EKA_LOG(
          "Setting StaticSecCtx[%ju] \'%s\' secId=0x%016jx,"
          "handle=%jd,bidMinPrice=%u,askMaxPrice=%u,"
          "bidSize=%u,askSize=%u,"
          "versionKey=%u,lowerBytesOfSecId=0x%x",
          i, sec.strId.c_str(), sec.binId, sec.handle,
          secCtx.bidMinPrice, secCtx.askMaxPrice,
          secCtx.bidSize, secCtx.askSize, secCtx.versionKey,
          secCtx.lowerBytesOfSecId);
      /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

      auto rc = efcSetStaticSecCtx(g_ekaDev, sec.handle,
                                   &secCtx, 0);
      if (rc != EKA_OPRESULT__OK)
        on_error("failed to efcSetStaticSecCtx");
    }
    i++;
  }
}
/* ############################################# */

void TestP4::checkAllCtxs() {
  for (const auto &sec : allSecs_) {
    if (!sec.valid || sec.handle < 0)
      continue;

    uint64_t protocol_id = 2; // pitch

    uint64_t hw_cmd = (sec.binId & 0x00ffffffffffffff) |
                      (protocol_id << 56);

    eka_write(0xf0038, hw_cmd);

    hw_ctxdump_t hwCtxDump = {};

    uint words2read = roundUp8(sizeof(hw_ctxdump_t)) / 8;
    uint64_t srcAddr = 0x71000;
    uint64_t *dstAddr = (uint64_t *)&hwCtxDump;

    for (uint w = 0; w < words2read; w++)
      *dstAddr++ = eka_read(srcAddr++);

    if (hwCtxDump.HashStatus == 0) {
      char errMsg[2048] = {};
      sprintf(errMsg,
              "\'%s\' (0x%016jx) (sw handle = %u) "
              "not found in hash",
              sec.strId.c_str(), sec.binId, sec.handle);
      EKA_WARN("%s", errMsg);
      ADD_FAILURE() << errMsg;
    } else {
      EKA_LOG("sec.binId 0x%016jx found, ctx in handle %u: "
              "bidMinPrice=%d,askMaxPrice=%d, bidSize=%d, "
              "askSize=%d, versionKey=%d, "
              "lowerBytesOfSecId=0x%x",
              sec.binId, hwCtxDump.Handle,
              hwCtxDump.SecCTX.bidMinPrice,
              hwCtxDump.SecCTX.askMaxPrice,
              hwCtxDump.SecCTX.bidSize,
              hwCtxDump.SecCTX.askSize,
              hwCtxDump.SecCTX.versionKey,
              hwCtxDump.SecCTX.lowerBytesOfSecId);
    }
  }
}
/* ############################################# */

void TestP4::configureStrat(const TestCaseConfig *tc) {
  ASSERT_NE(tc, nullptr);
  auto dev = g_ekaDev;
  ASSERT_NE(dev, nullptr);

  EKA_LOG("\n"
          "=========== Configuring P4 Test ===========");
  auto udpMcParams = &udpCtx_->udpConf_;

  FireStatisticsAddr_ = 0xf0340;

  armController_ = efcArmP4;
  disArmController_ = efcDisArmP4;

  struct EfcP4Params p4Params = {
      .feedVer = EfhFeedVer::kCBOE,
      .fireOnAllAddOrders = true,
      .max_size = 10000,
  };

  int rc =
      efcInitP4Strategy(g_ekaDev, udpMcParams, &p4Params);
  if (rc != EKA_OPRESULT__OK)
    on_error("efcInitP4Strategy returned %d", (int)rc);

  auto secConf = reinterpret_cast<const TestP4SecConf *>(
      tc->algoConfigParams);

  createSecList(tc->algoConfigParams);

// subscribing on list of securities
#if 0
  for (auto i = 0; i < nSec_; i++)
    EKA_LOG("efcEnableFiringOnSec: secList_[%d] = 0x%jx", i,
            secList_[i]);
#endif
  efcEnableFiringOnSec(g_ekaDev, secList_, nSec_);

  for (auto i = 0; i < nSec_; i++)
    EKA_LOG("secList_[i] = 0x%016jx", secList_);
  // ==============================================
  initializeAllCtxs(tc);

  // ==============================================
  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    for (auto mcGr = 0; mcGr < udpCtx_->nMcCons_[coreId];
         mcGr++) {
      // 1st in the chain is already preallocated
      int currActionIdx =
          coreId * EFC_PREALLOCATED_P4_ACTIONS_PER_LANE +
          mcGr;

      for (auto tcpConn = 0; tcpConn < tcpCtx_->nTcpSess_;
           tcpConn++) {
        rc = setActionTcpSock(
            g_ekaDev, currActionIdx,
            tcpCtx_->tcpSess_[tcpConn]->excSock_);
        if (rc != EKA_OPRESULT__OK)
          on_error("setActionTcpSock failed for Action %d",
                   currActionIdx);

        int nextActionIdx =
            tcpConn == tcpCtx_->nTcpSess_ - 1
                ? EPM_LAST_ACTION
                : efcAllocateNewAction(
                      g_ekaDev, EpmActionType::BoeFire);
        rc = setActionNext(dev, currActionIdx,
                           nextActionIdx);
        if (rc != EKA_OPRESULT__OK)
          on_error("setActionNext failed for Action %d",
                   currActionIdx);

        char fireMsg[1500] = {};
        sprintf(fireMsg,
                "BoeFireA: %03d         "
                "    MC:%d:%d "
                "Tcp:%d ",
                currActionIdx, coreId, mcGr, tcpConn);
        rc = efcSetActionPayload(
            dev, currActionIdx, fireMsg,
            sizeof(BoeQuoteUpdateShortMsg));
        if (rc != EKA_OPRESULT__OK)
          on_error(
              "efcSetActionPayload failed for Action %d",
              currActionIdx);

        currActionIdx = nextActionIdx;
      }
    }
  }
}
/* ############################################# */
/* --------------------------------------------- */

uint64_t TestP4::getBinSecId(std::string strId) {
  const char *secChar = strId.c_str();
  if (strlen(secChar) != 6)
    on_error("unexpected security \'%s\' len %ju != 6",
             secChar, strlen(secChar));
  char shiftedStr[8] = {};
  for (auto i = 0; i < 6; i++)
    shiftedStr[i + 2] = secChar[i];

  return be64toh(*(uint64_t *)shiftedStr);
}

/* ############################################# */

void TestP4::createSecList(const void *algoConfigParams) {
  if (!algoConfigParams)
    on_error("!algoConfigParams");

  auto secConf = reinterpret_cast<const TestP4SecConf *>(
      algoConfigParams);

  for (auto i = 0; i < secConf->nSec; i++) {
    TestP4CboeSec sec = {
        .strId = secConf->sec[i].strId,
        .binId = getBinSecId(secConf->sec[i].strId),
        .bidMinPrice = secConf->sec[i].bidMinPrice,
        .askMaxPrice = secConf->sec[i].askMaxPrice,
        .size = secConf->sec[i].size,
        .valid = true,
        .handle = -1};
    allSecs_.push_back(sec);

    secList_[i] = sec.binId;
  }

  nSec_ = secConf->nSec;
  EKA_LOG("Created List of %ju P4 Securities:", nSec_);
  for (auto i = 0; i < nSec_; i++)
    EKA_LOG("\t\'%.8s\', 0x%jx %c",
            secConf->sec[i].strId.c_str(), secList_[i],
            i == nSec_ - 1 ? '\n' : ',');
}
/* ############################################# */
void TestP4::setSecCtx(const TestP4CboeSec *secCtx,
                       SecCtx *dst) {

  dst->bidMinPrice = secCtx->bidMinPrice;
  dst->askMaxPrice = secCtx->askMaxPrice;
  dst->bidSize = secCtx->size;
  dst->askSize = secCtx->size;
  dst->versionKey = 5; // TBD

  dst->lowerBytesOfSecId =
      (uint8_t)(getBinSecId(secCtx->strId) & 0xFF);
}

/* ############################################# */

void TestP4::generateMdDataPkts(
    const void *mdInjectParams) {
  auto md =
      reinterpret_cast<const TestP4Md *>(mdInjectParams);
  // ==============================================
  insertedMd_.push_back(*md);
}
/* ############################################# */
std::pair<uint32_t, bool> TestP4::getArmVer() {
  auto curArm = eka_read(ArmDisarmP4Addr);
  auto curArmState = curArm & 0x1;
  auto curArmVer = (curArm >> 32) & 0xFFFFFFFF;
  return std::pair<uint32_t, bool>(curArmVer, curArmState);
}
/* ############################################# */

void TestP4::sendData() {
  for (const auto &md : insertedMd_) {
    char pktBuf[1500] = {};
    auto pktLen =
        createOrderExpanded(pktBuf, md.secId.c_str(),
                            md.side, md.price, md.size);
    EKA_LOG("Sending MD for %s: "
            "side= \'%c\', "
            "price=%u, "
            "size=%u",
            md.secId.c_str(), cboeSide(md.side), md.price,
            md.size);
    sendPktToAll(pktBuf, pktLen, md.expectedFire);
    if (testFailed_) {
      TEST_LOG("TEST FAILED at sending MD for %s: "
               "side= \'%c\', "
               "price=%u, "
               "size=%u",
               md.secId.c_str(), cboeSide(md.side),
               md.price, md.size);
      EXPECT_FALSE(!testFailed_);
      return;
    }
  }
}
// ==============================================
size_t TestP4::createOrderExpanded(char *dst,
                                   const char *id,
                                   SideT side,
                                   uint64_t price,
                                   uint32_t size) {
  auto p =
      reinterpret_cast<CboePitchAddOrderExpanded *>(dst);

  p->hdr.length = sizeof(CboePitchAddOrderExpanded);
  p->hdr.count = 1;
  p->hdr.sequence = sequence_++;

  p->msg.header.length = sizeof(p->msg);
  p->msg.header.type = (uint8_t)MsgId::ADD_ORDER_EXPANDED;
  p->msg.header.time = 0x11223344; // just a number

  p->msg.order_id = 0xaabbccddeeff5566;
  p->msg.side = cboeSide(side);
  p->msg.size = size;

  char dstSymb[8] = {id[0], id[1], id[2], id[3],
                     id[4], id[5], ' ',   ' '};

  memcpy(p->msg.exp_symbol, dstSymb, 8);

  p->msg.price = price;

  p->msg.customer_indicator = 'C';

  EKA_LOG("Pkt to Send: %s \'%s\' s=%c, P=%ju, S=%u, c=%c",
          EKA_BATS_PITCH_MSG_DECODE(p->msg.header.type),
          std::string(p->msg.exp_symbol,
                      sizeof(p->msg.exp_symbol))
              .c_str(),
          p->msg.side, p->msg.price, p->msg.size,
          p->msg.customer_indicator);
  return sizeof(CboePitchAddOrderExpanded);
}

// ==============================================

void TestP4::checkAlgoCorrectness(
    const TestCaseConfig *unused) {

  int i = 0;

  for (const auto &md : insertedMd_) {
    if (!md.expectedFire)
      continue;

    auto &fr = fireReports_.at(i);
    efcPrintFireReport(fr->buf, fr->len, g_ekaLogFile);

    getReportPtrs(fr->buf, fr->len);

    auto msgP = firePkt_ + TcpDatagramOffset;
    auto firedPayloadDiff =
        memcmp(msgP, echoedPkts_.at(i)->buf,
               sizeof(BoeQuoteUpdateShortMsg));
#if 1
    if (firedPayloadDiff) {
      char FireReportBufStr[10000] = {};
      hexDump2str("FireReport Msg", msgP,
                  sizeof(BoeQuoteUpdateShortMsg),
                  FireReportBufStr,
                  sizeof(FireReportBufStr));

      char echoedPktsBufStr[10000] = {};
      hexDump2str("Echo Pkt", echoedPkts_.at(i)->buf,
                  echoedPkts_.at(i)->len, echoedPktsBufStr,
                  sizeof(echoedPktsBufStr));

      EKA_LOG("FireReport Msg != echoedPkt: %s %s",
              FireReportBufStr, echoedPktsBufStr);
    }
#endif
    EXPECT_EQ(firedPayloadDiff, 0);

    auto boeQuoteUpdateShort =
        reinterpret_cast<const BoeQuoteUpdateShortMsg *>(
            msgP);

    auto firedSecIdStr =
        std::string(boeQuoteUpdateShort->Symbol, 6);

    EXPECT_EQ(md.secId, firedSecIdStr);

    EXPECT_EQ(md.price, boeQuoteUpdateShort->Price);
    EXPECT_EQ(md.size, boeQuoteUpdateShort->OrderQty);
    EXPECT_EQ(boeQuoteUpdateShort->Side,
              cboeOppositeSide(md.side));
    i++;
  }
}
