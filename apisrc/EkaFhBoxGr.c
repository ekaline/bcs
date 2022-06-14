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

#include "EkaFhBoxGr.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhBoxParser.h"

#include "eka_fh_q.h"

using namespace Hsvf;

void* getHsvfRetransmit(void* attr);

/* ##################################################################### */

EkaFhBoxGr::EkaFhBoxGr() {
  const std::time_t nowEpoch =  std::time(nullptr);
  if (nowEpoch == -1)
    on_error("unable to get epoch time: %s (%d)", strerror(errno), errno);
  (void)localtime_r(&nowEpoch, &this->localTimeComponents);
}

int EkaFhBoxGr::processFromQ(const EfhRunCtx* pEfhRunCtx) {
  if (! q->is_empty()) {
    EKA_LOG("%s:%u Q len = %u, 1st element Sequence=%ju",
	    EKA_EXCH_DECODE(exch),id,q->get_len(),q->get_1st_seq());
  }
  while (! q->is_empty()) {
    auto buf = q->pop();
    if (buf->sequence < expected_sequence) {
      EKA_WARN("%s:%u buf->sequence %ju < expected_sequence %ju",
	       EKA_EXCH_DECODE(exch),id,buf->sequence,expected_sequence);
    } else if (buf->sequence > expected_sequence) {
      	gapClosed = false;

	EKA_LOG("%s:%u Gap at Q: buf->sequence %ju > expected_sequence %ju, lost %jd",
		EKA_EXCH_DECODE(exch),id,
		buf->sequence,expected_sequence, buf->sequence - expected_sequence);
	sendFeedDown(pEfhRunCtx);
	closeIncrementalGap(pEfhCtx,pEfhRunCtx,expected_sequence, buf->sequence);
	state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	return 0;
    } else {
      /* EKA_LOG("q_len=%u,buf->sequence=%ju, expected_sequence=%ju", */
      /* 	      q->get_len(),buf->sequence,expected_sequence); */
      if (buf->type == fh_msg::MsgType::REAL) {
	this->gr_ts = buf->push_ts;
	parseMsg(pEfhRunCtx,(const unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
      }
      if (buf->sequence == 999'999'999) {
	EKA_LOG("%s:%u sequence wrap around 999'999'999 -> 0",
		EKA_EXCH_DECODE(exch),id);
	expected_sequence =  0;
      } else {
	expected_sequence =  buf->sequence + 1;
      }
    }
  }
  EKA_LOG("%s:%u: After Q draining expected_sequence = %ju",
	  EKA_EXCH_DECODE(exch),id,expected_sequence);
  return 0;
}
/* ##################################################################### */

int EkaFhBoxGr::bookInit () {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();
 
  return 0;
}
/* ##################################################################### */

bool EkaFhBoxGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
			       const uint8_t*   pkt, 
			       int16_t          pktLen) {
  int idx = 0;

  /* SEEMS TO BE A NOT NEEDED ARTIFACT */
  /* timespec now; */
  /* if (clock_gettime(CLOCK_REALTIME, &now) == -1) */
  /*   on_error("clock_gettime failed: %s (%d)", strerror(errno), errno); */

  while (idx < pktLen) {
    uint     msgLen   = Hsvf::getMsgLen(&pkt[idx],pktLen-idx);
    uint64_t msgSeq   = Hsvf::getMsgSequence(&pkt[idx]);
    if (idx + (int)msgLen > pktLen) {
      hexDump("Pkt with wrong msgLen",pkt,pktLen);
      on_error("idx %d + msgLen %u > pktLen %d",idx,msgLen,pktLen);
    }

    /* SEEMS TO BE A NOT NEEDED ARTIFACT */
    /* this->gr_ts = now.tv_sec * 1'000'000'000 + now.tv_nsec; */
    
    if (parseMsg(pEfhRunCtx,&pkt[idx+1],msgSeq,EkaFhMode::MCAST)) {
      EKA_WARN("Exiting in the middle of the packet");
      return true;
    }
    if (msgSeq == 999'999'999) {
      EKA_LOG("%s:%u sequence wrap around 999'999'999 -> 0",
	      EKA_EXCH_DECODE(exch),id);
      expected_sequence = 0;
    } else {
      expected_sequence = msgSeq + 1;
    }
    idx += msgLen;
    idx += trailingZeros(&pkt[idx],pktLen-idx );    
  }

  return false;
}
 
/* ##################################################################### */
void EkaFhBoxGr::pushUdpPkt2Q(const uint8_t* pkt, uint pktLen) {
  auto p = pkt;
  uint idx = 0;
  timespec now;
  if (clock_gettime(CLOCK_REALTIME, &now) == -1)
    on_error("clock_gettime failed: %s (%d)", strerror(errno), errno);
  while (idx < pktLen) {
    auto msgType = ((HsvfMsgHdr*)&p[idx+1])->MsgType;
    auto msgSeq  = Hsvf::getMsgSequence(&p[idx]);
    auto msgLen  = Hsvf::getMsgLen(&p[idx],pktLen - idx);    
    if (memcmp(msgType,"F ",2) == 0 ||  // Quote
    	memcmp(msgType,"Z ",2) == 0) {  // Time
      if (msgLen > fh_msg::MSG_SIZE) 
	on_error("\'%c\%c\' msgLen %u > fh_msg::MSG_SIZE %jd",
		 msgType[0],msgType[1],msgLen,fh_msg::MSG_SIZE);
      auto n = q->push();
      n->type = fh_msg::MsgType::REAL;
      memcpy (n->data,&p[idx+1],msgLen - 1);
      n->sequence = msgSeq;
      n->gr_id    = id;
      n->push_ts  = now.tv_sec * 1'000'000'000 + now.tv_nsec;
    } else if (memcmp(msgType,"V ",2) == 0) {  // Heartbeat
      // dont put Heartbeat into the Q
    } else {
      auto n = q->push();
      n->type = fh_msg::MsgType::DUMMY;
      // dont copy Msg payload
      n->sequence = msgSeq;
      n->gr_id    = id;
    }
    idx += msgLen;
    idx += trailingZeros(&p[idx],pktLen-idx);

  }
  return;
}
 
/* ##################################################################### */

int EkaFhBoxGr::closeIncrementalGap(EfhCtx*          pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint64_t         startSeq,
				    uint64_t         endSeq) {
  

  std::string threadName = std::string("ST_") + std::string(EKA_EXCH_SOURCE_DECODE(exch)) + '_' + std::to_string(id);
  auto attr  = new EkaFhThreadAttr(pEfhCtx, 
				   (const EfhRunCtx*)pEfhRunCtx, 
				   this, 
				   startSeq, 
				   endSeq,  
				   EkaFhMode::RECOVERY);
  if (attr == NULL) on_error("attr = NULL");
    
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    getHsvfRetransmit,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   


  return 0;
}
/* ##################################################################### */
