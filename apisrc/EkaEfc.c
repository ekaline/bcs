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
#include "EpmQEDFireTemplate.h"

void ekaFireReportThread(EkaDev *dev);

/* ################################################ */

/* ################################################ */
EkaEfc::EkaEfc(EkaEpm *epm, epm_strategyid_t id,
               epm_actionid_t baseActionIdx,
               const EpmStrategyParams *params,
               EfhFeedVer _hwFeedVer)
    : EpmStrategy(epm, id, baseActionIdx, params,
                  _hwFeedVer) {

  //  eka_write(dev,STAT_CLEAR   ,(uint64_t) 1);
  disableRxFire();
  eka_write(dev, P4_STRAT_CONF, (uint64_t)0);

  hwFeedVer = dev->efcFeedVer;
  EKA_LOG("Creating EkaEfc: hwFeedVer=%s (%d)",
          EKA_FEED_VER_DECODE(hwFeedVer), (int)hwFeedVer);

  for (auto i = 0; i < EFC_SUBSCR_TABLE_ROWS; i++) {
    hashLine[i] = new EkaHwHashTableLine(dev, hwFeedVer, i);
    if (hashLine[i] == NULL)
      on_error("hashLine[%d] == NULL", i);
  }

#ifndef _VERILOG_SIM
  cleanSubscrHwTable();
  cleanSecHwCtx();
  eka_write(dev, SCRPAD_EFC_SUBSCR_CNT, 0);
#endif

  switch (hwFeedVer) {
  case EfhFeedVer::kNASDAQ:
    epm->hwFire =
        new EpmFireSqfTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmFireSqfTemplate");
    ehp = new EhpNom(dev);
    break;
  case EfhFeedVer::kCBOE:
    /* epm->hwFire  = new
     * EpmFireBoeTemplate(epm->templatesNum++); */
    /* EKA_LOG("Initializing EpmFireBoeTemplate"); */
    epm->hwFire = new EpmBoeQuoteUpdateShortTemplate(
        epm->templatesNum++);
    EKA_LOG("Initializing EpmBoeQuoteUpdateShortTemplate");
    epm->hwCancel =
        new EpmCancelBoeTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCancelBoeTemplate");
    ehp = new EhpPitch(dev);
    break;
  case EfhFeedVer::kCME:
    epm->hwFire =
        new EpmCmeILinkTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCmeILinkTemplate");
    epm->swFire =
        new EpmCmeILinkSwTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCmeILinkSwTemplate");
    epm->cmeHb =
        new EpmCmeILinkHbTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCmeILinkHbTemplate");
    epm->DownloadSingleTemplate2HW(epm->cmeHb);
    ehp = new EhpCmeFC(dev);
    break;
  case EfhFeedVer::kQED:
    epm->hwFire = new EpmQEDFireTemplate(
        epm->templatesNum++); // TBD raw tcp
    EKA_LOG("Initializing hwFire EpmFireSqfTemplate (TBD) "
            "for QED");
    ehp = new EhpQED(dev);
    break;
  case EfhFeedVer::kNEWS:
    epm->hwFire =
        new EpmCmeILinkTemplate(epm->templatesNum++); // TBD
    EKA_LOG(
        "Initializing dummy EpmCmeILinkTemplate (news)");
    epm->cmeHb =
        new EpmCmeILinkHbTemplate(epm->templatesNum++);
    EKA_LOG(
        "Initializing dummy EpmCmeILinkHbTemplate"); // TBD
    ehp = new EhpNews(dev);
    break;
  case EfhFeedVer::kITCHFS:
    epm->hwFire = new EpmFastSweepUDPReactTemplate(
        epm->templatesNum++);
    EKA_LOG("Initializing fast sweep");
    ehp = new EhpItchFS(dev);
    break;
  default:
    on_error("Unexpected EFC HW Version: %d",
             (int)hwFeedVer);
  }
  epm->DownloadSingleTemplate2HW(epm->hwFire);
  if (epm->swFire)
    epm->DownloadSingleTemplate2HW(epm->swFire);

  if (ehp) {
    ehp->init();
    ehp->download2Hw();
    initHwRoundTable();
  }
#if EFC_CTX_SANITY_CHECK
  secIdList = new uint64_t[EFC_SUBSCR_TABLE_ROWS *
                           EFC_SUBSCR_TABLE_COLUMNS];
  if (secIdList == NULL)
    on_error("secIdList == NULL");
  memset(secIdList, 0,
         EFC_SUBSCR_TABLE_ROWS * EFC_SUBSCR_TABLE_COLUMNS);
#endif
  auto swStatistics = eka_read(dev, SW_STATISTICS);
  eka_write(dev, SW_STATISTICS,
            swStatistics | (1ULL << 63));
}
/* ################################################ */
EkaEfc::~EkaEfc() {
  disArmController();
  disableRxFire();
  eka_write(dev, P4_STRAT_CONF, (uint64_t)0);
  auto swStatistics = eka_read(dev, SW_STATISTICS);
  eka_write(dev, SW_STATISTICS,
            swStatistics & ~(1ULL << 63));
}

