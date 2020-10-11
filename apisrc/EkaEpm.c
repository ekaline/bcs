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

EkaOpResult EkaEpm::setAction(EkaCoreId coreId,
			      epm_strategyid_t strategyIdx,
			      epm_actionid_t actionIdx,
			      const EpmAction *epmAction) {
  if (! initialized) {
    EKA_WARN ("EKA_OPRESULT__ERR_EPM_UNINITALIZED");
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  }
  if (! validStrategyIdx(strategyIdx)) {
    EKA_WARN ("EKA_OPRESULT__ERR_INVALID_STRATEGY");
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  }
  if (! strategy[strategyIdx]->myAction(actionIdx)){
    EKA_WARN ("EKA_OPRESULT__ERR_INVALID_ACTION");
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
  stratNum = numStrategies;

  if (udpCh[coreId] == NULL) 
    udpCh[coreId] = new EkaUdpChannel(dev,coreId);

  epm_actionid_t currActionIdx = 0;
  for (auto i = 0; i < stratNum; i++) {
    strategy[i] = new EpmStrategy(this,i,currActionIdx, &params[i]);
    for (auto j = currActionIdx; j < currActionIdx + params[i].numActions; j++) {
      userAction[j] = addUserAction(j);
    }
    currActionIdx += params[i].numActions;
  }

  //  swProcessor = std::thread(&EkaEpm::swEpmProcessor,this);

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
  if (offset % EkaEpm::PayloadAlignment != 0)
    return EKA_OPRESULT__ERR_INVALID_ALIGN;
       
  if (offset + length > EkaEpm::PayloadMemorySize) 
    return EKA_OPRESULT__ERR_INVALID_OFFSET;
  // TO BE FIXED!!!
  memcpy(heap + offset,contents,length);
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
// Add Service Action
EkaEpmAction* EkaEpm::addAction(ActionType type, uint8_t _coreId, uint8_t _sessId, uint8_t _productIdx) {
  if (tcpFastPathPkt == NULL) on_error("tcpFastPathPkt == NULL");
  if (rawPkt == NULL) on_error("rawPkt == NULL");

  uint                    heapBudget = (uint)(-1);
  epm_actione_bitparams_t actionBitParams = {};
  uint64_t                dataTemplateAddr = -1;
  uint                    templateId = (uint)(-1);

  char actionName[30] = {};

  createActionMtx.lock();

  switch (type) {
  case ActionType::TcpFastPath :
    heapBudget                 = MAX_PKT_SIZE;
    actionBitParams.israw      = 0;
    actionBitParams.report_en  = 0;
    actionBitParams.feedbck_en = 1;
    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"TcpFastPath");
    break;

  case ActionType::TcpFullPkt  :
    heapBudget                 = MAX_PKT_SIZE;
    actionBitParams.israw      = 1;
    actionBitParams.report_en  = 0;
    actionBitParams.feedbck_en = 0;
    dataTemplateAddr = rawPkt->getDataTemplateAddr();
    templateId       = rawPkt->id;
    strcpy(actionName,"TcpFullPkt");
    break;

  case ActionType::TcpEptyAck :
    heapBudget                 = TCP_EMPTY_ACK_SIZE;
    actionBitParams.israw      = 0;
    actionBitParams.report_en  = 0;
    actionBitParams.feedbck_en = 0;
    dataTemplateAddr = tcpFastPathPkt->getDataTemplateAddr();
    templateId       = tcpFastPathPkt->id;
    strcpy(actionName,"TcpEptyAck");
    break;

  default:
    on_error("Unexpected EkaEpmAction type %d",(int)type);
  }

  EkaEpmAction* action = new EkaEpmAction(dev,
					  actionName,
					  type,
					  serviceActionIdx,
					  _coreId,
					  _sessId,
					  _productIdx,
					  actionBitParams,
					  serviceHeapAddr,
					  serviceActionAddr,
					  dataTemplateAddr,
					  templateId
					  );
  EKA_LOG("Service Action: idx = %u, heapAddr = 0x%jx, actionAddr = 0x%jx",
	  serviceActionIdx,serviceHeapAddr,serviceActionAddr);

  serviceActionIdx ++;
  serviceHeapAddr   += heapBudget;
  serviceActionAddr += ActionBudget;

  createActionMtx.unlock();

  copyBuf2Hw(dev,EpmActionBase, (uint64_t*)&action->hwAction,sizeof(action->hwAction)); //write to scratchpad
  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,0,action->idx,0);
  

  return action;
}
/* ---------------------------------------------------- */

EkaEpmAction* EkaEpm::addUserAction(epm_actionid_t userActionIdx) {
  epm_actione_bitparams_t actionBitParams = {};
  actionBitParams.israw      = 0;
  actionBitParams.report_en  = 1;
  actionBitParams.feedbck_en = 1;

  EkaEpmAction* action = new EkaEpmAction(dev,
					  (char*)"UserAction (TcpFastPath)",
					  ActionType::UserAction,
					  userActionIdx,
					  -1, //coreId,
					  -1, //sessId,
					  -1, //auxIdx,
					  actionBitParams,
					  -1, //heapAddr,
					  userActionAddr,
					  tcpFastPathPkt->getDataTemplateAddr(), //dataTemplateAddr,
					  tcpFastPathPkt->id  //templateId
					  );

  /* copyBuf2Hw(dev,EpmActionBase, (uint64_t*)&action->hwAction,sizeof(action->hwAction)); //write to scratchpad */
  /* atomicIndirectBufWrite(dev, 0xf0238 /\* ActionAddr *\/, 0,0,action->idx,0); */
  
  return action;
}
