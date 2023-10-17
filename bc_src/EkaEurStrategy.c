#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "Efc.h"
#include "Efh.h"
#include "EkaEpm.h"

#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"

#include "EkaEfcDataStructs.h"

#include "EkaEurStrategy.h"
#include "EkaHwHashTableLine.h"

/* --------------------------------------------------- */
EkaEurStrategy::EkaEurStrategy(
    const EfcUdpMcParams *mcParams)
    : EkaStrategyEhp(mcParams) {

  name_ = "Eurex";

  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);

  configureTemplates();
  configureEhp();
  // downloadEhp2Hw();

  EKA_LOG("Creating %s with %d MC groups on lane #0 "
          "and %d MC groups on lane #1",
          name_.c_str(), mcCoreSess_[0].numUdpSess,
          mcCoreSess_[1].numUdpSess);

  hashEng_ = new HashEngT();
  if (!hashEng_)
    on_error("!hashEng_");

  EKA_LOG("Created HashEng for Eurex with "
          "%ju Rows, %ju Cols",
          Rows, Cols);
}

/* --------------------------------------------------- */

/* --------------------------------------------------- */

EkaBCOpResult
EkaEurStrategy::subscribeSecList(const EkaBcSecId *prodList,
                                 size_t nProducts) {
  auto prod = prodList;
  for (auto i = 0; i < nProducts; i++) {
    if (hashEng_->subscribeSec(*prod)) {
      nSec_++;
#ifndef _VERILOG_SIM
      uint64_t val = eka_read(dev_, SW_STATISTICS);
      val &= 0xFFFFFFFF00000000;
      val |= (uint64_t)(nSec_);
      eka_write(dev_, SW_STATISTICS, val);
#endif
    }
    prod++;
  }

  hashEng_->packDB();

  return EKABC_OPRESULT__OK;
}

/* --------------------------------------------------- */
EkaBCOpResult EkaEurStrategy::downloadPackedDB() {
  for (auto i = 0; i < Rows; i++) {
    static const size_t BufLen = 256;
    uint64_t buf[BufLen] = {};

    auto [validCnt, len] = hashEng_->getPackedLine(i, buf);

    int packedWords = roundUp8(len) / 8;

    uint64_t *pWord = buf;
    for (auto i = 0; i < packedWords; i++) {
#ifdef EFC_PRINT_HASH
      if (validCnt != 0)
        EKA_LOG("%d: 0x%016jx", i, *pWord);
#endif
      eka_write(dev_, FH_SUBS_HASH_BASE + 8 * i, *pWord);
      pWord++;
    }

    union large_table_desc desc = {};
    desc.ltd.src_bank = 0;
    desc.ltd.src_thread = 0;
    desc.ltd.target_idx = i;
    eka_write(dev_, FH_SUBS_HASH_DESC, desc.lt_desc);
  }
  return EKABC_OPRESULT__OK;
}
/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::initProd(
    EkaBcSecHandle prodHande,
    const EkaBcEurProductInitParams *params) {
  if (prodHande < 0 || prodHande >= MaxSecurities_) {
    EKA_ERROR("Bad prodHande %jd", prodHande);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (prod[prodHande]) {
    EKA_ERROR("Product with prodHande %jd "
              "is already initialized",
              prodHande);
    return EKABC_OPRESULT__ERR_PRODUCT_ALREADY_INITED;
  }

  prod[prodHande] = new EkaBcEurProd(prodHande, params);

  if (!prod[prodHande])
    on_error("failed creating new Prod");

  return EKABC_OPRESULT__OK;
}

/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::setProdJumpParams(
    EkaBcSecHandle prodHande,
    const EkaBcEurJumpParams *params) {
  if (prodHande < 0 || prodHande >= MaxSecurities_) {
    EKA_ERROR("Bad prodHande %jd", prodHande);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (!prod[prodHande]) {
    EKA_ERROR("Product with prodHande %jd "
              "is not initialized",
              prodHande);
    return EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST;
  }

  return prod[prodHande]->setJumpParams(params);
}

/* --------------------------------------------------- */

EkaBCOpResult EkaEurStrategy::setProdReferenceJumpParams(
    EkaBcSecHandle triggerProd, EkaBcSecHandle fireProd,
    const EkaBcEurReferenceJumpParams *params) {
  if (triggerProd < 0 || triggerProd >= MaxSecurities_) {
    EKA_ERROR("Bad triggerProd %jd", triggerProd);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (fireProd < 0 || fireProd >= MaxSecurities_) {
    EKA_ERROR("Bad fireProd %jd", fireProd);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  if (!prod[triggerProd]) {
    EKA_ERROR("triggerProd with handle %jd "
              "is not initialized",
              triggerProd);
    return EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST;
  }

  if (!prod[fireProd]) {
    EKA_ERROR("fireProd with handle %jd "
              "is not initialized",
              fireProd);
    return EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST;
  }

  if (triggerProd == fireProd) {
    EKA_ERROR("triggerProd == fireProd =  %jd ", fireProd);
    return EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE;
  }

  return prod[triggerProd]->setReferenceJumpParams(fireProd,
                                                   params);
}

/* --------------------------------------------------- */
void EkaEurStrategy::configureTemplates() {
  int templateIdx = -1;
#if 0
  templateIdx = (int)EkaEpm::TemplateId::CmeHwCancel;
  epm_->epmTemplate[templateIdx] =
      new EpmCmeILinkTemplate(templateIdx);
  epm_->DownloadSingleTemplate2HW(
      epm_->epmTemplate[templateIdx]);

  templateIdx = (int)EkaEpm::TemplateId::CmeSwFire;
  epm_->epmTemplate[templateIdx] =
      new EpmCmeILinkSwTemplate(templateIdx);
  epm_->DownloadSingleTemplate2HW(
      epm_->epmTemplate[templateIdx]);

  templateIdx = (int)EkaEpm::TemplateId::CmeSwHb;
  epm_->epmTemplate[templateIdx] =
      new EpmCmeILinkHbTemplate(templateIdx);
  epm_->DownloadSingleTemplate2HW(
      epm_->epmTemplate[templateIdx]);
#endif
}
/* --------------------------------------------------- */
int EkaEurStrategy::sendDate2Hw() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  uint64_t current_time_ns =
      ((uint64_t)(t.tv_sec) * (uint64_t)1000000000 +
       (uint64_t)(t.tv_nsec)); // UTC
  //  current_time_ns += static_cast<uint64_t>(6*60*60) *
  //  static_cast<uint64_t>(1e9); //+6h UTC time

  eka_write(dev_, 0xf0300 + 0 * 8,
            current_time_ns); // data, last write commits
                              // the change
  eka_write(dev_, 0xf0300 + 1 * 8,
            0x0); // data, last write commits the change
  eka_write(dev_, 0xf0300 + 2 * 8,
            0x0); // data, last write commits the change

  return 1;
}
