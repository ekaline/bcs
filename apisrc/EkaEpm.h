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

class EkaEpm {
 public:
  static const uint MAX_CORES               = EkaDev::MAX_CORES;
  static const uint MAX_SESS_PER_CORE       = EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID         = EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE = EkaDev::TOTAL_SESSIONS_PER_CORE;
  static const uint MAX_PKT_SIZE            = EkaDev::MAX_PKT_SIZE;
  static const uint TCP_EMPTY_ACK_SIZE      = 64;

  static const uint MaxActions              = 4 * 1024;

  static const uint     UserActionsBaseIdx   = 1024;
  static const uint64_t EpmActionBase        = 0x89000;
  static const uint     ActionBudget         = 64;


  static const uint64_t PayloadMemorySize = 8 * 1024;
  static const uint64_t PayloadAlignment = 8;
  static const uint64_t DatagramOffset = 54; // 14+20+20
  static const uint64_t RequiredTailPadding = 0;
  static const uint64_t MaxStrategies = 32;
  static const uint64_t MaxServiceActions = 1024;
  static const uint64_t MaxUserActions = 4 * 1024;

  

  enum class ActionType : int {
    INVALID = 0,
      // Service Actions
      TcpFullPkt,
      TcpFastPath,
      TcpEptyAck,

      // User Actions
      UserAction
  };

  EkaEpm(EkaDev* _dev) {
    dev = _dev;
    EKA_LOG("Created Epm");
  }

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
  
  EkaOpResult raiseTriggers(EkaCoreId coreId,
			    const EpmTrigger *trigger);

  EkaOpResult payloadHeapCopy(EkaCoreId coreId,
			      epm_strategyid_t strategy, uint32_t offset,
			      uint32_t length, const void *contents);

  int DownloadSingleTemplate2HW(EpmTemplate* t);

  int DownloadTemplates2HW();
  int InitTemplates();

  // Service Actions only
  EkaEpmAction* addAction(ActionType type, uint8_t _coreId, uint8_t _sessId, uint8_t _auxIdx);

  // User Actions only
  EkaEpmAction* addUserAction(epm_actionid_t useActionIdx);


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

 private:
  epm_strategyid_t  stratNum = 0;
  EpmStrategy*      strategy[MaxStrategies] = {};

  EkaEpmAction*     serviceAction[MaxServiceActions]      = {};

  EkaEpmAction*     userAction[MaxUserActions]      = {};

  char              heap[EkaEpm::PayloadMemorySize] = {};
  
  bool              controllerEnabled = false;
  bool              initialized = false;

  std::mutex   createActionMtx;

  EkaUdpChannel*    udpCh[MAX_CORES] = {};

  uint     serviceActionIdx  = 0;
  uint64_t serviceHeapAddr   = EpmHeapHwBaseAddr;
  uint64_t serviceActionAddr = EpmActionBase;

  uint     userActionIdx  = 0;
  uint64_t userHeapAddr   = EpmHeapHwBaseAddr;
  uint64_t userActionAddr = EpmActionBase;

 protected:

};

#endif

