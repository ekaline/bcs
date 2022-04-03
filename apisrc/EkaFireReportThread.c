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

std::pair<int,size_t> processEpmReport(EkaDev* dev,
				       const uint8_t* srcReport,
				       uint len,
				       uint32_t epmReportIndex,
				       uint8_t* reportBuf) {
//    hexDump("SW triggered fire",srcReport,len);
    //--------------------------------------------------------------------------
    while (dev->userReportQ->isEmpty()) {}

    /* EKA_LOG("processFireReport: Report len = %u",len); */

    EkaUserReportElem* userReport = dev->userReportQ->pop();
    uint32_t userReportIndex = userReport->hdr.index;

    if (userReportIndex != epmReportIndex) {
	hexDump("Fire Report with wrong Index",srcReport,len);
	on_error("userReportIndex %u (0x%x) != epmReportIndex %u (0x%x)",
		 userReportIndex,userReportIndex,epmReportIndex,epmReportIndex);
    }
    //--------------------------------------------------------------------------
    uint8_t* b =  reportBuf;
    uint reportIdx = 0;
    //--------------------------------------------------------------------------
    auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
    containerHdr->type = EkaEventType::kEpmReport;
    containerHdr->num_of_reports = 1; // to be overwritten at the end
    b += sizeof(*containerHdr);
    //--------------------------------------------------------------------------
    auto epmReportHdr {reinterpret_cast<EfcReportHdr*>(b)};
    epmReportHdr->type = EfcReportType::kEpmReport;
    epmReportHdr->idx  = ++reportIdx;
    epmReportHdr->size = sizeof(EpmFireReport);
    b += sizeof(*epmReportHdr);
    //--------------------------------------------------------------------------

    auto epmReport {reinterpret_cast<EpmFireReport*>(b)};
    b += sizeof(*epmReport);
    //--------------------------------------------------------------------------
    
    auto hwEpmReport {reinterpret_cast<const hw_epm_report_t*>(srcReport)};

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
    //--------------------------------------------------------------------------

    return {hwEpmReport->strategyId,b-reportBuf};
}

/* ########################################################### */


std::pair<int,size_t> processFireReport(EkaDev* dev,
					const uint8_t* srcReport,
					uint len,
					uint32_t epmReportIndex,
					uint8_t* reportBuf) {
  //--------------------------------------------------------------------------
  while (dev->userReportQ->isEmpty()) {}

  /* EKA_LOG("processFireReport: Report len = %u",len); */

  EkaUserReportElem* userReport = dev->userReportQ->pop();
  uint32_t userReportIndex = userReport->hdr.index;

  if (userReportIndex != epmReportIndex) {
    hexDump("Fire Report with wrong Index",srcReport,len);
    on_error("userReportIndex %u (0x%x) != epmReportIndex %u (0x%x)",
	     userReportIndex,userReportIndex,epmReportIndex,epmReportIndex);
  }
  //--------------------------------------------------------------------------

  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  auto report {reinterpret_cast<const EfcNormalizedFireReport*>(srcReport)};

  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<EkaContainerGlobalHdr*>(b)};
  containerHdr->type = EkaEventType::kFireReport;
  containerHdr->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(*containerHdr);

  //--------------------------------------------------------------------------
  auto controllerStateHdr {reinterpret_cast<EfcReportHdr*>(b)};
  controllerStateHdr->type = EfcReportType::kControllerState;
  controllerStateHdr->idx  = ++reportIdx;

  // 1 byte for uint8_t unarm_reson;
  controllerStateHdr->size = sizeof(EfcControllerState); 
  b += sizeof(*controllerStateHdr);

  auto controllerState { reinterpret_cast<EfcControllerState*>(b) };

  // TBD!!! source->normalized_report.last_unarm_reason;
  controllerState->unarm_reason = 0;
  
  controllerState->fire_reason = report->controllerState.fireReason;
  b += sizeof(*controllerState);

  //--------------------------------------------------------------------------
  auto mdReportHdr {reinterpret_cast<EfcReportHdr*>(b)};

  mdReportHdr->type = EfcReportType::kMdReport;
  mdReportHdr->idx  = ++reportIdx;
  mdReportHdr->size = sizeof(EfcMdReport);
  b += sizeof(*mdReportHdr);

  //  hexDump("processFireReport: triggerOrder",&report->triggerOrder,sizeof(EfcFiredOrder));
  //  printFireOrder(dev,&report->triggerOrder);

  auto mdReport {reinterpret_cast<EfcMdReport*>(b)};
  mdReport->timestamp   = report->triggerOrder.timestamp;
  mdReport->sequence    = report->triggerOrder.sequence;
  mdReport->side        = report->triggerOrder.attr.bitmap.Side;
  mdReport->price       = report->triggerOrder.price;
  mdReport->size        = report->triggerOrder.size;
  mdReport->security_id = report->triggerOrder.securityId;
  mdReport->group_id    = report->triggerOrder.groupId;
  mdReport->core_id     = report->triggerOrder.attr.bitmap.CoreID;

  //  printMdReport(dev,mdReport);

  b += sizeof(*mdReport);

  //--------------------------------------------------------------------------
  auto secCtxHdr {reinterpret_cast<EfcReportHdr*>(b)};
  secCtxHdr->type = EfcReportType::kSecurityCtx;
  secCtxHdr->idx  = ++reportIdx;
  secCtxHdr->size = sizeof(SecCtx);
  b += sizeof(*secCtxHdr);

  auto secCtxReport {reinterpret_cast<SecCtx*>(b)};
  secCtxReport->lowerBytesOfSecId   = report->securityCtx.lowerBytesOfSecId;
  secCtxReport->bidSize             = report->securityCtx.bidSize;
  secCtxReport->askSize             = report->securityCtx.askSize;
  secCtxReport->askMaxPrice         = report->securityCtx.askMaxPrice;
  secCtxReport->bidMinPrice         = report->securityCtx.bidMinPrice;
  secCtxReport->versionKey          = report->securityCtx.versionKey;

  //  printSecCtx  (dev,secCtxReport);

  b += sizeof(*secCtxReport);

  //--------------------------------------------------------------------------
  auto firePktHdr {reinterpret_cast<EfcReportHdr*>(b)};
  auto firePktLen = userReport->hdr.length;
  firePktHdr->type = EfcReportType::kFirePkt;
  firePktHdr->idx  = ++reportIdx;
  firePktHdr->size = firePktLen;
  b += sizeof(EfcReportHdr);

  //  hexDump("processFireReport: userReport->data",&userReport->data,userReport->hdr.length);
  
  memcpy(b,&userReport->data,firePktLen);
  b += firePktLen;

  //--------------------------------------------------------------------------
  containerHdr->num_of_reports = reportIdx;

  int reportLen = b - &reportBuf[0];
  if (reportLen > (int)sizeof(reportBuf)) 
    on_error("reportLen %d > sizeof(reportBuf) %d",
	     reportLen,(int)sizeof(reportBuf));


  /* auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])}; */
  /* if (!efc) on_error("!efc"); */

  /* if (!efc->localCopyEfcRunCtx.onEfcFireReportCb)  */
  /*   on_error("!onFireReportCb"); */
  /* efc->localCopyEfcRunCtx.onEfcFireReportCb(&efc->localCopyEfcCtx, */
  /* 					    reinterpret_cast<EfcFireReport*>(reportBuf),  */
  /* 					    reportLen, */
  /* 					    efc->localCopyEfcRunCtx.cbCtx); */
  return {EFC_STRATEGY,b-reportBuf};
}

