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
void ekaFireReportThread(EkaDev* dev);


EkaEpm::EkaEpm(EkaDev* _dev) {
  dev = _dev;

  setActionRegionBaseIdx(dev,ServiceRegion,ServiceActionsBaseIdx);
  eka_write(dev,strategyEnableAddr(ServiceRegion),ALWAYS_ENABLE);
  EKA_LOG("Created Epm");
}

/* ---------------------------------------------------- */

EkaOpResult EkaEpm::raiseTriggers(const EpmTrigger *trigger) {
  if (trigger == NULL) on_error("trigger == NULL");
  uint strategyId = trigger->strategy;
  uint currAction = trigger->action;
  EKA_LOG("Raising Trigger: strategyId %u, ActionId %u",strategyId,currAction);fflush(stderr);

  if (strategy[strategyId] == NULL) on_error("strategy[%u] == NULL",strategyId);
  if (strategy[strategyId]->action[currAction] == NULL) 
    on_error("strategy[%u]->action[%u] == NULL",strategyId,currAction);

  /* EKA_LOG("Accepted Trigger: token=0x%jx, strategyId=%d, FistActionId=%d, ekaAction->idx = %u, epm_trig_desc.str.action_index=%u", */
  /* 	  trigger->token,trigger->strategy,trigger->action, */
  /* 	  strategy[strategyId]->action[currAction]->idx,epm_trig_desc.str.action_index); */

  strategy[strategyId]->action[currAction]->send();

  /* EKA_LOG("User Action: actionType = %u, strategyId=%u, actionId=%u,heapAddr=%ju, pktSize=%u", */
  /* 	  strategy[strategyId]->action[currAction]->hwAction.tcpCsSizeSource, */
  /* 	  strategyId,currAction, */
  /* 	  strategy[strategyId]->action[currAction]->hwAction.data_db_ptr, */
  /* 	  strategy[strategyId]->action[currAction]->hwAction.payloadSize */
  /* 	  ); */
  /* eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc); */

  return EKA_OPRESULT__OK;
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
  if (numStrategies > (int)MaxStrategies) 
    on_error("numStrategies %u > MaxStrategies %ju",numStrategies,MaxStrategies);
  stratNum = numStrategies;

  if (udpCh[coreId] == NULL) udpCh[coreId] = new EkaUdpChannel(dev,coreId);
  if (udpCh[coreId] == NULL) on_error("Failed to open Epm Udp Channel for CoreId %u",coreId);

  if (! dev->fireReportThreadActive) {
    dev->fireReportThread = std::thread(ekaFireReportThread,dev);
    dev->fireReportThread.detach();
    while (! dev->fireReportThreadActive) sleep(0);
    EKA_LOG("fireReportThread activated");
  }


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

EkaOpResult EkaEpm::payloadHeapCopy(EkaCoreId coreId,
				    epm_strategyid_t strategyIdx, 
				    uint32_t offset,
				    uint32_t length, 
				    const void *contents) {
  if ((offset - DatagramOffset) % PayloadAlignment != 0) {
    EKA_WARN("offset (%d) - DatagramOffset (%d) %% PayloadAlignment (=%d) != 0",
	     offset,DatagramOffset,PayloadAlignment);
    return EKA_OPRESULT__ERR_INVALID_ALIGN;
  }
       
  if (offset + length > PayloadMemorySize) {
    EKA_WARN("offset %d + length %d > PayloadMemorySize %d",
	     offset,length,PayloadMemorySize);
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

EkaEpmAction* EkaEpm::addAction(ActionType type, 
			  epm_strategyid_t actionRegion, 
			  epm_actionid_t localIdx, 
			  uint8_t _coreId, 
			  uint8_t _sessId, 
			  uint8_t _auxIdx) {
  if (tcpFastPathPkt == NULL) on_error("tcpFastPathPkt == NULL");
  if (rawPkt == NULL) on_error("rawPkt == NULL");

  uint            heapBudget = (uint)(-1);
  EpmActionBitmap actionBitParams = {};
  uint64_t        dataTemplateAddr = -1;
  uint            templateId = (uint)(-1);

  char            actionName[30] = {};
  epm_actionid_t  actionIdx      = -1;
  epm_actionid_t  localActionIdx = -1;
  uint64_t        heapAddr       = -1;
  uint64_t        actionAddr     = -1;

  createActionMtx.lock();

  switch (type) {
  case ActionType::TcpFastPath :
    heapBudget                 = MAX_PKT_SIZE;

    localActionIdx             = serviceActionIdx - ServiceActionsBaseIdx;
    actionIdx                  = serviceActionIdx ++;
    heapAddr                   = serviceHeapAddr;
    serviceHeapAddr           += heapBudget;
    actionAddr                 = serviceActionAddr;
    serviceActionAddr         += ActionBudget;

    actionBitParams.bitmap.action_valid = 1;
    actionBitParams.bitmap.israw      = 0;
    actionBitParams.bitmap.report_en  = 0;
    actionBitParams.bitmap.feedbck_en = 1;
    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"TcpFastPath");
    break;

  case ActionType::TcpFullPkt  :
    heapBudget                 = MAX_PKT_SIZE;

    localActionIdx             = serviceActionIdx - ServiceActionsBaseIdx;
    actionIdx                  = serviceActionIdx ++;
    heapAddr                   = serviceHeapAddr;
    serviceHeapAddr           += heapBudget;
    actionAddr                 = serviceActionAddr;
    serviceActionAddr         += ActionBudget;

    actionBitParams.bitmap.action_valid = 1;
    actionBitParams.bitmap.israw      = 1;
    actionBitParams.bitmap.report_en  = 0;
    actionBitParams.bitmap.feedbck_en = 0;
    dataTemplateAddr = rawPkt->getDataTemplateAddr();
    templateId       = rawPkt->id;
    strcpy(actionName,"TcpFullPkt");
    break;

  case ActionType::TcpEmptyAck :
    heapBudget                 = TCP_EMPTY_ACK_SIZE;

    localActionIdx             = serviceActionIdx - ServiceActionsBaseIdx;
    actionIdx                  = serviceActionIdx ++;
    heapAddr                   = serviceHeapAddr;
    serviceHeapAddr           += heapBudget;
    actionAddr                 = serviceActionAddr;
    serviceActionAddr         += ActionBudget;

    actionBitParams.bitmap.action_valid = 1;
    actionBitParams.bitmap.israw      = 0;
    actionBitParams.bitmap.report_en  = 0;
    actionBitParams.bitmap.feedbck_en = 0;

    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"TcpEmptyAck");
    break;

  case ActionType::UserAction :
    heapBudget                 = MAX_PKT_SIZE;

    localActionIdx             = localIdx;
    actionIdx                  = userActionIdx ++;

    heapAddr                   = userHeapAddr;
    userHeapAddr              += heapBudget;
    actionAddr                 = userActionAddr;
    userActionAddr            += ActionBudget;

    actionBitParams.bitmap.action_valid = 1;
    actionBitParams.bitmap.israw      = 0;
    actionBitParams.bitmap.report_en  = 1;
    actionBitParams.bitmap.feedbck_en = 1;

    actionBitParams.bitmap.empty_report_en = 1;

    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"UserAction");
    break;

  default:
    on_error("Unexpected EkaEpmAction type %d",(int)type);
  }

  /* EKA_LOG("--- %20s: %u:%u Action Idx: %u, Action addr: %8ju, Heap: %8ju + %4u", */
  /* 	  actionName, _coreId,_sessId,actionIdx,actionAddr,heapAddr,heapBudget); fflush(stderr); */

  if (userActionIdx > UserActionsBaseIdx + MaxUserActions) 
    on_error("userActionIdx %u > MaxUserActions %ju",userActionIdx, UserActionsBaseIdx + MaxUserActions);
  if (serviceActionIdx > ServiceActionsBaseIdx + MaxServiceActions) 
    on_error("serviceActionIdx %u > MaxServiceActions %ju",serviceActionIdx, ServiceActionsBaseIdx + MaxServiceActions);
  if (serviceHeapAddr > ServiceHeapBaseAddr + MaxServiceHeap)
    on_error("serviceHeapAddr %ju > MaxServiceHeap %ju",serviceHeapAddr, ServiceHeapBaseAddr + MaxServiceHeap);
  if (userHeapAddr > UserHeapBaseAddr + MaxUserHeap)
    on_error("userHeapAddr %ju > MaxUserHeap %ju",userHeapAddr, UserHeapBaseAddr + MaxUserHeap);

  EkaEpmAction* action = new EkaEpmAction(dev,
					  actionName,
					  type,
					  actionIdx,
					  localActionIdx,
					  actionRegion,
					  _coreId,
					  _sessId,
					  _auxIdx,
					  actionBitParams,
					  heapAddr,
					  
					  actionAddr,
					  dataTemplateAddr,
					  templateId
					  );
  if (action ==NULL) on_error("new EkaEpmAction = NULL");
  /* EKA_LOG("%s: idx = %3u, localIdx=%3u, heapAddr = 0x%jx, actionAddr = 0x%jx", */
  /* 	  actionName,actionIdx,actionIdx - 0,heapAddr,actionAddr); */

  createActionMtx.unlock();
  copyBuf2Hw(dev,EpmActionBase, (uint64_t*)&action->hwAction,sizeof(action->hwAction)); //write to scratchpad

  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,action->idx,0);

  return action;
}
/* ---------------------------------------------------- */
