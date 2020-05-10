#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <thread>

#include "eka_dev.h"
#include "eka_sn_dev.h"
#include "eka_user_channel.h"
#include "eka_fh.h"
#include "eka_fh_run_group.h"
#include "Eka.h"
#include "Efh.h"
#include "EkaEpm.h"

int ekaDefaultLog (void* /*unused*/, const char* function, const char* file, int line, int priority, const char* format, ...);
OnEfcFireReportCb* efcDefaultOnFireReportCb (EfcCtx* efcCtx, const EfcFireReport* efcFireReport, size_t size);
OnEkaExceptionReportCb* efhDefaultOnException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData);
void eka_write(EkaDev* dev, uint64_t addr, uint64_t val);
uint64_t eka_read(eka_dev_t* dev, uint64_t addr);
bool eka_is_all_zeros (const void* buf, ssize_t size);
void eka_close_tcp ( EkaDev* pEkaDev);

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
#if 0

static inline uint8_t getGrId(EkaFh* fh, const uint8_t* pkt) {
  for (uint8_t i = 0; i < fh->groups; i++) if (be16toh(fh->b_gr[i]->mcast_port) == EKA_UDPHDR_DST((pkt-8))) return i;
  return 0xFF;
}
/* ##################################################################### */
/*void* io_thread_loop (void* attr)  { */
/*   pthread_detach(pthread_self()); */
/*   EkaDev* dev = (EkaDev*) attr; */

/*   while (dev->io_thread_active) { */
/*     for (uint8_t i = 0; i < dev->numRunGr; i ++) { */
/*       if (dev->runGr[i] == NULL) on_error("dev->runGr[%u] == NULL",i); */
/*       if (dev->runGr[i]->udpCh == NULL) on_error("dev->runGr[%u]->udpCh == NULL",i); */
/*       if (! dev->runGr[i]->udpCh->has_data()) continue; */
/*       EkaSource exch = dev->runGr[i]->exch; */
/*       EkaFh* fh = dev->runGr[i]->fh; */

/*       const uint8_t* pkt = dev->runGr[i]->udpCh->get(); */
/*       if (pkt == NULL) on_error("pkt == NULL"); */

/*       uint8_t gr_id = getGrId(dev->runGr[i]->fh,pkt); */
/*       if (gr_id == 0xFF || fh->b_gr[gr_id] == NULL || fh->b_gr[gr_id]->q == NULL) {  */
/* 	dev->runGr[i]->udpCh->next();  */
/* 	continue;  */
/*       } */
/*       switch (exch) { */
/*       case EkaSource::kNOM_ITTO  : */
/*       case EkaSource::kGEM_TQF   : */
/*       case EkaSource::kISE_TQF   : */
/*       case EkaSource::kPHLX_TOPO : { */
/* 	FhNasdaqGr* gr = (FhNasdaqGr*)fh->b_gr[gr_id]; */
/* 	uint64_t sequence = EKA_MOLD_SEQUENCE(pkt); */
/* 	if (gr->exp_seq == 0) { */
/* 	  gr->exp_seq = sequence; */
/* 	  memcpy(gr->session_id,((struct mold_hdr*)pkt)->session_id,10); */
/* 	  EKA_DEBUG("%s:%u: session_id is set to %s",EKA_EXCH_DECODE(exch),gr->id,gr->session_id + '\0'); */
/* 	} */
/* 	if (EKA_MOLD_MSG_CNT(pkt) == 0xFFFF) { EKA_WARN("%s:%u : Mold EndOfSession",EKA_EXCH_DECODE(exch),gr_id); dev->runGr[i]->stopped_by_exchange = true; } */

/* 	uint indx = sizeof(mold_hdr); */
/* 	//-------------------------------------------------------- */

/* 	for (uint msg=0; msg < EKA_MOLD_MSG_CNT(pkt); msg++) { */
/* 	  uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(pkt[indx])); */
/* 	  assert (msg_len <= fh_msg::MSG_SIZE); */
/* 	  fh_msg* n = gr->q->push(); */
/* 	  //-------------------------------------------------------- */
/* #ifdef EKA_TIME_CHECK */
/* 	  if (sequence % (uint)gr->DIAGNOSTICS::SAMPLE_PERIOD == 0) { */
/* 	    struct timespec monotime; */
/* 	    clock_gettime(CLOCK_MONOTONIC, &monotime); */
/* 	    n->push_ts = ((uint64_t)monotime.tv_sec) * (uint64_t)SEC_TO_NANO +  ((uint64_t)monotime.tv_nsec); */
/* 	    //	    EKA_WARN("sequence = %ju, n->push_ts = %ju",sequence,n->push_ts); */

/* 	    n->push_cpu_id = sched_getcpu(); */
/* 	  } else { */
/* 	    n->push_ts = 0; */
/* 	    n->push_cpu_id = -1; */
/* 	    //	    EKA_WARN("UNSET sequence = %ju, n->push_ts = %ju",sequence,n->push_ts); */

/* 	  } */
/* #endif */
/* 	  //-------------------------------------------------------- */

  
/* 	  memcpy (n->data,&pkt[indx+2],msg_len); */
/* 	  n->sequence = sequence++; */
/* 	  n->gr_id = gr_id; */
/* 	  asm volatile ("mfence" ::: "memory"); */
/* 	  gr->q->push_completed(); */
/* 	  indx += msg_len + sizeof(msg_len); */
/* 	  gr->exp_seq = sequence; */
/* 	} */
/*       } */
/* 	break; */
/*       case EkaSource::kC1_PITCH   : */
/*       case EkaSource::kC2_PITCH   : */
/*       case EkaSource::kEDGX_PITCH : */
/*       case EkaSource::kBZX_PITCH  : { */

