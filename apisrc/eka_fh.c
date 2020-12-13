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
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <errno.h>
#include <thread>
#include <assert.h>
#include <time.h>

#include "Efh.h"
#include "EkaCtxs.h"
#include "eka_fh.h"
#include "EkaDev.h"
#include "EkaCore.h"
#include "eka_macros.h"
#include "eka_fh_group.h"
#include "eka_fh_batspitch_messages.h"
#include "EkaFhRunGr.h"
#include "eka_fh_xdp_messages.h"
#include "eka_fh_miax_messages.h"
#include "eka_hsvf_box_messages.h"
#include "EkaUdpChannel.h"

void* eka_get_glimpse_data(void* attr);
void* eka_get_mold_retransmit_data(void* attr);
void* eka_get_phlx_ord_glimpse_data(void* attr);
void* eka_get_phlx_mold_retransmit_data(void* attr);

void* eka_get_spin_data(void* attr);
void* eka_get_grp_retransmit_data(void* attr);
void* eka_get_sesm_data(void* attr);
void* eka_get_sesm_retransmit(void* attr);
void* eka_get_hsvf_retransmit(void* attr);

EkaOpResult eka_get_xdp_definitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhXdpGr* gr,EkaFhMode op);
EkaOpResult eka_hsvf_get_definitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, FhBoxGr* gr);

uint getHsvfMsgLen(const uint8_t* pkt, int bytes2run);
uint64_t getHsvfMsgSequence(uint8_t* msg);
uint trailingZeros(uint8_t* p, uint maxChars);

void hexDump (const char *desc, void *addr, int len);






 /* ##################################################################### */

int EkaFh::openGroups(EfhCtx* pEfhCtx, const EfhInitCtx* pEfhInitCtx) {
  assert (pEfhInitCtx->numOfGroups <= EKA_FH_GROUPS);
  groups = pEfhInitCtx->numOfGroups;

  for (uint8_t i=0; i < groups ; i++) {
    switch (exch) {
    case EkaSource::kNOM_ITTO:
      b_gr[i] =  new FhNomGr();
      break;
    case EkaSource::kGEM_TQF:
    case EkaSource::kISE_TQF:
    case EkaSource::kMRX_TQF:
      b_gr[i] =  new FhGemGr();
      break;
    case EkaSource::kPHLX_TOPO:
      b_gr[i] =  new FhPhlxGr();
      break;
    case EkaSource::kPHLX_ORD:
      b_gr[i] =  new FhPhlxOrdGr();
      break;
    case EkaSource::kC1_PITCH:
    case EkaSource::kC2_PITCH:
    case EkaSource::kBZX_PITCH:
    case EkaSource::kEDGX_PITCH:
      b_gr[i] =  new FhBatsGr();
      break;
    case EkaSource::kMIAX_TOM:
    case EkaSource::kPEARL_TOM:
      b_gr[i] =  new FhMiaxGr();
      break;
    case EkaSource::kARCA_XDP:
    case EkaSource::kAMEX_XDP:
      b_gr[i] =  new FhXdpGr();
      break;
     case EkaSource::kBOX_HSVF:
      b_gr[i] =  new FhBoxGr();
      break;
   default:
      on_error ("Invalid Exchange %s from: %s",EKA_EXCH_DECODE(exch),pEfhInitCtx->ekaProps->props[0].szKey);
    }
    assert (b_gr[i] != NULL);
    b_gr[i]->init(pEfhCtx,pEfhInitCtx,this,i,exch);
    b_gr[i]->bookInit(pEfhCtx,pEfhInitCtx);
    EKA_LOG("%s:%u initialized as %s:%u at runGroups",EKA_EXCH_DECODE(exch),b_gr[i]->id,EKA_EXCH_DECODE(b_gr[i]->exch),i);

    if (print_parsed_messages) {
      std::string parsedMsgFileName = std::string(EKA_EXCH_DECODE(exch)) + std::to_string(i) + std::string("_PARSED_MESSAGES.txt");
      if((b_gr[i]->book->parser_log = fopen(parsedMsgFileName.c_str(),"w")) == NULL) on_error ("Error %s",parsedMsgFileName.c_str());
      EKA_LOG("%s:%u created file %s",EKA_EXCH_DECODE(exch),b_gr[i]->id,parsedMsgFileName.c_str());
    }

  }
  return 0;
}




