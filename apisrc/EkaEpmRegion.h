#ifndef _EKA_EPM_REGION_H_
#define _EKA_EPM_REGION_H_

#include "EkaDev.h"
#include "EkaEpm.h"

class EkaEpmRegion {
public:
  struct RegionConfig {
    const char *name;
    size_t nActions;
    size_t actionHeapBudget;
    int wcChId;
  };

  enum Regions : int {
    Efc = 0,
    TcpTxFullPkt = 5,
    TcpTxEmptyAck = 6,
    EfcMc = 7,
    Total = 32
  };

  static const size_t HeapPerRegularAction =
      EkaDev::MAX_ETH_FRAME_SIZE;
  static const size_t HeapPerSmallAction =
      64; // IGMP, TCP ACK
  static const size_t HeapPerAverageTcpAction =
      (HeapPerRegularAction + HeapPerSmallAction) /
      2; // = 800
  static const size_t NumEfcActions = 2048;
  static const size_t NumNewsActions = 256;
  static const size_t NumIgmpActions = 2 * 64;
  static const size_t NumEfcIgmpActions =
      256; // jsust a num

  static const size_t NumTcpActions =
      EKA_MAX_CORES * EKA_MAX_TCP_SESSIONS_PER_CORE *
      2; // 4 * 32 * 2 = 256

  constexpr static RegionConfig region[Regions::Total] = {
      [Regions::Efc] = {"EFC", NumEfcActions,
                        HeapPerRegularAction, 0},
      [1] = {"News", NumNewsActions, HeapPerRegularAction,
             0},
      [2] = {"News", NumNewsActions, HeapPerRegularAction,
             0},
      [3] = {"News", NumNewsActions, HeapPerRegularAction,
             0},
      [4] = {"News", NumNewsActions, HeapPerRegularAction,
             0},
      [Regions::TcpTxFullPkt] = {"TcpTxFullPkt",
                                 NumTcpActions,
                                 HeapPerRegularAction, 0},
      [Regions::TcpTxEmptyAck] = {"TcpTxEmptyAck",
                                  NumTcpActions,
                                  HeapPerSmallAction, 0},
      [Regions::EfcMc] = {"EfcMc", NumEfcIgmpActions,
                          HeapPerSmallAction, 0},
      [8] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
             1},
      [9] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
             2},
      [10] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              3},
      [11] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              4},
      [12] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              5},
      [13] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              6},
      [14] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              7},
      [15] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              8},
      [16] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              9},
      [17] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              10},
      [18] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              11},
      [19] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              12},
      [20] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              13},
      [21] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              14},
      [22] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              15},
      [23] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              1},
      [24] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              2},
      [25] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              3},
      [26] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              4},
      [27] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              5},
      [28] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              6},
      [29] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              7},
      [30] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              8},
      [31] = {"EfhIgmp", NumIgmpActions, HeapPerSmallAction,
              9}};

  constexpr static void printConfig(void *buf,
                                    size_t bufLen) {
    auto c = (char *)buf;
    for (auto i = 0; i < Regions::Total; i++) {
      c += sprintf(
          c,
          "%2d %-14s: %4jd Actions: %4d..%4jd, "
          "%7jdB Heap: %7d..%7jd\n",
          i, region[i].name, region[i].nActions,
          getBaseActionIdx(i),
          getBaseActionIdx(i) + region[i].nActions - 1,
          region[i].nActions * region[i].actionHeapBudget,
          getBaseHeapOffs(i),
          getBaseHeapOffs(i) +
              region[i].nActions *
                  region[i].actionHeapBudget -
              1);
      if (c - (char *)buf > (int)bufLen)
        on_error("string len %jd > %d", c - (char *)buf,
                 (int)bufLen);
    }
  }

  constexpr static void sanityCheckRegionId(int regionId) {
    if (regionId < 0 || regionId >= Regions::Total)
      on_error("Bad regionId %d", regionId);
  }

  constexpr static void sanityCheckActionId(int regionId,
                                            int actionId) {
    if (actionId < 0 ||
        actionId >= getBaseActionIdx(regionId) +
                        getMaxActions(regionId))
      on_error("Bad actionId %d at region %d: "
               "getBaseActionIdx + getMaxActions = %d",
               actionId, regionId,
               getBaseActionIdx(regionId) +
                   getMaxActions(regionId));
  }

  constexpr static int getBaseActionIdx(int regionId) {
    sanityCheckRegionId(regionId);
    int baseIdx = 0;
    for (auto i = 0; i < regionId; i++) {
      baseIdx += region[i].nActions;
    }
    return baseIdx;
  }

  constexpr static int getMaxActions(int regionId) {
    sanityCheckRegionId(regionId);
    return region[regionId].nActions;
  }

  constexpr static int getBaseHeapOffs(int regionId) {
    sanityCheckRegionId(regionId);
    int baseHeapOffs = 0;
    for (auto i = 0; i < regionId; i++) {
      baseHeapOffs +=
          region[i].nActions * region[i].actionHeapBudget;
    }
    return baseHeapOffs;
  }

  static int geWcChByOffs(int heapOffs) {
    for (auto i = 0; i < Regions::Total; i++) {
      if (heapOffs >= getBaseHeapOffs(i) &&
          heapOffs < getBaseHeapOffs(i) + getHeapSize(i))
        return region[i].wcChId;
    }
    on_error("heapOffs %d (0x%x) is out of Heap %d (0x%x)",
             heapOffs, heapOffs,
             getBaseHeapOffs(Regions::Total - 1) +
                 getHeapSize(Regions::Total - 1),
             getBaseHeapOffs(Regions::Total - 1) +
                 getHeapSize(Regions::Total - 1));
  }

  constexpr static uint getHeapSize(int regionId) {
    sanityCheckRegionId(regionId);

    return region[regionId].nActions *
           region[regionId].actionHeapBudget;
  }

  constexpr static int getActionHeapOffs(int regionId,
                                         int actionId) {
    sanityCheckRegionId(regionId);
    sanityCheckActionId(regionId, actionId);

    return getBaseHeapOffs(regionId) +
           actionId * region[regionId].actionHeapBudget;
  }

  constexpr static int getEfhIgmpRegion(int udpChId) {
    auto Reserved = Regions::EfcMc + 1;
    if (udpChId >= Regions::Total - Reserved)
      on_error("udpChId %d exceeds MaxUdpChannelRegions %d",
               udpChId, Regions::Total - Reserved);

    return udpChId + Reserved;
  }

  // ######################################################
  EkaEpmRegion(EkaDev *_dev, int _id) {
    dev = _dev;
    id = _id;

    baseActionIdx = getBaseActionIdx(id);
    localActionIdx = 0;

    baseHeapOffs = getBaseHeapOffs(id);
    //    heapOffs       = baseHeapOffs;

    // writing region's baseActionIdx to FPGA
    eka_write(dev, 0x82000 + 8 * id, baseActionIdx);

    EKA_LOG("Created EpmRegion %s %u: "
            "baseActionIdx=%u, baseHeapOffs=%x",
            region[id].name, id, baseActionIdx,
            baseHeapOffs);
  }

  EkaDev *dev = NULL;
  int id = -1;

  epm_actionid_t baseActionIdx = -1;
  epm_actionid_t localActionIdx = 0;

  uint baseHeapOffs = 0;
  //  uint      heapOffs       = 0;

private:
};

#endif
