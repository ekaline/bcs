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
#include "EkaTcpSess.h"
#include "EkaUdpSess.h"

#include "EkaEfcDataStructs.h"

#include "EkaEurStrategy.h"
#include "EkaHwHashTableLine.h"

#include "EkaUdpChannel.h"

#include "EkaEobiParser.h"

#include "EkaBcEurProd.h"
#include "EkaEobiTypes.h"
#include "EpmEti8PktTemplate.h"

using namespace EkaEobi;

/* ----------------------------------------------- */

static inline void sendExcptRequestTrigger(EkaDev *dev) {
  eka_write(dev, 0xf0610, 0);
}

/* ################################################## */

void EkaEurStrategy::fireReportThreadLoop(
    const EkaBcRunCtx *pEkaBcRunCtx) {
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
      break;
    case EkaUserChannel::DMA_TYPE::EXCEPTION:
      r = processExceptionReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      break;
    case EkaUserChannel::DMA_TYPE::FAST_CANCEL:
      r = processFastCancelReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      printFireReport = true;
      break;
    case EkaUserChannel::DMA_TYPE::FIRE:
      r = processFireReport(
          dev_, payload, len, dev_->efc->userReportQ,
          dmaReportHdr->feedbackDmaIndex, reportBuf);
      printFireReport = true;
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

    if (!pEkaBcRunCtx || !pEkaBcRunCtx->onReportCb)
      on_error("pEkaBcRunCtx || pEfcRunCtx->onReportCb "
               "not defined");

    if (printFireReport) {
      char fireReportStr[16 * 1024] = {};
      hexDump2str("Fire Report", reportBuf, reportLen,
                  fireReportStr, sizeof(fireReportStr));
      EKA_LOG("reportCb: %s", fireReportStr);
    }
    pEkaBcRunCtx->onReportCb(reportBuf, reportLen,
                             pEkaBcRunCtx->cbCtx);
    epmReportCh->next();
  }
  dev_->fireReportThreadTerminated = true;
  EKA_LOG("Terminated");
  return;
}

/* ################################################## */

std::pair<int, size_t>
EkaEurStrategy::processSwTriggeredReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  on_error("Not expected");
}
/* ################################################## */
inline size_t
pushExceptionReport(int reportIdx, uint8_t *dst,
                    EkaBcExceptionsReport *src) {
  auto b = dst;
  auto exceptionReportHdr =
      reinterpret_cast<EkaBcReportHdr *>(b);
  exceptionReportHdr->type =
      EkaBcReportType::ExceptionReport;
  exceptionReportHdr->idx = reportIdx;
  exceptionReportHdr->size = sizeof(EkaBcExceptionsReport);
  b += sizeof(*exceptionReportHdr);
  //--------------------------------------------------------------------------
  auto exceptionReport{
      reinterpret_cast<EkaBcExceptionsReport *>(b)};
  b += sizeof(*exceptionReport);
  //--------------------------------------------------------------------------
  memcpy(exceptionReport, src, sizeof(*exceptionReport));
  //--------------------------------------------------------------------------
  return b - dst;
}
/* ################################################## */

std::pair<int, size_t>
EkaEurStrategy::processExceptionReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
#if 0
  int strategyId2ret = EPM_INVALID_STRATEGY;
  //--------------------------------------------------------------------------
  uint8_t *b = reportBuf;
  uint reportIdx = 0;
  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};
  containerHdr->eventType = EkaBcEventType::ExceptionEvent;
  containerHdr->nReports =
      0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  auto hwEpmReport{
      reinterpret_cast<const hw_epm_status_report_t *>(
          srcReport)};

  EkaBcExceptionsReport exceptReport = {};

  switch (static_cast<HwEpmActionStatus>(
      hwEpmReport->epm.action)) {
  case HwEpmActionStatus::HWPeriodicStatus:
    //    EKA_LOG("Processgin
    //    HwEpmActionStatus::HWPeriodicStatus,
    //    len=%d",srcReportLen);
    // copying port exception vectors
    for (int i = 0; i < EFC_MAX_CORES; i++) {
      exceptReport.exceptionStatus.portVector[i] =
          hwEpmReport->exception_report.core_vector[i];
    }
    // copying global exception vector
    exceptReport.exceptionStatus.globalVector =
        hwEpmReport->exception_report.global_vector;
    // copying arm status fields
    exceptReport.p4armStatus.armFlag =
        hwEpmReport->p4_arm_report.arm_state;
    exceptReport.p4armStatus.expectedVersion =
        hwEpmReport->p4_arm_report.arm_expected_version;
    exceptReport.nWarmStatus.armFlag =
        hwEpmReport->nw_arm_report.arm_state;
    exceptReport.nWarmStatus.expectedVersion =
        hwEpmReport->nw_arm_report.arm_expected_version;
    //    hexDump("------------\nexceptReport",hwEpmReport,sizeof(*hwEpmReport));
    /* EKA_LOG("P4 ARM=%d, VER=%d", */
    /* 	    hwEpmReport->p4_arm_report.arm_state,hwEpmReport->p4_arm_report.arm_expected_version);
     */
    /* EKA_LOG("NW ARM=%d, VER=%d", */
    /* 	    hwEpmReport->nw_arm_report.arm_state,hwEpmReport->nw_arm_report.arm_expected_version);
     */

    b += pushExceptionReport(++reportIdx, b, &exceptReport);
    break;
  default:
    // Broken EPM
    EKA_LOG("Processgin HwEpmActionStatus::Garbage, len=%d",
            srcReportLen);
    b += pushEpmReport(++reportIdx, b, &hwEpmReport->epm);
  }
  //--------------------------------------------------------------------------
  containerHdr->nReports = reportIdx;

  if (containerHdr->nReports)
    strategyId2ret = EPM_NO_STRATEGY;

  return {strategyId2ret, b - reportBuf};
#endif
  return {0, 0};
}

std::pair<int, size_t>
EkaEurStrategy::processFastCancelReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  on_error("Not expected");
}
/* ################################################## */
std::pair<int, size_t> EkaEurStrategy::processFireReport(
    EkaDev *dev, const uint8_t *srcReport,
    uint srcReportLen, EkaUserReportQ *q, uint32_t dmaIdx,
    uint8_t *reportBuf) {
  //--------------------------------------------------------------------------
#if 0
  uint8_t *b = reportBuf;
  uint reportIdx = 0;
  auto hwReport{
      reinterpret_cast<const EkaBcFireReport *>(
          srcReport)};

  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<EkaBcContainerGlobalHdr *>(b)};
  containerHdr->eventType = EkaBcEventType::EurFireEvent;
  containerHdr->nReports =
      0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  b += pushEpmReport(++reportIdx, b, &hwReport->epm);

  b += pushFiredPkt(++reportIdx, b, q, dmaIdx);

  //--------------------------------------------------------------------------
  containerHdr->nReports = reportIdx;

  return {EFC_STRATEGY, b - reportBuf};
#endif
  return {0, 0};
}