static int closeGap(EkaFhMode op, EfhCtx* pEfhCtx,const EfhRunCtx* pEfhRunCtx,FhGroup* gr, uint64_t start, uint64_t end) {
  pthread_detach(pthread_self());

  EfhFeedDownMsg efhFeedDownMsg{ EfhMsgType::kFeedDown, {gr->exch, (EkaLSI)gr->id}, ++gr->gapNum };
  pEfhRunCtx->onEfhFeedDownMsgCb(&efhFeedDownMsg, 0, pEfhRunCtx->efhRunUserData);
  EkaDev* dev = pEfhCtx->dev;

#ifdef EKA_TEST_IGNORE_GAP
  gr->gapClosed = true;
  gr->seq_after_snapshot = end + 1;
  EKA_LOG("%s:%u FH_LAB DUMMY Gap closed, gr->seq_after_snapshot = %ju",EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot);
#else
  std::string threadNamePrefix = op == EkaFhMode::SNAPSHOT ? std::string("ST_") : std::string("RT_");
  std::string threadName = threadNamePrefix + std::string(EKA_EXCH_SOURCE_DECODE(gr->exch)) + '_' + std::to_string(gr->id);
  EkaFhThreadAttr* attr = new EkaFhThreadAttr(pEfhCtx, (const EfhRunCtx*)pEfhRunCtx, gr->id, gr, start, end, op);

  EKA_INFO("%s:%u launching Gap Closing thread",EKA_EXCH_DECODE(gr->exch),gr->id);

  switch (gr->exch) {
  case EkaSource::kNOM_ITTO  :
  case EkaSource::kGEM_TQF   :
  case EkaSource::kISE_TQF   :
  case EkaSource::kMRX_TQF   :
    if (op == EkaFhMode::SNAPSHOT) 
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedSnapshot,eka_get_glimpse_data,        (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->snapshot_thread);   
    else
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedRecovery,eka_get_mold_retransmit_data,(void*)attr,dev->createThreadContext,(uintptr_t*)&gr->retransmit_thread);   
    break;
  case EkaSource::kPHLX_TOPO :
    dev->createThread(threadName.c_str(),EkaThreadType::kFeedSnapshot,eka_get_glimpse_data,        (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->snapshot_thread); 
    break;
  case EkaSource::kPHLX_ORD :
    if (op == EkaFhMode::SNAPSHOT) 
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedSnapshot,eka_get_phlx_ord_glimpse_data,        (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->snapshot_thread);   
    else
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedRecovery,eka_get_phlx_mold_retransmit_data,(void*)attr,dev->createThreadContext,(uintptr_t*)&gr->retransmit_thread);   
    break;
  case EkaSource::kC1_PITCH   :
  case EkaSource::kC2_PITCH   :
  case EkaSource::kBZX_PITCH  :
  case EkaSource::kEDGX_PITCH :
    if (op == EkaFhMode::SNAPSHOT) 
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedSnapshot,eka_get_spin_data,          (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->snapshot_thread);   
    else
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedRecovery,eka_get_grp_retransmit_data,(void*)attr,dev->createThreadContext,(uintptr_t*)&gr->retransmit_thread);   
    break;
  case EkaSource::kMIAX_TOM  :
  case EkaSource::kPEARL_TOM :
    if (op == EkaFhMode::SNAPSHOT) 
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedSnapshot,eka_get_sesm_data,          (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->snapshot_thread);   
    else
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedRecovery,eka_get_sesm_retransmit,    (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->retransmit_thread);   
    break;
  case EkaSource::kBOX_HSVF :
      dev->createThread(threadName.c_str(),EkaThreadType::kFeedRecovery,eka_get_hsvf_retransmit,    (void*)attr,dev->createThreadContext,(uintptr_t*)&gr->retransmit_thread);   
    break;
  default:
    on_error("%s:%u: Unsupported Snapshot",EKA_EXCH_DECODE(gr->exch),gr->id);
  }
#endif
  //  EKA_DEBUG("%s:%u GAP CLOSED: Seq_after_snapshot=%ju, gr->gapClosed=%d, gr->state = %d",EKA_EXCH_DECODE(gr->exch),gr->id,gr->seq_after_snapshot,gr->gapClosed, gr->state);

  return 0;
}

/* ##################################################################### */


/* ##################################################################### */

inline uint8_t EkaFh::getGrId(const uint8_t* pkt) {
  EkaIpHdr* ipHdr = (EkaIpHdr*)(pkt - 8 - 20);
  uint32_t ip = ipHdr->dest;
  for (uint8_t i = 0; i < groups; i++)
    if (b_gr[i]->mcast_ip == ip && be16toh(b_gr[i]->mcast_port) == EKA_UDPHDR_DST((pkt-8)))
      return i;
  
  EKA_WARN("ip 0x%08x %s not found",ip, EKA_IP2STR(ip));
  
  return 0xFF;
}
/* ##################################################################### */

uint8_t FhBats::getGrId(const uint8_t* pkt) {
  for (uint8_t i = 0; i < groups; i++) {
    /* FhBatsGr* gr = dynamic_cast<FhBatsGr*>(b_gr[i]); */
    /* if (gr == NULL) on_error("cannot convert gr to FhBatsGr"); */
    FhBatsGr* gr = static_cast<FhBatsGr*>(b_gr[i]);
    if ((be16toh(gr->mcast_port) == EKA_UDPHDR_DST((pkt-8))) && (EKA_BATS_UNIT(pkt) == (gr->batsUnit))) 
      return i;
  }
  return 0xFF;
}
/* ##################################################################### */

