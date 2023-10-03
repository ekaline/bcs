// SHURIK

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#include "Efh.h"
#include "Eka.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpm.h"
#include "EkaEpmRegion.h"
#include "EkaFh.h"
#include "EkaFhRunGroup.h"
#include "EkaHwCaps.h"
#include "EkaHwInternalStructs.h"
#include "EkaIgmp.h"
#include "EkaSnDev.h"
#include "EkaUdpChannel.h"
#include "EkaUserChannel.h"
#include "EkaUserReportQ.h"
#include "EkaWc.h"
#include "eka_hw_conf.h"

extern FILE *g_ekaLogFile;
extern EkaLogCallback g_ekaLogCB;

int ekaDefaultLog(void * /*unused*/, const char *function,
                  const char *file, int line, int priority,
                  const char *format, ...);
int ekaDefaultCreateThread(const char *name,
                           EkaServiceType type,
                           void *(*threadRoutine)(void *),
                           void *arg, void *context,
                           uintptr_t *handle);
/* OnEfcFireReportCb* efcDefaultOnFireReportCb (EfcCtx*
 * efcCtx, const EfcFireReport* efcFireReport, size_t size);
 */
/* OnEkaExceptionReportCb*
 * efhDefaultOnException(EkaExceptionReport* msg,
 * EfhRunUserData efhRunUserData); */
// void eka_write(EkaDev* dev, uint64_t addr, uint64_t val);
// uint64_t eka_read(eka_dev_t* dev, uint64_t addr);
bool eka_is_all_zeros(const void *buf, ssize_t size);
void eka_close_tcp(EkaDev *pEkaDev);

void ekaInitLwip(EkaDev *dev);
void setNetifIpMacSa(EkaDev *dev, EkaCoreId coreId,
                     const uint8_t *macSa);
void setNetifIpSrc(EkaDev *dev, EkaCoreId coreId,
                   const uint32_t *srcIp);
void setNetifGw(EkaDev *dev, EkaCoreId coreId,
                const uint32_t *pGwIp);
void setNetifNetmask(EkaDev *dev, EkaCoreId coreId,
                     const uint32_t *pNetmaskIp);
int ekaAddArpEntry(EkaDev *dev, EkaCoreId coreId,
                   const uint32_t *protocolAddr,
                   const uint8_t *hwAddr);

uint32_t getIfIp(const char *ifName);

void ekaServThread(EkaDev *dev);
void ekaTcpRxThread(EkaDev *dev);

/* #####################################################################
 */
static EfhFeedVer feedVer(int hwFeedVer) {
  switch (hwFeedVer) {
  case SN_NASDAQ:
    return EfhFeedVer::kNASDAQ;
  case SN_MIAX:
    return EfhFeedVer::kMIAX;
  case SN_PHLX:
    return EfhFeedVer::kPHLX;
  case SN_GEMX:
    return EfhFeedVer::kGEMX;
  case SN_CBOE:
    return EfhFeedVer::kCBOE;
  default:
    return EfhFeedVer::kInvalid;
  }
}

/* #####################################################################
 */

static void str_time_from_nano(uint64_t current_time,
                               char *time_str) {
  int nano = current_time % 1000;
  int micro = (current_time / 1000) % 1000;
  int mili = (current_time / (1000 * 1000)) % 1000;
  int current_time_seconds =
      current_time / (1000 * 1000 * 1000);
  time_t tmp = current_time_seconds;
  struct tm lt;
  (void)localtime_r(&tmp, &lt);
  char result[32] = {};
  strftime(result, sizeof(result), "%Y-%m-%d %H:%M:%S",
           &lt);
  sprintf(time_str, " %s.%03d.%03d.%03d", result, mili,
          micro, nano);
  return;
}
/* #####################################################################
 */

/* static uint64_t getFpgaTimeCycles () { // ignores
 * Application - PCIe - FPGA latency */
/*   struct timespec t; */
/*   clock_gettime(CLOCK_REALTIME, &t); //  */
/*   uint64_t current_time_ns = ((uint64_t)(t.tv_sec) *
 * (uint64_t)1000000000 + (uint64_t)(t.tv_nsec)); */
/*   uint64_t current_time_cycles = (current_time_ns *
 * (EKA_FPGA_FREQUENCY / 1000.0)); */
/*   /\* eka_write(dev,FPGA_RT_CNTR,current_time_cycles);
 * *\/ */
/*   /\* char t_str[64] = {}; *\/ */
/*   /\* str_time_from_nano(current_time_ns,t_str); *\/ */
/*   /\* EKA_LOG("setting HW time to %s",t_str); *\/ */
/*   return current_time_cycles; */
/* } */

