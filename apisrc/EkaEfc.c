#include <thread>
#include <arpa/inet.h>

#include "EkaEfc.h"
#include "EkaDev.h"
#include "EkaCore.h"
#include "EkaIgmp.h"
#include "EkaEpm.h"
#include "EkaHwHashTableLine.h"
#include "EkaUdpSess.h"
#include "EkaTcpSess.h"
#include "EkaEpmAction.h"
#include "EpmFireSqfTemplate.h"
#include "EpmFireBoeTemplate.h"
#include "EpmCmeILinkTemplate.h"
#include "EpmCmeILinkSwTemplate.h"
#include "EpmCmeILinkHbTemplate.h"
#include "EpmFastSweepUDPReactTemplate.h"
#include "EkaEfcDataStructs.h"
#include "EkaHwCaps.h"
#include "EhpNom.h"
#include "EhpPitch.h"
#include "EhpCmeFC.h"
#include "EhpNews.h"
#include "EhpItchFS.h"

void ekaFireReportThread(EkaDev* dev);

/* ################################################ */
static bool isAscii (char letter) {
  //  EKA_LOG("testing %d", letter);
  if ( (letter>='0' && letter<='9') || (letter>='a' && letter<='z') || (letter>='A' && letter<='Z') ) return true;
  return false;
}

/* ################################################ */
EkaEfc::EkaEfc(EkaEpm*                  epm, 
	       epm_strategyid_t         id, 
	       epm_actionid_t           baseActionIdx, 
	       const EpmStrategyParams* params, 
	       EfhFeedVer               _hwFeedVer) : 
EpmStrategy(epm,id,baseActionIdx,params,_hwFeedVer) {

  eka_write(dev,STAT_CLEAR   ,(uint64_t) 1);
  
  hwFeedVer = dev->efcFeedVer;
  EKA_LOG("Creating EkaEfc: hwFeedVer=%s (%d)",
	  EKA_FEED_VER_DECODE(hwFeedVer),(int)hwFeedVer);
  
  for (auto i = 0; i < EFC_SUBSCR_TABLE_ROWS; i++) {
    hashLine[i] = new EkaHwHashTableLine(dev, hwFeedVer, i);
    if (hashLine[i] == NULL) on_error("hashLine[%d] == NULL",i);
  }
  
#ifndef _VERILOG_SIM
  cleanSubscrHwTable();
  cleanSecHwCtx();
  eka_write(dev,SCRPAD_EFC_SUBSCR_CNT,0);
#endif

  switch (hwFeedVer) {
  case EfhFeedVer::kNASDAQ : 
    epm->hwFire  = new EpmFireSqfTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmFireSqfTemplate");
    ehp = new EhpNom(dev);
    break;
  case EfhFeedVer::kCBOE : 
    epm->hwFire  = new EpmFireBoeTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmFireBoeTemplate");
    ehp = new EhpPitch(dev);
    break;
  case EfhFeedVer::kCME : 
    epm->hwFire  = new EpmCmeILinkTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCmeILinkTemplate");
    epm->swFire  = new EpmCmeILinkSwTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCmeILinkSwTemplate");    
    epm->cmeHb  = new EpmCmeILinkHbTemplate(epm->templatesNum++);
    EKA_LOG("Initializing EpmCmeILinkHbTemplate");
    epm->DownloadSingleTemplate2HW(epm->cmeHb);

    ehp = new EhpCmeFC(dev);
    //ehp = NULL;
    //EKA_LOG("NO EHP for CME - using hardcoded CME Fast cancel parser");
    break;
  case EfhFeedVer::kNEWS : 
    epm->hwFire  = new EpmCmeILinkTemplate(epm->templatesNum++); //TBD
    EKA_LOG("Initializing dummy EpmCmeILinkTemplate (news)");
    epm->cmeHb  = new EpmCmeILinkHbTemplate(epm->templatesNum++);
    EKA_LOG("Initializing dummy EpmCmeILinkHbTemplate"); //TBD
    ehp = new EhpNews(dev);
    break;
  case EfhFeedVer::kITCHFS : 
    epm->hwFire  = new EpmFastSweepUDPReactTemplate(epm->templatesNum++);
    EKA_LOG("Initializing fast sweep");
    ehp = new EhpItchFS(dev);
    break;
  default :
    on_error("Unexpected EFC HW Version: %d",(int)hwFeedVer);
  }
  epm->DownloadSingleTemplate2HW(epm->hwFire);
  if (epm->swFire) epm->DownloadSingleTemplate2HW(epm->swFire);
    
  if (ehp) {
    ehp->init();
    ehp->download2Hw();
    initHwRoundTable();
  }
#if EFC_CTX_SANITY_CHECK
  secIdList = new uint64_t[EFC_SUBSCR_TABLE_ROWS * EFC_SUBSCR_TABLE_COLUMNS];
  if (secIdList == NULL) on_error("secIdList == NULL");
  memset(secIdList,0,EFC_SUBSCR_TABLE_ROWS * EFC_SUBSCR_TABLE_COLUMNS);
#endif
  
}

