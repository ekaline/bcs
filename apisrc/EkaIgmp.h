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


 private:
  static const int  MAX_IGMP_ENTRIES = 64;

  static void* igmpThreadLoopCb(void* pEkaIgmp);
  int          igmpThreadLoop();
  int          igmpLeaveAll();

  /* ------------------------------------------------- */

#ifdef _NO_PTHREAD_CB_
  std::thread           igmpThread;
#else
  pthread_t             igmpPthread;
#endif
  bool                  threadActive                = false;
  bool                  igmpLoopTerminated          = false;

  EkaIgmpEntry*         igmpEntry[MAX_IGMP_ENTRIES] = {};
  int                   numIgmpEntries              = 0;

  std::mutex            createEntryMtx;

  EkaDev*               dev                         = NULL;
};

#endif

