/* NE SHURIK */

#include "EkaEpmAction.h"
#include "EkaCsumSSE.h"
#include "EkaDev.h"
#include "EkaEpm.h"
#include "EkaEpmRegion.h"
#include "EkaTcpSess.h"
#include "EkaUdpTxSess.h"
#include "EkaWc.h"
#include "EpmTemplate.h"
#include "ekaNW.h"

/* uint32_t calc_pseudo_csum (const void* ip_hdr, const
 * void* tcp_hdr, */
/* 			   const void* payload, uint16_t
 * payload_size); */
unsigned short csum(const unsigned short *ptr, int nbytes);
uint32_t calcUdpCsum(void *ip_hdr, void *udp_hdr,
                     void *payload, uint16_t payload_size);
uint16_t udp_checksum(EkaUdpHdr *p_udp_header, size_t len,
                      uint32_t src_addr,
                      uint32_t dest_addr);

/* ---------------------------------------------------- */
int EkaEpmAction::setActionBitmap() {

  if (epmActionLocalCopy.actionFlags == AF_Valid)
    actionBitParams.bitmap.action_valid = 1;
  else
    actionBitParams.bitmap.action_valid = 0;

  actionBitParams.bitmap.originatedFromHw = 0;

  switch (type) {
  case EpmActionType::TcpFastPath:
    actionBitParams.bitmap.israw = 0;
    actionBitParams.bitmap.report_en = 0;
    actionBitParams.bitmap.feedbck_en = 1;
    break;
  case EpmActionType::TcpFullPkt:
    actionBitParams.bitmap.israw = 1;
    actionBitParams.bitmap.report_en = 0;
    actionBitParams.bitmap.feedbck_en = 0;
    break;
  case EpmActionType::TcpEmptyAck:
    actionBitParams.bitmap.israw = 0;
    actionBitParams.bitmap.report_en = 0;
    actionBitParams.bitmap.feedbck_en = 0;
    break;
  case EpmActionType::Igmp:
    actionBitParams.bitmap.israw = 1;
    actionBitParams.bitmap.report_en = 0;
    actionBitParams.bitmap.feedbck_en = 0;
    break;
  case EpmActionType::CmeSwFire:
    actionBitParams.bitmap.app_seq_inc = 1;
    actionBitParams.bitmap.israw = 0;
    actionBitParams.bitmap.report_en = 0;
    actionBitParams.bitmap.feedbck_en = 1;
    break;
  case EpmActionType::BoeFire:
  case EpmActionType::CmeHwCancel:
  case EpmActionType::QEDHwPurge:
  case EpmActionType::SqfFire:
    actionBitParams.bitmap.originatedFromHw = 1;
    [[fallthrough]];
  case EpmActionType::HwFireAction:
    actionBitParams.bitmap.app_seq_inc = 1;
    [[fallthrough]];
  case EpmActionType::BoeCancel:
  case EpmActionType::SqfCancel:

  case EpmActionType::CmeSwHeartbeat:

    actionBitParams.bitmap.israw = 0;
    actionBitParams.bitmap.report_en = 1;
    actionBitParams.bitmap.feedbck_en = 1;
    break;
  case EpmActionType::ItchHwFastSweep:
    actionBitParams.bitmap.israw = 1;
    actionBitParams.bitmap.report_en = 1;
    actionBitParams.bitmap.feedbck_en = 0;
    break;

  case EpmActionType::UserAction:
    actionBitParams.bitmap.israw = 0;
    actionBitParams.bitmap.report_en = 1;
    actionBitParams.bitmap.feedbck_en = 1;
    break;
  default:
    on_error("Unexpected EkaEpmAction type %d", (int)type);
  }
  return 0;
}
/* ---------------------------------------------------- */

