#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <linux/sockios.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
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

#include "EkaCmeFcStrategy.h"

#include "EhpCmeFC.h"

#include "EpmCmeILinkHbTemplate.h"
#include "EpmCmeILinkSwTemplate.h"
#include "EpmCmeILinkTemplate.h"

/* --------------------------------------------------- */
EkaCmeFcStrategy::EkaCmeFcStrategy(
    const EfcUdpMcParams *mcParams,
    const EfcCmeFcParams *cmeParams)
    : EkaStrategy(mcParams) {

  name_ = "CmeFastCancel";

  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);

  configureTemplates();
  configureEhp();
  downloadEhp2Hw();

  conf_.minNoMDEntries = cmeParams->minNoMDEntries;
  conf_.maxMsgSize = cmeParams->maxMsgSize;
  conf_.token = EkaEpm::DefaultToken;
  conf_.fireActionId = -1;
  conf_.strategyId = (uint8_t)EFC_STRATEGY;

  copyBuf2Hw(dev_, ConfHwAddr, (uint64_t *)&conf_,
             sizeof(conf_));

  EKA_LOG("Created %s with %d MC groups, "
          "minNoMDEntries = %u, maxMsgSize = %u",
          name_.c_str(), numUdpSess_, conf_.minNoMDEntries,
          conf_.maxMsgSize);
}
/* --------------------------------------------------- */

void EkaCmeFcStrategy::setFireAction(
    epm_actionid_t fireActionId, int productId) {

  conf_.fireActionId = fireActionId;
  copyBuf2Hw(dev_, ConfHwAddr, (uint64_t *)&conf_,
             sizeof(conf_));

  EKA_LOG("%s: First Fire Action Idx = %d", name_.c_str(),
          fireActionId);
}

/* --------------------------------------------------- */

/* --------------------------------------------------- */
void EkaCmeFcStrategy::configureTemplates() {
  int templateIdx = -1;

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
}

/* --------------------------------------------------- */
void EkaCmeFcStrategy::configureEhp() {
  auto ehp_ = new EhpCmeFC(dev_);
  if (!ehp_)
    on_error("!ehp_");
}
