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

class EkaHwHashTableLine;
class EkaIgmp;
class EkaUdpSess;
class EkaP4Strategy;
class EkaEpm;

class EkaEfc {
public:
  EkaEfc(const EfcInitCtx *pEfcInitCtx);
  ~EkaEfc();
  int downloadTable();
  int subscribeSec(uint64_t secId);
  int cleanSubscrHwTable();
  EfcSecCtxHandle getSubscriptionId(uint64_t secId);
  int initStratGlobalParams(
      const EfcStratGlobCtx *efcStratGlobCtx);
  int armController(EfcArmVer ver);
  int disArmController();
  int run(EfcCtx *pEfcCtx, const EfcRunCtx *pEfcRunCtx);

  void initP4(const EfcStrategyParams *stratParams,
              const EfcP4Params *p4Params);

  void armP4(EfcArmVer ver);
  void disarmP4();

  bool isReportOnly() { return stratGlobCtx.report_only; }

private:
  EkaUdpSess *findUdpSess(EkaCoreId coreId, uint32_t mcAddr,
                          uint16_t mcPort);
  int setHwGlobalParams();
  int setHwUdpParams();
  int enableRxFire();
  int disableRxFire();
  int checkSanity();

  EkaDev *dev_ = nullptr;
  EkaEpm *epm_ = nullptr;

  /* -----------------------------------------------------
   */
  static const int MAX_UDP_SESS = 64;

  EfcStratGlobCtx stratGlobCtx = {};

public:
  EkaP4Strategy *p4_ = nullptr;
  EfcRunCtx localCopyEfcRunCtx = {};
  bool report_only_ = false;
  uint64_t watchdog_timeout_sec_ = 0;

  uint64_t pktCnt = 0; // for EFH compatibility

  OnReportCb reportCb; ///< Callback function to process
                       ///< fire reports
  void *cbCtx;
};

#endif
