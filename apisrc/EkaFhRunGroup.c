#include "EkaFhRunGroup.h"
#include "EkaFhGroup.h"
#include "EkaUdpChannel.h"
#include "eka_fh_q.h"
#include "EkaIgmp.h"
#include "EkaCore.h"
#include "EkaEpm.h"
#include <string>

/* ##################################################################### */

EkaFhRunGroup::EkaFhRunGroup (EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t _runId ) {
  dev      = pEfhCtx->dev;

  runId    = _runId;
  fh       = dev->fh[pEfhCtx->fhId];
  if (fh == NULL) on_error("fh = NULL");
  exch     = fh->exch;

  numGr    = pEfhRunCtx->numGroups;
  coreId   = fh->c;

  int n = 0;
  for (auto i = 0; i < numGr; i++) {
    groupList[i] = pEfhRunCtx->groups[i].localId;
    n += sprintf (&list2print[n], "%d,", groupList[i]);
  }

  udpCh    = new EkaUdpChannel(dev,coreId);
  if (udpCh == NULL) on_error("udpCh == NULL");

  udpChId            = udpCh->chId;
  

  for (uint i = 0; i < MAX_GR2RUN; i++) grAfterGap[i] = false;
  cntGrAfterGap      = 0;

  thread_active      = true;
  hasGrpAfterGap     = false;
  stoppedByExchange  = false;

  char name[50] = {};
  sprintf(name,"RunGr%u",runId);
  ekaIgmp = new EkaIgmp(dev,/* udpCh, */coreId,udpChId,name);
  if (ekaIgmp == NULL) on_error("ekaIgmp == NULL");

  EKA_LOG("%s: coreId = %u, runId = %u, udpChId = %d, MC groups: %s",
	  EKA_EXCH_DECODE(exch),coreId,runId,udpChId, list2print);

}
/* ##################################################################### */

int EkaFhRunGroup::igmpMcJoin(uint32_t ip, uint16_t port, uint16_t vlanTag) {
  ekaIgmp->mcJoin(ip,port,vlanTag);
  udpCh->igmp_mc_join (ip, ip, port, 0);
  return 0;
}

/* ##################################################################### */
int EkaFhRunGroup::checkTimeOut(const EfhRunCtx* pEfhRunCtx) {
  static const int TimeOutSeconds = 4;
  static const int TimeOutSample  = 1000;

  if (++timeOutCntr % TimeOutSample != 0) return 0;
 
  for (auto i = 0; i < numGr; i++) {
    EkaFhGroup* gr = fh->b_gr[groupList[i]];
    if (gr == NULL) on_error("fh->gr[%u] == NULL",groupList[i]);

    auto now = std::chrono::high_resolution_clock::now();

    if (! gr->lastMdReceivedValid) {
      gr->lastMdReceivedValid = true;
      gr->lastMdReceived      = now;
      continue;
    }

    if (std::chrono::duration_cast<std::chrono::seconds>(now - gr->lastMdReceived).count() > TimeOutSeconds) {
      gr->sendNoMdTimeOut(pEfhRunCtx);
      gr->lastMdReceived      = now;
    }

  }
  return 0;
}
/* ##################################################################### */

int EkaFhRunGroup::sendFeedCloseAll(const EfhRunCtx* pEfhRunCtx) {
  for (auto i = 0; i < numGr; i++) {
    EkaFhGroup* gr = fh->b_gr[groupList[i]];
    if (gr == NULL) {
      EKA_WARN("fh->gr[%u] == NULL",groupList[i]);
      continue;
    }
    gr->sendFeedDownClosed(pEfhRunCtx);
  }
  return 0;
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

    gr->processFromQ(pEfhRunCtx);

    clearGrAfterGap(gr->id);
    return true;
  }
  return false;
}

/* ##################################################################### */
EkaFhRunGroup::~EkaFhRunGroup() {
  thread_active      = false;

  delete ekaIgmp;

  EKA_LOG("Run Gr %u: terminated",runId);  
}
