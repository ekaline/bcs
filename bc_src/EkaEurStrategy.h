#ifndef __EKA_EUR_STRATEGY_H__
#define __EKA_EUR_STRATEGY_H__

#include "EkaStrategyEhp.h"

class EkaEurStrategy : public EkaStrategyEhp<EhpEobi> {
public:
  EkaEurStrategy(const EfcUdpMcParams *mcParams);

private:
  void configureTemplates();

  void runBook();
  void createBooks(uint8_t coreId);
  void subscribeOnMc(uint8_t coreId);
  void createBooks(uint8_t coreId);
  int subscribeOnMc(uint8_t coreId);
  bool productsBelongTo(uint8_t coreId);

  int ekaWriteTob(EkaDev *dev, void *params,
                  uint paramsSize, eka_product_t product_id,
                  eka_side_type_t side);

private:
  std::vector<ProdCtx> prod;
  EkaUdpChannel *udpChannel[4 /*MAX_CORES*/] = {};

  int regionId_ = EkaEpmRegion::Regions::Efc;
};
