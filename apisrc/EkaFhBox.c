#include "EkaFhBox.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhBoxGr.h"

EkaOpResult getHsvfDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhBoxGr* gr);
uint64_t getHsvfMsgSequence(const uint8_t* msg);

/* ##################################################################### */
EkaFhGroup* EkaFhBox::addGroup() {
  //  return dynamic_cast<EkaFhGroup*>(new EkaFhBoxGr());
  return new EkaFhBoxGr();
}

/* ##################################################################### */

const uint8_t* EkaFhBox::getUdpPkt(EkaFhRunGroup* runGr, 
			     uint16_t*      pktLen, 
			     uint64_t*      sequence, 
			     uint8_t*       gr_id) {
  const uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *pktLen   = runGr->udpCh->getPayloadLen();
  *gr_id    = getGrId(pkt);
  *sequence = getHsvfMsgSequence(pkt);

  return pkt;
}

/* ##################################################################### */
EkaOpResult EkaFhBox::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
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
      runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }
    uint8_t        gr_id = 0xFF;
    uint16_t       pktLen = 0;
    uint64_t       sequence = 0;
    static const uint  bufSize = 1200;
    uint8_t        buf[bufSize] = {};
    const uint8_t* rawPkt = getUdpPkt(runGr,&pktLen,&sequence,&gr_id);
    if (rawPkt == NULL) continue;

    if (pktLen > bufSize) on_error("%u pktLen > %ju bufSize",
				   pktLen,(uint64_t)bufSize);

    memcpy(buf,rawPkt,pktLen);
    const uint8_t* pkt = buf;

    EkaFhBoxGr* gr = (EkaFhBoxGr*)b_gr[gr_id];
    if (gr == NULL) on_error("gr == NULL");

    gr->resetNoMdTimer();

#ifdef FH_LAB
    gr->state = EkaFhGroup::GrpState::NORMAL;
#endif

    //-----------------------------------------------------------------------------
    switch (gr->state) {
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::INIT : {
      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;
      gr->sendFeedDownInitial(pEfhRunCtx);
      gr->pushUdpPkt2Q(pkt,pktLen);

      gr->closeIncrementalGap(pEfhCtx,pEfhRunCtx, (uint64_t)1, sequence);
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::NORMAL : {
      //      if (sequence < gr->expected_sequence) break; // skipping stale messages
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%ju",
		EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence);

	EKA_LOG("%s:%u prev pktLen = %u, prev pkt msgCnt=%u, prev pkt Seq=%ju",
		EKA_EXCH_DECODE(exch),gr_id,gr->lastPktLen,gr->lastPktMsgCnt,gr->lastPkt1stSeq);

	//	hexDump("Gap Pkt",pkt,pktLen);
#ifdef FH_LAB
	gr->sendFeedDown(pEfhRunCtx);
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,pktLen);  
	break;
#endif


	gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
	gr->gapClosed = false;

	gr->pushUdpPkt2Q(pkt,pktLen);

	gr->sendFeedDown(pEfhRunCtx);
	gr->closeIncrementalGap(pEfhCtx,pEfhRunCtx,gr->expected_sequence, sequence);
      } else { // NORMAL
	runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,pktLen);      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      if (gr->gapClosed) { // ignore UDP pkt during initial Snapshot
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed: seq_after_snapshot = %ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	gr->pushUdpPkt2Q(pkt,pktLen);

	EKA_DEBUG("%s:%u Generating TOB quote for every Security",
		  EKA_EXCH_DECODE(gr->exch),gr->id);
	gr->book->sendTobImage(pEfhRunCtx);

	gr->sendFeedUpInitial(pEfhRunCtx);

	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;      
      }
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::RETRANSMIT_GAP : {
      gr->pushUdpPkt2Q(pkt,pktLen);

      if (gr->gapClosed) {
	EKA_LOG("%s:%u: RETRANSMIT_GAP Closed: seq_after_snapshot = %ju",
		EKA_EXCH_DECODE(exch),gr->id,gr->seq_after_snapshot);
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
      //-----------------------------------------------------------------------------
    }
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);

  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}

/* ##################################################################### */

EkaOpResult EkaFhBox::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {

  return getHsvfDefinitions(pEfhCtx, 
			    pEfhRunCtx,
			    (EkaFhBoxGr*)b_gr[(uint8_t)group->localId]);
}
