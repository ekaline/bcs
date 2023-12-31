#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "Efc.h"
#include "Efh.h"
#include "EkaEpm.h"

#include "EkaCore.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEpmAction.h"
#include "EkaEpmRegion.h"
#include "EkaHwCaps.h"
#include "EkaSnDev.h"

#include "EkaEfcDataStructs.h"

#include "EkaEurStrategy.h"
#include "EkaUserChannel.h"

#include "EkaEfcDataStructs.h"
#include "EkaUserReportQ.h"

using namespace EkaEobi;

/* ----------------------------------------------- */

static inline void sendExcptRequestTrigger(EkaDev *dev) {
  eka_write(dev, 0xf0610, 0);
}

/* ################################################## */

void EkaEurStrategy::fireReportThreadLoop(
    const EkaBcRunCtx *pEkaBcRunCtx) {

  if (!pEkaBcRunCtx)
    on_error("!pEkaBcRunCtx");
  if (!pEkaBcRunCtx->onReportCb)
    on_error("!pEkaBcRunCtx->onReportCb");
  EkaBcRunCtx localCopyEkaBcRunCtx = {};

  memcpy(&localCopyEkaBcRunCtx, pEkaBcRunCtx,
         sizeof(localCopyEkaBcRunCtx));

  const char *threadName = "FireReport";
  EKA_LOG("Launching %s", threadName);
  pthread_setname_np(pthread_self(), threadName);

  if (dev_->affinityConf.fireReportThreadCpuId >= 0) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(dev_->affinityConf.fireReportThreadCpuId,
            &cpuset);
    int rc = pthread_setaffinity_np(
        pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc < 0)
      on_error("Failed to set affinity");
    EKA_LOG("Affinity is set to CPU %d",
            dev_->affinityConf.fireReportThreadCpuId);
  }

  dev_->fireReportThreadActive = true;
  dev_->fireReportThreadTerminated = false;

  auto epmReportCh{dev_->epmReport};
  if (!epmReportCh)
    on_error("!epmReportCh");

  auto lastExcptRequestTriggerTime =
      std::chrono::high_resolution_clock::now();
  auto lastDateUpdateTime =
      std::chrono::high_resolution_clock::now();

  while (dev_->fireReportThreadActive) {
    auto now = std::chrono::high_resolution_clock::now();
    /* ----------------------------------------------- */
    if (std::chrono::duration_cast<
            std::chrono::milliseconds>(
            now - lastExcptRequestTriggerTime)
            .count() >
        EPM_EXCPT_REQUEST_TRIGGER_TIMEOUT_MILLISEC) {
      sendExcptRequestTrigger(dev_);
      lastExcptRequestTriggerTime = now;
    }
    /* ----------------------------------------------- */
    if (EFC_DATE_UPDATE_PERIOD_MILLISEC &&
        std::chrono::duration_cast<
            std::chrono::milliseconds>(now -
                                       lastDateUpdateTime)
                .count() >
            EFC_DATE_UPDATE_PERIOD_MILLISEC) {
      sendDate2Hw();
      lastDateUpdateTime = now;
    }
    /* ----------------------------------------------- */
    if (!epmReportCh->has_data()) {
      std::this_thread::yield();
      continue;
    }
    auto data = epmReportCh->get();
    auto len = epmReportCh->getPayloadSize();

    auto dmaReportHdr{
        reinterpret_cast<const report_dma_report_t *>(
            data)};
    //    hexDump("------------\ndmaReportHdr",dmaReportHdr,sizeof(*dmaReportHdr));

    if (dmaReportHdr->length + sizeof(*dmaReportHdr) !=
        len) {
      hexDump("EPM report", data, len);
      fflush(stdout);
      on_error("DMA length mismatch %u != %u",
               dmaReportHdr->length, len);
    }

    auto payload = data + sizeof(*dmaReportHdr);
    //    hexDump("------------\ndmaReportData",payload,dmaReportHdr->length);

    uint8_t reportBuf[4000] = {};
    bool printFireReport = false;
    std::pair<int, size_t> r;
    switch ((EkaUserChannel::DMA_TYPE)dmaReportHdr->type) {
    case EkaUserChannel::DMA_TYPE::SW_TRIGGERED:
      r = processSwTriggeredReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      //      EKA_LOG("XXXSWTRIGGERD");
      break;
    case EkaUserChannel::DMA_TYPE::EXCEPTION:
      r = processExceptionsReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      //      EKA_LOG("XXXEXCEPTION");
      break;
    case EkaUserChannel::DMA_TYPE::FAST_CANCEL:
      r = processFastCancelReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      break;
    case EkaUserChannel::DMA_TYPE::FIRE:
      r = processFireReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      //      EKA_LOG("XXXFIRE");
      break;
    default:
      on_error("Unexpected DMA type 0x%x",
               dmaReportHdr->type);
    }

    int strategyId = r.first;
    size_t reportLen = r.second;

    if (reportLen > sizeof(reportBuf))
      on_error("reportLen %jd > sizeof(reportBuf) %jd",
               reportLen, sizeof(reportBuf));

    if (!localCopyEkaBcRunCtx.onReportCb)
      on_error("!localCopyEkaBcRunCtx.onReportCb");

    if (printFireReport) {
      char fireReportStr[16 * 1024] = {};
      hexDump2str("Fire Report", reportBuf, reportLen,
                  fireReportStr, sizeof(fireReportStr));
      EKA_LOG("reportCb: %s", fireReportStr);
    }
    localCopyEkaBcRunCtx.onReportCb(
        reportBuf, reportLen, localCopyEkaBcRunCtx.cbCtx);
    epmReportCh->next();
  }
  dev_->fireReportThreadTerminated = true;
  EKA_LOG("Terminated");
  return;
}


