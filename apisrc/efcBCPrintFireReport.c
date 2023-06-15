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
#include "EkaBc.h"
#include "EkaEfcDataStructs.h"
#include "EkaHwExceptionsDecode.h"

extern EkaDev *g_ekaDev;

/* ########################################################### */
inline size_t printBCContainerGlobalHdr(FILE* file, const uint8_t* b) {
  auto containerHdr {reinterpret_cast<const EkaBCContainerGlobalHdr*>(b)};
  switch (containerHdr->type) {
  case EkaBCEventType::ExceptionEvent:
    break;
  default:
    fprintf(file,"BCPrints GlobalHdr %s with %d reports\n",
	    EkaBCEventType2STR(containerHdr->type),
	    containerHdr->num_of_reports);
  }
  

  return sizeof(*containerHdr);
}

/* ########################################################### */
size_t printBCFirePkt(FILE* file, const uint8_t* b, size_t size) {
  hexDump("BCPrints Fired Pkt",b,size,file);
  return size;
}

/* ########################################################### */
inline size_t printBCExceptionReport(FILE* file, const uint8_t* b) {
  auto exceptionsReport {reinterpret_cast<const EfcBCExceptionsReport*>(b)};
  char excptBuf[2048] = {};
  int decodeSize = ekaBCDecodeExceptions(excptBuf,exceptionsReport);
  if ((uint64_t)decodeSize > sizeof(excptBuf))
    on_error("decodeSize %d > sizeof(excptBuf) %jd",
	     decodeSize,sizeof(excptBuf));
  if ((uint64_t)decodeSize > 0) {
    fprintf(file,"ExceptionsReport:\n");
    fprintf(file,"%s\n",excptBuf);
    fprintf(stderr,RED "%s\n" RESET,excptBuf);
  }

  //  fprintf(file,"BCPrints ArmStatus:");
  //  fprintf(file,"\tarmFlag = %u \n"        ,exceptionsReport->armStatus.armFlag);
  //  fprintf(file,"\texpectedVersion = %u\n", exceptionsReport->armStatus.expectedVersion);
  return sizeof(*exceptionsReport);
}

/* ########################################################### */
int printBCEpmReport(FILE* file,const uint8_t* b) {
  auto epmReport {reinterpret_cast<const EpmBCFireReport*>(b)};

  fprintf(file,"BCPrints EpmReport StrategyId=%d,ActionId=%d,TriggerActionId=%d",
	  epmReport->strategyId,
	  epmReport->actionId,
	  (int)epmReport->action
	  );
  return sizeof(*epmReport);
}

/* ########################################################### */
int printBCFastCancelReport(FILE* file,const uint8_t* b) {
  auto epmReport {reinterpret_cast<const EpmBCFastCancelReport*>(b)};
  
  fprintf(file,"BCPrints FC Report eventIsZero=%d,numInGroup=%d,transactTime=%ju,headerTime=%ju,sequenceNumber=%u\n",
	  epmReport->eventIsZero,
	  epmReport->numInGroup,
	  epmReport->transactTime,
	  epmReport->headerTime,
	  epmReport->sequenceNumber
	  );
  return sizeof(*epmReport);
}

void efcBCPrintFireReport(const void* p, size_t len, void* ctx) {
  auto file {reinterpret_cast<std::FILE*>(ctx)};
  if (!file) file = stdout;

  auto b = static_cast<const uint8_t*>(p);

  //--------------------------------------------------------------------------
  auto containerHdr {reinterpret_cast<const EkaBCContainerGlobalHdr*>(b)};
  b += printBCContainerGlobalHdr(file,b);
  //--------------------------------------------------------------------------
  for (uint i = 0; i < containerHdr->num_of_reports; i++) {
    auto reportHdr {reinterpret_cast<const EfcBCReportHdr*>(b)};
    b += sizeof(*reportHdr);
    switch (reportHdr->type) {
    case EfcBCReportType::ExceptionReport:
      b += printBCExceptionReport(file,b);
      break;
    case EfcBCReportType::FirePkt:
      b += printBCFirePkt(file,b,reportHdr->size);
      break;
    case EfcBCReportType::EpmReport:
      b += printBCEpmReport(file,b);
      break;
    case EfcBCReportType::FastCancelReport:
      b += printBCFastCancelReport(file,b);
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
