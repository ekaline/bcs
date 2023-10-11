/* SHURIK */

#ifndef _EKA_EPM_H_
#define _EKA_EPM_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <iterator>
#include <thread>

#include <chrono>
#include <sys/time.h>

#include "eka_macros.h"

#include "Efc.h"
#include "Eka.h"
#include "Epm.h"
#include "Exc.h"

#include "EkaDev.h"
#include "EkaEpmRegion.h"

#include "EkaFileLock.h"

class EkaEpmAction;
class EkaUdpChannel;
class EpmStrategy;
class EkaIgmp;
class EpmTemplate;

/* ------------------------------------------------ */
static inline uint64_t
strategyEnableAddr(epm_strategyid_t id) {
  return (uint64_t)(0x85000 + id * 8);
}

/* ------------------------------------------------ */

class EkaEpm {
public:
  enum class FrameType : int {
    Invalid = 0,
    Tcp,
    Udp,
    Ip,
    Mac
  };

  enum class TemplateId : int {
    TcpFastPath = 0,
    Raw,

    BoeQuoteUpdateShort,
    BoeCancel,

    SqfHwFire,
    SqfSwCancel,

    CmeHwCancel,
    CmeSwFire,
    CmeSwHb,

    QedHwFire,

    Count
  };

  static const uint MAX_CORES = EkaDev::MAX_CORES;
  static const uint MAX_SESS_PER_CORE =
      EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID =
      EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE =
      EkaDev::TOTAL_SESSIONS_PER_CORE;
  static const uint ActionsPerTcpSess = 2;
  static const uint MAX_ETH_FRAME_SIZE =
      EkaDev::MAX_ETH_FRAME_SIZE;
  static const uint TCP_EMPTY_ACK_SIZE = 64;
  static const uint IGMP_V2_SIZE = 64;
  static const uint HW_FIRE_MSG_SIZE = 512;
  static const uint DEFAULT_1K_SIZE = 1024;

  static const int MAX_UDP_SESS = 64;

  static const uint64_t ALWAYS_ENABLE = 0xFFFFFFFFFFFFFFFF;
  static const uint64_t DefaultToken = 0x1122334455667788;

  static const int EPM_REGIONS = 32;
  static const uint MAX_HEAP_WR_THREADS = 16;

  // static const uint64_t EpmActionBase = 0x89000;
  static const uint ActionBudget = 64;
  static const uint64_t MaxHeap = 8 * 1024 * 1024;
  static const uint64_t HeapPage = 4 * 1024;

  static const uint64_t TcpDatagramOffset =
      sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
      sizeof(EkaTcpHdr);
  static const uint64_t UdpDatagramOffset =
      sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
      sizeof(EkaUdpHdr);
  static const uint64_t PayloadAlignment = 32;
  static const uint64_t RequiredTailPadding = 0;

  static const size_t TotalEpmActions =
      EkaEpmRegion::getTotalActions();

  using ActionType = EpmActionType;

  EkaEpm(EkaDev *_dev);
  ~EkaEpm();

  void initHeap(int regionId, uint8_t payloadByte = 0);

  int createRegion(int regionId);

  uint64_t getPayloadAlignment() {
    return PayloadAlignment;
  }

  uint64_t getDatagramOffset() { return TcpDatagramOffset; }

  uint64_t getUdpDatagramOffset() {
    return UdpDatagramOffset;
  }

  uint64_t getRequiredTailPadding() {
    return RequiredTailPadding;
  }

  uint64_t getMaxStrategies() {
    return EkaEpmRegion::MaxStrategies;
  }

  void DownloadSingleTemplate2HW(EpmTemplate *t);

  void InitDefaultTemplates();

  EkaEpmAction *addAction(ActionType type,
                          epm_actionid_t localIdx,
                          int regionId);

  epm_actionid_t allocateAction(EpmActionType actionType);
  void deleteAction(EpmActionType actionType,
                    epm_actionid_t localIdx, int regionId);

private:
  void actionParamsSanityCheck(ActionType type,
                               int regionId);
  bool alreadyJoined(epm_strategyid_t prevStrats,
                     uint32_t ip, uint16_t port);
  int joinMc(uint32_t ip, uint16_t port, int16_t vlanTag);

  bool validStrategyIdx(epm_strategyid_t strategyIdx) {
    return (strategyIdx < stratNum) &&
           (strategy[strategyIdx] != NULL);
  }

  /* ----------------------------------------------- */

public:
  static const uint EpmNumHwFields = 16;
  static const uint EpmHwFieldSize = 16;
  static const uint EpmMaxRawTcpSize =
      EkaDev::MAX_ETH_FRAME_SIZE;

  volatile bool active_ = false;
  EkaDev *dev = NULL;
  /*
    EpmTemplate *tcpFastPathPkt = NULL;
    EpmTemplate *rawPkt = NULL;
    EpmTemplate *hwFire = NULL;
    EpmTemplate *hwCancel = NULL;
    EpmTemplate *swFire = NULL;
    EpmTemplate *cmeILink = NULL;
    EpmTemplate *cmeHb = NULL;
   */

  uint templatesNum = 0;

  bool initialized = false;

  EkaUdpChannel *udpCh[MAX_CORES] = {};

  EkaIgmp *ekaIgmp = NULL;

  uint8_t heap[EkaEpm::MaxHeap] = {};

  EpmStrategy *strategy[EkaEpmRegion::MaxStrategies] = {};
  epm_strategyid_t stratNum = 0;

  bool controllerEnabled = false;

  uint64_t pktCnt = 0; // for EFH compatibility

  EkaEpmRegion *epmRegion[EPM_REGIONS] = {};

  std::mutex createActionMtx;
  std::mutex allocateActionMtx;

  EpmTemplate *epmTemplate[(int)TemplateId::Count] = {};

  EkaEpmAction *a_[TotalEpmActions] = {};

  EkaFileLock *writeAction2FpgaMtx_;

private:
  bool actionOccupied_[TotalEpmActions] = {};
  size_t nActions_ = EkaEpmRegion::P4Reserved;

protected:
};

/* ------------------------------------------------ */

#endif
