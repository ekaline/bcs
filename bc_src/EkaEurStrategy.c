#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "Efc.h"
#include "Efh.h"
#include "EkaEpm.h"

#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"

#include "EkaEfcDataStructs.h"

#include "EkaEurStrategy.h"
#include "EkaHwHashTableLine.h"

#include "EkaUdpChannel.h"

#include "EkaEobiParser.h"

#include "EkaBcEurProd.h"
#include "EkaEobiTypes.h"
#include "EpmEti8PktTemplate.h"

using namespace EkaEobi;

/* --------------------------------------------------- */
EkaEurStrategy::EkaEurStrategy(
    const EfcUdpMcParams *mcParams)
    : EkaStrategyEhp(mcParams) {

  name_ = "Eurex";

  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);

  configureTemplates();
  configureEhp();
  // downloadEhp2Hw();

  /* EKA_LOG("Creating %s with %d MC groups on lane #0 " */
  /*         "and %d MC groups on lane #1", */
  /*         name_.c_str(), mcCoreSess_[0].numUdpSess, */
  /*         mcCoreSess_[1].numUdpSess); */

  hashEng_ = new HashEngT();
  if (!hashEng_)
    on_error("!hashEng_");

  EKA_LOG("Created HashEng for Eurex with "
          "%ju Rows, %ju Cols",
          Rows, Cols);

  parser_ = new EkaEobiParser(this);
  if (!parser_)
    on_error("!parser_");
}
/* --------------------------------------------------- */

EkaEurStrategy::~EkaEurStrategy() {
  EKA_LOG("Destroying");
  runLoopThr_.join();
}

/* --------------------------------------------------- */
void EkaEurStrategy::ekaWriteTob(
    EkaBcSecHandle prodHandle,
    const EobiHwBookParams *params, SIDE side) {

  EKA_LOG("TOB prodHandle %jd, side %s", prodHandle,
          side == BID ? "BID" : "ASK");
  auto wr_ptr = reinterpret_cast<const uint64_t *>(params);
  auto nWords = roundUp8(sizeof(*params)) / 8;
  uint32_t dstAddr = 0x90000;
  for (int i = 0; i < nWords; i++) {
    eka_write(dstAddr + i * 8, *(wr_ptr + i));
    //    printf("config %d = %ju\n",i,*(wr_ptr + i));
  }
  union large_table_desc tob_desc = {};
  tob_desc.ltd.src_bank = 0;
  tob_desc.ltd.src_thread = 0;
  tob_desc.ltd.target_idx =
      prodHandle * 2 + (side == BID ? 0 : 1);
  eka_write(0xf0660, tob_desc.lt_desc); // desc
}

/* --------------------------------------------------- */
EkaBcSecHandle
EkaEurStrategy::getSubscriptionId(EkaBcEurSecId secId) {

  auto handle = static_cast<EkaBcSecHandle>(
      hashEng_->getSubscriptionId(secId));

  return handle;
}

/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::subscribeSecList(
    const EkaBcEurSecId *prodList, size_t nProducts) {
  auto prod = prodList;
  for (auto i = 0; i < nProducts; i++) {
    if (hashEng_->subscribeSec(*prod)) {
      nSec_++;
#ifndef _VERILOG_SIM
      uint64_t val = eka_read(dev_, SW_STATISTICS);
      val &= 0xFFFFFFFF00000000;
      val |= (uint64_t)(nSec_);
      eka_write(dev_, SW_STATISTICS, val);
#endif
    }
    prod++;
  }

  hashEng_->packDB();

  return EKABC_OPRESULT__OK;
}

