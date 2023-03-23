/* NE SHURIK */

#include "EkaEpmAction.h"
#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EpmTemplate.h"
#include "EkaEpm.h"
#include "EkaEpmRegion.h"
#include "EkaUdpTxSess.h"

uint32_t calc_pseudo_csum (const void* ip_hdr, const void* tcp_hdr,
			   const void* payload, uint16_t payload_size);
unsigned short csum(const unsigned short *ptr,int nbytes);
uint32_t calcUdpCsum (void* ip_hdr, void* udp_hdr, void* payload, uint16_t payload_size);
uint16_t udp_checksum(EkaUdpHdr *p_udp_header, size_t len, uint32_t src_addr, uint32_t dest_addr);

/* ---------------------------------------------------- */
int EkaEpmAction::setActionBitmap() {
  
  if (epmActionLocalCopy.actionFlags == AF_Valid) 
    actionBitParams.bitmap.action_valid = 1;
  else
    actionBitParams.bitmap.action_valid = 0;

  actionBitParams.bitmap.originatedFromHw = 0;

  switch (type) {
  case EpmActionType::TcpFastPath :
    actionBitParams.bitmap.israw        = 0;
    actionBitParams.bitmap.report_en    = 0;
    actionBitParams.bitmap.feedbck_en   = 1;
    break;
  case EpmActionType::TcpFullPkt  :
    actionBitParams.bitmap.israw        = 1;
    actionBitParams.bitmap.report_en    = 0;
    actionBitParams.bitmap.feedbck_en   = 0;
    break;
  case EpmActionType::TcpEmptyAck :
    actionBitParams.bitmap.israw        = 0;
    actionBitParams.bitmap.report_en    = 0;
    actionBitParams.bitmap.feedbck_en   = 0;
    break;
  case EpmActionType::Igmp :
    actionBitParams.bitmap.israw        = 1;
    actionBitParams.bitmap.report_en    = 0;
    actionBitParams.bitmap.feedbck_en   = 0;
    break;
  case EpmActionType::CmeSwFire    :
    actionBitParams.bitmap.app_seq_inc  = 1;
    actionBitParams.bitmap.israw        = 0;
    actionBitParams.bitmap.report_en    = 0;
    actionBitParams.bitmap.feedbck_en   = 1;
    break;
  case EpmActionType::BoeFire      :
  case EpmActionType::CmeHwCancel    :
  case EpmActionType::SqfFire      :
    actionBitParams.bitmap.originatedFromHw = 1;
    [[fallthrough]];
  case EpmActionType::HwFireAction :
    actionBitParams.bitmap.app_seq_inc  = 1;
    [[fallthrough]];
  case EpmActionType::BoeCancel    :
  case EpmActionType::SqfCancel    :

  case EpmActionType::CmeSwHeartbeat :
    
    actionBitParams.bitmap.israw        = 0;
    actionBitParams.bitmap.report_en    = 1;
    actionBitParams.bitmap.feedbck_en   = 1;
    break;
  case EpmActionType::ItchHwFastSweep :
    actionBitParams.bitmap.israw        = 1;
    actionBitParams.bitmap.report_en    = 1;
    actionBitParams.bitmap.feedbck_en   = 0;
    break;
    
  case EpmActionType::UserAction :
    actionBitParams.bitmap.israw        = 0;
    actionBitParams.bitmap.report_en    = 1;
    actionBitParams.bitmap.feedbck_en   = 1;
    break;
  default:
    on_error("Unexpected EkaEpmAction type %d",(int)type);
  }
  return 0;
}
/* ---------------------------------------------------- */

void EkaEpmAction::setIpTtl() {
  switch (type) {
  case EpmActionType::UserAction   :
  case EpmActionType::HwFireAction :
  case EpmActionType::BoeFire      :
  case EpmActionType::BoeCancel    :
  case EpmActionType::SqfFire      :
  case EpmActionType::SqfCancel    :
  case EpmActionType::CmeHwCancel  :
  case EpmActionType::ItchHwFastSweep   :
    ipHdr->_ttl = EFC_HW_TTL;
    ipHdr->_id  = EFC_HW_ID;
      return;
  default:
    return;
  }
}
/* ---------------------------------------------------- */

