/* SHURIK */
#include <arpa/inet.h>

#include "Efc.h"
#include "Eka.h"
#include "Epm.h"
#include "Exc.h"

#include "EkaEfc.h"
#include "EkaEpm.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaIgmp.h"
#include "EkaTcpSess.h"
#include "EkaUdpChannel.h"
#include "EpmFastPathTemplate.h"
#include "EpmFireBoeTemplate.h"
#include "EpmFireSqfTemplate.h"
#include "EpmRawPktTemplate.h"
#include "EpmStrategy.h"
#include "EpmTemplate.h"

void ekaFireReportThread(EkaDev *dev);

/* ---------------------------------------------------- */

EkaEpm::EkaEpm(EkaDev *_dev) {
  dev = _dev;

  const size_t strLen = 1024 * 64;
  auto epmRegionConfigString = new char[strLen];
  if (!epmRegionConfigString)
    on_error("failed creating epmRegionConfigString");

  EkaEpmRegion::printConfig(epmRegionConfigString, strLen);

  EKA_LOG("Created Epm with Regions:\n%s",
          epmRegionConfigString);
  delete[] epmRegionConfigString;
}
/* ---------------------------------------------------- */
int EkaEpm::createRegion(int regionId) {
  if (epmRegion[regionId])
    on_error("epmRegion[%d] already created", regionId);
  epmRegion[regionId] = new EkaEpmRegion(dev, regionId);
  if (!epmRegion[regionId])
    on_error("failed on new EkaEpmRegion");

  eka_write(dev, strategyEnableAddr(regionId),
            ALWAYS_ENABLE);

#ifndef _VERILOG_SIM
  initHeap(epmRegion[regionId]->baseHeapOffs,
           EkaEpmRegion::getHeapSize(regionId), regionId);
#endif

  return 0;
}

/* ---------------------------------------------------- */

void EkaEpm::initHeap(uint regionHeapBaseOffs,
                      uint regionHeapSize, uint regionId) {
  if (regionHeapSize % HeapPage != 0)
    on_error(
        "regionHeapSize %u is not multiple of HeapPage %ju",
        regionHeapSize, HeapPage);
  const int HeapWcPageSize = 1024;
  auto numPages = regionHeapSize / HeapWcPageSize;
  EKA_LOG("regionHeapBaseOffs=%u, regionHeapSize=%u, "
          "HeapWcPageSize=%d, numPages=%d",
          regionHeapBaseOffs, regionHeapSize,
          HeapWcPageSize, numPages);
  memset(&heap[regionHeapBaseOffs], 0, regionHeapSize);

  for (uint i = 0; i < numPages; i++) {
    uint64_t hwPageStart =
        regionHeapBaseOffs + i * HeapWcPageSize;
    uint64_t swPageStart =
        regionHeapBaseOffs + i * HeapWcPageSize;
    /* EKA_LOG("Cleaning hwPageStart=%ju + %d",
                         hwPageStart,HeapWcPageSize); */
    dev->ekaWc->epmCopyWcBuf(
        hwPageStart, &heap[swPageStart], HeapWcPageSize,
        0, // actionLocalIdx
        regionId);
    usleep(1); // preventing WC FIFO overrun
  }
}

/* ---------------------------------------------------- */

