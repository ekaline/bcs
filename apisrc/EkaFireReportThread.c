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

int processEpmReport(EkaDev* dev, const uint8_t* payload,uint len) {
  hw_epm_report_t* hwEpmReport = (hw_epm_report_t*) (payload + sizeof(report_dma_report_t));

  epm_strategyid_t strategyId = hwEpmReport->strategyId;
  epm_actionid_t   actionId   = hwEpmReport->actionId;

  EpmTrigger trigger = {
    .token    = hwEpmReport->token,
    .strategy = hwEpmReport->strategyId,
    .action   = hwEpmReport->triggerActionId
  };

  const EpmFireReport epmReport = {
    .trigger         = &trigger,
    .strategyId      = strategyId,
    .actionId        = actionId,
    .action          = (EpmTriggerAction)hwEpmReport->action,
    .error           = (EkaOpResult)hwEpmReport->error,
    .preLocalEnable  = hwEpmReport->preLocalEnable,
    .postLocalEnable = hwEpmReport->postLocalEnable,
    .preStratEnable  = hwEpmReport->preStratEnable,
    .postStratEnable = hwEpmReport->postStratEnable,
    .user            = hwEpmReport->user,
    .local           = hwEpmReport->islocal
  };

  if (dev->epm->strategy[strategyId] == NULL) on_error("dev->epm->strategy[%d] = NULL",strategyId);
  dev->epm->strategy[strategyId]->reportCb(&epmReport,1,dev->epm->strategy[strategyId]->cbCtx);

  return 0;
}
/* ########################################################### */

int processFireReport(EkaDev* dev, const uint8_t* srcReport,uint len) {
  uint8_t reportBuf[4000] ={};
  uint8_t* b =  reportBuf;
  uint reportIdx = 0;
  auto report { reinterpret_cast<const EfcNormalizedFireReport*>(srcReport) };

  //--------------------------------------------------------------------------
  ((EkaContainerGlobalHdr*)b)->type = EkaEventType:: kFireReport;
  ((EkaContainerGlobalHdr*)b)->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(EkaContainerGlobalHdr);

  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kControllerState;
  ((EfcReportHdr*)b)->idx  = ++reportIdx;
  ((EfcReportHdr*)b)->size = sizeof(EfcControllerState); // 1 byte for uint8_t unarm_reson;
  b += sizeof(EfcReportHdr);

  auto controllerState { reinterpret_cast<EfcControllerState*>(b) };
  controllerState->unarm_reason = 0; // TBD!!! source->normalized_report.last_unarm_reason;
  b += sizeof(EfcControllerState);

  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kMdReport;
  ((EfcReportHdr*)b)->idx  = ++reportIdx;
  ((EfcReportHdr*)b)->size = sizeof(EfcMdReport);
  b += sizeof(EfcReportHdr);

  auto mdReport { reinterpret_cast<EfcMdReport*>(b) };
  mdReport->timestamp = report->triggerOrder.timestamp;
  mdReport->sequence  = report->triggerOrder.sequence;
  mdReport->side      = report->triggerOrder.attr.bitmap.Side;
  mdReport->price     = report->triggerOrder.price;
  mdReport->size      = report->triggerOrder.size;
  mdReport->group_id  = report->triggerOrder.groupId;
  mdReport->core_id   = report->triggerOrder.attr.bitmap.CoreID;
  b += sizeof(EfcMdReport);

  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kSecurityCtx;
  ((EfcReportHdr*)b)->idx  = ++reportIdx;
  ((EfcReportHdr*)b)->size = sizeof(EfcSecurityCtx);
  b += sizeof(EfcReportHdr);

  auto secCtxReport { reinterpret_cast<EfcSecurityCtx*>(b) };
  secCtxReport->lower_bytes_of_sec_id = report->securityCtx.lowerBytesOfSecId;
  secCtxReport->ver_num               = report->securityCtx.verNum;
  secCtxReport->size                  = report->securityCtx.size;
  secCtxReport->ask_max_price         = report->securityCtx.askMaxPrice;
  secCtxReport->bid_min_price         = report->securityCtx.bidMinPrice;
  b += sizeof(EfcSecurityCtx);


  //--------------------------------------------------------------------------
  ((EkaContainerGlobalHdr*)b)->num_of_reports = reportIdx;

  int resLen = b - &reportBuf[0];
  if (resLen > (int)sizeof(reportBuf)) 
    on_error("resLen %d > sizeof(reportBuf) %d",resLen,(int)sizeof(reportBuf));

  if (dev->efc->localCopyEfcRunCtx.onEfcFireReportCb == NULL) on_error("onFireReportCb == NULL");
  dev->efc->localCopyEfcRunCtx.onEfcFireReportCb(&dev->efc->localCopyEfcCtx,
						 reinterpret_cast< EfcFireReport* >(reportBuf), 
						 resLen);
  return 0;
}

/* ########################################################### */

void ekaFireReportThread(EkaDev* dev) {
  EKA_LOG("Launching");
  //  uint32_t fire_counter = 0;

  dev->fireReportThreadActive = true;
  pthread_t thread = pthread_self();
  pthread_setname_np(thread,"EkaFireReportThread");
  dev->fireReportThreadTerminated = false;

  while (dev->fireReportThreadActive) {
    /* ----------------------------------------------- */
    if (! dev->epmReport->hasData()) continue;
    const uint8_t* data = dev->epmReport->get();
    uint len = dev->epmReport->getPayloadSize();

    hexDump("EPM/Fire report",data,len); fflush(stdout);

    if (((report_dma_report_t*)data)->length + sizeof(report_dma_report_t) != len) {
      hexDump("EPM report",data,len); fflush(stdout);
      on_error("DMA length mismatch %u != %u",be16toh(((report_dma_report_t*)data)->length),len);
    }
    const uint8_t* payload = data + sizeof(report_dma_report_t);

    switch ((EkaUserChannel::DMA_TYPE)((report_dma_report_t*)data)->type) {
      /* ----------------------------------------------- */
    case EkaUserChannel::DMA_TYPE::EPM:
      processEpmReport(dev,payload,len);
      break;
      /* ----------------------------------------------- */
    case EkaUserChannel::DMA_TYPE::FIRE:
      processFireReport(dev,payload,len);
      break;
      /* ----------------------------------------------- */
    default:
      on_error("Unexpected DMA type 0x%x",((report_dma_report_t*)data)->type);
    }
    dev->epmReport->next();
    
  }
  dev->fireReportThreadTerminated = true;
  EKA_LOG("Terminated");
  return;
}