/* ################################################ */

/* ################################################ */
int EkaEfc::armController(EfcArmVer ver) {
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev, P4_ARM_DISARM, armData); 
  return 0;
}
/* ################################################ */
int EkaEfc::disArmController() {
  eka_write(dev, P4_ARM_DISARM, 0); 
  return 0;
}

/* ################################################ */
int EkaEfc::initStrategy(const EfcStratGlobCtx* efcStratGlobCtx) {
  memcpy(&stratGlobCtx,efcStratGlobCtx,sizeof(EfcStratGlobCtx));
  eka_write(dev,P4_ARM_DISARM,0); 
  return 0;
}
/* ################################################ */
EkaUdpSess* EkaEfc::findUdpSess(EkaCoreId coreId, uint32_t mcAddr, uint16_t mcPort) {
  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);
    if (udpSess[i]->myParams(coreId,mcAddr,mcPort)) return udpSess[i];
  }
  return NULL;
}
/* ################################################ */
int EkaEfc::cleanSubscrHwTable() {
  EKA_LOG("Cleaning HW Subscription Table: %d rows, %d words per row",
	  EFC_SUBSCR_TABLE_ROWS,EFC_SUBSCR_TABLE_COLUMNS);

  uint64_t val = eka_read(dev, SW_STATISTICS);
  val &= 0xFFFFFFFF00000000;
  eka_write(dev, SW_STATISTICS, val);
  return 0;
}
/* ################################################ */
int EkaEfc::cleanSecHwCtx() {
  EKA_LOG("Cleaning HW Contexts of %d securities",MAX_SEC_CTX);

  for (EfcSecCtxHandle handle = 0; handle < (EfcSecCtxHandle)MAX_SEC_CTX; handle++) {
    const EkaHwSecCtx hwSecCtx = {};
    writeSecHwCtx(handle,&hwSecCtx,0/* writeChan */);
  }
  
  return 0;
}

/* ################################################ */
int EkaEfc::initHwRoundTable() {
#ifdef _VERILOG_SIM
  return 0;
#else

  for (uint64_t addr = 0; addr < ROUND_2B_TABLE_DEPTH; addr++) {
    uint64_t data = 0;
    switch (hwFeedVer) {
    case EfhFeedVer::kPHLX:
    case EfhFeedVer::kGEMX: 
      data = (addr / 10) * 10;
      break;
    case EfhFeedVer::kNASDAQ: 
    case EfhFeedVer::kMIAX:
    case EfhFeedVer::kCBOE:
    case EfhFeedVer::kCME:
    case EfhFeedVer::kNEWS:
    case EfhFeedVer::kITCHFS:
      data = addr;
      break;
    default:
      on_error("Unexpected hwFeedVer = 0x%x",(int)hwFeedVer);
    }

    uint64_t indAddr = 0x0100000000000000 + addr;
    indirectWrite(dev,indAddr,data);

    /* eka_write (dev,ROUND_2B_ADDR,addr); */
    /* eka_write (dev,ROUND_2B_DATA,data); */
    //    EKA_LOG("%016x (%ju) @ %016x (%ju)",data,data,addr,addr);
  }
#endif
  return 0;
}
/* ############################################### */
static bool isValidCboeSecondByte(char c) {
  switch (c) {
  case '0' :
  case '1' :
  case '2' :
  case '3' :
    return true;
  default:
    return false;
  }
}

bool EkaEfc::isValidSecId(uint64_t secId) {
  switch(hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
    if ((secId & ~0x1FFFFFULL) != 0) return false;
    return true;

  case EfhFeedVer::kMIAX:
    if ((secId & ~0x3E00FFFFULL) != 0) return false;
    return true;

  case EfhFeedVer::kCBOE:
    if (((char)((secId >> (8 * 5)) & 0xFF) != '0') ||
	! isValidCboeSecondByte((char)((secId >> (8 * 4)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 3)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 2)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 1)) & 0xFF)) ||
	! isAscii((char)((secId >> (8 * 0)) & 0xFF))) return false;
    return true;

  default:
    on_error ("Unexpected hwFeedVer: %d", (int)hwFeedVer);
  }
}
/* ################################################ */
static uint64_t char2num(char c) {
  if (c >= '0' && c <= '9') return c - '0';            // 10
  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;       // 36
  if (c >= 'a' && c <= 'z') return c - 'a' + 10 + 26;  // 62
  on_error("Unexpected char \'%c\' (0x%x)",c,c);
}