void EkaEpmAction::setIpTtl() {
  switch (type) {
  case EpmActionType::UserAction:
  case EpmActionType::HwFireAction:
  case EpmActionType::BoeFire:
  case EpmActionType::BoeCancel:
  case EpmActionType::SqfFire:
  case EpmActionType::SqfCancel:
  case EpmActionType::CmeHwCancel:
  case EpmActionType::QEDHwPurge:
  case EpmActionType::ItchHwFastSweep:
    ipHdr->_ttl = EFC_HW_TTL;
    ipHdr->_id = EFC_HW_ID;
    return;
  default:
    return;
  }
}
/* ---------------------------------------------------- */

static TcpCsSizeSource
setTcpCsSizeSource(EpmActionType type) {
  switch (type) {
  case EpmActionType::UserAction:
  case EpmActionType::HwFireAction:
  case EpmActionType::BoeFire:
  case EpmActionType::BoeCancel:
  case EpmActionType::SqfFire:
  case EpmActionType::SqfCancel:
  case EpmActionType::CmeHwCancel:
  case EpmActionType::QEDHwPurge:
  case EpmActionType::ItchHwFastSweep:
    return TcpCsSizeSource::FROM_ACTION;
  default:
    return TcpCsSizeSource::FROM_DESCR;
  }
}
/* ---------------------------------------------------- */
int EkaEpmAction::setHwAction() {
  hwAction.bit_params = actionBitParams;
  hwAction.tcpcs_template_db_ptr = epmTemplate->id;
  hwAction.template_db_ptr =
      epmTemplate->getDataTemplateAddr();
  hwAction.data_db_ptr = heapAddr;
  hwAction.target_prod_id = productIdx;
  hwAction.target_core_id = coreId;
  hwAction.target_session_id = sessId;
  hwAction.next_action_index =
      epmActionLocalCopy.nextAction;
  hwAction.mask_post_strat =
      epmActionLocalCopy.postStratMask;
  hwAction.mask_post_local =
      epmActionLocalCopy.postLocalMask;
  hwAction.enable_bitmap = epmActionLocalCopy.enable;
  hwAction.user = epmActionLocalCopy.user;
  hwAction.token = epmActionLocalCopy.token;
  hwAction.tcpCSum = tcpCSum;
  hwAction.payloadSize = pktSize;
  hwAction.tcpCsSizeSource = setTcpCsSizeSource(type);

  copyBuf2Hw(dev, EkaEpm::EpmActionBase,
             (uint64_t *)&hwAction,
             sizeof(hwAction)); // write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,
                         0, idx, 0);

  return 0;
}
/* ---------------------------------------------------- */
void EkaEpmAction::printHeap() {
  EKA_LOG("heapOffs = %u 0x%x", heapOffs, heapOffs);
  EKA_LOG("epmRegion = %u", region);
  hexDump("Heap", &epm->heap[heapOffs], pktSize);
}
/* ---------------------------------------------------- */
void EkaEpmAction::printHwAction() {
  //  EKA_LOG("hwAction.bit_params=0x%x",
  //  (uint)hwAction.bit_params);
  EKA_LOG("hwAction.tcpcs_template_db_ptr =0x%x",
          hwAction.tcpcs_template_db_ptr);
  EKA_LOG("hwAction.template_db_ptr=0x%x",
          hwAction.template_db_ptr);
  EKA_LOG("hwAction.data_db_ptr=0x%x",
          hwAction.data_db_ptr);
  EKA_LOG("hwAction.target_prod_id=0x%x",
          hwAction.target_prod_id);
  EKA_LOG("hwAction.target_core_id=0x%x",
          hwAction.target_core_id);
  EKA_LOG("hwAction.target_session_id=0x%x",
          hwAction.target_session_id);
  EKA_LOG("hwAction.next_action_index=0x%x",
          hwAction.next_action_index);
  EKA_LOG("hwAction.mask_post_strat=0x%jx",
          hwAction.mask_post_strat);
  EKA_LOG("hwAction.mask_post_local=0x%jx",
          hwAction.mask_post_local);
  EKA_LOG("hwAction.enable_bitmap=0x%jx",
          hwAction.enable_bitmap);
  EKA_LOG("hwAction.user=0x%jx", hwAction.user);
  EKA_LOG("hwAction.token=0x%jx", hwAction.token);
  EKA_LOG("hwAction.tcpCSum=0x%x", hwAction.tcpCSum);
  EKA_LOG("hwAction.payloadSize=0x%x",
          hwAction.payloadSize);
  EKA_LOG("hwAction.tcpCsSizeSource=0x%x",
          (uint)hwAction.tcpCsSizeSource);

  copyBuf2Hw(dev, EkaEpm::EpmActionBase,
             (uint64_t *)&hwAction,
             sizeof(hwAction)); // write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,
                         0, idx, 0);
}