/* ################################################ */

/* ################################################ */
int EkaEfc::armController(EfcArmVer ver) {
  EKA_LOG("Arming EFC");
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev, P4_ARM_DISARM, armData);
  return 0;
}
/* ################################################ */
int EkaEfc::disArmController() {
  EKA_LOG("Disarming EFC");
  eka_write(dev, P4_ARM_DISARM, 0);
  return 0;
}
/* ################################################ */
int EkaEfc::initStratGlobalParams(
    const EfcStratGlobCtx *ctx) {
  EKA_LOG("Initializing EFC global params");
  memcpy(&stratGlobCtx, ctx, sizeof(EfcStratGlobCtx));
  eka_write(dev, P4_ARM_DISARM, 0);
  return 0;
}
/* ################################################ */
EkaUdpSess *EkaEfc::findUdpSess(EkaCoreId coreId,
                                uint32_t mcAddr,
                                uint16_t mcPort) {
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL)
      on_error("udpSess[%d] == NULL", i);
    if (udpSess[i]->myParams(coreId, mcAddr, mcPort))
      return udpSess[i];
  }
  return NULL;
}

/* ################################################ */
int EkaEfc::disableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev, ENABLE_PORT);
  uint8_t tcpCores =
      dev->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores =
      dev->ekaHwCaps->hwCaps.core.bitmap_md_cores;

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
  eka_write(dev, ENABLE_PORT, fire_rx_tx_en);

  EKA_LOG("Disabling Fire and EFC Parser: fire_rx_tx_en = "
          "0x%016jx",
          fire_rx_tx_en);
  return 0;
}

/* ################################################ */
int EkaEfc::enableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev, ENABLE_PORT);
  uint8_t tcpCores =
      dev->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores =
      dev->ekaHwCaps->hwCaps.core.bitmap_md_cores;

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

  eka_write(dev, ENABLE_PORT, fire_rx_tx_en);
  EKA_LOG("Enabling Fire and EFC Parser: fire_rx_tx_en = "
          "0x%016jx",
          fire_rx_tx_en);
  return 0;
}
/* ################################################ */
int EkaEfc::checkSanity() {
  for (auto i = 0; i < EkaEpm::MAX_UDP_SESS; i++) {
    if (udpSess[i] == NULL)
      continue;
    if (!action[i]->initialized)
      on_error("EFC Trigger (UDP Session) #%d "
               "does not have initialized Action",
               i);
  }
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

  setHwGlobalParams();
  setHwUdpParams();
  /* if (hwFeedVer != EfhFeedVer::kCME) */
  /*   setHwStratRegion(); */
  //  igmpJoinAll();

  enableRxFire();

  if (dev->fireReportThreadActive) {
    on_error("fireReportThread already active");
  }

  dev->fireReportThread =
      std::thread(ekaFireReportThread, dev);
  dev->fireReportThread.detach();
  while (!dev->fireReportThreadActive)
    sleep(0);
  EKA_LOG("fireReportThread activated");

  return 0;
}

/* ################################################ */
int EkaEfc::setHwGlobalParams() {
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

  eka_write(dev, P4_GLOBAL_MAX_SIZE, stratGlobCtx.max_size);

  uint64_t p4_strat_conf = eka_read(dev, P4_STRAT_CONF);
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

  eka_write(dev, P4_STRAT_CONF, p4_strat_conf);
  eka_write(dev, P4_WATCHDOG_CONF, p4_watchdog_period);

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
    eka_write(dev, FH_GROUP_IPPORT, tmp_ipport);
  }

  EKA_LOG("downloading %d MC sessions to FPGA", numUdpSess);
  for (auto i = 0; i < numUdpSess; i++) {
    if (!udpSess[i])
      on_error("!udpSess[%d]", i);

    EKA_LOG(
        "configuring IP:UDP_PORT %s:%u for MD for group:%d",
        EKA_IP2STR(udpSess[i]->ip), udpSess[i]->port, i);
    uint32_t ip = udpSess[i]->ip;
    uint16_t port = udpSess[i]->port;

    uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                          ((uint64_t)port) << 32 |
                          be32toh(ip);
    //  EKA_LOG("HW Port-IP register = 0x%016jx (%x : %x)",
    //  tmp_ipport,ip,port);
    eka_write(dev, FH_GROUP_IPPORT, tmp_ipport);
  }
  return 0;
}

/* ################################################ */