/* #####################################################################
 */

EkaDev::EkaDev(const EkaDevInitCtx *initCtx) {
  exc_inited = false;
  exc_active = false;
  exc_inited = false;
  lwip_inited = false;

  for (uint i = 0; i < MAX_FEED_HANDLERS; i++)
    fh[i] = NULL;
  numFh = 0;
  for (uint i = 0; i < MAX_RUN_GROUPS; i++)
    runGr[i] = NULL;
  numRunGr = 0;

  /* -------------------------------------------- */
  dev = this;

  if (initCtx == NULL)
    on_error("initCtx == NULL");

  logCB = initCtx->logCallback == NULL
              ? ekaDefaultLog
              : initCtx->logCallback;
  g_ekaLogCB = logCB;
  logCtx = initCtx->logContext == NULL
               ? stdout
               : initCtx->logContext;
  g_ekaLogFile = (FILE *)logCtx;

  credAcquire = initCtx->credAcquire;
  credRelease = initCtx->credRelease;

  credContext = initCtx->credContext;
  createThreadContext = initCtx->createThreadContext;

  createThread = initCtx->createThread == NULL
                     ? ekaDefaultCreateThread
                     : initCtx->createThread;

  snDev = new EkaSnDev(this);
  if (!snDev)
    on_error("!snDev");
  ekaWc = new EkaWc(snDevWCPtr);
  if (!ekaWc)
    on_error("!ekaWc");

  ekaHwCaps = new EkaHwCaps(snDev->dev_id);

  if (ekaHwCaps == NULL)
    on_error("ekaHwCaps == NULL");

  ekaHwCaps->print();
  ekaHwCaps->check();

  hwFeedVer = feedVer(ekaHwCaps->hwCaps.version.parser);

#ifdef _ENFORCE_NOM_
  hwFeedVer = EfhFeedVer::kNASDAQ;
#endif

  //  eka_write(ENABLE_PORT,0);

  time_t t;
  srand((unsigned)time(&t));

  midnightSystemClock = systemClockAtMidnight();

  openEpm();

  ekaIgmp = new EkaIgmp(this);
  if (ekaIgmp == NULL)
    on_error("ekaIgmp == NULL");

  bool noCores = true;
  for (uint c = 0; c < MAX_CORES; c++) {
    uint32_t ip = 0;
    uint8_t mac[6] = {};
    snDev->getIpMac(c, &ip, mac);
#ifdef XN01_LAB_TCP_TEST
    if (c == 0)
      ip = inet_addr("100.0.0.110");
    if (c == 1)
      ip = inet_addr("200.0.0.111");
#endif
    if (ip != 0 && snDev->hasLink(c)) {
      core[c] = new EkaCore(dev, c, ip, mac, epmEnabled);
      EKA_LOG("FETH%u LINK=1 %s %s", c, EKA_IP2STR(ip),
              EKA_MAC2STR(mac));
      noCores = false;
    } else {
      //      EKA_LOG("Core %u has NO link or configured
      //      IP",c);
      core[c] = NULL;
    }
  }

  if (noCores)
    on_error("No FPGA ports have Link and/or IP");

  genIfIp = INADDR_ANY;
  const char *genIf[] = {"sfc0", "sfc1", "sfc2",
                         "sfc3", "eth0", "eth1"};
  for (auto i = 0; i < (int)std::size(genIf); i++) {
    uint32_t ip = getIfIp(genIf[i]);
    if (ip == 0)
      continue;
    strcpy(genIfName, genIf[i]);
    genIfIp = ip;
    break;
  }
  EKA_LOG("genIfIp of %s = %s", genIfName,
          EKA_IP2STR(genIfIp));

  /* -------------------------------------------- */

  pEfcRunCtx = new EfcRunCtx;
  assert(pEfcRunCtx != NULL);
  pEfcRunCtx = {};

  /* pEfcRunCtx->onEkaExceptionReportCb =
   * (OnEkaExceptionReportCb) efhDefaultOnException; */
  /* pEfcRunCtx->onEfcFireReportCb      =
   * (OnEfcFireReportCb)      efcDefaultOnFireReportCb; */

  EKA_LOG("EKALINE2 LIB BUILD TIME: %s @ %s", __DATE__,
          __TIME__);
  EKA_LOG("EKALINE2 LIB GIT: %s",
          EKA__TOSTRING(LIBEKA_GIT_VER));

  print_parsed_messages = false;

  clearHw();
  eka_write(FPGA_RT_CNTR, getFpgaTimeCycles());
  eka_write(SCRPAD_SW_VER,
            EKA_CORRECT_SW_VER | hwEnabledCores);

#ifdef EFH_TIME_CHECK_PERIOD
  deltaTimeLogFile = fopen("deltaTimeLogFile.csv", "w");
  if (deltaTimeLogFile == NULL)
    on_error("failed to create deltaTimeLogFile.csv");
  fprintf(deltaTimeLogFile, "%16s,%16s,%16s,%16s\n",
          "sequence", "currTimeNs", "exchTimeNs",
          "deltaNs");

#endif
}
/* #####################################################################
 */
