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

class EkaHwHashTableLine;
class EkaIgmp;
class EkaUdpSess;
class EkaEpmAction;
class EhpProtocol;

class EkaEfc : public EpmStrategy {
public:
  EkaEfc(EkaEpm *epm, epm_strategyid_t id,
         epm_actionid_t baseActionIdx,
         const EpmStrategyParams *params,
         EfhFeedVer hwFeedVer);
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

  inline void writeSecHwCtx(const EfcSecCtxHandle handle,
                            const EkaHwSecCtx *pHwSecCtx,
                            uint16_t writeChan) {
    uint64_t ctxWrAddr = P4_CTX_CHANNEL_BASE +
                         writeChan *
                             EKA_BANKS_PER_CTX_THREAD *
                             EKA_WORDS_PER_CTX_BANK * 8 +
                         ctxWriteBank[writeChan] *
                             EKA_WORDS_PER_CTX_BANK * 8;

    // EkaHwSecCtx is 8 Bytes ==> single write
    eka_write(dev, ctxWrAddr, *(uint64_t *)pHwSecCtx);

    union large_table_desc done_val = {};
    done_val.ltd.src_bank = ctxWriteBank[writeChan];
    done_val.ltd.src_thread = writeChan;
    done_val.ltd.target_idx = handle;
    eka_write(dev, P4_CONFIRM_REG, done_val.lt_desc);

    ctxWriteBank[writeChan] =
        (ctxWriteBank[writeChan] + 1) %
        EKA_BANKS_PER_CTX_THREAD;
  }

  EfcStratGlobCtx stratGlobCtx = {};

private:
  bool isValidSecId(uint64_t secId);
  int initHwRoundTable();
  int cleanSecHwCtx();
  int normalizeId(uint64_t secId);
  int getLineIdx(uint64_t normSecId);
  EkaUdpSess *findUdpSess(EkaCoreId coreId, uint32_t mcAddr,
                          uint16_t mcPort);
  int setHwGlobalParams();
  int setHwUdpParams();
  int setHwStratRegion();
  int enableRxFire();
  int disableRxFire();
  int checkSanity();

  /* -----------------------------------------------------
   */
  static const int MAX_UDP_SESS = 64;
  static const int MAX_TCP_SESS = 64;
  static const int MAX_FIRE_ACTIONS = 64;
  static const int MAX_CTX_THREADS = 16;

  EkaHwHashTableLine *hashLine[EFC_SUBSCR_TABLE_ROWS] = {};

public:
  int numSecurities = 0;
  int ctxWriteBank[MAX_CTX_THREADS] = {};
  EfcCtx localCopyEfcCtx = {};
  EfcRunCtx localCopyEfcRunCtx = {};

  uint64_t pktCnt = 0; // for EFH compatibility

  EhpProtocol *ehp = NULL;
  uint64_t *secIdList =
      NULL; // array of SecIDs, index is handle
};

#endif
