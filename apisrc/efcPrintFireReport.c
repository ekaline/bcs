#include <arpa/inet.h>
#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <linux/sockios.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "eka_macros.h"

#include "Efc.h"
#include "Efh.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "Epm.h"

#include "EkaEfcDataStructs.h"
#include "EkaHwExceptionsDecode.h"

extern EkaDev *g_ekaDev;

/* ##################################################  */
inline size_t printContainerGlobalHdr(FILE *file,
                                      const uint8_t *b) {
  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(b)};
  switch (containerHdr->type) {
  case EkaEventType::kExceptionEvent:
    break;
  default:
    fprintf(file, "%s with %d reports\n",
            EkaEventType2STR(containerHdr->type),
            containerHdr->num_of_reports);
  }

  return sizeof(*containerHdr);
}
/* ##################################################  */
inline size_t printControllerState(FILE *file,
                                   const uint8_t *b) {
  auto controllerState{
      reinterpret_cast<const EfcControllerState *>(b)};
  fprintf(file, "ControllerState (0x%02x):\n",
          controllerState->fire_reason);
  fprintf(file, "\tforce_fire = %d\n",
          (controllerState->fire_reason &
           EFC_FIRE_REASON_FORCE_FIRE) != 0);
  fprintf(file, "\tpass_bid   = %d\n",
          (controllerState->fire_reason &
           EFC_FIRE_REASON_PASS_BID) != 0);
  fprintf(file, "\tpass_ask   = %d\n",
          (controllerState->fire_reason &
           EFC_FIRE_REASON_PASS_ASK) != 0);
  fprintf(file, "\tsubscribed = %d\n",
          (controllerState->fire_reason &
           EFC_FIRE_REASON_SUBSCRIBED) != 0);
  fprintf(file, "\tarmed      = %d\n",
          (controllerState->fire_reason &
           EFC_FIRE_REASON_ARMED) != 0);

  return sizeof(*controllerState);
}
/* ##################################################  */
inline size_t printExceptionReport(FILE *file,
                                   const uint8_t *b) {
  auto exceptionsReport{
      reinterpret_cast<const EfcExceptionsReport *>(b)};
  char excptBuf[2048] = {};
  int decodeSize =
      ekaDecodeExceptions(excptBuf, exceptionsReport);
  if ((uint64_t)decodeSize > sizeof(excptBuf))
    on_error("decodeSize %d > sizeof(excptBuf) %jd",
             decodeSize, sizeof(excptBuf));
  if ((uint64_t)decodeSize > 0) {
    fprintf(file, "ExceptionsReport:\n");
    fprintf(file, "%s\n", excptBuf);
    fprintf(stderr, RED "%s\n" RESET, excptBuf);
  }
  return sizeof(*exceptionsReport);
}
/* ##################################################  */
size_t printSecurityCtx(FILE *file, const uint8_t *b) {
  auto secCtxReport{reinterpret_cast<const SecCtx *>(b)};
  fprintf(file, "SecurityCtx:\n");
  fprintf(file, "\tlowerBytesOfSecId = 0x%x \n",
          secCtxReport->lowerBytesOfSecId);
  fprintf(file, "\taskSize = %u\n", secCtxReport->askSize);
  fprintf(file, "\tbidSize = %u\n", secCtxReport->bidSize);
  fprintf(file, "\taskMaxPrice = %u (%u)\n",
          secCtxReport->askMaxPrice,
          secCtxReport->askMaxPrice * 100);
  fprintf(file, "\tbidMinPrice = %u (%u)\n",
          secCtxReport->bidMinPrice,
          secCtxReport->bidMinPrice * 100);
  fprintf(file, "\tversionKey = %u (0x%x)\n",
          secCtxReport->versionKey,
          secCtxReport->versionKey);

  return sizeof(*secCtxReport);
}
/* ##################################################  */
size_t printMdReport(FILE *file, const uint8_t *b) {
  auto mdReport{reinterpret_cast<const EfcMdReport *>(b)};
  fprintf(file, "MdReport:\n");
  fprintf(file, "\tMdCoreId = %u\n", mdReport->core_id);
  fprintf(file, "\tGroup = %hhu\n", mdReport->group_id);
  fprintf(file, "\tSequence no = %ju\n",
          intmax_t(mdReport->sequence));
  fprintf(
      file, "\tSID = %s\n",
      cboeSecIdString(&mdReport->security_id, 8).c_str());
  fprintf(file, "\tSide = \'%c\'\n",
          mdReport->side == 1 ? 'B'
          : 2                 ? 'S'
                              : 'X');
  fprintf(file, "\tPrice = %8jd\n",
          intmax_t(mdReport->price));
  fprintf(file, "\tSize = %8ju\n",
          intmax_t(mdReport->size));

  return sizeof(*mdReport);
}
/* ##################################################  */
int printBoeFire(FILE *file, const uint8_t *b) {
  auto fireMsg{
      reinterpret_cast<const BoeQuoteUpdateShortMsg *>(
          b + sizeof(EkaEthHdr) + sizeof(EkaIpHdr) +
          sizeof(EkaTcpHdr))};
  fprintf(file, "Fired BoeQuoteUpdateShortMsg:\n");

  fprintf(file, "\tHWinsertedSeq=%.8s\n",
          fireMsg->QuoteUpdateID_seq);
  fprintf(file, "\tSymbol=\'%s\' (0x%016jx)\n",
          cboeSecIdString(fireMsg->Symbol, 6).c_str(),
          *(uint64_t *)fireMsg->Symbol);
  fprintf(file, "\tSide=\'%c\'\n",
          fireMsg->Side == 1 ? 'B'
          : 2                ? 'S'
                             : 'X');
  fprintf(file, "\tOrderQty=0x%08x (%u)\n",
          fireMsg->OrderQty, fireMsg->OrderQty);
  fprintf(file, "\tPrice=0x%08x (%u)\n", fireMsg->Price,
          fireMsg->Price);
  fprintf(file, "\n");
  return 0;
}

