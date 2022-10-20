#include <inttypes.h>

#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EkaCore.h"
#include "EkaSnDev.h"
#include "EkaUserChannel.h"
#include "EkaUdpChannel.h"
#include "EkaEpm.h"
#include "EkaEfc.h"

#include "EpmStrategy.h"

#include "eka_macros.h"
#include "EkaEfcDataStructs.h"
#include "EkaUserReportQ.h"


inline size_t pushControllerState(int reportIdx, uint8_t* dst,
				  const EfcNormalizedFireReport* hwReport) {
    auto b = dst;
    auto controllerStateHdr {reinterpret_cast<EfcReportHdr*>(b)};
    controllerStateHdr->type = EfcReportType::kControllerState;
    controllerStateHdr->idx  = reportIdx;

    // 1 byte for uint8_t unarm_reson;
    controllerStateHdr->size = sizeof(EfcControllerState); 
    b += sizeof(*controllerStateHdr);

    auto controllerState { reinterpret_cast<EfcControllerState*>(b) };

    // TBD!!! source->normalized_report.last_unarm_reason;
    controllerState->unarm_reason = 0;
  
    controllerState->fire_reason = hwReport->controllerState.fireReason;
    b += sizeof(*controllerState);
    
    return b - dst;
}

inline size_t pushMdReport(int reportIdx, uint8_t* dst,
			   const EfcNormalizedFireReport* hwReport) {
    auto b = dst;
    auto mdReportHdr {reinterpret_cast<EfcReportHdr*>(b)};

    mdReportHdr->type = EfcReportType::kMdReport;
    mdReportHdr->idx  = reportIdx;
    mdReportHdr->size = sizeof(EfcMdReport);
    b += sizeof(*mdReportHdr);

    auto mdReport {reinterpret_cast<EfcMdReport*>(b)};
    mdReport->timestamp   = hwReport->triggerOrder.timestamp;
    mdReport->sequence    = hwReport->triggerOrder.sequence;
    mdReport->side        = hwReport->triggerOrder.attr.bitmap.Side;
    mdReport->price       = hwReport->triggerOrder.price;
    mdReport->size        = hwReport->triggerOrder.size;
    mdReport->security_id = hwReport->triggerOrder.securityId;
    mdReport->group_id    = hwReport->triggerOrder.groupId;
    mdReport->core_id     = hwReport->triggerOrder.attr.bitmap.CoreID;

    b += sizeof(*mdReport);
    return b - dst;
}

inline size_t pushSecCtx(int reportIdx, uint8_t* dst,
			 const EfcNormalizedFireReport* hwReport) {
    auto b = dst;
    auto secCtxHdr {reinterpret_cast<EfcReportHdr*>(b)};
    secCtxHdr->type = EfcReportType::kSecurityCtx;
    secCtxHdr->idx  = reportIdx;
    secCtxHdr->size = sizeof(SecCtx);
    b += sizeof(*secCtxHdr);

    auto secCtxReport {reinterpret_cast<SecCtx*>(b)};
    secCtxReport->lowerBytesOfSecId   = hwReport->securityCtx.lowerBytesOfSecId;
    secCtxReport->bidSize             = hwReport->securityCtx.bidSize;
    secCtxReport->askSize             = hwReport->securityCtx.askSize;
    secCtxReport->askMaxPrice         = hwReport->securityCtx.askMaxPrice;
    secCtxReport->bidMinPrice         = hwReport->securityCtx.bidMinPrice;
    secCtxReport->versionKey          = hwReport->securityCtx.versionKey;

    b += sizeof(*secCtxReport);
    return b - dst;
}

inline size_t pushFiredPkt(int reportIdx, uint8_t* dst,
			   EkaUserReportQ* q, uint32_t dmaIdx) {

  while (q->isEmpty()) {}
  auto firePkt = q->pop();
  auto firePktIndex = firePkt->hdr.index;
    
  if (firePktIndex != dmaIdx) {
    on_error("firePktIndex %u (0x%x) != dmaIdx %u (0x%x)",
	     firePktIndex,firePktIndex,dmaIdx,dmaIdx);
  }
    
  auto b = dst;
  auto firePktHdr {reinterpret_cast<EfcReportHdr*>(b)};
  firePktHdr->type = EfcReportType::kFirePkt;
  firePktHdr->idx  = reportIdx;
  firePktHdr->size = firePkt->hdr.length;
  b += sizeof(EfcReportHdr);

  memcpy(b,firePkt->data,firePkt->hdr.length);
  b += firePkt->hdr.length;
  //  hexDump("pushFiredPkt",firePkt->data,firePkt->hdr.length);

  return b - dst;
}