static TcpCsSizeSource setTcpCsSizeSource (EpmActionType type) {
  switch (type) {
  case EpmActionType::UserAction   :
  case EpmActionType::HwFireAction :
  case EpmActionType::BoeFire      :
  case EpmActionType::BoeCancel    :
  case EpmActionType::SqfFire      :
  case EpmActionType::SqfCancel    :
  case EpmActionType::CmeHwCancel    :
  case EpmActionType::ItchHwFastSweep   :
    return TcpCsSizeSource::FROM_ACTION;
  default:
    return TcpCsSizeSource::FROM_DESCR;
  }
}
/* ---------------------------------------------------- */
int EkaEpmAction::setHwAction() {
  hwAction.bit_params            = actionBitParams;
  hwAction.tcpcs_template_db_ptr = epmTemplate->id;  
  hwAction.template_db_ptr       = epmTemplate->getDataTemplateAddr();
  hwAction.data_db_ptr           = heapAddr;
  hwAction.target_prod_id        = productIdx;  
  hwAction.target_core_id        = coreId;  
  hwAction.target_session_id     = sessId;  
  hwAction.next_action_index     = epmActionLocalCopy.nextAction;
  hwAction.mask_post_strat       = epmActionLocalCopy.postStratMask;
  hwAction.mask_post_local       = epmActionLocalCopy.postLocalMask;
  hwAction.enable_bitmap         = epmActionLocalCopy.enable;
  hwAction.user                  = epmActionLocalCopy.user;
  hwAction.token                 = epmActionLocalCopy.token;
  hwAction.tcpCSum               = tcpCSum;
  hwAction.payloadSize           = pktSize;
  hwAction.tcpCsSizeSource       = setTcpCsSizeSource(type);

  return 0;
}

/* ---------------------------------------------------- */
int EkaEpmAction::setTemplate() {
  switch (type) {
  case EpmActionType::TcpFastPath :
    epmTemplate                        = epm->tcpFastPathPkt;
    break;
  case EpmActionType::TcpFullPkt  :
    epmTemplate                        = epm->rawPkt;
    break;
  case EpmActionType::TcpEmptyAck :
    epmTemplate                        = epm->tcpFastPathPkt;
    break;
  case EpmActionType::Igmp :
    epmTemplate                        = epm->rawPkt;
    break;
  case EpmActionType::SqfFire      :
  case EpmActionType::SqfCancel    :
  case EpmActionType::BoeFire      :
  case EpmActionType::BoeCancel    :
  case EpmActionType::HwFireAction :
  case EpmActionType::CmeHwCancel  :
  case EpmActionType::ItchHwFastSweep   :
    epmTemplate                        = epm->hwFire;
    break;
  case EpmActionType::CmeSwFire    :
    epmTemplate                        = epm->swFire;
    break;

  case EpmActionType::CmeSwHeartbeat :
    epmTemplate                        = epm->cmeHb;
    break;

  case EpmActionType::UserAction :
    epmTemplate                        = epm->tcpFastPathPkt;
    break;
  default:
    on_error("Unexpected EkaEpmAction type %d",(int)type);
  }
  if (epmTemplate == NULL) 
    on_error("type %d: epmTemplate == NULL ",(int)type);
  //  EKA_LOG("Teplate set to: \'%s\'",epmTemplate->name);
  return 0;
}

