#include <inttypes.h>

#include "EkaDev.h"
#include "ekaNW.h"
#include "EkaTcpSess.h"
#include "EkaCore.h"
#include "EkaSnDev.h"
#include "EkaUserChannel.h"
#include "EkaUdpChannel.h"
#include "EkaEpm.h"

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
      case EkaUserChannel::DMA_TYPE::EPM:
	if (be16toh(((dma_report_t*)payload)->length) + sizeof(dma_report_t) != len)
	  on_error("DMA length mismatch %u != %u",be16toh(((dma_report_t*)payload)->length),len);


	hexDump("EPM report",(void*)payload,len);
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
