#include "EkaIgmp.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEpm.h"
#include "EkaIgmpEntry.h"
#include "eka_macros.h"

void saveMcState(EkaDev *dev, int grId, int chId,
                 uint8_t coreId, uint32_t mcast_ip,
                 uint16_t mcast_port, uint64_t pktCnt);
void saveMcState(EkaDev *dev, int grId, int chId,
                 uint8_t coreId, uint32_t mcast_ip,
                 uint16_t mcast_port, uint64_t pktCnt);

/* ################################################## */

EkaIgmp::EkaIgmp(EkaDev *_dev) {
  dev = _dev;

  EKA_LOG("EkaIgmp created");

  igmpThread =
      std::thread(&EkaIgmp::igmpThreadLoopCb, this);

#if 0
  dev->createThread(
      "Igmp", EkaServiceType::kIGMP, igmpThreadLoopCb, this,
      dev->createThreadContext, (uintptr_t *)&igmpPthread);
#endif
}

/* ################################################## */
EkaIgmp::~EkaIgmp() {
  threadActive_ = false;
  createEntryMtx_.unlock();

#if 0
  EKA_LOG("Waiting for igmpLoopTerminated");
  while (!igmpLoopTerminated_)
    sleep(0);
#endif

  EKA_LOG("igmpThread.join()");

  // pthread_join(igmpPthread, NULL);
  igmpThread.join();
}

/* ################################################## */
int EkaIgmp::mcJoin(int epmRegion, EkaCoreId coreId,
                    uint32_t ip, uint16_t port,
                    uint16_t vlanTag, uint64_t *pPktCnt) {
  createEntryMtx_.lock();
  int perChId = -1;

  for (auto i = 0; i < numIgmpEntries; i++) {
    if (!igmpEntry[i])
      on_error("!igmpEntry[%d]", i);
    if (igmpEntry[i]->isMy(coreId, ip, port)) {
      perChId = i;
      goto EXISTING_ENTRY;
    }
  }
  if (numIgmpEntriesAtCh[epmRegion] == MAX_ENTRIES_PER_LANE)
    on_error("numIgmpEntriesAtCh[%d] %d == "
             "MAX_ENTRIES_PER_LANE %d",
             epmRegion, numIgmpEntriesAtCh[epmRegion],
             MAX_ENTRIES_PER_LANE);

  if (numIgmpEntriesAtCore[coreId] == MAX_ENTRIES_PER_LANE)
    on_error("numIgmpEntriesAtCore[%d] %d == "
             "MAX_ENTRIES_PER_LANE %d",
             coreId, numIgmpEntriesAtCore[coreId],
             MAX_ENTRIES_PER_LANE);

  perChId = numIgmpEntriesAtCh[epmRegion];

  igmpEntry[numIgmpEntries] =
      new EkaIgmpEntry(dev, epmRegion, coreId, perChId, ip,
                       port, vlanTag, pPktCnt);
  if (igmpEntry[numIgmpEntries] == NULL)
    on_error("igmpEntry[%d] == NULL", numIgmpEntries);

  EKA_LOG("MC join: chId=%d, coreId=%d, entryId=%d: %s:%u",
          epmRegion, coreId, perChId, EKA_IP2STR(ip), port);

  numIgmpEntries++;
  numIgmpEntriesAtCh[epmRegion]++;
  numIgmpEntriesAtCore[coreId]++;

EXISTING_ENTRY:
  createEntryMtx_.unlock();

  return perChId;
}

/* ################################################## */

/* ################################################## */
void EkaIgmp::igmpThreadLoopCb() {
  std::string threadName = std::string("Igmp");
  EKA_LOG("%s igmpThreadLoopCb() started",
          threadName.c_str());
  igmpLoopTerminated_ = false;
  threadActive_ = true;

  pthread_setname_np(pthread_self(), threadName.c_str());

  if (dev->affinityConf.igmpThreadCpuId >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(dev->affinityConf.igmpThreadCpuId, &cpuset);
    int rc = pthread_setaffinity_np(
        pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc < 0)
      on_error("Failed to set affinity");
    EKA_LOG("Affinity is set to CPU %d",
            dev->affinityConf.igmpThreadCpuId);
  }

  while (threadActive_) {
    for (int i = 0; threadActive_ && i < numIgmpEntries;
         i++) {
      igmpEntry[i]->sendIgmpJoin();
      igmpEntry[i]->saveMcState();
    }
    /* -------------------------------------- */
    if (threadActive_)
      sleep(1);
  }
  EKA_LOG("Loop terminated");

  igmpLeaveAll();
  EKA_LOG("All MC groups left");
  igmpLoopTerminated_ = true;

  return;
}

/* ################################################## */
int EkaIgmp::igmpLeaveAll() {
  EKA_LOG("leaving all %d MC groups", numIgmpEntries);
  for (int i = 0; i < numIgmpEntries; i++) {
    igmpEntry[i]->sendIgmpLeave();
    delete igmpEntry[i];
    igmpEntry[i] = nullptr;
  }

  return 0;
}
