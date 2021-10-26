#include "EkaFhPlr.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhPlrGr.h"
#include "EkaFhThreadAttr.h"
#include "EkaFhPlrParser.h"

using namespace Plr;

bool plrRecovery(const EfhRunCtx* pEfhRunCtx, EkaFhPlrGr* gr, EkaFhMode op,
		 uint64_t start, uint64_t end);

/* ##################################################################### */
EkaFhGroup* EkaFhPlr::addGroup() {
  return new EkaFhPlrGr();
}

/* ##################################################################### */
const uint8_t* EkaFhPlr::getUdpPkt(EkaFhRunGroup* runGr, 
			     uint*          msgInPkt, 
			     uint32_t*      sequence,
			     uint8_t*       gr_id,
			     uint8_t*       pktType) {

  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));
  auto pktHdr {reinterpret_cast<const PktHdr*>(pkt)};

  *msgInPkt = pktHdr->numMsgs;
  *sequence = pktHdr->seqNum;
  *gr_id    = getGrId(pkt);
  *pktType  = pktHdr->deliveryFlag;

  return pkt;
}

/* ##################################################################### */

EkaOpResult EkaFhPlr::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  active = true;
  while (runGr->thread_active && ! runGr->stoppedByExchange) {
    //-----------------------------------------------------------------------------
    if (runGr->drainQ(pEfhRunCtx)) continue;

    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) {
      if (++timeCheckCnt % TimeCheckRate == 0) {
	tradingHours = isTradingHours(8,30,16,00);
	//	tradingHours = isTradingHours(9,30,16,00);
      }
      if (tradingHours)   runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }
    uint     msgInPkt = 0;
    uint32_t sequence = 0;
    uint8_t  gr_id = 0xFF;
    DeliveryFlag pktType;
    
    const uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&sequence,&gr_id,(uint8_t*)&pktType);
    if (pkt == NULL) continue;

    auto gr {reinterpret_cast<EkaFhPlrGr*>(b_gr[gr_id])};
    if (gr == NULL) on_error("b_gr[%u] = NULL",gr_id);

    
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

    gr->resetNoMdTimer();
    
#ifdef FH_LAB
    gr->state = EkaFhGroup::GrpState::NORMAL;
    gr->processUdpPkt(pEfhRunCtx,pkt,msgInPkt,sequence,startTime);
#else

    if (msgInPkt == 0) goto SKIP; // Heartbeat
    
    //-----------------------------------------------------------------------------
    switch (gr->state) {
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::INIT : {
      if (any_group_getting_snapshot) break; // only 1 group can get snapshot at a time
      any_group_getting_snapshot = true;

      gr->gapClosed = false;
      gr->state = EkaFhGroup::GrpState::SNAPSHOT_GAP;
      gr->sendFeedDownInitial(pEfhRunCtx);
      gr->closeSnapshotGap(pEfhCtx,pEfhRunCtx, (uint64_t)0, (uint64_t)0);
    }
      break;
      //-----------------------------------------------------------------------------
    case EkaFhGroup::GrpState::NORMAL : {     
      if (sequence == 0) break; // unsequenced packet
      if (sequence < gr->expected_sequence) {
	EKA_WARN("%s:%u BACK-IN-TIME WARNING: sequence %u < expected_sequence %ju",
		 EKA_EXCH_DECODE(exch),gr_id,sequence,gr->expected_sequence);
	gr->sendBackInTimeEvent(pEfhRunCtx,sequence);
	gr->expected_sequence = sequence;
	break; 
      }
      if (sequence > gr->expected_sequence) { // GAP
	EKA_LOG("%s:%u Gap at NORMAL:  gr->expected_sequence=%ju, sequence=%u, msgInPkt=%u",
		EKA_EXCH_DECODE(exch),gr_id,gr->expected_sequence,sequence,msgInPkt);
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
	EKA_LOG("%s:%u: SNAPSHOT_GAP Closed",EKA_EXCH_DECODE(exch),gr->id);
	gr->state = EkaFhGroup::GrpState::NORMAL;
	any_group_getting_snapshot = false;
	gr->sendFeedUpInitial(pEfhRunCtx);

	runGr->setGrAfterGap(gr->id);
	gr->expected_sequence = gr->seq_after_snapshot;
#ifdef PLR_CERT
	uint64_t gapStart = gr->seq_after_snapshot - 10;
	uint64_t gapEnd   = gapStart + 5;
	EKA_LOG("\n#############\nPLR_CER artificial GAP closure: %ju..%ju\n#############",gapStart,gapEnd);
	gr->closeIncrementalGap(pEfhCtx, pEfhRunCtx, gapStart,gapEnd);
#endif 
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
      on_error("%s:%u: UNEXPECTED GrpState %u",EKA_EXCH_DECODE(exch),gr->id,(uint)gr->state);
      break;
    } // switch
#endif
  SKIP:    
    runGr->udpCh->next(); 
  } // while()
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;

}
/* ##################################################################### */

EkaOpResult EkaFhPlr::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, const EkaGroup* group) {
  auto gr {dynamic_cast<EkaFhPlrGr*>(b_gr[(uint8_t)group->localId])};
  if (! gr)
    on_error("%s:%u Group does not exist (gr == NULL)",EKA_EXCH_DECODE(exch),gr->id);
  else if (gr->productMask == PM_VanillaTrades) {
    // The trades channel does not give definitions and we'll wait forever
    // if we try to receive them.
    EKA_DEBUG("%s:%u: skipping definitions for trade group",EKA_EXCH_DECODE(exch),gr->id);
    return EKA_OPRESULT__OK;
  }
  plrRecovery(pEfhRunCtx, gr, EkaFhMode::DEFINITIONS, 0,0);
  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaOpResult EkaFhPlr::subscribeStaticSecurity(uint8_t groupNum, 
					   uint64_t securityId, 
					   EfhSecurityType efhSecurityType,
					   EfhSecUserData efhSecUserData,
					   uint64_t opaqueAttrA,
					   uint64_t opaqueAttrB) {
  if (groupNum >= groups) on_error("groupNum (%u) >= groups (%u)",groupNum,groups);

  auto gr {dynamic_cast<EkaFhPlrGr*>(b_gr[groupNum])};
  if (! gr)
    on_error("b_gr[%u] == NULL",groupNum);

  /* if (! (gr->productMask & PM_VanillaBook)) { */
  /*   EKA_WARN("%s:%u: Trying subscribe on Non MD group (productMask=0x%x)", */
  /* 	     EKA_EXCH_DECODE(exch),gr->id,gr->productMask); */

  /* } */
  gr->subscribeStaticSecurity(securityId, 
					  efhSecurityType,
					  efhSecUserData,
					  opaqueAttrA,
					  opaqueAttrB);

  gr->numSecurities++;

  auto tradesGroup = findTradesGroup();
  if (! tradesGroup) {
    EKA_WARN("%s:%u: WARNING no trades group found",
	     EKA_EXCH_DECODE(exch),gr->id);
    return EKA_OPRESULT__OK;
  } else {
    tradesGroup->subscribeStaticSecurity(securityId, 
					 efhSecurityType,
					 efhSecUserData,
					 opaqueAttrA,
					 opaqueAttrB);
    tradesGroup->numSecurities++;
  }
  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaFhPlrGr* EkaFhPlr::findTradesGroup() {
  for (uint i = 0; i < groups; i++) {
    auto gr {dynamic_cast<EkaFhPlrGr*>(b_gr[i])};
    if (! gr)
      on_error("b_gr[%u] == NULL",i);
    if (gr->productMask & ProductMask::PM_VanillaTrades)
      return gr;
  }
  return NULL;
}
