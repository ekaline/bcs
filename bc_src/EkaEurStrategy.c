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
