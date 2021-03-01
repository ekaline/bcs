#include "EkaFhBatsGr.h"
#include "EkaFhThreadAttr.h"
#include "eka_fh_q.h"
#include "EkaFhBatsParser.h"


void* getSpinData(void* attr);
void* getGrpRetransmitData(void* attr);

/* ##################################################################### */

bool EkaFhBatsGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				const uint8_t*   pkt, 
				uint             msgInPkt, 
				uint64_t         seq) {
  uint indx = sizeof(batspitch_sequenced_unit_header);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint8_t msg_len = pkt[indx];
    uint8_t* msgData = (uint8_t*)&pkt[indx];
    //-----------------------------------------------------------------------------
    if (parseMsg(pEfhRunCtx,msgData,sequence,EkaFhMode::MCAST)) return true;
    //-----------------------------------------------------------------------------

    expected_sequence = sequence == 4294967295 ? 1 : sequence + 1;
    indx += msg_len;
  }
  return false;
}

/* ##################################################################### */
void EkaFhBatsGr::pushUdpPkt2Q(const uint8_t* pkt, 
			       uint           msgInPkt, 
			       uint64_t       sequence) {
  uint indx = sizeof(batspitch_sequenced_unit_header);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint8_t msg_len = pkt[indx];
    if (msg_len > fh_msg::MSG_SIZE) on_error("msg_len %u > fh_msg::MSG_SIZE %u",msg_len,fh_msg::MSG_SIZE);
    fh_msg* n = q->push();
    memcpy (n->data,&pkt[indx],msg_len);
    n->sequence = sequence++;
    n->gr_id    = id;
    indx       += msg_len;
  }
  return;
}

/* ##################################################################### */

int EkaFhBatsGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}
/* ##################################################################### */
int EkaFhBatsGr::closeSnapshotGap(EfhCtx*            pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint64_t         startSeq,
				    uint64_t         endSeq) {
  
  std::string threadName = std::string("ST_") + std::string(EKA_EXCH_SOURCE_DECODE(exch)) + '_' + std::to_string(id);
  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
					       (const EfhRunCtx*)pEfhRunCtx, 
					       this, 
					       startSeq, 
					       endSeq, 
					       EkaFhMode::SNAPSHOT);
  if (attr == NULL) on_error("attr = NULL");
  
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    getSpinData,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;
}
/* ##################################################################### */
int EkaFhBatsGr::closeIncrementalGap(EfhCtx*            pEfhCtx, 
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
		    getGrpRetransmitData,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   


  return 0;
}
