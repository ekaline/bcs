#include "EkaEpmAction.h"
#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);
uint32_t calcEmptyPktPseudoCsum (EkaIpHdr* ipHdr, EkaTcpHdr* tcpHdr);
uint16_t calc_tcp_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);

unsigned int pseudo_csum(unsigned short *ptr,int nbytes);
uint16_t pseudo_csum2csum (uint32_t pseudo);
unsigned short csum(unsigned short *ptr,int nbytes);

EkaEpmAction::EkaEpmAction(EkaDev*                 _dev,
			   char*                   _actionName,
			   EkaEpm::ActionType      _type,
			   uint                    _idx,
			   uint                    _localIdx,
			   uint                    _region,
			   uint8_t                 _coreId,
			   uint8_t                 _sessId,
			   uint                    _productIdx,
			   EpmActionBitmap         _actionBitParams,
			   uint64_t		   _heapAddr,
			   uint64_t		   _actionAddr,
			   uint64_t		   _dataTemplateAddr,
			   uint		           _templateId) {

  dev             = _dev;
  strcpy(actionName,_actionName);

  if (dev == NULL) on_error("dev = NULL");
  //  EKA_LOG("Creating %s",actionName); fflush(stderr);

  epm             =  dev->epm;
  type            = _type;
  idx             = _idx;
  localIdx        = _localIdx;
  region          = _region;
  coreId          = _coreId;
  sessId          = _sessId;
  productIdx      = _productIdx;
  actionBitParams = _actionBitParams;
  heapAddr        = _heapAddr;
  actionAddr      = _actionAddr;
  templateAddr    = _dataTemplateAddr;
  templateId      = _templateId;

  heapOffs        = heapAddr - EpmHeapHwBaseAddr;

  if (epm == NULL) on_error("epm == NULL");

  ethHdr = (EkaEthHdr*) &epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);

  memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));

  hwAction.tcpCsSizeSource          = type == EkaEpm::ActionType::UserAction ? 
    TcpCsSizeSource::FROM_ACTION    : TcpCsSizeSource::FROM_DESCR;

  hwAction.data_db_ptr              = heapAddr;
  hwAction.template_db_ptr          = templateAddr;
  hwAction.tcpcs_template_db_ptr    = templateId;
  hwAction.bit_params               = actionBitParams;
  hwAction.target_prod_id           = productIdx;
  hwAction.target_core_id           = coreId;
  hwAction.target_session_id        = sessId;
  hwAction.next_action_index        = EPM_LAST_ACTION; 

  hwAction.mask_post_strat          = EkaEpm::ALWAYS_ENABLE;
  hwAction.mask_post_local          = EkaEpm::ALWAYS_ENABLE;
  hwAction.enable_bitmap            = EkaEpm::ALWAYS_ENABLE;
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

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, 2 /* thrId */, pktSize);

  /* if (type == EkaEpm::ActionType::UserAction) { */
  /*   print(); */
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
int EkaEpmAction::setFullPkt(uint thrId, void* buf, uint len) {
  pktSize  = len;
  memcpy(&epm->heap[heapOffs],buf,pktSize);
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);
  tcpCSum = 0;

  hwAction.payloadSize           = pktSize;
  hwAction.tcpCSum               = tcpCSum;
  return 0;
}
/* ----------------------------------------------------- */
int EkaEpmAction::setPktPayload(uint thrId, void* buf, uint len) {
  payloadLen = len;
  pktSize = EkaEpm::DatagramOffset + payloadLen;
  memcpy(&epm->heap[heapOffs + EkaEpm::DatagramOffset],buf,payloadLen);

  ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  /* hexDump("setPktPayload",ethHdr,pktSize); fflush(stdout); */

  //  copyIndirectBuf2HeapHw_swap4(dev,heapAddr + 48,(uint64_t*) (((uint8_t*)ethHdr) + 48), thrId, pktSize - 48);
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*)ethHdr, thrId, pktSize);

  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);

  hwAction.payloadSize           = pktSize;
  hwAction.tcpCSum               = tcpCSum;
  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::send() {
  epm_trig_desc_t epm_trig_desc = {};

  epm_trig_desc.str.action_index = localIdx;
  epm_trig_desc.str.size         = pktSize;
  epm_trig_desc.str.tcp_cs       = tcpCSum;
  epm_trig_desc.str.region       = region;

#if 0
   if (type == EkaEpm::ActionType::UserAction) {
     //if (1) {
    EKA_LOG("%s: action_index = %u,region=%u,size=%u",
	    actionName,
	    epm_trig_desc.str.action_index,
	    epm_trig_desc.str.region,
	    epm_trig_desc.str.size
	    );
    hexDump("EkaEpmAction::send() pkt",&epm->heap[heapOffs],pktSize);
    fflush(stdout);    fflush(stderr);
    print();
    fflush(stdout);    fflush(stderr);
  }
#endif
  eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc);
  return 0;
}

  /* ----------------------------------------------------- */

void EkaEpmAction::print() {
  EKA_LOG("%s: tcpCsSizeSource=%s, coreId = %u, sessId = %u, idx=%u, localIdx=%u, data_db_ptr=0x%jx, heaOffs=%u, template_db_ptr=0x%jx,tcpcs_template_db_ptr=%u, coreId=%u, sessId=%u, heapAddr=0x%jx, pktSize=%u ",
	  actionName,
	  hwAction.tcpCsSizeSource == TcpCsSizeSource::FROM_ACTION ? "FROM ACTION" : "FROM DESCR",
	  coreId,
	  sessId,
	  idx,
	  localIdx,
	  hwAction.data_db_ptr,
	  hwAction.data_db_ptr - EpmHeapHwBaseAddr,
	  hwAction.template_db_ptr,
	  hwAction.tcpcs_template_db_ptr,
	  hwAction.target_core_id,
	  hwAction.target_session_id,
	  pktSize,
	  hwAction.payloadSize
	  );
}
/* ----------------------------------------------------- */
