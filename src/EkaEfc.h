#ifndef _EKA_EFC_H_
#define _EKA_EFC_H_

#include "Efc.h"
#include "Efh.h"
#include "Epm.h"
#include "efh_macros.h"
#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaEfcDataStructs.h"
#include "EpmStrategy.h"

class EkaUdpSess;
class EkaMoexStrategy;

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
  void initMoex(const EfcUdpMcParams *mcParams);
  void armMoex(bool arm, EkaBcsArmVer ver);
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

  EkaMoexStrategy *moex_ = nullptr;

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
