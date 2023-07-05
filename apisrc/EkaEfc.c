#include <arpa/inet.h>
#include <thread>

#include "EhpCmeFC.h"
#include "EhpItchFS.h"
#include "EhpNews.h"
#include "EhpNom.h"
#include "EhpPitch.h"
#include "EhpQED.h"
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
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"
#include "EpmBoeQuoteUpdateShortTemplate.h"
#include "EpmCancelBoeTemplate.h"
#include "EpmCmeILinkHbTemplate.h"
#include "EpmCmeILinkSwTemplate.h"
#include "EpmCmeILinkTemplate.h"
#include "EpmFastSweepUDPReactTemplate.h"
#include "EpmFireBoeTemplate.h"
#include "EpmFireSqfTemplate.h"
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

  EKA_LOG("Clearing %ju Efc Actions",
          EkaEpmRegion::NumEfcActions);
  for (auto i = 0; i < EkaEpmRegion::NumEfcActions; i++) {
    epm_action_t emptyAction = {};

    auto globalIdx = EkaEpmRegion::getBaseActionIdx(
                         EkaEpmRegion::Regions::Efc) +
                     i;

    copyBuf2Hw(dev_, EkaEpm::EpmActionBase,
               (uint64_t *)&emptyAction,
               sizeof(emptyAction));
    atomicIndirectBufWrite(dev_, 0xf0238, 0, 0, globalIdx,
                           0);
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
void EkaEfc::armP4(EfcArmVer ver) { armController(ver); }

/* ################################################ */
void EkaEfc::disarmP4() { disArmController(); }

/* ################################################ */
int EkaEfc::initStratGlobalParams(
    const EfcStratGlobCtx *ctx) {
  EKA_LOG("Initializing EFC global params");
  memcpy(&stratGlobCtx, ctx, sizeof(EfcStratGlobCtx));
  eka_write(dev_, P4_ARM_DISARM, 0);
  return 0;
}
/* ################################################ */
EkaUdpSess *EkaEfc::findUdpSess(EkaCoreId coreId,
                                uint32_t mcAddr,
                                uint16_t mcPort) {
  /*   for (auto strat : {p4}) {
      if (!strat)
        continue;
      auto udpSess =
          strat->findUdpSess(coreId, mcAddr, mcPort);
      if (udpSess)
        return udpSess;
    }
    return nullptr; */

  return p4_->findUdpSess(coreId, mcAddr, mcPort);
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
    if ((0x1 << coreId) & mdCores) {
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
int EkaEfc::run(EfcCtx *pEfcCtx,
                const EfcRunCtx *pEfcRunCtx) {
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
/* ################################################ */
int EkaEfc::setHwUdpParams() {
  for (auto i = 0; i < MAX_UDP_SESS; i++) {
    uint32_t ip = 0;
    uint16_t port = 0;
    uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                          ((uint64_t)port) << 32 |
                          be32toh(ip);
    eka_write(dev_, FH_GROUP_IPPORT, tmp_ipport);
  }

  EKA_LOG("downloading %d MC sessions to FPGA",
          p4_->numUdpSess_);
  if (!p4_)
    on_error("!p4_");
  for (auto i = 0; i < p4_->numUdpSess_; i++) {
    if (!p4_->udpSess_[i])
      on_error("!udpSess[%d]", i);

    EKA_LOG(
        "configuring IP:UDP_PORT %s:%u for MD for group:%d",
        EKA_IP2STR(p4_->udpSess_[i]->ip),
        p4_->udpSess_[i]->port, i);
    uint32_t ip = p4_->udpSess_[i]->ip;
    uint16_t port = p4_->udpSess_[i]->port;

    uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                          ((uint64_t)port) << 32 |
                          be32toh(ip);
    //  EKA_LOG("HW Port-IP register = 0x%016jx (%x : %x)",
    //  tmp_ipport,ip,port);
    eka_write(dev_, FH_GROUP_IPPORT, tmp_ipport);
  }
  return 0;
}

/* ################################################ */