/* ##################################################################### */
bool FhRunGr::drainQ(const EfhRunCtx* pEfhRunCtx) {
  //  EKA_LOG("hasGrpAfterGap = %d",hasGrpAfterGap);
  if (hasGrpAfterGap) {
    FhGroup* gr = fh->b_gr[getGrAfterGap()];
    while (! gr->q->is_empty()) {
      fh_msg* buf = gr->q->pop();
      //      EKA_LOG("q_len=%u,buf->sequence=%ju, gr->expected_sequence=%ju",gr->q->get_len(),buf->sequence,gr->expected_sequence);
      if (buf->sequence < gr->expected_sequence) continue;
      gr->parseMsg(pEfhRunCtx,(unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
      gr->expected_sequence = buf->sequence + 1;
    }
    clearGrAfterGap(gr->id);
    return true;
  }
  return false;
}
/* ##################################################################### */
uint8_t* FhNasdaq::getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id) {
  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));
  uint msgCnt = EKA_MOLD_MSG_CNT(pkt);
  uint8_t grId = getGrId(pkt);
  if (msgCnt == 0xFFFF) {
    runGr->stoppedByExchange = true;
    return NULL; // EndOfMold
  }
  if (grId == 0xFF || (! runGr->isMyGr(grId)) || b_gr[grId] == NULL || b_gr[grId]->q == NULL) { 
    if (grId != 0) EKA_LOG("RunGr%u: Skipping gr_id = %u not belonging to %s",runGr->runId,grId,runGr->list2print);
    runGr->udpCh->next(); 
    return NULL;
  }
  FhGroup* gr = b_gr[grId];
  if (gr->firstPkt) {
    memcpy((uint8_t*)gr->session_id,((struct mold_hdr*)pkt)->session_id,10);
    gr->firstPkt = false;
    EKA_LOG("%s:%u session_id is set to %s",EKA_EXCH_DECODE(exch),grId,(char*)gr->session_id + '\0');
  }
  *msgInPkt = msgCnt;
  *sequence = EKA_MOLD_SEQUENCE(pkt);
  *gr_id = grId;
  return pkt;
}
/* ##################################################################### */
uint8_t* FhPhlxOrd::getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id) {
  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));
  uint msgCnt = EKA_PHLX_MOLD_MSG_CNT(pkt);
  uint8_t grId = getGrId(pkt);
  if (msgCnt == 0xFFFF) {
    runGr->stoppedByExchange = true;
    return NULL; // EndOfMold
  }
  if (grId == 0xFF || (! runGr->isMyGr(grId)) || b_gr[grId] == NULL || b_gr[grId]->q == NULL) { 
    if (grId != 0) EKA_LOG("RunGr%u: Skipping gr_id = %u not belonging to %s",runGr->runId,grId,runGr->list2print);
    runGr->udpCh->next(); 
    return NULL;
  }
  FhGroup* gr = b_gr[grId];
  if (gr->firstPkt) {
    memcpy((uint8_t*)gr->session_id,((PhlxMoldHdr*)pkt)->session_id,10);
    gr->firstPkt = false;
    EKA_LOG("%s:%u session_id is set to %s",EKA_EXCH_DECODE(exch),grId,(char*)gr->session_id + '\0');
  }
  *msgInPkt = msgCnt;
  *sequence = EKA_PHLX_MOLD_SEQUENCE(pkt);
  *gr_id = grId;
  return pkt;
}
/* ##################################################################### */
uint8_t* FhBats::getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id) {
  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  uint msgCnt = EKA_BATS_MSG_CNT((pkt));
  uint8_t grId = getGrId(pkt);

  if (grId == 0xFF || (! runGr->isMyGr(grId)) || b_gr[grId] == NULL || b_gr[grId]->q == NULL) { 
    if (grId != 0) EKA_LOG("RunGr%u: Skipping gr_id = %u not belonging to %s",runGr->runId,grId,runGr->list2print);
    runGr->udpCh->next(); 
    return NULL;
  }

  *msgInPkt = msgCnt;
  *sequence = EKA_BATS_SEQUENCE((pkt));
  *gr_id = grId;
  return pkt;
}
/* ##################################################################### */
uint8_t* FhXdp::getUdpPkt(FhRunGr* runGr, uint* msgInPkt, uint* pktSize, uint64_t* sequence,uint8_t* gr_id,uint16_t* streamId, uint8_t* pktType) {
  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *msgInPkt = EKA_XDP_MSG_CNT((pkt));
  *sequence = EKA_XDP_SEQUENCE((pkt));
  *gr_id    = getGrId(pkt);
  *streamId = EKA_XDP_STREAM_ID((pkt));
  *pktType =  EKA_XDP_PKT_TYPE((pkt));
  *pktSize =  EKA_XDP_PKT_SIZE((pkt));

  return pkt;
}
/* ##################################################################### */
uint8_t* FhMiax::getUdpPkt(FhRunGr* runGr, uint* msgInPkt, int16_t* pktLen, uint64_t* sequence,uint8_t* gr_id) {
  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *pktLen = runGr->udpCh->getPayloadLen();
  *gr_id    = getGrId(pkt);
  *sequence = EKA_GET_MACH_SEQ((pkt));
  *msgInPkt = 16; // just a number, greater than amount of messages in a packet
  return pkt;
}
/* ##################################################################### */
uint8_t* FhBox::getUdpPkt(FhRunGr* runGr, uint16_t* pktLen, uint64_t* sequence, uint8_t* gr_id) {

  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *pktLen   = runGr->udpCh->getPayloadLen();
  *gr_id    = getGrId(pkt);
  *sequence = getHsvfMsgSequence(pkt);

  return pkt;
}
/* ##################################################################### */
bool FhNasdaq::processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t seq) {
  uint indx = sizeof(mold_hdr);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(pkt[indx]));
    uint8_t* msgData = (uint8_t*)&pkt[indx + sizeof(msg_len)];
    //-----------------------------------------------------------------------------
    if (gr->parseMsg(pEfhRunCtx,msgData,sequence++,EkaFhMode::MCAST)) return true;
    //-----------------------------------------------------------------------------
    gr->expected_sequence = sequence;
    indx += msg_len + sizeof(msg_len);
  }
  return false;
}
/* ##################################################################### */
bool FhPhlxOrd::processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t seq) {
  uint indx = sizeof(PhlxMoldHdr);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = (uint16_t) *(uint16_t*)&(pkt[indx]);
    if (msg_len == 0) {
      EKA_WARN("%s:%u Mold session is terminated",EKA_EXCH_DECODE(exch),gr->id);
      return true;
    }

    uint8_t* msgData = (uint8_t*)&pkt[indx + sizeof(msg_len)];
    //-----------------------------------------------------------------------------
    if (gr->parseMsg(pEfhRunCtx,msgData,sequence++,EkaFhMode::MCAST)) return true;
    //-----------------------------------------------------------------------------
    gr->expected_sequence = sequence;
    indx += msg_len + sizeof(msg_len);
  }
  return false;
}
/* ##################################################################### */
bool FhMiax::processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhMiaxGr* gr, const uint8_t* pkt, int16_t pktLen) {
  if (pktLen < (int)sizeof(EkaMiaxMach)) on_error ("pktLen = %d < sizeof(EkaMiaxMach) %ju",pktLen,sizeof(EkaMiaxMach));
  int16_t processedBytes = 0;
  while (processedBytes < pktLen) {
    uint64_t sequence = EKA_GET_MACH_SEQ(pkt);
    uint machPktLen = EKA_GET_MACH_LEN(pkt);

    if (sequence == gr->expected_sequence) {
      MiaxMachType machType = EKA_GET_MACH_TYPE(pkt);
      switch (machType) {
      case MiaxMachType::Heartbeat : 
      case MiaxMachType::StartOfSession :
	break;
      case MiaxMachType::EndOfSession : 
	EKA_WARN("%s:%u : MiaxMachType::EndOfSession",EKA_EXCH_DECODE(exch),gr->id);
	return true;
      case MiaxMachType::AppData : {
	uint8_t* msgData = (uint8_t*)pkt + sizeof(EkaMiaxMach);
	gr->parseMsg(pEfhRunCtx,msgData,sequence,EkaFhMode::MCAST);
	gr->expected_sequence = sequence + 1;
      }
	break;
      default:
	on_error("%s:%u : Unexpected Mach Type %u",EKA_EXCH_DECODE(exch),gr->id,(uint8_t)machType);
      }
    }
    processedBytes += machPktLen;
    if (processedBytes > pktLen) on_error("processedBytes %d > pktLen %d",processedBytes,pktLen);
    pkt += machPktLen;
  }

  return false;
}
/* ##################################################################### */
void FhMiax::pushUdpPkt2Q(FhMiaxGr* gr, const uint8_t* pkt, int16_t pktLen, int8_t gr_id) {
  if (pktLen < (int)sizeof(EkaMiaxMach)) on_error ("pktLen = %d < sizeof(EkaMiaxMach) %ju",pktLen,sizeof(EkaMiaxMach));
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
      EKA_WARN("%s:%u : MiaxMachType::EndOfSession",EKA_EXCH_DECODE(exch),gr->id);
      return;
    case MiaxMachType::AppData : {
      uint8_t* msgData = (uint8_t*)pkt + sizeof(EkaMiaxMach);
      uint16_t msg_len = machPktLen - sizeof(EkaMiaxMach);
      assert (msg_len <= fh_msg::MSG_SIZE);
      fh_msg* n = gr->q->push();
      memcpy (n->data,msgData,msg_len);
      n->sequence = sequence;
      n->gr_id = gr_id;
    }
      break;
    default:
      on_error("%s:%u : Unexpected Mach Type %u",EKA_EXCH_DECODE(exch),gr->id,(uint8_t)machType);
    }
    processedBytes += machPktLen;
    if (processedBytes > pktLen) on_error("processedBytes %d > pktLen %d",processedBytes,pktLen);
    pkt += machPktLen;
  }
}
/* ##################################################################### */
void FhNasdaq::pushUdpPkt2Q(FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence,int8_t gr_id) {
  uint indx = sizeof(mold_hdr);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = be16toh((uint16_t) *(uint16_t*)&(pkt[indx]));
    assert (msg_len <= fh_msg::MSG_SIZE);
    fh_msg* n = gr->q->push();
    memcpy (n->data,&pkt[indx+sizeof(msg_len)],msg_len);
    n->sequence = sequence++;
    n->gr_id = gr_id;
    indx += msg_len + sizeof(msg_len);
  }
  return;
}
/* ##################################################################### */
void FhPhlxOrd::pushUdpPkt2Q(FhNasdaqGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence,int8_t gr_id) {
  uint indx = sizeof(PhlxMoldHdr);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = (uint16_t) *(uint16_t*)&(pkt[indx]);
    assert (msg_len <= fh_msg::MSG_SIZE);
    fh_msg* n = gr->q->push();
    memcpy (n->data,&pkt[indx+sizeof(msg_len)],msg_len);
    n->sequence = sequence++;
    n->gr_id = gr_id;
    indx += msg_len + sizeof(msg_len);
  }
  return;
}
/* ##################################################################### */
void FhBox::pushUdpPkt2Q(FhBoxGr* gr, const uint8_t* pkt, int16_t pktLen, int8_t gr_id) {
  uint8_t* p = (uint8_t*)pkt;
  int idx = 0;
  while (idx < pktLen) {
    uint msgLen = getHsvfMsgLen(&p[idx],pktLen - idx);
    char* msgType = ((HsvfMsgHdr*) &p[idx+1])->MsgType;
    if (memcmp(msgType,"F ",2) == 0 ||  // Quote
	memcmp(msgType,"Z ",2) == 0) {  // Time
      if (msgLen > fh_msg::MSG_SIZE) on_error("msgLen %u > fh_msg::MSG_SIZE %u",msgLen,fh_msg::MSG_SIZE);
      fh_msg* n = gr->q->push();
      memcpy (n->data,&p[idx+1],msgLen - 1);
      n->sequence = getHsvfMsgSequence(p);
      n->gr_id = gr_id;
    }
    idx += msgLen;
    idx += trailingZeros(&p[idx],pktLen-idx);

  }
  return;
}
/* ##################################################################### */
bool FhBats::processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhBatsGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t seq) {
  uint indx = sizeof(batspitch_sequenced_unit_header);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint8_t msg_len = pkt[indx];
    uint8_t* msgData = (uint8_t*)&pkt[indx];
    //-----------------------------------------------------------------------------
    if (gr->parseMsg(pEfhRunCtx,msgData,sequence++,EkaFhMode::MCAST)) return true;
    //-----------------------------------------------------------------------------
    gr->expected_sequence = sequence;
    indx += msg_len;
  }
  return false;
}
/* ##################################################################### */
bool FhXdp::processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhXdpGr* gr, uint pktSize, uint streamIdx, const uint8_t* pktPtr, uint msgInPkt, uint64_t seq) {
  uint8_t* p = (uint8_t*)pktPtr + sizeof(XdpPktHdr);//+sizeof(XdpStreamId);
  uint64_t sequence = seq;
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint16_t msg_len = EKA_XDP_MSG_LEN(p);
    //    if (sequence == gr->getExpectedSeq(streamIdx)) { // = ignore back in time messages

    //    EKA_LOG("pktSize=%u, delta_p=%ju, MsgType=%u, MsgSize=%u",pktSize,p - pktPtr,((XdpMsgHdr*)p)->MsgType,msg_len);
    //-----------------------------------------------------------------------------
    gr->parseMsg(pEfhRunCtx,p,sequence++,EkaFhMode::MCAST); // never end of MC from Msg
    //-----------------------------------------------------------------------------
    gr->setExpectedSeq(streamIdx,sequence);
    //    } 
    p += msg_len;
  }
  return false;
}
/* ##################################################################### */
bool FhBox::processUdpPkt(const EfhRunCtx* pEfhRunCtx,FhBoxGr* gr, const uint8_t* pkt, int16_t pktLen) {
  //  EKA_LOG("%s:%u : pktLen = %u",EKA_EXCH_DECODE(gr->exch),gr->id,pktLen);

  uint8_t* p = (uint8_t*)pkt;
  int idx = 0;
  while (idx < pktLen) {
    uint msgLen       = getHsvfMsgLen(&p[idx],pktLen-idx);
    uint64_t sequence = getHsvfMsgSequence(&p[idx]);
    if (sequence >= gr->expected_sequence) {
      if (gr->parseMsg(pEfhRunCtx,&p[idx+1],sequence,EkaFhMode::MCAST)) return true;
      gr->expected_sequence = sequence + 1;
    }

    idx += msgLen;
    idx += trailingZeros(&p[idx],pktLen-idx );
  }
  return false;
}
/* ##################################################################### */
void FhBats::pushUdpPkt2Q(FhBatsGr* gr, const uint8_t* pkt, uint msgInPkt, uint64_t sequence,int8_t gr_id) {
  uint indx = sizeof(batspitch_sequenced_unit_header);
  for (uint msg=0; msg < msgInPkt; msg++) {
    uint8_t msg_len = pkt[indx];
    if (msg_len > fh_msg::MSG_SIZE) on_error("msg_len %u > fh_msg::MSG_SIZE %u",msg_len,fh_msg::MSG_SIZE);
    fh_msg* n = gr->q->push();
    memcpy (n->data,&pkt[indx],msg_len);
    n->sequence = sequence++;
    n->gr_id = gr_id;
    indx += msg_len;
  }
  return;
}

