#include <assert.h>

#include "Efh.h"
#include "eka_macros.h"
#include "EkaCtxs.h"
#include "eka_dev.h"
#include "eka_fh.h"
#include "eka_fh_run_group.h"

/* ##################################################################### */

FhRunGr::FhRunGr (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx ) {
  dev = pEfhCtx->dev;
  runId = dev->numRunGr;
  fh = dev->fh[pEfhCtx->fhId];
  exch = fh->exch;
  firstGr = pEfhRunCtx->groups[0].localId;
  numGr = pEfhRunCtx->numGroups;

  udpCh = new fh_udp_channel(pEfhCtx);
  assert (udpCh != NULL);

  for (uint i = 0; i < MAX_GR2RUN; i++) grAfterGap[i] = false;
  cntGrAfterGap = 0;
  EKA_LOG("runId=%u, cntGrAfterGap = %u",runId,cntGrAfterGap);
  thread_active = true;
  hasGrpAfterGap = false;
  stoppedByExchange = false;

  EKA_LOG("%s: groups: %u .. %u",EKA_EXCH_DECODE(exch),firstGr,firstGr+numGr-1);
}
/* ##################################################################### */

uint FhRunGr::getGrAfterGap() {
  if (cntGrAfterGap == 0) on_error("cntGrAfterGap = 0");
  for (uint i = 0; i < MAX_GR2RUN; i ++) if (grAfterGap[i]) return i;
  on_error("grAfterGap not found");
  return 0xFF;
}
/* ##################################################################### */
void FhRunGr::setGrAfterGap(uint i) {
  if (cntGrAfterGap > numGr) on_error("cntGrAfterGap %u > numGr %u",cntGrAfterGap, numGr);

  grAfterGap[i] = true;
  cntGrAfterGap ++;  

  if (cntGrAfterGap > numGr) on_error("cntGrAfterGap %u > numGr %u",cntGrAfterGap,numGr);
  hasGrpAfterGap = true;
}
/* ##################################################################### */
void FhRunGr::clearGrAfterGap(uint i) {
  if (cntGrAfterGap > numGr) on_error("cntGrAfterGap %u > numGr %u",cntGrAfterGap, numGr);

  if (cntGrAfterGap == 0) on_error("cntGrAfterGap = 0");
  grAfterGap[i] = false;
  cntGrAfterGap --;

  if (cntGrAfterGap == 0) hasGrpAfterGap = false;
}
/* ##################################################################### */