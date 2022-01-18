#include "EkaFhNom.h"
#include "EkaFhNomGr.h"
#include "EkaFhNasdaq.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhNasdaqGr.h"
#include "EkaFhThreadAttr.h"

EkaFhGroup* EkaFhNom::addGroup() {
  return new EkaFhNomGr();
}

 /* ##################################################################### */
EkaOpResult EkaFhNom::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  uint64_t mcExpectedSeq = 1;
  bool mcGap = false;
  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    if (! runGr->udpCh->has_data()) {
      if (++timeCheckCnt % TimeCheckRate == 0) {
	tradingHours = isTradingHours(8,30,16,00);
      }
      if (tradingHours)   runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id    = 0xFF;

    auto pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (!pkt) continue;

    auto gr {dynamic_cast<EkaFhNomGr*>(b_gr[gr_id])};
    if (!gr) on_error("b_gr[%u] = NULL",gr_id);

    gr->resetNoMdTimer();

    if (sequence == 0 || msgInPkt == 0) {
      runGr->udpCh->next(); 
      continue; // unsequenced packet or heartbeat
    }

    
    if (mcExpectedSeq == 1) mcExpectedSeq = sequence;
    if (sequence < mcExpectedSeq) {
      EKA_WARN("%s:%u BACK-IN-TIME WARNING: sequence %ju < mcExpectedSeq %ju",
	       EKA_EXCH_DECODE(exch),gr_id,sequence,mcExpectedSeq);
      gr->sendBackInTimeEvent(pEfhRunCtx,sequence);
      gr->expected_sequence = sequence;
      break; 
    }
    if (sequence != mcExpectedSeq) {
      mcGap = true;
      EKA_LOG("%s:%u: MC Gap at %s: mcExpectedSeq %ju != sequence %ju",
	      EKA_EXCH_DECODE(exch),gr_id,
	      gr->printGrpState(),
	      mcExpectedSeq, sequence);
    }
    mcExpectedSeq = sequence + msgInPkt;
      
#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (gr->state == EkaFhGroup::GrpState::NORMAL && 
	sequence != 0 && 
	sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: (GAP_INJECT_INTERVAL = %d): pkt sequence %ju with %u messages dropped",
	       EKA_EXCH_DECODE(exch),gr_id, _EFH_TEST_GAP_INJECT_INTERVAL_,sequence,msgInPkt);
      runGr->udpCh->next(); 
      continue;
    }
#endif

       
#ifdef _EFH_UNRECOVERABLE_TEST_GAP_INJECT_INTERVAL_
    if (gr->state == EkaFhGroup::GrpState::NORMAL && 
	sequence != 0 && 
	sequence % _EFH_UNRECOVERABLE_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("\n=====================\n"
	       "%s:%u: UNRECOVERABLE TEST GAP INJECTED: (_EFH_UNRECOVERABLE_TEST_GAP_INJECT_INTERVAL_ = %d): pkt sequence %ju with %u messages dropped"
	       "\n=====================",
	       EKA_EXCH_DECODE(exch),gr_id, _EFH_UNRECOVERABLE_TEST_GAP_INJECT_INTERVAL_,sequence,msgInPkt);
      gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
      gr->gapClosed = false;
      mcGap = true;
      runGr->udpCh->next(); 
      continue;
    }
#endif
    
#ifdef FH_LAB
    gr->state = EkaFhGroup::GrpState::NORMAL;
    gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence);
#else
    //-----------------------------------------------------------------------------
    switch (gr->state) {
    case EkaFhGroup::GrpState::INIT : {
      EKA_LOG("%s:%u 1st MC msq sequence=%ju",
	      EKA_EXCH_DECODE(exch),gr_id,sequence);
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;
      gr->sendFeedDownInitial(pEfhRunCtx);
      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, 1, 0);
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::NORMAL :
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju, gap=%ju",
		EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence,
		sequence - gr->expected_sequence);
	gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;
	gr->sendFeedDown(pEfhRunCtx);
	gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
	gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx, gr->expected_sequence, sequence);
	mcGap = false; // to prevent immediate gap-in-gap
      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence);      
      }    
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::RETRANSMIT_GAP :
    case EkaFhGroup::GrpState::SNAPSHOT_GAP :
      if (mcGap) {
	mcGap = false;
	EKA_LOG("%s:%u: Gap in %s: invalidating Group and getting full Glimpse Snapshot",
		EKA_EXCH_DECODE(exch),gr->id,gr->printGrpState());
	gr->recovery_active = false;
	gr->snapshot_active = false;
	while (! gr->recoveryThreadDone)
	  sleep(0);
	gr->invalidateBook();
	gr->invalidateQ();
	gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
	gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;
	gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, 1, 0);
	break;
      }

      
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
      if (gr->gapClosed) {
	EKA_LOG("%s:%u: %s Closed, switching to fetch from Q: next sequence from Q = %ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->printGrpState(),gr->seq_after_snapshot);
	gr->expected_sequence = gr->seq_after_snapshot;
	gr->processFromQ(pEfhRunCtx);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx); //gr->sendFeedUpInitial(pEfhRunCtx);
	EKA_LOG("%s:%u: %s Closed - expected_sequence=%ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->printGrpState(),gr->expected_sequence);
      }
      break;
      //-----------------------------------------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
	       EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
#endif
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);

  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}