/* ##################################################################### */


EkaOpResult FhNasdaq::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  if (exch == EkaSource::kPHLX_TOPO  && isPreTradeTime(9,27)) {
    for (uint8_t j = 0; j < runGr->numGr; j++) {
      uint8_t grId = runGr->groupList[j];
      EKA_LOG("%s:%u: Running PreTrade Snapshot",EKA_EXCH_DECODE(exch),b_gr[grId]->id);
      b_gr[grId]->snapshotThreadDone = false;
      closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,b_gr[grId], 1, 1);
      while (! b_gr[grId]->snapshotThreadDone) {} // instead of thread.join()
      b_gr[grId]->expected_sequence = b_gr[grId]->recovery_sequence + 1;
      EKA_LOG("%s:%u: PreTrade Glimpse Snapshot is done, expected_sequence = %ju",EKA_EXCH_DECODE(exch),b_gr[grId]->id,b_gr[grId]->expected_sequence);
      b_gr[grId]->state = FhGroup::GrpState::NORMAL;

      EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {b_gr[grId]->exch, (EkaLSI)b_gr[grId]->id}, b_gr[grId]->gapNum };
      pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
    }
  }

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;
    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;

    uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (pkt == NULL) continue;
    FhNasdaqGr* gr = (FhNasdaqGr*)b_gr[gr_id];
    //-----------------------------------------------------------------------------
    switch (gr->state) {
    case FhGroup::GrpState::INIT : { 
      gr->gapClosed = false;
      gr->state = FhGroup::GrpState::SNAPSHOT_GAP;

      closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,gr, 1, 0);
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = FhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;
	if (exch == EkaSource::kPHLX_TOPO) {
	  pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);
	  closeGap(EkaFhMode::RECOVERY, pEfhCtx,pEfhRunCtx,gr, gr->expected_sequence, sequence);
	} else {
	  closeGap(EkaFhMode::RECOVERY, pEfhCtx,pEfhRunCtx,gr, gr->expected_sequence, sequence + msgInPkt);
	}
      } else { // NORMAL
	runGr->stoppedByExchange = processUdpPkt(pEfhRunCtx,gr,pkt,msgInPkt,sequence);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::SNAPSHOT_GAP : {
      if (exch == EkaSource::kPHLX_TOPO) {
	if (sequence + msgInPkt < gr->recovery_sequence) {
	  gr->gapClosed = true;
	  gr->snapshot_active = false;
	  gr->seq_after_snapshot = gr->recovery_sequence + 1;

	  EKA_DEBUG("%s:%u Generating TOB quote for every Security",EKA_EXCH_DECODE(gr->exch),gr->id);
	  ((TobBook*)gr->book)->sendTobImage(pEfhRunCtx);
	}
      } else {
	pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);
      }
      if (gr->gapClosed) {
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed - expected_sequence=%ju",EKA_EXCH_DECODE(exch),gr->id,gr->expected_sequence);
      }
    }
      break;
    case FhGroup::GrpState::RETRANSMIT_GAP : {
      pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);
      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, switching to fetch from Q",EKA_EXCH_DECODE(exch),gr->id);
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
      }
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  return EKA_OPRESULT__OK;
}