/* ################################################## */
inline size_t
pushExceptionsReport(int reportIdx, uint8_t *dst,
                     const EkaBcExceptionsReport *src) {
  auto b = dst;
  auto reportHdr = reinterpret_cast<EkaBcReportHdr *>(b);
  reportHdr->type = EkaBcReportType::ExceptionsReport;
  reportHdr->idx = reportIdx;
  reportHdr->size = sizeof(EkaBcExceptionsReport);
  b += sizeof(*reportHdr);
  //--------------------------------------------------------------------------
  auto ExceptionsReport =
      reinterpret_cast<EkaBcExceptionsReport *>(b);
  b += sizeof(*ExceptionsReport);
  //--------------------------------------------------------------------------
  memcpy(ExceptionsReport, src, sizeof(*ExceptionsReport));
  //--------------------------------------------------------------------------
  return b - dst;
}
/* ################################################## */

std::pair<int, size_t>
EkaEurStrategy::processExceptionsReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  //--------------------------------------------------------------------------
  uint8_t *b = reportBuf; // dst
  uint reportIdx = 0;
  auto hwReport{
      reinterpret_cast<const EkaBcExceptionsReport *>(
          srcReport)};

  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};
  containerHdr->eventType = EkaBcEventType::ExceptionEvent;
  containerHdr->nReports =
      0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  b += pushExceptionsReport(++reportIdx, b, hwReport);

  //--------------------------------------------------------------------------
  containerHdr->nReports = reportIdx;

  return {EFC_STRATEGY, b - reportBuf};
}

std::pair<int, size_t>
EkaEurStrategy::processFastCancelReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  on_error("Not expected");
}
/* ################################################## */

static inline size_t
pushEurFireReport(int reportIdx, uint8_t *dst,
                  const EkaBcFireReport *src) {

  auto b = dst;
  auto reportHdr = reinterpret_cast<EkaBcReportHdr *>(b);
  reportHdr->type = EkaBcReportType::EurFireReport;
  reportHdr->idx = reportIdx;
  reportHdr->size = sizeof(EkaBcFireReport);
  b += sizeof(*reportHdr);

  memcpy(b, src, sizeof(EkaBcFireReport));

  b += sizeof(EkaBcFireReport);
  return b - dst;
}

