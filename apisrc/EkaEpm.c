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

/* ---------------------------------------------------- */

EkaEpm::EkaEpm(EkaDev *_dev) {
  dev = _dev;
  active_ = true;

  writeAction2FpgaMtx_file_ = new EkaFileLock(
      "/tmp/_eka_writeAction2FpgaMtx_.lock", &active_);

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
EkaEpm::~EkaEpm() {
  active_ = false;
  delete writeAction2FpgaMtx_file_;
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

  auto nActions = EkaEpmRegion::getMaxActions(regionId);
  EKA_LOG("%s: Resetting %d Actions and memory %d..%d",
          EkaEpmRegion::getRegionName(regionId), nActions,
          EkaEpmRegion::getBaseHeapOffs(regionId),
          EkaEpmRegion::getBaseHeapOffs(regionId) +
              EkaEpmRegion::getHeapSize(regionId) - 1);

#ifndef _VERILOG_SIM
  for (uint i = 0; i < nActions; i++) {
    epm_action_t emptyAction = {};
    auto globalIdx =
        EkaEpmRegion::getBaseActionIdx(regionId) + i;
    EkaEpmAction::copyHwActionParams2Fpga(
        &emptyAction, globalIdx, regionId, i, this);
  }

  initHeap(regionId, 0x0);
#endif

  return 0;
}

/* ---------------------------------------------------- */

void EkaEpm::initHeap(int regionId, uint8_t payloadByte) {
  auto regionHeapSize = EkaEpmRegion::getHeapSize(regionId);

  auto regionHeapBaseOffs =
      EkaEpmRegion::getBaseHeapOffs(regionId);

  if (regionHeapSize % HeapPage != 0)
    on_error("regionHeapSize %u is not multiple of "
             "HeapPage %ju",
             regionHeapSize, HeapPage);
  const int HeapWcPageSize = 1024;
  auto numPages = regionHeapSize / HeapWcPageSize;
  EKA_LOG(
      "Initializing region %u regionHeapBaseOffs=%u, "
      "regionHeapSize=%u, "
      "HeapWcPageSize=%d, numPages=%d, payloadByte = 0x%x",
      regionId, regionHeapBaseOffs, regionHeapSize,
      HeapWcPageSize, numPages, payloadByte);

  memset(&heap[regionHeapBaseOffs], payloadByte,
         regionHeapSize);

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

void EkaEpm::InitDefaultTemplates() {
  EKA_LOG("Inititializing TcpFastPath and Raw templates");

  epmTemplate[(int)TemplateId::TcpFastPath] =
      new EpmFastPathTemplate(
          (uint)TemplateId::TcpFastPath);

  DownloadSingleTemplate2HW(
      epmTemplate[(int)TemplateId::TcpFastPath]);

  epmTemplate[(int)TemplateId::Raw] =
      new EpmRawPktTemplate((uint)TemplateId::Raw);

  DownloadSingleTemplate2HW(
      epmTemplate[(int)TemplateId::Raw]);
}
/* ---------------------------------------------------- */

void EkaEpm::DownloadSingleTemplate2HW(EpmTemplate *t) {
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
}

/* ---------------------------------------------------- */
void EkaEpm::actionParamsSanityCheck(ActionType type,
                                     int regionId) {
  if (regionId >= EkaEpmRegion::Regions::Total)
    on_error("Wrong regionId %d", regionId);

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
  /*
    if (_coreId >= MAX_CORES)
      on_error("coreId %u > MAX_CORES %u", _coreId,
               MAX_CORES);

    if (_sessId >= TOTAL_SESSIONS_PER_CORE)
      on_error("sessId %u >= TOTAL_SESSIONS_PER_CORE %u",
               _sessId, TOTAL_SESSIONS_PER_CORE); */
}
/* ---------------------------------------------------- */

EkaEpmAction *EkaEpm::addAction(ActionType type,
                                epm_actionid_t localIdx,
                                int regionId) {

  if (!epmRegion[regionId])
    on_error("!epmRegion[%u]", regionId);

  actionParamsSanityCheck(type, regionId);
  EkaEpmRegion::sanityCheckActionId(regionId,
                                    (int)localIdx);

  createActionMtx.lock();

  EKA_LOG("Creating Action[%d]: \'%s\' at regionId=%u",
          (int)localIdx, printActionType(type), regionId);

  epm_actionid_t globalIdx =
      EkaEpmRegion::getBaseActionIdx(regionId) + localIdx;

  if (a_[globalIdx])
    on_error("a_[%d] is already set", globalIdx);

  a_[globalIdx] =
      new EkaEpmAction(type, localIdx, regionId);

  if (!a_[globalIdx])
    on_error("!a_[%d]", globalIdx);

  createActionMtx.unlock();

  return a_[globalIdx];
}
/* ---------------------------------------------------- */

void EkaEpm::deleteAction(EpmActionType actionType,
                          epm_actionid_t localIdx,
                          int regionId) {
  createActionMtx.lock();
  epm_actionid_t globalIdx =
      EkaEpmRegion::getBaseActionIdx(regionId) + localIdx;

  if (a_[globalIdx]) {
    EKA_LOG("Deleting Action[%d]: \'%s\' at regionId=%u",
            (int)localIdx, printActionType(actionType),
            regionId);
    delete a_[globalIdx];
    a_[globalIdx] = nullptr;
  } else {
    EKA_WARN(
        "Warning: Cannot delete Action[%d] == nullptr: "
        "\'%s\' at regionId=%u",
        (int)localIdx, printActionType(actionType),
        regionId);
  }

  createActionMtx.unlock();
}

/* ---------------------------------------------------- */
epm_actionid_t
EkaEpm::allocateAction(EpmActionType actionType) {
  allocateActionMtx.lock();
  auto regionId = EkaEpmRegion::Regions::Efc;

  if (nActions_ == EkaEpmRegion::getMaxActions(regionId))
    on_error("Out of free actions: nActions_ = %ju",
             nActions_);

  auto globalIdx = nActions_;

  auto localIdx =
      globalIdx - EkaEpmRegion::getBaseActionIdx(regionId);

  a_[globalIdx] = addAction(actionType, localIdx, regionId);

  nActions_++;

  allocateActionMtx.unlock();

  return globalIdx;
}
