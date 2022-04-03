#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <errno.h>
#include <thread>

#include "Efh.h"
#include "Efc.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaSnDev.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "EkaEfc.h"

#include "EkaEfcDataStructs.h"

#if 0
EkaOpResult efcPrintFireReport_back( EfcCtx* pEfcCtx, const EfcReportHdr* p, bool mdOnly) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  const uint8_t* b = (const uint8_t*)p;
  //--------------------------------------------------------------------------
  if (((EkaContainerGlobalHdr*)b)->type == EkaEventType::kExceptionReport) {
    EKA_LOG("EXCEPTION_REPORT");
    return EKA_OPRESULT__OK;
  }
  //--------------------------------------------------------------------------
  if (((EkaContainerGlobalHdr*)b)->type != EkaEventType::kFireReport) 
    on_error("UNKNOWN Event report: 0x%02x",
	     static_cast< uint32_t >( ((EkaContainerGlobalHdr*)b)->type ) );

  /* EKA_LOG("\tTotal reports: %u",((EkaContainerGlobalHdr*)b)->num_of_reports); */
  uint total_reports = ((EkaContainerGlobalHdr*)b)->num_of_reports;
  b += sizeof(EkaContainerGlobalHdr);
  //--------------------------------------------------------------------------
  if (((EfcReportHdr*)b)->type != EfcReportType::kControllerState) 
    on_error("EfcControllerState report expected, 0x%02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type));

  /* EKA_LOG("\treport_type = %u EfcControllerState, idx=%u, size=%ju", */
  /* 	  static_cast< uint32_t >( ((EfcReportHdr*)b)->type ), */
  /* 	  ((EfcReportHdr*)b)->idx, */
  /* 	  ((EfcReportHdr*)b)->size); */
  b += sizeof(EfcReportHdr);

  {
    auto msg{ reinterpret_cast< const EfcControllerState* >( b ) };

#ifdef _PRINT_UNSUBSCRIBED_ONLY
    if ((msg->fire_reason & EFC_FIRE_REASON_SUBSCRIBED) != 0)
      return EKA_OPRESULT__OK;
#endif
    
    if (! mdOnly) printControllerStateReport(dev,msg);
    b += sizeof(EfcControllerState);
  }
  total_reports--;
  //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kMdReport) 
    on_error("MdReport report expected, %02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  /* EKA_LOG("\treport_type = %u MdReport, idx=%u, size=%ju", */
  /* 	  static_cast< uint32_t >( ((EfcReportHdr*)b)->type ), */
  /* 	  ((EfcReportHdr*)b)->idx, */
  /* 	  ((EfcReportHdr*)b)->size); */
  b += sizeof(EfcReportHdr);

  {
    auto msg{ reinterpret_cast< const EfcMdReport* >( b ) };

    printMdReport(dev,msg);

    b += sizeof(*msg);
  }
  total_reports--;

  //--------------------------------------------------------------------------

  if (((EfcReportHdr*)b)->type != EfcReportType::kSecurityCtx) 
    on_error("SecurityCtx report expected, %02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  /* EKA_LOG("\treport_type = %u SecurityCtx, idx=%u, size=%ju", */
  /* 	  static_cast< uint32_t >( ((EfcReportHdr*)b)->type ), */
  /* 	  ((EfcReportHdr*)b)->idx, */
  /* 	  ((EfcReportHdr*)b)->size); */
  b += sizeof(EfcReportHdr);

  {
    auto msg{ reinterpret_cast< const SecCtx* >( b ) };

    if (! mdOnly) printSecCtx(dev, msg);

    b += sizeof(*msg);
  }
  total_reports--;

  //--------------------------------------------------------------------------
  if (((EfcReportHdr*)b)->type != EfcReportType::kFirePkt) 
    on_error("FirePkt report expected, %02x received",
	     static_cast< uint32_t >( ((EfcReportHdr*)b)->type) );
  EKA_LOG("\treport_type = %u SecurityCtx, idx=%u, size=%ju",
  	  static_cast< uint32_t >( ((EfcReportHdr*)b)->type ),
  	  ((EfcReportHdr*)b)->idx,
  	  ((EfcReportHdr*)b)->size);
  //    auto reportSize = ((EfcReportHdr*)b)->size;
  //    hexDump("FirePkt",b,reportSize);
  b += sizeof(EfcReportHdr);
  {
    // TEMPORARY SOLUTION FOR BOE!!!
    auto msg {reinterpret_cast<const BoeNewOrderMsg*>(b +
						      sizeof(EkaEthHdr) +
						      sizeof(EkaIpHdr) +
						      sizeof(EkaTcpHdr))};
    if (! mdOnly) printBoeFire(dev,msg);
    b += ((EfcReportHdr*)b)->size;
  }
  total_reports--;

  //--------------------------------------------------------------------------


  return EKA_OPRESULT__OK;
}
#endif

/* ########################################################### */
inline size_t printContainerGlobalHdr(EkaDev* dev, const uint8_t* b) {
  auto containerHdr {reinterpret_cast<const EkaContainerGlobalHdr*>(b)};
  EKA_LOG("%s with %d reports",
	  EkaEventType2STR(containerHdr->type),
	  containerHdr->num_of_reports);

  return sizeof(*containerHdr);
}
/* ########################################################### */
inline size_t printControllerState(EkaDev* dev, const uint8_t* b) {
  auto controllerState {reinterpret_cast<const EfcControllerState*>(b)};
  EKA_LOG("ControllerState (0x%02x):",controllerState->fire_reason);
  EKA_LOG("\tforce_fire = %d", (controllerState->fire_reason & EFC_FIRE_REASON_FORCE_FIRE) != 0);
  EKA_LOG("\tpass_bid   = %d", (controllerState->fire_reason & EFC_FIRE_REASON_PASS_BID) != 0);
  EKA_LOG("\tpass_ask   = %d", (controllerState->fire_reason & EFC_FIRE_REASON_PASS_ASK) != 0);
  EKA_LOG("\tsubscribed = %d", (controllerState->fire_reason & EFC_FIRE_REASON_SUBSCRIBED) != 0);
  EKA_LOG("\tarmed      = %d", (controllerState->fire_reason & EFC_FIRE_REASON_ARMED) != 0);

  return sizeof(*controllerState);
}
/* ########################################################### */
inline size_t printExceptionReport(EkaDev* dev, const uint8_t* b) {

  return sizeof(EfcControllerState);
}
/* ########################################################### */
size_t printSecurityCtx(EkaDev* dev, const uint8_t* b) {
  auto secCtxReport {reinterpret_cast<const SecCtx*>(b)};
  EKA_LOG("SecurityCtx:");
  EKA_LOG("\tlowerBytesOfSecId = 0x%x ",secCtxReport->lowerBytesOfSecId);
  EKA_LOG("\taskSize = %u",             secCtxReport->askSize);
  EKA_LOG("\tbidSize = %u",             secCtxReport->bidSize);
  EKA_LOG("\taskMaxPrice = %u (%u)",    secCtxReport->askMaxPrice, secCtxReport->askMaxPrice * 100);
  EKA_LOG("\tbidMinPrice = %u (%u)",    secCtxReport->bidMinPrice, secCtxReport->bidMinPrice * 100);
  EKA_LOG("\tversionKey = %u (0x%x)",   secCtxReport->versionKey,  secCtxReport->versionKey);

  return sizeof(*secCtxReport);
}
/* ########################################################### */
size_t printMdReport(EkaDev* dev, const uint8_t* b) {
  auto mdReport {reinterpret_cast<const EfcMdReport*>(b)};
  EKA_LOG("MdReport:");
  EKA_LOG("\tGroup = %hhu", mdReport->group_id);
  EKA_LOG("\tSequence no = %ju", intmax_t(mdReport->sequence));
  EKA_LOG("\tSID = 0x%02x%02x%02x%02x%02x%02x%02x%02x",
      (uint8_t)((mdReport->security_id >> 7*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 6*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 5*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 4*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 3*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 2*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 1*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 0*8) & 0xFF));
  EKA_LOG("\tSide = %c",    mdReport->side == 1 ? 'B' : 'S');
  EKA_LOG("\tPrice = %8jd", intmax_t(mdReport->price));
  EKA_LOG("\tSize = %8ju",  intmax_t(mdReport->size));

  return sizeof(*mdReport);
}
/* ########################################################### */
int printBoeFire(EkaDev* dev,const uint8_t* b) {
  auto boeOrder {reinterpret_cast<const BoeNewOrderMsg*>(b)};
  EKA_LOG("Fired BOE NewOrder:");

  EKA_LOG("\tStartOfMessage=0x%04x",    boeOrder->StartOfMessage);
  EKA_LOG("\tMessageLength=0x%04x (%u)",boeOrder->MessageLength,boeOrder->MessageLength);
  EKA_LOG("\tMessageType=0x%x",         boeOrder->MessageType);
  EKA_LOG("\tMatchingUnit=%x",          boeOrder->MatchingUnit);
  EKA_LOG("\tSequenceNumber=%u",        boeOrder->SequenceNumber);
  EKA_LOG("\tClOrdID=\'%s\'",
	  std::string(boeOrder->ClOrdID,sizeof(boeOrder->ClOrdID)).c_str());
  EKA_LOG("\tSide=\'%c\'",              boeOrder->Side);
  EKA_LOG("\tOrderQty=0x%08x (%u)",     boeOrder->OrderQty,boeOrder->OrderQty);
  EKA_LOG("\tNumberOfBitfields=0x%x",   boeOrder->NumberOfBitfields);
  EKA_LOG("\tNewOrderBitfield1=0x%x",   boeOrder->NewOrderBitfield1);
  EKA_LOG("\tNewOrderBitfield2=0x%x",   boeOrder->NewOrderBitfield2);
  EKA_LOG("\tNewOrderBitfield3=0x%x",   boeOrder->NewOrderBitfield3);
  EKA_LOG("\tNewOrderBitfield4=0x%x",   boeOrder->NewOrderBitfield4);
  EKA_LOG("\tClearingFirm=\'%s\'",
	  std::string(boeOrder->ClearingFirm,sizeof(boeOrder->ClearingFirm)).c_str());
  EKA_LOG("\tClearingAccount=\'%s\'",
	  std::string(boeOrder->ClearingAccount,sizeof(boeOrder->ClearingAccount)).c_str());

  EKA_LOG("\tPrice=0x%016jx (%ju)",     boeOrder->Price,boeOrder->Price);
  EKA_LOG("\tOrdType=\'%c\'",           boeOrder->OrdType);
  EKA_LOG("\tTimeInForce=\'%c\'",       boeOrder->TimeInForce);
  EKA_LOG("\tSymbol=\'%c%c%c%c%c%c%c%c\' (0x%016jx)",
	  boeOrder->Symbol[7],
	  boeOrder->Symbol[6],
	  boeOrder->Symbol[5],
	  boeOrder->Symbol[4],
	  boeOrder->Symbol[3],
	  boeOrder->Symbol[2],
	  boeOrder->Symbol[1],
	  boeOrder->Symbol[0],
	  *(uint64_t*)boeOrder->Symbol);
  EKA_LOG("\tCapacity=\'%c\'",          boeOrder->Capacity);
  EKA_LOG("\tAccount=\'%s\'",
	  std::string(boeOrder->Account,sizeof(boeOrder->Account)).c_str());
  EKA_LOG("\tOpenClose=\'%c\'",         boeOrder->OpenClose);

  return 0;
}

/* ########################################################### */
size_t printFirePkt(EkaDev* dev, const uint8_t* b, size_t size,EfhFeedVer hwFeedVer) {
  EKA_LOG("FirePktReport:");
  switch (hwFeedVer) {
  case EfhFeedVer::kCBOE :
    printBoeFire(dev,b);
  default:
    hexDump("Fired Pkt:",b,size);
  }
  return size;
}
/* ########################################################### */
int printEpmReport(EkaDev* dev,const uint8_t* b) {
  auto epmReport {reinterpret_cast<const EpmFireReport*>(b)};

  EKA_LOG("StrategyId=%d,ActionId=%d,TriggerActionId=%d,"
	  "TriggerSource=%s,triggerToken=%016jx,user=%016jx,"
	  "preLocalEnable=%016jx,postLocalEnable=%016jx,"
	  "preStratEnable=%016jx,postStratEnable=%016jx",
  	   epmReport->strategyId,
  	   epmReport->actionId,
  	   epmReport->action,
  	   epmReport->local ? "FROM SW" : "FROM UDP",
  	   epmReport->triggerToken,
  	   epmReport->user,
  	   epmReport->preLocalEnable,
  	   epmReport->postLocalEnable,
  	   epmReport->preStratEnable,
  	   epmReport->postStratEnable
  	   );
  return sizeof(*epmReport);
}
/* ########################################################### */

void efcPrintFireReport(const void* p, size_t len, void* ctx) {
  auto dev {reinterpret_cast<EkaDev*>(ctx)};
  if (!dev) on_error("!dev");

  auto epm {dynamic_cast<EkaEpm*>(dev->epm)};
  if (!epm) on_error("!epm");
  
  auto efc {dynamic_cast<EkaEfc*>(epm->strategy[EFC_STRATEGY])};
  if (!efc) on_error("!efc");

  auto b = static_cast<const uint8_t*>(p);

  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<const EkaContainerGlobalHdr*>(b)};
  b += printContainerGlobalHdr(dev,b);
  //--------------------------------------------------------------------------
  for (uint i = 0; i < containerHdr->num_of_reports; i++) {
    auto reportHdr {reinterpret_cast<const EfcReportHdr*>(b)};
    b += sizeof(*reportHdr);
    switch (reportHdr->type) {
    case EfcReportType::kControllerState:
      b += printControllerState(dev,b);
      break;
    case EfcReportType::kExceptionReport:
      b += printExceptionReport(dev,b);
      break;
    case EfcReportType::kMdReport:
      b += printMdReport(dev,b);
      break;
    case EfcReportType::kSecurityCtx:
      b += printSecurityCtx(dev,b);
      break;
    case EfcReportType::kFirePkt:
      b += printFirePkt(dev,b,reportHdr->size,efc->hwFeedVer);
      break;
    case EfcReportType::kEpmReport:
      b += printEpmReport(dev,b);
      break;
    default:
      on_error("Unexpected reportHdr->type %d",
	       (int)reportHdr->type);
    }
    if (b - static_cast<const uint8_t*>(p) > (int64_t)len)
      on_error("FireReport parsing error: parsed bytes %jd > declared bytes %jd",
	       b - static_cast<const uint8_t*>(p), (int64_t)len);
  }
}