inline void clearExceptions(EkaDev* dev) {
  eka_read(dev,ADDR_INTERRUPT_MAIN_RC);
}

inline size_t pushExceptionReport(int reportIdx, uint8_t* dst,
				  EfcExceptionsReport* src) {
  auto b = dst;
  auto exceptionReportHdr {reinterpret_cast<EfcReportHdr*>(b)};
  exceptionReportHdr->type = EfcReportType::kExceptionReport;
  exceptionReportHdr->idx  = reportIdx;
  exceptionReportHdr->size = sizeof(EfcExceptionsReport);
  b += sizeof(*exceptionReportHdr);
  //--------------------------------------------------------------------------
  auto exceptionReport {reinterpret_cast<EfcExceptionsReport*>(b)};
  b += sizeof(*exceptionReport);
  //--------------------------------------------------------------------------
  memcpy(exceptionReport,src,sizeof(*exceptionReport));
  //--------------------------------------------------------------------------
  return b - dst;
}

inline size_t pushEpmReport(int reportIdx, uint8_t* dst,
			   const hw_epm_report_t* hwEpmReport) {
    auto b = dst;
    auto epmReportHdr {reinterpret_cast<EfcReportHdr*>(b)};
    epmReportHdr->type = EfcReportType::kEpmReport;
    epmReportHdr->idx  = reportIdx;
    epmReportHdr->size = sizeof(EpmFireReport);
    b += sizeof(*epmReportHdr);
    //--------------------------------------------------------------------------

    auto epmReport {reinterpret_cast<EpmFireReport*>(b)};
    b += sizeof(*epmReport);
    //--------------------------------------------------------------------------
    epmReport->strategyId      = hwEpmReport->strategyId;
    epmReport->actionId        = hwEpmReport->actionId;
    epmReport->triggerActionId = hwEpmReport->triggerActionId;
    epmReport->triggerToken    = hwEpmReport->token;
    epmReport->action          = (EpmTriggerAction)hwEpmReport->action;
    epmReport->error           = (EkaOpResult)hwEpmReport->error;
    epmReport->preLocalEnable  = hwEpmReport->preLocalEnable;
    epmReport->postLocalEnable = hwEpmReport->postLocalEnable;
    epmReport->preStratEnable  = hwEpmReport->preStratEnable;
    epmReport->postStratEnable = hwEpmReport->postStratEnable;
    epmReport->user            = hwEpmReport->user;
    epmReport->local           = (bool)hwEpmReport->islocal;
    
    return b - dst;
}
/* ########################################################### */
inline size_t pushFastCancelReport(int reportIdx, uint8_t* dst,
			   const hw_epm_fast_cancel_report_t* hwEpmReport) {
    auto b = dst;
    auto epmReportHdr {reinterpret_cast<EfcReportHdr*>(b)};
    epmReportHdr->type = EfcReportType::kFastCancelReport;
    epmReportHdr->idx  = reportIdx;
    epmReportHdr->size = sizeof(EpmFastCancelReport);
    b += sizeof(*epmReportHdr);
    //--------------------------------------------------------------------------

    auto epmReport {reinterpret_cast<EpmFastCancelReport*>(b)};
    b += sizeof(*epmReport);
    //--------------------------------------------------------------------------
    epmReport->numInGroup     = hwEpmReport->num_in_group;
    epmReport->headerSize     = hwEpmReport->header_size;
    epmReport->sequenceNumber = hwEpmReport->sequence_number;
    
    return b - dst;
}
/* ########################################################### */
inline size_t pushNewsReport(int reportIdx, uint8_t* dst,
			     const hw_epm_news_report_t* hwEpmReport) {
    auto b = dst;
    auto epmReportHdr {reinterpret_cast<EfcReportHdr*>(b)};
    epmReportHdr->type = EfcReportType::kNewsReport;
    epmReportHdr->idx  = reportIdx;
    epmReportHdr->size = sizeof(EpmNewsReport);
    b += sizeof(*epmReportHdr);
    //--------------------------------------------------------------------------

    auto epmReport {reinterpret_cast<EpmNewsReport*>(b)};
    b += sizeof(*epmReport);
    //--------------------------------------------------------------------------
    epmReport->strategyIndex  = hwEpmReport->strategy_index;
    epmReport->strategyRegion = hwEpmReport->strategy_region;
    epmReport->token          = hwEpmReport->token;
    
    return b - dst;
}

