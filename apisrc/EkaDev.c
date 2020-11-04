// SHURIK

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <thread>

#include "EkaDev.h"
#include "EkaCore.h"
#include "EkaSnDev.h"
#include "eka_fh.h"
#include "Eka.h"
#include "Efh.h"
#include "EkaEpm.h"
#include "EkaFhRunGr.h"

int ekaDefaultLog (void* /*unused*/, const char* function, const char* file, int line, int priority, const char* format, ...);
OnEfcFireReportCb* efcDefaultOnFireReportCb (EfcCtx* efcCtx, const EfcFireReport* efcFireReport, size_t size);
OnEkaExceptionReportCb* efhDefaultOnException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData);
void eka_write(EkaDev* dev, uint64_t addr, uint64_t val);
uint64_t eka_read(eka_dev_t* dev, uint64_t addr);
bool eka_is_all_zeros (const void* buf, ssize_t size);
void eka_close_tcp ( EkaDev* pEkaDev);


void ekaInitLwip (EkaDev* dev);
void setNetifIpMacSa(EkaDev* dev, uint8_t coreId, const uint8_t* macSa);
void setNetifIpSrc(EkaDev* dev, uint8_t coreId, const uint32_t* srcIp);

void ekaServThread(EkaDev* dev);

/* ##################################################################### */

void* igmp_thread (void* attr) {
  pthread_detach(pthread_self());
  EkaDev* dev = (EkaDev*) attr;
  EKA_LOG("Launching IGMPs for %u FHs",dev->numFh);
  while (dev->igmp_thread_active) {
    for (auto i = 0; i < dev->CONF::MAX_FEED_HANDLERS; i++) {
      if (! dev->igmp_thread_active) return NULL;
      if (dev->fh[i] == NULL) continue;
      dev->fh[i]->send_igmp(true,dev->igmp_thread_active);      
      usleep(10);
    }
    if (! dev->igmp_thread_active) return NULL;
    usleep(1000000);
  }
  EKA_LOG("Closing IGMPs for %u FHs",dev->numFh);

  return NULL;
}
/* ##################################################################### */

static void str_time_from_nano(uint64_t current_time, char* time_str){
  int nano = current_time % 1000;
  int micro = (current_time/1000) % 1000;
  int mili = (current_time/(1000*1000)) % 1000;
  int current_time_seconds = current_time/(1000*1000*1000);
  time_t tmp = current_time_seconds;
  struct tm lt;
  (void) localtime_r(&tmp, &lt);
  char result[32] = {};
  strftime(result, sizeof(result), "%Y-%m-%d %H:%M:%S", &lt);
  sprintf(time_str," %s.%03d.%03d.%03d", result, mili, micro, nano);
  return;
}
/* ##################################################################### */

static uint64_t getFpgaTimeCycles () { // ignores Application - PCIe - FPGA latency
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); // 
  uint64_t current_time_ns = ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 + (uint64_t)(t.tv_nsec));
  uint64_t current_time_cycles = (current_time_ns * (EKA_FPGA_FREQUENCY / 1000.0));
  /* eka_write(dev,FPGA_RT_CNTR,current_time_cycles); */
  /* char t_str[64] = {}; */
  /* str_time_from_nano(current_time_ns,t_str); */
  /* EKA_LOG("setting HW time to %s",t_str); */
  return current_time_cycles;
}


/* ##################################################################### */


