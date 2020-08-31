#include <assert.h>

#include "Efh.h"
#include "eka_macros.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "eka_fh.h"
#include "EkaFhRunGr.h"
#include "EkaUdpChannel.h"

/* ##################################################################### */

FhRunGr::FhRunGr (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t _runId ) {
  dev = pEfhCtx->dev;
  coreId = 0; // TMP

  runId = _runId;
  fh = dev->fh[pEfhCtx->fhId];
  exch = fh->exch;
  //  firstGr = pEfhRunCtx->groups[0].localId;
  numGr = pEfhRunCtx->numGroups;
  int n = 0;

  for (auto i = 0; i < numGr; i++) {
    groupList[i] = pEfhRunCtx->groups[i].localId;
    n += sprintf (&list2print[n], "%d,", groupList[i]);
  }

  udpCh = new EkaUdpChannel(dev,coreId);
  assert (udpCh != NULL);

  for (uint i = 0; i < MAX_GR2RUN; i++) grAfterGap[i] = false;
  cntGrAfterGap = 0;

  thread_active = true;
  hasGrpAfterGap = false;
  stoppedByExchange = false;

  EKA_LOG("%s: runId = %u,groups: %s",EKA_EXCH_DECODE(exch),runId,list2print);
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
