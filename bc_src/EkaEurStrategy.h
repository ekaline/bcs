#ifndef __EKA_EUR_STRATEGY_H__
#define __EKA_EUR_STRATEGY_H__

#include "EkaStrategyEhp.h"

#include "EhpEur.h"
#include "EkaHashEng.h"

class EkaEurStrategy : public EkaStrategyEhp<EhpEur> {
public:
  EkaEurStrategy(const EfcUdpMcParams *mcParams);

  EkaBCOpResult subscribeSecList(const EkaBcSecId *prodList,
                                 size_t nProducts);

  EkaBCOpResult
  initProd(EkaBcSecHandle prodHande,
           const EkaBcEurProductInitParams *params);

private:
  void configureTemplates();

  EkaBCOpResult downloadPackedDB();

#if 0
  void runBook();
  void createBooks(uint8_t coreId);
  bool productsBelongTo(uint8_t coreId);

  int ekaWriteTob(EkaDev *dev, void *params,
                  uint paramsSize, eka_product_t product_id,
                  eka_side_type_t side);
#endif

private:
  static const int MaxSecurities_ = 16;
  static const size_t Rows = 64 * 1024;
  static const size_t Cols = 3;

  using HashEngT =
      EkaHashEng<EkaBcSecId, EfhFeedVer::kEUR, Rows, Cols>;

  int nSec_ = 0;

  HashEngT *hashEng_ = nullptr;

  EkaUdpChannel *udpChannel[4 /*MAX_CORES*/] = {};

  int regionId_ = EkaEpmRegion::Regions::Efc;
};

#endif
