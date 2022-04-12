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

#include "eka_macros.h"

#include "Efh.h"
#include "Efc.h"
#include "Epm.h"
#include "EkaDev.h"
#include "EkaEfc.h"

#include "EkaEfcDataStructs.h"
#include "EkaHwExceptionsDecode.h"

extern EkaDev *g_ekaDev;

/* ########################################################### */
inline size_t printContainerGlobalHdr(FILE* file, const uint8_t* b) {
  auto containerHdr {reinterpret_cast<const EkaContainerGlobalHdr*>(b)};
  fprintf(file,"%s with %d reports\n",
	  EkaEventType2STR(containerHdr->type),
	  containerHdr->num_of_reports);

  return sizeof(*containerHdr);
}
/* ########################################################### */
inline size_t printControllerState(FILE* file, const uint8_t* b) {
  auto controllerState {reinterpret_cast<const EfcControllerState*>(b)};
  fprintf(file,"ControllerState (0x%02x):\n",controllerState->fire_reason);
  fprintf(file,"\tforce_fire = %d\n", (controllerState->fire_reason & EFC_FIRE_REASON_FORCE_FIRE) != 0);
  fprintf(file,"\tpass_bid   = %d\n", (controllerState->fire_reason & EFC_FIRE_REASON_PASS_BID) != 0);
  fprintf(file,"\tpass_ask   = %d\n", (controllerState->fire_reason & EFC_FIRE_REASON_PASS_ASK) != 0);
  fprintf(file,"\tsubscribed = %d\n", (controllerState->fire_reason & EFC_FIRE_REASON_SUBSCRIBED) != 0);
  fprintf(file,"\tarmed      = %d\n", (controllerState->fire_reason & EFC_FIRE_REASON_ARMED) != 0);

  return sizeof(*controllerState);
}
/* ########################################################### */
inline size_t printExceptionReport(FILE* file, const uint8_t* b) {
  auto exceptionsReport {reinterpret_cast<const EfcExceptionsReport*>(b)};
  char excptBuf[2048] = {};
  int decodeSize = ekaDecodeExceptions(excptBuf,exceptionsReport);
  if ((uint64_t)decodeSize > sizeof(excptBuf))
    on_error("decodeSize %d > sizeof(excptBuf) %jd",
	     decodeSize,sizeof(excptBuf));
  fprintf(file,"ExceptionsReport:\n");
  fprintf(file,"%s\n",excptBuf);
  return sizeof(*exceptionsReport);
}
/* ########################################################### */
size_t printSecurityCtx(FILE* file, const uint8_t* b) {
  auto secCtxReport {reinterpret_cast<const SecCtx*>(b)};
  fprintf(file,"SecurityCtx:");
  fprintf(file,"\tlowerBytesOfSecId = 0x%x \n",secCtxReport->lowerBytesOfSecId);
  fprintf(file,"\taskSize = %u\n",             secCtxReport->askSize);
  fprintf(file,"\tbidSize = %u\n",             secCtxReport->bidSize);
  fprintf(file,"\taskMaxPrice = %u (%u)\n",    secCtxReport->askMaxPrice, secCtxReport->askMaxPrice * 100);
  fprintf(file,"\tbidMinPrice = %u (%u)\n",    secCtxReport->bidMinPrice, secCtxReport->bidMinPrice * 100);
  fprintf(file,"\tversionKey = %u (0x%x)\n",   secCtxReport->versionKey,  secCtxReport->versionKey);

  return sizeof(*secCtxReport);
}
/* ########################################################### */
size_t printMdReport(FILE* file, const uint8_t* b) {
  auto mdReport {reinterpret_cast<const EfcMdReport*>(b)};
  fprintf(file,"MdReport:");
  fprintf(file,"\tGroup = %hhu\n", mdReport->group_id);
  fprintf(file,"\tSequence no = %ju\n", intmax_t(mdReport->sequence));
  fprintf(file,"\tSID = 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
      (uint8_t)((mdReport->security_id >> 7*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 6*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 5*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 4*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 3*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 2*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 1*8) & 0xFF),
      (uint8_t)((mdReport->security_id >> 0*8) & 0xFF));
  fprintf(file,"\tSide = %c\n",    mdReport->side == 1 ? 'B' : 'S');
  fprintf(file,"\tPrice = %8jd\n", intmax_t(mdReport->price));
  fprintf(file,"\tSize = %8ju\n",  intmax_t(mdReport->size));

  return sizeof(*mdReport);
}
/* ########################################################### */
int printBoeFire(FILE* file,const uint8_t* b) {
  auto boeOrder {reinterpret_cast<const BoeNewOrderMsg*>(b)};
  fprintf(file,"Fired BOE NewOrder:");

  fprintf(file,"\tStartOfMessage=0x%04x\n",    boeOrder->StartOfMessage);
  fprintf(file,"\tMessageLength=0x%04x (%u)\n",boeOrder->MessageLength,boeOrder->MessageLength);
  fprintf(file,"\tMessageType=0x%x\n",         boeOrder->MessageType);
  fprintf(file,"\tMatchingUnit=%x\n",          boeOrder->MatchingUnit);
  fprintf(file,"\tSequenceNumber=%u\n",        boeOrder->SequenceNumber);
  fprintf(file,"\tClOrdID=\'%s\'\n",
	  std::string(boeOrder->ClOrdID,sizeof(boeOrder->ClOrdID)).c_str());
  fprintf(file,"\tSide=\'%c\'\n",              boeOrder->Side);
  fprintf(file,"\tOrderQty=0x%08x (%u)\n",     boeOrder->OrderQty,boeOrder->OrderQty);
  fprintf(file,"\tNumberOfBitfields=0x%x\n",   boeOrder->NumberOfBitfields);
  fprintf(file,"\tNewOrderBitfield1=0x%x\n",   boeOrder->NewOrderBitfield1);
  fprintf(file,"\tNewOrderBitfield2=0x%x\n",   boeOrder->NewOrderBitfield2);
  fprintf(file,"\tNewOrderBitfield3=0x%x\n",   boeOrder->NewOrderBitfield3);
  fprintf(file,"\tNewOrderBitfield4=0x%x\n",   boeOrder->NewOrderBitfield4);
  fprintf(file,"\tClearingFirm=\'%s\'\n",
	  std::string(boeOrder->ClearingFirm,sizeof(boeOrder->ClearingFirm)).c_str());
  fprintf(file,"\tClearingAccount=\'%s\'\n",
	  std::string(boeOrder->ClearingAccount,sizeof(boeOrder->ClearingAccount)).c_str());

  fprintf(file,"\tPrice=0x%016jx (%ju)\n",     boeOrder->Price,boeOrder->Price);
  fprintf(file,"\tOrdType=\'%c\'\n",           boeOrder->OrdType);
  fprintf(file,"\tTimeInForce=\'%c\'\n",       boeOrder->TimeInForce);
  fprintf(file,"\tSymbol=\'%c%c%c%c%c%c%c%c\' (0x%016jx)\n",
	  boeOrder->Symbol[7],
	  boeOrder->Symbol[6],
	  boeOrder->Symbol[5],
	  boeOrder->Symbol[4],
	  boeOrder->Symbol[3],
	  boeOrder->Symbol[2],
	  boeOrder->Symbol[1],
	  boeOrder->Symbol[0],
	  *(uint64_t*)boeOrder->Symbol);
  fprintf(file,"\tCapacity=\'%c\'\n",          boeOrder->Capacity);
  fprintf(file,"\tAccount=\'%s\'\n",
	  std::string(boeOrder->Account,sizeof(boeOrder->Account)).c_str());
  fprintf(file,"\tOpenClose=\'%c\'\n",         boeOrder->OpenClose);

  return 0;
}