/*       } */
/* 	break; */
/*       case EkaSource::kMIAX_TOM   : */
/*       case EkaSource::kPEARL_TOM  : { */

/*       } */
/* 	break; */
/*       case EkaSource::kARCA_XDP   : */
/*       case EkaSource::kAMEX_XDP   : { */

/*       } */
/* 	break; */
/*       default: */
/* 	on_error ("Unexpected EXCH %s (%u)",EKA_EXCH_DECODE(exch),(uint)exch); */
/*       } */
/*       dev->runGr[i]->udpCh->next(); */
/*     } */
/*   } */
/*   return NULL; */
/* } */
#endif

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


  logCB  = initCtx->logCallback == NULL ? ekaDefaultLog : initCtx->logCallback;
  logCtx = initCtx->logContext;

  credAcquire = initCtx->credAcquire;
  credRelease = initCtx->credRelease;
  createThread = initCtx->createThread;

  credContext = initCtx->credContext;
  createThreadContext = initCtx->createThreadContext;

  dev = this;

  pEfcRunCtx = (EfcRunCtx*) calloc(1, sizeof(EfcRunCtx));
  assert (pEfcRunCtx != NULL);

  pEfcRunCtx->onEkaExceptionReportCb = (OnEkaExceptionReportCb) efhDefaultOnException;
  pEfcRunCtx->onEfcFireReportCb      =  (OnEfcFireReportCb)     efcDefaultOnFireReportCb;

  sn_dev = new eka_sn_dev(this);

  epm = new EkaEpm();

  hw.enabled_cores = (sn_dev->read(VERSION2) >> 56) & 0xFF;
  hw.feed_ver = (sn_dev->read(VERSION1) >> HW_FEED_SHIFT_SIZE) & HW_FEED_SHIFT_MASK;

  txPktBuf = sn_dev->txPktBuf;

  for (uint c = 0; c < hw.enabled_cores; c++) {
    sn_dev->get_ip_mac(c, &core[c].src_ip,  &core[c].macsa[0]);
    core[c].connected = sn_dev->get_link_status(c,NULL,NULL);
    EKA_LOG("Port %u: %s, %s, %s",c,EKA_IP2STR(core[c].src_ip),EKA_MAC2STR(core[c].macsa),core[c].connected ? "Connected" : "Not Connected");
  }

  EKA_LOG("EKALINE2 LIB BUILD TIME: %s @ %s",__DATE__,__TIME__);

  eka_write (this,FH_ENABLE_TRAFFIC_TO_UDP_CHANNEL,0x3); // copy all traffic to TCP User Ch

  igmp_thread_active = true;
  pthread_t igmp_thr;
  dev->createThread("IGMP_THREAD",EkaThreadType::kIGMP,igmp_thread,(void*)dev,createThreadContext,(uintptr_t*)&igmp_thr);

  //  io_thread_active = true;
  //  pthread_t io_thr;
  //  dev->createThread("IO_THREAD",EkaThreadType::kPacketIO,io_thread_loop,(void*)dev,createThreadContext,(uintptr_t*)&io_thr);

  print_parsed_messages = false;
}


int EkaDev::configurePort(const EkaCoreInitCtx* pCoreInit) {
  uint8_t c = pCoreInit->coreId;
  if (! core[c].connected) on_error("Trying to configure not connected core %d",c);

  const EkaCoreInitAttrs* attrs = &pCoreInit->attrs;

  if (attrs == NULL) return EKA_OPRESULT__OK;
  if (! eka_is_all_zeros((void*)&attrs->src_mac_addr,6)) {
    for (auto i =0;i<6;i++) core[c].macsa[i] = ((uint8_t*)&attrs->src_mac_addr)[i];
    EKA_LOG("Setting SRC MAC: %s",EKA_MAC2STR(core[c].macsa));
  }
  if (! eka_is_all_zeros((void*)&attrs->nexthop_mac,6)) {
    for (auto i =0;i<6;i++) core[c].macda[i] = ((uint8_t*)&attrs->nexthop_mac)[i];
    EKA_LOG("Setting DST MAC: %s",EKA_MAC2STR(core[c].macda));
    core[c].macda_set_externally = true;
  }
  if (! eka_is_all_zeros((void*)&attrs->host_ip,4)) {
    memcpy(&core[c].src_ip,&attrs->host_ip,4);
    EKA_LOG("Setting HOST IP: %s",EKA_IP2STR(core[c].src_ip));
  }

  return 0;
}

uint8_t EkaDev::getNumFh() {
  return numFh;
}


EkaDev::~EkaDev() {
  EKA_LOG("shutting down...");
  igmp_thread_active = false;
  io_thread_active = false;
  serv_thr.active = false;
  exc_active = false;

  EKA_LOG("Closing %u FHs",numFh);
  fflush(stderr);

  for (auto i = 0; i < numFh; i++) {
    if (fh[i] != NULL) fh[i]->stop();
  }
  for (uint i = 0; i < numRunGr; i++) {
    if (runGr[i] != NULL) runGr[i]->thread_active = false;
  }
  sleep(3);

  for (auto i = 0; i < numFh; i++) {
    if (fh[i] == NULL) continue;
    delete fh[i];
    fh[i] = NULL;
    usleep(10);
  }

  eka_close_tcp(dev);
  if (exc_inited) {
    delete fp_ch;
    delete fr_ch;
  }

  delete sn_dev;

}
