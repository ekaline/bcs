#include "EkaFhRunGroup.h"
#include "EkaFhGroup.h"
#include "EkaUdpChannel.h"
#include "eka_fh_q.h"
#include "EkaIgmpEntry.h"
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
  
  dev->epm->createRegion((uint)udpChId, udpChId * EkaEpm::ActionsPerRegion);

  for (uint i = 0; i < MAX_GR2RUN; i++) grAfterGap[i] = false;
  cntGrAfterGap      = 0;

  thread_active      = true;
  hasGrpAfterGap     = false;
  stoppedByExchange  = false;

  EKA_LOG("%s: coreId = %u, runId = %u, udpChId = %d, MC groups: %s",
	  EKA_EXCH_DECODE(exch),coreId,runId,udpChId, list2print);

  igmpThread = std::thread(&EkaFhRunGroup::igmpThreadLoop,this);
  igmpThread.detach();
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
int EkaFhRunGroup::igmpMcJoin(uint32_t ip, uint16_t port, uint16_t vlanTag) {
  for (auto i = 0; i < numIgmpEntries; i++) {
    if (igmpEntry[i] == NULL) on_error("igmpEntry[%d] == NULL",i);
    if (igmpEntry[i]->isMy(ip,port)) return 0;
  }
  if (numIgmpEntries == MAX_IGMP_ENTRIES) 
    on_error("numIgmpEntries %d == MAX_IGMP_ENTRIES %d",numIgmpEntries,MAX_IGMP_ENTRIES);

  igmpEntry[numIgmpEntries] = new EkaIgmpEntry(dev,udpChId,ip,port,vlanTag,coreId);
  if (igmpEntry[numIgmpEntries] == NULL) on_error("igmpEntry[%d] == NULL",numIgmpEntries);

  EKA_LOG("Run Gr %u: MC join: %s:%u",runId,EKA_IP2STR(ip),port);

  numIgmpEntries++;

  udpCh->igmp_mc_join (dev->core[coreId]->srcIp, ip, port, 0);
  return 0;
}


/* ##################################################################### */
int EkaFhRunGroup::igmpThreadLoop() {
  std::string threadName = "EfhIgmp" + std::to_string(runId);
  pthread_setname_np(pthread_self(),threadName.c_str());

  EKA_LOG("Run Gr %u: %s igmpThreadLoop() lunched",runId,threadName.c_str());
  igmpLoopTerminated = false;

  while (thread_active) {
    for (int i = 0; i < numIgmpEntries; i++) {
      igmpEntry[i]->sendIgmpJoin();
    }
    sleep (1);
  }
  igmpLeaveAll();
  igmpLoopTerminated = true;
  
  return 0;
}

/* ##################################################################### */
int EkaFhRunGroup::igmpLeaveAll() {
  EKA_LOG("Run Gr %u: leaving MC groups",runId);
  for (int i = 0; i < numIgmpEntries; i++) {
    igmpEntry[i]->sendIgmpLeave();
    delete igmpEntry[i];
  }
  return 0;
}
/* ##################################################################### */
EkaFhRunGroup::~EkaFhRunGroup() {
  thread_active = false;
  //  igmpLeaveAll();

  EKA_LOG("Run Gr %u: Waiting for igmpLoopTerminated",runId);
  while (! igmpLoopTerminated) sleep(0);

  EKA_LOG("Run Gr %u: terminated",runId);  
}
