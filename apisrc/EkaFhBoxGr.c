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

void* getHsvfRetransmit(void* attr);
uint getHsvfMsgLen(const uint8_t* pkt, int bytes2run);
uint64_t getHsvfMsgSequence(const uint8_t* msg);
uint trailingZeros(const uint8_t* p, uint maxChars);

/* ##################################################################### */

int EkaFhBoxGr::processFromQ(const EfhRunCtx* pEfhRunCtx) {
  while (! q->is_empty()) {
    fh_msg* buf = q->pop();
    //      EKA_LOG("q_len=%u,buf->sequence=%ju, expected_sequence=%ju",q->get_len(),buf->sequence,expected_sequence);
    parseMsg(pEfhRunCtx,(unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
    expected_sequence = buf->sequence + 1;
  }
  return 0;
}
/* ##################################################################### */

int EkaFhBoxGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();
 
  return 0;
}
/* ##################################################################### */

bool EkaFhBoxGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
			       const uint8_t*   pkt, 
			       int16_t          pktLen) {
  //  const uint8_t* p = pkt;
  int idx = 0;

  lastPktLen    = pktLen;
  lastPktMsgCnt = 0;

  while (idx < pktLen) {
    uint     msgLen   = getHsvfMsgLen(&pkt[idx],pktLen-idx);
    if (idx + (int)msgLen > pktLen) {
      hexDump("Pkt with wrong msgLen",pkt,pktLen);
      on_error("idx %d + msgLen %u > pktLen %d",idx,msgLen,pktLen);
    }
    uint64_t msgSeq = getHsvfMsgSequence(&pkt[idx]);

    lastProcessedSeq = msgSeq;

    if (msgSeq >= expected_sequence) {
      if (parseMsg(pEfhRunCtx,&pkt[idx+1],msgSeq,EkaFhMode::MCAST)) {
	EKA_WARN("Exiting in the middle of the packet");
	return true;
      }
      expected_sequence = msgSeq + 1;
    } 

    idx += msgLen;
    idx += trailingZeros(&pkt[idx],pktLen-idx );    
    lastPktMsgCnt++;
  }

  if (expected_sequence != lastProcessedSeq + 1) 
    on_error("expected_sequence %ju != lastProcessedSeq %ju + 1",expected_sequence,lastProcessedSeq);

  return false;
}
 
/* ##################################################################### */
void EkaFhBoxGr::pushUdpPkt2Q(const uint8_t* pkt, uint pktLen) {
  const uint8_t* p = pkt;
  uint idx = 0;
  while (idx < pktLen) {
    uint msgLen = getHsvfMsgLen(&p[idx],pktLen - idx);    
    char* msgType = ((HsvfMsgHdr*) &p[idx+1])->MsgType;
    if (memcmp(msgType,"F ",2) == 0 ||  // Quote
	memcmp(msgType,"Z ",2) == 0) {  // Time
      if (msgLen > fh_msg::MSG_SIZE) 
	on_error("msgLen %u > fh_msg::MSG_SIZE %u",msgLen,fh_msg::MSG_SIZE);
      fh_msg* n = q->push();
      memcpy (n->data,&p[idx+1],msgLen - 1);
      n->sequence = getHsvfMsgSequence(&p[idx]);
      n->gr_id = id;
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
