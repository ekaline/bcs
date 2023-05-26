#include <string>

#include "EkaFhRunGroup.h"
#include "EkaFhGroup.h"
#include "EkaUdpChannel.h"
#include "eka_fh_q.h"
#include "EkaIgmp.h"
#include "EkaCore.h"
#include "EkaEpm.h"
#include "EkaSnDev.h"
#include "eka_sn_addr_space.h"
#include "EkaMcState.h"
#include "EkaEpmRegion.h"
#include "ctls.h"
#include "eka.h"
#include "smartnic.h"


/* ######################################################### */

EkaFhRunGroup::EkaFhRunGroup (EfhCtx* pEfhCtx,
			      const EfhRunCtx* pEfhRunCtx,
			      uint8_t _runId ) {
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

  udpCh = new EkaUdpChannel(dev,dev->snDev->dev_id,coreId,-1);
  if (!udpCh)
    on_error("!udpCh");
  
  udpChId            = udpCh->chId;
  
  for (uint i = 0; i < MAX_GR2RUN; i++)
		grAfterGap[i] = false;
  cntGrAfterGap      = 0;

  thread_active      = true;
  hasGrpAfterGap     = false;
  stoppedByExchange  = false;
  allGroupsClosed    = false;

  char name[50] = {};
  sprintf(name,"RunGr%u",runId);

  epmRegionId = EkaEpmRegion::getEfhIgmpRegion(udpChId);
  dev->epm->createRegion(epmRegionId);

  EKA_LOG("%s: coreId = %u, runId = %u, udpChId = %d, "
	  "epmRegionId = %d, MC groups: %s",
	  EKA_EXCH_DECODE(exch),coreId,runId,udpChId,
	  epmRegionId,list2print);

}
/* ######################################################### */
bool EkaFhRunGroup::igmpSanityCheck(int grId2check,
				    uint32_t ip, uint16_t port) {
  /* EKA_LOG("%s:%u on coreId %d udpChId %u", */
  /* 	  EKA_IP2STR(ip),port,coreId,udpChId); */
  struct McChState {
    int          num            = 0;  // num of "logical" groups at this Channel
    int          currHwGr       = 0;
    EkaMcState   grpState[64]   = {};
    EkaHwMcState grpHwState[64] = {};
  };

  struct McState {
    int          totalNum    = 0;
    int          maxGrPerCh  = 0;
    McChState    chState[32] = {};
  };

  McState mcState = {};

  sc_multicast_subscription_t hwIgmp[8 * 64] = {};

  int fd = SN_GetFileDescriptor(dev->snDev->dev_id);
  if (fd < 0)
    on_error("fd = %d",fd);

  eka_ioctl_t __attribute__ ((aligned(0x1000))) state = {};

  state.cmd = EKA_GET_IGMP_STATE;
  state.paramA = (uint64_t)hwIgmp;

  int rc = ioctl(fd,SMARTNIC_EKALINE_DATA,&state);
  if (rc != 0) on_error("ioctl failed: rc = %d",rc);

  for (auto chId = 0; chId < 32; chId++) {
    mcState.chState[chId].currHwGr = 0;
  }

  for (auto i = 0; i < 512; i++) {
    int chId = EkaEpmRegion::getEfhIgmpRegion(hwIgmp[i].channel - 32);
    if ((hwIgmp[i].group_address != 0) && (chId < 0 || chId > 31)) 
      on_error("chId=%d,hwIgmp[%d].channel=%u",chId,i,hwIgmp[i].channel);
    if (hwIgmp[i].group_address == 0) continue;

    int grId = mcState.chState[chId].currHwGr;
    if (grId < 0 || grId > 63) 
      on_error("chId=%d,grId=%d,hwIgmp[%d].positionIndex=%u,"
	       "group_address=0x%08x",
	       chId,grId,i,hwIgmp[i].positionIndex,
	       hwIgmp[i].group_address);

    EKA_LOG ("%3d (%3d): pos=%3u, lane=%u, ch=%2u (%d), %s:%u",
	     i, grId,
	     hwIgmp[i].positionIndex,
	     hwIgmp[i].lane,
	     hwIgmp[i].channel,chId,
	     EKA_IP2STR(be32toh(hwIgmp[i].group_address)),
	     hwIgmp[i].ip_port_number
	     );

    mcState.chState[chId].grpHwState[grId].coreId = hwIgmp[i].lane;
    mcState.chState[chId].grpHwState[grId].ip = be32toh(hwIgmp[i].group_address);
    mcState.chState[chId].grpHwState[grId].port = hwIgmp[i].ip_port_number;
    mcState.chState[chId].grpHwState[grId].positionIndex = hwIgmp[i].positionIndex;
    mcState.chState[chId].currHwGr++;
  }

  if (coreId != mcState.chState[epmRegionId].grpHwState[grId2check].coreId) {
    EKA_WARN("WARNING: chId %d, grId %d: "
						 "expected coreId %d != actual %d",
	     epmRegionId,
	     grId2check,
	     coreId,
	     mcState.chState[epmRegionId].grpHwState[grId2check].coreId);
    return false;
  }

  if (ip != mcState.chState[epmRegionId].grpHwState[grId2check].ip || 
      port != mcState.chState[epmRegionId].grpHwState[grId2check].port) {
    EKA_WARN("WARNING: chId %d, grId %d: "
						 "expected %s:%u != actual %s:%u",
	     epmRegionId,grId2check,
	     EKA_IP2STR(ip),
	     port,
	     EKA_IP2STR(mcState.chState[epmRegionId].grpHwState[grId2check].ip),
	     mcState.chState[epmRegionId].grpHwState[grId2check].port
	     );
    return false;
  }

  return true;
}
/* ######################################################### */
int EkaFhRunGroup::igmpMcJoin(uint32_t ip, uint16_t port,
			      uint16_t vlanTag, uint64_t* pPktCnt) {
  dev->igmpJoinMtx.lock();
  int grId = dev->ekaIgmp->mcJoin(epmRegionId,coreId,ip,port,
																	vlanTag,pPktCnt);
  udpCh->igmp_mc_join (ip, port, 0);
  bool sanityCheckPassed = igmpSanityCheck(grId,ip,port);
  EKA_LOG("%d: %s:%u on coreId %d epmRegionId %d: %s",
	  grId,
	  EKA_IP2STR(ip),port,coreId,epmRegionId,
	  sanityCheckPassed ? GRN "PASSED" RESET : RED "FAILED" RESET);
  dev->igmpJoinMtx.unlock();
  return 0;
}