/* ########################################################### */
/* void getExceptionsReport(EkaDev* dev,EfcExceptionsReport* excpt) { */
/*   excpt->globalExcpt = eka_read(dev,ADDR_INTERRUPT_SHADOW_RO); */
/*   for (int i = 0; i < EFC_MAX_CORES; i++) { */
/*     excpt->coreExcpt[i] = eka_read(dev,EKA_ADDR_INTERRUPT_0_SHADOW_RO + i * 0x1000); */
/*   } */
/* } */
/* ########################################################### */

std::pair<int,size_t> processSwTriggeredReport(EkaDev* dev,
					       const uint8_t*  srcReport,
					       uint            srcReportLen,
					       EkaUserReportQ* q,
					       uint32_t        dmaIdx,
					       uint8_t*        reportBuf) {

  int strategyId2ret = EPM_INVALID_STRATEGY;
  //--------------------------------------------------------------------------
  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
  containerHdr->type = EkaEventType::kEpmEvent;
  containerHdr->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  auto hwEpmReport {reinterpret_cast<const hw_epm_sw_trigger_report_t*>(srcReport)};

  switch (static_cast<HwEpmActionStatus>(hwEpmReport->epm.action)) {
  case HwEpmActionStatus::Sent : 
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm); 
    b += pushFiredPkt (++reportIdx,b,q,dmaIdx);
    strategyId2ret = hwEpmReport->epm.strategyId;
    EKA_LOG("processEpmReport HwEpmActionStatus::Sent, len=%d",srcReportLen);
    break;
  default:
    // Broken EPM send reported by hwEpmReport->action
    EKA_LOG("Processgin HwEpmActionStatus::Garbage, len=%d",srcReportLen);
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm);
  } 
  //--------------------------------------------------------------------------
  containerHdr->num_of_reports = reportIdx;

  return {strategyId2ret,b-reportBuf};
}

/* ########################################################### */

std::pair<int,size_t> processExceptionReport(EkaDev* dev,
					     const uint8_t*  srcReport,
					     uint            srcReportLen,
					     EkaUserReportQ* q,
					     uint32_t        dmaIdx,
					     uint8_t*        reportBuf) {

  int strategyId2ret = EPM_INVALID_STRATEGY;
  //--------------------------------------------------------------------------
  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
  containerHdr->type = EkaEventType::kExceptionEvent;
  containerHdr->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  auto hwEpmReport {reinterpret_cast<const hw_epm_status_report_t*>(srcReport)};

  EfcExceptionsReport exceptReport = {};

  switch (static_cast<HwEpmActionStatus>(hwEpmReport->epm.action)) {
  case HwEpmActionStatus::HWPeriodicStatus :
    //    EKA_LOG("Processgin HwEpmActionStatus::HWPeriodicStatus, len=%d",srcReportLen);
    //copying port exception vectors
    for (int i = 0; i < EFC_MAX_CORES; i++) {
      exceptReport.exceptionStatus.portVector[i] = hwEpmReport->exception_report.core_vector[i];
    }
    //copying global exception vector
    exceptReport.exceptionStatus.globalVector = hwEpmReport->exception_report.global_vector;
    //copying arm status fields
    exceptReport.armStatus.armFlag                = hwEpmReport->arm_report.arm_state;
    exceptReport.armStatus.expectedVersion = hwEpmReport->arm_report.arm_expected_version;
    //    hexDump("------------\nexceptReport",hwEpmReport,sizeof(*hwEpmReport));
    //    EKA_LOG("ARM=%d VER=%d",hwEpmReport->arm_report.arm_state,hwEpmReport->arm_report.arm_expected_version);

    b += pushExceptionReport(++reportIdx,b,&exceptReport);
    break;
  default:
    // Broken EPM 
    EKA_LOG("Processgin HwEpmActionStatus::Garbage, len=%d",srcReportLen);
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm);
  } 
  //--------------------------------------------------------------------------
  containerHdr->num_of_reports = reportIdx;

  if (containerHdr->num_of_reports)
    strategyId2ret = EPM_NO_STRATEGY;

  return {strategyId2ret,b-reportBuf};
}

/* ########################################################### */

