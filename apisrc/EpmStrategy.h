#ifndef _EKA_EPM_STRATEGY_H_
#define _EKA_EPM_STRATEGY_H_

#include "EpmCore.h"

class EpmStrategy {
 public:
  EpmStrategy(EpmCore* parent, epm_strategyid_t id, epm_actionid_t   baseActionIdx, const EpmStrategyParams *params);

  EkaOpResult setEnableBits(epm_enablebits_t  enable);
  EkaOpResult getEnableBits(epm_enablebits_t *enable);

  bool hasSame(uint32_t ip, uint16_t port);
  bool myAction(epm_actionid_t actionId);

  EkaOpResult setAction(epm_actionid_t actionIdx, const EpmAction *epmAction);
  EkaOpResult getAction(epm_actionid_t actionIdx, EpmAction *epmAction);

 private:
  EkaDev*           dev;
  EpmCore*          parent;
  epm_strategyid_t  id;

  epm_enablebits_t  enable;

  EpmAction*       action[EkaEpm::MaxActions]      = {};

  epm_actionid_t   baseActionIdx;
  epm_actionid_t   numActions; ///< No. of actions entries used by this strategy
  const sockaddr  *triggerAddr;///< Address to receive trigger packets
  EpmFireReportCb  reportCb;   ///< Callback function to process fire reports
  void            *cbCtx;

  uint32_t         ip;
  uint16_t         port;
};

#endif
