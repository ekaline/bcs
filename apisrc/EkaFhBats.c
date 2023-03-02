#include "EkaFhBats.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhBatsGr.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhBatsParser.h"

using namespace Bats;

void* getSpinData(void* attr);

/* ##################################################################### */
EkaFhGroup* EkaFhBats::addGroup() {
  return new EkaFhBatsGr();
}

/* ##################################################################### */
const uint8_t* EkaFhBats::getUdpPkt(EkaFhRunGroup* runGr, uint* msgInPkt, uint64_t* sequence,uint8_t* gr_id) {
  auto pkt = runGr->udpCh->get();
  if (! pkt) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  uint msgCnt = EKA_BATS_MSG_CNT((pkt));
  uint8_t grId = getGrId(pkt);

  if (grId == 0xFF || (! runGr->isMyGr(grId)) || b_gr[grId] == NULL || b_gr[grId]->q == NULL) { 
    //    if (grId != 0) EKA_LOG("RunGr%u: Skipping gr_id = %u not belonging to %s",runGr->runId,grId,runGr->list2print);
    runGr->udpCh->next(); 
    return NULL;
  }

  *msgInPkt = msgCnt;
  *sequence = EKA_BATS_SEQUENCE((pkt));
  *gr_id    = grId;
  return pkt;
}
/* ##################################################################### */

uint8_t EkaFhBats::getGrId(const uint8_t* pkt) {
  for (uint8_t i = 0; i < groups; i++) {
    auto gr = dynamic_cast<EkaFhBatsGr*>(b_gr[i]);
    if (! gr) on_error("b_gr[%u] = NULL",i);

    if (gr->mcast_port == EKA_UDPHDR_DST((pkt-8)) &&
	EKA_BATS_UNIT(pkt) == gr->batsUnit) 
      return i;
  }
  return 0xFF;
}

/* ##################################################################### */

EkaOpResult EkaFhBats::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  auto runGr = dev->runGr[runGrId];
  if (! runGr) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  uint32_t monitorCounter = 0;

  active = true;
  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    if (++monitorCounter % 1000 == 0) {
      for (uint8_t i = 0; i < runGr->numGr; i++) {
        b_gr[runGr->groupList[i]]->printBookState();
      }
    }

    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) {
      if (++timeCheckCnt % TimeCheckRate == 0) {
	tradingHours = isTradingHours(8,30,16,00);
      }
      if (tradingHours)   runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;

    auto pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id);
    if (! pkt) continue;

    auto gr = dynamic_cast<EkaFhBatsGr*>(b_gr[gr_id]);
    if (! gr) on_error("b_gr[%u] = NULL",gr_id);

    gr->resetNoMdTimer();
        
    std::chrono::high_resolution_clock::time_point startTime{};

#if EFH_TIME_CHECK_PERIOD
    if (sequence % EFH_TIME_CHECK_PERIOD == 0) {
      startTime = std::chrono::high_resolution_clock::now();
    }
#endif
    
#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (gr->state == EkaFhGroup::GrpState::NORMAL && 
	sequence != 0 && 
	sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: (GAP_INJECT_INTERVAL = %d): pkt sequence %ju with unknown number of messages dropped",
	       EKA_EXCH_DECODE(exch),gr_id, _EFH_TEST_GAP_INJECT_INTERVAL_,sequence);
      runGr->udpCh->next(); 
      continue;
    }
#endif

#ifdef FH_LAB
    gr->state = EkaFhGroup::GrpState::NORMAL;
    gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence,startTime);
#else
    //-----------------------------------------------------------------------------
    switch (gr->state) {
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::INIT : {
      if (any_group_getting_snapshot) break; // only 1 group can get snapshot at a time
      any_group_getting_snapshot = true;

      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;
      gr->sendFeedDownInitial(pEfhRunCtx);
      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, 0, 0);
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::NORMAL : {
      if (sequence == 0) break; // unsequenced packet
      if (sequence < gr->expected_sequence) {
	EKA_WARN("%s:%u BACK-IN-TIME WARNING: sequence %ju < expected_sequence %ju",
		 EKA_EXCH_DECODE(exch),gr_id,sequence,gr->expected_sequence);
	gr->sendBackInTimeEvent(pEfhRunCtx,sequence);
	if (sequence != 1) // right after Spin
	  gr->expected_sequence = sequence;
	break; 
      }
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",
		EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	gr->sendFeedDown(pEfhRunCtx);
	gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);
	gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx, gr->expected_sequence, sequence);
      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence,startTime);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      gr->pushUdpPkt2Q(pkt,msgInPkt,sequence);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed",
		EKA_EXCH_DECODE(exch),gr->id);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	any_group_getting_snapshot = false;
	gr->sendFeedUpInitial(pEfhRunCtx);

	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
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
    }
#endif
    runGr->udpCh->next(); 
  }
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;

}
/* ##################################################################### */

EkaOpResult EkaFhBats::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, const EkaGroup* group) {
  auto attr = new EkaFhThreadAttr(pEfhCtx, 
				  pEfhRunCtx, 
				  b_gr[group->localId], 
				  1, 0, 
				  EkaFhMode::DEFINITIONS);
  if (! attr) on_error("attr = NULL");
  getSpinData(attr);

  return EKA_OPRESULT__OK;
}
