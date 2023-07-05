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

#include "EkaQedStrategy.h"

#include "EhpCmeFC.h"
#include "EhpItchFS.h"
#include "EhpNews.h"
#include "EhpNom.h"
#include "EhpPitch.h"
#include "EhpQED.h"

#include "EpmBoeQuoteUpdateShortTemplate.h"
#include "EpmCancelBoeTemplate.h"
#include "EpmCmeILinkHbTemplate.h"
#include "EpmCmeILinkSwTemplate.h"
#include "EpmCmeILinkTemplate.h"
#include "EpmFastSweepUDPReactTemplate.h"
#include "EpmFireBoeTemplate.h"
#include "EpmFireSqfTemplate.h"

#include "EkaHwHashTableLine.h"

/* --------------------------------------------------- */
EkaQedStrategy::EkaQedStrategy(
    const EfcUdpMcParams *mcParams,
    const EfcQedParams *qedParams)
    : EkaStrategy(mcParams) {

  name_ = "QedPurge";

  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);

  configureTemplates();
  configureEhp();

  for (auto i = 0; i < EKA_QED_PRODUCTS; i++) {
    conf_.product[i].enable = qedParams->product[i].enable;
    conf_.product[i].minNumLevel =
        qedParams->product[i].min_num_level;
    conf_.product[i].dsID = qedParams->product[i].ds_id;
    conf_.product[i].token = EkaEpm::DefaultToken;
    conf_.product[i].fireActionId = -1;
    conf_.product[i].strategyId = (uint8_t)EFC_STRATEGY;

    EKA_LOG("Configuring QED FPGA: product=%d, enable=%d, "
            "min_num_level=%d,ds_id=0x%x,token=0x%jx,"
            "fireActionId=%d,strategyId=%d",
            i, conf_.product[i].enable,
            conf_.product[i].minNumLevel,
            conf_.product[i].dsID, conf_.product[i].token,
            conf_.product[i].fireActionId,
            conf_.product[i].strategyId);
    //  hexDump("EfcQEDStrategyConf",&conf,sizeof(conf),stderr);

    if (conf_.product[i].enable)
      prodCnt_++;
  }

  copyBuf2Hw(dev_, 0x86000, (uint64_t *)&conf_,
             sizeof(conf_));

  EKA_LOG("Created %s with %d MC groups, "
          "valid products = %u",
          name_.c_str(), numUdpSess_, prodCnt_);
}
/* --------------------------------------------------- */

void EkaQedStrategy::setFireAction(
    epm_actionid_t fireActionId, int productId) {
  if (productId < 0 || productId >= EKA_QED_PRODUCTS)
    on_error("Invalid productId %d", productId);

  if (!conf_.product[productId].enable)
    on_error("productId %d is not enabled", productId);

  conf_.product[productId].fireActionId = fireActionId;
  copyBuf2Hw(dev_, 0x86000, (uint64_t *)&conf_,
             sizeof(conf_));

  EKA_LOG("Qed: Action[%d] is set to Fire for product %d",
          fireActionId, productId);
}

/* --------------------------------------------------- */
void EkaQedStrategy::arm(EfcArmVer ver) {
  EKA_LOG("Arming Qed");
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev_, 0xf07d0, armData);
}
/* --------------------------------------------------- */

void EkaQedStrategy::disarm() {
  EKA_LOG("Disarming Qed");
  eka_write(dev_, 0xf07d0, 0);
}
/* --------------------------------------------------- */

/* --------------------------------------------------- */
void EkaQedStrategy::configureTemplates() {
  int templateIdx = (int)EkaEpm::TemplateId::QedHwFire;

  epm_->epmTemplate[templateIdx] =
      new EpmBoeQuoteUpdateShortTemplate(templateIdx);

  epm_->DownloadSingleTemplate2HW(
      epm_->epmTemplate[templateIdx]);
}

/* --------------------------------------------------- */
void EkaQedStrategy::configureEhp() {
  auto ehp = new EhpQED(dev_);
  if (!ehp)
    on_error("!ehp");

  ehp->init();

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    if (coreIdBitmap_ & (1 << coreId))
      ehp->download2Hw(coreId);
  }
}