/* ################################################ */
int EkaEfc::normalizeId(uint64_t secId) {
  switch(hwFeedVer) {
  case EfhFeedVer::kGEMX:
  case EfhFeedVer::kNASDAQ:
  case EfhFeedVer::kPHLX:
  case EfhFeedVer::kMIAX:
    return secId;
  case EfhFeedVer::kCBOE: {
    uint64_t res = 0;
    char c = '\0'; 
    uint64_t n = 0;

    c = (char) ((secId >> (8 * 0)) & 0xFF);
    n = char2num(c)    << (6 * 0);
    res |= n;

    c = (char) ((secId >> (8 * 1)) & 0xFF);
    n = char2num(c)    << (6 * 1);
    res |= n;

    c = (char) ((secId >> (8 * 2)) & 0xFF);
    n = char2num(c)    << (6 * 2);
    res |= n;

    c = (char) ((secId >> (8 * 3)) & 0xFF);
    n = char2num(c)    << (6 * 3);
    res |= n;

    c = (char) ((secId >> (8 * 4)) & 0xFF);
    n = char2num(c)    << (6 * 4);
    res |= n;

    return res;
  }
  default:
    on_error ("Unexpected hwFeedVer: %d", (int)hwFeedVer);
  }
}
/* ################################################ */
int EkaEfc::getLineIdx(uint64_t normSecId) {
  return (int) normSecId & (EFC_SUBSCR_TABLE_ROWS - 1);
}
/* ################################################ */
int EkaEfc::subscribeSec(uint64_t secId) {
  if (! isValidSecId(secId)) 
    return -1;
  //    on_error("Security %ju (0x%jx) violates Hash function assumption",secId,secId);

  if (numSecurities == EKA_MAX_P4_SUBSCR) {
    EKA_WARN("numSecurities %d  == EKA_MAX_P4_SUBSCR: secId %ju (0x%jx) is ignored",
	     numSecurities,secId,secId);
    return -1;
  }

  uint64_t normSecId = normalizeId(secId);
  int      lineIdx   = getLineIdx(normSecId);

  //  EKA_DEBUG("Subscribing on 0x%jx, lineIdx = 0x%x (%d)",secId,lineIdx,lineIdx);
  if (hashLine[lineIdx]->addSecurity(normSecId)) {
    numSecurities++;
    uint64_t val = eka_read(dev, SW_STATISTICS);
    val &= 0xFFFFFFFF00000000;
    val |= (uint64_t)(numSecurities);
    
#ifndef _VERILOG_SIM
    eka_write(dev, SW_STATISTICS, val);
#endif
    
  }
  return 0;
}
/* ################################################ */
EfcSecCtxHandle EkaEfc::getSubscriptionId(uint64_t secId) {
  if (! isValidSecId(secId)) 
    return -1;
  //    on_error("Security %ju (0x%jx) violates Hash function assumption",secId,secId);
  uint64_t normSecId = normalizeId(secId);
  int      lineIdx   = getLineIdx(normSecId);

  auto handle = hashLine[lineIdx]->getSubscriptionId(normSecId);

#if EFC_CTX_SANITY_CHECK
  secIdList[handle] = secId;
#endif
  
  return handle;
}

/* ################################################ */
int EkaEfc::downloadTable() {
  int sum = 0;
  for (auto i = 0; i < EFC_SUBSCR_TABLE_ROWS; i++) {
    struct timespec req = {0, 1000};
    struct timespec rem = {};

    sum += hashLine[i]->pack(sum);
    hashLine[i]->downloadPacked();

    nanosleep(&req, &rem);  // added due to "too fast" write to card
  }

  if (sum != numSecurities) 
    on_error("sum %d != numSecurities %d",sum,numSecurities);

  return 0;
}

/* ################################################ */
int EkaEfc::enableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev,ENABLE_PORT);
  uint8_t tcpCores = dev->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores  = dev->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES; coreId++) {
    if ((0x1 << coreId) & tcpCores) {
      fire_rx_tx_en |= 1ULL << (16 + coreId); //fire core enable */
    }
    if ((0x1 << coreId) & mdCores) {
      fire_rx_tx_en |= 1ULL << coreId;          // RX (Parser) core enable */
    }
  }

  eka_write(dev,ENABLE_PORT,fire_rx_tx_en);

  EKA_LOG("fire_rx_tx_en = 0x%016jx",fire_rx_tx_en);
  return 0;
}
/* ################################################ */
int EkaEfc::checkSanity() {
  for (auto i = 0; i < EkaEpm::MAX_UDP_SESS; i++) {
    if (udpSess[i] == NULL) continue;
    if (! action[i]->initialized)
      on_error("EFC Trigger (UDP Session) #%d "
	       "does not have initialized Action",i);
  }
  return 0;
}

