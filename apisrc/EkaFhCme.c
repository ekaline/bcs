#include "EkaFhCme.h"
#include "EkaUdpChannel.h"
#include "EkaFhRunGroup.h"
#include "EkaFhCmeGr.h"
#include "EkaFhCmeParser.h"

EkaOpResult getCmeDefinitions(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaFhCmeGr* gr,EkaFhMode op);
int ekaUdpMcConnect(EkaDev* dev, uint32_t ip, uint16_t port);

/* ##################################################################### */
EkaFhGroup* EkaFhCme::addGroup() {
  return new EkaFhCmeGr();
}

/* ##################################################################### */
const uint8_t* EkaFhCme::getUdpPkt(EkaFhRunGroup* runGr, 
			     uint*          msgInPkt, 
			     uint*          pktSize, 
			     uint64_t*      sequence,
			     uint8_t*       gr_id,
			     uint16_t*      streamId, 
			     uint8_t*       pktType) {

  uint8_t* pkt = (uint8_t*)runGr->udpCh->get();
  if (pkt == NULL) on_error("%s: pkt == NULL",EKA_EXCH_DECODE(exch));

  *msgInPkt = EKA_CME_MSG_CNT((pkt));
  *sequence = EKA_CME_SEQUENCE((pkt));
  *gr_id    = getGrId(pkt);
  *streamId = EKA_CME_STREAM_ID((pkt));
  *pktType  = EKA_CME_PKT_TYPE((pkt));
  *pktSize  = EKA_CME_PKT_SIZE((pkt));

  return pkt;
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

  for (uint8_t j = 0; j < runGr->numGr; j++) {
    uint8_t grId = runGr->groupList[j];
    EkaFhCmeGr* gr = (EkaFhCmeGr*)b_gr[grId];
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

    EkaFhCmeGr* gr = (EkaFhCmeGr*)b_gr[gr_id];
    if (gr == NULL) on_error("b_gr[%u] == NULL",gr_id);
    gr->resetNoMdTimer();

    uint streamIdx = gr->findAndInstallStream(streamId, sequence);

    /* EKA_LOG("%s:%u Seq=%ju,expSeq=%ju, pktSize=%u msgInPkt =%u",EKA_EXCH_DECODE(exch),gr_id, */
    /* 	    sequence,gr->getExpectedSeq(streamIdx),pktSize,msgInPkt); */


    //-----------------------------------------------------------------------------
    if (pktType == (uint8_t)EKA_CME_DELIVERY_FLAG::SequenceReset) gr->resetExpectedSeq(streamIdx);
    //-----------------------------------------------------------------------------
    if (gr->inGap) {
      if (gr->isGapOver()) {
	gr->inGap = false;
	gr->sendFeedUp(pEfhRunCtx);
      }
    }
    //-----------------------------------------------------------------------------
    if (sequence > gr->getExpectedSeq(streamIdx) && ! gr->inGap) {
      gr->sendFeedDown(pEfhRunCtx);

      EKA_LOG("%s:%u for Stream %u expectedSeq = %u < sequence %u, lost %u messages",
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

EkaOpResult EkaFhCme::getDefinitions (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  EkaFhCmeGr* gr = b_gr[(uint8_t)group->localId];
  if (gr == NULL) on_error("gr[%u] == NULL",(uint8_t)group->localId);

  int sock = ekaUdpMcConnect(dev, gr->snapshot_ip, gr->snapshot_port);
  if (sock < 0) on_error ("sock = %d",sock);

  sockaddr_in addr = {};
  addr.sin_addr.s_addr = gr->snapshot_ip;
  addr.sin_port        = gr->snapshot_port;
  socklen_t addrlen = sizeof(sockaddr);

  while (runGr->thread_active) {
    uint8_t pkt[1536] = {};
    int size = recvfrom(sock, pkt, sizeof(pkt), 0, (sockaddr*) &addr, &addrlen);
    if (size < 0) on_error("size = %d",size);
    gr->processUdpPkt(pEfhRunCtx,pkt,size,EkaFhMode::DEFINITIONS);
  }

  close (sock);
  return EKA_OPRESULT__OK;

}
