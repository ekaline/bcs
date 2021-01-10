#include "eka_macros.h"
#include "EkaDev.h"
#include "EkaIgmp.h"
#include "EkaIgmpEntry.h"
#include "EkaEpm.h"
#include "EkaCore.h"


/* ##################################################################### */

EkaIgmp::EkaIgmp(EkaDev* _dev, /* EkaUdpChannel* _udpCh,  */uint8_t _coreId, uint _epmRegion, const char* _name) {
  dev       = _dev;
  /* udpCh     = _udpCh; */
  coreId    = _coreId;
  epmRegion = _epmRegion;
  strcpy(name,_name);

  dev->epm->createRegion(epmRegion, epmRegion * EkaEpm::ActionsPerRegion);
#ifdef _NO_PTHREAD_CB_
  igmpThread = std::thread(&EkaIgmp::igmpThreadLoop,this);
  igmpThread.detach();
#else
  dev->createThread(name,
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
  EKA_LOG("%s: Waiting for igmpLoopTerminated",name);
  while (! igmpLoopTerminated) sleep(0);
}

/* ##################################################################### */
int EkaIgmp::mcJoin(uint32_t ip, uint16_t port, uint16_t vlanTag) {
  for (auto i = 0; i < numIgmpEntries; i++) {
    if (igmpEntry[i] == NULL) on_error("igmpEntry[%d] == NULL",i);
    if (igmpEntry[i]->isMy(ip,port)) return 0;
  }
  if (numIgmpEntries == MAX_IGMP_ENTRIES) 
    on_error("numIgmpEntries %d == MAX_IGMP_ENTRIES %d",numIgmpEntries,MAX_IGMP_ENTRIES);

  igmpEntry[numIgmpEntries] = new EkaIgmpEntry(dev,epmRegion,ip,port,vlanTag,coreId);
  if (igmpEntry[numIgmpEntries] == NULL) on_error("igmpEntry[%d] == NULL",numIgmpEntries);

  EKA_LOG("%s: MC join: %s:%u",name,EKA_IP2STR(ip),port);

  numIgmpEntries++;

  /* udpCh->igmp_mc_join (dev->core[coreId]->srcIp, ip, port, 0); */
  return 0;
}


/* ##################################################################### */
int EkaIgmp::igmpThreadLoop() {
  std::string threadName = std::string("Igmp_") + std::string(name);
  pthread_setname_np(pthread_self(),threadName.c_str());

  EKA_LOG("%s: %s igmpThreadLoop() lunched",name,threadName.c_str());
  igmpLoopTerminated = false;
  threadActive = true;

  while (threadActive) {
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
void* EkaIgmp::igmpThreadLoopCb(void* pEkaIgmp) {
  EkaIgmp* igmp = (EkaIgmp*)pEkaIgmp;
  EkaDev* dev = igmp->dev;

  std::string threadName = std::string("Igmp_") + std::string(igmp->name);
  EKA_LOG("%s: %s igmpThreadLoopCb() lunched",igmp->name,threadName.c_str());
  igmp->igmpLoopTerminated = false;
  igmp->threadActive = true;

  while (igmp->threadActive) {
    for (int i = 0; i < igmp->numIgmpEntries; i++) {
      igmp->igmpEntry[i]->sendIgmpJoin();
    }
    sleep (1);
  }
  igmp->igmpLeaveAll();
  igmp->igmpLoopTerminated = true;
  
  return 0;
}

/* ##################################################################### */
int EkaIgmp::igmpLeaveAll() {
  EKA_LOG("%s: leaving MC groups",name);
  for (int i = 0; i < numIgmpEntries; i++) {
    igmpEntry[i]->sendIgmpLeave();
    delete igmpEntry[i];
  }

  return 0;
}
