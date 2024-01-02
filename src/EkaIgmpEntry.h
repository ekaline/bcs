#ifndef _EKA_IGMP_ENTRY_H_
#define _EKA_IGMP_ENTRY_H_

#include "eka_macros.h"

#include "Eka.h"

class EkaDev;
class EkaEpmAction;

class EkaIgmpEntry {
public:
  EkaIgmpEntry(EkaDev *_dev, int _epmRegion,
               EkaCoreId _coreId, int _perChId,
               uint32_t _ip, uint16_t _port,
               int16_t _vlanTag, uint64_t *pPktCnt);

  bool isMy(EkaCoreId coreId, uint32_t _ip, uint16_t _port);
  int sendIgmpJoin();
  int sendIgmpLeave();
  void saveMcState();

private:
  EkaDev *dev_ = NULL;
  bool noIgmp_ = false;

  int regionId_ = -1;
  int perChId_ = -1;
  EkaCoreId coreId_ = -1;
  uint32_t ip_ = -1;
  uint16_t port_ = -1;
  uint16_t vlanTag_ = 0;
  uint64_t *pPktCnt_ = NULL;

  EkaEpmAction *igmpJoinAction_ = NULL;
  EkaEpmAction *igmpLeaveAction_ = NULL;
};

#endif
