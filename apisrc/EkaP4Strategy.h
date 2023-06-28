#ifndef _EKA_P4_STRATEGY_H_
#define _EKA_P4_STRATEGY_H_

#include "EkaEpmRegion.h"
#include "eka_macros.h"

class EkaP4Strategy : EkaStrategy {
public:
  EkaP4Strategy(EfhFeedVer feedVer,
                const EpmStrategyParams *params);

  void cleanSubscrHwTable();
  void cleanSecHwCtx();
  void initHwRoundTable();
  bool isValidSecId(uint64_t secId);
  int normalizeId(uint64_t secId);
  int getLineIdx(uint64_t normSecId);
  int subscribeSec(uint64_t secId);
  EfcSecCtxHandle getSubscriptionId(uint64_t secId);
  int downloadTable();

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

public:
  int numSecurities = 0;
  int ctxWriteBank[MAX_CTX_THREADS] = {};

private:
  void preallocateFireActions();
  void configureTemplates();

  bool isValidSecId(uint64_t secId);
  int initHwRoundTable();
  int cleanSecHwCtx();
  int normalizeId(uint64_t secId);
  int getLineIdx(uint64_t normSecId);

  EfhFeedVer feedVer_ = EfhFeedVer::kInvalid;
};
#endif