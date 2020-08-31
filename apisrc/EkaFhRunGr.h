#ifndef _EKA_FH_RUN_GROUP_H
#define _EKA_FH_RUN_GROUP_H

class EkaUdpChannel;
class EkaDev;
class EkaFh;

class FhRunGr {
 public:
  inline bool isMyGr(uint8_t _gr) {
    for (auto i = 0; i < numGr; i++) 
      if (groupList[i] == _gr) return true;

    return false;
  }

  FhRunGr(EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx, uint8_t runId);
  uint getGrAfterGap();
  void setGrAfterGap(uint i);
  void clearGrAfterGap(uint i);

  //  template <class FhGroup> bool drainQ(const EfhRunCtx* pEfhRunCtx);
  bool drainQ(const EfhRunCtx* pEfhRunCtx);


  static const uint MAX_GR2RUN = 64;
  uint8_t               groupList[MAX_GR2RUN] = {};
  char                  list2print[300] = {};

  bool                  grAfterGap[MAX_GR2RUN];
  uint                  cntGrAfterGap;
  bool                  hasGrpAfterGap;

  EkaDev*               dev;
  uint8_t               coreId; // =0

  uint8_t               runId;
  EkaUdpChannel*        udpCh;
  EkaSource             exch;
  EkaFh*                fh;
  //  uint8_t               firstGr; // 1st of the MC groups belonging to this RunGr
  uint8_t               numGr = 0; // total MC groups belonging to this RunGr

  //  volatile bool         thread_active;
  bool                  thread_active;
  bool                  stoppedByExchange;
};

#endif
