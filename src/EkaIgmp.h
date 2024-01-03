#ifndef _EKA_IGMP_H_
#define _EKA_IGMP_H_

#include <pthread.h>
#include <thread>

#include "Eka.h"

class EkaIgmpEntry;
class EkaDev;

class EkaIgmp {
public:
  EkaIgmp(EkaDev *dev);
  ~EkaIgmp();

  int mcJoin(int epmRegion, EkaCoreId coreId, uint32_t ip,
             uint16_t port, uint16_t vlanTag,
             uint64_t *pPktCnt);

public:
  volatile bool threadActive_ = false;
  volatile bool igmpLoopTerminated_ = false;

private:
  static const int MAX_LANES = 8;
  static const int MAX_ENTRIES_PER_LANE = 64;
  static const int MAX_IGMP_ENTRIES =
      MAX_LANES * MAX_ENTRIES_PER_LANE;
  static const int MAX_UDP_CHANNELS = 32;

  void igmpThreadLoopCb();
  int igmpThreadLoop();
  int igmpLeaveAll();

  /* ------------------------------------------------- */

  std::thread igmpThread;
  //  pthread_t             igmpPthread;

  EkaIgmpEntry *igmpEntry[MAX_IGMP_ENTRIES] = {};
  int numIgmpEntries = 0;
  int numIgmpEntriesAtCh[MAX_UDP_CHANNELS] = {};
  int numIgmpEntriesAtCore[MAX_LANES] = {};
  std::mutex createEntryMtx_;

  EkaDev *dev = NULL;
};

#endif