/* ######################################################### */
int EkaFhRunGroup::checkTimeOut(const EfhRunCtx* pEfhRunCtx) {
  static const int TimeOutSeconds = 4;
  static const int TimeOutSample  = 1000;

  if (++timeOutCntr % TimeOutSample != 0) return 0;
 
  for (auto i = 0; i < numGr; i++) {
    EkaFhGroup* gr = fh->b_gr[groupList[i]];
    if (!gr)
			on_error("!fh->gr[%u]",groupList[i]);

    auto now = std::chrono::high_resolution_clock::now();

    if (! gr->lastMdReceivedValid) {
      gr->lastMdReceivedValid = true;
      gr->lastMdReceived      = now;
      continue;
    }

    if (std::chrono::duration_cast<std::chrono::seconds>
				(now - gr->lastMdReceived).count() > TimeOutSeconds) {
#ifndef _DONT_SEND_MDTIMEOUT
      gr->sendNoMdTimeOut(pEfhRunCtx);
#endif
      gr->lastMdReceived      = now;
    }

  }
  return 0;
}
/* ######################################################### */

int EkaFhRunGroup::sendFeedCloseAll(const EfhRunCtx* pEfhRunCtx) {
  for (auto i = 0; i < numGr; i++) {
    EkaFhGroup* gr = fh->b_gr[groupList[i]];
    if (!gr) {
      EKA_WARN("!fh->gr[%u]",groupList[i]);
      continue;
    }
    gr->sendFeedDownClosed(pEfhRunCtx);
  }
  allGroupsClosed = true;
  return 0;
}

/* ######################################################### */


uint EkaFhRunGroup::getGrAfterGap() {
  if (cntGrAfterGap == 0) on_error("cntGrAfterGap = 0");
  for (uint i = 0; i < MAX_GR2RUN; i ++)
    if (grAfterGap[i])
      return i;
  on_error("grAfterGap not found");
  return 0xFF;
}
/* ######################################################### */
void EkaFhRunGroup::setGrAfterGap(uint i) {
  if (cntGrAfterGap > numGr)
    on_error("cntGrAfterGap %u > numGr %u",
						 cntGrAfterGap, numGr);

  grAfterGap[i] = true;
  cntGrAfterGap ++;  

  if (cntGrAfterGap > numGr)
    on_error("cntGrAfterGap %u > numGr %u",
						 cntGrAfterGap,numGr);
  hasGrpAfterGap = true;
}
/* ######################################################### */
void EkaFhRunGroup::clearGrAfterGap(uint i) {
  if (cntGrAfterGap > numGr)
    on_error("cntGrAfterGap %u > numGr %u",
						 cntGrAfterGap, numGr);

  if (cntGrAfterGap == 0)
    on_error("cntGrAfterGap = 0");
  grAfterGap[i] = false;
  cntGrAfterGap --;

  if (cntGrAfterGap == 0) hasGrpAfterGap = false;
}
/* ######################################################### */
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

/* ######################################################### */
EkaFhRunGroup::~EkaFhRunGroup() {
  thread_active      = false;

  EKA_LOG("Waiting for all groups sending FeedDownClosed...");
  while (! allGroupsClosed) {
    sleep(0);
  }

  EKA_LOG("Run Gr %u: terminated",runId);  
}