/* ##################################################################### */


EkaOpResult FhPhlxOrd::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;
    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;

    uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (pkt == NULL) continue;
    FhNasdaqGr* gr = (FhNasdaqGr*)b_gr[gr_id];
    //-----------------------------------------------------------------------------
    switch (gr->state) {
    case FhGroup::GrpState::INIT : { 
      gr->gapClosed = false;
      gr->state = FhGroup::GrpState::SNAPSHOT_GAP;

      closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,gr, 1, 2);
      gr->expected_sequence = 2;
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = FhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,gr, gr->expected_sequence, sequence + msgInPkt);
      } else { // NORMAL
	runGr->stoppedByExchange = processUdpPkt(pEfhRunCtx,gr,pkt,msgInPkt,sequence);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::SNAPSHOT_GAP : {
      if (exch == EkaSource::kPHLX_TOPO || exch == EkaSource::kPHLX_ORD) {
	if (sequence + msgInPkt < gr->recovery_sequence) {
	  gr->gapClosed = true;
	  gr->snapshot_active = false;
	  gr->seq_after_snapshot = gr->recovery_sequence + 1;

	  EKA_DEBUG("%s:%u Generating TOB quote for every Security",EKA_EXCH_DECODE(gr->exch),gr->id);
	  ((TobBook*)gr->book)->sendTobImage(pEfhRunCtx);
	}
      } else {
	pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);
      }
      if (gr->gapClosed) {
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	//	gr->expected_sequence = gr->seq_after_snapshot;
	gr->expected_sequence = 2;
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed - expected_sequence=%ju",EKA_EXCH_DECODE(exch),gr->id,gr->expected_sequence);
      }
    }
      break;
    case FhGroup::GrpState::RETRANSMIT_GAP : {
      pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);
      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, switching to fetch from Q",EKA_EXCH_DECODE(exch),gr->id);
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
      }
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  return EKA_OPRESULT__OK;
}

