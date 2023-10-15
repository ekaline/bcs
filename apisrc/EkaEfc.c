#include <arpa/inet.h>
#include <thread>

#include "EhpCmeFC.h"
#include "EhpItchFS.h"
#include "EhpNews.h"
#include "EhpNom.h"
#include "EhpPitch.h"
#include "EkaCmeFcStrategy.h"

#include "EkaEurStrategy.h"

#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEfcDataStructs.h"
#include "EkaEpm.h"
#include "EkaEpmAction.h"
#include "EkaHwCaps.h"
#include "EkaHwHashTableLine.h"
#include "EkaIgmp.h"
#include "EkaP4Strategy.h"
#include "EkaQedStrategy.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"
#include "EkaUserReportQ.h"

void ekaFireReportThread(EkaDev *dev);

extern EkaDev *g_ekaDev;

/* ################################################ */

/* ################################################ */
EkaEfc::EkaEfc(const EfcInitCtx *pEfcInitCtx) {
  dev_ = g_ekaDev;
  if (!dev_ || !dev_->epm)
    on_error("!dev_ || !dev_->epm");
  epm_ = dev_->epm;

  //  eka_write(dev_,STAT_CLEAR   ,(uint64_t) 1);
  epm_->createRegion(EkaEpmRegion::Regions::Efc);
  epm_->createRegion(EkaEpmRegion::Regions::EfcMc);
  report_only_ = pEfcInitCtx->report_only;
  watchdog_timeout_sec_ = pEfcInitCtx->watchdog_timeout_sec;
  EKA_LOG("EFC is created with: report_only=%d, "
          "watchdog_timeout_sec = %ju",
          report_only_, watchdog_timeout_sec_);

  uint64_t p4_strat_conf = eka_read(dev_, P4_STRAT_CONF);
  uint64_t p4_watchdog_period = EKA_WATCHDOG_SEC_VAL;

  if (report_only_)
    p4_strat_conf |= EKA_P4_REPORT_ONLY_BIT;
  /*   if (stratGlobCtx.debug_always_fire)
      p4_strat_conf |= EKA_P4_ALWAYS_FIRE_BIT;
    if (stratGlobCtx.debug_always_fire_on_unsubscribed)
      p4_strat_conf |= EKA_P4_ALWAYS_FIRE_UNSUBS_BIT; */
  /* if (stratGlobCtx.auto_rearm == 1)  */
  /*   p4_strat_conf |= EKA_P4_AUTO_REARM_BIT; */
  if (watchdog_timeout_sec_ != 0)
    p4_watchdog_period =
        EKA_WATCHDOG_SEC_VAL * watchdog_timeout_sec_;

  eka_write(dev_, P4_STRAT_CONF, p4_strat_conf);
  eka_write(dev_, P4_WATCHDOG_CONF, p4_watchdog_period);

  userReportQ = new EkaUserReportQ(dev_);
  if (!userReportQ)
    on_error("Failed on new EkaUserReportQ");

  // Clearing EHP
  uint8_t mdCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES;
       coreId++) {
    if ((0x1 << coreId) & mdCores) {
      uint64_t base = 0x8a000 + coreId * 0x1000;
      uint8_t hwMaxEhpTemplate[4 * 1024] = {};

      EKA_LOG("Clearing Ehp templates, base=0x%jx, "
              "coreId=%u, size=%ju",
              base, coreId, sizeof(hwMaxEhpTemplate));

      copyBuf2Hw(dev_, base, (uint64_t *)&hwMaxEhpTemplate,
                 sizeof(hwMaxEhpTemplate));
    }
  }
}
/* ################################################ */
EkaEfc::~EkaEfc() {
  disArmController();
  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);
  auto swStatistics = eka_read(dev_, SW_STATISTICS);
  eka_write(dev_, SW_STATISTICS,
            swStatistics & ~(1ULL << 63));
}