/* ---------------------------------------------------- */
int EkaEpmAction::setTemplate() {
  switch (type) {
  case EpmActionType::TcpFastPath:
    epmTemplate = epm->tcpFastPathPkt;
    break;
  case EpmActionType::TcpFullPkt:
    epmTemplate = epm->rawPkt;
    break;
  case EpmActionType::TcpEmptyAck:
    epmTemplate = epm->tcpFastPathPkt;
    break;
  case EpmActionType::Igmp:
    epmTemplate = epm->rawPkt;
    break;
  case EpmActionType::SqfFire:
  case EpmActionType::SqfCancel:
  case EpmActionType::BoeFire:
  case EpmActionType::BoeCancel:
  case EpmActionType::HwFireAction:
  case EpmActionType::CmeHwCancel:
  case EpmActionType::QEDHwPurge:
  case EpmActionType::ItchHwFastSweep:
    epmTemplate = epm->hwFire;
    break;
  case EpmActionType::CmeSwFire:
    epmTemplate = epm->swFire;
    break;

  case EpmActionType::CmeSwHeartbeat:
    epmTemplate = epm->cmeHb;
    break;

  case EpmActionType::UserAction:
    epmTemplate = epm->tcpFastPathPkt;
    break;
  default:
    on_error("Unexpected EkaEpmAction type %d", (int)type);
  }
  if (epmTemplate == NULL)
    on_error("type %d: epmTemplate == NULL ", (int)type);
  //  EKA_LOG("Teplate set to: \'%s\'",epmTemplate->name);
  return 0;
}

/* ---------------------------------------------------- */
int EkaEpmAction::setName() {
  switch (type) {
  case EpmActionType::TcpFastPath:
    strcpy(actionName, "TcpFastPath");
    break;
  case EpmActionType::TcpFullPkt:
    strcpy(actionName, "TcpFullPkt");
    break;
  case EpmActionType::TcpEmptyAck:
    strcpy(actionName, "TcpEmptyAck");
    break;
  case EpmActionType::Igmp:
    strcpy(actionName, "Igmp");
    break;
  case EpmActionType::SqfFire:
    strcpy(actionName, "SqfFire");
    break;
  case EpmActionType::SqfCancel:
    strcpy(actionName, "SqfCancel");
    break;
  case EpmActionType::BoeFire:
    strcpy(actionName, "BoeFire");
    break;
  case EpmActionType::BoeCancel:
    strcpy(actionName, "BoeCancel");
    break;
  case EpmActionType::CmeHwCancel:
    strcpy(actionName, "CmeHwCancel");
    break;
  case EpmActionType::QEDHwPurge:
    strcpy(actionName, "QEDHwPurge");
    break;
  case EpmActionType::CmeSwFire:
    strcpy(actionName, "CmeSwFire");
    break;
  case EpmActionType::CmeSwHeartbeat:
    strcpy(actionName, "CmeSwHeartbeat");
    break;
  case EpmActionType::ItchHwFastSweep:
    strcpy(actionName, "ItchHwFastSweep");
    break;
  case EpmActionType::HwFireAction:
    strcpy(actionName, "HwFire");
    break;
  case EpmActionType::UserAction:
    strcpy(actionName, "UserAction");
    break;
  default:
    on_error("Unexpected EkaEpmAction type %d", (int)type);
  }
  return 0;
}
/* ---------------------------------------------------- */
EkaEpm::FrameType EkaEpmAction::getFrameType() {
  switch (type) {
  case EpmActionType::INVALID:
    on_error("Invalid ActionType");
  case EpmActionType::ItchHwFastSweep:
    return EkaEpm::FrameType::Udp;
  case EpmActionType::Igmp:
    return EkaEpm::FrameType::Ip;
  default:
    return EkaEpm::FrameType::Tcp;
  }
}
/* ---------------------------------------------------- */
bool EkaEpmAction::isTcp() {
  return getFrameType() == EkaEpm::FrameType::Tcp;
}
/* ---------------------------------------------------- */
bool EkaEpmAction::isUdp() {
  return getFrameType() == EkaEpm::FrameType::Udp;
}

