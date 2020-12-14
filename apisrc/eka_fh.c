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
