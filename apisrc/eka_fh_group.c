#include <thread>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "eka_fh.h"
#include "eka_fh_group.h"
#include "eka_fh_book.h"
#include "eka_data_structs.h"
#include "eka_dev.h"
#include "eka_fh_udp_channel.h"
#include "eka_fh_miax_messages.h"
#include "eka_fh_batspitch_messages.h"

void hexDump (const char *desc, void *addr, int len);
void hexDumpStderr (const char* desc, const void *addr, int len);

void eka_send_pkt(EkaDev* dev, uint8_t core,  uint8_t sess, void *buf, int len);

void create_igmp_pkt (char* dst, bool join, uint8_t* macsa, uint32_t ip_src, uint32_t ip_dst);



void FhGroup::print_q_state() {
  if (q == NULL) {
    EKA_DEBUG("NO Q");
    return;
  }
  EKA_DEBUG("Q.max_len=%10u, Q.ever_max_len=%10u, CPU:%u",
	   q->get_max_len(),
	   q->get_ever_max_len(),
	   sched_getcpu()
	   );
  q->reset_max_len();
  return;  
}
 /* ##################################################################### */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


FhGroup::FhGroup () {
  thread_active = false;
  snapshot_active = false;
  heartbeat_active = false;

  gapNum = 0;
  expected_sequence = 0;
  seq_after_snapshot = 0;
  
  exp_seq = 0;
  spinImageSeq = 0;
  recovery_sequence = 0;
  firstPkt = true;

  trade_status = EfhTradeStatus::kUninit;

  mcast_ip = 0;
  mcast_port = 0;
  mcast_set = false;

  snapshot_ip = 0;
  snapshot_port = 0;
  snapshot_set = false;

  recovery_ip = 0;
  recovery_port = 0;
  recovery_set = false;

  auth_set = false;

  memset(auth_user,0,sizeof(auth_user));
  memset(auth_passwd,0,sizeof(auth_passwd));

  upd_ctr = 0;
  book = NULL;
  no_igmp = false;
  dropMe = false;
  state = GrpState::INIT;

  snapshot_sock = -1;
  recovery_sock = -1;
  //  printf("Created generic group\n");
}
 /* ##################################################################### */

FhBatsGr::FhBatsGr () {

  memset(sessionSubID,0,sizeof(sessionSubID));
  batsUnit = 0;
}
 /* ##################################################################### */

FhMiaxGr::FhMiaxGr () {

}
 /* ##################################################################### */

FhBoxGr::FhBoxGr () {
  txSeqNum = 1;
}

 /* ##################################################################### */

FhXdpGr::FhXdpGr () {
  numStreams = 0;
  numUnderlyings = 0;
  inGap = false;
  gapStart = std::chrono::high_resolution_clock::now();
}

 /* ##################################################################### */

int FhGroup::init (EfhCtx* pEfhCtx_p, const EfhInitCtx* pInitCtx,EkaFh* efh, uint8_t gr_id,EkaSource ec) {
  pEfhCtx = pEfhCtx_p;

  assert(pEfhCtx != NULL);
  assert(pInitCtx != NULL);

  dev = pEfhCtx->dev;
  exch = ec;
  feed_ver = EFH_EXCH2FEED(exch);
  core = pEfhCtx->coreId;
  no_igmp = ! EKA_NATIVE_MAC(dev->core[core].macsa);

  fh = efh;

  EKA_LOG("%s:%u initializing FhGroup, feed_ver=%s",EKA_EXCH_DECODE(exch),gr_id,EKA_FEED_VER_DECODE(feed_ver));
  udp_ch = NULL;
  q = NULL;
  id = gr_id;
  state = GrpState::INIT;
  return 0;
}

/* ##################################################################### */

void FhGroup::createQ(EfhCtx* pEfhCtx, const uint qsize) {  
  q = new fh_q(pEfhCtx,exch,(FhGroup*)this,id,(uint)qsize);
  EKA_DEBUG("%s:%u created Q with %u elemets",EKA_EXCH_DECODE(exch),id, qsize);
  return;
}

/* ##################################################################### */

void FhGroup::openUdp(EfhCtx* pEfhCtx) {  
  udp_ch = new fh_udp_channel(pEfhCtx);
  EKA_DEBUG("%s:%u UDP Ch is open. %s IGMPs",EKA_EXCH_DECODE(exch),id, no_igmp ? "NO" : "SENDING");
  return;
}

 /* ##################################################################### */

int FhGroup::stop() {  
  send_igmp(false);
  EKA_DEBUG("%s:%u - Stopping: IGMP LEAVE sent",EKA_EXCH_DECODE(exch),id);

  thread_active = false;
  snapshot_active = false;
  heartbeat_active = false;

  /* shutdown(recovery_sock,SHUT_RD); */
  /* shutdown(recovery_sock,SHUT_WR); */
  /* shutdown(snapshot_sock,SHUT_RD); */
  /* shutdown(snapshot_sock,SHUT_WR); */
  return 0;
}
 /* ##################################################################### */

FhGroup::~FhGroup () {  
  if (udp_ch != NULL) {
    delete udp_ch;
    EKA_DEBUG("%s:%u UDP Channel is deleted",EKA_EXCH_DECODE(exch),id);
  }
  if (fh->print_parsed_messages) fclose(book->parser_log);

  if (q != NULL) {
    delete q;
    EKA_DEBUG("%s:%u Q is deleted",EKA_EXCH_DECODE(exch),id);
    //    book->invalidate();
  }
  EKA_DEBUG("%s:%u closed",EKA_EXCH_DECODE(exch),id);
}

 /* ##################################################################### */

void FhGroup::send_igmp(bool join_leave) {
  if (no_igmp) return;
  if (mcast_ip == 0) return;

  char igmp_join[64] = {};
  memset(igmp_join,0,sizeof(igmp_join));
  create_igmp_pkt (igmp_join, join_leave, dev->core[core].macsa, dev->core[core].src_ip, mcast_ip);
  //  hexDump((const char*)"igmp_join",(void*)igmp_join,64);
  eka_send_pkt(dev, 0, 0, igmp_join, 60 /*14 + 24 + 8*/);
  return;
}


/* ##################################################################### */

 /* ##################################################################### */

int FhNomGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new NomBook(pEfhCtx,pEfhInitCtx,this);
  if (book == NULL) on_error("book == NULL, &book = %p",&book);
  ((NomBook*)book)->init();
  return 0;
}

int FhBatsGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new BatsBook(pEfhCtx,pEfhInitCtx,this);
  if (book == NULL) on_error("book == NULL, &book = %p",&book);
  ((BatsBook*)book)->init();
  return 0;
}

int FhPhlxGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new TobBook(pEfhCtx,pEfhInitCtx,this);
  ((TobBook*)book)->init();
  return 0;
}

int FhGemGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new TobBook(pEfhCtx,pEfhInitCtx,this);
  ((TobBook*)book)->init();
  return 0;
}

int FhMiaxGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new TobBook(pEfhCtx,pEfhInitCtx,this);
  ((TobBook*)book)->init();
  return 0;
}

int FhXdpGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new TobBook(pEfhCtx,pEfhInitCtx,this);
  ((TobBook*)book)->init();
  return 0;
}


int FhBoxGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new TobBook(pEfhCtx,pEfhInitCtx,this);
  ((TobBook*)book)->init();
  return 0;
}

/* ##################################################################### */

