/* SHURIK */

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
class EkaEpmRegion;
class EkaIgmp;
class EkaEpm;

/* ------------------------------------------------ */
static inline uint64_t strategyEnableAddr(epm_strategyid_t  id) {
  return (uint64_t) (0x85000 + id * 8);
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

  
  static const uint MAX_CORES                   = EkaDev::MAX_CORES;
  static const uint MAX_SESS_PER_CORE           = EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID             = EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE     = EkaDev::TOTAL_SESSIONS_PER_CORE;
  static const uint ActionsPerTcpSess           = 2;
  static const uint MAX_ETH_FRAME_SIZE          = EkaDev::MAX_ETH_FRAME_SIZE;
  static const uint TCP_EMPTY_ACK_SIZE          = 64;
  static const uint IGMP_V2_SIZE                = 64;
  static const uint HW_FIRE_MSG_SIZE            = 512;
  static const uint DEFAULT_1K_SIZE             = 1024;

  static const int MAX_UDP_SESS      = 64;


  /* const uint IGMP_ACTIONS_BASE           = MAX_CORES * TOTAL_SESSIONS_PER_CORE * 3; */
  /* const uint MAX_IGMP_ACTIONS            = EkaDev::MAX_UDP_CHANNELS; */

  static const uint64_t ALWAYS_ENABLE           = 0xFFFFFFFFFFFFFFFF;
  static const uint64_t DefaultToken            = 0x1122334455667788;

  static const int EPM_REGIONS                  = 32;
  static const uint MAX_HEAP_WR_THREADS         = 16;
  static const uint MaxActions                  = 8 * 1024;
  static const uint MaxActionsPerStrategy       = 256;
  static const int  MaxStrategies               = 4;

  static const uint64_t EpmActionBase           = 0x89000;
  static const uint     ActionBudget            = 64;
  static const uint64_t MaxHeap                 = 8 * 1024 * 1024;
  static const uint64_t HeapPage                = 4 * 1024;

  static const uint64_t MaxUserHeap             = 6 * 1024 * 1024;
  static const uint64_t MaxServiceHeap          = MaxHeap - MaxUserHeap;

  static const uint     HeapPerEfcAction        = 1536;
  static const uint     HeapPerRegion           = MaxHeap    / EPM_REGIONS;
  static const uint     ActionsPerRegion        = MaxActions / EPM_REGIONS;

	static const uint64_t PayloadMemorySize       = MaxUserHeap;
  static const uint64_t TcpDatagramOffset       = sizeof(EkaEthHdr)+sizeof(EkaIpHdr)+sizeof(EkaTcpHdr);
  static const uint64_t UdpDatagramOffset       = sizeof(EkaEthHdr)+sizeof(EkaIpHdr)+sizeof(EkaUdpHdr);
  static const uint64_t PayloadAlignment        = 32;
  static const uint64_t RequiredTailPadding     = 0;

  // static const uint8_t  UserRegion              = 0;
  // static const uint8_t  EfcRegion               = 0;
  // static const uint8_t  TcpTxRegion             = MaxStrategies;   // 4
  // static const uint8_t  EpmMcRegion             = TcpTxRegion + 1; // 5
  // static const int      ReservedRegions         = EpmMcRegion + 1; // 6
  // static const int      MaxUdpChannelRegions    = EPM_REGIONS - ReservedRegions;

  static const uint     UserActionsBaseIdx      = 0;
  static const uint64_t MaxUserActions          = MaxStrategies * MaxActionsPerStrategy;
  static const uint64_t MaxServiceActions       = MaxActions - MaxUserActions;

	static const uint     P4ReservedActions       = 64;
	static const uint     EfcAllocatableBase      = P4ReservedActions;
	static const uint     MaxEfcActions           = 256;
	
  static const uint64_t UserHeapBaseAddr        = EpmHeapHwBaseAddr;
  static const uint64_t UserActionBaseAddr      = EpmActionBase;

  static const uint     ServiceActionsBaseIdx   = UserActionsBaseIdx + MaxUserActions;
  static const uint64_t ServiceHeapBaseAddr     = UserHeapBaseAddr   + MaxUserHeap;
  static const uint64_t ServiceActionBaseAddr   = UserActionBaseAddr + MaxUserActions * ActionBudget;

  static const uint     UserHeapBaseOffs        = 0;
  static const uint     ServiceHeapBaseOffs     = MaxUserHeap;

  using ActionType = EpmActionType;

  EkaEpm(EkaDev* _dev);

  void initHeap(uint start, uint size, uint regionId);

  int createRegion(int regionId);

  uint64_t getPayloadMemorySize () {
    return PayloadMemorySize;
  }

  uint64_t getPayloadAlignment () {
    return PayloadAlignment;
  }

  uint64_t getDatagramOffset () {
    return TcpDatagramOffset;
  }

  uint64_t getUdpDatagramOffset () {
    return UdpDatagramOffset;
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

  EkaOpResult setAction(epm_strategyid_t strategy,
			epm_actionid_t action,
			const EpmAction *epmAction);


  EkaOpResult getAction(epm_strategyid_t strategy,
			epm_actionid_t action,
			EpmAction *epmAction);

  EkaOpResult enableController(EkaCoreId coreId, bool enable);

  EkaOpResult initStrategies(const EpmStrategyParams *params,
			     epm_strategyid_t numStrategies);

  EkaOpResult setStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t enable);
  EkaOpResult getStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t *enable);
  
  EkaOpResult raiseTriggers(const EpmTrigger *trigger);

  EkaOpResult payloadHeapCopy(epm_strategyid_t strategy, uint32_t offset,
			      uint32_t length, const void *contents, const bool isUdpDatagram);

  int DownloadSingleTemplate2HW(EpmTemplate* t);

  int DownloadTemplates2HW();
  int InitTemplates();

  EkaEpmAction* addAction(ActionType      type, 
			  int             regionId, 
			  epm_actionid_t  localIdx, 
			  uint8_t         coreId, 
			  uint8_t         sessId, 
			  uint8_t         auxIdx);

 private:
  void actionParamsSanityCheck(ActionType type, 
			       int        regionId, 
			       uint8_t    _coreId, 
			       uint8_t    _sessId);
  bool alreadyJoined(epm_strategyid_t prevStrats,uint32_t ip, uint16_t port);
  int joinMc(uint32_t ip, uint16_t port, int16_t vlanTag);

  bool validStrategyIdx(epm_strategyid_t strategyIdx) {
    return (strategyIdx < stratNum) && (strategy[strategyIdx] != NULL);
  }

  /* ---------------------------------------------------------- */

 public:
  static const uint EpmNumHwFields   = 16;
  static const uint EpmHwFieldSize   = 16;
  static const uint EpmMaxRawTcpSize = EkaDev::MAX_ETH_FRAME_SIZE;

  volatile bool active = false;
  EkaDev* dev = NULL;

  EpmTemplate*      tcpFastPathPkt          = NULL;
  EpmTemplate*      rawPkt                  = NULL;
  EpmTemplate*      hwFire                  = NULL;
  EpmTemplate*      hwCancel                = NULL;
  EpmTemplate*      swFire                  = NULL;
  EpmTemplate*      cmeILink                = NULL;
  EpmTemplate*      cmeHb                   = NULL;
  
  uint              templatesNum            = 0;

  bool              initialized             = false;

  EkaUdpChannel*    udpCh[MAX_CORES]        = {};

  EkaIgmp*          ekaIgmp                 = NULL;

  uint8_t           heap[EkaEpm::MaxHeap]   = {};

  EpmStrategy*      strategy[MaxStrategies] = {};
  epm_strategyid_t  stratNum = 0;

//  EkaEpmAction*     serviceAction[MaxServiceActions]      = {};

//  EkaEpmAction*     userAction[MaxUserActions]      = {};
  
  bool              controllerEnabled = false;

  uint64_t          pktCnt = 0; // for EFH compatibility

  EkaEpmRegion*    epmRegion[EPM_REGIONS] = {};

 private:
  std::mutex   createActionMtx;

  uint     userActionIdx  = UserActionsBaseIdx;
  uint64_t userHeapAddr   = UserHeapBaseAddr;
  uint64_t userActionAddr = UserActionBaseAddr;

  uint     serviceActionIdx  = ServiceActionsBaseIdx;
  uint64_t serviceHeapAddr   = ServiceHeapBaseAddr;
  uint64_t serviceActionAddr = ServiceActionBaseAddr;

  uint userHeapOffs          = UserHeapBaseOffs;
  uint serviceHeapOffs       = ServiceHeapBaseOffs;

 protected:

};

/* ------------------------------------------------ */

#endif

