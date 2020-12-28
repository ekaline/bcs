#ifndef _EKA_IGMP_ENTRY_H_
#define _EKA_IGMP_ENTRY_H_

#include "eka_macros.h"

class EkaDev;
class EkaEpmAction;

class EkaIgmpEntry {
 public:
  EkaIgmpEntry(EkaDev* _dev, int _udpChId, uint32_t _ip, uint16_t _port, int16_t _vlanTag, uint8_t _coreId);

  bool   isMy(uint32_t _ip, uint16_t _port);
  int    sendIgmpJoin() ;
  int    sendIgmpLeave();

 private:
  EkaDev*  dev     =  NULL;
  int      udpChId = -1;
  uint32_t ip      = -1;
  uint16_t port    = -1;
  uint16_t vlanTag =  0;
  uint8_t  coreId  = -1;

  bool     noIgmp  = false;

  EkaEpmAction* igmpJoinAction = NULL;
  EkaEpmAction* igmpLeaveAction = NULL;
};

#endif
