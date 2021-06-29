#ifndef _EKA_FH_RUN_GROUP_H
#define _EKA_FH_RUN_GROUP_H

#include "EkaDev.h"
#include "EkaFh.h"

class EkaUdpChannel;
class EkaIgmp;

class EkaFhRunGroup {
 public:
  inline bool isMyGr(uint8_t _gr) {
    for (auto i = 0; i < numGr; i++) 
      if (groupList[i] == _gr) return true;

    return false;
  }

  EkaFhRunGroup(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runId);
  ~EkaFhRunGroup();
  uint getGrAfterGap();
  void setGrAfterGap(uint i);
  void clearGrAfterGap(uint i);

  bool drainQ(const EfhRunCtx* pEfhRunCtx);

  int igmpMcJoin(uint32_t ip, uint16_t port, uint16_t vlanTag, uint64_t* pPktCnt);
  int checkTimeOut(const EfhRunCtx* pEfhRunCtx);
  int sendFeedCloseAll(const EfhRunCtx* pEfhRunCtx);
 private:
  int igmpThreadLoop();
  int igmpLeaveAll();
  bool igmpSanityCheck(int grId2check, uint32_t ip, uint16_t port);

 public:
  static const uint MAX_GR2RUN = 64;
  static const int  MAX_IGMP_ENTRIES = 64;

  uint8_t               groupList[MAX_GR2RUN] = {};
  uint8_t               numGr             = 0; // total MC groups belonging to this RunGr

  char                  list2print[300]   = {};

  bool                  grAfterGap[MAX_GR2RUN] = {};
  uint                  cntGrAfterGap     = 0;
  bool                  hasGrpAfterGap    = false;

  uint8_t               coreId            = -1;

  uint8_t               runId             = -1;
  EkaUdpChannel*        udpCh             = NULL;
  int                   udpChId           = -1;
  int                   epmRegion         = -1;

  EkaSource             exch              = EkaSource::kInvalid;
  EkaFh*                fh                = NULL;

  bool                  thread_active     = false;
  bool                  stoppedByExchange = false;

 private:
  volatile bool         allGroupsClosed   = false;
  uint64_t              timeOutCntr       = 0;
  EkaDev*               dev               = NULL;

};

#endif
