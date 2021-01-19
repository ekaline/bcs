#include <inttypes.h>

#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EkaCore.h"
#include "EkaSnDev.h"
#include "EkaUserChannel.h"
#include "EkaUdpChannel.h"
#include "EkaEpm.h"
#include "EpmStrategy.h"

#include "eka_macros.h"

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

int processFireReport(EkaDev* dev, const uint8_t* payload,uint len) {
  uint8_t buf[1200] = {};

  uint8_t* b =  buf;
  uint reportIdx = 0;

  ((EkaContainerGlobalHdr*)b)->type = EkaEventType:: kFireReport;
  ((EkaContainerGlobalHdr*)buf)->num_of_reports = 0; // to be overwritten at the end
  b += sizeof(EkaContainerGlobalHdr);
  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kControllerState;
  ((EfcReportHdr*)b)->idx  = ++reportIdx;
  ((EfcReportHdr*)b)->size = sizeof(EfcControllerState); // 1 byte for uint8_t unarm_reson;
  b += sizeof(EfcReportHdr);

  ((EfcControllerState*)b)->unarm_reason = 0; // TBD!!! source->normalized_report.last_unarm_reason;
  b += sizeof(EfcControllerState);
  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kMdReport;
  ((EfcReportHdr*)b)->idx  = ++reportIdx;
  ((EfcReportHdr*)b)->size = sizeof(EfcMdReport);
  b += sizeof(EfcReportHdr);

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
