#ifndef __EKA_MOEX_STRATEGY_H__
#define __EKA_MOEX_STRATEGY_H__

#include "EkaStrategyEhp.h"

#include "EhpMoex.h"
// #include "EkaBcsMoexProd.h"
#include "EkaHashEng.h"

class EkaBcsMoexProd;
class EkaEobiParser;
class EkaEobiBook;
class EkaDev;
class EkaMoexProdPair;

class EkaMoexStrategy : public EkaStrategyEhp<EhpBcsMoex> {
private:
  static const int MaxSecurities_ = 16;
  static const size_t Rows = 64 * 1024;
  static const size_t Cols = 3;

  //  using HashEngT = EkaHashEng<EkaBcEurSecId,
  //                              EfhFeedVer::kEUR, Rows,
  //                              Cols>;

public:
  EkaMoexStrategy(const EfcUdpMcParams *mcParams);
  ~EkaMoexStrategy();

  OpResult subscribeSecList(const EkaBcsMoexSecId *prodList,
                            size_t nProducts);

  EkaBcsSecHandle getSubscriptionId(EkaBcsMoexSecId secId);

  OpResult initPair(PairIdx idx,
                    const ProdPairInitParams *params);

  OpResult
  setPairDynamicParams(PairIdx idx,
                       const ProdPairDynamicParams *params);

  // OpResult
  // setProdJumpParams(EkaBcsSecHandle prodHande,
  //                   const EkaBcsMoexJumpParams *params);

  void arm(bool arm, EkaBcsArmVer ver);

  void runLoop(const EkaBcsRunCtx *pEkaBcsRunCtx);

  // EkaBcsMoexProd *getProd(EkaBcsSecHandle prodHandle);

  OpResult downloadPackedDB();

  OpResult downloadProdInfoDB();

private:
  typedef enum {
    BID = 0,
    ASK = 1,
    IRRELEVANT = 3,
    ERROR = 101,
    BOTH = 200,
    NONE = 300
  } SIDE;

  void configureTemplates();

  void writeTob(EkaBcsSecHandle handle, void *params,
                uint paramsSize, SIDE side);

  std::pair<int, size_t> processSwTriggeredReprt(
      EkaDev *dev, const uint8_t *srcReport,
      uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
      uint8_t *reportBuf);

  std::pair<int, size_t> processExceptionsReport(
      EkaDev *dev, const uint8_t *srcReport,
      uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
      uint8_t *reportBuf);

  std::pair<int, size_t> processFastCancelReport(
      EkaDev *dev, const uint8_t *srcReport,
      uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
      uint8_t *reportBuf);

  std::pair<int, size_t>
  processFireReport(EkaDev *dev, const uint8_t *srcReport,
                    uint srcReportLen, EkaUserReportQ *q,
                    uint32_t dmaIdx, uint8_t *reportBuf);

public:
  std::thread runLoopThr_;
  std::thread fireReportLoopThr_;

private:
  int nSec_ = 0;

  // EkaBcsMoexProd *prod_[MaxSecurities_] = {};

  EkaMoexProdPair *pair_[MOEX_MAX_PROD_PAIRS] = {};

public:
  //  EkaEobiParser *parser_ = nullptr;
};

#endif