/* ---------------------------------------------------- */
int EkaEpmAction::setName() {
  switch (type) {
  case EpmActionType::TcpFastPath :
    strcpy(actionName,"TcpFastPath");
    break;
  case EpmActionType::TcpFullPkt  :
    strcpy(actionName,"TcpFullPkt");
    break;
  case EpmActionType::TcpEmptyAck :
    strcpy(actionName,"TcpEmptyAck");
    break;
  case EpmActionType::Igmp :
    strcpy(actionName,"Igmp");
    break;
  case EpmActionType::SqfFire      :
    strcpy(actionName,"SqfFire");
    break;  
  case EpmActionType::SqfCancel    :
    strcpy(actionName,"SqfCancel");
    break;
  case EpmActionType::BoeFire      :
    strcpy(actionName,"BoeFire");
    break;
  case EpmActionType::BoeCancel    :
    strcpy(actionName,"BoeCancel");
    break;
  case EpmActionType::CmeHwCancel    :
    strcpy(actionName,"CmeHwCancel");
    break;
  case EpmActionType::CmeSwFire    :
    strcpy(actionName,"CmeSwFire");
    break;
  case EpmActionType::CmeSwHeartbeat    :
    strcpy(actionName,"CmeSwHeartbeat");
    break;
  case EpmActionType::ItchHwFastSweep   :
    strcpy(actionName,"ItchHwFastSweep");
    break;
  case EpmActionType::HwFireAction :
    strcpy(actionName,"HwFire");
    break;
  case EpmActionType::UserAction :
    strcpy(actionName,"UserAction");
    break;
  default:
    on_error("Unexpected EkaEpmAction type %d",(int)type);
  }
  return 0;
}
/* ---------------------------------------------------- */
EkaEpm::FrameType EkaEpmAction::getFrameType() {
  switch (type) {
  case EpmActionType::INVALID :
    on_error("Invalid ActionType");
  case EpmActionType::ItchHwFastSweep :
    return EkaEpm::FrameType::Udp;     
  case EpmActionType::Igmp :
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
  case EkaEpm::FrameType::Mac :
    return sizeof(EkaEthHdr);
  case EkaEpm::FrameType::Ip :
    return sizeof(EkaEthHdr) + sizeof(EkaIpHdr);    
  case EkaEpm::FrameType::Tcp :
    return sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr);    
  case EkaEpm::FrameType::Udp :
    return sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr);
  default :
    on_error("Unexpected frameType %d",(int)frameType);
  }  
}
/* ---------------------------------------------------- */
uint16_t EkaEpmAction::getL3L4len() {
  EkaEpm::FrameType frameType = getFrameType();

  switch (frameType) {
  case EkaEpm::FrameType::Mac :
    return (uint16_t)0;
  case EkaEpm::FrameType::Ip :
    return (uint16_t)sizeof(EkaIpHdr);    
  case EkaEpm::FrameType::Tcp :
    return (uint16_t)(sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));    
  case EkaEpm::FrameType::Udp :
    return (uint16_t)(sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));
  default :
    on_error("Unexpected frameType %d",(int)frameType);
  }  
}
/* ---------------------------------------------------- */

EkaEpmAction::EkaEpmAction(EkaDev*                 _dev,
			   EkaEpm::ActionType      _type,
			   uint                    _idx,
			   uint                    _localIdx,
			   uint                    _region,
			   uint8_t                 _coreId,
			   uint8_t                 _sessId,
			   uint                    _auxIdx,
			   uint  		   _heapOffs,
			   uint64_t		   _actionAddr) {

  dev             = _dev;
  if (!dev) on_error("!dev");
  initialized     = false;
  

  epm             =  dev->epm;
  if (!epm) on_error("!epm");

  type            = _type;
  idx             = _idx;
  localIdx        = _localIdx;
  region          = _region;
  coreId          = _coreId;
  sessId          = _sessId;
  productIdx      = _auxIdx;
  actionAddr      = _actionAddr;

  thrId           = calcThrId(type,sessId,productIdx);

  heapOffs        = _heapOffs;
  heapAddr        = EpmHeapHwBaseAddr + heapOffs;

  ethHdr = (EkaEthHdr*) &epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  udpHdr = (EkaUdpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));

  memset(&epm->heap[heapOffs],0,getPayloadOffset());

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
  epmActionLocalCopy.nextAction    = EPM_LAST_ACTION;
  epmActionLocalCopy.enable        = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy.postStratMask = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy.postLocalMask = EkaEpm::ALWAYS_ENABLE;
  epmActionLocalCopy.actionFlags   = AF_Valid;
  return 0;
}

