#include "EkaEpmAction.h"
#include "EkaDev.h"
#include "ekaNW.h"


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
			    uint8_t                 _coreId,
			    uint8_t                 _sessId,
			    uint                    _productIdx,
			    epm_actione_bitparams_t _actionBitParams,
			    uint64_t		       _heapAddr,
			    uint64_t		       _actionAddr,
			    uint64_t		       _dataTemplateAddr,
			    uint		       _templateId) {

  dev             = _dev;
  strcpy(actionName,_actionName);

  if (dev == NULL) on_error("dev = NULL");
  //  EKA_LOG("Creating %s",actionName); fflush(stderr);

  epm             =  dev->epm;
  type            = _type;
  idx             = _idx;
  coreId          = _coreId;
  sessId          = _sessId;
  productIdx      = _productIdx;
  actionBitParams = _actionBitParams;
  heapAddr        = _heapAddr;
  actionAddr      = _actionAddr;
  templateAddr    = _dataTemplateAddr;
  templateId      = _templateId;

  nextIdx         = EPM_LAST_ACTION;

  heapOffs        = heapAddr - EpmHeapHwBaseAddr;

  if (epm == NULL) on_error("epm == NULL");

  ethHdr = (EkaEthHdr*) &epm->heap[heapOffs];
  ipHdr  = (EkaIpHdr*) ((uint8_t*)ethHdr + sizeof(EkaEthHdr));
  tcpHdr = (EkaTcpHdr*) ((uint8_t*)ipHdr + sizeof(EkaIpHdr));
  payload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);

  memset(ethHdr,0,sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr));

  hwAction.tcpCsSizeSource      = type == EkaEpm::ActionType::UserAction ? TcpCsSizeSource::FROM_ACTION : TcpCsSizeSource::FROM_DESCR;

  hwAction.data_db_ptr           = heapAddr;
  hwAction.template_db_ptr       = templateAddr;
  hwAction.tcpcs_template_db_ptr = templateId;
  hwAction.bit_params.israw      = actionBitParams.israw;
  hwAction.bit_params.report_en  = actionBitParams.report_en;
  hwAction.bit_params.feedbck_en = actionBitParams.feedbck_en;
  hwAction.target_prod_id        = productIdx;
  hwAction.target_core_id        = coreId;
  hwAction.target_session_id     = sessId;
  hwAction.next_action_index     = nextIdx; 
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

  return 0;
}

/* ----------------------------------------------------- */

int EkaEpmAction::update (uint8_t                 _coreId,
			  uint8_t                 _sessId,
			  epm_actionid_t          _nextAction,
			  uint64_t		      _heapAddr,
			  const uint64_t 	      _mask_post_strat,
			  const uint64_t 	      _mask_post_local,
			  const uint64_t  	      _user,
			  const epm_token_t	      _token,
			  uint16_t 		      _payloadSize,
			  uint32_t 		      _tcpCSum
			  ) {
  coreId          = _coreId;
  sessId          = _sessId;
  heapAddr        = _heapAddr;
  nextIdx         = _nextAction;

  heapOffs = heapAddr - EpmHeapHwBaseAddr;

  hwAction.target_core_id        = coreId;
  hwAction.target_session_id     = sessId;
  hwAction.data_db_ptr           = heapAddr;
  hwAction.next_action_index     = nextIdx;
  hwAction.mask_post_strat       = _mask_post_strat;
  hwAction.mask_post_local       = _mask_post_local;
  hwAction.user                  = _user;
  hwAction.token                 = _token;
  hwAction.payloadSize           = _payloadSize;
  hwAction.tcpCSum               = _tcpCSum;

  return 0;
}
/* ----------------------------------------------------- */
int EkaEpmAction::setFullPkt(uint thrId, void* buf, uint len) {
  pktSize  = len;
  //  EKA_LOG("%s : heapOffs=%u len=%u",actionName,heapOffs,len);fflush(stderr);
  memcpy(&epm->heap[heapOffs],buf,pktSize);
  //  hexDump("setFullPkt",ethHdr,pktSize); fflush(stdout);
  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);
  tcpCSum = 0;
  return 0;
}
/* ----------------------------------------------------- */
int EkaEpmAction::setPktPayload(uint thrId, void* buf, uint len) {
  payloadLen = len;
  pktSize = sizeof(EkaEthHdr) + sizeof(EkaIpHdr) + sizeof(EkaTcpHdr) + payloadLen;
  memcpy(&epm->heap[heapOffs + EkaEpm::DatagramOffset],buf,payloadLen);

  ipHdr->_len    = be16toh(sizeof(EkaIpHdr)+sizeof(EkaTcpHdr)+payloadLen);
  ipHdr->_chksum = 0;
  ipHdr->_chksum = csum((unsigned short *)ipHdr, sizeof(EkaIpHdr));

  copyIndirectBuf2HeapHw_swap4(dev,heapAddr,(uint64_t*) ethHdr, thrId, pktSize);

  tcpCSum = calc_pseudo_csum(ipHdr,tcpHdr,payload,payloadLen);

  return 0;
}
/* ----------------------------------------------------- */

int EkaEpmAction::send() {
  epm_trig_desc_t epm_trig_desc = {};

  epm_trig_desc.str.action_index = idx;
  epm_trig_desc.str.size         = pktSize;
  epm_trig_desc.str.tcp_cs       = tcpCSum;
  epm_trig_desc.str.region       = region;

  /* if (type == EkaEpm::ActionType::TcpFastPath) { */
  /*   hexDump("TcpFastPath pkt",&epm->heap[heapOffs],pktSize); */
  /*   EKA_LOG("%s: epm_trig_desc = %016jx,size=%x,",actionName,epm_trig_desc.desc,epm_trig_desc.str.size); */
  /* } */
  eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc);
  return 0;
}

  /* ----------------------------------------------------- */

void EkaEpmAction::print() {
  EKA_LOG("%s: coreId = %u, sessId = %u, idx=%u, data_db_ptr=0x%jx, template_db_ptr=0x%jx, tcpcs_template_db_ptr=%u, coreId=%u, sessId=%u, heapAddr=0x%jx, payloadSize=%u ",
	  actionName,
	  coreId,
	  sessId,
	  idx,
	  hwAction.data_db_ptr,
	  hwAction.template_db_ptr,
	  hwAction.tcpcs_template_db_ptr,
	  hwAction.target_core_id,
	  hwAction.target_session_id,
	  heapAddr,
	  hwAction.payloadSize
	  );
}
/* ----------------------------------------------------- */
