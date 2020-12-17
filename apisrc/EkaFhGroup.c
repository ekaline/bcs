#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sched.h>

#include "EkaFhGroup.h"
#include "eka_fh_book.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"

int createIgmpPkt (char* dst, bool join, uint8_t* macsa, uint32_t ip_src, uint32_t ip_dst);

 /* ##################################################################### */

int EkaFhGroup::processFromQ(const EfhRunCtx* pEfhRunCtx) {
  while (! q->is_empty()) {
    fh_msg* buf = q->pop();
    //      EKA_LOG("q_len=%u,buf->sequence=%ju, expected_sequence=%ju",q->get_len(),buf->sequence,expected_sequence);

    if (buf->sequence < expected_sequence) continue;
    parseMsg(pEfhRunCtx,(unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
    expected_sequence = buf->sequence + 1;
  }
  return 0;
}

 /* ##################################################################### */

void EkaFhGroup::sendFeedUp(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");

  EfhFeedUpMsg efhFeedUpMsg = {
    EfhMsgType::kFeedUp, 
    {exch, id}, 
    gapNum 
  };
  pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
}

 /* ##################################################################### */

void EkaFhGroup::sendFeedDown(const EfhRunCtx* pEfhRunCtx) {
  if (pEfhRunCtx == NULL) on_error("pEfhRunCtx == NULL");

  EfhFeedDownMsg efhFeedDownMsg = { 
    EfhMsgType::kFeedDown, 
    {exch, id}, 
    ++gapNum 
  };
  pEfhRunCtx->onEfhFeedDownMsgCb(&efhFeedDownMsg, 0, pEfhRunCtx->efhRunUserData);
}

 /* ##################################################################### */

int EkaFhGroup::init (EfhCtx* _pEfhCtx, 
		      const EfhInitCtx* pInitCtx,
		      EkaFh* _fh, 
		      uint8_t gr_id,
		      EkaSource _exch) {
  pEfhCtx   = _pEfhCtx;

  assert(pEfhCtx != NULL);
  assert(pInitCtx != NULL);

  dev      = pEfhCtx->dev;
  exch     = _exch;
  feed_ver = EFH_EXCH2FEED(exch);
  core     = pEfhCtx->coreId;
  if (dev->core[core] == NULL) on_error("dev->core[%u] == NULL",core);

  no_igmp  = ! EKA_NATIVE_MAC(dev->core[core]->macSa);

  fh       = _fh;

  q        = NULL;
  id       = gr_id;
  state    = GrpState::INIT;

  EKA_LOG("%s:%u is initialized, feed_ver=%s",
	  EKA_EXCH_DECODE(exch),gr_id,EKA_FEED_VER_DECODE(feed_ver));

  return 0;
}
 /* ##################################################################### */

void EkaFhGroup::createQ(EfhCtx* pEfhCtx, const uint qsize) {  
  if (qsize != 0) { 
    q = new fh_q(pEfhCtx,exch,(EkaFhGroup*)this,id,(uint)qsize);
    if (q == NULL) on_error("q == NULL");
  } 
  EKA_DEBUG("%s:%u created Q with %u elemets",EKA_EXCH_DECODE(exch),id, qsize);
  return;
}
 /* ##################################################################### */

int EkaFhGroup::stop() {  
  send_igmp(false);
  EKA_DEBUG("%s:%u - Stopping: IGMP LEAVE sent",EKA_EXCH_DECODE(exch),id);

  thread_active    = false;
  snapshot_active  = false;
  heartbeat_active = false;

  /* shutdown(recovery_sock,SHUT_RD); */
  /* shutdown(recovery_sock,SHUT_WR); */
  /* shutdown(snapshot_sock,SHUT_RD); */
  /* shutdown(snapshot_sock,SHUT_WR); */
  return 0;
}

 /* ##################################################################### */

EkaFhGroup::~EkaFhGroup () {  
  if (fh->print_parsed_messages) fclose(book->parser_log);

  if (q != NULL) {
    delete q;
    EKA_DEBUG("%s:%u Q is deleted",EKA_EXCH_DECODE(exch),id);
    //    book->invalidate();
  }
  EKA_DEBUG("%s:%u closed",EKA_EXCH_DECODE(exch),id);
}

 /* ##################################################################### */

void EkaFhGroup::send_igmp(bool join_leave) {
  if (no_igmp) return;
  if (mcast_ip == 0) return;

  char igmpPkt[64] = {};
  memset(igmpPkt,0,sizeof(igmpPkt));
  uint pktLen = createIgmpPkt(igmpPkt, join_leave, dev->core[core]->macSa, dev->core[core]->srcIp, mcast_ip);

  EkaTcpSess* controlTcpSess = dev->getControlTcpSess(core);
  controlTcpSess->sendFullPkt((void*)igmpPkt,pktLen);

  //  if (! join_leave) hexDump("IGMP LEAVE sent",igmpPkt,pktLen);
  return;
}

 /* ##################################################################### */

int EkaFhGroup::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new TobBook(pEfhCtx,pEfhInitCtx,this);
  ((TobBook*)book)->init();
  return 0;
}
 /* ##################################################################### */

void EkaFhGroup::print_q_state() {
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