EkaDev::EkaDev(const EkaDevInitCtx* initCtx) {
  exc_inited = false;
  efc_run_threadIsUp = false;
  efc_fire_report_threadIsUp = false;
  exc_active = false;
  exc_inited = false;
  lwip_inited = false;

  for (auto i = 0; i < CONF::MAX_FEED_HANDLERS; i++) fh[i] = NULL;
  numFh = 0;
  for (auto i = 0; i < CONF::MAX_RUN_GROUPS; i++) runGr[i] = NULL;
  numRunGr = 0;

/* -------------------------------------------- */
  dev = this;
  logCB  = initCtx->logCallback == NULL ? ekaDefaultLog : initCtx->logCallback;
  logCtx = initCtx->logContext;

  snDev          = new EkaSnDev(this);

  uint64_t mirrorNonUdp2UserChannel = 1ULL << 32;
  uint64_t portState = eka_read(ENABLE_PORT);
  portState |= mirrorNonUdp2UserChannel;
  portState = 0x81010003; // TO DOUBLE CHECK!!!
  portState = 0x181010003;
  EKA_LOG("portState = 0x%016x",portState);
  eka_write(ENABLE_PORT,portState);


  time_t t;
  srand((unsigned) time(&t));

  ekaInitLwip(this);

  totalNumTcpSess = 0;
  use_vlan = false;

  bool noCores = true;
  for (uint c = 0; c < CONF::MAX_CORES; c++) {
    uint32_t ip = 0;
    uint8_t mac[6] = {};
    snDev->getIpMac(c,&ip,mac);
#ifdef XN01_LAB_TCP_TEST
    if (c == 0) ip = inet_addr("100.0.0.110");
#endif
    if (ip != 0 && snDev->hasLink(c)) {
      core[c] = new EkaCore(dev,c,ip,mac);
      EKA_LOG("FETH%u LINK=1 %s %s",c,EKA_IP2STR(ip),EKA_MAC2STR(mac));
      noCores = false;
    } else {
      //      EKA_LOG("Core %u has NO link or configured IP",c);
      core[c] = NULL;
    }
  }

  if (noCores) on_error("No FPGA ports have Link and/or IP");

  servThreadActive = false;
  servThread    = std::thread(ekaServThread,this);
  servThread.detach();
  while (!servThreadActive /* || !tcpRxThreadActive */) {}
  EKA_LOG("Serv thread activated");

  hwEnabledCores = (eka_read(VERSION2) >> 56) & 0xFF;
  hwFeedVer      = (eka_read(VERSION1) >> HW_FEED_SHIFT_SIZE) & HW_FEED_SHIFT_MASK;

/* -------------------------------------------- */


  credAcquire = initCtx->credAcquire;
  credRelease = initCtx->credRelease;
  createThread = initCtx->createThread;

  credContext = initCtx->credContext;
  createThreadContext = initCtx->createThreadContext;


  pEfcRunCtx = (EfcRunCtx*) calloc(1, sizeof(EfcRunCtx));
  assert (pEfcRunCtx != NULL);

  pEfcRunCtx->onEkaExceptionReportCb = (OnEkaExceptionReportCb) efhDefaultOnException;
  pEfcRunCtx->onEfcFireReportCb      = (OnEfcFireReportCb)      efcDefaultOnFireReportCb;

  epm = new EkaEpm(this);

  EKA_LOG("EKALINE2 LIB BUILD TIME: %s @ %s",__DATE__,__TIME__);


  igmp_thread_active = true;
  pthread_t igmp_thr;
  dev->createThread("IGMP_THREAD",EkaThreadType::kIGMP,igmp_thread,(void*)dev,createThreadContext,(uintptr_t*)&igmp_thr);

  //  io_thread_active = true;
  //  pthread_t io_thr;
  //  dev->createThread("IO_THREAD",EkaThreadType::kPacketIO,io_thread_loop,(void*)dev,createThreadContext,(uintptr_t*)&io_thr);

  print_parsed_messages = false;

  clearHw();
  eka_write(FPGA_RT_CNTR,getFpgaTimeCycles());
  eka_write(SCRPAD_SW_VER,EKA_CORRECT_SW_VER | hwEnabledCores);

}

static void set_time (EkaDev* dev) { // ignores Application - PCIe - FPGA latency
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); // 
  uint64_t current_time_ns = ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 + (uint64_t)(t.tv_nsec));
  uint64_t current_time_cycles = (current_time_ns * (EKA_FPGA_FREQUENCY / 1000.0));
  eka_write(dev,FPGA_RT_CNTR,current_time_cycles);
  char t_str[64] = {};
  str_time_from_nano(current_time_ns,t_str);
  EKA_LOG("setting HW time to %s",t_str);
  return;
}

