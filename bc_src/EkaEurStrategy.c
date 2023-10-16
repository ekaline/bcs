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
  return EKABC_OPRESULT__OK;
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
