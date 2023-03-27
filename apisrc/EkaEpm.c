/* SHURIK */
#include <arpa/inet.h>

#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "Epm.h"

#include "EkaEpm.h"
#include "EpmTemplate.h"
#include "EpmFastPathTemplate.h"
#include "EpmRawPktTemplate.h"
#include "EpmFireSqfTemplate.h"
#include "EpmFireBoeTemplate.h"
#include "EkaTcpSess.h"
#include "EkaEpmAction.h"
#include "EpmStrategy.h"
#include "EkaEpmRegion.h"
#include "EkaIgmp.h"
#include "EkaEfc.h"
#include "EkaUdpChannel.h"
#include "EkaHwCaps.h"

void ekaFireReportThread(EkaDev* dev);

/* ---------------------------------------------------- */

EkaEpm::EkaEpm(EkaDev* _dev) {
  dev = _dev;

  EKA_LOG("Created Epm");
}
/* ---------------------------------------------------- */
int EkaEpm::createRegion(uint regionId, epm_actionid_t baseActionIdx) {
  epmRegion[regionId] = new EkaEpmRegion(dev, regionId, baseActionIdx);
  if (epmRegion[regionId] == NULL) on_error("failed on new EkaEpmRegion");

  eka_write(dev,strategyEnableAddr(regionId), ALWAYS_ENABLE);

#ifndef _VERILOG_SIM
  initHeap(epmRegion[regionId]->baseHeapOffs,HeapPerRegion,regionId);
#endif

  return 0;
}

/* ---------------------------------------------------- */

void EkaEpm::initHeap(uint regionHeapBaseOffs, uint regionHeapSize, uint regionId) {
  if (regionHeapSize % HeapPage != 0) 
    on_error("regionHeapSize %u is not multiple of HeapPage %ju",
	     regionHeapSize, HeapPage);
  const int HeapWcPageSize = 1024;
  auto numPages = regionHeapSize / HeapWcPageSize;
  EKA_LOG("regionHeapBaseOffs=%u, regionHeapSize=%u, HeapWcPageSize=%d, numPages=%d",
	  regionHeapBaseOffs,regionHeapSize,HeapWcPageSize,numPages);
  memset(&heap[regionHeapBaseOffs],0,regionHeapSize);

  for (uint i = 0; i < numPages; i++) {
    uint64_t hwPageStart = regionHeapBaseOffs + i * HeapWcPageSize;
    uint64_t swPageStart = regionHeapBaseOffs + i * HeapWcPageSize;
    /* EKA_LOG("Cleaning hwPageStart=%ju + %d",hwPageStart,HeapWcPageSize); */
#if 1
    dev->ekaWc->epmCopyWcBuf(hwPageStart,
			     &heap[swPageStart],
			     HeapWcPageSize,
			     EkaWc::AccessType::HeapInit,
			     0, // actionLocalIdx
			     regionId,
			     0, // tcpPseudoCsum
			     EkaWc::SendOp::DontSend);
#endif    
  }
}

/* ---------------------------------------------------- */

