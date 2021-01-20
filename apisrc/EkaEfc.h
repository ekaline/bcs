#ifndef _EKA_EFC_H_
#define _EKA_EFC_H_

#include "Efh.h"
#include "Efc.h"
#include "eka_macros.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"

class EkaHwHashTableLine;
class EkaIgmp;
class EkaUdpSess;
class EkaEpmAction;

class EkaEfc {
 public:
  EkaEfc(EkaDev* dev, EfhFeedVer hwFeedVer, const EfcInitCtx* pEfcInitCtx);

  int downloadTable();
  int subscribeSec(uint64_t secId);
  int cleanSubscrHwTable();
  int getSubscriptionId(uint64_t secId);
  int initStrategy(const EfcStratGlobCtx* efcStratGlobCtx);
  int armController();
  int disArmController();
  int run(EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx);
  int createFireAction(uint8_t group, ExcConnHandle hConn);
  int setActionPayload(ExcConnHandle hConn,const void* fireMsg, size_t fireMsgSize);

 private:
  bool          isValidSecId(uint64_t secId);
  int           getMcParams(const EfcInitCtx* pEfcInitCtx);
  int           confParse(const char *key, const char *value);
  int           initHwRoundTable();
  int           normalizeId(uint64_t secId);
  int           getLineIdx(uint64_t normSecId);
  EkaUdpSess*   findUdpSess(uint32_t mcAddr, uint16_t mcPort);
  int           setHwGlobalParams();
  int           setHwUdpParams();
  int           igmpJoinAll();
  EkaEpmAction* findFireAction(ExcConnHandle hConn);
  int           setHwStratRegion();
  int           enableRxFire();

  /* ----------------------------------------------------- */
  static const int MAX_UDP_SESS      = 64;
  static const int MAX_TCP_SESS      = 64;
  static const int MAX_FIRE_ACTIONS  = 64;
  static const int MAX_CTX_THREADS   = 16;

  EkaIgmp*            ekaIgmp    = NULL;
  uint8_t             mdCoreId   = -1;
  uint8_t             fireCoreId = -1;
  EkaDev*             dev        = NULL;
  EfhFeedVer          hwFeedVer  = EfhFeedVer::kInvalid;
  EkaHwHashTableLine* hashLine[EKA_SUBSCR_TABLE_ROWS] = {};
  int                 numSecurities = 0;

  EfcStratGlobCtx     stratGlobCtx = {};

  EkaUdpSess*         udpSess[MAX_UDP_SESS] = {};
  int                 numUdpSess = 0;

  EkaEpmAction*       fireAction[MAX_FIRE_ACTIONS] = {};
  int                 numFireActions = 0;

 public:
  int                 ctxWriteBank[MAX_CTX_THREADS] = {};
  EfcCtx              localCopyEfcCtx = {};
  EfcRunCtx           localCopyEfcRunCtx = {};
};

#endif
