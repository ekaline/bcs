#ifndef _EKA_IGMP_H_
#define _EKA_IGMP_H_

#include <thread>
#include <pthread.h>

#include "Eka.h"

class EkaIgmpEntry;
class EkaDev;

class EkaIgmp {
 public:
  EkaIgmp(EkaDev* dev);
  ~EkaIgmp();

  int mcJoin(int epmRegion, EkaCoreId coreId, uint32_t ip, uint16_t port, uint16_t vlanTag, uint64_t* pPktCnt);


 public:
  volatile bool        threadActive                = false;
  volatile bool        igmpLoopTerminated          = false;

 private:
  static const int  MAX_LANES            = 8;
  static const int  MAX_ENTRIES_PER_LANE = 64;
  static const int  MAX_IGMP_ENTRIES     = MAX_LANES * MAX_ENTRIES_PER_LANE;
  static const int  MAX_UDP_CHANNELS     = 32;

  static void* igmpThreadLoopCb(void* pEkaIgmp);
  int          igmpThreadLoop();
  int          igmpLeaveAll();

  /* ------------------------------------------------- */

#ifdef _NO_PTHREAD_CB_
  std::thread           igmpThread;
#else
  pthread_t             igmpPthread;
#endif

  EkaIgmpEntry*         igmpEntry[MAX_IGMP_ENTRIES] = {};
  int                   numIgmpEntries              = 0;
  int                   numIgmpEntriesAtCh[MAX_UDP_CHANNELS] = {};
  int                   numIgmpEntriesAtCore[MAX_LANES] = {};
  std::mutex            createEntryMtx;

  EkaDev*               dev                         = NULL;
};

#endif

