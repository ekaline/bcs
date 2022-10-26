/* NE SHURIK */

#include "EkaEpmAction.h"
#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EpmTemplate.h"
#include "EkaEpm.h"
#include "EkaEpmRegion.h"

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);
unsigned short csum(unsigned short *ptr,int nbytes);
uint32_t calcUdpCsum (void* ip_hdr, void* udp_hdr, void* payload, uint16_t payload_size);
uint16_t udp_checksum(EkaUdpHdr *p_udp_header, size_t len, uint32_t src_addr, uint32_t dest_addr);

/* ---------------------------------------------------- */
int EkaEpmAction::setActionBitmap() {
  
  if (epmActionLocalCopy.actionFlags == AF_Valid) 
    actionBitParams.bitmap.action_valid = 1;
  else
    actionBitParams.bitmap.action_valid = 0;

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
  case EpmActionType::HwFireAction :
  case EpmActionType::BoeFire      :
  case EpmActionType::BoeCancel    :

  case EpmActionType::CmeHwCancel    :
  case EpmActionType::CmeSwFire    :
    
    actionBitParams.bitmap.app_seq_inc  = 1;
  case EpmActionType::SqfFire      :
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
  case EpmActionType::CmeSwFire    :
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
  initialized     = false;
  
  if (dev == NULL) on_error("dev = NULL");

  epm             =  dev->epm;
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

  if (epm == NULL) on_error("epm == NULL");

  ethHdr = (EkaEthHdr*) &epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  udpHdr = (EkaUdpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  if (type == EpmActionType::ItchHwFastSweep) { // UDP
    payload = (uint8_t*)udpHdr + sizeof(EkaUdpHdr);
    memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));
  } else {
    payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);
    memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));
  }

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

  ipHdr->_len    = be16toh(sizeof(EkaIpHdr) + sizeof(EkaUdpHdr) + payloadSize);
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));
  
  udpHdr->src    = be16toh(srcPort);
  udpHdr->dest   = be16toh(dstPort);
  udpHdr->len    = be16toh(sizeof(EkaUdpHdr) + payloadSize);
  udpHdr->chksum = 0;

  payloadLen = payloadSize;

  pktSize = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr) + payloadLen;

  memset(payload,0,payloadLen);

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

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
  if (type == EpmActionType::ItchHwFastSweep) {
    payload = (uint8_t*)tcpHdr + sizeof(EkaUdpHdr);
    memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr));
  } else {
    payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);
    memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));
  }
  memcpy(&(ethHdr->dest),macDa,6);
  memcpy(&(ethHdr->src), macSa,6);
  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl = 0x45;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl = 64;
  if (type == EpmActionType::ItchHwFastSweep)
    ipHdr->_proto = EKA_PROTO_UDP;
  else
    ipHdr->_proto = EKA_PROTO_TCP;

  ipHdr->_chksum = 0;
  ipHdr->_id  = 0;
  ipHdr->_len = 0;

  ipHdr->src  = srcIp;
  ipHdr->dest = dstIp;
  if (type == EpmActionType::ItchHwFastSweep) {
    ipHdr->_len = be16toh(sizeof(EkaIpHdr)+sizeof(EkaUdpHdr));
    udpHdr->src    = be16toh(srcPort);
    udpHdr->dest   = be16toh(dstPort);
  } else {
    tcpHdr->src    = be16toh(srcPort);
    tcpHdr->dest   = be16toh(dstPort);
    tcpHdr->_hdrlen_rsvd_flags = be16toh(0x5000 | EKA_TCP_FLG_PSH | EKA_TCP_FLG_ACK);
    tcpHdr->urgp = 0;
    //---------------------------------------------------------
    ipHdr->_len = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr));
  }
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  if (type == EpmActionType::ItchHwFastSweep) {

    pktSize = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaUdpHdr) + payloadLen;
  } else {
    payloadLen = 0;
    pktSize = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr) + payloadLen;
  }
  //---------------------------------------------------------
  setIpTtl();
  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);
  //---------------------------------------------------------

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::updateAttrs (uint8_t _coreId, uint8_t _sessId, const EpmAction *epmAction) {
  if (epmAction == NULL) on_error("epmAction == NULL");
  if (epmAction->offset  < EkaEpm::DatagramOffset) 
    on_error("epmAction->offset %d < EkaEpm::DatagramOffset %ju",epmAction->offset,EkaEpm::DatagramOffset);

  memcpy (&epmActionLocalCopy,epmAction,sizeof(epmActionLocalCopy));
  
  type = epmAction->type;
  setActionBitmap();
  setTemplate();
  setName();
  
  heapOffs   = epmAction->offset - EkaEpm::DatagramOffset;
  if (heapOffs % 32 != 0) on_error("heapOffs %d (must be X32)",heapOffs);

  heapAddr   = EpmHeapHwBaseAddr + heapOffs;
  coreId     = _coreId;
  sessId     = _sessId;

/* ----------------------------------------------------- */
  ethHdr = (EkaEthHdr*) &dev->epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);

/* ----------------------------------------------------- */

  if (dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId);
  EkaTcpSess* sess = dev->core[coreId]->tcpSess[sessId];
  if (sess == NULL) on_error("dev->core[%u]->tcpSess[%u] == NULL",coreId,sessId);

  setNwHdrs(sess->macDa,sess->macSa,sess->srcIp,sess->dstIp,sess->srcPort,sess->dstPort);

  /* EKA_LOG("%s:%u --> %s:%u",EKA_IP2STR(sess->srcIp),sess->srcPort, */
  /* 	  EKA_IP2STR(sess->dstIp),sess->dstPort); */