/* ##################################################################### */

EkaOpResult FhBats::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;

    uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (pkt == NULL) continue;
    FhBatsGr* gr = (FhBatsGr*)b_gr[gr_id];

    //-----------------------------------------------------------------------------
    switch (gr->state) {
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::INIT : {
      if (any_group_getting_snapshot) break; // only 1 group can get snapshot at a time
      any_group_getting_snapshot = true;

      gr->gapClosed = false;
      gr->state = FhGroup::GrpState::SNAPSHOT_GAP;

      closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,gr, 0, 0);
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = FhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	closeGap(EkaFhMode::RECOVERY, pEfhCtx,pEfhRunCtx,gr, gr->expected_sequence, sequence + msgInPkt);
      } else { // NORMAL
	runGr->stoppedByExchange = processUdpPkt(pEfhRunCtx,gr,pkt,msgInPkt,sequence);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::SNAPSHOT_GAP : {
      pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed",EKA_EXCH_DECODE(exch),gr->id);
	gr->state = FhGroup::GrpState::NORMAL;
	any_group_getting_snapshot = false;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
      }
    }
      break;
    case FhGroup::GrpState::RETRANSMIT_GAP : {
      pushUdpPkt2Q(gr,pkt,msgInPkt,sequence,gr_id);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, switching to fetch from Q",EKA_EXCH_DECODE(exch),gr->id);
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
      }
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
    runGr->udpCh->next(); 
  }
  return EKA_OPRESULT__OK;

}
 /* ##################################################################### */

