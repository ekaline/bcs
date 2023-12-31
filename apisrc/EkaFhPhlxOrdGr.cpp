#include "EkaFhPhlxOrdGr.h"
#include "EkaFhThreadAttr.h"
#include "eka_fh_q.h"

void* getMolUdpPlxOrdData(void* attr);

/* ##################################################################### */
bool EkaFhPhlxOrdGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				   const uint8_t*   pkt, 
				   uint             msgInPkt, 
				   uint64_t         seq) {
  uint     indx     = sizeof(PhlxMoldHdr);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = (uint16_t) *(uint16_t*)&(pkt[indx]);
    if (msg_len == 0) {
      EKA_WARN("%s:%u Mold session is terminated",EKA_EXCH_DECODE(exch),id);
      return true;
    }

    uint8_t* msgData = (uint8_t*)&pkt[indx + sizeof(msg_len)];
    //-----------------------------------------------------------------------------
    if (parseMsg(pEfhRunCtx,msgData,sequence++,EkaFhMode::MCAST)) return true;
    //-----------------------------------------------------------------------------
    expected_sequence = sequence;
    indx += msg_len + sizeof(msg_len);
  }
  return false;
}
/* ##################################################################### */

void EkaFhPhlxOrdGr::pushUdpPkt2Q(const uint8_t* pkt, 
				  uint           msgInPkt, 
				  uint64_t       sequence) {

  uint indx = sizeof(PhlxMoldHdr);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = (uint16_t) *(uint16_t*)&(pkt[indx]);
    if (msg_len > fh_msg::MSG_SIZE) on_error("msg_len > fh_msg::MSG_SIZE");
    fh_msg* n = q->push();
    memcpy (n->data,&pkt[indx+sizeof(msg_len)],msg_len);
    n->sequence = sequence++;
    n->gr_id = id;
    indx += msg_len + sizeof(msg_len);
  }
  return;
}

/* ##################################################################### */

int EkaFhPhlxOrdGr::bookInit () {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();
  return 0;
}
/* ##################################################################### */

void EkaFhPhlxOrdGr::printBookState() {
  if (!book) {
    EKA_LOG("%s:%u: No book",EKA_EXCH_DECODE(exch),id);
  } else {
    book->printStats();
  }
}
/* ##################################################################### */

int EkaFhPhlxOrdGr::closeIncrementalGap(EfhCtx*        pEfhCtx, 
				       const EfhRunCtx* pEfhRunCtx, 
				       uint64_t          startSeq,
				       uint64_t          endSeq) {
  

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
		    getMolUdpPlxOrdData,
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   
/* ##################################################################### */


  return 0;
}
