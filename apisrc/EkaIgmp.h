#ifndef _EKA_IGMP_H_
#define _EKA_IGMP_H_

#include <thread>
#include <pthread.h>

class EkaIgmpEntry;
class EkaDev;

class EkaIgmp {
 public:
  EkaIgmp(EkaDev* dev,/* EkaUdpChannel* udpCh, */  uint8_t coreId, uint epmRegion, const char* name);
  ~EkaIgmp();

  int mcJoin(uint32_t ip, uint16_t port, uint16_t vlanTag);


 private:
  static const int  MAX_IGMP_ENTRIES = 64;

  static void* igmpThreadLoopCb(void* pEkaIgmp);
  int          igmpThreadLoop();
  int          igmpLeaveAll();

  /* ------------------------------------------------- */

  char                  name[256]                   = {};
  uint                  epmRegion                   = -1;

#ifdef _NO_PTHREAD_CB_
  std::thread           igmpThread;
#else
  pthread_t             igmpPthread;
#endif
  bool                  threadActive                = false;
  bool                  igmpLoopTerminated          = false;

  EkaIgmpEntry*         igmpEntry[MAX_IGMP_ENTRIES] = {};
  int                   numIgmpEntries              = 0;

  uint8_t               coreId                      = -1;
  EkaDev*               dev                         = NULL;
};

#endif

