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

int EkaFhBoxGr::processFromQ(const EfhRunCtx* pEfhRunCtx) {
  while (! q->is_empty()) {
    fh_msg* buf = q->pop();
    if (buf->sequence < expected_sequence) {
      EKA_WARN("%s:%u buf->sequence %ju < expected_sequence %ju",
	       EKA_EXCH_DECODE(exch),id,buf->sequence,expected_sequence);
    } else if (buf->sequence > expected_sequence) {
      	gapClosed = false;

	EKA_LOG("%s:%u Gap at processing from Q: buf->sequence %ju > expected_sequence %ju",
		EKA_EXCH_DECODE(exch),id,buf->sequence,expected_sequence);
	sendFeedDown(pEfhRunCtx);
	closeIncrementalGap(pEfhCtx,pEfhRunCtx,expected_sequence, buf->sequence);
	state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	return 0;
    } else {
      EKA_LOG("q_len=%u,buf->sequence=%ju, expected_sequence=%ju",
      	      q->get_len(),buf->sequence,expected_sequence);
      
      parseMsg(pEfhRunCtx,(const unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
      expected_sequence = buf->sequence >= 999999999 ? buf->sequence + 1 - 999999999 : buf->sequence + 1;
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

  lastPktLen    = pktLen;
  lastPktMsgCnt = 0;

  uint64_t firstMsgSeq   = Hsvf::getHsvfMsgSequence(&pkt[idx]);

  while (idx < pktLen) {
    uint     msgLen   = Hsvf::getHsvfMsgLen(&pkt[idx],pktLen-idx);
    uint64_t msgSeq   = Hsvf::getHsvfMsgSequence(&pkt[idx]);
    if (idx + (int)msgLen > pktLen) {
      hexDump("Pkt with wrong msgLen",pkt,pktLen);
      on_error("idx %d + msgLen %u > pktLen %d",idx,msgLen,pktLen);
    }

    lastProcessedSeq = msgSeq;

    if (msgSeq >= expected_sequence) {
      if (parseMsg(pEfhRunCtx,&pkt[idx+1],msgSeq,EkaFhMode::MCAST)) {
	EKA_WARN("Exiting in the middle of the packet");
	return true;
      }
      expected_sequence = msgSeq >= 999999999 ?  msgSeq + 1 - 999999999 : msgSeq + 1;
    } 

    idx += msgLen;
    idx += trailingZeros(&pkt[idx],pktLen-idx );    
    lastPktMsgCnt++;
  }

  if (expected_sequence != lastProcessedSeq + 1) 
    on_error("expected_sequence %ju != lastProcessedSeq %ju + 1",expected_sequence,lastProcessedSeq);

  if (lastProcessedSeq - firstMsgSeq != lastPktMsgCnt - 1)
    EKA_WARN("lastProcessedSeq %ju - firstMsgSeq %ju != lastPktMsgCnt %u - 1",
	     lastProcessedSeq,firstMsgSeq,lastPktMsgCnt);
  return false;
}
 
/* ##################################################################### */
void EkaFhBoxGr::pushUdpPkt2Q(const uint8_t* pkt, uint pktLen) {
  const uint8_t* p = pkt;
  uint idx = 0;
  while (idx < pktLen) {
    auto msgType = ((HsvfMsgHdr*)&p[idx+1])->MsgType;
    auto msgLen  = Hsvf::getHsvfMsgLen(&p[idx],pktLen - idx);    
    auto msgSeq  = Hsvf::getHsvfMsgSequence(&p[idx]);
    /* if (memcmp(msgType,"F ",2) == 0 ||  // Quote */
    /* 	memcmp(msgType,"Z ",2) == 0) {  // Time */
    if	(memcmp(msgType,"V ",2) != 0) {  // Heartbeat
      if (msgLen > fh_msg::MSG_SIZE) 
	on_error("msgLen %u > fh_msg::MSG_SIZE %u",msgLen,fh_msg::MSG_SIZE);
      fh_msg* n = q->push();
      memcpy (n->data,&p[idx+1],msgLen - 1);
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
  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
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