/* ################################################## */

static inline size_t
pushEurEpmReport(int reportIdx, uint8_t *dst,
                  const EkaBcSwReport *src) {

  auto b = dst;
  auto reportHdr = reinterpret_cast<EkaBcReportHdr *>(b);
  reportHdr->type = EkaBcReportType::EurSWFireReport;
  reportHdr->idx = reportIdx;
  reportHdr->size = sizeof(EkaBcSwReport);
  b += sizeof(*reportHdr);

  memcpy(b, src, sizeof(EkaBcSwReport));

  b += sizeof(EkaBcSwReport);
  return b - dst;
}

/* ################################################## */
static inline size_t
pushFiredPkt(volatile bool *fireReportThreadActive,
             int reportIdx, uint8_t *dst, EkaUserReportQ *q,
             uint32_t dmaIdx) {

  while (*fireReportThreadActive && q->isEmpty()) {
  }
  auto firePkt = q->pop();
  auto firePktIndex = firePkt->hdr.index;

  if (firePktIndex != dmaIdx) {
    on_error("firePktIndex %u (0x%x) != dmaIdx %u (0x%x)",
             firePktIndex, firePktIndex, dmaIdx, dmaIdx);
  }

  auto b = dst;
  auto firePktHdr{reinterpret_cast<EfcReportHdr *>(b)};
  firePktHdr->type = EfcReportType::kFirePkt;
  firePktHdr->idx = reportIdx;
  firePktHdr->size = firePkt->hdr.length;
  b += sizeof(EfcReportHdr);

  memcpy(b, firePkt->data, firePkt->hdr.length);
  b += firePkt->hdr.length;
  //  hexDump("pushFiredPkt",firePkt->data,firePkt->hdr.length);

  return b - dst;
}

/* ################################################## */

std::pair<int, size_t> EkaEurStrategy::processFireReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  //--------------------------------------------------------------------------
  uint8_t *b = reportBuf; // dst
  uint reportIdx = 0;
  auto hwReport{
      reinterpret_cast<const EkaBcFireReport *>(srcReport)};

  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};
  containerHdr->eventType = EkaBcEventType::FireEvent;
  containerHdr->nReports =
      0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  b += pushEurFireReport(++reportIdx, b, hwReport);

  b += pushFiredPkt(&dev_->fireReportThreadActive,
                    ++reportIdx, b, q, dmaIdx);

  //--------------------------------------------------------------------------
  containerHdr->nReports = reportIdx;

  return {EFC_STRATEGY, b - reportBuf};
}
/* ################################################## */

/* ################################################## */

std::pair<int, size_t>
EkaEurStrategy::processSwTriggeredReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  
  //--------------------------------------------------------------------------
  uint8_t *b = reportBuf;
  uint reportIdx = 0;
  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};
  containerHdr->eventType = EkaBcEventType::EpmEvent;
  containerHdr->nReports =
      0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  auto hwEpmReport{
      reinterpret_cast<const EkaBcSwReport *>(
          srcReport)};

  switch (static_cast<HwEpmActionStatus>(
      hwEpmReport->fireStatus)) {
  case HwEpmActionStatus::Sent:
    b += pushEurEpmReport(++reportIdx, b, hwEpmReport);
    b += pushFiredPkt(&dev_->fireReportThreadActive,
                    ++reportIdx, b, q, dmaIdx);
    //    EKA_LOG("processEpmReport HwEpmActionStatus::Sent,  len=%d",srcReportLen);
    break;
  default:
    // Broken EPM send reported by hwEpmReport->action
    EKA_LOG("Processgin HwEpmActionStatus::Garbage, len=%d",
            srcReportLen);
    b += pushEurEpmReport(++reportIdx, b, hwEpmReport);
  }
  //--------------------------------------------------------------------------
  containerHdr->nReports = reportIdx;

  return {EFC_STRATEGY, b - reportBuf};
  
}