EkaOpResult
EkaEpm::raiseTriggers(const EpmTrigger *trigger) {
  if (!trigger)
    on_error("!trigger");
  if (!dev->fireReportThreadActive) {
    on_error(
        "fireReportThread is not active! Call efcRun()");
  }

  uint strategyId = trigger->strategy;
  uint actionId = trigger->action;
  EKA_LOG("Raising Trigger: strategyId %u, ActionId %u",
          strategyId, actionId);
  fflush(stderr);

  auto s = strategy[strategyId];
  if (!s)
    on_error("!strategy[%u]", strategyId);

  auto a = s->action[actionId];
  if (!a)
    on_error("!strategy[%u]->action[%u]", strategyId,
             actionId);

#if 0
  EKA_LOG("Accepted Trigger: token=0x%jx, strategyId=%d, "
					"FistActionId=%d, ekaAction->idx = %u, "
					"epm_trig_desc.str.action_index=%u",
					trigger->token,strategyId,actionId,
					a->idx,
					epm_trig_desc.str.action_index);
#endif
  a->send();

#if 0
  EKA_LOG("User Action: actionType = %u, strategyId=%u, "
					"actionId=%u,heapAddr=%ju, pktSize=%u",
  	  a->hwAction.tcpCsSizeSource,
  	  strategyId,actionId,
  	  a->hwAction.data_db_ptr,
  	  a->hwAction.payloadSize
  	  );
  eka_write(dev,EPM_TRIGGER_DESC_ADDR,epm_trig_desc.desc);
#endif
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
  /*   EkaEpmAction* fireAction =
   * dev->efc->createFireAction(actionIdx,
   * epmAction->hConn); */
  /*   if (fireAction == NULL) on_error("fireAction ==
   * NULL"); */

  /*   fireAction->updateAttrs(excGetCoreId(epmAction->hConn),
   */
  /* 			    excGetSessionId(epmAction->hConn),
   */
  /* 			    epmAction); */

  /*   fireAction->updatePayload(epmAction->offset,epmAction->length);
   */

  /*   return EKA_OPRESULT__OK; */
  /* } */

  if (!initialized) {
    EKA_WARN("EKA_OPRESULT__ERR_EPM_UNINITALIZED");
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  }

  if (!validStrategyIdx(strategyIdx)) {
    EKA_WARN("EKA_OPRESULT__ERR_INVALID_STRATEGY: "
             "strategyIdx=%d",
             strategyIdx);
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  }
  if (!strategy[strategyIdx]->myAction(actionIdx)) {
    EKA_WARN("EKA_OPRESULT__ERR_INVALID_ACTION: "
             "strategyIdx=%d, actionIdx=%d",
             strategyIdx, actionIdx);
    return EKA_OPRESULT__ERR_INVALID_ACTION;
  }
  EKA_LOG("Setting Action %d epmAction->type=\'%s\'",
          actionIdx, printActionType(epmAction->type));
  return strategy[strategyIdx]->setAction(actionIdx,
                                          epmAction);
}
/* ---------------------------------------------------- */
EkaOpResult EkaEpm::getAction(epm_strategyid_t strategyIdx,
                              epm_actionid_t actionIdx,
                              EpmAction *epmAction) {
  if (!initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (!validStrategyIdx(strategyIdx))
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;
  if (!strategy[strategyIdx]->myAction(actionIdx))
    return EKA_OPRESULT__ERR_INVALID_ACTION;

  return strategy[strategyIdx]->getAction(actionIdx,
                                          epmAction);
}
/* ---------------------------------------------------- */

EkaOpResult EkaEpm::enableController(EkaCoreId coreId,
                                     bool enable) {
  controllerEnabled = enable;

  uint64_t fire_rx_en = eka_read(dev, ENABLE_PORT);
  if (enable) {
    fire_rx_en |= 1ULL << (coreId);      // rx
    fire_rx_en |= 1ULL << (16 + coreId); // fire
    EKA_LOG("Controller enabled for coreId %u (0x%016jx)",
            coreId, fire_rx_en);
  } else {
    fire_rx_en &= ~(1ULL << (coreId));      // rx
    fire_rx_en &= ~(1ULL << (16 + coreId)); // fire
    EKA_LOG("Controller disabled for coreId %u (0x%016jx)",
            coreId, fire_rx_en);
  }
  eka_write(dev, ENABLE_PORT, fire_rx_en);

  return EKA_OPRESULT__OK;
}

/* ---------------------------------------------------- */

EkaOpResult
EkaEpm::initStrategies(const EpmStrategyParams *params,
                       epm_strategyid_t numStrategies) {
  if (numStrategies > EkaEpmRegion::MaxStrategies)
    on_error("numStrategies %u > MaxStrategies %d",
             numStrategies, EkaEpmRegion::MaxStrategies);

  stratNum = numStrategies;

  createRegion(EkaEpmRegion::Regions::EfcMc);

  epm_actionid_t currActionIdx = 0;

  //  for (auto i = 0; i < EkaEpmRegion::MaxStrategies; i++)
  //  {
  for (auto i = 0; i < stratNum; i++) {

    EKA_LOG("Imitializing strategy %d, hwFeedVer=%d", i,
            (int)dev->hwFeedVer);
    if (epmRegion[i])
      on_error("epmRegion[%d] is already initialized", i);
    createRegion(i);

    if (strategy[i])
      on_error("strategy[%d] is already initialized", i);

    if (i == EFC_STRATEGY) {
      dev->ekaHwCaps->checkEfc();
      strategy[i] = new EkaEfc(this, i, currActionIdx,
                               &params[i], dev->hwFeedVer);
    } else {
      strategy[i] =
          new EpmStrategy(this, i, currActionIdx,
                          &params[i], dev->hwFeedVer);
    }
    if (!strategy[i])
      on_error("Fail to create strategy[%d]", i);

    currActionIdx += params[i].numActions;

    /*     if (currActionIdx > (int)MaxUserActions)
          on_error("currActionIdx %d > MaxUserActions
       %ju", currActionIdx,MaxUserActions); */
  }

  initialized = true;
  return EKA_OPRESULT__OK;
}
/* ---------------------------------------------------- */

EkaOpResult
EkaEpm::setStrategyEnableBits(epm_strategyid_t strategyIdx,
                              epm_enablebits_t enable) {
  if (!initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;

  if (!validStrategyIdx(strategyIdx))
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->setEnableBits(enable);
}
/* ---------------------------------------------------- */

EkaOpResult
EkaEpm::getStrategyEnableBits(epm_strategyid_t strategyIdx,
                              epm_enablebits_t *enable) {
  if (!initialized)
    return EKA_OPRESULT__ERR_EPM_UNINITALIZED;
  if (!validStrategyIdx(strategyIdx))
    return EKA_OPRESULT__ERR_INVALID_STRATEGY;

  return strategy[strategyIdx]->getEnableBits(enable);
}

/* ---------------------------------------------------- */

EkaOpResult
EkaEpm::payloadHeapCopy(epm_strategyid_t strategyIdx,
                        uint32_t offset, uint32_t length,
                        const void *contents,
                        const bool isUdpDatagram) {
  uint64_t payloadOffset =
      isUdpDatagram ? UdpDatagramOffset : TcpDatagramOffset;

  if ((offset - payloadOffset) % PayloadAlignment != 0) {
    EKA_WARN("offset (%d) - payloadOffset "
             "(%d) %% PayloadAlignment (=%d) != 0",
             (int)offset, (int)payloadOffset,
             (int)PayloadAlignment);
    return EKA_OPRESULT__ERR_INVALID_ALIGN;
  }

  if (offset + length >
      EkaEpmRegion::getWritableHeapSize()) {
    EKA_WARN("offset %d + length %d > PayloadMemorySize %u",
             (int)offset, (int)length,
             EkaEpmRegion::getWritableHeapSize());
    return EKA_OPRESULT__ERR_INVALID_OFFSET;
  }

  memcpy(&heap[offset], contents, length);

  return EKA_OPRESULT__OK;
}

/* ---------------------------------------------------- */

void EkaEpm::InitDefaultTemplates() {
  EKA_LOG("Inititializing TcpFastPath and Raw templates");

  template[TemplateId::TcpFastPath] =
      new EpmFastPathTemplate(TemplateId::TcpFastPath);

  DownloadSingleTemplate2HW(
      template[TemplateId::TcpFastPath]);

  template[TemplateId::Raw] =
      new EpmFastPathTemplate(TemplateId::Raw);
  DownloadSingleTemplate2HW(template[TemplateId::Raw]);
}
/* ---------------------------------------------------- */

int EkaEpm::DownloadSingleTemplate2HW(EpmTemplate *t) {
  if (!t)
    on_error("!t");

  EKA_LOG("Downloading %s, id=%u, getDataTemplateAddr=%jx, "
          "getCsumTemplateAddr=%jx ",
          t->name, t->id, t->getDataTemplateAddr(),
          t->getCsumTemplateAddr());

  copyBuf2Hw_swap4(dev, t->getDataTemplateAddr(),
                   (uint64_t *)t->data, EpmMaxRawTcpSize);
  copyBuf2Hw(dev, t->getCsumTemplateAddr(),
             (uint64_t *)&t->hw_tcpcs_template,
             sizeof(t->hw_tcpcs_template));

  return 0;
}
/* ---------------------------------------------------- */

/* ---------------------------------------------------- */
void EkaEpm::actionParamsSanityCheck(ActionType type,
                                     int regionId,
                                     uint8_t _coreId,
                                     uint8_t _sessId) {
  if (type == ActionType::UserAction)
    return;

  if ((type == ActionType::TcpFullPkt ||
       type == ActionType::TcpFastPath) &&
      regionId != EkaEpmRegion::Regions::TcpTxFullPkt)
    on_error("regionId %u doesnt match actionType %d",
             regionId, (int)type);

  if ((type == ActionType::TcpEmptyAck) &&
      regionId != EkaEpmRegion::Regions::TcpTxEmptyAck)
    on_error("regionId %u doesnt match actionType %d",
             regionId, (int)type);

  if (_coreId >= MAX_CORES)
    on_error("coreId %u > MAX_CORES %u", _coreId,
             MAX_CORES);

  if (_sessId >= TOTAL_SESSIONS_PER_CORE)
    on_error("sessId %u >= TOTAL_SESSIONS_PER_CORE %u",
             _sessId, TOTAL_SESSIONS_PER_CORE);
}
/* ---------------------------------------------------- */

EkaEpmAction *
EkaEpm::addAction(ActionType type, int regionId,
                  epm_actionid_t _localIdx, uint8_t _coreId,
                  uint8_t _sessId, uint8_t _auxIdx) {

  if (regionId >= EPM_REGIONS ||
      epmRegion[regionId] == NULL)
    on_error("wrong epmRegion[%u] = %p", regionId,
             epmRegion[regionId]);

  actionParamsSanityCheck(type, regionId, _coreId, _sessId);

  //  uint            heapBudget      = getHeapBudget(type);

  createActionMtx.lock();

  epm_actionid_t localActionIdx = -1;

  switch (type) {
  case ActionType::TcpEmptyAck:
  case ActionType::TcpFullPkt:
  case ActionType::TcpFastPath:
    localActionIdx =
        _coreId * TOTAL_SESSIONS_PER_CORE + _sessId;
    break;
  default:
    localActionIdx = epmRegion[regionId]->localActionIdx++;
  }

  EKA_LOG("Action %d: %s, regionId=%u, heapOffs=%u",
          (int)localActionIdx, printActionType(type),
          regionId, heapOffs);

  EkaEpmRegion::sanityCheckActionId(regionId,
                                    (int)localActionIdx);

  epm_actionid_t actionIdx =
      EkaEpmRegion::getBaseActionIdx(regionId) +
      localActionIdx;

  EkaEpmAction *ekaA =
      new EkaEpmAction(dev, type, actionIdx, localActionIdx,
                       regionId, _coreId, _sessId, _auxIdx);

  if (!ekaA)
    on_error("!ekaA");

  copyBuf2Hw(dev, EpmActionBase,
             (uint64_t *)&ekaA->hwAction,
             sizeof(ekaA->hwAction)); // write to scratchpad

  atomicIndirectBufWrite(dev, 0xf0238 /* ActionAddr */, 0,
                         0, ekaA->idx, 0);
  createActionMtx.unlock();

  return ekaA;
}
/* ---------------------------------------------------- */
epm_actionid_t
EkaEpm::allocateAction(EpmActionType actionType) {
  createActionMtx.lock();
  auto regionId = EkaEpmRegion::Regions::Efc;

  if (nActions_ == getMaxActions(regionId))
    on_error("Out of free actions: nActions_ = %d",
             nActions_);

  auto globalIdx = nActions_;

  if (a_[globalIdx])
    on_error("a_[%d] already exists", globalIdx);

  auto localIdx =
      globalIdx - EkaEpmRegion::getBaseActionIdx(regionId);

  a_[globalIdx] = new EkaEpmAction(
      dev_, actionType, globalIdx, localIdx, regionId,
      -1 /* coreId */, -1 /* sessId */, -1 /* auxIdx */);

  if (!a_[globalIdx])
    on_error("Failed creating a_[%d]", globalIdx);

  nActions_++;

  createActionMtx.unlock();

  return globalIdx;
}