/* ################################################ */
void EkaEfc::initP4(const EfcUdpMcParams *mcParams,
                    const EfcP4Params *p4Params) {
  p4_ = new EkaP4Strategy(mcParams, p4Params);

  if (totalCoreIdBitmap_ & p4_->getCoreBitmap())
    on_error("P4 cores 0x%x collide with previously "
             "allocated 0x%x",
             p4_->getCoreBitmap(), totalCoreIdBitmap_);

  totalCoreIdBitmap_ |= p4_->getCoreBitmap();
}
/* ################################################ */
void EkaEfc::initQed(const EfcUdpMcParams *mcParams,
                     const EfcQedParams *qedParams) {
  qed_ = new EkaQedStrategy(mcParams, qedParams);

  if (totalCoreIdBitmap_ & qed_->getCoreBitmap())
    on_error(
        "Qed cores bitmap 0x%x collide with previously "
        "allocated 0x%x",
        qed_->getCoreBitmap(), totalCoreIdBitmap_);

  totalCoreIdBitmap_ |= qed_->getCoreBitmap();
}
/* ################################################ */
void EkaEfc::initCmeFc(const EfcUdpMcParams *mcParams,
                       const EfcCmeFcParams *cmeParams) {
  cme_ = new EkaCmeFcStrategy(mcParams, cmeParams);

  if (totalCoreIdBitmap_ & cme_->getCoreBitmap())
    on_error(
        "CmeFc cores bitmap 0x%x collide with previously "
        "allocated 0x%x",
        cme_->getCoreBitmap(), totalCoreIdBitmap_);

  totalCoreIdBitmap_ |= cme_->getCoreBitmap();
}

/* ################################################ */
void EkaEfc::initEur() {
  eur_ = new EkaEurStrategy();

  if (totalCoreIdBitmap_ & eur_->getCoreBitmap())
    on_error(
        "Eur cores bitmap 0x%x collide with previously "
        "allocated 0x%x",
        eur_->getCoreBitmap(), totalCoreIdBitmap_);

  totalCoreIdBitmap_ |= eur_->getCoreBitmap();
}

/* ################################################ */

void EkaEfc::qedSetFireAction(epm_actionid_t fireActionId,
                              int productId) {
  if (!qed_)
    on_error("Qed is not initialized. Run "
             "efcInitQedStrategy()");
  qed_->setFireAction(fireActionId, productId);
}
/* ################################################ */

void EkaEfc::cmeFcSetFireAction(
    epm_actionid_t fireActionId) {
  if (!cme_)
    on_error("CmeFc is not initialized. Run "
             "efcInitCmeFcStrategy()");
  cme_->setFireAction(fireActionId);
}
/* ################################################ */

void EkaEfc::bcCmeFcSetFireAction(
    epm_actionid_t fireActionId) {
  if (!bcCme_)
    on_error("BcCmeFc is not initialized. Run "
             "ekaBcInitCmeFcStrategy()");
  bcCme_->setFireAction(fireActionId);
}
/* ################################################ */
int EkaEfc::armController(EfcArmVer ver) {
  EKA_LOG("Arming EFC");
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev_, P4_ARM_DISARM, armData);
  return 0;
}
/* ################################################ */
int EkaEfc::disArmController() {
  EKA_LOG("Disarming EFC");
  eka_write(dev_, P4_ARM_DISARM, 0);
  return 0;
}
/* ################################################ */
void EkaEfc::armP4(EfcArmVer ver) {
  if (!p4_)
    on_error(
        "P4 is not initialized. Run efcInitP4Strategy()");
  p4_->arm(ver);
}

/* ################################################ */
void EkaEfc::disarmP4() {
  if (!p4_)
    on_error(
        "P4 is not initialized. Run efcInitP4Strategy()");
  p4_->disarm();
}
/* ################################################ */
void EkaEfc::armQed(EfcArmVer ver) {
  if (!qed_)
    on_error("Qed is not initialized. Run "
             "efcInitQedStrategy()");
  qed_->arm(ver);
}

/* ################################################ */
void EkaEfc::disarmQed() {
  if (!qed_)
    on_error("Qed is not initialized. Run "
             "efcInitQedStrategy()");
  qed_->disarm();
}
/* ################################################ */
void EkaEfc::armCmeFc(EfcArmVer ver) {
  if (!cme_)
    on_error("CmeFc is not initialized. Run "
             "efcInitCmeFcStrategy()");
  cme_->arm(ver);
}

