#include "eka_macros.h"
#include "EkaDev.h"
#include "EkaIgmp.h"
#include "EkaIgmpEntry.h"
#include "EkaEpm.h"
#include "EkaCore.h"

void saveMcState(EkaDev* dev, int grId, int chId, uint8_t coreId, uint32_t mcast_ip, uint16_t mcast_port, uint64_t pktCnt);

/* ##################################################################### */

EkaIgmp::EkaIgmp(EkaDev* _dev) {
  dev       = _dev;

  EKA_LOG("EkaIgmp created");

#ifdef _NO_PTHREAD_CB_
  igmpThread = std::thread(&EkaIgmp::igmpThreadLoop,this);
  igmpThread.detach();
#else
  dev->createThread("Igmp",
  		    EkaServiceType::kIGMP,
  		    igmpThreadLoopCb,
  		    this,
  		    dev->createThreadContext,
  		    (uintptr_t*)&igmpPthread); 
#endif
}

/* ##################################################################### */
EkaIgmp::~EkaIgmp() {
  threadActive = false;
  EKA_LOG("%s: Waiting for igmpLoopTerminated");
  while (! igmpLoopTerminated) sleep(0);
}

/* ##################################################################### */
int EkaIgmp::mcJoin(int epmRegion, EkaCoreId coreId, uint32_t ip, uint16_t port, uint16_t vlanTag, uint64_t* pPktCnt) {
  for (auto i = 0; i < numIgmpEntries; i++) {
    if (igmpEntry[i] == NULL) on_error("igmpEntry[%d] == NULL",i);
    if (igmpEntry[i]->isMy(coreId,ip,port)) return 0;
  }
  if (numIgmpEntries == MAX_IGMP_ENTRIES) 
    on_error("numIgmpEntries %d == MAX_IGMP_ENTRIES %d",numIgmpEntries,MAX_IGMP_ENTRIES);

  igmpEntry[numIgmpEntries] = new EkaIgmpEntry(dev,epmRegion,coreId,ip,port,vlanTag,pPktCnt);
  if (igmpEntry[numIgmpEntries] == NULL) on_error("igmpEntry[%d] == NULL",numIgmpEntries);

  EKA_LOG("MC join: %s:%u",EKA_IP2STR(ip),port);

  numIgmpEntries++;

  return 0;
}


/* ##################################################################### */
int EkaIgmp::igmpThreadLoop() {
  std::string threadName = std::string("Igmp");
  pthread_setname_np(pthread_self(),threadName.c_str());

  EKA_LOG("%s igmpThreadLoop() started",threadName.c_str());
  igmpLoopTerminated = false;
  threadActive = true;

  while (threadActive) {
    for (int i = 0; i < numIgmpEntries; i++) {
      igmpEntry[i]->sendIgmpJoin();
      saveMcState(dev,
		  i,
		  igmpEntry[i]->udpChId, 
		  igmpEntry[i]->coreId, 
		  igmpEntry[i]->ip,
		  igmpEntry[i]->port,
		  igmpEntry[i]->pPktCnt == NULL ? 0 : *igmpEntry[i]->pPktCnt);
    }
    sleep (1);
  }
  igmpLeaveAll();
  igmpLoopTerminated = true;
  
  return 0;
}
/* ##################################################################### */
void* EkaIgmp::igmpThreadLoopCb(void* pEkaIgmp) {
  EkaIgmp* igmp = (EkaIgmp*)pEkaIgmp;
  EkaDev* dev = igmp->dev;

  std::string threadName = std::string("Igmp");
  EKA_LOG("%s igmpThreadLoopCb() started",threadName.c_str());
  igmp->igmpLoopTerminated = false;
  igmp->threadActive = true;

  while (igmp->threadActive) {
    for (int i = 0; i < igmp->numIgmpEntries; i++) {
      igmp->igmpEntry[i]->sendIgmpJoin();
      saveMcState(dev,
		  i,
		  igmp->igmpEntry[i]->udpChId, 
		  igmp->igmpEntry[i]->coreId, 
		  igmp->igmpEntry[i]->ip,
		  igmp->igmpEntry[i]->port,
		  igmp->igmpEntry[i]->pPktCnt == NULL ? 0 : *igmp->igmpEntry[i]->pPktCnt);
    }
    sleep (1);
  }
  igmp->igmpLeaveAll();
  igmp->igmpLoopTerminated = true;
  
  return 0;
}

/* ##################################################################### */
int EkaIgmp::igmpLeaveAll() {
  EKA_LOG("leaving all MC groups");
  for (int i = 0; i < numIgmpEntries; i++) {
    igmpEntry[i]->sendIgmpLeave();
    delete igmpEntry[i];
  }

  return 0;
}
