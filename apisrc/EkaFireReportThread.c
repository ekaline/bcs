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
    .local           = (bool)hwEpmReport->islocal
  };

  if (dev->epm->strategy[strategyId] == NULL) on_error("dev->epm->strategy[%d] = NULL",strategyId);
  dev->epm->strategy[strategyId]->reportCb(&epmReport,1,dev->epm->strategy[strategyId]->cbCtx);

  return 0;
}
/* ########################################################### */
/* int printMdReport(EkaDev* dev, const EfcMdReport* msg) { */
/*   //  hexDump("printMdReport",msg,sizeof(EfcMdReport)); */

/*   EKA_LOG("MdReport:"); */
/*   EKA_LOG("\ttimestamp=0x%jx ", msg->timestamp); */
/*   EKA_LOG("\tsequence=0x%jx ",  msg->sequence); */
/*   EKA_LOG("\tside=%c ",       msg->side == 1 ? 'b' : 'a'); */
/*   EKA_LOG("\tprice = %ju ",   msg->price); */
/*   EKA_LOG("\tsize=%ju ",      msg->size); */
/*   EKA_LOG("\tgroup_id=%u ",   msg->group_id); */
/*   EKA_LOG("\tcore_id=%u ",    msg->core_id); */

/*   return 0; */
/* } */
int printMdReport(EkaDev* dev, const EfcMdReport* msg) {
  //  hexDump("printMdReport",msg,sizeof(EfcMdReport));

  //  EKA_LOG("MdReport: timestamp=0x%jx sequence=0x%jx secid=0x%jx side=%c price = %ju size=%ju group_id=%u core_id=%u", 
  //  EKA_LOG("MdReport: timestamp=%ju sequence=%ju secid=%ju side=%c price=%ju size=%ju group_id=%u core_id=%u", 
  //	  msg->timestamp,
  //	  msg->sequence,
  //	  msg->security_id,
  //	  msg->side == 1 ? 'b' : 'a',
  //	  msg->price,
  //	  msg->size,
  //	  msg->group_id,
  //	  msg->core_id
  //	  );

  //NOM
  /* printf("MdReport: GR%d,SN:%ju,SID:%16ju,%c,P:%8ju,S:%8ju\n", */
  /* 	 msg->group_id, */
  /* 	 msg->sequence, */
  /* 	 msg->security_id, */
  /* 	 msg->side == 1 ? 'B' : 'S', */
  /* 	 msg->price, */
  /* 	 msg->size); */
  //PITCH
  printf("MdReport: GR%d,SN:%ju,SID:%c%c%c%c%c%c,%c,P:%8ju,S:%8ju\n",
	 msg->group_id,
	 msg->sequence,

	 (msg->security_id >> 5*8) & 0xFF,
	 (msg->security_id >> 4*8) & 0xFF,
	 (msg->security_id >> 3*8) & 0xFF,
	 (msg->security_id >> 2*8) & 0xFF,
	 (msg->security_id >> 1*8) & 0xFF,
	 (msg->security_id >> 0*8) & 0xFF,

	 msg->side == 1 ? 'B' : 'S',
	 msg->price,
	 msg->size);
  
  return 0;
}
/* ########################################################### */
int printSecCtx(EkaDev* dev, const EfcSecurityCtx* msg) {
  EKA_LOG("SecurityCtx:");
  EKA_LOG("\tlowerBytesOfSecId = 0x%x ",msg->lower_bytes_of_sec_id);
  EKA_LOG("\tverNum = %u",              msg->ver_num);
  EKA_LOG("\tsize = %u",                msg->size);
  EKA_LOG("\taskMaxPrice = %u",         msg->ask_max_price);
  EKA_LOG("\tbidMinPrice = %u",         msg->bid_min_price);

  return 0;
}
/* ########################################################### */
int printFireOrder(EkaDev* dev,const EfcFiredOrder* msg) {
  EKA_LOG("attr = %02x",      (int)msg->attr.bits);
  EKA_LOG("price = %u",       msg->price);
  EKA_LOG("size = %u",        msg->size);
  EKA_LOG("counter = %u",     msg->counter);
  EKA_LOG("securityId = %jx",  msg->securityId);
  EKA_LOG("groupId = %u",     msg->groupId);
  EKA_LOG("sequence = 0x%jx", msg->sequence);
  EKA_LOG("timestamp = 0x%jx",msg->timestamp);

  return 0;
}

/* ########################################################### */

int processFireReport(EkaDev* dev, const uint8_t* srcReport,uint len, uint32_t epmReportIndex) {
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

  //  hexDump("processFireReport: triggerOrder",&report->triggerOrder,sizeof(EfcFiredOrder));
  //  printFireOrder(dev,&report->triggerOrder);

  auto mdReport { reinterpret_cast<EfcMdReport*>(b) };
  mdReport->timestamp   = report->triggerOrder.timestamp;
  mdReport->sequence    = report->triggerOrder.sequence;
  mdReport->side        = report->triggerOrder.attr.bitmap.Side;
  mdReport->price       = report->triggerOrder.price;
  mdReport->size        = report->triggerOrder.size;
  mdReport->security_id = report->triggerOrder.securityId;
  mdReport->group_id    = report->triggerOrder.groupId;
  mdReport->core_id     = report->triggerOrder.attr.bitmap.CoreID;

  //  printMdReport(dev,mdReport);

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

  //  printSecCtx  (dev,secCtxReport);

  b += sizeof(EfcSecurityCtx);

  //--------------------------------------------------------------------------
  ((EfcReportHdr*)b)->type = EfcReportType::kFirePkt;
  ((EfcReportHdr*)b)->idx  = ++reportIdx;
  ((EfcReportHdr*)b)->size = userReport->hdr.length;
  b += sizeof(EfcReportHdr);

  memcpy(b,&userReport->data,userReport->hdr.length);
  b += userReport->hdr.length;

  //--------------------------------------------------------------------------
  ((EkaContainerGlobalHdr*)&reportBuf[0])->num_of_reports = reportIdx;

  int reportLen = b - &reportBuf[0];
  if (reportLen > (int)sizeof(reportBuf)) 
    on_error("reportLen %d > sizeof(reportBuf) %d",
	     reportLen,(int)sizeof(reportBuf));


  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  if (efc->localCopyEfcRunCtx.onEfcFireReportCb == NULL) 
    on_error("onFireReportCb == NULL");
  efc->localCopyEfcRunCtx.onEfcFireReportCb(&efc->localCopyEfcCtx,
					    reinterpret_cast< EfcFireReport* >(reportBuf), 
					    reportLen);
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

    //    hexDump("ekaFireReportThread: EPM/Fire report",data,len); fflush(stdout);

    if (((report_dma_report_t*)data)->length + sizeof(report_dma_report_t) != len) {
      hexDump("EPM report",data,len); fflush(stdout);
      on_error("DMA length mismatch %u != %u",
	       ((report_dma_report_t*)data)->length,len);
    }

    uint32_t epmReportIndex = ((report_dma_report_t*)data)->feedbackDmaIndex;
    const uint8_t* payload = data + sizeof(report_dma_report_t);

    switch ((EkaUserChannel::DMA_TYPE)((report_dma_report_t*)data)->type) {
      /* ----------------------------------------------- */
    case EkaUserChannel::DMA_TYPE::EPM:
      processEpmReport(dev,payload,len);
      break;
      /* ----------------------------------------------- */
    case EkaUserChannel::DMA_TYPE::FIRE:
      processFireReport(dev,payload,len,epmReportIndex);
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
