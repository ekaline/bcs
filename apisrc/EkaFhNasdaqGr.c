#include "EkaFhNasdaqGr.h"
#include "EkaFhThreadAttr.h"
#include "eka_fh_q.h"

void* getMolUdp64Data(void* attr);
void* getSoupBinData(void* attr);


/* ##################################################################### */
bool EkaFhNasdaqGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				  const uint8_t*   pkt, 
				  uint             msgInPkt, 
				  uint64_t         seq) {
  uint     indx     = sizeof(mold_hdr);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(pkt[indx]));
    uint8_t* msgData = (uint8_t*)&pkt[indx + sizeof(msg_len)];
    //-----------------------------------------------------------------------------
    if (sequence == expected_sequence){
      if (parseMsg(pEfhRunCtx,msgData,sequence,EkaFhMode::MCAST)) return true;
    } else {
      EKA_WARN("WARNING: sequence %ju != expected_sequence %ju",
	       sequence,expected_sequence);
    }
    //-----------------------------------------------------------------------------
    expected_sequence = sequence + 1;
    sequence++;
    indx += msg_len + sizeof(msg_len);
  }
  return false;  
}
/* ##################################################################### */
void EkaFhNasdaqGr::pushUdpPkt2Q(const uint8_t* pkt, 
				 uint           msgInPkt, 
				 uint64_t       sequence) {
  uint indx = sizeof(mold_hdr);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(pkt[indx]));
    if (msg_len > fh_msg::MSG_SIZE)
      on_error("msg_len %u > fh_msg::MSG_SIZE %ju",
	       msg_len,fh_msg::MSG_SIZE);
    
    fh_msg* n = q->push();
    memcpy (n->data,&pkt[indx+sizeof(msg_len)],msg_len);
    n->sequence = sequence++;
    n->gr_id    = id;
    indx       += msg_len + sizeof(msg_len);
  }
  return;
}

/* ##################################################################### */
int EkaFhNasdaqGr::closeSnapshotGap(EfhCtx*          pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint64_t         startSeq,
				    uint64_t         endSeq) {
  
  std::string threadName = std::string("ST_") + std::string(EKA_EXCH_SOURCE_DECODE(exch)) + '_' + std::to_string(id);
  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
					       pEfhRunCtx, 
					       this, 
					       startSeq, 
					       endSeq, 
					       EkaFhMode::SNAPSHOT);
  if (attr == NULL) on_error("attr = NULL");
  
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    getSoupBinData,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;
}
/* ##################################################################### */
int EkaFhNasdaqGr::closeIncrementalGap(EfhCtx*          pEfhCtx, 
				       const EfhRunCtx* pEfhRunCtx, 
				       uint64_t         startSeq,
				       uint64_t         endSeq) {
  

  std::string threadName = std::string("ST_") + 
    std::string(EKA_EXCH_SOURCE_DECODE(exch)) + 
    '_' + 
    std::to_string(id);

  bool useSoupbin = false; //endSeq - startSeq > 100;

  EkaFhThreadAttr* attr  = new EkaFhThreadAttr(pEfhCtx, 
					       pEfhRunCtx, 
					       this, 
					       startSeq, 
					       endSeq,  
					       EkaFhMode::RECOVERY);
  if (attr == NULL) on_error("attr = NULL");
    
  dev->createThread(threadName.c_str(),
		    EkaServiceType::kFeedSnapshot,
		    useSoupbin ? getSoupBinData : getMolUdp64Data,
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   


  return 0;
}

/* ##################################################################### */
