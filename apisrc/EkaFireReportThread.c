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

void ekaFireReportThread(EkaDev* dev) {
  EKA_LOG("Launching");
  //  uint32_t fire_counter = 0;

  dev->fireReportThreadActive = true;
  pthread_t thread = pthread_self();
  pthread_setname_np(thread,"EkaFireReportThread");
  dev->fireReportThreadTerminated = false;

  while (dev->fireReportThreadActive) {
    /* ----------------------------------------------- */
    if (dev->epmReport->hasData()) {
      const uint8_t* payload = dev->epmReport->get();
      uint len = dev->epmReport->getPayloadSize();

      switch ((EkaUserChannel::DMA_TYPE)((report_dma_report_t*)payload)->type) {
      case EkaUserChannel::DMA_TYPE::EPM: {
	//	if (be16toh(((report_dma_report_t*)payload)->length) + sizeof(report_dma_report_t) != len) {
	if (((report_dma_report_t*)payload)->length + sizeof(report_dma_report_t) != len) {
	  hexDump("EPM report",payload,len); fflush(stdout);
	  on_error("DMA length mismatch %u != %u",be16toh(((report_dma_report_t*)payload)->length),len);
	}

	hw_epm_report_t* hwEpmReport = (hw_epm_report_t*) (payload + sizeof(report_dma_report_t));
	
	/* hexDump("EPM report",(void*)payload,len); */

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
      }
	break;
      default:
	on_error("Unexpected DMA type 0x%x",((report_dma_report_t*)payload)->type);
      }
      dev->epmReport->next();
    }
  }
  dev->fireReportThreadTerminated = true;
  EKA_LOG("Terminated");
  return;
}
