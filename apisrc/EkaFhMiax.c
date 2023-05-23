#include "EkaFhMiax.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhMiaxGr.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhMiaxParser.h"

void* getSesmData(void* attr);

/* ############################################## */
EkaFhGroup* EkaFhMiax::addGroup() {
  return new EkaFhMiaxGr();
}
/* ############################################## */

const uint8_t* EkaFhMiax::getUdpPkt(EkaFhRunGroup* runGr, 
																		uint*          msgInPkt, 
																		int16_t*       pktLen, 
																		uint64_t*      sequence,
																		uint8_t*       gr_id,
																		bool*          isHeartbeat) {

  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL)
		on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *pktLen      = runGr->udpCh->getPayloadLen();
  *gr_id       = getGrId(pkt);
  *sequence    = EKA_GET_MACH_SEQ((pkt));
  *isHeartbeat = EKA_IS_MACH_HEARTBEAT((pkt));
  *msgInPkt    = 16; // just a number, greater than amount of messages in a packet
  return pkt;
}
/* ############################################## */


EkaOpResult EkaFhMiax::runGroups( EfhCtx* pEfhCtx,
																	const EfhRunCtx* pEfhRunCtx,
																	uint8_t runGrId ) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
  if (runGr == NULL)
		on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
						EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ "
						"Main Thread for %s Run Group %u: %s GROUPS "
						"~~~~~~~~~~~~~",
						EKA_EXCH_DECODE(exch),runGr->runId,
						runGr->list2print);

  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    //-----------------------------------------
    if (! runGr->udpCh->has_data()) {
			if (runGr->checkNoMd)
				runGr->checkGroupsNoMd(pEfhRunCtx);				
      continue;
    }
    uint     msgInPkt = 0;
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;
    int16_t  pktLen = 0;
    bool     isHeartbeat = false;

    const uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,
																	 &pktLen,&sequence,&gr_id,
																	 &isHeartbeat);
    if (pkt == NULL) continue;
    if (gr_id == 0xFF) {
      runGr->udpCh->next(); 
      continue;
    }

    EkaFhMiaxGr* gr = (EkaFhMiaxGr*)b_gr[gr_id];
    if (gr == NULL) on_error("gr = NULL");
    gr->resetNoMdTimer();

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (gr->state == EkaFhGroup::GrpState::NORMAL && 
				sequence != 0 && 
				sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: "
							 "(GAP_INJECT_INTERVAL = %d): pkt sequence %ju "
							 "with unknown number of messages dropped",
							 EKA_EXCH_DECODE(exch),gr_id,
							 _EFH_TEST_GAP_INJECT_INTERVAL_,sequence);
      runGr->udpCh->next(); 
      continue;
    }
#endif

    /* if (isHeartbeat) { */
    /*   runGr->udpCh->next();  */
    /*   continue; */
    /* } */

    //-----------------------------------------
    switch (gr->state) {
      //-----------------------------------------
    case EkaFhGroup::GrpState::INIT : {
      if (any_group_getting_snapshot)
				break; // only 1 group can get snapshot at a time
      any_group_getting_snapshot = true;

      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;

      gr->sendFeedDownInitial(pEfhRunCtx);
      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, 0, 0);
    }
      break;
      //-----------------------------------------
    case EkaFhGroup::GrpState::NORMAL : {
      if (sequence == 0) break; // unsequenced packet
      if (sequence < gr->expected_sequence) {
				if (isHeartbeat) {
					gr->expected_sequence = sequence + 1;
				} else {
					EKA_WARN("%s:%u BACK-IN-TIME WARNING: "
									 "sequence %ju < expected_sequence %ju",
									 EKA_EXCH_DECODE(exch),gr_id,
									 sequence,gr->expected_sequence);
					gr->sendBackInTimeEvent(pEfhRunCtx,sequence);
					gr->expected_sequence = sequence;
				}
				break; 
      }
      if (sequence > gr->expected_sequence) { // GAP
				EKA_LOG("%s:%u Gap at NORMAL:  "
								"gr->expected_sequence=%ju, sequence=%ju",
								EKA_EXCH_DECODE(exch),gr_id,
								gr->expected_sequence,sequence);
				gr->state = EkaFhGroup::GrpState::RETRANSMIT_GAP;
				gr->gapClosed = false;

				gr->sendFeedDown(pEfhRunCtx);
				gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx,
																gr->expected_sequence,
																sequence + msgInPkt);

      } else { // NORMAL
				runGr->stoppedByExchange = gr->processUdpPkt(pEfhRunCtx,
																										 pkt,pktLen);     
      }
    }
      break;
      //-----------------------------------------
    case EkaFhGroup::GrpState::SNAPSHOT_GAP : {
      gr->pushUdpPkt2Q(pkt,pktLen);

      if (gr->gapClosed) {
				EKA_LOG("%s:%u: SNAPSHOT_GAP Closed: "
								"gr->seq_after_snapshot = %ju",
								EKA_EXCH_DECODE(exch),gr->id,
								gr->seq_after_snapshot);
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
				EKA_LOG("%s:%u: RETRANSMIT_GAP Closed, "
								"gr->seq_after_snapshot = %ju, "
								"switching to fetch from Q",
								EKA_EXCH_DECODE(exch),gr->id,
								gr->seq_after_snapshot);
				gr->state = EkaFhGroup::GrpState::NORMAL;
				gr->sendFeedUp(pEfhRunCtx);

				runGr->setGrAfterGap(gr->id);
				gr->expected_sequence = gr->seq_after_snapshot + 1;
      }
    }
      break;
      //-----------------------------------------
    default:
      on_error("%s:%u: UNEXPECTED GrpState %u",
							 EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    }
    runGr->udpCh->next(); 
  }
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;

}

/* ############################################## */

EkaOpResult EkaFhMiax::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, const EkaGroup* group) {
  EkaFhThreadAttr* attr = new EkaFhThreadAttr(pEfhCtx, 
																							pEfhRunCtx, 
																							b_gr[(uint8_t)group->localId], 
																							1, 0, 
																							EkaFhMode::DEFINITIONS);
  getSesmData(attr);
  return EKA_OPRESULT__OK;
}
