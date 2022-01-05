#include "EkaFhPlrGr.h"
#include "EkaFhPlrParser.h"
#include "eka_fh_q.h"
#include "EkaFhThreadAttr.h"

using namespace Plr;
void* runPlrRecoveryThread(void* attr);


/* ##################################################################### */

int EkaFhPlrGr::bookInit () {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}
/* ##################################################################### */

bool EkaFhPlrGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				const uint8_t*  pkt, 
				uint            msgInPkt, 
				uint64_t        seq,
				std::chrono::high_resolution_clock::time_point startTime) {
  auto pktHdr {reinterpret_cast<const PktHdr*>(pkt)};
  auto p = pkt + sizeof(*pktHdr);
  
  auto sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};    
    //-----------------------------------------------------------------------------
    if (parseMsg(pEfhRunCtx,p,sequence,EkaFhMode::MCAST,startTime)) return true;
    //-----------------------------------------------------------------------------
    expected_sequence = ++sequence;
    p += msgHdr->size;
  }
  return false;
}
/* ##################################################################### */
void EkaFhPlrGr::pushUdpPkt2Q(const uint8_t* pkt, 
			       uint          msgInPkt, 
			       uint64_t      seq) {
  auto pktHdr {reinterpret_cast<const PktHdr*>(pkt)};
  auto p = pkt + sizeof(*pktHdr);
  
  auto sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    auto msgHdr {reinterpret_cast<const MsgHdr*>(p)};
    if (msgHdr->size > fh_msg::MSG_SIZE)
      on_error("msgHdr->size %u > fh_msg::MSG_SIZE %jd",
	       msgHdr->size,fh_msg::MSG_SIZE);
    fh_msg* n = q->push();
    memcpy (n->data,p,msgHdr->size);
    n->sequence = sequence++;
    n->gr_id    = id;
    p += msgHdr->size;
  }
  return;
}

/* ##################################################################### */


int EkaFhPlrGr::closeSnapshotGap(EfhCtx*          pEfhCtx, 
				 const EfhRunCtx* pEfhRunCtx, 
				 uint64_t         startSeq,
				 uint64_t         endSeq) {
  
  std::string threadName = std::string("ST_") + std::string(EKA_EXCH_SOURCE_DECODE(exch)) + '_' + std::to_string(id);
  auto attr  = new EkaFhThreadAttr(pEfhCtx, 
				   pEfhRunCtx, 
				   this, 
				   startSeq, 
				   endSeq, 
				   EkaFhMode::SNAPSHOT);
  if (attr == NULL) on_error("attr = NULL");
  
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    runPlrRecoveryThread,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;
}

/* ##################################################################### */

int EkaFhPlrGr::closeIncrementalGap(EfhCtx*          pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint64_t         startSeq,
				    uint64_t         endSeq) {
  std::string threadName = std::string("ST_") + std::string(EKA_EXCH_SOURCE_DECODE(exch)) + '_' + std::to_string(id);
  auto attr  = new EkaFhThreadAttr(pEfhCtx, 
				   pEfhRunCtx, 
				   this, 
				   startSeq, 
				   endSeq, 
				   EkaFhMode::RECOVERY);
  if (attr == NULL) on_error("attr = NULL");
  
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    runPlrRecoveryThread,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;
}
/* ##################################################################### */
