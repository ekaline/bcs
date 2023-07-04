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

extern EkaDev *g_ekaDev;

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

  if (epmActionLocalCopy_.actionFlags == AF_Valid)
    actionBitParams_.bitmap.action_valid = 1;
  else
    actionBitParams_.bitmap.action_valid = 0;

  actionBitParams_.bitmap.originatedFromHw = 0;

  switch (type_) {
  case EpmActionType::TcpFastPath:
    actionBitParams_.bitmap.israw = 0;
    actionBitParams_.bitmap.report_en = 0;
    actionBitParams_.bitmap.feedbck_en = 1;
    break;
  case EpmActionType::TcpFullPkt:
    actionBitParams_.bitmap.israw = 1;
    actionBitParams_.bitmap.report_en = 0;
    actionBitParams_.bitmap.feedbck_en = 0;
    break;
  case EpmActionType::TcpEmptyAck:
    actionBitParams_.bitmap.israw = 0;
    actionBitParams_.bitmap.report_en = 0;
    actionBitParams_.bitmap.feedbck_en = 0;
    break;
  case EpmActionType::Igmp:
    actionBitParams_.bitmap.israw = 1;
    actionBitParams_.bitmap.report_en = 0;
    actionBitParams_.bitmap.feedbck_en = 0;
    break;
  case EpmActionType::CmeSwFire:
    actionBitParams_.bitmap.app_seq_inc = 1;
    actionBitParams_.bitmap.israw = 0;
    actionBitParams_.bitmap.report_en = 0;
    actionBitParams_.bitmap.feedbck_en = 1;
    break;
  case EpmActionType::BoeFire:
  case EpmActionType::CmeHwCancel:
  case EpmActionType::QEDHwPurge:
  case EpmActionType::SqfFire:
    actionBitParams_.bitmap.originatedFromHw = 1;
    [[fallthrough]];
  case EpmActionType::HwFireAction:
    actionBitParams_.bitmap.app_seq_inc = 1;
    [[fallthrough]];
  case EpmActionType::BoeCancel:
  case EpmActionType::SqfCancel:

  case EpmActionType::CmeSwHeartbeat:

    actionBitParams_.bitmap.israw = 0;
    actionBitParams_.bitmap.report_en = 1;
    actionBitParams_.bitmap.feedbck_en = 1;
    break;
  case EpmActionType::ItchHwFastSweep:
    actionBitParams_.bitmap.israw = 1;
    actionBitParams_.bitmap.report_en = 1;
    actionBitParams_.bitmap.feedbck_en = 0;
    break;

  case EpmActionType::UserAction:
    actionBitParams_.bitmap.israw = 0;
    actionBitParams_.bitmap.report_en = 1;
    actionBitParams_.bitmap.feedbck_en = 1;
    break;
  default:
    on_error("Unexpected EkaEpmAction type_ %d",
             (int)type_);
  }
  return 0;
}
/* ---------------------------------------------------- */