/* ----------------------------------------------------- */
int EkaEpmAction::setUdpMcNwHdrs(uint8_t* macSa, 
				 uint32_t srcIp, 
				 uint32_t dstIp, 
				 uint16_t srcPort, 
				 uint16_t dstPort,
				 uint     payloadSize) {
  ethHdr = (EkaEthHdr*) &dev->epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  udpHdr = (EkaUdpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  payload = (uint8_t*)tcpHdr + sizeof(EkaUdpHdr);

  memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));

  uint8_t macDa[6] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
  macDa[3] = ((uint8_t*) &dstIp)[1] & 0x7F;
  macDa[4] = ((uint8_t*) &dstIp)[2];
  macDa[5] = ((uint8_t*) &dstIp)[3];

  memcpy(&(ethHdr->dest),macDa,6);
  memcpy(&(ethHdr->src), macSa,6);

  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl   = 0x45;
  ipHdr->_tos    = 0;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl    = 64;
  ipHdr->_proto  = EKA_PROTO_UDP;
  ipHdr->_chksum = 0;
  ipHdr->_id     = 0;

  ipHdr->src     = srcIp;
  ipHdr->dest    = dstIp;

  ipHdr->_len    = be16toh(getL3L4len() + payloadSize);
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));
  
  udpHdr->src    = be16toh(srcPort);
  udpHdr->dest   = be16toh(dstPort);
  udpHdr->len    = be16toh(sizeof(EkaUdpHdr) + payloadSize);
  udpHdr->chksum = 0;

  payloadLen = payloadSize;

  pktSize = getPayloadOffset() + payloadLen;

  memset(payload,0,payloadLen);

  //  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

  auto heapWrChId = dev->heapWrChannels.getChannelId(EkaHeapWrChannels::AccessType::HeapPreload);
  setHeapWndAndCopy(dev,heapAddr,(uint64_t*) ethHdr, heapWrChId, pktSize);
  return 0;
}