/* ########################################################### */

void ekaFireReportThread(EkaDev* dev) {
  EKA_LOG("Launching");
  //  uint32_t fire_counter = 0;

  dev->fireReportThreadActive = true;
  pthread_t thread = pthread_self();
  pthread_setname_np(thread,"EkaFireReportThread");
  dev->fireReportThreadTerminated = false;

  auto epmReportCh {dev->epmReport};

  auto epm {dev->epm};
  if (!epm) on_error("!epm");

  if (!epmReportCh) on_error("!epmReportCh");

  while (dev->fireReportThreadActive) {
    /* ----------------------------------------------- */
    if (! epmReportCh->hasData()) continue;
    auto data = epmReportCh->get();
    auto len  = epmReportCh->getPayloadSize();

//    hexDump("ekaFireReportThread: EPM/Fire report",data,len); fflush(stdout);

    auto dmaReportHdr {reinterpret_cast<const report_dma_report_t*>(data)};
    
    if (dmaReportHdr->length + sizeof(*dmaReportHdr) != len) {
      hexDump("EPM report",data,len); fflush(stdout);
      on_error("DMA length mismatch %u != %u",
	       dmaReportHdr->length,len);
    }

    auto epmReportIndex = dmaReportHdr->feedbackDmaIndex;
    auto payload = data + sizeof(*dmaReportHdr);
    uint8_t reportBuf[4000] = {};
    std::pair<int,size_t> r;
    switch ((EkaUserChannel::DMA_TYPE)dmaReportHdr->type) {
      /* ----------------------------------------------- */
    case EkaUserChannel::DMA_TYPE::EPM:
	r = processEpmReport(dev,payload,len,
			     epmReportIndex,reportBuf);
	break;
      /* ----------------------------------------------- */
    case EkaUserChannel::DMA_TYPE::FIRE:
	r = processFireReport(dev,payload,len,
			      epmReportIndex,reportBuf);
	break;
      /* ----------------------------------------------- */
    default:
      on_error("Unexpected DMA type 0x%x",dmaReportHdr->type);
    }

    int    strategyId = r.first;
    size_t reportLen  = r.second;
    auto reportedStrategy {epm->strategy[strategyId]};
    if (!reportedStrategy)
	on_error("!strategy[%d]",strategyId);
    if (! reportedStrategy->reportCb)
	on_error("reportCb is not defined");
    reportedStrategy->reportCb(reportBuf,reportLen,dev);
    epmReportCh->next();
    
  }
  dev->fireReportThreadTerminated = true;
  EKA_LOG("Terminated");
  return;
}