EkaOpResult EkaEpm::raiseTriggers(const EpmTrigger *trigger) {
  if (trigger == NULL) on_error("trigger == NULL");
  if (! dev->fireReportThreadActive) {
    on_error("fireReportThread is not active! Call efcRun()");
  }
  
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

EkaOpResult EkaEpm::setAction(epm_strategyid_t strategyIdx,
			      epm_actionid_t actionIdx,
			      const EpmAction *epmAction) {

  /* if (strategyIdx == EFC_STRATEGY) { */
  /*   if (dev->efc == NULL) { */
  /*     EKA_WARN ("EKA_OPRESULT__ERR_EFC_UNINITALIZED"); */
  /*     return EKA_OPRESULT__ERR_EFC_UNINITALIZED; */
  /*   } */
  /*   EKA_LOG("Creating EFC Action %u",actionIdx); */
  /*   EkaEpmAction* fireAction = dev->efc->createFireAction(actionIdx, epmAction->hConn); */
  /*   if (fireAction == NULL) on_error("fireAction == NULL"); */

  /*   fireAction->updateAttrs(excGetCoreId(epmAction->hConn), */
  /* 			    excGetSessionId(epmAction->hConn), */
  /* 			    epmAction); */

  /*   fireAction->updatePayload(epmAction->offset,epmAction->length); */

  /*   return EKA_OPRESULT__OK; */
  /* } */


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
  EKA_LOG("Setting Action %d epmAction->type=%d",actionIdx,(int)epmAction->type);
  return strategy[strategyIdx]->setAction(actionIdx,epmAction);
}
/* ---------------------------------------------------- */
EkaOpResult EkaEpm::getAction(epm_strategyid_t strategyIdx,
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

  uint64_t fire_rx_en = eka_read(dev,ENABLE_PORT);
  if (enable) {
    fire_rx_en |= 1ULL << (coreId); //rx
    fire_rx_en |= 1ULL << (16 + coreId); // fire
    EKA_LOG ("Controller enabled for coreId %u (0x%016jx)",coreId,fire_rx_en);
  } else {
    fire_rx_en &= ~(1ULL << (coreId)); //rx
    fire_rx_en &= ~(1ULL << (16 + coreId)); // fire
    EKA_LOG ("Controller disabled for coreId %u (0x%016jx)",coreId,fire_rx_en);
  }
  eka_write(dev,ENABLE_PORT,fire_rx_en);

  return EKA_OPRESULT__OK;
}


/* ---------------------------------------------------- */

EkaOpResult EkaEpm::initStrategies(const EpmStrategyParams *params,
				   epm_strategyid_t numStrategies) {
  if (numStrategies > MaxStrategies) 
    on_error("numStrategies %u > MaxStrategies %d",numStrategies,MaxStrategies);

  stratNum = numStrategies;

  /* if (! dev->fireReportThreadActive) { */
  /*   dev->fireReportThread = std::thread(ekaFireReportThread,dev); */
  /*   dev->fireReportThread.detach(); */
  /*   while (! dev->fireReportThreadActive) sleep(0); */
  /*   EKA_LOG("fireReportThread activated"); */
  /* } */

  // allocating UDP Channel to EPM MC region (preventing collision with EFH)
  /* auto udpCh    = new EkaUdpChannel(dev,params[0].triggerParams->coreId,EpmMcRegion); */
  /* if (udpCh == NULL) on_error("udpCh == NULL"); */
  /* if (udpCh->chId != EpmMcRegion) */
  /*   on_error("EpmMcRegion udpCh->chId %u != %u",udpCh->chId,EpmMcRegion); */
  
  createRegion(EpmMcRegion,EpmMcRegion * ActionsPerRegion);

  epm_actionid_t currActionIdx = 0;

  for (auto i = 0; i < stratNum; i++) {
    EKA_LOG("Imitializing strategy %d, hwFeedVer=%d",i,(int)dev->hwFeedVer);
    if (epmRegion[i] != NULL) on_error("epmRegion[%d] != NULL",i);
    createRegion((uint)i,currActionIdx);

    if (strategy[i] != NULL) on_error("strategy[%d] != NULL",i);

    // allocating UDP Channel to EPM region (preventing collision with EFH)
    /* auto udpCh    = new EkaUdpChannel(dev,params[i].triggerParams->coreId,-1); */
    /* if (udpCh == NULL) on_error("udpCh == NULL"); */
    
    if (i == EFC_STRATEGY) {
      dev->ekaHwCaps->checkEfc();
      strategy[i] = new EkaEfc(this,i,currActionIdx, &params[i],dev->hwFeedVer);
    } else {
      strategy[i] = new EpmStrategy(this,i,currActionIdx, &params[i],dev->hwFeedVer);
    }
    if (strategy[i] == NULL) on_error("Fail to create strategy[%d]",i);

    currActionIdx += params[i].numActions;

    if (currActionIdx > (int)MaxUserActions) 
      on_error("currActionIdx %d > MaxUserActions %ju",currActionIdx,MaxUserActions);
  }

  initialized = true;
  return EKA_OPRESULT__OK;
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::setStrategyEnableBits(epm_strategyid_t strategyIdx,
					  epm_enablebits_t enable) {
  if (! initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;

  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->setEnableBits(enable);
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::getStrategyEnableBits(epm_strategyid_t strategyIdx,
					  epm_enablebits_t *enable) {
  if (! initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (! validStrategyIdx(strategyIdx)) return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->getEnableBits(enable);
}

/* ---------------------------------------------------- */

EkaOpResult EkaEpm::payloadHeapCopy(epm_strategyid_t strategyIdx, 
				    uint32_t offset,
				    uint32_t length, 
				    const void *contents, const bool isUdpDatagram) {
  uint64_t payloadOffset = isUdpDatagram ? UdpDatagramOffset : TcpDatagramOffset;

  if ((offset - payloadOffset) % PayloadAlignment != 0) {
    EKA_WARN("offset (%d) - payloadOffset (%d) %% PayloadAlignment (=%d) != 0",
	     (int)offset,(int)payloadOffset,(int)PayloadAlignment);
    return EKA_OPRESULT__ERR_INVALID_ALIGN;
  }
       
  if (offset + length > PayloadMemorySize) {
    EKA_WARN("offset %d + length %d > PayloadMemorySize %d",
	     (int)offset,(int)length,(int)PayloadMemorySize);
    return EKA_OPRESULT__ERR_INVALID_OFFSET;
  }

  memcpy(&heap[offset],contents,length);

  return EKA_OPRESULT__OK;
}

/* ---------------------------------------------------- */

int EkaEpm::InitTemplates() {
  templatesNum   = 0;

  tcpFastPathPkt = new EpmFastPathTemplate(templatesNum++);
  if (! tcpFastPathPkt) on_error("! tcpFastPathPkt");

  rawPkt         = new EpmRawPktTemplate(templatesNum++);
  if (! rawPkt) on_error("! rawPkt");

  return 0;
}
/* ---------------------------------------------------- */

int EkaEpm::DownloadSingleTemplate2HW(EpmTemplate* t) {
  if (t == NULL) on_error("t == NULL");

  EKA_LOG("Downloading %s, id=%u, getDataTemplateAddr=%jx, getCsumTemplateAddr=%jx ",
	  t->name,t->id,t->getDataTemplateAddr(),t->getCsumTemplateAddr());
  /* volatile epm_tcpcs_template_t hw_tcpcs_template = {}; */

  // TCP CS template
  /* for (uint f = 0; f < EpmNumHwFields; f++) { */
  /*   for (uint b = 0; b < EpmHwFieldSize; b++) { */
  /*     if (t->hwField[f].cksmMSB[b]) { */
  /* 	uint16_t temp = hw_tcpcs_template.high.field[f].bitmap | */
  /* 	  ((uint16_t)1)<<b; */
  /* 	hw_tcpcs_template.high.field[f].bitmap = temp; */
  /* 	//	hw_tcpcs_template.high.field[f].bitmap |= ((uint16_t)1)<<b; */
  /*     } */
  /*     if (t->hwField[f].cksmLSB[b]) { */
  /* 	uint16_t temp = hw_tcpcs_template.low.field[f].bitmap | */
  /* 	  ((uint16_t)1)<<b; */
  /* 	hw_tcpcs_template.low.field[f].bitmap = temp; */
  /* 	//	hw_tcpcs_template.low.field[f].bitmap  |= ((uint16_t)1)<<b; */
  /*     } */
  /*   } */
  /* } */

  copyBuf2Hw_swap4(dev,t->getDataTemplateAddr(),
		   (uint64_t*) t->data ,
		   EpmMaxRawTcpSize);
  copyBuf2Hw      (dev,t->getCsumTemplateAddr(),
		   (uint64_t*) &t->hw_tcpcs_template,
		   sizeof(t->hw_tcpcs_template));

  return 0;
}
/* ---------------------------------------------------- */


int EkaEpm::DownloadTemplates2HW() {
  EKA_LOG("Downloading %u templates",templatesNum);

  DownloadSingleTemplate2HW(tcpFastPathPkt);
  DownloadSingleTemplate2HW(rawPkt);
  //  DownloadSingleTemplate2HW(hwFire);

  return 0;
}

/* ---------------------------------------------------- */
void EkaEpm::actionParamsSanityCheck(ActionType type, 
				     uint       actionRegion, 
				     uint8_t    _coreId, 
				     uint8_t    _sessId) {
  if (type == ActionType::UserAction)
    return;

  if ((type == ActionType::TcpFullPkt  ||
       type == ActionType::TcpFastPath ||
       type == ActionType::TcpEmptyAck) &&
      actionRegion != TcpTxRegion)
    on_error("actionRegion %u doesnt match actionType %d",
	     actionRegion, (int)type);

  if (_coreId >= MAX_CORES)
    on_error("coreId %u > MAX_CORES %u",_coreId,MAX_CORES);

  if (_sessId >= TOTAL_SESSIONS_PER_CORE)
    on_error("sessId %u >= TOTAL_SESSIONS_PER_CORE %u",
	     _sessId, TOTAL_SESSIONS_PER_CORE);
       
}
/* ---------------------------------------------------- */

EkaEpmAction* EkaEpm::addAction(ActionType     type, 
				uint           actionRegion, 
				epm_actionid_t _localIdx, 
				uint8_t        _coreId, 
				uint8_t        _sessId, 
				uint8_t        _auxIdx) {
  EKA_LOG("Action %d: type=%d, actionRegion=%u,heapOffs=%u",
	  (int)_localIdx,(int)type,actionRegion,
	  epmRegion[actionRegion]->heapOffs);
  
  if (actionRegion >= EPM_REGIONS || epmRegion[actionRegion] == NULL) 
    on_error("wrong epmRegion[%u] = %p",
	     actionRegion,epmRegion[actionRegion]);
  actionParamsSanityCheck(type,actionRegion,_coreId,_sessId);

  uint            heapBudget      = getHeapBudget(type);

  createActionMtx.lock();

  epm_actionid_t  localActionIdx = -1;

  switch (type) {
  case ActionType::TcpEmptyAck :
    localActionIdx = _coreId * TOTAL_SESSIONS_PER_CORE +
      _sessId * ActionsPerTcpSess;
    break;
  case ActionType::TcpFullPkt :
  case ActionType::TcpFastPath :
    localActionIdx = _coreId * TOTAL_SESSIONS_PER_CORE +
      _sessId * ActionsPerTcpSess + 1;
    break;
  default:
    localActionIdx = epmRegion[actionRegion]->localActionIdx++;
  }
  if (localActionIdx >= (int)ActionsPerRegion)
    on_error("localActionIdx %d >= ActionsPerRegion %u",
	     localActionIdx, ActionsPerRegion);

  
  epm_actionid_t  actionIdx      = epmRegion[actionRegion]->baseActionIdx + localActionIdx;
  uint            heapOffs       = epmRegion[actionRegion]->heapOffs;

  uint64_t actionAddr = EpmActionBase + actionIdx * ActionBudget;
  epmRegion[actionRegion]->heapOffs += heapBudget;

  if (epmRegion[actionRegion]->heapOffs -
      epmRegion[actionRegion]->baseHeapOffs > HeapPerRegion)
    on_error("heapOffs %u - baseHeapOffs %u (=%u) >= HeapPerRegion %u",
	     epmRegion[actionRegion]->heapOffs,
	     epmRegion[actionRegion]->baseHeapOffs,
	     epmRegion[actionRegion]->heapOffs -
	     epmRegion[actionRegion]->baseHeapOffs,
	     HeapPerRegion);
  
  EkaEpmAction* action = new EkaEpmAction(dev,
					  type,
					  actionIdx,
					  localActionIdx,
					  actionRegion,
					  _coreId,
					  _sessId,
					  _auxIdx,
					  heapOffs,					  
					  actionAddr
					  );

  if (action ==NULL) on_error("new EkaEpmAction = NULL");
  /* EKA_LOG("%s: idx = %3u, localIdx=%3u, heapOffs = 0x%jx, actionAddr = 0x%jx", */
  /* 	  actionName,actionIdx,localActionIdx,heapOffs,actionAddr); */

  createActionMtx.unlock();
  copyBuf2Hw(dev,EpmActionBase, (uint64_t*)&action->hwAction,sizeof(action->hwAction)); //write to scratchpad

  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,action->idx,0);

  return action;
}
/* ---------------------------------------------------- */