/* ################################################ */
void EkaEfc::disarmCmeFc() {
  if (!cme_)
    on_error("CmeFc is not initialized. Run "
             "efcInitCmeFcStrategy()");
  cme_->disarm();
}
/* ################################################ */
void EkaEfc::armBcCmeFc(EfcArmVer ver) {
  if (!bcCme_)
    on_error("BcCmeFc is not initialized. Run "
             "ekaBcInitCmeFcStrategy()");
  bcCme_->arm(ver);
}
/* ################################################ */
void EkaEfc::disarmBcCmeFc() {
  if (!bcCme_)
    on_error("BcCmeFc is not initialized. Run "
             "ekaBcInitCmeFcStrategy()");
  bcCme_->disarm();
}
/* ################################################ */
void EkaEfc::armEur(EfcArmVer ver) {
  if (!eur_)
    on_error("Eur is not initialized. Run "
             "ekaBcInitEurStrategy()");
  eur_->arm(ver);
}
/* ################################################ */
void EkaEfc::disarmEur() {
  if (!eur_)
    on_error("Eur is not initialized. Run "
             "ekaBcInitEurStrategy()");
  eur_->disarm();
}
/* ################################################ */
#if 0
int EkaEfc::initStratGlobalParams(
    const EfcStratGlobCtx *ctx) {
  EKA_LOG("Initializing EFC global params");
  memcpy(&stratGlobCtx, ctx, sizeof(EfcStratGlobCtx));
  eka_write(dev_, P4_ARM_DISARM, 0);
  return 0;
}
#endif
/* ################################################ */
EkaUdpSess *EkaEfc::findUdpSess(EkaCoreId coreId,
                                uint32_t mcAddr,
                                uint16_t mcPort) {

  EkaStrategy *strategies[] = {p4_, qed_, cme_};

  for (auto const &strat : strategies) {
    if (!strat)
      continue;
    auto udpSess =
        strat->findUdpSess(coreId, mcAddr, mcPort);
    if (udpSess)
      return udpSess;
  }
  return nullptr;
}

/* ################################################ */
int EkaEfc::disableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev_, ENABLE_PORT);
  uint8_t tcpCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES;
       coreId++) {
    if ((0x1 << coreId) & tcpCores) {
      fire_rx_tx_en &=
          ~(1ULL << (16 + coreId)); // disable fire
    }
    if ((0x1 << coreId) & mdCores) {
      fire_rx_tx_en &=
          ~(1ULL << (coreId)); // RX (Parser) core disable
    }
  }
  eka_write(dev_, ENABLE_PORT, fire_rx_tx_en);

  EKA_LOG("Disabling Fire and EFC Parser: fire_rx_tx_en = "
          "0x%016jx",
          fire_rx_tx_en);
  return 0;
}

/* ################################################ */
int EkaEfc::enableRxFire() {
  EkaStrategy *strategies[] = {p4_, qed_, cme_};

  uint8_t rxCoresBitmap = 0;

  for (auto const &strat : strategies) {
    if (!strat)
      continue;
    rxCoresBitmap |= strat->coreIdBitmap_;
  }

  uint64_t fire_rx_tx_en = eka_read(dev_, ENABLE_PORT);
  uint8_t tcpCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES;
       coreId++) {
    if ((0x1 << coreId) & tcpCores) {
      fire_rx_tx_en |= 1ULL
                       << (16 + coreId); // fire core enable
    }
    if ((0x1 << coreId) & rxCoresBitmap & mdCores) {
      fire_rx_tx_en |= 1ULL
                       << coreId; // RX (Parser) core enable
    }
  }

  eka_write(dev_, ENABLE_PORT, fire_rx_tx_en);
  EKA_LOG("Enabling Fire and EFC Parser: fire_rx_tx_en = "
          "0x%016jx",
          fire_rx_tx_en);
  return 0;
}
/* ################################################ */
int EkaEfc::checkSanity() {
  /*   for (auto i = 0; i < EkaEpm::MAX_UDP_SESS; i++) {
      if (udpSess[i] == NULL)
        continue;
      if (!action[i]->initialized)
        on_error("EFC Trigger (UDP Session) #%d "
                 "does not have initialized Action",
                 i);
    } */
  return 0;
}

/* ################################################ */
int EkaEfc::run(const EfcRunCtx *pEfcRunCtx) {
  // TO BE FIXED!!!
  //  checkSanity();

  reportCb = pEfcRunCtx->onEfcFireReportCb
                 ? pEfcRunCtx->onEfcFireReportCb
                 : efcPrintFireReport;
  cbCtx = pEfcRunCtx->cbCtx;

  // setHwGlobalParams();
  setHwUdpParams();
  /* if (hwFeedVer != EfhFeedVer::kCME) */
  /*   setHwStratRegion(); */
  //  igmpJoinAll();

  enableRxFire();

  if (dev_->fireReportThreadActive) {
    on_error("fireReportThread already active");
  }

  dev_->fireReportThread =
      std::thread(ekaFireReportThread, dev_);
  dev_->fireReportThread.detach();
  while (!dev_->fireReportThreadActive)
    sleep(0);
  EKA_LOG("fireReportThread activated");

  return 0;
}

