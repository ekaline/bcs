#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "Efc.h"
#include "Efh.h"
#include "EkaEpm.h"
#include "EkaHwCaps.h"
#include "EkaIgmp.h"

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

extern EkaDev *g_ekaDev;

class EkaUdpSess;

/* --------------------------------------------------- */
EkaStrategy::EkaStrategy(const EfcUdpMcParams *mcParams) {
  dev_ = g_ekaDev;
  if (!dev_)
    on_error("!dev_");
  epm_ = dev_->epm;
  if (!epm_)
    on_error("!epm_");

  if (!mcParams)
    on_error("!mcParams");

  if (mcParams->nMcGroups > MaxUdpMcGroups)
    on_error("nMcGroups %ju > "
             "MaxUdpMcGroups %ju",
             mcParams->nMcGroups, MaxUdpMcGroups);

  numUdpSess_ = mcParams->nMcGroups;

  for (auto i = 0; i < numUdpSess_; i++) {
    coreIdBitmap_ |= (1 << mcParams->groups[i].coreId);

    if (!(coreIdBitmap_ &
          dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores))
      on_error("lane %d of MC Group %s:%u is not supported "
               "for HW parser",
               mcParams->groups[i].coreId,
               mcParams->groups[i].mcIp,
               mcParams->groups[i].mcUdpPort);

    udpSess_[i] =
        new EkaUdpSess(dev_, i, mcParams->groups[i].coreId,
                       inet_addr(mcParams->groups[i].mcIp),
                       mcParams->groups[i].mcUdpPort);

    dev_->ekaIgmp->mcJoin(
        EkaEpmRegion::Regions::EfcMc, udpSess_[i]->coreId,
        udpSess_[i]->ip, udpSess_[i]->port,
        0,     // VLAN
        NULL); // pPktCnt
  }

  disableRxFire();
  disArmController();
}
/* --------------------------------------------------- */

EkaStrategy::~EkaStrategy() {
  disArmController();
  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);
  auto swStatistics = eka_read(dev_, SW_STATISTICS);
  eka_write(dev_, SW_STATISTICS,
            swStatistics & ~(1ULL << 63));
}
/* --------------------------------------------------- */
/* --------------------------------------------------- */
uint8_t EkaStrategy::getCoreBitmap() {
  return coreIdBitmap_;
}

/* --------------------------------------------------- */
void EkaStrategy::clearAllHwUdpParams() {
  for (auto i = 0; i < MaxUdpMcGroups; i++) {
    uint32_t ip = 0;
    uint16_t port = 0;
    uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                          ((uint64_t)port) << 32 |
                          be32toh(ip);
    // eka_write(dev_, FH_GROUP_IPPORT, tmp_ipport);
  }
}

/* --------------------------------------------------- */
void EkaStrategy::armController(EfcArmVer ver) {
  EKA_LOG("Arming EFC");
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev_, P4_ARM_DISARM, armData);
}
/* --------------------------------------------------- */
void EkaStrategy::disArmController() {
  EKA_LOG("Disarming EFC");
  eka_write(dev_, P4_ARM_DISARM, 0);
}
/* --------------------------------------------------- */

void EkaStrategy::setHwUdpParams() {
  EKA_LOG("downloading %d MC sessions to FPGA",
          numUdpSess_);
  for (auto i = 0; i < numUdpSess_; i++) {
    if (!udpSess_[i])
      on_error("!udpSess_[%d]", i);

    EKA_LOG("configuring IP:UDP_PORT %s:%u for MD for "
            "group:%d",
            EKA_IP2STR(udpSess_[i]->ip), udpSess_[i]->port,
            i);
    uint32_t ip = udpSess_[i]->ip;
    uint16_t port = udpSess_[i]->port;

    uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                          ((uint64_t)port) << 32 |
                          be32toh(ip);
    //  EKA_LOG("HW Port-IP register = 0x%016jx (%x :
    //  %x)", tmp_ipport,ip,port);
    eka_write(dev_, FH_GROUP_IPPORT, tmp_ipport);
  }
}
/* --------------------------------------------------- */

void EkaStrategy::enableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev_, ENABLE_PORT);
  uint8_t tcpCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES;
       coreId++) {
    if ((0x1 << coreId) & tcpCores) {
      fire_rx_tx_en |= 1ULL
                       << (16 + coreId); // fire core enable
    }
    if ((0x1 << coreId) & mdCores) {
      fire_rx_tx_en |= 1ULL
                       << coreId; // RX (Parser) core enable
    }
  }

  eka_write(dev_, ENABLE_PORT, fire_rx_tx_en);
  EKA_LOG("Enabling Fire and EFC Parser: fire_rx_tx_en = "
          "0x%016jx",
          fire_rx_tx_en);
}
/* --------------------------------------------------- */
void EkaStrategy::disableRxFire() {
  uint64_t fire_rx_tx_en = eka_read(dev_, ENABLE_PORT);
  uint8_t tcpCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_tcp_cores;
  uint8_t mdCores =
      dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores;

  for (uint8_t coreId = 0; coreId < EkaDev::MAX_CORES;
       coreId++) {
    if ((0x1 << coreId) & tcpCores) {
      fire_rx_tx_en &=
          ~(1ULL << (16 + coreId)); // disable fire
    }
    if ((0x1 << coreId) & mdCores) {
      fire_rx_tx_en &=
          ~(1ULL << (coreId)); // RX (Parser) core disable
    }
  }
  eka_write(dev_, ENABLE_PORT, fire_rx_tx_en);

  EKA_LOG("Disabling Fire and EFC Parser: fire_rx_tx_en = "
          "0x%016jx",
          fire_rx_tx_en);
}

/* --------------------------------------------------- */
EkaUdpSess *EkaStrategy::findUdpSess(EkaCoreId coreId,
                                     uint32_t mcAddr,
                                     uint16_t mcPort) {
  for (auto i = 0; i < numUdpSess_; i++) {
    if (!udpSess_[i])
      on_error("!udpSess[%d]", i);
    if (udpSess_[i]->myParams(coreId, mcAddr, mcPort))
      return udpSess_[i];
  }
  return nullptr;
}
/* --------------------------------------------------- */