/* ---------------------------------------------------- */
size_t EkaEpmAction::getPayloadOffset() {
  EkaEpm::FrameType frameType = getFrameType();

  switch (frameType) {
  case EkaEpm::FrameType::Mac:
    return sizeof(EkaEthHdr);
  case EkaEpm::FrameType::Ip:
    return sizeof(EkaEthHdr) + sizeof(EkaIpHdr);
  case EkaEpm::FrameType::Tcp:
    return sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
           sizeof(EkaTcpHdr);
  case EkaEpm::FrameType::Udp:
    return sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
           sizeof(EkaUdpHdr);
  default:
    on_error("Unexpected frameType %d", (int)frameType);
  }
}
/* ---------------------------------------------------- */
uint16_t EkaEpmAction::getL3L4len() {
  EkaEpm::FrameType frameType = getFrameType();

  switch (frameType) {
  case EkaEpm::FrameType::Mac:
    return (uint16_t)0;
  case EkaEpm::FrameType::Ip:
    return (uint16_t)sizeof(EkaIpHdr);
  case EkaEpm::FrameType::Tcp:
    return (uint16_t)(sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));
  case EkaEpm::FrameType::Udp:
    return (uint16_t)(sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));
  default:
    on_error("Unexpected frameType %d", (int)frameType);
  }
}
/* ---------------------------------------------------- */

EkaEpmAction::EkaEpmAction(EkaDev *_dev,
                           EkaEpm::ActionType _type,
                           uint _idx, uint _localIdx,
                           uint _region, uint8_t _coreId,
                           uint8_t _sessId, uint _auxIdx) {

  dev = _dev;
  if (!dev)
    on_error("!dev");
  initialized = false;

  epm = dev->epm;
  if (!epm)
    on_error("!epm");

  snDevWCPtr = dev->snDevWCPtr;
  if (!snDevWCPtr)
    on_error("!snDevWCPtr");

  type = _type;
  idx = _idx;
  localIdx = _localIdx;
  region = _region;
  coreId = _coreId;
  sessId = _sessId;
  productIdx = _auxIdx;
  actionAddr = EpmActionBase + idx * ActionBudget;

  heapOffs =
      EkaEpmRegion::getActionHeapOffs(region, localIdx);

  heapAddr = heapOffs;

  ethHdr = (EkaEthHdr *)&epm->heap[heapOffs];
  ipHdr =
      (EkaIpHdr *)((uint8_t *)ethHdr + sizeof(EkaEthHdr));
  tcpHdr =
      (EkaTcpHdr *)((uint8_t *)ipHdr + sizeof(EkaIpHdr));
  udpHdr =
      (EkaUdpHdr *)((uint8_t *)ipHdr + sizeof(EkaIpHdr));

  memset(&epm->heap[heapOffs], 0, getPayloadOffset());

  payload = &epm->heap[heapOffs] + getPayloadOffset();

  initEpmActionLocalCopy();
  setTemplate();
  setActionBitmap();
  setHwAction();
  setIpTtl();
  setName();
  //  print("From constructor");
}
/* ----------------------------------------------------- */
int EkaEpmAction::initEpmActionLocalCopy() {
  epmActionLocalCopy.nextAction = EPM_LAST_ACTION;
  epmActionLocalCopy.enable = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy.postStratMask = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy.postLocalMask = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy.actionFlags = AF_Valid;
  return 0;
}