/* ----------------------------------------------------- */
int EkaEpmAction::setNwHdrs(uint8_t* macDa, 
			    uint8_t* macSa, 
			    uint32_t srcIp, 
			    uint32_t dstIp, 
			    uint16_t srcPort, 
			    uint16_t dstPort) {
  ethHdr = (EkaEthHdr*) &dev->epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  memset(&epm->heap[heapOffs],0,getPayloadOffset());
  payload = &epm->heap[heapOffs] + getPayloadOffset();

  memcpy(&(ethHdr->dest),macDa,6);
  memcpy(&(ethHdr->src), macSa,6);
  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl = 0x45;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl = 64;

  ipHdr->_proto = isUdp() ? EKA_PROTO_UDP : EKA_PROTO_TCP;

  ipHdr->_chksum = 0;
  ipHdr->_id     = 0;
  //  ipHdr->_len    = 0;
  ipHdr->src     = srcIp;
  ipHdr->dest    = dstIp;

  ipHdr->_len    = be16toh(getL3L4len() + payloadLen);

  if (isUdp()) {
    udpHdr->src    = be16toh(srcPort);
    udpHdr->dest   = be16toh(dstPort);
    udpHdr->len    = be16toh(sizeof(EkaUdpHdr) + payloadLen);
  }

  if (isTcp()) {
    tcpHdr->src    = be16toh(srcPort);
    tcpHdr->dest   = be16toh(dstPort);
    tcpHdr->_hdrlen_rsvd_flags = be16toh(0x5000 | EKA_TCP_FLG_PSH | EKA_TCP_FLG_ACK);
    tcpHdr->urgp = 0;
  }

  setIpTtl();
  
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  pktSize = getPayloadOffset() + payloadLen;
  
  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,0/* payloadLen */);
  //---------------------------------------------------------
  //  hexDump("setNwHdrs",&epm->heap[heapOffs],pktSize);

  //copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

  auto heapWrChId = dev->heapWrChannels.getChannelId(EkaHeapWrChannels::AccessType::HeapPreload);
  setHeapWndAndCopy(dev,heapAddr,(uint64_t*) ethHdr, heapWrChId, pktSize);
  
  /* EKA_LOG("()()()()()()()()()() ipHdr->_len = %d, udpHdr->len = %d, pktSize = %d, ipHdr->_chksum 0x%x", */
  /* 	  be16toh(ipHdr->_len), */
  /* 	  be16toh(udpHdr->len), */
  /* 	  pktSize, */
  /* 	  be16toh(ipHdr->_chksum) */
	  
  /* 	  ); */
  
  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::updateAttrs (uint8_t _coreId, uint8_t _sessId, const EpmAction *epmAction) {
  if (epmAction == NULL) on_error("epmAction == NULL");
  type = epmAction->type;

  if (epmAction->offset < getPayloadOffset())
    on_error("epmAction->offset %d < getPayloadOffset() %ju",epmAction->offset,getPayloadOffset());

  memcpy (&epmActionLocalCopy,epmAction,sizeof(epmActionLocalCopy));
  
  setActionBitmap();
  setTemplate();
  setName();

  heapOffs   = epmAction->offset - getPayloadOffset();
  if (heapOffs % 32 != 0) on_error("heapOffs %d (must be X32)",heapOffs);

  heapAddr   = EpmHeapHwBaseAddr + heapOffs;
  coreId     = _coreId;
  sessId     = _sessId;

/* ----------------------------------------------------- */
  ethHdr = (EkaEthHdr*) &dev->epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  payload = &epm->heap[heapOffs] + getPayloadOffset();

/* ----------------------------------------------------- */

  if (!dev->core[coreId]) on_error("!dev->core[%u]",coreId);

  if (isUdp()) { // UDP
    EkaUdpTxSess* udpSess = dev->core[coreId]->udpTxSess[sessId];
    if (!udpSess) on_error("dev->core[%u]->udpTxSess[%u] == NULL",coreId,sessId);
    setNwHdrs(udpSess->macDa_,udpSess->macSa_,udpSess->srcIp_,udpSess->dstIp_,udpSess->srcPort_,udpSess->dstPort_);
  } else {
    EkaTcpSess*   sess    = dev->core[coreId]->tcpSess[sessId];
    if (!sess) on_error("dev->core[%u]->tcpSess[%u] == NULL",coreId,sessId);
    setNwHdrs(sess->macDa,sess->macSa,sess->srcIp,sess->dstIp,sess->srcPort,sess->dstPort);
  }
  

  /* EKA_LOG("%s:%u --> %s:%u",EKA_IP2STR(sess->srcIp),sess->srcPort, */
  /* 	  EKA_IP2STR(sess->dstIp),sess->dstPort); */
/* ----------------------------------------------------- */
  payloadLen = epmAction->length;
  pktSize    = getPayloadOffset() + payloadLen;

  ipHdr->_len    = be16toh(getL3L4len() + payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));
  
