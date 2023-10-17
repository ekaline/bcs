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
#include "EkaBc.h"
#include "EkaDev.h"
#include "EkaEfc.h"
#include "EkaEfcDataStructs.h"
#include "EkaHwExceptionsDecode.h"
#include "Epm.h"

extern EkaDev *g_ekaDev;

/* #################################################### */
inline size_t printBCContainerGlobalHdr(FILE *file,
                                        const uint8_t *b) {
  auto containerHdr{
      reinterpret_cast<const EkaBCContainerGlobalHdr *>(b)};
  switch (containerHdr->type) {
  case EkaBCExceptionEvent:
    break;
  default:
    fprintf(file, "BCPrints GlobalHdr %s with %d reports\n",
            EkaBCEventType2STR(containerHdr->type),
            containerHdr->num_of_reports);
  }

  return sizeof(*containerHdr);
}

/* #################################################### */
size_t printBCFirePkt(FILE *file, const uint8_t *b,
                      size_t size) {
  hexDump("BCPrints Fired Pkt", b, size, file);
  return size;
}

/* ####################################################  */

inline int
ekaBCDecodeExceptions(char *dst,
                      const EfcBCExceptionsReport *excpt) {
  auto d = dst;
  bool exceptionRaised = false;
  if (excpt->exceptionStatus.globalVector)
    exceptionRaised = true;
  ;
  for (auto i = 0; i < 4; i++) { // 4 Cores
    if (excpt->exceptionStatus.portVector[i])
      exceptionRaised = true;
    ;
  }
  if (exceptionRaised)
    d += sprintf(d, "\n\nFPGA internal exceptions:\n");
  else
    goto END;
  if ((excpt->exceptionStatus.globalVector >> 0) & 0x1)
    d += sprintf(
        d,
        "Bit 0: SW->HW Configuration Write Corruption\n");
  if ((excpt->exceptionStatus.globalVector >> 1) & 0x1)
    d += sprintf(
        d, "Bit 1: HW->SW Dummy Packet DMA data full\n");
  if ((excpt->exceptionStatus.globalVector >> 2) & 0x1)
    d += sprintf(
        d, "Bit 2: HW->SW Dummy Packet DMA control full\n");
  if ((excpt->exceptionStatus.globalVector >> 3) & 0x1)
    d += sprintf(
        d, "Bit 3: HW->SW Report feedback DMA data full\n");
  if ((excpt->exceptionStatus.globalVector >> 4) & 0x1)
    d += sprintf(
        d,
        "Bit 4: HW->SW Report feedback DMA control full\n");
  if ((excpt->exceptionStatus.globalVector >> 5) & 0x1)
    d += sprintf(d, "Bit 5: Controller Watchdog Expired\n");
  if ((excpt->exceptionStatus.globalVector >> 6) & 0x1)
    d += sprintf(
        d, "Bit 6: SW->HW EPM Trigger control full\n");
  if ((excpt->exceptionStatus.globalVector >> 7) & 0x1)
    d += sprintf(d, "Bit 7: EPM Wrong Action Type\n");
  if ((excpt->exceptionStatus.globalVector >> 8) & 0x1)
    d += sprintf(d, "Bit 8: EPM Wrong Action Source\n");
  if ((excpt->exceptionStatus.globalVector >> 9) & 0x1)
    d += sprintf(d, "Bit 9: Strategy Double Fire "
                    "Protection Triggered\n");
  if ((excpt->exceptionStatus.globalVector >> 10) & 0x1)
    d += sprintf(d,
                 "Bit 10: Strategy Context FIFO overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 11) & 0x1)
    d += sprintf(
        d, "Bit 11: Strategy MD vs Context Out of Sync\n");
  if ((excpt->exceptionStatus.globalVector >> 12) & 0x1)
    d += sprintf(d,
                 "Bit 12: Strategy Security ID mismatch\n");
  if ((excpt->exceptionStatus.globalVector >> 13) & 0x1)
    d += sprintf(
        d, "Bit 13: SW->HW Strategy Update overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 14) & 0x1)
    d += sprintf(d, "Bit 14: Write Combining: WC to EPM "
                    "fifo overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 15) & 0x1)
    d += sprintf(
        d, "Bit 15: Write Combining: CTRL 16B unaligned\n");
  if ((excpt->exceptionStatus.globalVector >> 16) & 0x1)
    d += sprintf(
        d, "Bit 16: Write Combining: CTRL fragmented\n");
  if ((excpt->exceptionStatus.globalVector >> 17) & 0x1)
    d += sprintf(d,
                 "Bit 17: Write Combining: Missing TLP\n");
  if ((excpt->exceptionStatus.globalVector >> 18) & 0x1)
    d += sprintf(
        d,
        "Bit 18: Write Combining: Unknown CTRL Opcode\n");
  if ((excpt->exceptionStatus.globalVector >> 19) & 0x1)
    d += sprintf(
        d,
        "Bit 19: Write Combining: Unaligned data in TLP\n");
  if ((excpt->exceptionStatus.globalVector >> 20) & 0x1)
    d += sprintf(d, "Bit 20: Write Combining: Unaligned "
                    "data in non-TLP\n");
  if ((excpt->exceptionStatus.globalVector >> 21) & 0x1)
    d += sprintf(d, "Bit 21: Write Combining: CTRL bitmap "
                    "ff override\n");
  if ((excpt->exceptionStatus.globalVector >> 22) & 0x1)
    d += sprintf(
        d, "Bit 22: Write Combining: CTRL LSB override\n");
  if ((excpt->exceptionStatus.globalVector >> 23) & 0x1)
    d += sprintf(
        d, "Bit 23: Write Combining: CTRL MSB override\n");
  if ((excpt->exceptionStatus.globalVector >> 24) & 0x1)
    d += sprintf(
        d, "Bit 24: Write Combining: Partial CTRL info\n");
  if ((excpt->exceptionStatus.globalVector >> 25) & 0x1)
    d += sprintf(
        d,
        "Bit 25: Write Combining: pre-WC fifo overrun\n");
  if ((excpt->exceptionStatus.globalVector >> 26) & 0x1)
    d += sprintf(d, "Bit 26: Write Combining: Same heap "
                    "bank violation\n");

  for (auto i = 0; i < 4; i++) { // 4 Cores
    if (excpt->exceptionStatus.portVector[i]) {
      d += sprintf(d, "\nResolving exception for Core%d\n",
                   i);
      if ((excpt->exceptionStatus.portVector[i] >> 0) & 0x1)
        d += sprintf(d, "Bit 0: MD Parser Error\n");
      if ((excpt->exceptionStatus.portVector[i] >> 1) & 0x1)
        d += sprintf(
            d,
            "Bit 1: TCP Sequence Management Corruption\n");
      if ((excpt->exceptionStatus.portVector[i] >> 2) & 0x1)
        d += sprintf(
            d, "Bit 2: TCP Ack Management Corruption\n");
      if ((excpt->exceptionStatus.portVector[i] >> 3) & 0x1)
        d += sprintf(d, "Bit 3: HW->SW LWIP data drop\n");
      if ((excpt->exceptionStatus.portVector[i] >> 4) & 0x1)
        d +=
            sprintf(d, "Bit 4: HW->SW LWIP control drop\n");
      if ((excpt->exceptionStatus.portVector[i] >> 5) & 0x1)
        d +=
            sprintf(d, "Bit 5: HW->SW Sniffer data drop\n");
      if ((excpt->exceptionStatus.portVector[i] >> 6) & 0x1)
        d += sprintf(
            d, "Bit 6: HW->SW Sniffer control drop\n");
    }
  }
END:
  //  d += sprintf(d,"Arm=%d,
  //  Ver=%d\n",excpt->armStatus.armFlag,excpt->armStatus.expectedVersion);
  return d - dst;
}

/* #################################################### */

inline size_t printBCExceptionReport(FILE *file,
                                     const uint8_t *b) {
  auto exceptionsReport{
      reinterpret_cast<const EfcBCExceptionsReport *>(b)};
  char excptBuf[2048] = {};
  int decodeSize =
      ekaBCDecodeExceptions(excptBuf, exceptionsReport);
  if ((uint64_t)decodeSize > sizeof(excptBuf))
    on_error("decodeSize %d > sizeof(excptBuf) %jd",
             decodeSize, sizeof(excptBuf));
  if ((uint64_t)decodeSize > 0) {
    fprintf(file, "ExceptionsReport:\n");
    fprintf(file, "%s\n", excptBuf);
    fprintf(stderr, RED "%s\n" RESET, excptBuf);
  }

  //  fprintf(file,"BCPrints ArmStatus:");
  //  fprintf(file,"\tarmFlag = %u \n"
  //  ,exceptionsReport->armStatus.armFlag);
  //  fprintf(file,"\texpectedVersion = %u\n",
  //  exceptionsReport->armStatus.expectedVersion);
  return sizeof(*exceptionsReport);
}

struct EpmBCFireReport {
  int32_t
      strategyId; ///< Strategy ID the report corresponds to
  int32_t actionId; ///< Action ID the report corresponds to
  int32_t triggerActionId; ///< Action ID of the Trigger
  uint64_t triggerToken; ///< Security token of the Trigger
  EpmBCTriggerAction
      action; ///< What device did in response to trigger
  EkaBCOpResult error;      ///< Error code for SendError
  uint64_t preLocalEnable;  ///< Action-level enable bits
                            ///< before fire
  uint64_t postLocalEnable; ///< Action-level enable bits
                            ///< after firing
  uint64_t preStratEnable;  ///< Strategy-level enable bits
                            ///< before fire
  uint64_t postStratEnable; ///< Strategy-level enable bits
                            ///< after fire
  uintptr_t user; ///< Opaque value copied from EpmAction
  bool local;     ///< True -> called from epmRaiseTrigger
};

struct EpmBCFastCancelReport {
  uint8_t eventIsZero;     ///< Field from trigger MD
  uint8_t numInGroup;      ///< Field from trigger MD
  uint64_t transactTime;   ///< Field from trigger MD
  uint64_t headerTime;     ///< Field from trigger MD
  uint32_t sequenceNumber; ///< Field from trigger MD
};

/* #################################################### */
int printBCEpmReport(FILE *file, const uint8_t *b) {
  auto epmReport{
      reinterpret_cast<const EpmBCFireReport *>(b)};

  fprintf(file,
          "BCPrints EpmReport "
          "StrategyId=%d,ActionId=%d,TriggerActionId=%d",
          epmReport->strategyId, epmReport->actionId,
          (int)epmReport->action);
  return sizeof(*epmReport);
}

/* #################################################### */
int printBCFastCancelReport(FILE *file, const uint8_t *b) {
  auto epmReport{
      reinterpret_cast<const EpmBCFastCancelReport *>(b)};

  fprintf(file,
          "BCPrints FC Report "
          "eventIsZero=%d,numInGroup=%d,transactTime=%ju,"
          "headerTime=%ju,sequenceNumber=%u\n",
          epmReport->eventIsZero, epmReport->numInGroup,
          epmReport->transactTime, epmReport->headerTime,
          epmReport->sequenceNumber);
  return sizeof(*epmReport);
}

void efcBCPrintFireReport(const void *p, size_t len,
                          void *ctx) {
  auto file{reinterpret_cast<std::FILE *>(ctx)};
  if (!file)
    file = stdout;

  auto b = static_cast<const uint8_t *>(p);

  //--------------------------------------------------------------------------
  auto containerHdr{
      reinterpret_cast<const EkaBCContainerGlobalHdr *>(b)};
  b += printBCContainerGlobalHdr(file, b);
  //--------------------------------------------------------------------------
  for (uint i = 0; i < containerHdr->num_of_reports; i++) {
    auto reportHdr{
        reinterpret_cast<const EfcBCReportHdr *>(b)};
    b += sizeof(*reportHdr);
    switch (reportHdr->type) {
    case EfcBCReportType::ExceptionReport:
      b += printBCExceptionReport(file, b);
      break;
    case EfcBCReportType::FirePkt:
      b += printBCFirePkt(file, b, reportHdr->size);
      break;
    case EfcBCReportType::EpmReport:
      b += printBCEpmReport(file, b);
      break;
    case EfcBCReportType::FastCancelReport:
      b += printBCFastCancelReport(file, b);
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