int EkaDev::configurePort(const EkaCoreInitCtx* pCoreInit) {
  uint8_t c = pCoreInit->coreId;
  if (core[c] == NULL) on_error("Trying to configure not connected core %d",c);

  const EkaCoreInitAttrs* attrs = &pCoreInit->attrs;

  if (attrs == NULL) return EKA_OPRESULT__OK;
  if (! eka_is_all_zeros((void*)&attrs->src_mac_addr,6)) {
    for (auto i =0;i<6;i++) core[c]->macSa[i] = ((uint8_t*)&attrs->src_mac_addr)[i];
    setNetifIpMacSa(dev,c,(const uint8_t*)&attrs->src_mac_addr);
    EKA_LOG("Setting SRC MAC: %s",EKA_MAC2STR(core[c]->macSa));
  }
  if (! eka_is_all_zeros((void*)&attrs->nexthop_mac,6)) {
    for (auto i =0;i<6;i++) core[c]->macDa[i] = ((uint8_t*)&attrs->nexthop_mac)[i];
    EKA_LOG("Setting DST MAC: %s",EKA_MAC2STR(core[c]->macDa));
    core[c]->macDa_set_externally = true;
  }
  if (! eka_is_all_zeros((void*)&attrs->host_ip,4)) {
    memcpy(&core[c]->srcIp,&attrs->host_ip,4);
    setNetifIpSrc(dev,c,&attrs->host_ip);
    EKA_LOG("Setting HOST IP: %s",EKA_IP2STR(core[c]->srcIp));
  }

  return 0;
}

uint8_t EkaDev::getNumFh() {
  return numFh;
}


EkaDev::~EkaDev() {
  EKA_LOG("shutting down...");

  EKA_LOG("Closing %u FHs",numFh);
  fflush(stderr);

  EKA_LOG("Closing Epm");
  dev->epm->active = false;

  for (auto i = 0; i < numFh; i++) {
    if (fh[i] != NULL) fh[i]->stop();
  }
  for (uint i = 0; i < numRunGr; i++) {
    if (runGr[i] != NULL) runGr[i]->thread_active = false;
  }

  igmp_thread_active = false;
  io_thread_active = false;
  serv_thr.active = false;
  exc_active = false;
  servThreadActive = false;

  sleep(3);

  for (auto i = 0; i < numFh; i++) {
    if (fh[i] == NULL) continue;
    delete fh[i];
    fh[i] = NULL;
    usleep(10);
  }

  for (uint c = 0; c < CONF::MAX_CORES; c++) {
    if (core[c] == NULL) continue;
    delete core[c];
  }

  // Clesed Dev indication
  uint64_t val = eka_read(SW_STATISTICS);
  val = val & 0x7fffffffffffffff;
  eka_write(SW_STATISTICS, val);

  delete snDev;

}

EkaTcpSess* EkaDev::findTcpSess(uint32_t ipSrc, uint16_t udpSrc, uint32_t ipDst, uint16_t udpDst) {
  for (uint c = 0; c < CONF::MAX_CORES; c++) {
    if (core[c] == NULL) continue;
    EkaTcpSess* s = core[c]->findTcpSess(ipSrc,udpSrc,ipDst,udpDst);
    if (s != NULL) return s;
  }
  return NULL;
}

EkaTcpSess* EkaDev::findTcpSess(int sock) {
  for (uint c = 0; c < CONF::MAX_CORES; c++) {
    if (core[c] == NULL) continue;
    EkaTcpSess* s = core[c]->findTcpSess(sock);
    if (s != NULL) return s;
  }
  return NULL;
}


EkaTcpSess* EkaDev::getControlTcpSess(uint8_t coreId) {
  return core[coreId]->tcpSess[EkaCore::MAX_SESS_PER_CORE];
}


uint8_t EkaDev::findCoreByMacSa(const uint8_t* macSa) {
  for (uint8_t c = 0; c < CONF::MAX_CORES; c++) {
    if (core[c] == NULL) continue;
    if (memcmp(core[c]->macSa,macSa,6) == 0) return c;
  }
  return 65; // NO CORE FOUND
}

void     EkaDev::eka_write(uint64_t addr, uint64_t val) { 
  snDev->write(addr, val); 
}
uint64_t EkaDev::eka_read(uint64_t addr) { 
  return snDev->read(addr); 
}


int EkaDev::clearHw() {
  eka_write(STAT_CLEAR   ,(uint64_t) 1); // Clearing HW Statistics
  eka_write(SW_STATISTICS,(uint64_t) 0); // Clearing SW Statistics
  eka_write(P4_STRAT_CONF,(uint64_t) 0); // Clearing Strategy params

  eka_read(0xf0728); // Clearing Interrupts
  eka_read(0xf0798); // Clearing Interrupts

  for (uint64_t p = 0; p < SW_SCRATCHPAD_SIZE/8; p++) 
    eka_write(SW_SCRATCHPAD_BASE +8*p,(uint64_t) 0);

  // Open Dev indication
  eka_write(SW_STATISTICS, (1ULL<<63));
  return 0;
}
