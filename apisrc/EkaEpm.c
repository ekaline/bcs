#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "EkaEpm.h"
#include "EpmTemplate.h"
#include "EpmFastPathTemplate.h"
#include "EpmRawPktTemplate.h"
#include "EkaTcpSess.h"
#include "EkaEpmAction.h"
#include "EpmStrategy.h"
#include "EkaUdpChannel.h"

uint32_t calc_pseudo_csum (void* ip_hdr, void* tcp_hdr, void* payload, uint16_t payload_size);

/* ---------------------------------------------------- */
int EkaEpm::ekaEpmProcessTrigger(const void* payload,uint len) {
  /* hexDump("Trigger",(void*)payload,len); */
  EpmTrigger* trigger = (EpmTrigger*)payload;
  EKA_LOG("Accepted Trigger: token=0x%jx, strategyId=%d, FistActionId=%d",
	  trigger->token,trigger->strategy,trigger->action);

  uint strategyId = trigger->strategy;
  uint currAction = trigger->action;
  
  while (currAction != EPM_LAST_ACTION) {
    uint heapOffs    = strategy[strategyId]->action[currAction].ekaAction->heapAddr - UserHeapBaseAddr;
    uint payloadSize = strategy[strategyId]->action[currAction].epmAction->length;

    EKA_LOG("Executing action %d:  heapOffs=%u, heapAddr = %ju, payloadSize=%u ",
	    currAction,heapOffs,heapOffs + UserHeapBaseAddr, payloadSize);

    if (currAction == 100) {
      EKA_LOG("currAction = 100, idx = %u, heapAddr = %ju",
	      strategy[strategyId]->action[currAction].ekaAction->idx,
	      strategy[strategyId]->action[currAction].ekaAction->heapAddr);
      hexDump("Heap Pkt",&heap[heapOffs],DatagramOffset + payloadSize);
    }
    //---------------------------------------------------------

    uint8_t* ipHdr      = &heap[heapOffs + sizeof(EkaEthHdr)];
    uint8_t* tcpHdr     = ipHdr  + sizeof(EkaIpHdr);
    uint8_t* tcpPayload = tcpHdr + sizeof(EkaTcpHdr);

    epm_trig_desc_t epm_trig_desc = {};
    epm_trig_desc.str.action_index = strategy[strategyId]->action[currAction].ekaAction->idx;
    epm_trig_desc.str.payload_size = DatagramOffset + payloadSize;
    epm_trig_desc.str.tcp_cs       = calc_pseudo_csum(ipHdr,tcpHdr,tcpPayload,payloadSize);

    eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc);
    //---------------------------------------------------------


    currAction = strategy[strategyId]->action[currAction].ekaAction->nextIdx;
  }

  return 0;
}

/* ---------------------------------------------------- */