void EkaEpmAction::setIpTtl() {
  switch (type_) {
  case EpmActionType::UserAction:
  case EpmActionType::HwFireAction:
  case EpmActionType::BoeFire:
  case EpmActionType::BoeCancel:
  case EpmActionType::SqfFire:
  case EpmActionType::SqfCancel:
  case EpmActionType::CmeHwCancel:
  case EpmActionType::QEDHwPurge:
  case EpmActionType::ItchHwFastSweep:
    ipHdr_->_ttl = EFC_HW_TTL;
    ipHdr_->_id = EFC_HW_ID;
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
  hwAction_.bit_params = actionBitParams_;
  hwAction_.tcpcs_template_db_ptr = epmTemplate_->id;
  hwAction_.template_db_ptr =
      epmTemplate_->getDataTemplateAddr();
  hwAction_.data_db_ptr = heapOffs_;
  hwAction_.target_prod_id = productIdx_;
  hwAction_.target_core_id = coreId_;
  hwAction_.target_session_id = sessId_;
  hwAction_.next_action_index =
      epmActionLocalCopy_.nextAction;
  hwAction_.mask_post_strat =
      epmActionLocalCopy_.postStratMask;
  hwAction_.mask_post_local =
      epmActionLocalCopy_.postLocalMask;
  hwAction_.enable_bitmap = epmActionLocalCopy_.enable;
  hwAction_.user = epmActionLocalCopy_.user;
  hwAction_.token = epmActionLocalCopy_.token;
  hwAction_.tcpCSum = tcpCSum_;
  hwAction_.payloadSize = pktSize_;
  hwAction_.tcpCsSizeSource = setTcpCsSizeSource(type_);

  copyBuf2Hw(dev_, EkaEpm::EpmActionBase,
             (uint64_t *)&hwAction_,
             sizeof(hwAction_)); // write to scratchpad
  atomicIndirectBufWrite(dev_, 0xf0238 /* ActionAddr */, 0,
                         0, globalIdx(), 0);

  return 0;
}
/* ---------------------------------------------------- */
void EkaEpmAction::printHeap() {
  EKA_LOG("heapOffs_ = %u 0x%x", heapOffs_, heapOffs_);
  EKA_LOG("epmRegion = %u", regionId_);
  hexDump("Heap", &epm_->heap[heapOffs_], pktSize_);
}
/* ---------------------------------------------------- */
void EkaEpmAction::printHwAction() {
  //  EKA_LOG("hwAction_.bit_params=0x%x",
  //  (uint)hwAction_.bit_params);
  EKA_LOG("hwAction_.tcpcs_template_db_ptr =0x%x",
          hwAction_.tcpcs_template_db_ptr);
  EKA_LOG("hwAction_.template_db_ptr=0x%x",
          hwAction_.template_db_ptr);
  EKA_LOG("hwAction_.data_db_ptr=0x%x",
          hwAction_.data_db_ptr);
  EKA_LOG("hwAction_.target_prod_id=0x%x",
          hwAction_.target_prod_id);
  EKA_LOG("hwAction_.target_core_id=0x%x",
          hwAction_.target_core_id);
  EKA_LOG("hwAction_.target_session_id=0x%x",
          hwAction_.target_session_id);
  EKA_LOG("hwAction_.next_action_index=0x%x",
          hwAction_.next_action_index);
  EKA_LOG("hwAction_.mask_post_strat=0x%jx",
          hwAction_.mask_post_strat);
  EKA_LOG("hwAction_.mask_post_local=0x%jx",
          hwAction_.mask_post_local);
  EKA_LOG("hwAction_.enable_bitmap=0x%jx",
          hwAction_.enable_bitmap);
  EKA_LOG("hwAction_.user=0x%jx", hwAction_.user);
  EKA_LOG("hwAction_.token=0x%jx", hwAction_.token);
  EKA_LOG("hwAction_.tcpCSum=0x%x", hwAction_.tcpCSum);
  EKA_LOG("hwAction_.payloadSize=0x%x",
          hwAction_.payloadSize);
  EKA_LOG("hwAction_.tcpCsSizeSource=0x%x",
          (uint)hwAction_.tcpCsSizeSource);
}

/* ---------------------------------------------------- */
int EkaEpmAction::setTemplate() {
  switch (type_) {
  case EpmActionType::TcpFastPath:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::TcpFastPath];
    break;
  case EpmActionType::TcpFullPkt:
    epmTemplate_ =
        epm_->epmTemplate[(int)EkaEpm::TemplateId::Raw];
    break;
  case EpmActionType::TcpEmptyAck:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::TcpFastPath];
    break;
  case EpmActionType::Igmp:
    epmTemplate_ =
        epm_->epmTemplate[(int)EkaEpm::TemplateId::Raw];
    break;
  case EpmActionType::SqfFire:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::SqfHwFire];
    break;
  case EpmActionType::SqfCancel:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::SqfSwCancel];
    break;
  case EpmActionType::BoeFire:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::BoeQuoteUpdateShort];
    break;
  case EpmActionType::BoeCancel:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::BoeCancel];
    break;
  case EpmActionType::CmeHwCancel:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::CmeHwCancel];
    break;
  case EpmActionType::ItchHwFastSweep:
    epmTemplate_ =
        epm_->epmTemplate[(int)EkaEpm::TemplateId::Raw];
    break;
  case EpmActionType::CmeSwFire:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::CmeSwFire];
    break;
  case EpmActionType::CmeSwHeartbeat:
    epmTemplate_ =
        epm_->epmTemplate[(int)EkaEpm::TemplateId::CmeSwHb];
    break;
  case EpmActionType::QEDHwPurge:
    epmTemplate_ = epm_->epmTemplate[(
        int)EkaEpm::TemplateId::QedHwFire];
    break;
  case EpmActionType::UserAction:
    epm_->epmTemplate[(int)EkaEpm::TemplateId::TcpFastPath];
    break;
  default:
    on_error("Unexpected EkaEpmAction type_ %d",
             (int)type_);
  }
  if (epmTemplate_ == NULL)
    on_error("type_ %d: epmTemplate_ == NULL ", (int)type_);
  //  EKA_LOG("Teplate set to: \'%s\'",epmTemplate_->name);
  return 0;
}