bool EkaDev::initEpmTx() {
  // clearing interrupts, App Seq, etc.
  eka_write(STAT_CLEAR, (uint64_t)1);

  const uint64_t MaxSizeOfStrategyConf = 0x1000;

  uint8_t strategyConfToClean[MaxSizeOfStrategyConf] = {};
  copyBuf2Hw(dev, 0x84000, (uint64_t *)strategyConfToClean,
             sizeof(strategyConfToClean));

  // dissabling TCP traffic
  for (auto coreId = 0; coreId < MAX_CORES; coreId++)
    eka_write(0xe0000 + coreId * 0x1000 + 0x200, 0);

  epmReport = new EkaUserChannel(
      dev, snDev->dev_id, EkaUserChannel::TYPE::EPM_REPORT);
  if (!epmReport || !epmReport->isOpen()) {
    EKA_ERROR("Failed to open epmReport Channel");
    return false;
  }

  epmFeedback = new EkaUserChannel(
      dev, snDev->dev_id,
      EkaUserChannel::TYPE::EPM_FEEDBACK);
  if (!epmFeedback || !epmFeedback->isOpen()) {
    EKA_ERROR("Failed to open epmFeedback Channel");
    return false;
  }

  lwipRx = new EkaUserChannel(
      dev, snDev->dev_id, EkaUserChannel::TYPE::LWIP_RX);
  if (!lwipRx || !lwipRx->isOpen()) {
    EKA_ERROR("Failed to open lwipRx Channel");
    return false;
  }

  ekaInitLwip(this);

  epm->createRegion(EkaEpmRegion::Regions::TcpTxFullPkt);
  epm->createRegion(EkaEpmRegion::Regions::TcpTxEmptyAck);

  uint64_t fire_rx_tx_en = eka_read(ENABLE_PORT);
  fire_rx_tx_en |= (1ULL << 32); // turn off tcprx
  EKA_LOG("Turning off tcprx = 0x%016jx", fire_rx_tx_en);
  eka_write(ENABLE_PORT, fire_rx_tx_en);

  userReportQ = new EkaUserReportQ(this);
  if (!userReportQ)
    on_error("Failed on new EkaUserReportQ");

  servThreadActive = false;
  servThread = std::thread(ekaServThread, this);
  servThread.detach();

  tcpRxThreadActive = false;
  tcpRxThread = std::thread(ekaTcpRxThread, this);
  tcpRxThread.detach();

  while (!tcpRxThreadActive || !tcpRxThreadActive) {
  }
  EKA_LOG("Serv and TcpRx threads activated");

  auto swStatistics = eka_read(SW_STATISTICS);
  eka_write(SW_STATISTICS, swStatistics | (1ULL << 63));
  return true;
}

/* #####################################################################
 */
bool EkaDev::checkAndSetEpmTx() {
  if (!dev)
    on_error("!dev");

  if (!dev->epmEnabled) {
    EKA_LOG("Initializing TCP functionality");

    dev->epmEnabled = dev->initEpmTx();
    if (!dev->epmEnabled)
      on_error("TX functionality is not available for this "
               "Ekaline SW instance - caught by another "
               "process");
  }
  return true;
}
/* #####################################################################
 */

bool EkaDev::openEpm() {
  ekaHwCaps->checkEpm();
  epm = new EkaEpm(this);
  if (!epm)
    on_error("!epm");
  epm->InitTemplates();
  epm->DownloadTemplates2HW();

  return true;
}

/* #####################################################################
 */

static void set_time(EkaDev *dev) { // ignores Application -
                                    // PCIe - FPGA latency
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); //
  uint64_t current_time_ns =
      ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 +
       (uint64_t)(t.tv_nsec));
  uint64_t current_time_cycles =
      (current_time_ns * (EKA_FPGA_FREQUENCY / 1000.0));
  eka_write(dev, FPGA_RT_CNTR, current_time_cycles);
  char t_str[64] = {};
  str_time_from_nano(current_time_ns, t_str);
  EKA_LOG("setting HW time to %s", t_str);
  return;
}
/* #####################################################################
 */

