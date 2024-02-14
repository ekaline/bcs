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

#include "EkaSnDev.h"

#include "EkaEfc.h"
#include "EkaUdpChannel.h"

#include "EkaStrategy.h"

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

  for (auto i = 0; i < mcParams->nMcGroups; i++) {
    auto coreId = mcParams->groups[i].coreId;
    auto mcIp = mcParams->groups[i].mcIp;
    auto mcUdpPort = mcParams->groups[i].mcUdpPort;

    coreIdBitmap_ |= (1 << coreId);

    if (!(coreIdBitmap_ &
          dev_->ekaHwCaps->hwCaps.core.bitmap_md_cores))
      on_error("lane %d of MC Group %s:%u is not supported "
               "for HW parser",
               coreId, mcIp, mcUdpPort);

    auto inCoreIdx = mcCoreSess_[coreId].numUdpSess;
    mcCoreSess_[coreId].udpSess[inCoreIdx] = new EkaUdpSess(
        dev_, i, coreId, inet_addr(mcIp), mcUdpPort);
    mcCoreSess_[coreId].numUdpSess++;

    dev_->ekaIgmp->mcJoin(
        EkaEpmRegion::Regions::EfcMc,
        mcCoreSess_[coreId].udpSess[inCoreIdx]->coreId,
        mcCoreSess_[coreId].udpSess[inCoreIdx]->ip,
        mcCoreSess_[coreId].udpSess[inCoreIdx]->port,
        0,     // VLAN
        NULL); // pPktCnt
  }

  disableRxFire();
  disarm();
}
/* --------------------------------------------------- */
/* void EkaStrategy::downloadEhp2Hw() {
  ehp_->init();

  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    if (coreIdBitmap_ & (1 << coreId)) {
      EKA_LOG("Downloading Ehp for %s on coreId %d",
              name_.c_str(), coreId);
      ehp_->download2Hw(coreId);
    }
  }
} */
/* --------------------------------------------------- */

EkaStrategy::~EkaStrategy() {
  active_ = false;

  disarm();
  disableRxFire();
  eka_write(dev_, P4_STRAT_CONF, (uint64_t)0);
  auto swStatistics = eka_read(dev_, SW_STATISTICS);
  eka_write(dev_, SW_STATISTICS,
            swStatistics & ~(1ULL << 63));
}
/* --------------------------------------------------- */
void EkaStrategy::joinUdpChannels() {
  for (auto coreId = 0; coreId < EFC_MAX_CORES; coreId++) {
    if (!mcCoreSess_[coreId].numUdpSess)
      continue;

    udpChannel_[coreId] = new EkaUdpChannel(
        dev_, dev_->snDev->dev_id, coreId, -1);
    if (!udpChannel_[coreId])
      on_error("!udpChannel_[%d]", coreId);

    for (auto i = 0; i < mcCoreSess_[coreId].numUdpSess;
         i++) {
      if (!mcCoreSess_[coreId].udpSess[i])
        on_error("!udpSess[%d][%d]", coreId, i);

      auto ip = mcCoreSess_[coreId].udpSess[i]->ip;
      auto port = mcCoreSess_[coreId].udpSess[i]->port;

      EKA_LOG("Subscribing UdpChannel[%d]: "
              "Lane %d %s:%u",
              udpChannel_[coreId]->chId, coreId,
              EKA_IP2STR(ip), port);

      udpChannel_[coreId]->igmp_mc_join(ip, port, 0);

      uint64_t tmp_ipport = ((uint64_t)i) << 56 |
                            ((uint64_t)port) << 32 |
                            be32toh(ip);
      //  EKA_LOG("HW Port-IP register = 0x%016jx (%x :
      //  %x)", tmp_ipport,ip,port);
      eka_write(dev_, EkaEfc::HwUdpMcConfig + coreId * 8,
                tmp_ipport);
    }
  }
}

/* --------------------------------------------------- */

void EkaStrategy::arm(EfcArmVer ver) {
  EKA_LOG("Arming %s", name_.c_str());
  uint64_t armData = ((uint64_t)ver << 32) | 1;
  eka_write(dev_, 0xf07d0, armData);
}
/* --------------------------------------------------- */

void EkaStrategy::disarm() {
  EKA_LOG("Disarming %s", name_.c_str());
  eka_write(dev_, 0xf07d0, 0);
}
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

  for (auto i = 0; i < mcCoreSess_[coreId].numUdpSess;
       i++) {
    if (!mcCoreSess_[coreId].udpSess[i])
      on_error("!udpSess[%d]", i);
    if (mcCoreSess_[coreId].udpSess[i]->myParams(
            coreId, mcAddr, mcPort))
      return mcCoreSess_[coreId].udpSess[i];
  }
  return nullptr;
}
/* --------------------------------------------------- */
// Probably not needed anymore!
void EkaStrategy::disableHwUdp() {
  uint64_t fire_rx_tx_en = eka_read(dev_, ENABLE_PORT);
  fire_rx_tx_en &= 0xffffffff7fffffff; //[31] = 0, kill udp
  EKA_LOG("Prebook, turn off udp = 0x%016jx",
          fire_rx_tx_en);
  eka_write(dev_, ENABLE_PORT, fire_rx_tx_en);
  sleep(1);
}
/* ----------------------------------------------------- */

void EkaStrategy::enableHwUdp() {
  uint64_t fire_rx_tx_en = eka_read(dev_, ENABLE_PORT);
  fire_rx_tx_en |= 1ULL << (31); //[31] = 1, pass udp
  EKA_LOG("Post book, turn on udp = 0x%016jx",
          fire_rx_tx_en);
  eka_write(dev_, ENABLE_PORT, fire_rx_tx_en);
}