EkaOpResult FhMiax::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t gr_id = 0xFF;
    int16_t pktLen = 0;

    uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&pktLen,&sequence,&gr_id);
    if (pkt == NULL) continue;
    if (gr_id == 0xFF) {
      runGr->udpCh->next(); 
      continue;
    }
    FhMiaxGr* gr = (FhMiaxGr*)b_gr[gr_id];
    if (gr == NULL) on_error("gr = NULL");

    //-----------------------------------------------------------------------------
    switch (gr->state) {
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::INIT : {
      if (any_group_getting_snapshot) break; // only 1 group can get snapshot at a time
      any_group_getting_snapshot = true;

      gr->gapClosed = false;
      gr->state = FhGroup::GrpState::SNAPSHOT_GAP;

      closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,gr, 0, 0);
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = FhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	closeGap(EkaFhMode::RECOVERY, pEfhCtx,pEfhRunCtx,gr, gr->expected_sequence, sequence + msgInPkt);
      } else { // NORMAL
	runGr->stoppedByExchange = processUdpPkt(pEfhRunCtx,gr,pkt,pktLen);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::SNAPSHOT_GAP : {
      pushUdpPkt2Q(gr,pkt,pktLen,gr_id);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed: gr->seq_after_snapshot = %ju",EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = FhGroup::GrpState::NORMAL;
	any_group_getting_snapshot = false;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot + 1;
      }
    }
      break;
    case FhGroup::GrpState::RETRANSMIT_GAP : {
      pushUdpPkt2Q(gr,pkt,pktLen,gr_id);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, gr->seq_after_snapshot = %ju, switching to fetch from Q",EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot + 1;
      }
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
    runGr->udpCh->next(); 
  }
  return EKA_OPRESULT__OK;

}
 /* ##################################################################### */