int EkaDev::configurePort(const EkaCoreInitCtx *pCoreInit) {
  const EkaCoreId c = pCoreInit->coreId;
  if (core[c] == NULL) {
    EKA_WARN("trying to configure not connected core %d",
             c);
    errno = ENODEV;
    return -1;
  }
  /* else if (!epmEnabled) { */
  /*   EKA_WARN("cannot configure port; EPM not enabled");
   */
  /*   errno = ENOSYS; */
  /*   return -1; */
  /* } */

  const EkaCoreInitAttrs *attrs = &pCoreInit->attrs;

  if (attrs == NULL)
    return EKA_OPRESULT__OK;
  if (!eka_is_all_zeros((void *)&attrs->src_mac_addr, 6)) {
    for (auto i = 0; i < 6; i++)
      core[c]->macSa[i] =
          ((uint8_t *)&attrs->src_mac_addr)[i];
    memcpy(&core[c]->macSa, &attrs->src_mac_addr, 6);
    //    setNetifIpMacSa(dev,c,(const
    //    uint8_t*)&attrs->src_mac_addr);
    EKA_LOG("Core %u: Setting SRC MAC: %s", c,
            EKA_MAC2STR(core[c]->macSa));

    core[c]->connectedL1Switch = true;
  }
  if (!eka_is_all_zeros((void *)&attrs->host_ip, 4)) {
    memcpy(&core[c]->srcIp, &attrs->host_ip, 4);
    //    setNetifIpSrc(dev,c,&attrs->host_ip);
    EKA_LOG("Core %u: Setting HOST IP: %s", c,
            EKA_IP2STR(core[c]->srcIp));
  }
  if (!eka_is_all_zeros((void *)&attrs->nexthop_mac, 6)) {
    memcpy(&core[c]->macDa, &attrs->nexthop_mac, 6);
    EKA_LOG("Core %u: Setting macDA: %s", c,
            EKA_MAC2STR(core[c]->macDa));
  }
  if (!eka_is_all_zeros((void *)&attrs->gateway, 4)) {
    memcpy(&core[c]->gwIp, &attrs->gateway, 4);
    //    setNetifGw(dev,c,&attrs->gateway);
    EKA_LOG("Core %u: Setting GW: %s", c,
            EKA_IP2STR(core[c]->gwIp));
    /* if (! eka_is_all_zeros((void*)&attrs->nexthop_mac,6))
     * { */
    /*   if (ekaAddArpEntry(this, c, &attrs->gateway,
     * attrs->nexthop_mac.ether_addr_octet) == -1) */
    /*     return -1; */
    /*   EKA_LOG("Core %u: Adding GW static ARP: %s",c, */
    /*           EKA_MAC2STR(attrs->nexthop_mac.ether_addr_octet));
     */
    /* } */
  } else if (!eka_is_all_zeros((void *)&attrs->nexthop_mac,
                               6)) {
    EKA_WARN("gateway MAC is specified, but gateway IP "
             "not; can't add static ARP");
    errno = EINVAL;
    return -1;
  }
  if (!eka_is_all_zeros((void *)&attrs->netmask, 4)) {
    memcpy(&core[c]->netmask, &attrs->netmask, 4);
    /* setNetifNetmask(dev,c,&attrs->netmask); */
    EKA_LOG("Core %u: Setting Netmask: %s", c,
            EKA_IP2STR(core[c]->netmask));
  }

  return 0;
}

uint8_t EkaDev::getNumFh() { return numFh; }

/* #####################################################################
 */

