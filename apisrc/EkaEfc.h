#ifndef _EKA_EFC_H_
#define _EKA_EFC_H_

#include "Efc.h"
#include "Efh.h"
#include "Epm.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaEfcDataStructs.h"
#include "EkaP4Strategy.h"
#include "EpmStrategy.h"

class EkaUdpSess;
class EkaP4Strategy;
class EkaQedStrategy;
class EkaCmeFcStrategy;
class EkaBcCmeStrategy;
class EkaEurStrategy;

class EkaEpm;
class EkaUserReportQ;

class EkaEfc {
public:
  EkaEfc(const EfcInitCtx *pEfcInitCtx);
  ~EkaEfc();
  int downloadTable();
  int subscribeSec(uint64_t secId);
  int cleanSubscrHwTable();
  EfcSecCtxHandle getSubscriptionId(uint64_t secId);
  int armController(EfcArmVer ver);
  int disArmController();
  int run(const EfcRunCtx *pEfcRunCtx);

  /* --------------------------------------------------- */

  void initP4(const EfcUdpMcParams *mcParams,
              const EfcP4Params *p4Params);

  void armP4(EfcArmVer ver);
  void disarmP4();
  /* --------------------------------------------------- */

  void initQed(const EfcUdpMcParams *mcParams,
               const EfcQedParams *p4Params);
  void qedSetFireAction(epm_actionid_t fireActionId,
                        int productId);
  void armQed(EfcArmVer ver);
  void disarmQed();
  /* --------------------------------------------------- */

  void initCmeFc(const EfcUdpMcParams *mcParams,
                 const EfcCmeFcParams *cmeParams);
  void cmeFcSetFireAction(epm_actionid_t fireActionId);
  void armCmeFc(EfcArmVer ver);
  void disarmCmeFc();
  /* --------------------------------------------------- */

  void initBcCmeFc(const EfcUdpMcParams *mcParams,
                   const EkaBcCmeFcAlgoParams *cmeParams);
  void bcCmeFcSetFireAction(epm_actionid_t fireActionId);
  void armBcCmeFc(EfcArmVer ver);
  void disarmBcCmeFc();
  /* --------------------------------------------------- */
  void initEur(const EfcUdpMcParams *mcParams);
  void armEur(EkaBcSecHandle prodHande, bool armBid,
              bool armAsk, EkaBcArmVer ver);
  /* --------------------------------------------------- */

  bool isReportOnly() { return report_only_; }

public:
  int enableRxFire();
  int setHwUdpParams();

private:
  EkaUdpSess *findUdpSess(EkaCoreId coreId, uint32_t mcAddr,
                          uint16_t mcPort);
  // int setHwGlobalParams();
  int disableRxFire();
  int checkSanity();

  EkaDev *dev_ = nullptr;
  EkaEpm *epm_ = nullptr;

  /* --------------------------------------------------- */
  static const int MAX_UDP_SESS = 64;

  uint8_t totalCoreIdBitmap_ = 0x00;

public:
  static const int HwUdpMcConfig =
      0xf0500; // base, every core: + 8

  EkaP4Strategy *p4_ = nullptr;
  EkaQedStrategy *qed_ = nullptr;
  EkaCmeFcStrategy *cme_ = nullptr;

  EkaEurStrategy *eur_ = nullptr;
  EkaBcCmeStrategy *bcCme_ = nullptr;

  EfcRunCtx localCopyEfcRunCtx = {};
  bool report_only_ = false;
  uint64_t watchdog_timeout_sec_ = 0;

  uint64_t pktCnt = 0; // for EFH compatibility

  EkaUserReportQ *userReportQ = NULL;

  OnReportCb reportCb; ///< Callback function to process
                       ///< fire reports
  void *cbCtx;
};

#endif
