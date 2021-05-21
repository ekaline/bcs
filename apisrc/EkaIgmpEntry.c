#include "EkaIgmpEntry.h"
#include "EkaEpm.h"
#include "EkaCore.h"
#include "EkaEpmAction.h"

int createIgmpPkt (IgmpPkt* dst, bool join, uint8_t* macsa, uint32_t ip_src, uint32_t ip_dst);
/* -------------------------------------------- */

EkaIgmpEntry::EkaIgmpEntry(EkaDev* _dev, int _udpChId, EkaCoreId _coreId, int _perChId, uint32_t _ip, uint16_t _port, int16_t _vlanTag, uint64_t* _pPktCnt) {
  dev     = _dev;
  udpChId = _udpChId;
  perChId = _perChId;
  ip      = _ip;
  port    = _port;
  vlanTag = _vlanTag;
  coreId  = _coreId;
  pPktCnt = _pPktCnt;

  if (udpChId < 0 || udpChId >= (int)EkaEpm::EPM_REGIONS) on_error("bad udpChId %d",udpChId);

  if (dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId);

  noIgmp  = ! EKA_NATIVE_MAC(dev->core[coreId]->macSa);
  
  if (noIgmp) return;

  uint epmActionRegion = (uint)udpChId;

  static const bool JOIN  = true;
  static const bool LEAVE = false;

  //  char __attribute__ ((aligned(sizeof(uint32_t)))) igmpJoinPkt[64] = {};
  IgmpPkt igmpJoinPkt = {};
  uint igmpJoinPktLen = createIgmpPkt(&igmpJoinPkt, JOIN, dev->core[coreId]->macSa, dev->core[coreId]->srcIp, ip);
  igmpJoinAction  = dev->epm->addAction(EkaEpm::ActionType::Igmp,epmActionRegion,0,coreId,0,0);
  igmpJoinAction->setEthFrame((uint8_t*)&igmpJoinPkt,igmpJoinPktLen);

  //  char __attribute__ ((aligned(sizeof(uint32_t)))) igmpLeavePkt[64] = {};
  IgmpPkt igmpLeavePkt = {};
  uint igmpLeavePktLen = createIgmpPkt(&igmpLeavePkt, LEAVE, dev->core[coreId]->macSa, dev->core[coreId]->srcIp, ip);
  igmpLeaveAction  = dev->epm->addAction(EkaEpm::ActionType::Igmp,epmActionRegion,0,coreId,0,0);
  igmpLeaveAction->setEthFrame((uint8_t*)&igmpLeavePkt,igmpLeavePktLen);
}
/* -------------------------------------------- */

bool EkaIgmpEntry::isMy(EkaCoreId _coreId, uint32_t _ip, uint16_t _port) {
  return (coreId == _coreId && ip == _ip && port == _port);
}
/* -------------------------------------------- */

int EkaIgmpEntry::sendIgmpJoin() {
  if (noIgmp) return 1;
  //  igmpJoinAction->print("Sending Igmp");
  return igmpJoinAction->send();
}
/* -------------------------------------------- */

int EkaIgmpEntry::sendIgmpLeave() {
  if (noIgmp) return 1;
  return igmpLeaveAction->send();
}
