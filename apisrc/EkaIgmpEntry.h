#ifndef _EKA_IGMP_ENTRY_H_
#define _EKA_IGMP_ENTRY_H_

#include "eka_macros.h"

#include "Eka.h"

class EkaDev;
class EkaEpmAction;

class EkaIgmpEntry {
 public:
  EkaIgmpEntry(EkaDev* _dev, int _udpChId, EkaCoreId _coreId, 
	       int _perChId, uint32_t _ip, uint16_t _port, 
	       int16_t _vlanTag, uint64_t* pPktCnt);

  bool   isMy(EkaCoreId coreId, uint32_t _ip, uint16_t _port);
  int    sendIgmpJoin();
  int    sendIgmpLeave();

  int       udpChId = -1;
  int       perChId = -1;
  EkaCoreId coreId  = -1;
  uint32_t  ip      = -1;
  uint16_t  port    = -1;
  uint16_t  vlanTag =  0;
  uint64_t* pPktCnt = NULL;

 private:
  EkaDev*  dev     =  NULL;
  bool     noIgmp  = false;

  EkaEpmAction* igmpJoinAction = NULL;
  EkaEpmAction* igmpLeaveAction = NULL;
};

#endif
