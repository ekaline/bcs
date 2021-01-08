#ifndef _EKA_IGMP_H_
#define _EKA_IGMP_H_

#include <thread>

class EkaIgmpEntry;
class EkaDev;
//class EkaUdpChannel;

class EkaIgmp {
 public:
  EkaIgmp(EkaDev* dev,/* EkaUdpChannel* udpCh, */  uint8_t coreId, uint epmRegion, const char* name);
  ~EkaIgmp();

  int mcJoin(uint32_t ip, uint16_t port, uint16_t vlanTag);


 private:
  static const int  MAX_IGMP_ENTRIES = 64;


  int igmpThreadLoop();
  int igmpLeaveAll();

  //  EkaUdpChannel*        udpCh                       = NULL;
  char                  name[256]                   = {};
  uint                  epmRegion                   = -1;

  std::thread           igmpThread;
  //pthread_t             igmpThread;
  bool                  threadActive                = false;
  bool                  igmpLoopTerminated          = false;

  EkaIgmpEntry*         igmpEntry[MAX_IGMP_ENTRIES] = {};
  int                   numIgmpEntries              = 0;

  uint8_t               coreId                      = -1;
  EkaDev*               dev                         = NULL;
};

#endif

