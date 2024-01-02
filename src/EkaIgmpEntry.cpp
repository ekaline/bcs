#include "EkaIgmpEntry.h"
#include "EkaCore.h"
#include "EkaEpm.h"
#include "EkaEpmAction.h"
#include "EkaMcState.h"

int createIgmpPkt(IgmpPkt *dst, bool join, uint8_t *macsa,
                  uint32_t ip_src, uint32_t ip_dst);
/* -------------------------------------------- */

EkaIgmpEntry::EkaIgmpEntry(EkaDev *_dev, int _epmRegion,
                           EkaCoreId _coreId, int _perChId,
                           uint32_t _ip, uint16_t _port,
                           int16_t _vlanTag,
                           uint64_t *_pPktCnt) {
  dev_ = _dev;
  regionId_ = _epmRegion;
  perChId_ = _perChId;
  ip_ = _ip;
  port_ = _port;
  vlanTag_ = _vlanTag;
  coreId_ = _coreId;
  pPktCnt_ = _pPktCnt;

  if (!dev_->core[coreId_])
    on_error("!dev_->core[%u]", coreId_);

  noIgmp_ = !EKA_NATIVE_MAC(dev_->core[coreId_]->macSa);

  if (noIgmp_)
    return;

  static const bool JOIN = true;
  static const bool LEAVE = false;

  IgmpPkt igmpJoinPkt = {};
  uint igmpJoinPktLen = createIgmpPkt(
      &igmpJoinPkt, JOIN, dev_->core[coreId_]->macSa,
      dev_->core[coreId_]->srcIp, ip_);

  igmpJoinAction_ = dev_->epm->addAction(
      EkaEpm::ActionType::Igmp, perChId_ * 2, regionId_);
  if (!igmpJoinAction_)
    on_error("!igmpJoinAction_");

  igmpJoinAction_->setCoreId(coreId_);

  igmpJoinAction_->preloadFullPkt((uint8_t *)&igmpJoinPkt,
                                  igmpJoinPktLen);

  IgmpPkt igmpLeavePkt = {};
  uint igmpLeavePktLen = createIgmpPkt(
      &igmpLeavePkt, LEAVE, dev_->core[coreId_]->macSa,
      dev_->core[coreId_]->srcIp, ip_);

  igmpLeaveAction_ =
      dev_->epm->addAction(EkaEpm::ActionType::Igmp,
                           perChId_ * 2 + 1, regionId_);
  if (!igmpLeaveAction_)
    on_error("!igmpLeaveAction_");

  igmpLeaveAction_->setCoreId(coreId_);

  igmpLeaveAction_->preloadFullPkt((uint8_t *)&igmpLeavePkt,
                                   igmpLeavePktLen);
}
/* -------------------------------------------- */

bool EkaIgmpEntry::isMy(EkaCoreId _coreId, uint32_t _ip,
                        uint16_t _port) {
  return (coreId_ == _coreId && ip_ == _ip &&
          port_ == _port);
}
/* -------------------------------------------- */

void EkaIgmpEntry::saveMcState() {
  EkaMcState state = {.ip = ip_,
                      .port = port_,
                      .coreId = coreId_,
                      .pad = 0,
                      .pktCnt = pPktCnt_ ? *pPktCnt_ : 0};

  uint64_t chBaseAddr =
      SCRPAD_MC_STATE_BASE + regionId_ *
                                 MAX_MC_GROUPS_PER_UDP_CH *
                                 sizeof(EkaMcState);
  uint64_t addr =
      chBaseAddr + perChId_ * sizeof(EkaMcState);

  uint64_t *pData = (uint64_t *)&state;
  eka_write(dev_, addr, *pData++);
  eka_write(dev_, addr + 8, *pData++);
}
/* -------------------------------------------- */
int EkaIgmpEntry::sendIgmpJoin() {
  if (noIgmp_)
    return 1;
  //  igmpJoinAction_->print("Sending Igmp");
  return igmpJoinAction_->send();
}
/* -------------------------------------------- */

int EkaIgmpEntry::sendIgmpLeave() {
  if (noIgmp_)
    return 1;
  return igmpLeaveAction_->send();
}
