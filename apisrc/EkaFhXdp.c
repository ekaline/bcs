#include "EkaFhXdp.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhXdpGr.h"
#include "EkaFhXdpParser.h"

EkaOpResult getXdpDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhXdpGr* gr,EkaFhMode op);

using namespace Xdp;

/* ##################################################################### */
EkaFhGroup* EkaFhXdp::addGroup() {
  return new EkaFhXdpGr();
}

/* ##################################################################### */
const uint8_t* EkaFhXdp::getUdpPkt(EkaFhRunGroup* runGr, 
			     uint*          msgInPkt, 
			     uint*          pktSize, 
			     uint64_t*      sequence,
			     uint8_t*       gr_id,
			     uint16_t*      streamId, 
			     uint8_t*       pktType) {

  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *msgInPkt = EKA_XDP_MSG_CNT((pkt));
  *sequence = EKA_XDP_SEQUENCE((pkt));
  *gr_id    = getGrId(pkt);
  *streamId = EKA_XDP_STREAM_ID((pkt));
  *pktType  = EKA_XDP_PKT_TYPE((pkt));
  *pktSize  = EKA_XDP_PKT_SIZE((pkt));

  return pkt;
}

/* ##################################################################### */

EkaOpResult EkaFhXdp::runGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runGrId ) {
  EkaFhRunGroup* runGr = dev->runGr[runGrId];
  if (runGr == NULL) on_error("runGr == NULL");

  EKA_DEBUG("Initializing %s Run Group %u: %s GROUPS",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);
  initGroups(pEfhCtx, pEfhRunCtx, runGr);

  EKA_DEBUG("\n~~~~~~~~~~ Main Thread for %s Run Group %u: %s GROUPS ~~~~~~~~~~~~~",
	    EKA_EXCH_DECODE(exch),runGr->runId,runGr->list2print);

  for (uint8_t j = 0; j < runGr->numGr; j++) {
    uint8_t grId = runGr->groupList[j];
    EkaFhXdpGr* gr = (EkaFhXdpGr*)b_gr[grId];
    if (gr == NULL) on_error("b_gr[%u] == NULL",grId);
    gr->sendFeedDownInitial(pEfhRunCtx);

    gr->inGap = true;
    gr->setGapStart();
  }

  while (runGr->thread_active) {
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
    uint16_t streamId = 0;
    uint8_t  pktType = 0;
    uint     pktSize = 0; 

    const uint8_t* pkt = getUdpPkt(runGr,&msgInPkt,&pktSize,&sequence,&gr_id, &streamId, &pktType);
    if (pkt == NULL) continue;

    EkaFhXdpGr* gr = (EkaFhXdpGr*)b_gr[gr_id];
    if (gr == NULL) on_error("b_gr[%u] == NULL",gr_id);
#ifdef _EFH_TEST_GAP_INJECT_INTERVAL_
    if (!gr->inGap && 
	sequence != 0 && 
	sequence % _EFH_TEST_GAP_INJECT_INTERVAL_ == 0) {
      EKA_WARN("%s:%u: TEST GAP INJECTED: (GAP_INJECT_INTERVAL = %d): pkt sequence %ju with %u messages dropped",
	       EKA_EXCH_DECODE(exch),gr_id, _EFH_TEST_GAP_INJECT_INTERVAL_,sequence,msgInPkt);
      runGr->udpCh->next(); 
      continue;
    }
#endif
    gr->resetNoMdTimer();

    auto streamIdx = gr->findOrInstallStream(streamId, sequence);

    /* EKA_LOG("%s:%u Seq=%ju,expSeq=%ju, pktSize=%u msgInPkt =%u",EKA_EXCH_DECODE(exch),gr_id, */
    /* 	    sequence,gr->getExpectedSeq(streamIdx),pktSize,msgInPkt); */


    //-----------------------------------------------------------------------------
    if (pktType == (uint8_t)DELIVERY_FLAG::SequenceReset) gr->resetExpectedSeq(streamIdx);
    //-----------------------------------------------------------------------------
    if (gr->inGap) {
      if (gr->isGapOver()) {
	gr->inGap = false;
	gr->sendFeedUp(pEfhRunCtx);
      }
    }
    //-----------------------------------------------------------------------------

    /* if (sequence < gr->getExpectedSeq(streamIdx) && ! gr->inGap) { */
    /*   EKA_WARN("%s:%u BACK-IN-TIME WARNING: sequence %ju < expected_sequence %u", */
    /* 	       EKA_EXCH_DECODE(exch),gr_id,sequence,gr->getExpectedSeq(streamIdx)); */
    /*   gr->sendBackInTimeEvent(pEfhRunCtx,sequence); */
    /*   break;  */
    /* } */

    if (sequence > gr->getExpectedSeq(streamIdx) && ! gr->inGap) {
      gr->sendFeedDown(pEfhRunCtx);

      EKA_LOG("%s:%u for Stream %u expectedSeq = %u < sequence %ju, lost %ju messages",
	      EKA_EXCH_DECODE(exch),gr->id,gr->getId(streamIdx),gr->getExpectedSeq(streamIdx),sequence,sequence-gr->getExpectedSeq(streamIdx));
      gr->inGap = true;
      gr->setGapStart();
    }
    //-----------------------------------------------------------------------------

    if (gr->processUdpPkt(pEfhRunCtx,pktSize,streamIdx,pkt,msgInPkt,(uint64_t)sequence)) break;

    //-----------------------------------------------------------------------------
    runGr->udpCh->next(); 
  }
  EKA_INFO("%s RunGroup %u EndOfSession",EKA_EXCH_DECODE(exch),runGrId);
  runGr->sendFeedCloseAll(pEfhRunCtx);

  return EKA_OPRESULT__OK;
}
 /* ##################################################################### */

EkaOpResult EkaFhXdp::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, const EkaGroup* group) {
  static const int ReTryAttempts = 10;
  auto gr {dynamic_cast<EkaFhXdpGr*>(b_gr[(uint8_t)group->localId])};
  for (auto i = 0; i < ReTryAttempts; i++) {
    auto rc = getXdpDefinitions(pEfhCtx,
				pEfhRunCtx,
				gr,
				EkaFhMode::DEFINITIONS);
    if (rc == EKA_OPRESULT__OK) return rc;
    EKA_LOG("%s:%u Definitions attempt %d/%d failed: waiting %d seconds to retry",
	    EKA_EXCH_DECODE(exch),gr->id,i,ReTryAttempts,gr->connectRetryDelayTime);
    sleep(gr->connectRetryDelayTime);    
  }
}