std::pair<int,size_t> processFastCancelReport(EkaDev* dev,
				       const uint8_t*  srcReport,
				       uint            srcReportLen,
				       EkaUserReportQ* q,
				       uint32_t        dmaIdx,
				       uint8_t*        reportBuf) {

  int strategyId2ret = EPM_INVALID_STRATEGY;
  //--------------------------------------------------------------------------
  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
  containerHdr->type = EkaEventType::kFastCancelEvent;
  containerHdr->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  auto hwEpmReport {reinterpret_cast<const hw_epm_fast_cancel_report_t*>(srcReport)};

  switch (static_cast<HwEpmActionStatus>(hwEpmReport->epm.action)) {
  case HwEpmActionStatus::Sent : 
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm);
    b += pushFastCancelReport(++reportIdx,b,hwEpmReport);
    b += pushFiredPkt (++reportIdx,b,q,dmaIdx);
    strategyId2ret = hwEpmReport->epm.strategyId;
    EKA_LOG("Processgin HwEpmActionStatus::Sent, len=%d",srcReportLen);
    break;
  default:
    // Broken EPM send reported by hwEpmReport->action
    EKA_LOG("Processgin HwEpmActionStatus::Garbage, len=%d",srcReportLen);
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm);
  } 
  //--------------------------------------------------------------------------
  containerHdr->num_of_reports = reportIdx;

  return {strategyId2ret,b-reportBuf};
}

/* ########################################################### */

std::pair<int,size_t> processNewsReport(EkaDev* dev,
				       const uint8_t*  srcReport,
				       uint            srcReportLen,
				       EkaUserReportQ* q,
				       uint32_t        dmaIdx,
				       uint8_t*        reportBuf) {

  int strategyId2ret = EPM_INVALID_STRATEGY;
  //--------------------------------------------------------------------------
  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
  containerHdr->type = EkaEventType::kNewsEvent;
  containerHdr->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  auto hwEpmReport {reinterpret_cast<const hw_epm_news_report_t*>(srcReport)};

  switch (static_cast<HwEpmActionStatus>(hwEpmReport->epm.action)) {
  case HwEpmActionStatus::Sent : 
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm);
    b += pushNewsReport(++reportIdx,b,hwEpmReport);
    b += pushFiredPkt (++reportIdx,b,q,dmaIdx); 
    strategyId2ret = hwEpmReport->epm.strategyId;
    EKA_LOG("Processgin HwEpmActionStatus::Sent, len=%d",srcReportLen);
    break;
  default:
    // Broken EPM send reported by hwEpmReport->action
    EKA_LOG("Processgin HwEpmActionStatus::Garbage, len=%d",srcReportLen);
    b += pushEpmReport(++reportIdx,b,&hwEpmReport->epm);
  } 
  //--------------------------------------------------------------------------
  containerHdr->num_of_reports = reportIdx;

  return {strategyId2ret,b-reportBuf};
}

/* ########################################################### */


std::pair<int,size_t> processFireReport(EkaDev*         dev,
					const uint8_t*  srcReport,
					uint            srcReportLen,
					EkaUserReportQ* q,
					uint32_t        dmaIdx,
					uint8_t*        reportBuf) {
  //--------------------------------------------------------------------------

  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  auto hwReport {reinterpret_cast<const EfcNormalizedFireReport*>(srcReport)};

  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
  containerHdr->type = EkaEventType::kFireEvent;
  containerHdr->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(*containerHdr);
  //--------------------------------------------------------------------------
  b += pushControllerState(++reportIdx,b,hwReport);

  b += pushMdReport(++reportIdx,b,hwReport);

  b += pushSecCtx(++reportIdx,b,hwReport);

  b += pushFiredPkt (++reportIdx,b,q,dmaIdx);

  //--------------------------------------------------------------------------
  containerHdr->num_of_reports = reportIdx;

  return {EFC_STRATEGY,b-reportBuf};
}

/* ----------------------------------------------- */
static inline void sendDate2Hw(EkaDev* dev) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t); 
  uint64_t current_time_ns = ((uint64_t)(t.tv_sec) * (uint64_t)1000'000'000
			      + (uint64_t)(t.tv_nsec));

  eka_write(dev,0xf0300+0*8,be64toh(current_time_ns)); //data
  eka_write(dev,0xf0300+1*8,0);
  eka_write(dev,0xf0300+2*8,0);
  
  return;
}
/* ----------------------------------------------- */

static inline void sendHb2HW(EkaDev* dev) {
  eka_read(dev,0xf0f08);
}
/* ----------------------------------------------- */

