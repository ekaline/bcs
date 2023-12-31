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

#include "EkaFhPhlxOrd.h"
#include "EkaFhPhlxOrdGr.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"

/* ##################################################################### */
EkaFhGroup*          EkaFhPhlxOrd::addGroup() {
  return new EkaFhPhlxOrdGr();
}

/* ##################################################################### */

const uint8_t* EkaFhPhlxOrd::getUdpPkt(EkaFhRunGroup* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id) {
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
  EkaFhPhlxOrdGr* gr = dynamic_cast<EkaFhPhlxOrdGr*>(b_gr[grId]);
  if (gr == NULL) on_error("gr[%u] == NULL",grId);

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

EkaOpResult EkaFhPhlxOrd::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
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
    if (! runGr->udpCh->has_data()) {
			if (runGr->checkNoMd)
				runGr->checkGroupsNoMd(pEfhRunCtx);				
      continue;
    }
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;

    const uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (pkt == NULL) continue;
    EkaFhPhlxOrdGr* gr = (EkaFhPhlxOrdGr*)b_gr[gr_id];
    if (gr == NULL) on_error("b_gr[%u] = NULL",gr_id);
    gr->resetNoMdTimer();

    //-----------------------------------------------------------------------------
    switch (gr->state) {
    case EkaFhGroup::GrpState::INIT : { 
      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;

      gr->sendFeedDownInitial(pEfhRunCtx);
      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx,1, 2);
      gr->expected_sequence = 2;
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",
		EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	gr->sendFeedDown(pEfhRunCtx);
	//	gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, gr->expected_sequence, sequence + msgInPkt);
	gr->closeIncrementalGap(pEfhCtx,pEfhRunCtx, gr->expected_sequence, sequence + msgInPkt);
      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      if (sequence + msgInPkt < gr->recovery_sequence) {
	gr->gapClosed = true;
	gr->snapshot_active = false;
	gr->seq_after_snapshot = gr->recovery_sequence + 1;

	/* EKA_DEBUG("%s:%u Generating TOB quote for every Security", */
	/* 	  EKA_EXCH_DECODE(gr->exch),gr->id); */
	/* ((TobBook*)gr->book)->sendTobImage(pEfhRunCtx); */
      }

      if (gr->gapClosed) {
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->sendFeedUpInitial(pEfhRunCtx);

	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;

	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed - expected_sequence=%ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->expected_sequence);
      }
    }
      break;
    case EkaFhGroup::GrpState::RETRANSMIT_GAP : {
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, switching to fetch from Q",EKA_EXCH_DECODE(exch),gr->id);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx);

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
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}

/* ##################################################################### */