/* ########################################################### */
size_t printFirePkt(FILE* file, const uint8_t* b, size_t size) {
  auto dev = g_ekaDev;
  if (!dev) on_error("!g_ekaDev");
  auto epm {static_cast<const EkaEpm*>(dev->epm)};
  if (!epm) on_error("!epm");
  auto efc {static_cast<const EkaEfc*>(epm->strategy[EFC_STRATEGY])};
  
  fprintf(file,"FirePktReport:");
  switch (efc->hwFeedVer) {
  case EfhFeedVer::kCBOE :
    printBoeFire(file,b);
  default:
    hexDump("Fired Pkt",b,size,file);
  }
  return size;
}
/* ########################################################### */
int printEpmReport(FILE* file,const uint8_t* b) {
  auto epmReport {reinterpret_cast<const EpmFireReport*>(b)};

  fprintf(file,"StrategyId=%d,ActionId=%d,TriggerActionId=%d,"
	  "TriggerSource=%s,triggerToken=%016jx,user=%016jx,"
	  "preLocalEnable=%016jx,postLocalEnable=%016jx,"
	  "preStratEnable=%016jx,postStratEnable=%016jx\n",
	  epmReport->strategyId,
	  epmReport->actionId,
	  epmReport->action,
	  epmReport->local ? "FROM SW" : "FROM UDP\n",
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
  auto file {reinterpret_cast<std::FILE*>(ctx)};
  if (!file) file = stdout;

  auto b = static_cast<const uint8_t*>(p);

  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<const EkaContainerGlobalHdr*>(b)};
  b += printContainerGlobalHdr(file,b);
  //--------------------------------------------------------------------------
  for (uint i = 0; i < containerHdr->num_of_reports; i++) {
    auto reportHdr {reinterpret_cast<const EfcReportHdr*>(b)};
    b += sizeof(*reportHdr);
    switch (reportHdr->type) {
    case EfcReportType::kControllerState:
      b += printControllerState(file,b);
      break;
    case EfcReportType::kExceptionReport:
      b += printExceptionReport(file,b);
      break;
    case EfcReportType::kMdReport:
      b += printMdReport(file,b);
      break;
    case EfcReportType::kSecurityCtx:
      b += printSecurityCtx(file,b);
      break;
    case EfcReportType::kFirePkt:
      b += printFirePkt(file,b,reportHdr->size);
      break;
    case EfcReportType::kEpmReport:
      b += printEpmReport(file,b);
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
