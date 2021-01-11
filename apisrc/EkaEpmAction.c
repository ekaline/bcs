/* NE SHURIK */

#include "EkaEpmAction.h"
#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EpmTemplate.h"

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);
unsigned short csum(unsigned short *ptr,int nbytes);


EkaEpmAction::EkaEpmAction(EkaDev*                 _dev,
			   char*                   _actionName,
			   EkaEpm::ActionType      _type,
			   uint                    _idx,
			   uint                    _localIdx,
			   uint                    _region,
			   uint8_t                 _coreId,
			   uint8_t                 _sessId,
			   uint                    _auxIdx,
			   EpmActionBitmap         _actionBitParams,
			   uint  		   _heapOffs,
			   uint64_t		   _actionAddr,
			   EpmTemplate*            _epmTemplate) {

  dev             = _dev;
  strcpy(actionName,_actionName);

  if (dev == NULL) on_error("dev = NULL");
  //  EKA_LOG("Creating %s",actionName); fflush(stderr);

  if (_epmTemplate == NULL) on_error("_epmTemplate == NULL");

  epm             =  dev->epm;
  type            = _type;
  idx             = _idx;
  localIdx        = _localIdx;
  region          = _region;
  coreId          = _coreId;
  sessId          = _sessId;
  productIdx      = _auxIdx;
  actionBitParams = _actionBitParams;
  actionAddr      = _actionAddr;
  epmTemplate     = _epmTemplate;

  thrId           = calcThrId(type,sessId,productIdx);

  heapOffs        = _heapOffs;
  heapAddr        = EpmHeapHwBaseAddr + heapOffs;

  if (epm == NULL) on_error("epm == NULL");

  ethHdr = (EkaEthHdr*) &epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);

  memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));

  hwAction.tcpCsSizeSource          = type == EkaEpm::ActionType::UserAction ? 
    TcpCsSizeSource::FROM_ACTION    : TcpCsSizeSource::FROM_DESCR;

  hwAction.data_db_ptr              = heapAddr;
  hwAction.template_db_ptr          = epmTemplate->getDataTemplateAddr();
  hwAction.tcpcs_template_db_ptr    = epmTemplate->id;
  hwAction.bit_params               = actionBitParams;
  hwAction.target_prod_id           = productIdx;
  hwAction.target_core_id           = coreId;
  hwAction.target_session_id        = sessId;
  hwAction.next_action_index        = EPM_LAST_ACTION; 

  hwAction.mask_post_strat          = EkaEpm::ALWAYS_ENABLE;
  hwAction.mask_post_local          = EkaEpm::ALWAYS_ENABLE;
  hwAction.enable_bitmap            = EkaEpm::ALWAYS_ENABLE;

  //  print("From constructor");
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
  payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);

  memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));

  memcpy(&(ethHdr->dest),macDa,6);
  memcpy(&(ethHdr->src), macSa,6);
  ethHdr->type = be16toh(0x0800);

  ipHdr->_v_hl = 0x45;
  ipHdr->_offset = 0x0040;
  ipHdr->_ttl = 64;
  ipHdr->_proto = EKA_PROTO_TCP;
  ipHdr->_chksum = 0;
  ipHdr->_id  = 0;
  ipHdr->_len = 0;

  ipHdr->src  = srcIp;
  ipHdr->dest = dstIp;

  tcpHdr->src    = be16toh(srcPort);
  tcpHdr->dest   = be16toh(dstPort);
  tcpHdr->_hdrlen_rsvd_flags = be16toh(0x5000 | EKA_TCP_FLG_PSH | EKA_TCP_FLG_ACK);
  tcpHdr->urgp = 0;
  //---------------------------------------------------------
  ipHdr->_len = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr));
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  payloadLen = 0;
  pktSize = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr) + payloadLen;

  //---------------------------------------------------------

  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);
  //---------------------------------------------------------

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

  /* if (type == EkaEpm::ActionType::UserAction) { */
  /*   print("from setNwHdrs"); */
  /*   hexDump("setNwHdrs",ethHdr,pktSize); */
  /* } */

  /* EKA_LOG("%20s: core=%u, sess=%u NW headers preloaded for: %s:%u -- %s:%u", */
  /* 	  actionName,coreId,sessId, */
  /* 	  EKA_IP2STR(*(uint32_t*)(&ipHdr->src)),be16toh(tcpHdr->src), */
  /* 	  EKA_IP2STR(*(uint32_t*)(&ipHdr->dest)),be16toh(tcpHdr->dest) */
  /* 	  ); */
  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::updateAttrs (uint8_t _coreId, uint8_t _sessId, const EpmAction *epmAction) {
  if (epmAction == NULL) on_error("epmAction == NULL");
  if (epmAction->offset  < EkaEpm::DatagramOffset) 
    on_error("epmAction->offset %d < EkaEpm::DatagramOffset %ju",epmAction->offset,EkaEpm::DatagramOffset);


  memcpy (&epmActionLocalCopy,epmAction,sizeof(epmActionLocalCopy));

  heapOffs = epmAction->offset - EkaEpm::DatagramOffset;
  heapAddr = EpmHeapHwBaseAddr + heapOffs;
  coreId   = _coreId;
  sessId   = _sessId;

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

/* ----------------------------------------------------- */

  if (heapOffs % 32 != 0) on_error("heapOffs %d (must be X32)",heapOffs);
 
  hwAction.target_core_id        = coreId;
  hwAction.target_session_id     = sessId;
  hwAction.data_db_ptr           = heapAddr;
  hwAction.next_action_index     = epmAction->nextAction;
  hwAction.enable_bitmap         = epmAction->enable;
  hwAction.mask_post_strat       = epmAction->postStratMask;
  hwAction.mask_post_local       = epmAction->postLocalMask;
  hwAction.user                  = epmAction->user;
  hwAction.token                 = epmAction->token;
  if (epmAction->actionFlags == AF_Valid) 
    hwAction.bit_params.bitmap.action_valid = 1;
  else
    hwAction.bit_params.bitmap.action_valid = 0;

  return 0;
}
/* ----------------------------------------------------- */
int EkaEpmAction::setFullPkt(const void* buf, uint len) {
  pktSize  = len;
  memcpy(&epm->heap[heapOffs],buf,pktSize);
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);
  tcpCSum = 0;

  hwAction.payloadSize           = pktSize;
  hwAction.tcpCSum               = tcpCSum;
  return 0;
}
/* ----------------------------------------------------- */
// OBSOLETE!!!
int EkaEpmAction::setPktPayload(const void* buf, uint len) {
  memcpy(&epm->heap[heapOffs + EkaEpm::DatagramOffset],buf,len);

  if (payloadLen != len) {
    payloadLen = len;
    pktSize = EkaEpm::DatagramOffset + payloadLen;

    ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadLen);
    ipHdr->_chksum = 0;
    ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

    copyIndirectBuf2HeapHw_swap4(dev,heapAddr,     (uint64_t*)ethHdr,                     thrId, pktSize);

  } else {
    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 48,(uint64_t*) (((uint8_t*)ethHdr) + 48), thrId, pktSize - 48);
  }

  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);

  if (type == EkaEpm::ActionType::UserAction) {
    hwAction.tcpCSum      = tcpCSum;
    hwAction.payloadSize  = pktSize; 
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
  /* if (type == EkaEpm::ActionType::UserAction) { */
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
    print("From send()");
    fflush(stdout);    fflush(stderr);
  /* } */
#endif
  eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc);
  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::send() {
  return send(tcpCSum);
}
/* ----------------------------------------------------- */
int EkaEpmAction::fastSend(const void* buf, uint len) {
  //  hexDump("EkaEpmAction::fastSend buf",buf,len);
  /* if (payloadLen == len && memcmp(payload,buf,len) == 0) */
  /*   return send();  */

  bool same = true;

  if (payloadLen != len) {
    same = false;
    payloadLen = len;
    pktSize = EkaEpm::DatagramOffset + payloadLen;

    ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadLen);
    ipHdr->_chksum = 0;
    ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

    //    EKA_LOG("len = %u, payloadLen = %u, ipHdr->_len = %u (0x%x)",len,payloadLen,be16toh(ipHdr->_len),ipHdr->_len);

    // wrting to FPGA heap IP len & csum
    copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 16, (uint64_t*) &epm->heap[heapOffs + 16], thrId, 16);
  }

  uint8_t prevBuf[2000] = {};
  memcpy(prevBuf,&epm->heap[heapOffs],pktSize);
  memcpy(&epm->heap[heapOffs + EkaEpm::DatagramOffset],buf,len);

  //  hexDump("EkaEpmAction::fastSend before clearHwFields",&epm->heap[heapOffs],pktSize);
  epmTemplate->clearHwFields(&epm->heap[heapOffs]);
  //  hexDump("EkaEpmAction::fastSend after clearHwFields",&epm->heap[heapOffs],pktSize);

  uint payloadWords  = (6 + len) / 8 + !!((6 + len) % 8);
  uint64_t* prevData = (uint64_t*) &prevBuf[48];
  uint64_t* newData  = (uint64_t*) &epm->heap[heapOffs + 48];

  for (uint w = 0; w < payloadWords; w ++) {
    if (*prevData != *newData) {
      same = false;
      copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 48 + 8 * w, newData, thrId, 8);
      //      EKA_LOG("%02u: Overwriting 0x%016jx by 0x%016jx",w+6,*prevData, *newData);
    }
    prevData++;
    newData++;
  }

  /* print("From fastSend()"); */
  /* EKA_LOG("Action Idx = %u, len = %u, payloadLen = %u, heapAddr = 0x%jx",localIdx,len,payloadLen,heapAddr); */
  /* hexDump(actionName,&epm->heap[heapOffs],pktSize); */
  

  if (! same) {
    tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);
    if (type == EkaEpm::ActionType::UserAction) {
      hwAction.tcpCSum      = tcpCSum;
      hwAction.payloadSize  = pktSize; 
    }
  }
  
  //  hexDump((std::string("fastSend: ") + std::string(actionName) + "__after update").c_str(),&epm->heap[heapOffs],pktSize);
  return send();
}
/* ----------------------------------------------------- */
int EkaEpmAction::fastSend(const void* buf) {
  return fastSend(buf,payloadLen);
}

/* ----------------------------------------------------- */
void EkaEpmAction::print(const char* msg) {
  EKA_LOG("%s: %s, region = %u, idx=%u, localIdx=%u, heapOffs=0x%x, heapAddr=0x%jx, actionAddr = 0x%jx, pktSize=%u ",
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
