#ifndef _EKA_EFC_H_
#define _EKA_EFC_H_

#include "Efh.h"
#include "Efc.h"
#include "Epm.h"
#include "eka_macros.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"

#include "EpmStrategy.h"

class EkaHwHashTableLine;
class EkaIgmp;
class EkaUdpSess;
class EkaEpmAction;
class EhpProtocol;

class EkaEfc : public EpmStrategy {
 public:
  EkaEfc(EkaEpm*                  epm, 
	       epm_strategyid_t         id, 
	       epm_actionid_t           baseActionIdx, 
	       const EpmStrategyParams* params, 
	       EfhFeedVer               hwFeedVer);
  int downloadTable();
  int subscribeSec(uint64_t secId);
  int cleanSubscrHwTable();
  EfcSecCtxHandle getSubscriptionId(uint64_t secId);
  int initStrategy(const EfcStratGlobCtx* efcStratGlobCtx);
  int armController(EfcArmVer ver);
  int disArmController();
  int run(EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx);

 private:
  bool          isValidSecId(uint64_t secId);
  int           initHwRoundTable();
  int           normalizeId(uint64_t secId);
  int           getLineIdx(uint64_t normSecId);
  EkaUdpSess*   findUdpSess(EkaCoreId coreId, uint32_t mcAddr, uint16_t mcPort);
  int           setHwGlobalParams();
  int           setHwUdpParams();
  int           setHwStratRegion();
  int           enableRxFire();
  int           checkSanity();
  
  /* ----------------------------------------------------- */
  static const int MAX_UDP_SESS      = 64;
  static const int MAX_TCP_SESS      = 64;
  static const int MAX_FIRE_ACTIONS  = 64;
  static const int MAX_CTX_THREADS   = 16;

  EkaHwHashTableLine* hashLine[EFC_SUBSCR_TABLE_ROWS] = {};

  EfcStratGlobCtx     stratGlobCtx = {};

 public:
  int                 numSecurities = 0;
  int                 ctxWriteBank[MAX_CTX_THREADS] = {};
  EfcCtx              localCopyEfcCtx = {};
  EfcRunCtx           localCopyEfcRunCtx = {};

  uint64_t            pktCnt         = 0; // for EFH compatibility

  EhpProtocol*        ehp = NULL;
  uint64_t*           secIdList = NULL; // array of SecIDs, index is handle
};

#endif