/* ########################################################### */

void ekaFireReportThread(EkaDev* dev) {
  EKA_LOG("Launching");
  dev->fireReportThreadActive = true;
  pthread_t thread = pthread_self();
  pthread_setname_np(thread,"EkaFireReportThread");
  dev->fireReportThreadTerminated = false;
  int64_t updCnt = 0;
  
  auto epmReportCh {dev->epmReport};

  auto epm {dev->epm};
  if (!epm) on_error("!epm");

  if (!epmReportCh) on_error("!epmReportCh");

  const int64_t HwDateUpdatePeriod = 1024 * 1024;
  while (dev->fireReportThreadActive) {
    if ((updCnt++ % HwDateUpdatePeriod)==0) {
      sendDate2Hw(dev);
      //      sendHb2HW(dev);
    }    
    /* ----------------------------------------------- */
    if (! epmReportCh->has_data()) continue;
    auto data = epmReportCh->get();
    auto len  = epmReportCh->getPayloadSize();

    auto dmaReportHdr {reinterpret_cast<const report_dma_report_t*>(data)};
    //    hexDump("------------\ndmaReportHdr",dmaReportHdr,sizeof(*dmaReportHdr));

    if (dmaReportHdr->length + sizeof(*dmaReportHdr) != len) {
      hexDump("EPM report",data,len); fflush(stdout);
      on_error("DMA length mismatch %u != %u",
	       dmaReportHdr->length,len);
    }
    
    auto payload = data + sizeof(*dmaReportHdr);
    //    hexDump("------------\ndmaReportData",payload,dmaReportHdr->length);

    uint8_t reportBuf[4000] = {};
    std::pair<int,size_t> r;
    switch ((EkaUserChannel::DMA_TYPE)dmaReportHdr->type) {
    case EkaUserChannel::DMA_TYPE::SW_TRIGGERED:
      r = processSwTriggeredReport(dev,payload,len,
				   dev->userReportQ,
				   dmaReportHdr->feedbackDmaIndex,
				   reportBuf);
      break;
    case EkaUserChannel::DMA_TYPE::EXCEPTION:
      r = processExceptionReport(dev,payload,len,
				 dev->userReportQ,
				 dmaReportHdr->feedbackDmaIndex,
				 reportBuf);
      break;
    case EkaUserChannel::DMA_TYPE::FAST_CANCEL:
      r = processFastCancelReport(dev,payload,len,
				  dev->userReportQ,
				  dmaReportHdr->feedbackDmaIndex,
				  reportBuf);
      break;
    case EkaUserChannel::DMA_TYPE::NEWS:
      r = processNewsReport(dev,payload,len,
			    dev->userReportQ,
			    dmaReportHdr->feedbackDmaIndex,
			    reportBuf);
      break;
    case EkaUserChannel::DMA_TYPE::FIRE:
      r = processFireReport(dev,payload,len,
			    dev->userReportQ,
			    dmaReportHdr->feedbackDmaIndex,
			    reportBuf);
      break;
    default:
      on_error("Unexpected DMA type 0x%x",dmaReportHdr->type);
    }

    int    strategyId = r.first;
    size_t reportLen  = r.second;
    
    if (reportLen > sizeof(reportBuf)) 
    on_error("reportLen %jd > sizeof(reportBuf) %jd",
	     reportLen,sizeof(reportBuf));

    if (strategyId != EPM_INVALID_STRATEGY) {
      if (strategyId != EPM_NO_STRATEGY) { //valid strategy
	auto reportedStrategy {epm->strategy[strategyId]};
	if (!reportedStrategy) {
	  hexDump("Bad Report",reportBuf,reportLen);
	  on_error("!strategy[%d]",strategyId);
	}
	if (!reportedStrategy->reportCb)
	  on_error("reportCb is not defined");
	reportedStrategy->reportCb(reportBuf,reportLen,
				   reportedStrategy->cbCtx);
      }
      else { //no strategy, as exception
	if (!dev->pEfcRunCtx || !dev->pEfcRunCtx->onEfcFireReportCb)
	  EKA_WARN("dev->pEfcRunCtx->reportCb is not defined");
	else
	  dev->pEfcRunCtx->onEfcFireReportCb(reportBuf,reportLen,
					     dev->pEfcRunCtx->cbCtx);
      }
    }
    epmReportCh->next();

  }
  dev->fireReportThreadTerminated = true;
  EKA_LOG("Terminated");
  return;
}
