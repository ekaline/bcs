#ifndef _EPM_CORE_H_
#define _EPM_CORE_H_

#include "eka_macros.h"

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "EkaDev.h"
#include "EkaEpm.h"

class EkaUdpChannel;
class EpmStrategy;

class EpmCore {
 public:
  EpmCore(EkaEpm* parent, EkaCoreId _coreId);
  EkaOpResult setAction(epm_strategyid_t strategy,
			epm_actionid_t actionIdx, 
			const EpmAction *epmAction);
  
  EkaOpResult getAction(epm_strategyid_t strategy,
			epm_actionid_t actionIdx, 
			EpmAction *epmAction);
  
  EkaOpResult enableController(bool enable);

  EkaOpResult initStrategies(const EpmStrategyParams *params,
			     epm_strategyid_t numStrategies);

  EkaOpResult setStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t enable);

  EkaOpResult getStrategyEnableBits(epm_strategyid_t strategy,
				    epm_enablebits_t *enable);

  EkaOpResult raiseTriggers(const EpmTrigger *trigger);

  EkaOpResult payloadHeapCopy(epm_strategyid_t strategy, 
			      uint32_t offset,
			      uint32_t length, 
			      const void *contents);
  bool alreadyJoined(epm_strategyid_t prevStrats,uint32_t ip, uint16_t port);

  int joinMc(uint32_t ip, uint16_t port, int16_t vlanTag);

 private:
  void swEpmProcessor();
  int  openUdpChannel();

  bool validStrategyIdx(epm_strategyid_t strategyIdx) {
    return strategyIdx < stratNum || strategy[strategyIdx] != NULL;
  }

  epm_strategyid_t  stratNum = 0;
  EpmStrategy*      strategy[EkaEpm::MaxStrategies] = {};

  EpmAction*        action[EkaEpm::MaxActions]      = {};
  char              heap[EkaEpm::PayloadMemorySize] = {};
  
  bool              controllerEnabled = false;
  bool              initialized = false;

  EkaCoreId         coreId;
  EkaEpm*           parent;

  std::thread       swProcessor;


 public:
  uint16_t          vlanTag;
  EkaDev*           dev   = NULL;
  EkaUdpChannel*    epmCh = NULL;

  
};


#endif
