#ifndef __EKA_EUR_STRATEGY_H__
#define __EKA_EUR_STRATEGY_H__

#include "EkaStrategyEhp.h"

#include "EhpEur.h"
#include "EkaBcEurProd.h"
#include "EkaHashEng.h"

#include "EkaEobiBook.h"

#include "EkaEobiTypes.h"
using namespace EkaEobi;

class EkaBcEurProd;
class EkaEobiParser;
class EkaEobiBook;
class EkaDev;

class EkaEurStrategy : public EkaStrategyEhp<EhpEur> {
private:
  static const int MaxSecurities_ = 16;
  static const size_t Rows = 64 * 1024;
  static const size_t Cols = 3;

  using HashEngT = EkaHashEng<EkaBcEurSecId,
                              EfhFeedVer::kEUR, Rows, Cols>;

public:
  EkaEurStrategy(const EfcUdpMcParams *mcParams);
  ~EkaEurStrategy();

  EkaBCOpResult
  subscribeSecList(const EkaBcEurSecId *prodList,
                   size_t nProducts);

  EkaBcSecHandle getSubscriptionId(EkaBcEurSecId secId);

  EkaBCOpResult
  initProd(EkaBcSecHandle prodHande,
           const EkaBcEurProductInitParams *params);

  EkaBCOpResult
  setProdJumpParams(EkaBcSecHandle prodHande,
                    const EkaBcEurJumpParams *params);

  void arm(EkaBcSecHandle prodHande, bool armBid,
           bool armAsk, EkaBcArmVer ver);

  EkaBCOpResult setProdReferenceJumpParams(
      EkaBcSecHandle triggerProd, EkaBcSecHandle fireProd,
      const EkaBcEurReferenceJumpParams *params);

  int sendDate2Hw();

  void runLoop(const EkaBcRunCtx *pEkaBcRunCtx);

  void ekaWriteTob(EkaBcSecHandle prodHandle,
                   const EobiHwBookParams *params,
                   SIDE side);

  EkaBCOpResult downloadPackedDB();

private:
  void configureTemplates();

  void writeTob(EkaBcSecHandle handle, void *params,
                uint paramsSize, SIDE side);

public:
public:
  EkaEobiBook *findBook(ExchSecurityId secId);

  void onTobChange(MdOut *mdOut, EkaEobiBook *book,
                   SIDE side);

  std::thread runLoopThr_;

private:
  int nSec_ = 0;

  EkaBcEurProd *prod_[MaxSecurities_] = {};

  HashEngT *hashEng_ = nullptr;

  EkaEobiParser *parser_ = nullptr;
};

#endif
