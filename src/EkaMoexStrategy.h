#ifndef __EKA_MOEX_STRATEGY_H__
#define __EKA_MOEX_STRATEGY_H__

#include "EkaStrategyEhp.h"

#include "EhpMoex.h"
#include "EkaBcsMoexProd.h"
#include "EkaHashEng.h"

#include "EkaEobiBook.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

class EkaBcsMoexProd;
class EkaEobiParser;
class EkaEobiBook;
class EkaDev;

class EkaMoexStrategy : public EkaStrategyEhp<EhpMoex> {
private:
  static const int MaxSecurities_ = 16;
  static const size_t Rows = 64 * 1024;
  static const size_t Cols = 3;

  //  using HashEngT = EkaHashEng<EkaBcEurSecId,
  //                              EfhFeedVer::kEUR, Rows, Cols>;

public:
  EkaMoexStrategy(const EfcUdpMcParams *mcParams);
  ~EkaMoexStrategy();

  OpResult
  subscribeSecList(const EkaBcsMoexSecId *prodList,
                   size_t nProducts);

  EkaBcsSecHandle getSubscriptionId(EkaBcsMoexSecId secId);

  OpResult
  initProd(EkaBcsSecHandle prodHande,
           const EkaBcsMoexProductInitParams *params);

  OpResult setProdDynamicParams(
      EkaBcsSecHandle prodHande,
      const EkaBcsProductDynamicParams *params);

  // OpResult
  // setProdJumpParams(EkaBcsSecHandle prodHande,
  //                   const EkaBcsMoexJumpParams *params);

  void arm(EkaBcsSecHandle prodHande, bool armBid,
           bool armAsk, EkaBcsArmVer ver);

  // OpResult setProdReferenceJumpParams(
  //     EkaBcsSecHandle triggerProd, EkaBcsSecHandle fireProd,
  //     const EkaBcsMoexReferenceJumpParams *params);

  int sendDate2Hw();

  void runLoop(const EkaBcsRunCtx *pEkaBcsRunCtx);

  // void ekaWriteTob(EkaBcsSecHandle prodHandle,
  //                  const EobiHwBookParams *params,
  //                  SIDE side);

  EkaBcsMoexProd *getProd(EkaBcsSecHandle prodHandle);
  
  OpResult downloadPackedDB();

  OpResult downloadProdInfoDB();

  void
  fireReportThreadLoop(const EkaBcsRunCtx *pEkaBcsRunCtx);

private:
  void configureTemplates();

  void writeTob(EkaBcsSecHandle handle, void *params,
                uint paramsSize, SIDE side);

  std::pair<int, size_t> processSwTriggeredReport(
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
  // EkaEobiBook *findBook(ExchSecurityId secId);

  // void onTobChange(MdOut *mdOut, EkaEobiBook *book,
  //                  SIDE side);

  std::thread runLoopThr_;
  std::thread fireReportLoopThr_;

private:
  int nSec_ = 0;

  EkaBcsMoexProd *prod_[MaxSecurities_] = {};

  HashEngT *hashEng_ = nullptr;

public:
  //  EkaEobiParser *parser_ = nullptr;
};

#endif