EkaOpResult FhXdp::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);
  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  for (uint8_t j = 0; j < runGr->numGr; j++) {
    uint8_t grId = runGr->groupList[j];
    EfhFeedDownMsg efhFeedDownMsg{ EfhMsgType::kFeedDown, {b_gr[grId]->exch, (EkaLSI)b_gr[grId]->id}, ++b_gr[grId]->gapNum };
    pEfhRunCtx->onEfhFeedDownMsgCb(&efhFeedDownMsg, 0, pEfhRunCtx->efhRunUserData);
    ((FhXdpGr*)b_gr[grId])->inGap = true;
    ((FhXdpGr*)b_gr[grId])->setGapStart();
  }

  while (runGr->thread_active) {
    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;
    uint16_t streamId = 0;
    uint8_t  pktType = 0;
    uint     pktSize = 0; 

    uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&pktSize,&sequence,&gr_id, &streamId, &pktType);
    if (pkt == NULL) continue;

    FhXdpGr* gr = (FhXdpGr*)b_gr[gr_id];
    uint streamIdx = gr->findAndInstallStream(streamId, sequence);

    /* EKA_LOG("%s:%u Seq=%ju,expSeq=%ju, pktSize=%u msgInPkt =%u",EKA_EXCH_DECODE(exch),gr_id, */
    /* 	    sequence,gr->getExpectedSeq(streamIdx),pktSize,msgInPkt); */


    //-----------------------------------------------------------------------------
    if (pktType == (uint8_t)EKA_XDP_DELIVERY_FLAG::SequenceReset) gr->resetExpectedSeq(streamIdx);
    //-----------------------------------------------------------------------------
    if (gr->inGap) {
      if (gr->isGapOver()) {
	gr->inGap = false;
	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
      }
    }
    //-----------------------------------------------------------------------------
    if (sequence > gr->getExpectedSeq(streamIdx) && ! gr->inGap) {
      EfhFeedDownMsg efhFeedDownMsg{ EfhMsgType::kFeedDown, {gr->exch, (EkaLSI)gr->id}, ++gr->gapNum };
      pEfhRunCtx->onEfhFeedDownMsgCb(&efhFeedDownMsg, 0, pEfhRunCtx->efhRunUserData);

      EKA_LOG("%s:%u for Stream %u expectedSeq = %u < sequence %u, lost %u messages",
	      EKA_EXCH_DECODE(exch),gr->id,gr->getId(streamIdx),gr->getExpectedSeq(streamIdx),sequence,sequence-gr->getExpectedSeq(streamIdx));
      gr->inGap = true;
      gr->setGapStart();
    }
    //-----------------------------------------------------------------------------

    if (processUdpPkt(pEfhRunCtx,gr,pktSize,streamIdx,pkt,msgInPkt,(uint64_t)sequence)) break;

    //-----------------------------------------------------------------------------
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaOpResult FhBox::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  FhRunGr* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);
  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) continue;
    uint8_t gr_id = 0xFF;
    uint16_t pktLen = 0;
    uint64_t sequence = 0;
    const uint8_t* pkt = getUdpPkt(runGr,&pktLen,&sequence,&gr_id);
    if (pkt == NULL) continue;
   
    FhBoxGr* gr = (FhBoxGr*)b_gr[gr_id];
    if (gr == NULL) on_error("gr == NULL");
    //-----------------------------------------------------------------------------
    switch (gr->state) {
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::INIT : {
      gr->gapClosed = false;
      gr->state = FhGroup::GrpState::SNAPSHOT_GAP;

      closeGap(EkaFhMode::SNAPSHOT, pEfhCtx,pEfhRunCtx,gr, 1, sequence + 10 /* max messages in Pkt */);
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::NORMAL : {
      //      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = FhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	pushUdpPkt2Q(gr,pkt,pktLen,gr_id);

	closeGap(EkaFhMode::RECOVERY, pEfhCtx,pEfhRunCtx,gr, gr->expected_sequence, sequence);
      } else { // NORMAL
	runGr->stoppedByExchange = processUdpPkt(pEfhRunCtx,gr,pkt,pktLen);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::SNAPSHOT_GAP : {
      if (gr->gapClosed) {; // ignore UDP pkt during initial Snapshot
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed: last snapshot sequence = %ju",EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = FhGroup::GrpState::NORMAL;

	EKA_DEBUG("%s:%u Generating TOB quote for every Security",EKA_EXCH_DECODE(gr->exch),gr->id);
	((TobBook*)gr->book)->sendTobImage64(pEfhRunCtx);

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);

	gr->expected_sequence = gr->seq_after_snapshot;      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case FhGroup::GrpState::RETRANSMIT_GAP : {
      pushUdpPkt2Q(gr,pkt,pktLen,gr_id);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed: last retransmitted sequence = %ju",EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = FhGroup::GrpState::NORMAL;

	EfhFeedUpMsg efhFeedUpMsg{ EfhMsgType::kFeedUp, {gr->exch, (EkaLSI)gr->id}, gr->gapNum };
	pEfhRunCtx->onEfhFeedUpMsgCb(&efhFeedUpMsg, 0, pEfhRunCtx->efhRunUserData);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;     
      } 
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
      //-----------------------------------------------------------------------------
    }
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  return EKA_OPRESULT__OK;
}
/* ##################################################################### */

EkaOpResult FhNasdaq::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  EkaFhThreadAttr* attr = new EkaFhThreadAttr(pEfhCtx, pEfhRunCtx, (uint8_t)group->localId, b_gr[(uint8_t)group->localId], 1, 0, EkaFhMode::DEFINITIONS);
  //  eka_get_glimpse_data(pEfhCtx,pEfhRunCtx,b_gr[(uint8_t)group->localId],EkaFhMode::DEFINITIONS,1,0);
  eka_get_glimpse_data(attr);
  while (! b_gr[(uint8_t)group->localId]->heartbeatThreadDone) {
    sleep (0);
  }
  return EKA_OPRESULT__OK;
}

 /* ##################################################################### */

EkaOpResult FhBats::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  EkaFhThreadAttr* attr = new EkaFhThreadAttr(pEfhCtx, pEfhRunCtx, (uint8_t)group->localId, b_gr[(uint8_t)group->localId], 1, 0, EkaFhMode::DEFINITIONS);
  eka_get_spin_data(attr);
  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaOpResult FhMiax::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  EkaFhThreadAttr* attr = new EkaFhThreadAttr(pEfhCtx, pEfhRunCtx, (uint8_t)group->localId, b_gr[(uint8_t)group->localId], 1, 0, EkaFhMode::DEFINITIONS);
  eka_get_sesm_data(attr);
  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaOpResult FhXdp::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  return eka_get_xdp_definitions(pEfhCtx,pEfhRunCtx,(FhXdpGr*)b_gr[(uint8_t)group->localId],EkaFhMode::DEFINITIONS);
}
 /* ##################################################################### */

EkaOpResult FhBox::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  //  EKA_LOG("Defintions for %s",EKA_PRINT_GRP(group));fflush(stdout);fflush(stderr);

  return eka_hsvf_get_definitions(pEfhCtx, pEfhRunCtx,(FhBoxGr*)b_gr[(uint8_t)group->localId]);
}

 /* ##################################################################### */

EkaFhThreadAttr::EkaFhThreadAttr(EfhCtx* efhCtx, const EfhRunCtx* efhRunCtx, uint rungrid, FhGroup* fhGr, uint64_t start, uint64_t end, EkaFhMode   oper) {
  pEfhCtx = efhCtx;
  pEfhRunCtx = (EfhRunCtx*)efhRunCtx;
  gr = fhGr;
  startSeq = start;
  endSeq = end;
  runGrId = rungrid;
  op = oper;
}