/* ----------------------------------------------------- */

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);
  setIpTtl();
  
  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);

  setHwAction();
      
  copyBuf2Hw(dev,EkaEpm::EpmActionBase,
	     (uint64_t*)&hwAction,sizeof(hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,idx,0);

  /* hexDump("updateAttrs",&epm->heap[heapOffs],pktSize); */

  //  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*)&epm->heap[heapOffs],thrId,pktSize);
  auto heapWrChId = dev->heapWrChannels.getChannelId(EkaHeapWrChannels::AccessType::HeapPreload);
  setHeapWndAndCopy(dev,heapAddr,(uint64_t*) ethHdr, heapWrChId, pktSize);
  
  //  print("from updateAttrs");

  return 0;
}
/* ----------------------------------------------------- */
int EkaEpmAction::setEthFrame(const void* buf, uint len) {
  pktSize  = len;
  memcpy(&epm->heap[heapOffs],buf,pktSize);
  //  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

  auto heapWrChId = dev->heapWrChannels.getChannelId(EkaHeapWrChannels::AccessType::HeapPreload);
  setHeapWndAndCopy(dev,heapAddr,(uint64_t*) ethHdr, heapWrChId, pktSize);
    
  tcpCSum = 0;

  hwAction.payloadSize           = pktSize;
  hwAction.tcpCSum               = tcpCSum;
  return 0;
}
/* ----------------------------------------------------- */
// For TCP only!
int EkaEpmAction::setPktPayload(const void* buf, uint len) {
  bool same = true;
  if (!buf) on_error("!buf");

  auto heapWrChId = dev->heapWrChannels.getChannelId(EkaHeapWrChannels::AccessType::TcpSend);
  dev->heapWrChannels.getChannel(heapWrChId);
  auto wndBase = heapAddr;
  setHeapWnd(dev,heapWrChId,wndBase);

  if (payloadLen != len) {
    same = false;
    payloadLen = len;

    ipHdr->_len    = be16toh(getL3L4len() + payloadLen);
    pktSize = getPayloadOffset() + payloadLen;

    ipHdr->_chksum = 0;
    ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

    // wrting to FPGA heap IP len & csum1
    //    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 16, (uint64_t*) &epm->heap[heapOffs + 16], thrId, 16);
    heapCopy(dev,wndBase,heapAddr + 16, (uint64_t*) &epm->heap[heapOffs + 16],heapWrChId,16);
    /* EKA_LOG("()()()()()()()()()() payloadLen != len ipHdr->_len = %d, pktSize = %d", */
    /* 	    ipHdr->_len, */
    /* 	    pktSize); */

  } else {
    /* EKA_LOG("()()()()()()()()()() payloadLen == len == %d",payloadLen); */
  }
  uint8_t prevBuf[2000] = {};
  memcpy(prevBuf,&epm->heap[heapOffs],pktSize);
  memcpy(&epm->heap[heapOffs + getPayloadOffset()],buf,len);

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  // 14 + 20 + 20 = 54 ==> 6 words + 6 bytes
  uint payloadWords  = (6 + len) / 8 + !!((6 + len) % 8);
  uint64_t* prevData = (uint64_t*) &prevBuf[48];
  uint64_t* newData  = (uint64_t*) &epm->heap[heapOffs + 48];

  for (uint w = 0; w < payloadWords; w ++) {
    if (*prevData != *newData) {
      same = false;
      //      copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 48 + 8 * w, newData, thrId, 8);
      heapCopy(dev,wndBase,heapAddr + 48 + 8 * w,newData,heapWrChId,8);
    }
    prevData++;
    newData++;
  }

  if (! same) {
    setIpTtl();
    tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);
    if (hwAction.tcpCsSizeSource == TcpCsSizeSource::FROM_ACTION) {
      hwAction.tcpCSum      = tcpCSum;
      hwAction.payloadSize  = pktSize; 
      
      copyBuf2Hw(dev,EkaEpm::EpmActionBase, (uint64_t*)&hwAction,sizeof(hwAction)); //write to scratchpad
      atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,idx,0);
    }
  }
  
  dev->heapWrChannels.releaseChannel(heapWrChId);

  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::updatePayload(uint offset, uint len) {
  /* heapOffs   = offset - EkaEpm::DatagramOffset; */
  /* if (heapOffs % 64 != 0) on_error("offset %u is not alligned",offset); */
  payloadLen = len;

  //  heapAddr        = EpmHeapHwBaseAddr + heapOffs;

  pktSize = getPayloadOffset() + payloadLen;
  ipHdr->_len    = be16toh(getL3L4len() + payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));
  
  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  setIpTtl();
  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);
  //  copyIndirectBuf2HeapHw_swap4(dev,heapAddr, (uint64_t*) &epm->heap[heapOffs], thrId, pktSize);

  auto heapWrChId = dev->heapWrChannels.getChannelId(EkaHeapWrChannels::AccessType::HeapPreload);
  setHeapWndAndCopy(dev,heapAddr,(uint64_t*) ethHdr, heapWrChId, pktSize);
  
  hwAction.tcpCSum      = tcpCSum;
  hwAction.payloadSize  = pktSize; 
  copyBuf2Hw(dev,EkaEpm::EpmActionBase, (uint64_t*)&hwAction,sizeof(hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,idx,0);

  //  print("from updatePayload");
  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::setUdpPktPayload(const void* buf, uint len) {
  if (!buf) on_error("!buf");

  payloadLen = len;
  pktSize = getPayloadOffset() + payloadLen;

  ipHdr->_len    = be16toh(getL3L4len() + payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  // wrting to FPGA heap IP len & csum
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 16, (uint64_t*) &epm->heap[heapOffs + 16], thrId, 16);


  memcpy(&epm->heap[heapOffs + getPayloadOffset()],buf,len);


  /* hexDump("setUdpPktPayload",&epm->heap[heapOffs],pktSize); */
  
  //  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  // 14 + 20 + 8 = 42 ==> 5 words + 2 bytes
  uint payloadWords  = (2 + len) / 8 + !!((2 + len) % 8);
  uint64_t* newData  = (uint64_t*) &epm->heap[heapOffs + 40];

  for (uint w = 0; w < payloadWords; w ++) {
    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 40 + 8 * w, newData, thrId, 8);
    newData++;
  }

  //  if (! same) {
  if (0) {
    //    udpHdr->chksum = calcUdpCsum(ipHdr,udpHdr,payload,payloadLen);
    udpHdr->chksum =  udp_checksum(udpHdr, payloadLen + 8, ipHdr->src,ipHdr->dest);
    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 40, newData, thrId, 8);
  }
  
  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::send(uint32_t _tcpCSum) {
  epm_trig_desc_t epm_trig_desc = {};

  epm_trig_desc.str.action_index = localIdx;
  epm_trig_desc.str.size         = pktSize;
  epm_trig_desc.str.tcp_cs       = _tcpCSum;
  epm_trig_desc.str.region       = region;

#if 1
  EKA_LOG("%s: action_index = %u,region=%u,size=%u, heapOffs=0x%x, heapAddr=0x%jx ",
	  actionName,
	  epm_trig_desc.str.action_index,
	  epm_trig_desc.str.region,
	  epm_trig_desc.str.size,
	  heapOffs,
	  heapAddr
	  );
  char hexDumpstr[8000] = {};
  hexDump2str("EkaEpmAction::send() pkt",&epm->heap[heapOffs],pktSize,
	      hexDumpstr, sizeof(hexDumpstr));
#endif
  
  eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc);

  //  print("EkaEpmAction::send");
  //  return pktSize;
  return payloadLen;
}
/* ----------------------------------------------------- */

int EkaEpmAction::send() {
  return send(tcpCSum);
}
/* ----------------------------------------------------- */
int EkaEpmAction::fastSend(const void* buf, uint len) {
  setPktPayload(buf, len);
  return send();
}
/* ----------------------------------------------------- */
int EkaEpmAction::fastSend(const void* buf) {
  return fastSend(buf,payloadLen);
}

/* ----------------------------------------------------- */
void EkaEpmAction::print(const char* msg) {
  EKA_LOG("%s: %s, region=%u, idx=%u, localIdx=%u, heapOffs=0x%x, heapAddr=0x%jx,  actionAddr=0x%jx, pktSize=%u,  ",
	  msg,
	  actionName,
	  region,
	  idx,
	  localIdx,
	  heapOffs,
	  heapAddr,
	  actionAddr,
	  pktSize
	  );
}
/* ----------------------------------------------------- */