/* --------------------------------------------------- */
EkaBCOpResult EkaEurStrategy::downloadPackedDB() {
  for (auto i = 0; i < Rows; i++) {
    static const size_t BufLen = 256;
    uint64_t buf[BufLen] = {};

    auto [validCnt, len] = hashEng_->getPackedLine(i, buf);
#ifdef _VERILOG_SIM
    if (validCnt == 0)
      continue;
#endif
    int packedWords = roundUp8(len) / 8;

    uint64_t *pWord = buf;
    for (auto i = 0; i < packedWords; i++) {
      // #ifdef EFC_PRINT_HASH
#if 1
      if (validCnt != 0)
        EKA_LOG("%d: 0x%016jx", i, *pWord);
#endif
      eka_write(dev_, FH_SUBS_HASH_BASE + 8 * i, *pWord);
      pWord++;
    }

    union large_table_desc desc = {};
    desc.ltd.src_bank = 0;
    desc.ltd.src_thread = 0;
    desc.ltd.target_idx = i;
    eka_write(dev_, FH_SUBS_HASH_DESC, desc.lt_desc);
  }
  return EKABC_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::initProd(
    EkaBcSecHandle prodHande,
    const EkaBcEurProductInitParams *params) {
  if (prodHande < 0 || prodHande >= MaxSecurities_) {
    EKA_ERROR("Bad prodHande %jd", prodHande);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (prod_[prodHande]) {
    EKA_ERROR("Product with prodHande %jd "
              "is already initialized",
              prodHande);
    return EKABC_OPRESULT__ERR_PRODUCT_ALREADY_INITED;
  }

  prod_[prodHande] =
      new EkaBcEurProd(this, prodHande, params);

  if (!prod_[prodHande])
    on_error("failed creating new Prod");

  return EKABC_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::setProdJumpParams(
    EkaBcSecHandle prodHande,
    const EkaBcEurJumpParams *params) {
  if (prodHande < 0 || prodHande >= MaxSecurities_) {
    EKA_ERROR("Bad prodHande %jd", prodHande);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (!prod_[prodHande]) {
    EKA_ERROR("Product with prodHande %jd "
              "is not initialized",
              prodHande);
    return EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST;
  }

  return prod_[prodHande]->setJumpParams(params);
}

/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::setProdReferenceJumpParams(
    EkaBcSecHandle triggerProd, EkaBcSecHandle fireProd,
    const EkaBcEurReferenceJumpParams *params) {
  if (triggerProd < 0 || triggerProd >= MaxSecurities_) {
    EKA_ERROR("Bad triggerProd %jd", triggerProd);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (fireProd < 0 || fireProd >= MaxSecurities_) {
    EKA_ERROR("Bad fireProd %jd", fireProd);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (!prod_[triggerProd]) {
    EKA_ERROR("triggerProd with handle %jd "
              "is not initialized",
              triggerProd);
    return EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST;
  }

  if (!prod_[fireProd]) {
    EKA_ERROR("fireProd with handle %jd "
              "is not initialized",
              fireProd);
    return EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST;
  }

  if (triggerProd == fireProd) {
    EKA_ERROR("triggerProd == fireProd =  %jd ", fireProd);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  return prod_[triggerProd]->setReferenceJumpParams(
      fireProd, params);
}

/* --------------------------------------------------- */
void EkaEurStrategy::configureTemplates() {
  int templateIdx = (int)EkaEpm::TemplateId::EurEtiFire;
#if 1
  templateIdx = (int)EkaEpm::TemplateId::EurEtiFire;
  epm_->epmTemplate[templateIdx] =
      new EpmEti8PktTemplate(templateIdx);

  EKA_LOG("EpmEti8PktTemplate: "
          "templateIdx = %d, "
          "payload syze = %u Bytes",
          templateIdx,
          epm_->epmTemplate[templateIdx]->getByteSize());
  epm_->DownloadSingleTemplate2HW(
      epm_->epmTemplate[templateIdx]);

#endif
}
/* --------------------------------------------------- */
int EkaEurStrategy::sendDate2Hw() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  uint64_t current_time_ns =
      ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 +
       (uint64_t)(t.tv_nsec)); // UTC
  //  current_time_ns += static_cast<uint64_t>(6*60*60) *
  //  static_cast<uint64_t>(1e9); //+6h UTC time

  eka_write(dev_, 0xf0300 + 0 * 8,
            current_time_ns); // data, last write commits
                              // the change
  eka_write(dev_, 0xf0300 + 1 * 8,
            0x0); // data, last write commits the change
  eka_write(dev_, 0xf0300 + 2 * 8,
            0x0); // data, last write commits the change

  return 1;
}
/* --------------------------------------------------- */

void EkaEurStrategy::runLoop(
    const EkaBcRunCtx *pEkaBcRunCtx) {
  setThreadAffinityName(pthread_self(), "EkalineBookLoop",
                        dev_->affinityConf.bookThreadCpuId);
  EKA_LOG("Running EkaEurStrategy::runLoop()");

  while (active_) {
    for (auto coreId = 0; coreId < EFC_MAX_CORES;
         coreId++) {
      auto ch = udpChannel_[coreId];
      if (!ch)
        continue;
      if (!ch->has_data())
        continue;

      auto pkt = ch->get();
      if (!pkt)
        on_error("!pkt");

      EkaIpHdr *ipH = (EkaIpHdr *)(pkt - 8 - 20);
      EkaUdpHdr *udpH = (EkaUdpHdr *)(pkt - 8);

      auto udpSess = findUdpSess(coreId, ipH->dest,
                                 be16toh((udpH->dest)));
      if (!udpSess) {
#ifdef _EKA_PARSER_PRINT_ALL_
        EKA_LOG("%s:%u packet does not belog to our UDP "
                "sessions",
                EKA_IP2STR((ipH->dest)),
                be16toh(udpH->dest));
#endif
        ch->next();
        continue;
      }
      uint payloadSize = ch->getPayloadLen();
#ifdef _EKA_PARSER_PRINT_ALL_
      //    EKA_LOG("Pkt: %u byte ",payloadSize);
#endif
      MdOut mdOut = {};
      parser_->processPkt(&mdOut,
                          ProcessAction::UpdateBookOnly,
                          pkt, payloadSize);
#if 0
      if (!udpSess->isCorrectSeq(mdOut.pktSeq,
                                 mdOut.msgNum)) {
        /* on_error("Sequence Gap"); */
      }
#endif
      ch->next();
    } // per CoreId for loop
  }   // while (active_)
  terminated_ = true;
}

/* --------------------------------------------------- */

/* --------------------------------------------------- */
EkaEobiBook *
EkaEurStrategy::findBook(ExchSecurityId secId) {
  for (auto i = 0; i < nSec_; i++) {
    if (!prod_[i])
      on_error("!prod_[%d]", i);
    if (prod_[i]->isBook_ && prod_[i]->secId_ == secId)
      return prod_[i]->book_;
  }
  return nullptr;
}
/* --------------------------------------------------- */

void EkaEurStrategy::arm(EkaBcSecHandle prodHande,
                         bool armBid, bool armAsk,
                         EkaBcArmVer ver) {
#if 0
assign rf_arm_product         = register_write_data[0  +:  4];
assign rf_arm_decide_buy_bit  = register_write_data[8  +:  1]; //1-arm, 0--disarm
assign rf_arm_decide_sell_bit = register_write_data[9  +:  1];  //1-arm, 0--disarm
assign rf_arm_version         = register_write_data[32 +: 32]; //only for arm==1

address 0xf07c8
#endif

  if (prodHande < 0 || prodHande >= MaxSecurities_)
    on_error("Bad prodHande %jd", prodHande);

  uint64_t armData = 0;

  armData |= prodHande & 0x0F; // 4 bits

  armData |= (armBid << 8);
  armData |= (armAsk << 9);

  if (armBid || armAsk)
    armData |= (static_cast<uint64_t>(ver) << 32);

  eka_write(0xf07c8, armData);
}

/* --------------------------------------------------- */
static inline void getDepthData(EkaEobiBook *b, SIDE side,
                                EobiDepthParams *dst,
                                uint depth) {
  uint32_t accSize = 0;
  for (uint i = 0; i < depth; i++) {
    dst->entry[i].price = b->getEntryPrice(side, i);
    accSize += b->getEntrySize(side, i);
    dst->entry[i].size = accSize;
  }
}
/* ----------------------------------------------------- */
static inline uint16_t price2Norm(uint64_t _price,
                                  uint64_t step,
                                  uint64_t bottom) {
  if (_price == 0)
    return 0;
  if (unlikely(_price < bottom))
    on_error("price %ju < bottom %ju", _price, bottom);
  return (_price - bottom) / step;
}

/* --------------------------------------------------- */

void EkaEurStrategy::onTobChange(MdOut *mdOut,
                                 EkaEobiBook *book,
                                 SIDE side) {
#if 1
  EobiHwBookParams tobParams = {};

  auto *prod = book->prod_;

  uint64_t tobPrice =
      static_cast<uint64_t>(book->getEntryPrice(side, 0));
  uint64_t step = book->step_;
  uint64_t bottom = book->bottom_;

  tobParams.tob.last_transacttime = mdOut->transactTime;
  tobParams.tob.price = tobPrice;
  tobParams.tob.size = book->getEntrySize(side, 0);
  tobParams.tob.msg_seq_num = mdOut->sequence;

  tobParams.tob.normprice =
      price2Norm(tobPrice, step, bottom);

  tobParams.tob.firePrice = tobPrice;

  uint64_t betterPrice = tobPrice == 0 ? 0
                         : side == SIDE::BID
                             ? tobPrice + step
                             : tobPrice - step;

  tobParams.tob.fireBetterPrice = betterPrice;

  getDepthData(book, side, &tobParams.depth,
               HW_DEPTH_PRICES);

// #ifdef _EKA_PARSER_PRINT_ALL_
#if 1
  EKA_LOG("TOB changed price=%ju size=%u normprice=%d",
          tobParams.tob.price, tobParams.tob.size,
          tobParams.tob.normprice);
#endif

#ifndef PCAP_TEST
  ekaWriteTob(prod->handle_, &tobParams, side);
#endif
#endif
}
/* --------------------------------------------------- */