/* ----------------------------------------------------- */
int EkaEpmAction::setUdpMcNwHdrs(
    uint8_t *macSa, uint32_t srcIp, uint32_t dstIp,
    uint16_t srcPort, uint16_t dstPort, uint payloadSize) {
  ethHdr = (EkaEthHdr *)&dev->epm->heap[heapOffs];
  ipHdr =
      (EkaIpHdr *)((uint8_t *)ethHdr + sizeof(EkaEthHdr));
  udpHdr =
      (EkaUdpHdr *)((uint8_t *)ipHdr + sizeof(EkaIpHdr));
  payload = (uint8_t *)tcpHdr + sizeof(EkaUdpHdr);

  memset(ethHdr, 0,
         sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
             sizeof(EkaUdpHdr));

  uint8_t macDa[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
  macDa[3] = ((uint8_t *)&dstIp)[1] & 0x7F;
  macDa[4] = ((uint8_t *)&dstIp)[2];
  macDa[5] = ((uint8_t *)&dstIp)[3];

  memcpy(&(ethHdr->dest), macDa, 6);
  memcpy(&(ethHdr->src), macSa, 6);

  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl = 0x45;
  ipHdr->_tos = 0;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl = 64;
  ipHdr->_proto = EKA_PROTO_UDP;
  ipHdr->_chksum = 0;
  ipHdr->_id = 0;

  ipHdr->src = srcIp;
  ipHdr->dest = dstIp;

  ipHdr->_len = be16toh(getL3L4len() + payloadSize);
  ipHdr->_chksum =
      csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  udpHdr->src = be16toh(srcPort);
  udpHdr->dest = be16toh(dstPort);
  udpHdr->len = be16toh(sizeof(EkaUdpHdr) + payloadSize);
  udpHdr->chksum = 0;

  payloadLen = payloadSize;

  pktSize = getPayloadOffset() + payloadLen;

  memset(payload, 0, payloadLen);
  //  TEST_LOG("im here");
  //  copyHeap2Fpga();

  return 0;
}

/* ----------------------------------------------------- */
int EkaEpmAction::setNwHdrs(uint8_t *macDa, uint8_t *macSa,
                            uint32_t srcIp, uint32_t dstIp,
                            uint16_t srcPort,
                            uint16_t dstPort) {
  ethHdr = (EkaEthHdr *)&dev->epm->heap[heapOffs];
  ipHdr =
      (EkaIpHdr *)((uint8_t *)ethHdr + sizeof(EkaEthHdr));
  tcpHdr =
      (EkaTcpHdr *)((uint8_t *)ipHdr + sizeof(EkaIpHdr));
  memset(&epm->heap[heapOffs], 0, getPayloadOffset());
  payload = &epm->heap[heapOffs] + getPayloadOffset();

  memcpy(&(ethHdr->dest), macDa, 6);
  memcpy(&(ethHdr->src), macSa, 6);
  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl = 0x45;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl = 64;

  ipHdr->_proto = isUdp() ? EKA_PROTO_UDP : EKA_PROTO_TCP;

  ipHdr->_chksum = 0;
  ipHdr->_id = 0;
  //  ipHdr->_len    = 0;
  ipHdr->src = srcIp;
  ipHdr->dest = dstIp;

  ipHdr->_len = be16toh(getL3L4len() + payloadLen);

  if (isUdp()) {
    udpHdr->src = be16toh(srcPort);
    udpHdr->dest = be16toh(dstPort);
    udpHdr->len = be16toh(sizeof(EkaUdpHdr) + payloadLen);
  }

  if (isTcp()) {
    tcpHdr->src = be16toh(srcPort);
    tcpHdr->dest = be16toh(dstPort);
    tcpHdr->_hdrlen_rsvd_flags =
        be16toh(0x5000 | EKA_TCP_FLG_PSH | EKA_TCP_FLG_ACK);
    tcpHdr->urgp = 0;
  }

  setIpTtl();

  ipHdr->_chksum = 0;
  ipHdr->_chksum =
      csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  pktSize = getPayloadOffset() + payloadLen;

  tcpCSum = ekaPseudoTcpCsum(ipHdr, tcpHdr);
  //---------------------------------------------------------
  //  hexDump("setNwHdrs",&epm->heap[heapOffs],pktSize);

  //  TEST_LOG("im here");

  //  copyHeap2Fpga();

  return 0;
}

/* ----------------------------------------------------- */
void EkaEpmAction::setTcpSess(EkaTcpSess *tcpSess) {
  mySess = tcpSess;
}
/* ----------------------------------------------------- */

int EkaEpmAction::updateAttrs(uint8_t _coreId,
                              uint8_t _sessId,
                              const EpmAction *epmAction) {
  if (!epmAction)
    on_error("!epmAction");
  type = epmAction->type;

  if (epmAction->offset < getPayloadOffset())
    on_error(
        "epmAction->offset %d < getPayloadOffset() %ju",
        epmAction->offset, getPayloadOffset());

  memcpy(&epmActionLocalCopy, epmAction,
         sizeof(epmActionLocalCopy));

  setActionBitmap();
  setTemplate();
  setName();

  heapOffs = epmAction->offset - getPayloadOffset();
  if (heapOffs % 32 != 0)
    on_error("heapOffs %d (must be X32)", heapOffs);

  heapAddr = heapOffs;
  coreId = _coreId;
  sessId = _sessId;

  /* -----------------------------------------------------
   */
  ethHdr = (EkaEthHdr *)&dev->epm->heap[heapOffs];
  ipHdr =
      (EkaIpHdr *)((uint8_t *)ethHdr + sizeof(EkaEthHdr));
  tcpHdr =
      (EkaTcpHdr *)((uint8_t *)ipHdr + sizeof(EkaIpHdr));
  payload = &epm->heap[heapOffs] + getPayloadOffset();

  /* -----------------------------------------------------
   */

  if (!dev->core[coreId])
    on_error("!dev->core[%u]", coreId);

  if (isUdp()) { // UDP
    EkaUdpTxSess *udpSess =
        dev->core[coreId]->udpTxSess[sessId];
    if (!udpSess)
      on_error("!dev->core[%u]->udpTxSess[%u]", coreId,
               sessId);
    setNwHdrs(udpSess->macDa_, udpSess->macSa_,
              udpSess->srcIp_, udpSess->dstIp_,
              udpSess->srcPort_, udpSess->dstPort_);
  } else {
    EkaTcpSess *sess = dev->core[coreId]->tcpSess[sessId];
    if (!sess)
      on_error("!dev->core[%u]->tcpSess[%u]", coreId,
               sessId);
    setNwHdrs(sess->macDa, sess->macSa, sess->srcIp,
              sess->dstIp, sess->srcPort, sess->dstPort);
  }

  /* -----------------------------------------------------
   */
  payloadLen = epmAction->length;
  pktSize = getPayloadOffset() + payloadLen;

  updatePayload();

  return 0;
}
/* ----------------------------------------------------- */

void EkaEpmAction::updatePayload() {
  ipHdr->_len = be16toh(getL3L4len() + payloadLen);

  ipHdr->_chksum = 0;
  ipHdr->_chksum =
      csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  if (isUdpAction(type))
    udpHdr->len = be16toh(sizeof(EkaUdpHdr) + payloadLen);
  /* -----------------------------------------------------
   */

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);
  setIpTtl();

  tcpCSum = ekaPseudoTcpCsum(ipHdr, tcpHdr);

  setHwAction();

  // write to scratchpad
  copyBuf2Hw(dev, EkaEpm::EpmActionBase,
             (uint64_t *)&hwAction, sizeof(hwAction));
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,
                         0, idx, 0);

  /* hexDump("updateAttrs",&epm->heap[heapOffs],pktSize); */
  //  TEST_LOG("im here");
  copyHeap2Fpga();
}
/* ----------------------------------------------------- */