/* ----------------------------------------------------- */
  payloadLen = epmAction->length;
  pktSize    = EkaEpm::DatagramOffset + payloadLen;

/* ----------------------------------------------------- */

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);
  setIpTtl();
  
  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);

  setHwAction();
      
  copyBuf2Hw(dev,EkaEpm::EpmActionBase,
	     (uint64_t*)&hwAction,sizeof(hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,idx,0);

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*)&epm->heap[heapOffs],thrId,pktSize);

  print("from updateAttrs");

  return 0;
}
/* ----------------------------------------------------- */
int EkaEpmAction::setEthFrame(const void* buf, uint len) {
  pktSize  = len;
  memcpy(&epm->heap[heapOffs],buf,pktSize);
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);
  tcpCSum = 0;

  hwAction.payloadSize           = pktSize;
  hwAction.tcpCSum               = tcpCSum;
  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::setPktPayload(const void* buf, uint len) {
  bool same = true;
  if (buf == NULL) on_error("buf == NULL");

  if (payloadLen != len) {
    same = false;
    payloadLen = len;
    pktSize = EkaEpm::DatagramOffset + payloadLen;

    ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadLen);
    ipHdr->_chksum = 0;
    ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

    // wrting to FPGA heap IP len & csum
    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 16, (uint64_t*) &epm->heap[heapOffs + 16], thrId, 16);
  }

  uint8_t prevBuf[2000] = {};
  memcpy(prevBuf,&epm->heap[heapOffs],pktSize);
  memcpy(&epm->heap[heapOffs + EkaEpm::DatagramOffset],buf,len);

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  uint payloadWords  = (6 + len) / 8 + !!((6 + len) % 8);
  uint64_t* prevData = (uint64_t*) &prevBuf[48];
  uint64_t* newData  = (uint64_t*) &epm->heap[heapOffs + 48];

  for (uint w = 0; w < payloadWords; w ++) {
    if (*prevData != *newData) {
      same = false;
      copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 48 + 8 * w, newData, thrId, 8);
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
  
  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::updatePayload(uint offset, uint len) {
  /* heapOffs   = offset - EkaEpm::DatagramOffset; */
  /* if (heapOffs % 64 != 0) on_error("offset %u is not alligned",offset); */
  payloadLen = len;

  //  heapAddr        = EpmHeapHwBaseAddr + heapOffs;

  pktSize = EkaEpm::DatagramOffset + payloadLen;
  ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  setIpTtl();
  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr, (uint64_t*) &epm->heap[heapOffs], thrId, pktSize);

  hwAction.tcpCSum      = tcpCSum;
  hwAction.payloadSize  = pktSize; 
  copyBuf2Hw(dev,EkaEpm::EpmActionBase, (uint64_t*)&hwAction,sizeof(hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,idx,0);

  //  print("from updatePayload");
  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::setUdpPktPayload(const void* buf, uint len) {
  bool same = true;
  if (buf == NULL) on_error("buf == NULL");

  uint udpPayloadOffset = sizeof(EkaEthHdr) + sizeof(EkaIpHdr)+sizeof(EkaUdpHdr);

  if (payloadLen != len) {
    same = false;
    payloadLen = len;
    pktSize = udpPayloadOffset + payloadLen;

    ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaUdpHdr)+payloadLen);
    ipHdr->_chksum = 0;
    ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

    // wrting to FPGA heap IP len & csum
    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 16, (uint64_t*) &epm->heap[heapOffs + 16], thrId, 16);
  }

  uint8_t prevBuf[2000] = {};
  memcpy(prevBuf,&epm->heap[heapOffs],pktSize);
  memcpy(&epm->heap[heapOffs + udpPayloadOffset],buf,len);

  //  epmTemplate->clearHwFields(&epm->heap[heapOffs]);

  // 14 + 20 + 8 = 42 ==> 5 words + 2 bytes
  uint payloadWords  = (2 + len) / 8 + !!((2 + len) % 8);
  uint64_t* prevData = (uint64_t*) &prevBuf[40];
  uint64_t* newData  = (uint64_t*) &epm->heap[heapOffs + 40];

  for (uint w = 0; w < payloadWords; w ++) {
    if (*prevData != *newData) {
      same = false;
      if (w != 6) copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 40 + 8 * w, newData, thrId, 8);
    }
    prevData++;
    newData++;
  }

  if (! same) {
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

#if 0
  /* if (type == EkaEpm::EpmActionType::UserAction) { */
    EKA_LOG("%s: action_index = %u,region=%u,size=%u,tcpCSum=%08x, heapAddr = 0x%jx ",
	    actionName,
	    epm_trig_desc.str.action_index,
	    epm_trig_desc.str.region,
	    epm_trig_desc.str.size,
	    epm_trig_desc.str.tcp_cs,
	    heapAddr
	    );
    fflush(stdout);    fflush(stderr);

    hexDump("EkaEpmAction::send() pkt",&epm->heap[heapOffs],pktSize);
    fflush(stdout);    fflush(stderr);
    //    print("From send()");
    fflush(stdout);    fflush(stderr);
  /* } */
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
