#include "EkaIgmp.h"
#include "EkaCore.h"
#include "EkaDev.h"
#include "EkaEpm.h"
#include "EkaFhRunGroup.h"
#include "EkaIgmpEntry.h"
#include "eka_macros.h"

void saveMcState(EkaDev *dev, int grId, int chId,
                 uint8_t coreId, uint32_t mcast_ip,
                 uint16_t mcast_port, uint64_t pktCnt);

/* ################################################## */

EkaIgmp::EkaIgmp(EkaDev *_dev) {
  dev = _dev;

  EKA_LOG("EkaIgmp created");

  dev->createThread(
      "Igmp", EkaServiceType::kIGMP, igmpThreadLoopCb, this,
      dev->createThreadContext, (uintptr_t *)&igmpPthread);
}

/* ################################################## */
EkaIgmp::~EkaIgmp() {
  threadActive = false;
  EKA_LOG("Waiting for igmpLoopTerminated");
  while (!igmpLoopTerminated)
    sleep(0);
}

/* ################################################## */
int EkaIgmp::mcJoin(int epmRegion, EkaCoreId coreId,
                    uint32_t ip, uint16_t port,
                    uint16_t vlanTag, uint64_t *pPktCnt) {
  createEntryMtx.lock();
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
  createEntryMtx.unlock();

  return perChId;
}

/* ################################################## */

/* ################################################## */
void *EkaIgmp::igmpThreadLoopCb(void *pEkaIgmp) {
  EkaIgmp *igmp = (EkaIgmp *)pEkaIgmp;
  EkaDev *dev = igmp->dev;

  std::string threadName = std::string("Igmp");
  EKA_LOG("%s igmpThreadLoopCb() started",
          threadName.c_str());
  igmp->igmpLoopTerminated = false;
  igmp->threadActive = true;

  auto lastNoMdTimeCheck = std::chrono::steady_clock::now();

  while (igmp->threadActive) {
    for (int i = 0; i < igmp->numIgmpEntries; i++) {
      igmp->igmpEntry[i]->sendIgmpJoin();
      igmp->igmpEntry[i]->saveMcState();
    }
    /* -------------------------------------- */
    if (isTradingHours(8, 00, 16, 30)) {
      static const int TimeOutSeconds = 4;
      auto now = std::chrono::steady_clock::now();
      auto timeSinceLastNoMdCheck =
          std::chrono::duration_cast<std::chrono::seconds>(
              now - lastNoMdTimeCheck)
              .count();
      if (timeSinceLastNoMdCheck > TimeOutSeconds) {
        for (uint i = 0; i < EkaDev::MAX_RUN_GROUPS; i++) {
          if (dev->runGr[i]) {
            dev->runGr[i]->checkNoMd = true;
          }
        }
        lastNoMdTimeCheck = now;
      }
    }
    /* -------------------------------------- */

    sleep(1);
  }
  igmp->igmpLeaveAll();
  igmp->igmpLoopTerminated = true;

  return 0;
}

/* ################################################## */
int EkaIgmp::igmpLeaveAll() {
  EKA_LOG("leaving all MC groups");
  for (int i = 0; i < numIgmpEntries; i++) {
    igmpEntry[i]->sendIgmpLeave();
    delete igmpEntry[i];
    igmpEntry[i] = nullptr;
  }

  return 0;
}