EkaOpResult EkaEpm::setAction(EkaCoreId coreId,
			      epm_strategyid_t strategyIdx,
			      epm_actionid_t actionIdx,
			      const EpmAction *epmAction) {
  if (! initialized) {
    EKA_WARN ("EKA_OPRESULT__ERR_EPM_UNINITALIZED");
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  }
  if (! validStrategyIdx(strategyIdx)) {
    EKA_WARN ("EKA_OPRESULT__ERR_INVALID_STRATEGY: strategyIdx=%d",strategyIdx);
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  }
  if (! strategy[strategyIdx]->myAction(actionIdx)){
    EKA_WARN ("EKA_OPRESULT__ERR_INVALID_ACTION: strategyIdx=%d, actionIdx=%d",strategyIdx,actionIdx);
    return EKA_OPRESULT__ERR_INVALID_ACTION;
  }

  return strategy[strategyIdx]->setAction(actionIdx,epmAction);
}
/* ---------------------------------------------------- */
EkaOpResult EkaEpm::getAction(EkaCoreId coreId,
				 epm_strategyid_t strategyIdx,
				 epm_actionid_t actionIdx,
				 EpmAction *epmAction) {
  if (! initialized) return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  if (! strategy[strategyIdx]->myAction(actionIdx)) return EKA_OPRESULT__ERR_INVALID_ACTION;

  return strategy[strategyIdx]->getAction(actionIdx,epmAction);
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::enableController(EkaCoreId coreId, bool enable) {
  controllerEnabled = enable;
  return EKA_OPRESULT__OK;
}

/* ---------------------------------------------------- */

EkaOpResult EkaEpm::initStrategies(EkaCoreId coreId,
				   const EpmStrategyParams *params,
				   epm_strategyid_t numStrategies) {
  if (numStrategies > (int)EkaEpm::MaxStrategies) 
    on_error("numStrategies %u > EkaEpm::MaxStrategies %ju",numStrategies,EkaEpm::MaxStrategies);
  stratNum = numStrategies;

  if (udpCh[coreId] == NULL) udpCh[coreId] = new EkaUdpChannel(dev,coreId);
  if (udpCh[coreId] == NULL) on_error("Failed to open Epm Udp Channel for CoreId %u",coreId);

  epm_actionid_t currActionIdx = 0;
  for (auto i = 0; i < stratNum; i++) {
    strategy[i] = new EpmStrategy(this,i,currActionIdx, &params[i]);
    if (strategy[i] == NULL) on_error("Fail to create strategy[%d]",i);
    udpCh[coreId]->igmp_mc_join(0, strategy[i]->ip, be16toh(strategy[i]->port),0);
    currActionIdx += params[i].numActions;
    if (currActionIdx > (int)MaxUserActions) 
      on_error("currActionIdx %d > MaxUserActions %ju",currActionIdx,MaxUserActions);
  }
  initialized = true;
  return EKA_OPRESULT__OK;
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::setStrategyEnableBits(EkaCoreId coreId,
                                     epm_strategyid_t strategyIdx,
                                     epm_enablebits_t enable) {
  if (! initialized) 
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;

  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->setEnableBits(enable);
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::getStrategyEnableBits(EkaCoreId coreId,
					  epm_strategyid_t strategyIdx,
					  epm_enablebits_t *enable) {
  if (! initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->getEnableBits(enable);
}

/* ---------------------------------------------------- */
  
EkaOpResult EkaEpm::raiseTriggers(EkaCoreId coreId,
				  const EpmTrigger *trigger) {
  // TBD !!!
  return EKA_OPRESULT__OK;
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::payloadHeapCopy(EkaCoreId coreId,
				    epm_strategyid_t strategyIdx, 
				    uint32_t offset,
				    uint32_t length, 
				    const void *contents) {
  if (offset % 8 != EkaEpm::PayloadAlignment) {
    EKA_WARN("offset %d %% 8 (=%d) != EkaEpm::PayloadAlignment (%d)",
	     offset,offset % 8, EkaEpm::PayloadAlignment);
    return EKA_OPRESULT__ERR_INVALID_ALIGN;
  }
       
  if (offset + length > EkaEpm::PayloadMemorySize) {
    EKA_WARN("offset %d + length %d > EkaEpm::PayloadMemorySize %d",
	     offset,length,EkaEpm::PayloadMemorySize);
    return EKA_OPRESULT__ERR_INVALID_OFFSET;
  }

  memcpy(&heap[offset],contents,length);

  return EKA_OPRESULT__OK;
}

/* ---------------------------------------------------- */

int EkaEpm::InitTemplates() {
  templatesNum   = 0;

  EKA_LOG("Initializing EpmFastPathTemplate");
  tcpFastPathPkt = new EpmFastPathTemplate(templatesNum++,"EpmFastPathTemplate");
  if (tcpFastPathPkt == NULL) on_error("tcpFastPathPkt == NULL");

  EKA_LOG("Initializing EpmRawPktTemplate");
  rawPkt         = new EpmRawPktTemplate(templatesNum++  ,"EpmRawPktTemplate" );
  if (rawPkt == NULL) on_error("rawPkt == NULL");

  return 0;
}
/* ---------------------------------------------------- */

int EkaEpm::DownloadSingleTemplate2HW(EpmTemplate* t) {
  if (t == NULL) on_error("t == NULL");

  EKA_LOG("Downloading %s, id=%u, getDataTemplateAddr=%jx, getCsumTemplateAddr=%jx ",
	  t->name,t->id,t->getDataTemplateAddr(),t->getCsumTemplateAddr());
  volatile epm_tcpcs_template_t hw_tcpcs_template = {};

  // TCP CS template
  for (uint f = 0; f < EpmNumHwFields; f++) {
    for (uint b = 0; b < EpmHwFieldSize; b++) {
      if (t->hwField[f].cksmMSB[b])
	hw_tcpcs_template.high.field[f].bitmap |= ((uint16_t)1)<<b;
      if (t->hwField[f].cksmLSB[b])
	hw_tcpcs_template.low.field[f].bitmap  |= ((uint16_t)1)<<b;
    }
  }

  copyBuf2Hw_swap4(dev,t->getDataTemplateAddr(),(uint64_t*) t->data             ,EpmMaxRawTcpSize);
  copyBuf2Hw      (dev,t->getCsumTemplateAddr(),(uint64_t*) &hw_tcpcs_template  ,sizeof(epm_tcpcs_template_t));

  return 0;
}
/* ---------------------------------------------------- */


int EkaEpm::DownloadTemplates2HW() {
  EKA_LOG("Downloading %u templates",templatesNum);

  DownloadSingleTemplate2HW(tcpFastPathPkt);
  DownloadSingleTemplate2HW(rawPkt);

  return 0;
}
/* ---------------------------------------------------- */

EkaEpmAction* EkaEpm::addAction(ActionType type, uint8_t _coreId, uint8_t _sessId, uint8_t _productIdx) {
  if (tcpFastPathPkt == NULL) on_error("tcpFastPathPkt == NULL");
  if (rawPkt == NULL) on_error("rawPkt == NULL");

  uint                    heapBudget = (uint)(-1);
  epm_actione_bitparams_t actionBitParams = {};
  uint64_t                dataTemplateAddr = -1;
  uint                    templateId = (uint)(-1);

  char actionName[30] = {};
  uint     actionIdx  = -1;
  uint64_t heapAddr   = -1;
  uint64_t actionAddr = -1;

  createActionMtx.lock();

  switch (type) {
  case ActionType::TcpFastPath :
    heapBudget                 = MAX_PKT_SIZE;

    actionIdx                  = serviceActionIdx ++;
    heapAddr                   = serviceHeapAddr;
    serviceHeapAddr           += heapBudget;
    actionAddr                 = serviceActionAddr;
    serviceActionAddr         += ActionBudget;

    actionBitParams.israw      = 0;
    actionBitParams.report_en  = 0;
    actionBitParams.feedbck_en = 1;
    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"TcpFastPath");
    break;

  case ActionType::TcpFullPkt  :
    heapBudget                 = MAX_PKT_SIZE;

    actionIdx                  = serviceActionIdx ++;
    heapAddr                   = serviceHeapAddr;
    serviceHeapAddr           += heapBudget;
    actionAddr                 = serviceActionAddr;
    serviceActionAddr         += ActionBudget;

    actionBitParams.israw      = 1;
    actionBitParams.report_en  = 0;
    actionBitParams.feedbck_en = 0;
    dataTemplateAddr = rawPkt->getDataTemplateAddr();
    templateId       = rawPkt->id;
    strcpy(actionName,"TcpFullPkt");
    break;

  case ActionType::TcpEptyAck :
    heapBudget                 = TCP_EMPTY_ACK_SIZE;

    actionIdx                  = serviceActionIdx ++;
    heapAddr                   = serviceHeapAddr;
    serviceHeapAddr           += heapBudget;
    actionAddr                 = serviceActionAddr;
    serviceActionAddr         += ActionBudget;

    actionBitParams.israw      = 0;
    actionBitParams.report_en  = 0;
    actionBitParams.feedbck_en = 0;
    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"TcpEptyAck");
    break;

  case ActionType::UserAction :
    heapBudget                 = MAX_PKT_SIZE;

    actionIdx                  = userActionIdx ++;
    heapAddr                   = userHeapAddr;
    userHeapAddr              += heapBudget;
    actionAddr                 = userActionAddr;
    userActionAddr            += ActionBudget;

    actionBitParams.israw      = 0;
    //    actionBitParams.report_en  = 1;
    actionBitParams.report_en  = 0; // TMP!!!
    actionBitParams.feedbck_en = 1;
    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"UserAction");
    break;

  default:
    on_error("Unexpected EkaEpmAction type %d",(int)type);
  }

  if (userActionIdx > UserActionsBaseIdx + MaxUserActions) 
    on_error("userActionIdx %u > MaxUserActions %ju",userActionIdx, UserActionsBaseIdx + MaxUserActions);
  if (serviceActionIdx > ServiceActionsBaseIdx + MaxServiceActions) 
    on_error("serviceActionIdx %u > MaxServiceActions %ju",serviceActionIdx, ServiceActionsBaseIdx + MaxServiceActions);
  if (serviceHeapAddr > ServiceHeapBaseAddr + ServiceUserHeap)
    on_error("serviceHeapAddr %ju > MaxServiceHeap %ju",serviceHeapAddr, ServiceHeapBaseAddr + ServiceUserHeap);
  EkaEpmAction* action = new EkaEpmAction(dev,
					  actionName,
					  type,
					  actionIdx,
					  _coreId,
					  _sessId,
					  _productIdx,
					  actionBitParams,
					  heapAddr,
					  actionAddr,
					  dataTemplateAddr,
					  templateId
					  );
  /* EKA_LOG("%s: idx = %u, heapAddr = 0x%jx, actionAddr = 0x%jx", */
  /* 	  actionName,actionIdx,heapAddr,actionAddr); */

  createActionMtx.unlock();

  copyBuf2Hw(dev,EpmActionBase, (uint64_t*)&action->hwAction,sizeof(action->hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,action->idx,0);
  

  return action;
}
/* ---------------------------------------------------- */