/* ##################################################  */
size_t printFirePkt(FILE *file, const uint8_t *b,
                    size_t size) {
  auto dev = g_ekaDev;
  if (!dev)
    on_error("!g_ekaDev");
  auto epm{static_cast<const EkaEpm *>(dev->epm)};
  if (!epm)
    on_error("!epm");
  auto efc = dev->efc;
  if (!efc)
    on_error("!efc");

  fprintf(file, "FirePktReport:");
  if (efc->p4_)
    switch (efc->p4_->feedVer_) {
    case EfhFeedVer::kCBOE:
      printBoeFire(file, b);
      return size;
    default:
      on_error("Unsupported EfhFeedVer %d",
               (int)efc->p4_->feedVer_);
    }
  hexDump("Fired Pkt", b, size, file);
  return size;
}
/* ##################################################  */
int printEpmReport(FILE *file, const uint8_t *b) {
  auto epmReport{
      reinterpret_cast<const EpmFireReport *>(b)};

  fprintf(file,
          "%s," // strategy
          "%s " // action type
          "ActionId=%d (%d),"
          "%s," // Sent/Error
          "%s," // TriggerSource
          "\n",
          epmReport->strategyId == EFC_STRATEGY
              ? "EFC"
              : "UnexpectedStrategy",
          printActionType(
              getActionTypeFromUser(epmReport->user)),
          getActionGlobalIdxFromUser(epmReport->user),
          epmReport->actionId,
          epmPrintFireEvent(epmReport->action),
          epmReport->local ? "FROM SW" : "FROM UDP");
  return sizeof(*epmReport);
}
/* ##################################################  */
int printFastCancelReport(FILE *file, const uint8_t *b) {
  auto epmReport{
      reinterpret_cast<const EpmFastCancelReport *>(b)};

  fprintf(file,
          "numInGroup=%d,headerSize=%d,sequenceNumber=%d\n",
          epmReport->numInGroup, epmReport->headerSize,
          epmReport->sequenceNumber);
  return sizeof(*epmReport);
}
/* ##################################################  */
int printFastSweepReport(FILE *file, const uint8_t *b) {
  auto epmReport{
      reinterpret_cast<const EpmFastSweepReport *>(b)};

  fprintf(file,
          "udpPayloadSize=%d,locateID=%d "
          "(0x%x),lastMsgNum=%d,firstMsgType=%c,"
          "lastMsgType=%c\n",
          epmReport->udpPayloadSize, epmReport->locateID,
          epmReport->locateID, epmReport->lastMsgNum,
          epmReport->firstMsgType, epmReport->lastMsgType);
  return sizeof(*epmReport);
}
/* ##################################################  */
int printQEDReport(FILE *file, const uint8_t *b) {
  auto epmReport{reinterpret_cast<const EpmQEDReport *>(b)};

  fprintf(file, "udpPayloadSize=%d,DSID=%d (0x%x)\n",
          epmReport->udpPayloadSize, epmReport->DSID,
          epmReport->DSID);
  return sizeof(*epmReport);
}
/* ##################################################  */
int printNewsReport(FILE *file, const uint8_t *b) {
  auto epmReport{
      reinterpret_cast<const EpmNewsReport *>(b)};

  fprintf(
      file,
      "strategyIndex=%d,strategyRegion=%d,token=0x%jx\n",
      epmReport->strategyIndex, epmReport->strategyRegion,
      epmReport->token);
  return sizeof(*epmReport);
}

/* ##################################################  */

void efcPrintFireReport(const void *p, size_t len,
                        void *ctx) {
  auto file{reinterpret_cast<std::FILE *>(ctx)};
  if (!file)
    file = stdout;

  auto b = static_cast<const uint8_t *>(p);

  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<const EkaContainerGlobalHdr *>(b)};
  b += printContainerGlobalHdr(file, b);
  //--------------------------------------------------------------------------
  for (uint i = 0; i < containerHdr->num_of_reports; i++) {
    auto reportHdr{
        reinterpret_cast<const EfcReportHdr *>(b)};
    b += sizeof(*reportHdr);
    switch (reportHdr->type) {
    case EfcReportType::kControllerState:
      b += printControllerState(file, b);
      break;
    case EfcReportType::kExceptionReport:
      b += printExceptionReport(file, b);
      break;
    case EfcReportType::kMdReport:
      b += printMdReport(file, b);
      break;
    case EfcReportType::kSecurityCtx:
      b += printSecurityCtx(file, b);
      break;
    case EfcReportType::kFirePkt:
      b += printFirePkt(file, b, reportHdr->size);
      break;
    case EfcReportType::kEpmReport:
      b += printEpmReport(file, b);
      break;
    case EfcReportType::kFastCancelReport:
      b += printFastCancelReport(file, b);
      break;
    case EfcReportType::kNewsReport:
      b += printNewsReport(file, b);
      break;
    case EfcReportType::kFastSweepReport:
      b += printFastSweepReport(file, b);
      break;
    case EfcReportType::kQEDReport:
      b += printQEDReport(file, b);
      break;
    default:
      on_error("Unexpected reportHdr->type %d",
               (int)reportHdr->type);
    }
    if (b - static_cast<const uint8_t *>(p) > (int64_t)len)
      on_error("FireReport parsing error: parsed bytes %jd "
               "> declared bytes %jd",
               b - static_cast<const uint8_t *>(p),
               (int64_t)len);
  }
}
