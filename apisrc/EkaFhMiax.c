#include "EkaFhMiax.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhMiaxGr.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhMiaxParser.h"

void* getSesmData(void* attr);

/* ##################################################################### */
EkaFhGroup* EkaFhMiax::addGroup() {
  return new EkaFhMiaxGr();
}
/* ##################################################################### */

const uint8_t* EkaFhMiax::getUdpPkt(EkaFhRunGroup* runGr, 
			   uint*          msgInPkt, 
			   int16_t*       pktLen, 
			   uint64_t*      sequence,
			   uint8_t*       gr_id) {

  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *pktLen   = runGr->udpCh->getPayloadLen();
  *gr_id    = getGrId(pkt);
  *sequence = EKA_GET_MACH_SEQ((pkt));
  *msgInPkt = 16; // just a number, greater than amount of messages in a packet
  return pkt;
}
/* ##################################################################### */


EkaOpResult EkaFhMiax::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
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
      if (++timeCheckCnt % TimeCheckRate == 0) {
	tradingHours = isTradingHours(9,30,16,00);
      }
      if (tradingHours)   runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;
    int16_t  pktLen = 0;

    const uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&pktLen,&sequence,&gr_id);
    if (pkt == NULL) continue;
    if (gr_id == 0xFF) {
      runGr->udpCh->next(); 
      continue;
    }
    EkaFhMiaxGr* gr = (EkaFhMiaxGr*)b_gr[gr_id];
    if (gr == NULL) on_error("gr = NULL");
    gr->resetNoMdTimer();

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
      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);
	gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	gr->sendFeedDown(pEfhRunCtx);
	gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx, gr->expected_sequence, sequence + msgInPkt);

      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,pktLen);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      gr->pushUdpPkt2Q(pkt,pktLen);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed: gr->seq_after_snapshot = %ju",EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	any_group_getting_snapshot = false;
	gr->sendFeedUp(pEfhRunCtx);

	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot + 1;
      }
    }
      break;
    case EkaFhGroup::GrpState::RETRANSMIT_GAP : {
      gr->pushUdpPkt2Q(pkt,pktLen);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, gr->seq_after_snapshot = %ju, switching to fetch from Q",EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->sendFeedUp(pEfhRunCtx);

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
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;

}

 /* ##################################################################### */

EkaOpResult EkaFhMiax::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  EkaFhThreadAttr* attr = new EkaFhThreadAttr(pEfhCtx, 
					      pEfhRunCtx, 
					      b_gr[(uint8_t)group->localId], 
					      1, 0, 
					      EkaFhMode::DEFINITIONS);
  getSesmData(attr);
  return EKA_OPRESULT__OK;
}
