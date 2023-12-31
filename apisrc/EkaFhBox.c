#include "EkaFhBox.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhBoxGr.h"
#include "EkaFhBoxParser.h"

EkaOpResult getHsvfDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhBoxGr* gr);

/* #################################################### */
EkaFhGroup* EkaFhBox::addGroup() {
  //  return dynamic_cast<EkaFhGroup*>(new EkaFhBoxGr());
  return new EkaFhBoxGr();
}

/* #################################################### */

const uint8_t* EkaFhBox::getUdpPkt(EkaFhRunGroup* runGr, 
																	 int16_t*      pktLen, 
																	 uint64_t*      sequence, 
																	 uint8_t*       gr_id) {
  auto pkt = runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *pktLen   = runGr->udpCh->getPayloadLen();
  *gr_id    = getGrId(pkt);
  *sequence = Hsvf::getMsgSequence(pkt);

  if (*gr_id == 0xFF) {
    runGr->udpCh->next(); 
    return NULL;
  }
  return pkt;
}

/* #################################################### */
EkaOpResult EkaFhBox::runGroups( EfhCtx* pEfhCtx,
																 const EfhRunCtx* pEfhRunCtx,
																 uint8_t runGrId ) {
  auto runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
  int64_t pktCnt = 0;
#endif
  
  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
						EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);
  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ "
						"Main Thread for %s Run Group %u: %s GROUPS "
						"~~~~~~~~~~~~~",
						EKA_EXCH_DECODE(exch),runGr->runId,
						runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //----------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    //----------------------------------------------
    if (! runGr->udpCh->has_data()) {
			if (runGr->checkNoMd)
				runGr->checkGroupsNoMd(pEfhRunCtx);
      continue;
    }
    uint8_t  gr_id = 0xFF;
    int16_t  pktLen = 0;
    uint64_t sequence = 0;

    auto pkt = getUdpPkt(runGr,&pktLen,&sequence,&gr_id);
    if (!pkt) continue;

    auto gr {dynamic_cast<EkaFhBoxGr*>(b_gr[gr_id])};
    if (!gr)
			on_error("gr == NULL");

		gr->resetNoMdTimer();

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    pktCnt++;
    if ((gr->state == EkaFhGroup::GrpState::NORMAL ||
				 gr->state == EkaFhGroup::GrpState::RETRANSMIT_GAP) && 
				(sequence != 0) && 
				((pktCnt       % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) ||
				 ((pktCnt - 2) % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0))) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: "
							 "(GAP_INJECT_INTERVAL = %d): "
							 "pktCnt=%jd, sequence %ju",
							 EKA_EXCH_DECODE(exch),gr_id,
							 _EFH_TEST_GAP_INJECT_INTERVAL_,
							 pktCnt,sequence);
      runGr->udpCh->next(); 
      continue;
    }
#endif

#ifdef FH_LAB
    /* gr->state = EkaFhGroup::GrpState::NORMAL; */
#endif

    //----------------------------------------------
    switch (gr->state) {
      //----------------------------------------------
    case EkaFhGroup::GrpState::INIT : {
      if (isTradingHours(9,30,16,00)) {
				gr->gapClosed = true;
				gr->state = EkaFhGroup::GrpState::NORMAL;
				EKA_LOG("%s:%u INIT Gap at Trading hours: "
								"\'Progressing\' FeedUp",
								EKA_EXCH_DECODE(exch),gr_id);
				gr->expected_sequence = sequence + 1;
				gr->sendFeedDownInitial(pEfhRunCtx);
				gr->sendProgressingFeedUp(pEfhRunCtx);
      } else {
				gr->gapClosed = false;
				gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;
				gr->sendFeedDownInitial(pEfhRunCtx);
				gr->pushUdpPkt2Q(pkt,pktLen);
				EKA_LOG("%s:%u INIT Gap: pushing sequence %ju to Q",
								EKA_EXCH_DECODE(exch),gr_id,sequence);

				gr->closeIncrementalGap(pEfhCtx,pEfhRunCtx,
																(uint64_t)1, sequence - 1);
      }
    }
      break;
      //----------------------------------------------
    case EkaFhGroup::GrpState::NORMAL : {
      if (sequence < gr->expected_sequence) {
				if (gr->expected_sequence == gr->seq_after_snapshot)
					break; // end of recovery cycle
				if (! Hsvf::isHeartbeat(pkt)) {
					EKA_WARN("%s:%u BACK-IN-TIME WARNING: "
									 "sequence %ju < expected %ju",
									 EKA_EXCH_DECODE(exch),gr_id,
									 sequence,gr->expected_sequence);
					gr->sendBackInTimeEvent(pEfhRunCtx,sequence);
					gr->expected_sequence = sequence;
					break; 
				}
      }
      if (sequence > gr->expected_sequence) { // GAP
				EKA_LOG("%s:%u Gap at NORMAL:  "
								"expected=%ju, sequence=%ju, lost %jd",
								EKA_EXCH_DECODE(exch),gr_id,
								gr->expected_sequence,sequence,
								sequence - gr->expected_sequence);

				if (gr->skipRecovery()) {
					EKA_LOG("%s:%u gapNum %u > gapsLimit %u: "
									"skipping TCP Recovery",
									EKA_EXCH_DECODE(exch),gr_id,
									gr->gapNum,gr->gapsLimit);
					gr->sendFeedDown(pEfhRunCtx);
					gr->sendProgressingFeedUp(pEfhRunCtx);
					gr->expected_sequence = sequence;
					runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,
																											 pkt,pktLen);      
				} else {

					//	hexDump("Gap Pkt",pkt,pktLen);
#ifdef FH_LAB
					/* gr->sendFeedDown(pEfhRunCtx); */
					/* runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,pkt,pktLen);   */
					/* break; */
#endif
					gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
					gr->gapClosed = false;

					gr->pushUdpPkt2Q(pkt,pktLen);

					gr->sendFeedDown(pEfhRunCtx);
					gr->closeIncrementalGap(pEfhCtx,pEfhRunCtx,
																	gr->expected_sequence,
																	sequence - 1);
				}
      } else { // NORMAL
				runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,
																										 pkt,pktLen);     
      }
    }
      break;
      //----------------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      if (gr->gapClosed) { // ignore UDP pkt during initial Snapshot
				EKA_LOG("%s:%u: SNAPSHOT_GAP Closed: "
								"seq_after_snapshot = %ju",
								EKA_EXCH_DECODE(exch),gr->id,
								gr->seq_after_snapshot);
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
      //----------------------------------------------
    case EkaFhGroup::GrpState::RETRANSMIT_GAP : {
      gr->pushUdpPkt2Q(pkt,pktLen);

      if (gr->gapClosed) {
				EKA_LOG("%s:%u: RETRANSMIT_GAP Closed: "
								"seq_after_snapshot = %ju",
								EKA_EXCH_DECODE(exch),gr->id,
								gr->seq_after_snapshot);
				gr->state = EkaFhGroup::GrpState::NORMAL;
				gr->sendFeedUp(pEfhRunCtx);

				runGr->setGrAfterGap(gr->id);
				gr->expected_sequence = gr->seq_after_snapshot;     
      } 
    }
      break;
      //----------------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
							 EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
      //----------------------------------------------
    }
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",
					 EKA_EXCH_DECODE(exch),runGrId);

  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}

/* #################################################### */

EkaOpResult EkaFhBox::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, const EkaGroup* group) {

  return getHsvfDefinitions(pEfhCtx, 
														pEfhRunCtx,
														(EkaFhBoxGr*)b_gr[(uint8_t)group->localId]);
}
