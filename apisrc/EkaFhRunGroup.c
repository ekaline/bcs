#include "EkaFhRunGroup.h"
#include "EkaFhGroup.h"
#include "EkaUdpChannel.h"
#include "eka_fh_q.h"

/* ##################################################################### */

EkaFhRunGroup::EkaFhRunGroup (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t _runId ) {
  dev      = pEfhCtx->dev;
  coreId   = pEfhCtx->coreId;

  runId    = _runId;
  fh       = dev->fh[pEfhCtx->fhId];
  exch     = fh->exch;
  //  firstGr = pEfhRunCtx->groups[0].localId;
  numGr    = pEfhRunCtx->numGroups;

  int n = 0;
  for (auto i = 0; i < numGr; i++) {
    groupList[i] = pEfhRunCtx->groups[i].localId;
    n += sprintf (&list2print[n], "%d,", groupList[i]);
  }

  udpCh    = new EkaUdpChannel(dev,coreId);
  if (udpCh == NULL) on_error("udpCh == NULL");

  for (uint i = 0; i < MAX_GR2RUN; i++) grAfterGap[i] = false;
  cntGrAfterGap      = 0;

  thread_active      = true;
  hasGrpAfterGap     = false;
  stoppedByExchange  = false;

  EKA_LOG("%s: runId = %u,groups: %s",EKA_EXCH_DECODE(exch),runId,list2print);
}
/* ##################################################################### */

uint EkaFhRunGroup::getGrAfterGap() {
  if (cntGrAfterGap == 0) on_error("cntGrAfterGap = 0");
  for (uint i = 0; i < MAX_GR2RUN; i ++) if (grAfterGap[i]) return i;
  on_error("grAfterGap not found");
  return 0xFF;
}
/* ##################################################################### */
void EkaFhRunGroup::setGrAfterGap(uint i) {
  if (cntGrAfterGap > numGr) on_error("cntGrAfterGap %u > numGr %u",cntGrAfterGap, numGr);

  grAfterGap[i] = true;
  cntGrAfterGap ++;  

  if (cntGrAfterGap > numGr) on_error("cntGrAfterGap %u > numGr %u",cntGrAfterGap,numGr);
  hasGrpAfterGap = true;
}
/* ##################################################################### */
void EkaFhRunGroup::clearGrAfterGap(uint i) {
  if (cntGrAfterGap > numGr) on_error("cntGrAfterGap %u > numGr %u",cntGrAfterGap, numGr);

  if (cntGrAfterGap == 0) on_error("cntGrAfterGap = 0");
  grAfterGap[i] = false;
  cntGrAfterGap --;

  if (cntGrAfterGap == 0) hasGrpAfterGap = false;
}
/* ##################################################################### */
bool EkaFhRunGroup::drainQ(const EfhRunCtx* pEfhRunCtx) {
  //  EKA_LOG("hasGrpAfterGap = %d",hasGrpAfterGap);
  if (hasGrpAfterGap) {
    EkaFhGroup* gr = fh->b_gr[getGrAfterGap()];
    while (! gr->q->is_empty()) {
      fh_msg* buf = gr->q->pop();
      //      EKA_LOG("q_len=%u,buf->sequence=%ju, gr->expected_sequence=%ju",gr->q->get_len(),buf->sequence,gr->expected_sequence);
      if (gr->exch != EkaSource::kBOX_HSVF)
	if (buf->sequence < gr->expected_sequence) continue;
      gr->parseMsg(pEfhRunCtx,(unsigned char*)buf->data,buf->sequence,EkaFhMode::MCAST);
      gr->expected_sequence = buf->sequence + 1;
    }
    clearGrAfterGap(gr->id);
    return true;
  }
  return false;
}
/* ##################################################################### */
