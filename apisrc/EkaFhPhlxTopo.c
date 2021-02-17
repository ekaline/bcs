#include "EkaFhPhlxTopo.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhPhlxTopoGr.h"

 /* ##################################################################### */

static inline bool isPreTradeTime(int hour, int minute) {
  time_t rawtime;
  time (&rawtime);
  struct tm * ct = localtime (&rawtime);
  if (ct->tm_hour < hour || (ct->tm_hour == hour && ct->tm_min < minute)) return true;
  return false;
}

/* ##################################################################### */
EkaFhGroup*          EkaFhPhlxTopo::addGroup() {
  return new EkaFhPhlxTopoGr();
}


/* ##################################################################### */

EkaOpResult EkaFhPhlxTopo::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");
  
  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);
  
  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  if (isPreTradeTime(9,27)) {
    for (uint8_t j = 0; j < runGr->numGr; j++) {
      uint8_t grId = runGr->groupList[j];
      EkaFhPhlxTopoGr* gr = (EkaFhPhlxTopoGr*)b_gr[grId];
      if (gr == NULL) on_error("b_gr[%u] == NULL",grId);

      EKA_LOG("%s:%u: Running PreTrade Snapshot",EKA_EXCH_DECODE(exch),gr->id);
      gr->snapshotThreadDone = false;

      gr->sendFeedDownInitial(pEfhRunCtx);

      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx,1, 1);

      while (! gr->snapshotThreadDone) {} // instead of thread.join()
      gr->expected_sequence = gr->recovery_sequence + 1;
      EKA_LOG("%s:%u: PreTrade Soupbin Snapshot is done, expected_sequence = %ju",
	      EKA_EXCH_DECODE(exch),gr->id,gr->expected_sequence);

      gr->state = EkaFhGroup::GrpState::NORMAL;
      gr->sendFeedUpInitial(pEfhRunCtx);
    }
  }

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;
    //-----------------------------------------------------------------------------

    if (! runGr->udpCh->has_data()) {
      if (++timeCheckCnt % TimeCheckRate == 0) {
	tradingHours = isTradingHours(9,30,16,00);
      }
      if (tradingHours)   runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }

    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id    = 0xFF;

    const uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (pkt == NULL) continue;

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (sequence != 0 && sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: (GAP_INJECT_INTERVAL = %d): pkt sequence %ju with %u messages dropped",
	       EKA_EXCH_DECODE(exch),gr_id, _EFH_TEST_GAP_INJECT_INTERVAL_,sequence,msgInPkt);
      runGr->udpCh->next(); 
      continue;
    }
#endif

    EkaFhPhlxTopoGr* gr = (EkaFhPhlxTopoGr*)b_gr[gr_id];
    if (gr == NULL) on_error("b_gr[%u] = NULL",gr_id);
    gr->resetNoMdTimer();

    //-----------------------------------------------------------------------------
    switch (gr->state) {
    case EkaFhGroup::GrpState::INIT : { 
      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;

      gr->sendFeedDownInitial(pEfhRunCtx);

      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, 1, 0);
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
	gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);

	gr->closeIncrementalGap(pEfhCtx,pEfhRunCtx,gr->expected_sequence,sequence);

      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      if (sequence <= gr->recovery_sequence) {
	// Recovery feed sequence took over the MCAST sequence
	gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);

	gr->gapClosed = true;
	gr->snapshot_active = false;
	gr->seq_after_snapshot = gr->recovery_sequence + 1;
	  
	EKA_DEBUG("%s:%u Generating TOB quote for every Security",
		  EKA_EXCH_DECODE(gr->exch),gr->id);
	gr->book->sendTobImage(pEfhRunCtx);
      }
      if (gr->gapClosed) {
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed - expected_sequence=%ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->expected_sequence);
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::RETRANSMIT_GAP : {
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, switching to fetch from Q",
		EKA_EXCH_DECODE(exch),gr->id);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx);
	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
      }
    }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
	       EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    } // switch
    runGr->udpCh->next(); 
  } // while
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}



