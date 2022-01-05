#include "EkaFhCme.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhCmeGr.h"
#include "EkaFhCmeParser.h"

using namespace Cme;

EkaOpResult getCmeDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhCmeGr* gr,EkaFhMode op);
int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);

/* ##################################################################### */
EkaFhGroup* EkaFhCme::addGroup() {
  return new EkaFhCmeGr();
}

/* ##################################################################### */
const uint8_t* EkaFhCme::getUdpPkt(EkaFhRunGroup* runGr, 
			     int16_t*       pktLen, 
			     uint64_t*      sequence,
			     uint8_t*       gr_id) {

  auto pkt = runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));
  *pktLen   = runGr->udpCh->getPayloadLen();

  *sequence = getPktSeq(pkt);
  *gr_id    = getGrId(pkt);

  return pkt;
}
 /* ##################################################################### */

EkaOpResult EkaFhCme::initGroups(EfhCtx*          pEfhCtx, 
				 const EfhRunCtx* pEfhRunCtx, 
				 EkaFhRunGroup*   runGr) {
  for (uint8_t i = 0; i < runGr->numGr; i++) {
    if (! runGr->isMyGr(pEfhRunCtx->groups[i].localId)) 
      on_error("pEfhRunCtx->groups[%d].localId = %u doesnt belong to %s",
	       i,pEfhRunCtx->groups[i].localId,runGr->list2print);
    if (pEfhRunCtx->groups[i].source != exch) 
      on_error("pEfhRunCtx->groups[i].source != exch");

    auto gr = dynamic_cast<EkaFhCmeGr*>(b_gr[pEfhRunCtx->groups[i].localId]);
    if (! gr) on_error ("b_gr[%u] == NULL",pEfhRunCtx->groups[i].localId);

    gr->createPktQ();
    gr->expected_sequence = 0;

    runGr->igmpMcJoin(gr->mcast_ip,gr->mcast_port,0,&gr->pktCnt);
    //    runGr->igmpMcJoin(gr->recovery_ip,gr->recovery_port,0);
    EKA_DEBUG("%s:%u: joined Incr Feed %s:%u, Recovery Feed %s:%u, for %u securities",
	      EKA_EXCH_DECODE(exch),gr->id,
	      EKA_IP2STR(gr->mcast_ip),   gr->mcast_port,
	      EKA_IP2STR(gr->recovery_ip),gr->recovery_port,
	      gr->getNumSecurities());
    //    gr->sendFeedDownInitial(pEfhRunCtx);
  }
  return EKA_OPRESULT__OK;
}
/* ##################################################################### */

EkaOpResult EkaFhCme::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);
  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  //-----------------------------------------------------------------------------

  while (runGr->thread_active) {
    //-----------------------------------------------------------------------------
    //  if (runGr->drainQ(pEfhRunCtx)) continue;
    //-----------------------------------------------------------------------------
    if (! runGr->udpCh->has_data()) {
      if (++timeCheckCnt % TimeCheckRate == 0) {
	tradingHours = isTradingHours(9,30,16,00);
      }
      if (tradingHours)   runGr->checkTimeOut(pEfhRunCtx);
      continue;
    }
    //-----------------------------------------------------------------------------
    uint64_t sequence = 0;
    uint8_t  gr_id = 0xFF;
    int16_t  pktSize = 0; 

    auto pkt = getUdpPkt(runGr,&pktSize,&sequence,&gr_id);
    if (pkt == NULL) continue;

#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (sequence != 0 && sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: (GAP_INJECT_INTERVAL = %d): pkt sequence %ju",
	       EKA_EXCH_DECODE(exch),gr_id, _EFH_TEST_GAP_INJECT_INTERVAL_,sequence);
      runGr->udpCh->next(); 
      continue;
    }
#endif

    auto gr = dynamic_cast<EkaFhCmeGr*>(b_gr[gr_id]);
    if (gr == NULL) on_error("b_gr[%u] == NULL",gr_id);
    gr->resetNoMdTimer();

    //-----------------------------------------------------------------------------
    if (gr->inGap) {
      gr->pushPkt2Q(pkt,pktSize,sequence);
      if (gr->snapshotClosed) {
	gr->processFromQ(pEfhRunCtx);
	gr->inGap = false;
	gr->sendFeedUp(pEfhRunCtx);
	//	runGr->setGrAfterGap(gr->id);
	gr->snapshotClosed = false;
      }
    } else {
      if (sequence < gr->expected_sequence) {
	// skip stale
	EKA_WARN("%s:%u BACK-IN-TIME WARNING: sequence %ju < expected_sequence %ju",
		 EKA_EXCH_DECODE(exch),gr_id,sequence,gr->expected_sequence);
	gr->sendBackInTimeEvent(pEfhRunCtx,sequence);
	//	gr->expected_sequence = sequence;
      } else if (sequence != gr->expected_sequence) {
  	EKA_LOG("%s:%u %s GAP: sequence %ju != expected_sequence %ju, lost %jd",
		EKA_EXCH_DECODE(exch),gr_id,
		gr->expected_sequence ? "REAL" : "INITIAL",
		sequence,gr->expected_sequence, sequence - gr->expected_sequence);

	if (gr->expected_sequence == 0) {
	  gr->sendFeedDownInitial(pEfhRunCtx);
	} else {
	  gr->sendFeedDown(pEfhRunCtx);
	}

	gr->pushPkt2Q(pkt,pktSize,sequence);
	gr->expected_sequence = sequence;
	gr->inGap = true;
	gr->snapshotClosed = false;
	gr->closeSnapshotGap(pEfhCtx, pEfhRunCtx, sequence); 
      } else {
	//-----------------------------------------------------------------------------
	if (gr->processPkt(pEfhRunCtx,pkt,pktSize,EkaFhMode::MCAST)) break;
	gr->expected_sequence = sequence + 1;
	//-----------------------------------------------------------------------------
      }
    }
    runGr->udpCh->next(); 
  } // while
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaOpResult EkaFhCme::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, const EkaGroup* group) {
  auto gr = dynamic_cast<EkaFhCmeGr*>(b_gr[group->localId]);
  if (gr == NULL) on_error("gr[%u] == NULL",(uint8_t)group->localId);

  return gr->recoveryLoop(pEfhRunCtx, EkaFhMode::DEFINITIONS);
}