/* ################################################ */
int EkaEfc::run(EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx) {
  checkSanity();
  
  reportCb   = pEfcRunCtx->onEfcFireReportCb ? pEfcRunCtx->onEfcFireReportCb :
    efcPrintFireReport;
  cbCtx      = pEfcRunCtx->cbCtx;

  setHwGlobalParams();
  setHwUdpParams();
  if (hwFeedVer != EfhFeedVer::kCME)
    setHwStratRegion();
  //  igmpJoinAll();

  enableRxFire();

  if (dev->fireReportThreadActive) {
    on_error("fireReportThread already active");
  }
  
  dev->fireReportThread = std::thread(ekaFireReportThread,dev);
  dev->fireReportThread.detach();
  while (! dev->fireReportThreadActive)
    sleep(0);
  EKA_LOG("fireReportThread activated");
  
  return 0;
}

/* ################################################ */
int EkaEfc::setHwGlobalParams() {
  EKA_LOG("enable_strategy=%d, report_only=%d, debug_always_fire=%d, debug_always_fire_on_unsubscribed=%d, max_size=%d, watchdog_timeout_sec=%ju",
	  stratGlobCtx.enable_strategy, 
	  stratGlobCtx.report_only, 
	  stratGlobCtx.debug_always_fire, 
	  stratGlobCtx.debug_always_fire_on_unsubscribed, 
	  stratGlobCtx.max_size, 
	  stratGlobCtx.watchdog_timeout_sec);

  eka_write(dev,P4_GLOBAL_MAX_SIZE,stratGlobCtx.max_size);

  uint64_t p4_strat_conf = eka_read(dev,P4_STRAT_CONF);
  uint64_t p4_watchdog_period = EKA_WATCHDOG_SEC_VAL;

  if (stratGlobCtx.report_only)                       p4_strat_conf |= EKA_P4_REPORT_ONLY_BIT;
  if (stratGlobCtx.debug_always_fire)                 p4_strat_conf |= EKA_P4_ALWAYS_FIRE_BIT;
  if (stratGlobCtx.debug_always_fire_on_unsubscribed) p4_strat_conf |= EKA_P4_ALWAYS_FIRE_UNSUBS_BIT;
  /* if (stratGlobCtx.auto_rearm == 1)  */
  /*   p4_strat_conf |= EKA_P4_AUTO_REARM_BIT; */
  if (stratGlobCtx.watchdog_timeout_sec != 0) 
    p4_watchdog_period = EKA_WATCHDOG_SEC_VAL * stratGlobCtx.watchdog_timeout_sec;
    

  eka_write(dev,P4_STRAT_CONF,   p4_strat_conf);
  eka_write(dev,P4_WATCHDOG_CONF,p4_watchdog_period);

  return 0;
}
/* ################################################ */
int EkaEfc::setHwUdpParams() {
  for (auto i = 0; i < MAX_UDP_SESS; i++) {
    uint32_t ip   = 0;
    uint16_t port = 0;
    uint64_t tmp_ipport =
      ((uint64_t)i)    << 56 |
      ((uint64_t)port) << 32 |
      be32toh(ip);
    eka_write (dev,FH_GROUP_IPPORT,tmp_ipport);
  }
  
  EKA_LOG("downloading %d MC sessions to FPGA",numUdpSess);
  for (auto i = 0; i < numUdpSess; i++) {
    if (!udpSess[i]) on_error("!udpSess[%d]",i);

    EKA_LOG("configuring IP:UDP_PORT %s:%u for MD for group:%d",
	    EKA_IP2STR(udpSess[i]->ip),udpSess[i]->port,i);
    uint32_t ip   = udpSess[i]->ip;
    uint16_t port = udpSess[i]->port;

    uint64_t tmp_ipport =
      ((uint64_t)i)    << 56 |
      ((uint64_t)port) << 32 |
      be32toh(ip);
    //  EKA_LOG("HW Port-IP register = 0x%016jx (%x : %x)",
    //  tmp_ipport,ip,port);
    eka_write (dev,FH_GROUP_IPPORT,tmp_ipport);

  }
  return 0;
}
/* ################################################ */
int EkaEfc::setHwStratRegion() {
  struct StratRegion {
    uint8_t region;
    uint8_t strategyIdx;
  } __attribute__ ((aligned(sizeof(uint64_t)))) __attribute__((packed));

  StratRegion stratRegion[MAX_UDP_SESS] = {};

  for (auto i = 0; i < numUdpSess; i++) {
    if (udpSess[i] == NULL) on_error("udpSess[%d] == NULL",i);
    stratRegion[i].region      = EkaEpm::EfcRegion;
    stratRegion[i].strategyIdx = 0;
  }
  copyBuf2Hw(dev,0x83000,(uint64_t*) &stratRegion,sizeof(stratRegion));

  return 0;
}
/* ################################################ */