/* ---------------------------------------------------- */
int EkaEpmAction::setName() {
  name_ = std::string(printActionType(type_));
  return 0;
}
/* ---------------------------------------------------- */
EkaEpm::FrameType EkaEpmAction::getFrameType() {
  switch (type_) {
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

EkaEpmAction::EkaEpmAction(EkaEpm::ActionType type,
                           uint localIdx, uint regionId) {

  dev_ = g_ekaDev;
  if (!dev_)
    on_error("!dev_");
  initialized_ = false;

  epm_ = dev_->epm;
  if (!epm_)
    on_error("!epm_");

  snDevWCPtr_ = dev_->snDevWCPtr;
  if (!snDevWCPtr_)
    on_error("!snDevWCPtr_");

  type_ = type;
  idx_ = localIdx;
  regionId_ = regionId;
  coreId_ = -1;
  sessId_ = -1;
  productIdx_ = -1;

  heapOffs_ =
      EkaEpmRegion::getActionHeapOffs(regionId_, idx_);

  ethHdr_ = (EkaEthHdr *)&epm_->heap[heapOffs_];
  ipHdr_ =
      (EkaIpHdr *)((uint8_t *)ethHdr_ + sizeof(EkaEthHdr));
  tcpHdr_ =
      (EkaTcpHdr *)((uint8_t *)ipHdr_ + sizeof(EkaIpHdr));
  udpHdr_ =
      (EkaUdpHdr *)((uint8_t *)ipHdr_ + sizeof(EkaIpHdr));

  memset(&epm_->heap[heapOffs_], 0, getPayloadOffset());

  payload_ = &epm_->heap[heapOffs_] + getPayloadOffset();

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
  epmActionLocalCopy_.nextAction = EPM_LAST_ACTION;
  epmActionLocalCopy_.enable = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy_.postStratMask = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy_.postLocalMask = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy_.actionFlags = AF_Valid;
  return 0;
}

/* ----------------------------------------------------- */
int EkaEpmAction::setUdpMcNwHdrs(
    uint8_t *macSa, uint32_t srcIp, uint32_t dstIp,
    uint16_t srcPort, uint16_t dstPort, uint payloadSize) {
  ethHdr_ = (EkaEthHdr *)&epm_->heap[heapOffs_];
  ipHdr_ =
      (EkaIpHdr *)((uint8_t *)ethHdr_ + sizeof(EkaEthHdr));
  udpHdr_ =
      (EkaUdpHdr *)((uint8_t *)ipHdr_ + sizeof(EkaIpHdr));
  payload_ = (uint8_t *)tcpHdr_ + sizeof(EkaUdpHdr);

  memset(ethHdr_, 0,
         sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
             sizeof(EkaUdpHdr));

  uint8_t macDa[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
  macDa[3] = ((uint8_t *)&dstIp)[1] & 0x7F;
  macDa[4] = ((uint8_t *)&dstIp)[2];
  macDa[5] = ((uint8_t *)&dstIp)[3];

  memcpy(&(ethHdr_->dest), macDa, 6);
  memcpy(&(ethHdr_->src), macSa, 6);

  ethHdr_->type = be16toh(0x0800);

  ipHdr_->_v_hl = 0x45;
  ipHdr_->_tos = 0;
  ipHdr_->_offset = 0x0040;
  ipHdr_->_ttl = 64;
  ipHdr_->_proto = EKA_PROTO_UDP;
  ipHdr_->_chksum = 0;
  ipHdr_->_id = 0;

  ipHdr_->src = srcIp;
  ipHdr_->dest = dstIp;

  ipHdr_->_len = be16toh(getL3L4len() + payloadSize);
  ipHdr_->_chksum =
      csum((unsigned short *)ipHdr_, sizeof(EkaIpHdr));

  udpHdr_->src = be16toh(srcPort);
  udpHdr_->dest = be16toh(dstPort);
  udpHdr_->len = be16toh(sizeof(EkaUdpHdr) + payloadSize);
  udpHdr_->chksum = 0;

  payloadLen_ = payloadSize;

  pktSize_ = getPayloadOffset() + payloadLen_;

  memset(payload_, 0, payloadLen_);
  //  TEST_LOG("im here");
  //  copyHeap2Fpga();

  return 0;
}

/* ----------------------------------------------------- */
int EkaEpmAction::setNwHdrs(uint8_t *macDa, uint8_t *macSa,
                            uint32_t srcIp, uint32_t dstIp,
                            uint16_t srcPort,
                            uint16_t dstPort) {
  ethHdr_ = (EkaEthHdr *)&epm_->heap[heapOffs_];
  ipHdr_ =
      (EkaIpHdr *)((uint8_t *)ethHdr_ + sizeof(EkaEthHdr));
  tcpHdr_ =
      (EkaTcpHdr *)((uint8_t *)ipHdr_ + sizeof(EkaIpHdr));
  memset(&epm_->heap[heapOffs_], 0, getPayloadOffset());
  payload_ = &epm_->heap[heapOffs_] + getPayloadOffset();

  memcpy(&(ethHdr_->dest), macDa, 6);
  memcpy(&(ethHdr_->src), macSa, 6);
  ethHdr_->type = be16toh(0x0800);

  ipHdr_->_v_hl = 0x45;
  ipHdr_->_offset = 0x0040;
  ipHdr_->_ttl = 64;

  ipHdr_->_proto = isUdp() ? EKA_PROTO_UDP : EKA_PROTO_TCP;

  ipHdr_->_chksum = 0;
  ipHdr_->_id = 0;
  //  ipHdr_->_len    = 0;
  ipHdr_->src = srcIp;
  ipHdr_->dest = dstIp;

  ipHdr_->_len = be16toh(getL3L4len() + payloadLen_);

  if (isUdp()) {
    udpHdr_->src = be16toh(srcPort);
    udpHdr_->dest = be16toh(dstPort);
    udpHdr_->len = be16toh(sizeof(EkaUdpHdr) + payloadLen_);
  }

  if (isTcp()) {
    tcpHdr_->src = be16toh(srcPort);
    tcpHdr_->dest = be16toh(dstPort);
    tcpHdr_->_hdrlen_rsvd_flags =
        be16toh(0x5000 | EKA_TCP_FLG_PSH | EKA_TCP_FLG_ACK);
    tcpHdr_->urgp = 0;
  }

  setIpTtl();

  ipHdr_->_chksum = 0;
  ipHdr_->_chksum =
      csum((unsigned short *)ipHdr_, sizeof(EkaIpHdr));

  pktSize_ = getPayloadOffset() + payloadLen_;

  tcpCSum_ = ekaPseudoTcpCsum(ipHdr_, tcpHdr_);
  //---------------------------------------------------------
  //  hexDump("setNwHdrs",&epm_->heap[heapOffs_],pktSize_);

  //  TEST_LOG("im here");

  //  copyHeap2Fpga();

  return 0;
}
/* ----------------------------------------------------- */
uint64_t EkaEpmAction::actionAddr() {
  return EkaEpm::EpmActionBase +
         idx_ * EkaEpm::ActionBudget;
}

/* ----------------------------------------------------- */
void EkaEpmAction::setTcpSess(EkaTcpSess *tcpSess) {
  if (!tcpSess)
    on_error("!tcpSess");
  tcpSess_ = tcpSess;
  coreId_ = tcpSess->coreId;
  sessId_ = tcpSess->sessId;

  setNwHdrs(tcpSess->macDa, tcpSess->macSa, tcpSess->srcIp,
            tcpSess->dstIp, tcpSess->srcPort,
            tcpSess->dstPort);
  setHwAction();
}
/* ----------------------------------------------------- */
void EkaEpmAction::setUdpSess(EkaUdpSess *udpSess) {
  udpSess_ = udpSess;
  setHwAction();
}
/* ----------------------------------------------------- */
void EkaEpmAction::setCoreId(EkaCoreId coreId) {
  coreId_ = coreId;
  setHwAction();
}
/* ----------------------------------------------------- */
void EkaEpmAction::setNextAction(
    epm_actionid_t nextActionIdx) {
  epmActionLocalCopy_.nextAction = nextActionIdx;
  setHwAction();
}

/* ----------------------------------------------------- */

int EkaEpmAction::updateAttrs(uint8_t _coreId,
                              uint8_t _sessId,
                              const EpmAction *epmAction) {
  if (!epmAction)
    on_error("!epmAction");
  type_ = epmAction->type;

  if (epmAction->offset < getPayloadOffset())
    on_error(
        "epmAction->offset %d < getPayloadOffset() %ju",
        epmAction->offset, getPayloadOffset());

  memcpy(&epmActionLocalCopy_, epmAction,
         sizeof(epmActionLocalCopy_));

  setActionBitmap();
  setTemplate();
  setName();

  heapOffs_ = epmAction->offset - getPayloadOffset();
  if (heapOffs_ % 32 != 0)
    on_error("heapOffs_ %d (must be X32)", heapOffs_);

  coreId_ = _coreId;
  sessId_ = _sessId;

  /* -----------------------------------------------------
   */
  ethHdr_ = (EkaEthHdr *)&epm_->heap[heapOffs_];
  ipHdr_ =
      (EkaIpHdr *)((uint8_t *)ethHdr_ + sizeof(EkaEthHdr));
  tcpHdr_ =
      (EkaTcpHdr *)((uint8_t *)ipHdr_ + sizeof(EkaIpHdr));
  payload_ = &epm_->heap[heapOffs_] + getPayloadOffset();

  /* -----------------------------------------------------
   */

  if (!dev_->core[coreId_])
    on_error("!dev_->core[%u]", coreId_);

  if (isUdp()) { // UDP
    EkaUdpTxSess *udpSess =
        dev_->core[coreId_]->udpTxSess[sessId_];
    if (!udpSess)
      on_error("!dev_->core[%u]->udpTxSess[%u]", coreId_,
               sessId_);
    setNwHdrs(udpSess->macDa_, udpSess->macSa_,
              udpSess->srcIp_, udpSess->dstIp_,
              udpSess->srcPort_, udpSess->dstPort_);
  } else {
    EkaTcpSess *sess =
        dev_->core[coreId_]->tcpSess[sessId_];
    if (!sess)
      on_error("!dev_->core[%u]->tcpSess[%u]", coreId_,
               sessId_);
    setNwHdrs(sess->macDa, sess->macSa, sess->srcIp,
              sess->dstIp, sess->srcPort, sess->dstPort);
  }

  /* -----------------------------------------------------
   */
  payloadLen_ = epmAction->length;
  pktSize_ = getPayloadOffset() + payloadLen_;

  updatePayload();

  return 0;
}
/* ----------------------------------------------------- */

void EkaEpmAction::setPayload(const void *buf, size_t len) {
  payload_ = &epm_->heap[heapOffs_ + getPayloadOffset()];
  payloadLen_ = len;
  memcpy(payload_, buf, payloadLen_);
  pktSize_ = getPayloadOffset() + payloadLen_;
  updatePayload();
}

/* ----------------------------------------------------- */

void EkaEpmAction::updatePayload() {
  ipHdr_->_len = be16toh(getL3L4len() + payloadLen_);

  ipHdr_->_chksum = 0;
  ipHdr_->_chksum =
      csum((unsigned short *)ipHdr_, sizeof(EkaIpHdr));

  if (isUdpAction(type_))
    udpHdr_->len = be16toh(sizeof(EkaUdpHdr) + payloadLen_);
  /* -----------------------------------------------------
   */

  epmTemplate_->clearHwFields(&epm_->heap[heapOffs_]);
  setIpTtl();

  tcpCSum_ = ekaPseudoTcpCsum(ipHdr_, tcpHdr_);

  setHwAction();

  /* hexDump("updateAttrs",&epm_->heap[heapOffs_],
              pktSize_); */
  copyHeap2Fpga();
}
/* ----------------------------------------------------- */

// used for IGMP, Empty Ack
int EkaEpmAction::preloadFullPkt(const void *buf,
                                 uint len) {
  pktSize_ = len;
  tcpCSum_ = 0;

  hwAction_.payloadSize = pktSize_;
  hwAction_.tcpCSum = tcpCSum_;

  memcpy(&epm_->heap[heapOffs_], buf, pktSize_);
  copyHeap2Fpga();

  return 0;
}
/* ----------------------------------------------------- */
// used for LWIP re-transmit
int EkaEpmAction::sendFullPkt(const void *buf, uint len) {
  pktSize_ = len;
  tcpCSum_ = 0;

  memcpy(&epm_->heap[heapOffs_], buf, pktSize_);
  copyHeap2FpgaAndSend();

  return 0;
}
/* ----------------------------------------------------- */
// For TCP fastSend() only! (excSend() and efcCmeSend() )
int EkaEpmAction::setPktPayloadAndSendWC(const void *buf,
                                         uint len) {
  if (!buf)
    on_error("!buf");

  ipHdr_->_len = be16toh(getL3L4len() + len);
  pktSize_ = getPayloadOffset() + len;

  ipHdr_->_chksum = 0;
  ipHdr_->_chksum =
      csum((unsigned short *)ipHdr_, sizeof(EkaIpHdr));

  setIpTtl();

  memcpy(&epm_->heap[heapOffs_ + getPayloadOffset()], buf,
         len);

  epmTemplate_->clearHwFields(&epm_->heap[heapOffs_]);

  tcpCSum_ = ekaPseudoTcpCsum(ipHdr_, tcpHdr_);

  copyHeap2FpgaAndSend();

  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::setUdpPktPayload(const void *buf,
                                   uint len) {
  if (!buf)
    on_error("!buf");

  payloadLen_ = len;
  pktSize_ = getPayloadOffset() + payloadLen_;

  ipHdr_->_len = be16toh(getL3L4len() + payloadLen_);
  ipHdr_->_chksum = 0;
  ipHdr_->_chksum =
      csum((unsigned short *)ipHdr_, sizeof(EkaIpHdr));

  memcpy(&epm_->heap[heapOffs_ + getPayloadOffset()], buf,
         len);

  copyHeap2Fpga();

  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::send(uint32_t _tcpCSum) {
  epm_trig_desc_t epm_trig_desc = {};

  epm_trig_desc.str.action_index = idx_;
  epm_trig_desc.str.size = pktSize_;
  epm_trig_desc.str.tcp_cs = _tcpCSum;
  epm_trig_desc.str.region = regionId_;

#if 0
  char hexDumpstr[8000] = {};
  hexDump2str("EkaEpmAction::send() pkt",&epm_->heap[heapOffs_],pktSize_,
	      hexDumpstr, sizeof(hexDumpstr));
  EKA_LOG("%s: action_index = %u,region=%u,size=%u, heapOffs_=0x%x\n%s ",
	  actionName,
	  epm_trig_desc.str.action_index,
	  epm_trig_desc.str.region,
	  epm_trig_desc.str.size,
	  heapOffs_,
	  hexDumpstr
	  );
#endif

  eka_write(dev_, EPM_TRIGGER_DESC_ADDR,
            epm_trig_desc.desc);

  //  print("EkaEpmAction::send");
  //  return pktSize_;
  return payloadLen_;
}
/* ----------------------------------------------------- */

int EkaEpmAction::send() { return send(tcpCSum_); }
/* ----------------------------------------------------- */
int EkaEpmAction::fastSend(const void *buf, uint len) {
  if (type_ != EkaEpm::ActionType::TcpFastPath) {
    if (!tcpSess_)
      on_error("type_=%d: TCP session is not set",
               (int)type_);
    int rc = tcpSess_->preSendCheck(len);
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
  EKA_LOG("%s: %s, region=%u, idx=%u "
          "heapOffs_=0x%x,   "
          "actionAddr=0x%jx, pktSize_=%u,  ",
          msg, name_.c_str(), regionId_, idx_, heapOffs_,
          actionAddr(), pktSize_);
}
/* ----------------------------------------------------- */
