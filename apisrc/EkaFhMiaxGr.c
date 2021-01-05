#include <assert.h>

#include "EkaFhMiaxGr.h"
#include "EkaFhThreadAttr.h"
#include "eka_fh_q.h"
#include "EkaFhMiaxParser.h"

void* getSesmData(void* attr);
void* getSesmRetransmit(void* attr);

/* ##################################################################### */

bool EkaFhMiaxGr::processUdpPkt(const EfhRunCtx* pEfhRunCtx,
				const uint8_t*   pkt, 
				int16_t          pktLen) {
  if (pktLen < (int)sizeof(EkaMiaxMach)) 
    on_error ("pktLen = %d < sizeof(EkaMiaxMach) %ju",pktLen,sizeof(EkaMiaxMach));
  int16_t processedBytes = 0;
  while (processedBytes < pktLen) {
    uint64_t sequence = EKA_GET_MACH_SEQ(pkt);
    uint machPktLen = EKA_GET_MACH_LEN(pkt);

    if (sequence == expected_sequence) {
      MiaxMachType machType = EKA_GET_MACH_TYPE(pkt);
      switch (machType) {
      case MiaxMachType::Heartbeat : 
      case MiaxMachType::StartOfSession :
	break;
      case MiaxMachType::EndOfSession : 
	EKA_WARN("%s:%u : MiaxMachType::EndOfSession",EKA_EXCH_DECODE(exch),id);
	return true;
      case MiaxMachType::AppData : {
	uint8_t* msgData = (uint8_t*)pkt + sizeof(EkaMiaxMach);
	parseMsg(pEfhRunCtx,msgData,sequence,EkaFhMode::MCAST);
	expected_sequence = sequence + 1;
      }
	break;
      default:
	on_error("%s:%u : Unexpected Mach Type %u",EKA_EXCH_DECODE(exch),id,(uint8_t)machType);
      }
    }
    processedBytes += machPktLen;
    if (processedBytes > pktLen) on_error("processedBytes %d > pktLen %d",processedBytes,pktLen);
    pkt += machPktLen;
  }

  return false;
}


/* ##################################################################### */
void EkaFhMiaxGr::pushUdpPkt2Q(const uint8_t* pkt, int16_t pktLen) {
  if (pktLen < (int)sizeof(EkaMiaxMach)) 
    on_error ("pktLen = %d < sizeof(EkaMiaxMach) %ju",pktLen,sizeof(EkaMiaxMach));
  int16_t processedBytes = 0;
  while (processedBytes < pktLen) {
    uint64_t sequence = EKA_GET_MACH_SEQ(pkt);
    MiaxMachType machType = EKA_GET_MACH_TYPE(pkt);
    uint machPktLen = EKA_GET_MACH_LEN(pkt);

    switch (machType) {
    case MiaxMachType::Heartbeat : 
    case MiaxMachType::StartOfSession :
      break;
    case MiaxMachType::EndOfSession : 
      EKA_WARN("%s:%u : MiaxMachType::EndOfSession",EKA_EXCH_DECODE(exch),id);
      return;
    case MiaxMachType::AppData : {
      uint8_t* msgData = (uint8_t*)pkt + sizeof(EkaMiaxMach);
      uint16_t msg_len = machPktLen - sizeof(EkaMiaxMach);
      assert (msg_len <= fh_msg::MSG_SIZE);
      fh_msg* n = q->push();
      memcpy (n->data,msgData,msg_len);
      n->sequence = sequence;
      n->gr_id    = id;
    }
      break;
    default:
      on_error("%s:%u : Unexpected Mach Type %u",EKA_EXCH_DECODE(exch),id,(uint8_t)machType);
    }
    processedBytes += machPktLen;
    if (processedBytes > pktLen) on_error("processedBytes %d > pktLen %d",processedBytes,pktLen);
    pkt += machPktLen;
  }
}

/* ##################################################################### */
int EkaFhMiaxGr::closeSnapshotGap(EfhCtx*           pEfhCtx, 
				    const EfhRunCtx* pEfhRunCtx, 
				    uint64_t          startSeq,
				    uint64_t          endSeq) {
  
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
		    getSesmData,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   

  return 0;
}
/* ##################################################################### */
int EkaFhMiaxGr::closeIncrementalGap(EfhCtx*        pEfhCtx, 
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
		    getSesmRetransmit,        
		    attr,
		    dev->createThreadContext,
		    (uintptr_t*)&snapshot_thread);   


  return 0;
}
/* ##################################################################### */
int EkaFhMiaxGr::bookInit (EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  book = new FhBook(dev,id,exch);
  if (book == NULL) on_error("book = NULL");

  book->init();

  return 0;
}