void EkaEpmAction::updateHwAction() { setHwAction(); }
/* ----------------------------------------------------- */

// used for IGMP, Empty Ack
int EkaEpmAction::preloadFullPkt(const void *buf,
                                 uint len) {
  pktSize = len;
  tcpCSum = 0;

  hwAction.payloadSize = pktSize;
  hwAction.tcpCSum = tcpCSum;

  memcpy(&epm->heap[heapOffs], buf, pktSize);
  copyHeap2Fpga();

  return 0;
}
/* ----------------------------------------------------- */
// used for LWIP re-transmit
int EkaEpmAction::sendFullPkt(const void *buf, uint len) {
  pktSize = len;
  tcpCSum = 0;

  memcpy(&epm->heap[heapOffs], buf, pktSize);
  copyHeap2FpgaAndSend();

  return 0;
}
/* ----------------------------------------------------- */
// For TCP fastSend() only! (excSend() and efcCmeSend() )
int EkaEpmAction::setPktPayloadAndSendWC(const void *buf,
                                         uint len) {
  if (!buf)
    on_error("!buf");

  ipHdr->_len = be16toh(getL3L4len() + len);
  pktSize = getPayloadOffset() + len;

  ipHdr->_chksum = 0;
  ipHdr->_chksum =
      csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  setIpTtl();

  memcpy(&epm->heap[heapOffs + getPayloadOffset()], buf,
         len);

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  tcpCSum = ekaPseudoTcpCsum(ipHdr, tcpHdr);

  copyHeap2FpgaAndSend();

  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::setUdpPktPayload(const void *buf,
                                   uint len) {
  if (!buf)
    on_error("!buf");

  payloadLen = len;
  pktSize = getPayloadOffset() + payloadLen;

  ipHdr->_len = be16toh(getL3L4len() + payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum =
      csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  memcpy(&epm->heap[heapOffs + getPayloadOffset()], buf,
         len);

  copyHeap2Fpga();

  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::send(uint32_t _tcpCSum) {
  epm_trig_desc_t epm_trig_desc = {};

  epm_trig_desc.str.action_index = localIdx;
  epm_trig_desc.str.size = pktSize;
  epm_trig_desc.str.tcp_cs = _tcpCSum;
  epm_trig_desc.str.region = region;

#if 0
  char hexDumpstr[8000] = {};
  hexDump2str("EkaEpmAction::send() pkt",&epm->heap[heapOffs],pktSize,
	      hexDumpstr, sizeof(hexDumpstr));
  EKA_LOG("%s: action_index = %u,region=%u,size=%u, heapOffs=0x%x, heapAddr=0x%jx:\n%s ",
	  actionName,
	  epm_trig_desc.str.action_index,
	  epm_trig_desc.str.region,
	  epm_trig_desc.str.size,
	  heapOffs,
	  heapAddr,
	  hexDumpstr
	  );
#endif

  eka_write(dev, EPM_TRIGGER_DESC_ADDR, epm_trig_desc.desc);

  //  print("EkaEpmAction::send");
  //  return pktSize;
  return payloadLen;
}
/* ----------------------------------------------------- */

int EkaEpmAction::send() { return send(tcpCSum); }
/* ----------------------------------------------------- */
int EkaEpmAction::fastSend(const void *buf, uint len) {
  if (type != EkaEpm::ActionType::TcpFastPath) {
    if (!mySess)
      on_error("type=%d: TCP session is not set",
               (int)type);
    int rc = mySess->preSendCheck(len);
    if (rc <= 0)
      return rc;
    if (rc != (int)len)
      on_error("rc %d != len %u", rc, len);
  }
  setPktPayloadAndSendWC(buf, len);
  return len;
}

/* ----------------------------------------------------- */
void EkaEpmAction::print(const char *msg) {
  EKA_LOG("%s: %s, region=%u, idx=%u, localIdx=%u, "
          "heapOffs=0x%x, heapAddr=0x%jx,  "
          "actionAddr=0x%jx, pktSize=%u,  ",
          msg, actionName, region, idx, localIdx, heapOffs,
          heapAddr, actionAddr, pktSize);
}
/* ----------------------------------------------------- */