/* ################################################ */
#if 0
int EkaEfc::setHwGlobalParams() {
  on_error("Should not be called!");
  // kept for reference only
  EKA_LOG("enable_strategy=%d, report_only=%d, "
          "debug_always_fire=%d, "
          "debug_always_fire_on_unsubscribed=%d, "
          "max_size=%d, watchdog_timeout_sec=%ju",
          stratGlobCtx.enable_strategy,
          stratGlobCtx.report_only,
          stratGlobCtx.debug_always_fire,
          stratGlobCtx.debug_always_fire_on_unsubscribed,
          stratGlobCtx.max_size,
          stratGlobCtx.watchdog_timeout_sec);

  eka_write(dev_, P4_GLOBAL_MAX_SIZE,
            stratGlobCtx.max_size);

  uint64_t p4_strat_conf = eka_read(dev_, P4_STRAT_CONF);
  uint64_t p4_watchdog_period = EKA_WATCHDOG_SEC_VAL;

  if (stratGlobCtx.report_only)
    p4_strat_conf |= EKA_P4_REPORT_ONLY_BIT;
  if (stratGlobCtx.debug_always_fire)
    p4_strat_conf |= EKA_P4_ALWAYS_FIRE_BIT;
  if (stratGlobCtx.debug_always_fire_on_unsubscribed)
    p4_strat_conf |= EKA_P4_ALWAYS_FIRE_UNSUBS_BIT;
  /* if (stratGlobCtx.auto_rearm == 1)  */
  /*   p4_strat_conf |= EKA_P4_AUTO_REARM_BIT; */
  if (stratGlobCtx.watchdog_timeout_sec != 0)
    p4_watchdog_period = EKA_WATCHDOG_SEC_VAL *
                         stratGlobCtx.watchdog_timeout_sec;

  eka_write(dev_, P4_STRAT_CONF, p4_strat_conf);
  eka_write(dev_, P4_WATCHDOG_CONF, p4_watchdog_period);

  return 0;
}
#endif
/* ################################################ */
int EkaEfc::setHwUdpParams() {
  const int HwUdpMcConfig =
      0xf0500; // base, every core: + 8

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    if (!((1 << coreId) &
          dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores))
      continue;

    for (auto i = 0; i < EFC_MAX_MC_GROUPS_PER_LANE; i++) {
      // Cleaning all MC groups
      uint32_t ip = 0;
      uint16_t port = 0;
      uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                            ((uint64_t)port) << 32 |
                            be32toh(ip);
      eka_write(dev_, HwUdpMcConfig + coreId * 8,
                tmp_ipport);
    }

    EkaStrategy *strategies[] = {p4_, qed_, cme_};

    for (auto const &strat : strategies) {
      if (!strat)
        continue;
      if (strat->mcCoreSess_[coreId].numUdpSess)
        EKA_LOG("%s: downloading %d MC sessions for coreId "
                "%d to FPGA",
                strat->name_.c_str(),
                strat->mcCoreSess_[coreId].numUdpSess,
                coreId);

      for (auto i = 0;
           i < strat->mcCoreSess_[coreId].numUdpSess; i++) {
        if (!strat->mcCoreSess_[coreId].udpSess[i])
          on_error("!udpSess[%d][%d]", coreId, i);

        auto ip = strat->mcCoreSess_[coreId].udpSess[i]->ip;
        auto port =
            strat->mcCoreSess_[coreId].udpSess[i]->port;

        EKA_LOG(
            "configuring CoreId %d IP:UDP_PORT %s:%u for "
            "MD for group: %d",
            coreId, EKA_IP2STR(ip), port, i);

        uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                              ((uint64_t)port) << 32 |
                              be32toh(ip);
        //  EKA_LOG("HW Port-IP register = 0x%016jx (%x :
        //  %x)", tmp_ipport,ip,port);
        eka_write(dev_, HwUdpMcConfig + coreId * 8,
                  tmp_ipport);
      }
    }
  }
  return 0;
}

/* ################################################ */
