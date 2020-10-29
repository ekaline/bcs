#ifndef _EKA_EPM_H_
#define _EKA_EPM_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <iterator>
#include <thread>

#include <sys/time.h>
#include <chrono>

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "EkaDev.h"


class EpmTemplate;

class EkaEpmAction;
class EkaUdpChannel;
class EpmStrategy;

/* ------------------------------------------------ */

inline void setActionRegionBaseIdx(EkaDev* dev, uint region, uint idx) {
  EKA_LOG("region=%u, baseIdx=%u, addr=0x%jx",region,idx,0x82000 + 8 * region);
  eka_write(dev,0x82000 + 8 * region, idx);
}

/* ------------------------------------------------ */
static inline uint64_t strategyEnableAddr(epm_strategyid_t  id) {
  return (uint64_t) (0x85000 + id * 8);
}
/* ------------------------------------------------ */

class EkaEpm {
 public:
  static const uint MAX_CORES                   = EkaDev::MAX_CORES;
  static const uint MAX_SESS_PER_CORE           = EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID             = EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE     = EkaDev::TOTAL_SESSIONS_PER_CORE;
  static const uint MAX_PKT_SIZE                = EkaDev::MAX_PKT_SIZE;
  static const uint TCP_EMPTY_ACK_SIZE          = 64;

  static const uint64_t ALWAYS_ENABLE           = 0xFFFFFFFFFFFFFFFF;

  static const uint EPM_REGIONS                 = 8;

  static const uint MaxActions                  = 4 * 1024;
  static const uint MaxActionsPerStrategy       = 256;
  static const uint64_t MaxStrategies           = EPM_REGIONS - 1;

  static const uint64_t EpmActionBase           = 0x89000;
  static const uint     ActionBudget            = 64;
  static const uint64_t MaxHeap                 = 4 * 1024 * 1024;

  static const uint64_t MaxUserHeap             = 3 * 1024 * 1024;
  static const uint64_t MaxServiceHeap          = MaxHeap - MaxUserHeap;


  static const uint64_t PayloadMemorySize       = MaxUserHeap;
  static const uint64_t DatagramOffset          = sizeof(EkaEthHdr)+sizeof(EkaIpHdr)+sizeof(EkaTcpHdr);
  static const uint64_t PayloadAlignment        = 32;
  static const uint64_t RequiredTailPadding     = 0;

  static const uint8_t  ServiceRegion           = MaxStrategies;

  static const uint     UserActionsBaseIdx      = 0;
  static const uint64_t MaxUserActions          = MaxStrategies * MaxActionsPerStrategy;
  static const uint64_t MaxServiceActions       = MaxActions - MaxUserActions;

  static const uint64_t UserHeapBaseAddr        = EpmHeapHwBaseAddr;
  static const uint64_t UserActionBaseAddr      = EpmActionBase;

  static const uint     ServiceActionsBaseIdx   = UserActionsBaseIdx + MaxUserActions;
  static const uint64_t ServiceHeapBaseAddr     = UserHeapBaseAddr   + MaxUserHeap;
  static const uint64_t ServiceActionBaseAddr   = UserActionBaseAddr + MaxUserActions * ActionBudget;



  enum class ActionType : int {
    INVALID = 0,
      // Service Actions
      TcpFullPkt,
      TcpFastPath,
      TcpEmptyAck,

      // User Actions
      UserAction
  };


  EkaEpm(EkaDev* _dev);

  uint64_t getPayloadMemorySize () {
    return PayloadMemorySize;
  }

  uint64_t getPayloadAlignment () {
    return PayloadAlignment;
  }

  uint64_t getDatagramOffset () {
    return DatagramOffset;
  }

  uint64_t getRequiredTailPadding () {
    return RequiredTailPadding;
  }

  uint64_t getMaxStrategies () {
    return MaxStrategies;
  }

  uint64_t getMaxActions () {
    return MaxUserActions;
  }

  EkaOpResult setAction(EkaCoreId coreId,
			epm_strategyid_t strategy,
			epm_actionid_t action,
			const EpmAction *epmAction);


  EkaOpResult getAction(EkaCoreId coreId,
			epm_strategyid_t strategy,
			epm_actionid_t action,
			EpmAction *epmAction);

  EkaOpResult enableController(EkaCoreId coreId, bool enable);

  EkaOpResult initStrategies(EkaCoreId coreId,
			     const EpmStrategyParams *params,
			     epm_strategyid_t numStrategies);

  EkaOpResult setStrategyEnableBits(EkaCoreId coreId,
				    epm_strategyid_t strategy,
				    epm_enablebits_t enable);
  EkaOpResult getStrategyEnableBits(EkaCoreId coreId,
				    epm_strategyid_t strategy,
				    epm_enablebits_t *enable);
  
  EkaOpResult raiseTriggers(const EpmTrigger *trigger);

  EkaOpResult payloadHeapCopy(EkaCoreId coreId,
			      epm_strategyid_t strategy, uint32_t offset,
			      uint32_t length, const void *contents);

  int DownloadSingleTemplate2HW(EpmTemplate* t);

  int DownloadTemplates2HW();
  int InitTemplates();

  EkaEpmAction* addAction(ActionType type, 
			  epm_strategyid_t actionRegion, 
			  epm_actionid_t localIdx, 
			  uint8_t _coreId, 
			  uint8_t _sessId, 
			  uint8_t _auxIdx);

 private:
  bool alreadyJoined(epm_strategyid_t prevStrats,uint32_t ip, uint16_t port);
  int joinMc(uint32_t ip, uint16_t port, int16_t vlanTag);

  bool validStrategyIdx(epm_strategyid_t strategyIdx) {
    return (strategyIdx < stratNum) && (strategy[strategyIdx] != NULL);
  }

  /* ---------------------------------------------------------- */

 public:
  static const uint EpmNumHwFields   = 16;
  static const uint EpmHwFieldSize   = 16;
  static const uint EpmMaxRawTcpSize = EkaDev::MAX_PKT_SIZE;

  volatile bool active = false;
  EkaDev* dev = NULL;

  uint         templatesNum = 0;
  EpmTemplate* tcpFastPathPkt;
  EpmTemplate* rawPkt;

  bool              initialized = false;

  EkaUdpChannel*    udpCh[MAX_CORES] = {};

  uint8_t           heap[EkaEpm::MaxHeap] = {};

  EpmStrategy*      strategy[MaxStrategies] = {};
  epm_strategyid_t  stratNum = 0;

//  EkaEpmAction*     serviceAction[MaxServiceActions]      = {};

//  EkaEpmAction*     userAction[MaxUserActions]      = {};
  
  bool              controllerEnabled = false;

 private:
  std::mutex   createActionMtx;

  uint     userActionIdx  = UserActionsBaseIdx;
  uint64_t userHeapAddr   = UserHeapBaseAddr;
  uint64_t userActionAddr = UserActionBaseAddr;

  uint     serviceActionIdx  = ServiceActionsBaseIdx;
  uint64_t serviceHeapAddr   = ServiceHeapBaseAddr;
  uint64_t serviceActionAddr = ServiceActionBaseAddr;


 protected:

};

#endif

