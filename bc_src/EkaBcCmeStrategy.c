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

#include "EkaBcCmeStrategy.h"

/* --------------------------------------------------- */
EkaBcCmeStrategy::EkaBcCmeStrategy(
    const EfcUdpMcParams *mcParams,
    const EkaBcCmeFcAlgoParams *cmeParams)
    : EkaStrategyEhp(mcParams) {

  name_ = "BcCmeFC";

  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);

  configureTemplates();
  configureEhp();
  // downloadEhp2Hw();

  conf_.minNoMDEntries = params->minNoMDEntries;
  conf_.minTimeDiff = params->minTimeDiff;
  conf_.token = DefaultToken;
  conf_.fireActionId = (uint16_t)params->fireActionId;
  conf_.strategyId = (uint8_t)EFC_STRATEGY;

  EKA_LOG("Configuring Cme Fast Cancel FPGA: "
          "minNoMDEntries=%d, "
          "minTimeDiff=%ju, "
          "fireActionId=%d",
          conf_.minNoMDEntries, conf_.minTimeDiff,
          conf_.fireActionId);
  //  hexDump("EfcCmeFastCancelStrategyConf",&conf,
  //          sizeof(conf),stderr);
  copyBuf2Hw(dev, 0x84000, (uint64_t *)&conf_,
             sizeof(conf_));

  EKA_LOG("Creating %s with %d MC groups on lane #0 "
          "and %d MC groups on lane #1",
          name_.c_str(), mcCoreSess_[0].numUdpSess,
          mcCoreSess_[1].numUdpSess);
}
/* --------------------------------------------------- */
void EkaBcCmeStrategy::configureTemplates() {
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
