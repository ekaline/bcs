#ifndef _EKA_EPM_STRATEGY_H_
#define _EKA_EPM_STRATEGY_H_

#include "EkaEpm.h"

class EkaEpmAction;
class EkaUdpSess;
#if 0
class EpmStrategy {
public:
  EpmStrategy(EkaEpm *epm, epm_strategyid_t id,
              epm_actionid_t baseActionIdx,
              const EpmStrategyParams *params,
              EfhFeedVer hwFeedVer);
  virtual ~EpmStrategy() {}

  EkaOpResult setEnableBits(epm_enablebits_t enable);
  EkaOpResult getEnableBits(epm_enablebits_t *enable);

  //  bool hasSame(uint32_t ip, uint16_t port);
  bool myAction(epm_actionid_t actionId);

  EkaOpResult setAction(epm_actionid_t actionIdx,
                        const EpmAction *epmAction);
  EkaOpResult getAction(epm_actionid_t actionIdx,
                        EpmAction *epmAction);

  // private:
  /* -----------------------------------------------------
   */
public:
  EkaDev *dev;
  EkaEpm *epm;
  epm_strategyid_t id;

  epm_enablebits_t enable;
  int maxActions_ = 0;

  EkaEpmAction
      *action[EkaEpmRegion::getMaxActionsPerRegion()] = {};

  epm_actionid_t baseActionIdx;
  epm_actionid_t numActions; ///< No. of actions entries
                             ///< used by this strategy
  const sockaddr
      *triggerAddr; ///< Address to receive trigger packets
  //  EpmFireReportCb   reportCb;   ///< Callback function
  //  to process fire reports
  OnReportCb reportCb; ///< Callback function to process
                       ///< fire reports
  void *cbCtx;

  EfhFeedVer hwFeedVer = EfhFeedVer::kInvalid;

  EkaUdpSess *udpSess[EkaEpm::MAX_UDP_SESS] = {};
  int numUdpSess = 0;
};
#endif
#endif
