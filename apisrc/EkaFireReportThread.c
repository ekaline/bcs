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

  while (dev->fireReportThreadActive) {
    /* ----------------------------------------------- */
    if (dev->snDev->fireReport->hasData()) {
      const uint8_t* payload = dev->snDev->fireReport->get();
      uint len = dev->snDev->fireReport->getPayloadSize();

      switch ((EkaUserChannel::DMA_TYPE)((dma_report_t*)payload)->type) {
      case EkaUserChannel::DMA_TYPE::EPM: {
	if (be16toh(((dma_report_t*)payload)->length) + sizeof(dma_report_t) != len)
	  on_error("DMA length mismatch %u != %u",be16toh(((dma_report_t*)payload)->length),len);
	hexDump("EPM report",(void*)payload,len);
	hw_epm_report_t* hwEpmReport = (hw_epm_report_t*) (payload + sizeof(dma_report_t));
	
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
	  .action          = EpmTriggerAction::Sent,
	  .error           = EKA_OPRESULT__OK,
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
	on_error("Unexpected DMA type 0x%x",((dma_report_t*)payload)->type);
      }
      dev->snDev->fireReport->next();
    }
  }
  EKA_LOG("Terminated");
  return;
}
