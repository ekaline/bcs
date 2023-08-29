#include "TestP4.h"

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

  createSecList(secConf);

  // subscribing on list of securities
  efcEnableFiringOnSec(g_ekaDev, secList_, nSec_);

  // ==============================================
  // setting security contexts
  for (size_t i = 0; i < nSec_; i++) {
    auto handle = getSecCtxHandle(g_ekaDev, secList_[i]);

    if (handle < 0) {
      EKA_WARN(
          "Security[%ju] %s was not "
          "fit into FPGA hash: handle = %jd",
          i, cboeSecIdString(secConf->sec[i].id, 8).c_str(),
          handle);
      continue;
    }

    SecCtx secCtx = {};
    getSecCtx(&secConf->sec[i], &secCtx);

    EKA_LOG(
        "Setting StaticSecCtx[%ju] \'%s\' secId=0x%016jx,"
        "handle=%jd,bidMinPrice=%u,askMaxPrice=%u,"
        "bidSize=%u,askSize=%u,"
        "versionKey=%u,lowerBytesOfSecId=0x%x",
        i, cboeSecIdString(secConf->sec[i].id, 8).c_str(),
        secList_[i], handle, secCtx.bidMinPrice,
        secCtx.askMaxPrice, secCtx.bidSize, secCtx.askSize,
        secCtx.versionKey, secCtx.lowerBytesOfSecId);
    /* hexDump("secCtx",&secCtx,sizeof(secCtx)); */

    rc = efcSetStaticSecCtx(g_ekaDev, handle, &secCtx, 0);
    if (rc != EKA_OPRESULT__OK)
      on_error("failed to efcSetStaticSecCtx");
  }

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

static uint64_t getBinSecId(const char *secChar) {
  if (strlen(secChar) != 6)
    on_error("unexpected security \'%s\' len %ju != 6",
             secChar, strlen(secChar));
  char shiftedStr[8] = {};
  for (auto i = 0; i < 6; i++)
    shiftedStr[i + 2] = secChar[i];

  return be64toh(*(uint64_t *)shiftedStr);
}

void TestP4::createSecList(const TestP4SecConf *secConf) {
  for (auto i = 0; i < secConf->nSec; i++) {
    secList_[i] = getBinSecId(secConf->sec[i].id);
  }
  nSec_ = secConf->nSec;
  EKA_LOG("Created List of %ju P4 Securities:", nSec_);
  for (auto i = 0; i < nSec_; i++)
    EKA_LOG("\t\'%.8s\', 0x%jx %c",
            cboeSecIdString(secConf->sec[i].id, 8).c_str(),
            secList_[i], i == nSec_ - 1 ? '\n' : ',');
}
/* ############################################# */
void TestP4::getSecCtx(const TestP4CboeSec *secCtx,
                       SecCtx *dst) {

  dst->bidMinPrice = secCtx->bidMinPrice / 100;
  dst->askMaxPrice = secCtx->askMaxPrice / 100;
  dst->bidSize = secCtx->size;
  dst->askSize = secCtx->size;
  dst->versionKey = 5; // TBD

  dst->lowerBytesOfSecId =
      (uint8_t)(getBinSecId(secCtx->id) & 0xFF);
}
/* ############################################# */

void TestP4::sendData(const void *mdInjectParams) {
  EfcArmVer p4ArmVer = 0;

  auto mdConf =
      reinterpret_cast<const TestP4Md *>(mdInjectParams);
  // ==============================================

  uint32_t sequence = 32;
  const int LoopIterations = loop_ ? 10000 : 1;

  for (auto l = 0; l < LoopIterations; l++) {
    efcArmP4(g_ekaDev, p4ArmVer);
    char pktBuf[1500] = {};

    auto pktLen = createOrderExpanded(
        pktBuf, mdConf->secId, mdConf->side, mdConf->price,
        mdConf->size);

    p4ArmVer = sendPktToAll(pktBuf, pktLen, p4ArmVer);

    auto fReport = fireReports.back();
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

  EKA_LOG("%s \'%s\' s=%c, P=%ju, S=%u, c=%c",
          EKA_BATS_PITCH_MSG_DECODE(p->msg.header.type),
          std::string(p->msg.exp_symbol,
                      sizeof(p->msg.exp_symbol))
              .c_str(),
          p->msg.side, p->msg.price, p->msg.size,
          p->msg.customer_indicator);
  return sizeof(CboePitchAddOrderExpanded);
}