EkaDev::~EkaDev() {
  TEST_LOG("shutting down...");

  /* igmp_thread_active = false; */

  exc_active = false;
  servThreadActive = false;
  fireReportThreadActive = false;
  ekaIgmp->threadActive = false;

  EKA_LOG("Waiting for servThreadTerminated...");
  while (!servThreadTerminated) {
    sleep(0);
  }

  tcpRxThreadActive = false;
  fireReportThreadActive = false;
  ekaIgmp->threadActive = false;

  EKA_LOG("Waiting for tcpRxThreadTerminated...");
  while (!tcpRxThreadTerminated) {
    sleep(0);
  }

  EKA_LOG("Waiting for fireReportThreadTerminated...");
  while (!fireReportThreadTerminated) {
    sleep(0);
  }

  EKA_LOG("Waiting for igmpLoopTerminated...");
  while (!ekaIgmp->igmpLoopTerminated) {
    sleep(0);
  }

  TEST_LOG("Closing %u FHs", numFh);
  fflush(stderr);

  TEST_LOG("Closing Epm");
  dev->epm->active = false;

  auto efc{
      dynamic_cast<EkaEfc *>(epm->strategy[EFC_STRATEGY])};
  if (efc)
    delete efc;

  for (auto i = 0; i < numFh; i++) {
    if (fh[i] != NULL)
      fh[i]->stop();
  }
  for (uint i = 0; i < numRunGr; i++) {
    if (runGr[i] != NULL)
      runGr[i]->thread_active = false;
    delete runGr[i];
  }
  usleep(10);

  for (auto i = 0; i < numFh; i++) {
    if (fh[i] == NULL)
      continue;
    while (!fh[i]->terminated)
      sleep(0);
    delete fh[i];
    fh[i] = NULL;
    usleep(10);
  }

  if (epmEnabled) {
    for (uint c = 0; c < MAX_CORES; c++) {
      if (core[c]) {
        delete core[c];
        core[c] = NULL;
      }
    }
  }
#ifdef EFH_TIME_CHECK_PERIOD
  fclose(deltaTimeLogFile);
#endif

  if (epmEnabled) {
    uint64_t val = eka_read(SW_STATISTICS);
    val = val & ~(1ULL << 63); // EFC/EPM device
    eka_write(SW_STATISTICS, val);
  } else {
    uint64_t val = eka_read(SW_STATISTICS);
    val = val & ~(1ULL << 62); // EFH device
    EKA_LOG("Turning off EFH Open dev: 0x%016jx", val);
    eka_write(SW_STATISTICS, val);
  }

  delete snDev;
}
/* #####################################################################
 */

EkaTcpSess *EkaDev::findTcpSess(uint32_t srcIp,
                                uint16_t srcPort,
                                uint32_t dstIp,
                                uint16_t dstPort) {
  for (uint c = 0; c < MAX_CORES; c++) {
    if (core[c] == NULL)
      continue;
    EkaTcpSess *s = core[c]->findTcpSess(srcIp, srcPort,
                                         dstIp, dstPort);
    if (s != NULL)
      return s;
  }
  return NULL;
}
/* #####################################################################
 */

EkaTcpSess *EkaDev::findTcpSess(int sock) {
  for (uint c = 0; c < MAX_CORES; c++) {
    if (core[c] == NULL)
      continue;
    EkaTcpSess *s = core[c]->findTcpSess(sock);
    if (s != NULL)
      return s;
  }
  return NULL;
}
/* #####################################################################
 */

EkaTcpSess *EkaDev::getControlTcpSess(EkaCoreId coreId) {
  return core[coreId]->tcpSess[MAX_SESS_PER_CORE];
}
/* #####################################################################
 */

EkaCoreId EkaDev::findCoreByMacSa(const uint8_t *macSa) {
  for (EkaCoreId c = 0; c < MAX_CORES; c++) {
    if (core[c] == NULL)
      continue;
    if (memcmp(core[c]->macSa, macSa, 6) == 0)
      return c;
  }
  return 0xFF; // NO CORE FOUND
}
/* #####################################################################
 */

/* void     EkaDev::eka_write(uint64_t addr, uint64_t val) {
 */
/*   snDev->write(addr, val);  */
/* } */
/* #####################################################################
 */

/* uint64_t EkaDev::eka_read(uint64_t addr) {  */
/*   return snDev->read(addr);  */
/* } */
/* #####################################################################
 */

int EkaDev::clearHw() {
  //  eka_write(STAT_CLEAR   ,(uint64_t) 1); // Clearing HW
  //  Statistics eka_write(SW_STATISTICS,(uint64_t) 0); //
  //  Clearing SW Statistics
  //  eka_write(P4_STRAT_CONF,(uint64_t) 0); // Clearing
  //  Strategy params

  eka_read(ADDR_INTERRUPT_MAIN_RC);   // Clearing Interrupts
  eka_read(ADDR_INTERRUPT_SHADOW_RC); // Clearing Interrupts

  for (uint64_t p = 0; p < SW_SCRATCHPAD_SIZE / 8; p++)
    eka_write(SW_SCRATCHPAD_BASE + 8 * p, (uint64_t)0);

  /* const EfcCmeFastCancelStrategyConf fc_conf = {}; */
  /* copyBuf2Hw(dev,0x84000,(uint64_t
   * *)&fc_conf,sizeof(fc_conf)); */

  /* const EfcItchFastSweepStrategyConf fs_conf = {}; */
  /* copyBuf2Hw(dev,0x84000,(uint64_t
   * *)&fs_conf,sizeof(fs_conf)); */

  return 0;
}
