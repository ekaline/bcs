#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "Efc.h"
#include "Efh.h"
#include "Eka.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "Exc.h"
#include "eka_macros.h"

#include "EkaFhGroup.h"

#include "EkaFhBatsParser.h"

/* #include "EfhBatsProps.h" */
/* #include "EfhBoxProps.h" */
/* #include "EfhGemProps.h" */
/* #include "EfhIseProps.h" */
/* #include "EfhMiaxProps.h" */
/* #include "EfhNomProps.h" */
/* #include "EfhPhlxOrdProps.h" */
/* #include "EfhPhlxTopoProps.h" */
/* #include "EfhXdpProps.h" */
/* #include "EfhCmeProps.h" */
/* #include "EfhBxProps.h" */

#include "EfhTestFuncs.h"

extern TestCtx *testCtx;

/* ------------------------------------------------------------
 */
static std::string eka_get_date() {
  const char *months[] = {"JAN", "FEB", "MAR", "APR",
                          "MAY", "JUN", "JUL", "AUG",
                          "SEP", "OCT", "NOV", "DEC"};
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str, "%d-%s-%02d", 1900 + tm.tm_year,
          months[tm.tm_mon], tm.tm_mday);
  return std::string(t_str);
}

static std::string eka_get_time() {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min,
          tm.tm_sec);
  return std::string(t_str);
}

static void print_usage(char *cmd) {
  printf("USAGE: %s -g <RunGroup> <flags> \n", cmd);
  printf("\tRunGroup Format: \"[coreId]:[Feed Code]:[First "
         "MC Gr ID]..[Last MC Gr ID]\"\n");
  printf("\tRunGroup Format Example: \"2:CC:0..11\"\n");
  printf("\t\tSupported Feed Codes:\n");
  printf("\t\t\tNA  - NOM         A feed\n");
  printf("\t\t\tNB  - NOM         B feed\n");
  printf("\t\t\tBXA - BX          A feed\n");
  printf("\t\t\tBXB - BX          B feed\n");
  printf("\t\t\tGA  - GEM         A feed\n");
  printf("\t\t\tGB  - GEM         B feed\n");
  printf("\t\t\tIA  - ISE         A feed\n");
  printf("\t\t\tIB  - ISE         B feed\n");
  printf("\t\t\tTA  - PHLX TOPO   A feed\n");
  printf("\t\t\tTB  - PHLX TOPO   B feed\n");
  printf("\t\t\tOA  - PHLX ORD    A feed\n");
  printf("\t\t\tOB  - PHLX ORD    B feed\n");
  printf("\t\t\tCA  - C1          A feed\n");
  printf("\t\t\tCB  - C1          B feed\n");
  printf("\t\t\tCC  - C1          C feed -- main feed for "
         "EFH CBOE vanilla testing\n");
  printf("\t\t\tCD  - C1          D feed\n");
  printf("\t\t\tCAC - C1        CAC feed -- main feed for "
         "EFH CBOE Complex testing\n");
  printf("\t\t\tMA  - MIAX TOM    A feed\n");
  printf("\t\t\tMB  - MIAX TOM    B feed\n");
  printf("\t\t\tPA  - PEARL TOM   A feed\n");
  printf("\t\t\tPB  - PEARL TOM   B feed\n");
  printf("\t\t\tRA  - ARCA XDP    A feed\n");
  printf("\t\t\tRB  - ARCA XDP    B feed\n");
  printf("\t\t\tXA  - AMEX XDP    A feed\n");
  printf("\t\t\tXB  - AMEX XDP    B feed\n");
  printf("\t\t\tRPA - ARCA PLR    A feed\n");
  printf("\t\t\tRPB - ARCA PLR    B feed\n");
  printf("\t\t\tXPA - AMEX PLR    A feed\n");
  printf("\t\t\tXPB - AMEX PLR    B feed\n");
  printf("\t\t\tBA  - BOX vanilla A feed\n");
  printf("\t\t\tBB  - BOX vanilla B feed\n");
  printf("\t\t\tBC  - BOX RFQ(PIP)A feed\n");
  printf("\t\t\tBD  - BOX Complex A feed\n");
  printf("\t\t\tEA  - CME         A feed\n");
  printf("\t\t\tEB  - CME         B feed\n");
  printf("\t\t\tMRA - MRX         A feed\n");
  printf("\t\t\tMRB - MRX         B feed\n");
  printf("\t-u <Underlying Name> - subscribe on all "
         "options belonging to\n");
  printf("\t-t Print TOB updates (EFH)\n");
  printf("\t-a subscribe all\n");
  printf("\t-v verbose statistics\n");
  return;
}

static int getAttr(int argc, char *argv[],
                   std::vector<TestRunGroup> &testRunGroups,
                   std::vector<std::string> &underlyings,
                   bool *print_tob_updates,
                   bool *subscribe_all,
                   bool *print_parsed_messages,
                   bool *verbose_statistics) {
  int opt;
  while ((opt = getopt(argc, argv, ":u:g:htavp")) != -1) {
    switch (opt) {
    case 't':
      printf("Print TOB Updates (EFH)\n");
      *print_tob_updates = true;
      break;
    case 'a':
      printf("subscribe all\n");
      *subscribe_all = true;
      break;
    case 'u':
      underlyings.push_back(std::string(optarg));
      printf("Underlying to subscribe: %s\n", optarg);
      break;
    case 'p':
      *print_parsed_messages = true;
      printf("print_parsed_messages = true\n");
      break;
    case 'v':
      *verbose_statistics = true;
      printf("verbose_statistics = true\n");
      break;
    case 'h':
      print_usage(argv[0]);
      exit(1);
      break;
    case 'g': {
      TestRunGroup newTestRunGroup = {};
      newTestRunGroup.optArgStr = std::string(optarg);
      testRunGroups.push_back(newTestRunGroup);
      break;
    }
    case ':':
      printf("option needs a value\n");
      break;
    case '?':
      printf("unknown option: %c\n", optopt);
      break;
    }
  }
  return 0;
}

static void INThandler(int sig) {
  signal(sig, SIG_IGN);
  if (testCtx)
    testCtx->keep_work = false;
  fprintf(stderr, "%s: Ctrl-C detected:  exitting...\n",
          __func__);
  return;
}

static int createThread(const char *name,
                        EkaServiceType type,
                        void *(*threadRoutine)(void *),
                        void *arg, void *context,
                        uintptr_t *handle) {
  pthread_create((pthread_t *)handle, NULL, threadRoutine,
                 arg);
  pthread_setname_np((pthread_t)*handle, name);
  return 0;
}

static int credAcquire(EkaCredentialType credType,
                       EkaGroup group, const char *user,
                       size_t userLength,
                       const struct timespec *leaseTime,
                       const struct timespec *timeout,
                       void *context,
                       EkaCredentialLease **lease) {
  printf(
      "Credential with USER %.*s is acquired for %s:%hhu\n",
      (int)userLength, user, EKA_EXCH_DECODE(group.source),
      group.localId);
  return 0;
}

static int credRelease(EkaCredentialLease *lease,
                       void *context) {
  return 0;
}

static int getTradeTimeCb(const EfhDateComponents *,
                          uint32_t *iso8601Date,
                          time_t *epochTime, void *ctx) {
  return 0;
}

static void *onOrder(const EfhOrderMsg *msg,
                     EfhSecUserData secData,
                     EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;

  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI grId = msg->header.group.localId;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  int secIdx = (int)secData;
#endif

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];

  if (pEfhCtx->printQStatistics &&
      (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,
                           (EkaGroup *)&msg->header.group,
                           testCtx->verbose_statistics);
  }

  if (!testCtx->print_tob_updates)
    return NULL;

  fprintf(gr->MD,
          "%s,%s,%ju,"
#ifndef EKA_TEST_IGNORE_DEFINITIONS
          "%s,%s,"
#endif
          "%.*f,%u,%c,%s,%ju\n",
          eka_get_date().c_str(), eka_get_time().c_str(),
          msg->header.securityId,
#ifndef EKA_TEST_IGNORE_DEFINITIONS
          gr->security.at(secIdx).classSymbol.c_str(),
          gr->security.at(secIdx).avtSecName.c_str(),
#endif
          //	  EKA_DEC_POINTS_10000(msg->bookSide.price),
          //((float) msg->bookSide.price / 10000),
          decPoints(msg->bookSide.price, 10000),
          ((float)msg->bookSide.price / 10000),
          msg->bookSide.size,

          EKA_TS_DECODE(msg->tradeStatus),

          (ts_ns2str(msg->header.timeStamp)).c_str(),
          msg->header.timeStamp);

  return NULL;
}

static void *onTrade(const EfhTradeMsg *msg,
                     EfhSecUserData secData,
                     EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;

  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI grId = msg->header.group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  //  auto efhGr =
  //  pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];

  std::string currAvtSecName = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  //  int64_t priceScaleFactor = exch == EkaSource::kCME_SBE
  //  ? CME_DEFAULT_DISPLAY_PRICE_SCALE :
  //  DEFAULT_DISPLAY_PRICE_SCALE;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  if (!gr->security.empty()) {
    int secIdx = (int)secData;
    currAvtSecName =
        gr->security.at(secIdx).avtSecName.c_str();
    currClassSymbol = gr->security.at(secIdx).classSymbol;
    //  int64_t priceScaleFactor    = 100;
    //  //gr->security.at(secIdx).displayPriceScale;
  }
#endif

  if (!testCtx->print_tob_updates)
    return NULL;

  fprintf(gr->MD, "Trade,");
  fprintf(gr->MD, "%s,", eka_get_date().c_str());
  fprintf(gr->MD, "%s,", eka_get_time().c_str());
  fprintf(gr->MD, "%ju,", msg->header.securityId);
  fprintf(gr->MD, "%s,", currAvtSecName.c_str());
  fprintf(gr->MD, "%s,", currClassSymbol.c_str());
  fprintf(gr->MD, "%ld,", msg->price);
  fprintf(gr->MD, "%u,", msg->size);
  fprintf(gr->MD, "%d,", (int)msg->tradeCond);
  fprintf(gr->MD, "%s,",
          ts_ns2str(msg->header.timeStamp).c_str());
  fprintf(gr->MD, "%ju,", msg->header.timeStamp);
  fprintf(gr->MD, "\n");

  return NULL;
}

static void *
onEfhGroupStateChange(const EfhGroupStateChangedMsg *msg,
                      EfhSecUserData secData,
                      EfhRunUserData userData) {
  //  int file_idx = (uint8_t)(msg->group.localId);
  //  EfhCtx* pEfhCtx = (EfhCtx*) userData;

#ifdef FH_LAB
  FILE *logFile = stdout;
#else
  EkaSource exch = msg->group.source;
  EkaLSI grId = msg->group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);
  if (gr->MD == NULL)
    on_error("gr->MD == NULL");

  FILE *logFile = gr->MD;
#endif

  fprintf(logFile, "%s: %s : FeedDown\n",
          EKA_PRINT_GRP(&msg->group),
          eka_get_time().c_str());

  switch (msg->groupState) {
    /* ----------------------------- */
  case EfhGroupState::kError:
    switch (msg->errorDomain) {
    case EfhErrorDomain::kUpdateTimeout:
      printf("=========================\n%s: "
             "MdTimeOut\n=========================\n",
             EKA_PRINT_GRP(&msg->group));
      break;

    case EfhErrorDomain::kExchangeError:
      printf("=========================\n%s: "
             "ExchangeError\n=========================\n",
             EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt ==
          testCtx->MaxFatalErrors)
        on_error("testCtx->MaxFatalErrors %d reached",
                 testCtx->MaxFatalErrors);
      break;

    case EfhErrorDomain::kSocketError:
      printf("=========================\n%s: "
             "SocketError\n=========================\n",
             EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt ==
          testCtx->MaxFatalErrors)
        on_error("testCtx->MaxFatalErrors %d reached",
                 testCtx->MaxFatalErrors);
      break;

    case EfhErrorDomain::kCredentialError:
      printf("=========================\n%s: "
             "CredentialError\n=========================\n",
             EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt ==
          testCtx->MaxFatalErrors)
        on_error("testCtx->MaxFatalErrors %d reached",
                 testCtx->MaxFatalErrors);

      break;

    case EfhErrorDomain::kOSError:
      printf("=========================\n%s: "
             "OSError\n=========================\n",
             EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt ==
          testCtx->MaxFatalErrors)
        on_error("testCtx->MaxFatalErrors %d reached",
                 testCtx->MaxFatalErrors);

      break;

    case EfhErrorDomain::kDeviceError:
      printf("=========================\n%s: "
             "DeviceError\n=========================\n",
             EKA_PRINT_GRP(&msg->group));
      if (++testCtx->fatalErrorCnt ==
          testCtx->MaxFatalErrors)
        on_error("testCtx->MaxFatalErrors %d reached",
                 testCtx->MaxFatalErrors);

      break;

    default:
      printf("=========================\n%s: errorDomain = "
             "\'%c\'\n=========================\n",
             EKA_PRINT_GRP(&msg->group),
             (char)msg->errorDomain);
    }

    break;
    /* ----------------------------- */

  case EfhGroupState::kStaleData: {
    std::string gapType = std::string("StaleData");
    fprintf(logFile, "%s: %s : %s FeedDown\n",
            EKA_PRINT_GRP(&msg->group),
            eka_get_time().c_str(), gapType.c_str());
    printf("=========================\n"
           "%s: %s: %s %ju\n"
           "=========================\n",
           EKA_PRINT_GRP(&msg->group),
           eka_get_time().c_str(), gapType.c_str(),
           msg->code);
  } break;
  case EfhGroupState::kInitializing:
  case EfhGroupState::kClosed:
  case EfhGroupState::kGap: {
    std::string gapType = std::string("Unknown");
    switch (msg->systemState) {
    case EfhSystemState::kInitial:
      gapType = std::string("InitialGap");
      break;
    case EfhSystemState::kTrading:
      gapType = std::string("NormalGap");
      break;
    case EfhSystemState::kClosed:
      gapType = std::string("System Closed");
      break;
    default:
      gapType = std::string("Unknown Gap");
    }
    fprintf(logFile, "%s: %s : %s FeedDown\n",
            EKA_PRINT_GRP(&msg->group),
            eka_get_time().c_str(), gapType.c_str());
    printf("=========================\n%s: %s: %s "
           "%ju\n=========================\n",
           EKA_PRINT_GRP(&msg->group),
           eka_get_time().c_str(), gapType.c_str(),
           msg->code);
  } break;
    /* ----------------------------- */
  case EfhGroupState::kProgressing: {
    std::string gapType = std::string("Progressing");
    fprintf(logFile, "%s: %s : %s \n",
            EKA_PRINT_GRP(&msg->group),
            eka_get_time().c_str(), gapType.c_str());
    printf("=========================\n%s: %s: %s "
           "%ju\n=========================\n",
           EKA_PRINT_GRP(&msg->group),
           eka_get_time().c_str(), gapType.c_str(),
           msg->code);
  } break;
    /* ----------------------------- */
  case EfhGroupState::kNormal: {
    std::string gapType = std::string("Unknown");
    switch (msg->systemState) {
    case EfhSystemState::kInitial:
      gapType = std::string("InitialFeedUp");
      break;
    case EfhSystemState::kTrading:
      gapType = std::string("NormalFeedUp");
      break;
    default:
      gapType = std::string("Unknown Gap");
    }
    fprintf(logFile, "%s: %s : %s \n",
            EKA_PRINT_GRP(&msg->group),
            eka_get_time().c_str(), gapType.c_str());
    printf("=========================\n%s: %s: %s "
           "%ju\n=========================\n",
           EKA_PRINT_GRP(&msg->group),
           eka_get_time().c_str(), gapType.c_str(),
           msg->code);
  } break;
    /* ----------------------------- */
  case EfhGroupState::kWarning: {
    fprintf(logFile, "%s: %s : BACK-IN-TIME \n",
            EKA_PRINT_GRP(&msg->group),
            eka_get_time().c_str());
    printf("=========================\n%s: %s: "
           "BACK-IN-TIME %ju\n=========================\n",
           EKA_PRINT_GRP(&msg->group),
           eka_get_time().c_str(), msg->code);
  } break;
    /* ----------------------------- */
  default:
    on_error("Unexpected EfhGroupState \'%c\'",
             (char)msg->groupState);
  }
  return NULL;
}

static void onException(EkaExceptionReport *msg,
                        EfhRunUserData efhRunUserData) {
  if (!testCtx->keep_work)
    return;
  printf("%s: Doing nothing\n", __func__);
  return;
}

/* void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport*
 * fire_report_buf, size_t size) { */
static void onFireReport(const void *fire_report_buf,
                         size_t size) {
  if (!testCtx->keep_work)
    return;
  printf("%s: Doing nothing \n", __func__);
  return;
}
extern FILE *mdFile;

static void *onMd(const EfhMdHeader *msg,
                  EfhRunUserData efhRunUserData) {
  if (!testCtx->keep_work)
    return NULL;
  EfhCtx *pEfhCtx = (EfhCtx *)efhRunUserData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

#ifdef FH_LAB
  FILE *logFile = (FILE *)efhRunUserData; // stdout;
#else
  EkaSource exch = msg->group.source;
  EkaLSI grId = msg->group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);
  if (gr->MD == NULL)
    on_error("gr->MD == NULL");

  FILE *logFile = gr->MD;
#endif

  if (logFile == NULL)
    on_error("logFile == NULL");
  fprintf(logFile,
          "%ju,"    /* sequence       */
          "%s,"     /* ts string      */
          "0x%jx,", /* ts hex         */
          msg->sequenceNumber,
          ts_ns2str(msg->timeStamp).c_str(),
          msg->timeStamp);

  switch (msg->mdMsgType) {
  case EfhMdType::kNewOrder: {
    auto m{reinterpret_cast<const MdNewOrder *>(msg)};
    const char *c = (const char *)&m->hdr.securityId;
    fprintf(logFile,
            "%s (0x%x),"            /* msg type       */
            "\'%c%c%c%c%c%c%c%c\'," /* secId string   */
            "0x%016jx,"             /* secId hex      */
            "0x%jx,"                /* orderId        */
            "\'%c\',"               /* side           */
            "P:%ju (0x%016jx),"     /* price          */
            "S:%u (0x%08x)\n",      /* size           */
            DecodeMdType(m->hdr.mdMsgType),
            m->hdr.mdRawMsgType, c[0], c[1], c[2], c[3],
            c[4], c[5], c[6], c[7],
            //	    std::string(*(char*)&m->hdr.securityId,sizeof(m->hdr.securityId)).c_str(),
            m->hdr.securityId, m->orderId,
            m->side == EfhOrderSide::kBid ? 'B' : 'A',
            m->price, m->price, m->size, m->size);
    //    fflush(logFile);
  } break;

  case EfhMdType::kNewPlevel: {
    auto m{reinterpret_cast<const MdNewPlevel *>(msg)};
    fprintf(logFile,
            "%s (0x%x),0x%016jx "
            "(%ju),%ju,%s,%ju,%c,%u,%ju,%u\n",
            DecodeMdType(m->hdr.mdMsgType),
            m->hdr.mdRawMsgType, m->hdr.securityId,
            m->hdr.securityId, m->hdr.sequenceNumber,
            ts_ns2str(m->hdr.timeStamp).c_str(),
            m->hdr.timeStamp,
            m->side == EfhOrderSide::kBid ? 'B' : 'A',
            m->pLvl, m->price, m->size);
  } break;
  case EfhMdType::kChangePlevel: {
    auto m{reinterpret_cast<const MdChangePlevel *>(msg)};
    fprintf(logFile,
            "%s (0x%x),0x%016jx "
            "(%ju),%ju,%s,%ju,%c,%u,%ju,%u\n",
            DecodeMdType(m->hdr.mdMsgType),
            m->hdr.mdRawMsgType, m->hdr.securityId,
            m->hdr.securityId, m->hdr.sequenceNumber,
            ts_ns2str(m->hdr.timeStamp).c_str(),
            m->hdr.timeStamp,
            m->side == EfhOrderSide::kBid ? 'B' : 'A',
            m->pLvl, m->price, m->size);
  } break;
  case EfhMdType::kDeletePlevel: {
    auto m{reinterpret_cast<const MdDeletePlevel *>(msg)};
    fprintf(logFile,
            "%s (0x%x),0x%016jx (%ju),%ju,%s,%ju,%c,%u\n",
            DecodeMdType(m->hdr.mdMsgType),
            m->hdr.mdRawMsgType, m->hdr.securityId,
            m->hdr.securityId, m->hdr.sequenceNumber,
            ts_ns2str(m->hdr.timeStamp).c_str(),
            m->hdr.timeStamp,
            m->side == EfhOrderSide::kBid ? 'B' : 'A',
            m->pLvl);
  } break;
  default:
    return NULL;
  }
  return NULL;
}

static void *onQuote(const EfhQuoteMsg *msg,
                     EfhSecUserData secData,
                     EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;
  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI grId = msg->header.group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];

#ifdef EKA_TEST_IGNORE_DEFINITIONS
  std::string currAvtSecName = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  int64_t priceScaleFactor =
      exch == EkaSource::kCME_SBE
          ? CME_DEFAULT_DISPLAY_PRICE_SCALE
          : DEFAULT_DISPLAY_PRICE_SCALE;
#else
  int secIdx = (int)secData;
  if (secIdx < 0 || secIdx >= (int)gr->security.size()) {
    /* on_error("Bad secIdx = %d, gr->security.size() = %d",
     */
    /* 	     secIdx, (int)gr->security.size()); */
    on_warning("Bad secIdx = %d, gr->security.size() = %d, "
               "msg->header.securityId = %ju",
               secIdx, (int)gr->security.size(),
               msg->header.securityId);
    return NULL;
  }
  std::string currAvtSecName =
      gr->security.at(secIdx).avtSecName.c_str();
  std::string currClassSymbol =
      gr->security.at(secIdx).classSymbol;
  int64_t priceScaleFactor =
      gr->security.at(secIdx).displayPriceScale; // 100
#endif

  if (pEfhCtx->printQStatistics &&
      (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,
                           (EkaGroup *)&msg->header.group,
                           testCtx->verbose_statistics);
  }

  if (!testCtx->print_tob_updates)
    return NULL;

  fprintf(gr->MD,
          "%s,%s,"
          "%ju,"
#ifndef EKA_TEST_IGNORE_DEFINITIONS
          "%s,%s,"
#endif
          "%c,"    // dummy '1'
          "%u,"    // bidSide.size
          "%.*f,"  // bidSide.price
          "%u,"    // bidSide.customerSize
          "%u,"    // msg->askSide.size
          "%.*f,"  // askSide.price
          "%u,"    // askSide.customerSize
          "%c,%c," // tradeStatus
          "%d,%d," // Size Breakdown 0,0
          "%s,%ju" // ts
          "\n",
          eka_get_date().c_str(), eka_get_time().c_str(),
          msg->header.securityId,
#ifndef EKA_TEST_IGNORE_DEFINITIONS
          currAvtSecName.c_str(), currClassSymbol.c_str(),
#endif
          '1', msg->bidSide.size,
          decPoints(msg->bidSide.price, priceScaleFactor),
          ((float)msg->bidSide.price / priceScaleFactor),

          msg->bidSide.customerSize,

          msg->askSide.size,
          decPoints(msg->askSide.price, priceScaleFactor),
          ((float)msg->askSide.price / priceScaleFactor),

          msg->askSide.customerSize,
          EKA_TS_DECODE(msg->tradeStatus),
          EKA_TS_DECODE(msg->tradeStatus), 0,
          0, // Size Breakdown
          (ts_ns2str(msg->header.timeStamp)).c_str(),
          msg->header.timeStamp);

  return NULL;
}

static void *onImbalance(const EfhImbalanceMsg *msg,
                         EfhSecUserData secData,
                         EfhRunUserData userData) {
  return NULL;
}

/* void eka_create_avt_definition (char* dst, const
 * EfhOptionDefinitionMsg* msg) { */
/*   if (msg->header.group.source  == EkaSource::kCME_SBE &&
 * msg->securityType == EfhSecurityType::kOption) { */
/*     std::string classSymbol    =
 * std::string(msg->classSymbol,sizeof(msg->classSymbol));
 */
/*     sprintf(dst,"%s_%c%04jd", */
/* 	    classSymbol.c_str(), */
/* 	    msg->optionType == EfhOptionType::kCall ? 'C' :
 * 'P', */
/* 	    msg->strikePrice); */
/*   } else { */

/*     uint8_t y,m,d; */

/*     d = msg->commonDef.expiryDate % 100; */
/*     m = ((msg->commonDef.expiryDate - d) / 100) % 100; */
/*     y = msg->commonDef.expiryDate / 10000 - 2000; */

/*     memcpy(dst,msg->underlying,6); */
/*     for (auto i = 0; i < 6; i++) if (dst[i] == 0 ||
 * dst[i] == ' ') dst[i] = '_'; */
/*     char call_put = msg->optionType ==
 * EfhOptionType::kCall ? 'C' : 'P'; */
/*     sprintf(dst+6,"%02u%02u%02u%c%08jd",y,m,d,call_put,msg->strikePrice);
 */
/*   } */
/*   return; */
/* } */
/* ------------------------------------------------------------
 */

static void *
onComplexDefinition(const EfhComplexDefinitionMsg *msg,
                    EfhSecUserData secData,
                    EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;
  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI grId = msg->header.group.localId;

  auto gr = testCtx->grCtx[(int)exch][grId];
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  fprintf(gr->MD, "ComplexDefinition,");
  fprintf(gr->MD, "%ju,", msg->header.securityId);
  fprintf(gr->MD, "\'%s\',",
          std::string(msg->commonDef.underlying,
                      sizeof(msg->commonDef.underlying))
              .c_str());
  fprintf(gr->MD, "\'%s\',",
          std::string(msg->commonDef.classSymbol,
                      sizeof(msg->commonDef.classSymbol))
              .c_str());
  fprintf(
      gr->MD, "\'%s\',",
      std::string(msg->commonDef.exchSecurityName,
                  sizeof(msg->commonDef.exchSecurityName))
          .c_str());
  for (auto i = 0; i < msg->numLegs; i++) {
    auto leg{&msg->legs[i]};
    fprintf(gr->MD, "{%ju(0x%jx),", leg->securityId,
            leg->securityId);
    fprintf(gr->MD, "%c,%c,",
            printEfhSecurityType(leg->type),
            printEfhOrderSide(leg->side));
    fprintf(gr->MD, "%d,%d,", leg->ratio, leg->optionDelta);
    fprintf(gr->MD, "%jd},", leg->price);
  }
  fprintf(gr->MD, "\n");

  TestSecurityCtx newSecurity = {
      .securityId = msg->header.securityId,
      .avtSecName = std::string(
          msg->commonDef.exchSecurityName,
          sizeof(msg->commonDef.exchSecurityName)),
      .underlying =
          std::string(msg->commonDef.underlying,
                      sizeof(msg->commonDef.underlying)),
      .classSymbol =
          std::string(msg->commonDef.classSymbol,
                      sizeof(msg->commonDef.classSymbol)),
      .exch = msg->commonDef.exchange,
      .displayPriceScale =
          exch == EkaSource::kCME_SBE
              ? CME_DEFAULT_DISPLAY_PRICE_SCALE
              : DEFAULT_DISPLAY_PRICE_SCALE,
      .isCompllex = true};

  gr->security.push_back(newSecurity);
  auto sec_idx = gr->security.size() - 1;

  efhSubscribeStatic(
      pEfhCtx, (EkaGroup *)&msg->header.group,
      msg->header.securityId, EfhSecurityType::kOption,
      (EfhSecUserData)sec_idx, msg->commonDef.opaqueAttrA,
      msg->commonDef.opaqueAttrB);

  return NULL;
}
/* ------------------------------------------------------------
 */

static void *onAuctionUpdate(const EfhAuctionUpdateMsg *msg,
                             EfhSecUserData secData,
                             EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;
  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI grId = msg->header.group.localId;

  auto gr{testCtx->grCtx[(int)exch][grId]};
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  auto secCtx = gr->getSecurityCtx(msg->header.securityId);

  std::string currAvtSecName = "DEFAULT_AVT_SEC_NAME";
  std::string currClassSymbol = "DEFAULT_UNDERLYING_ID";
  int64_t priceScaleFactor =
      exch == EkaSource::kCME_SBE
          ? CME_DEFAULT_DISPLAY_PRICE_SCALE
          : DEFAULT_DISPLAY_PRICE_SCALE;
  EfhExchange exchName = EfhExchange::kUnknown;

  if (secCtx != nullptr) {
    currAvtSecName = secCtx->avtSecName;
    currClassSymbol = secCtx->classSymbol;
    priceScaleFactor = secCtx->displayPriceScale;
    exchName = secCtx->exch;
  }

  //  RfqTable5,date,time,QuoteID,Name,Security,Price,Size,Capacity,ExpirationTime,Side,Exchange,Type,ActionType,Customer,Imbalance
  //  RfqTable5,20210628,10:48:20.030.380,l6s01552,TSLA,TSLA__210702P00650000,5.5,67,C,,B,B,D,N,0333SF3,0
  fprintf(gr->MD, "RfqTable5,");
  fprintf(gr->MD, "%s,", eka_get_date().c_str());
  fprintf(gr->MD, "%s,", eka_get_time().c_str());
  fprintf(gr->MD, "%s,", msg->auctionId);
  fprintf(gr->MD, "%s,", currClassSymbol.c_str());
  fprintf(gr->MD, "%s,", currAvtSecName.c_str());
  fprintf(gr->MD, "%*.f,",
          decPoints(msg->price, priceScaleFactor),
          ((float)msg->price / priceScaleFactor));
  fprintf(gr->MD, "%u,", msg->quantity);
  fprintf(gr->MD, "%s,",
          ts_ns2str(msg->endTimeNanos).c_str());

  fprintf(gr->MD, "%c,",
          msg->side == EfhOrderSide::kBid ? 'B' : 'S');
  fprintf(gr->MD, "%c,", (char)exchName);

  fprintf(gr->MD, "%c,", (char)msg->auctionType);
  fprintf(gr->MD, "%c,", (char)msg->capacity);
  fprintf(gr->MD, "%d,", (int)msg->securityType);

  fprintf(
      gr->MD, "%s,",
      std::string(msg->firmId, sizeof msg->firmId).c_str());

  fprintf(gr->MD, "%s,",
          (ts_ns2str(msg->header.timeStamp)).c_str());
  fprintf(gr->MD, "\n");

  return NULL;
}
/* ------------------------------------------------------------
 */

static void *
onFutureDefinition(const EfhFutureDefinitionMsg *msg,
                   EfhSecUserData secData,
                   EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;

  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  auto exch{msg->header.group.source};
  auto grId{msg->header.group.localId};

  auto gr{testCtx->grCtx[(int)exch][grId]};
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  std::string underlyingName =
      std::string(msg->commonDef.underlying,
                  sizeof(msg->commonDef.underlying));
  std::string classSymbol =
      std::string(msg->commonDef.classSymbol,
                  sizeof(msg->commonDef.classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(),
               ' ', '\0');
  std::replace(classSymbol.begin(), classSymbol.end(), ' ',
               '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize(strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = "Future";

  //  eka_create_avt_definition(avtSecName,msg);
  fprintf(gr->fullDict, "%s,%ju,%s,%s,%s\n", avtSecName,
          msg->header.securityId, underlyingName.c_str(),
          classSymbol.c_str(),
          EKA_PRINT_GRP(&msg->header.group));

  if (testCtx->subscribe_all ||
      (std::find(testCtx->underlyings.begin(),
                 testCtx->underlyings.end(),
                 underlyingName) !=
       testCtx->underlyings.end())) {
    if (std::find(testCtx->securities.begin(),
                  testCtx->securities.end(),
                  msg->header.securityId) !=
        testCtx->securities.end())
      return NULL;
    testCtx->securities.push_back(msg->header.securityId);

    TestSecurityCtx newSecurity = {
        .securityId = msg->header.securityId,
        .avtSecName = std::string(avtSecName),
        .underlying = underlyingName,
        .classSymbol = classSymbol,
        .exch = msg->commonDef.exchange,
        .displayPriceScale =
            exch == EkaSource::kCME_SBE
                ? CME_DEFAULT_DISPLAY_PRICE_SCALE
                : DEFAULT_DISPLAY_PRICE_SCALE,
        .isCompllex = false};

    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;

    efhSubscribeStatic(
        pEfhCtx, (EkaGroup *)&msg->header.group,
        msg->header.securityId, EfhSecurityType::kOption,
        (EfhSecUserData)sec_idx, msg->commonDef.opaqueAttrA,
        msg->commonDef.opaqueAttrB);

    fprintf(gr->subscrDict, "%s,%ju%s\n", avtSecName,
            msg->header.securityId,
            EKA_PRINT_GRP(&msg->header.group));
  }

  return NULL;
}
/* ------------------------------------------------------------
 */

void *onOptionDefinition(const EfhOptionDefinitionMsg *msg,
                         EfhSecUserData secData,
                         EfhRunUserData userData) {
  if (!testCtx->keep_work)
    return NULL;

  EfhCtx *pEfhCtx = (EfhCtx *)userData;
  if (pEfhCtx == NULL)
    on_error("pEfhCtx == NULL");

  auto exch{msg->header.group.source};
  auto grId{msg->header.group.localId};

  auto gr{testCtx->grCtx[(int)exch][grId]};
  if (gr == NULL)
    on_error("Uninitialized testCtx->grCtx[%d][%d]",
             (int)exch, grId);

  std::string underlyingName =
      std::string(msg->commonDef.underlying,
                  sizeof(msg->commonDef.underlying));
  std::string classSymbol =
      std::string(msg->commonDef.classSymbol,
                  sizeof(msg->commonDef.classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(),
               ' ', '\0');
  std::replace(classSymbol.begin(), classSymbol.end(), ' ',
               '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize(strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = {};

  eka_create_avt_definition(avtSecName, msg);
  fprintf(gr->fullDict, "%s,%ju,%s,%s,%s,%s,%ju,%ju\n",
          avtSecName, msg->header.securityId,
          exch == EkaSource::kC1_PITCH
              ? EKA_PRINT_BATS_SYMBOL(
                    (char *)&msg->commonDef.opaqueAttrA)
              : " ",
          underlyingName.c_str(), classSymbol.c_str(),
          EKA_PRINT_GRP(&msg->header.group),
          msg->commonDef.opaqueAttrA,
          msg->commonDef.opaqueAttrB);

  if (testCtx->subscribe_all ||
      (std::find(testCtx->underlyings.begin(),
                 testCtx->underlyings.end(),
                 underlyingName) !=
       testCtx->underlyings.end())) {
    if (std::find(testCtx->securities.begin(),
                  testCtx->securities.end(),
                  msg->header.securityId) !=
        testCtx->securities.end())
      return NULL;
    testCtx->securities.push_back(msg->header.securityId);

    TestSecurityCtx newSecurity = {
        .securityId = msg->header.securityId,
        .avtSecName = std::string(avtSecName),
        .underlying = underlyingName,
        .classSymbol = classSymbol,
        .exch = msg->commonDef.exchange,
        .displayPriceScale =
            exch == EkaSource::kCME_SBE
                ? CME_DEFAULT_DISPLAY_PRICE_SCALE
                : DEFAULT_DISPLAY_PRICE_SCALE,
        .isCompllex = false};

    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;

    efhSubscribeStatic(
        pEfhCtx, (EkaGroup *)&msg->header.group,
        msg->header.securityId, EfhSecurityType::kOption,
        (EfhSecUserData)sec_idx, msg->commonDef.opaqueAttrA,
        msg->commonDef.opaqueAttrB);

    fprintf(gr->subscrDict, "%s,%ju,%s,%s\n", avtSecName,
            msg->header.securityId,
            exch == EkaSource::kC1_PITCH
                ? EKA_PRINT_BATS_SYMBOL(
                      (char *)&msg->commonDef.opaqueAttrA)
                : " ",
            EKA_PRINT_GRP(&msg->header.group));
  }

  return NULL;
}

static EkaSource feedname2source(std::string feedName) {
  /* -------------------------------------------------------
   */
  if (feedName == std::string("CA"))
    return EkaSource::kC1_PITCH;
  if (feedName == std::string("CB"))
    return EkaSource::kC1_PITCH;
  if (feedName == std::string("CC"))
    return EkaSource::kC1_PITCH;
  if (feedName == std::string("CD"))
    return EkaSource::kC1_PITCH;

  if (feedName == std::string("CAC"))
    return EkaSource::kC1_PITCH;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MA"))
    return EkaSource::kMIAX_TOM;
  if (feedName == std::string("MB"))
    return EkaSource::kMIAX_TOM;

  if (feedName == std::string("PA"))
    return EkaSource::kPEARL_TOM;
  if (feedName == std::string("PB"))
    return EkaSource::kPEARL_TOM;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RA"))
    return EkaSource::kARCA_XDP;
  if (feedName == std::string("RB"))
    return EkaSource::kARCA_XDP;

  if (feedName == std::string("XA"))
    return EkaSource::kAMEX_XDP;
  if (feedName == std::string("XB"))
    return EkaSource::kAMEX_XDP;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RPA"))
    return EkaSource::kARCA_PLR;
  if (feedName == std::string("RPB"))
    return EkaSource::kARCA_PLR;

  if (feedName == std::string("XPA"))
    return EkaSource::kAMEX_PLR;
  if (feedName == std::string("XPB"))
    return EkaSource::kAMEX_PLR;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BA"))
    return EkaSource::kBOX_HSVF;
  if (feedName == std::string("BB"))
    return EkaSource::kBOX_HSVF;
  if (feedName == std::string("BC"))
    return EkaSource::kBOX_HSVF;
  if (feedName == std::string("BD"))
    return EkaSource::kBOX_HSVF;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("NA"))
    return EkaSource::kNOM_ITTO;
  if (feedName == std::string("NB"))
    return EkaSource::kNOM_ITTO;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BXA"))
    return EkaSource::kBX_DPTH;
  if (feedName == std::string("BXB"))
    return EkaSource::kBX_DPTH;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MRA"))
    return EkaSource::kMRX2_TOP;
  if (feedName == std::string("MRB"))
    return EkaSource::kMRX2_TOP;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("GA"))
    return EkaSource::kGEM_TQF;
  if (feedName == std::string("GB"))
    return EkaSource::kGEM_TQF;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("IA"))
    return EkaSource::kISE_TQF;
  if (feedName == std::string("IB"))
    return EkaSource::kISE_TQF;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("TA"))
    return EkaSource::kPHLX_TOPO;
  if (feedName == std::string("TB"))
    return EkaSource::kPHLX_TOPO;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("OA"))
    return EkaSource::kPHLX_ORD;
  if (feedName == std::string("OB"))
    return EkaSource::kPHLX_ORD;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("EA"))
    return EkaSource::kCME_SBE;
  if (feedName == std::string("EB"))
    return EkaSource::kCME_SBE;
  /* -------------------------------------------------------
   */
  on_error("Unsupported feed name \"%s\"",
           feedName.c_str());
}

static EkaProp *feedname2prop(std::string feedName) {
  /* -------------------------------------------------------
   */
  if (feedName == std::string("CA"))
    return efhBatsC1InitCtxEntries_A;
  if (feedName == std::string("CB"))
    return efhBatsC1InitCtxEntries_B;
  if (feedName == std::string("CC"))
    return efhBatsC1InitCtxEntries_C;
  if (feedName == std::string("CD"))
    return efhBatsC1InitCtxEntries_D;

  if (feedName == std::string("CAC"))
    return efhBatsC1InitCtxEntries_CAC;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MA"))
    return efhMiaxInitCtxEntries_A;
  if (feedName == std::string("MB"))
    return efhMiaxInitCtxEntries_B;

  if (feedName == std::string("PA"))
    return efhPearlInitCtxEntries_A;
  if (feedName == std::string("PB"))
    return efhPearlInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RA"))
    return efhArcaInitCtxEntries_A;
  if (feedName == std::string("RB"))
    return efhArcaInitCtxEntries_B;

  if (feedName == std::string("XA"))
    return efhAmexInitCtxEntries_A;
  if (feedName == std::string("XB"))
    return efhAmexInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RPA"))
    return efhArcaPlrInitCtxEntries_A;
  if (feedName == std::string("RPB"))
    return efhArcaPlrInitCtxEntries_B;

  if (feedName == std::string("XPA"))
    return efhAmexPlrInitCtxEntries_A;
  if (feedName == std::string("XPB"))
    return efhAmexPlrInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BA"))
    return efhBoxInitCtxEntries_A;
  if (feedName == std::string("BB"))
    return efhBoxInitCtxEntries_B;
  if (feedName == std::string("BC"))
    return efhBoxInitCtxEntries_C;
  if (feedName == std::string("BD"))
    return efhBoxInitCtxEntries_D;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("NA"))
    return efhNomInitCtxEntries_A;
  if (feedName == std::string("NB"))
    return efhNomInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BXA"))
    return efhBxInitCtxEntries_A;
  if (feedName == std::string("BXB"))
    return efhBxInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MRA"))
    return efhMrx2TopInitCtxEntries_A;
  if (feedName == std::string("MRB"))
    return efhMrx2TopInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("GA"))
    return efhGemInitCtxEntries_A;
  if (feedName == std::string("GB"))
    return efhGemInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("IA"))
    return efhIseInitCtxEntries_A;
  if (feedName == std::string("IB"))
    return efhIseInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("TA"))
    return efhPhlxTopoInitCtxEntries_A;
  if (feedName == std::string("TB"))
    return efhPhlxTopoInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("OA"))
    return efhPhlxOrdInitCtxEntries_A;
  if (feedName == std::string("OB"))
    return efhPhlxOrdInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  if (feedName == std::string("EA"))
    return efhCmeInitCtxEntries_A;
  if (feedName == std::string("EB"))
    return efhCmeInitCtxEntries_B;
  /* -------------------------------------------------------
   */
  on_error("Unsupported feed name \"%s\"",
           feedName.c_str());
}

static size_t feedname2numProps(std::string feedName) {
  /* -------------------------------------------------------
   */
  if (feedName == std::string("CA"))
    return std::size(efhBatsC1InitCtxEntries_A);
  if (feedName == std::string("CB"))
    return std::size(efhBatsC1InitCtxEntries_B);
  if (feedName == std::string("CC"))
    return std::size(efhBatsC1InitCtxEntries_C);
  if (feedName == std::string("CD"))
    return std::size(efhBatsC1InitCtxEntries_D);

  if (feedName == std::string("CAC"))
    return std::size(efhBatsC1InitCtxEntries_CAC);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MA"))
    return std::size(efhMiaxInitCtxEntries_A);
  if (feedName == std::string("MB"))
    return std::size(efhMiaxInitCtxEntries_B);

  if (feedName == std::string("PA"))
    return std::size(efhPearlInitCtxEntries_A);
  if (feedName == std::string("PB"))
    return std::size(efhPearlInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RA"))
    return std::size(efhArcaInitCtxEntries_A);
  if (feedName == std::string("RB"))
    return std::size(efhArcaInitCtxEntries_B);

  if (feedName == std::string("XA"))
    return std::size(efhAmexInitCtxEntries_A);
  if (feedName == std::string("XB"))
    return std::size(efhAmexInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RPA"))
    return std::size(efhArcaPlrInitCtxEntries_A);
  if (feedName == std::string("RPB"))
    return std::size(efhArcaPlrInitCtxEntries_B);

  if (feedName == std::string("XPA"))
    return std::size(efhAmexPlrInitCtxEntries_A);
  if (feedName == std::string("XPB"))
    return std::size(efhAmexPlrInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BA"))
    return std::size(efhBoxInitCtxEntries_A);
  if (feedName == std::string("BB"))
    return std::size(efhBoxInitCtxEntries_B);
  if (feedName == std::string("BC"))
    return std::size(efhBoxInitCtxEntries_C);
  if (feedName == std::string("BD"))
    return std::size(efhBoxInitCtxEntries_D);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("NA"))
    return std::size(efhNomInitCtxEntries_A);
  if (feedName == std::string("NB"))
    return std::size(efhNomInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BXA"))
    return std::size(efhBxInitCtxEntries_A);
  if (feedName == std::string("BXB"))
    return std::size(efhBxInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MRA"))
    return std::size(efhMrx2TopInitCtxEntries_A);
  if (feedName == std::string("MRB"))
    return std::size(efhMrx2TopInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("GA"))
    return std::size(efhGemInitCtxEntries_A);
  if (feedName == std::string("GB"))
    return std::size(efhGemInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("IA"))
    return std::size(efhIseInitCtxEntries_A);
  if (feedName == std::string("IB"))
    return std::size(efhIseInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("TA"))
    return std::size(efhPhlxTopoInitCtxEntries_A);
  if (feedName == std::string("TB"))
    return std::size(efhPhlxTopoInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("OA"))
    return std::size(efhPhlxOrdInitCtxEntries_A);
  if (feedName == std::string("OB"))
    return std::size(efhPhlxOrdInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("EA"))
    return std::size(efhCmeInitCtxEntries_A);
  if (feedName == std::string("EB"))
    return std::size(efhCmeInitCtxEntries_B);
  /* -------------------------------------------------------
   */
  on_error("Unsupported feed name \"%s\"",
           feedName.c_str());
}

static size_t feedname2numGroups(std::string feedName) {
  /* -------------------------------------------------------
   */
  if (feedName == std::string("CA"))
    return std::size(batsC1Groups);
  if (feedName == std::string("CB"))
    return std::size(batsC1Groups);
  if (feedName == std::string("CC"))
    return std::size(batsC1Groups);
  if (feedName == std::string("CD"))
    return std::size(batsC1Groups);

  if (feedName == std::string("CAC"))
    return std::size(batsC1Groups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MA"))
    return std::size(miaxGroups);
  if (feedName == std::string("MB"))
    return std::size(miaxGroups);

  if (feedName == std::string("PA"))
    return std::size(pearlGroups);
  if (feedName == std::string("PB"))
    return std::size(pearlGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RPA"))
    return std::size(arcaPlrGroups);
  if (feedName == std::string("RPB"))
    return std::size(arcaPlrGroups);

  if (feedName == std::string("XPA"))
    return std::size(amexPlrGroups);
  if (feedName == std::string("XPB"))
    return std::size(amexPlrGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("RA"))
    return std::size(arcaGroups);
  if (feedName == std::string("RB"))
    return std::size(arcaGroups);

  if (feedName == std::string("XA"))
    return std::size(amexGroups);
  if (feedName == std::string("XB"))
    return std::size(amexGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BA"))
    return std::size(boxGroups);
  if (feedName == std::string("BB"))
    return std::size(boxGroups);
  if (feedName == std::string("BC"))
    return std::size(boxGroups);
  if (feedName == std::string("BD"))
    return std::size(boxGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("NA"))
    return std::size(nomGroups);
  if (feedName == std::string("NB"))
    return std::size(nomGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("BXA"))
    return std::size(bxGroups);
  if (feedName == std::string("BXB"))
    return std::size(bxGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("MRA"))
    return std::size(mrx2TopGroups);
  if (feedName == std::string("MRB"))
    return std::size(mrx2TopGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("GA"))
    return std::size(gemGroups);
  if (feedName == std::string("GB"))
    return std::size(gemGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("IA"))
    return std::size(iseGroups);
  if (feedName == std::string("IB"))
    return std::size(iseGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("TA"))
    return std::size(phlxTopoGroups);
  if (feedName == std::string("TB"))
    return std::size(phlxTopoGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("OA"))
    return std::size(phlxOrdGroups);
  if (feedName == std::string("OB"))
    return std::size(phlxOrdGroups);
  /* -------------------------------------------------------
   */
  if (feedName == std::string("EA"))
    return std::size(cmeGroups);
  if (feedName == std::string("EB"))
    return std::size(cmeGroups);
  /* -------------------------------------------------------
   */
  on_error("Unsupported feed name \"%s\"",
           feedName.c_str());
}

static int
createCtxts(std::vector<TestRunGroup> &testRunGroups,
            std::vector<EfhInitCtx> &efhInitCtx,
            std::vector<EfhRunCtx> &efhRunCtx) {
  const std::regex rg_regex("([0-9])\\:([A-Z][A-Z][A-Z]*)"
                            "\\:([0-9]+)\\.\\.([0-9]+)");

  for (auto &runGr : testRunGroups) {
    std::smatch base_match;
    if (!std::regex_match(runGr.optArgStr, base_match,
                          rg_regex))
      on_error("%s is not valid format run Gr",
               runGr.optArgStr.c_str());

    auto feedName = base_match[2].str();
    auto exch =
        static_cast<EkaSource>(feedname2source(feedName));
    auto coreId = static_cast<EkaCoreId>(
        std::stoi(base_match[1].str()));
    auto firstGrId =
        static_cast<EkaLSI>(std::stoi(base_match[3].str()));
    auto lastGrId =
        static_cast<EkaLSI>(std::stoi(base_match[4].str()));
    /* -------------------------------------------------------
     */

    EfhInitCtx newEfhInitCtx = {
        .ekaProps = new EkaProps(),
        .numOfGroups = feedname2numGroups(feedName),
        .coreId = coreId,
        .getTradeTime = getTradeTimeCb,
        .recvSoftwareMd = true,
    };
    newEfhInitCtx.ekaProps->numProps =
        feedname2numProps(feedName);
    newEfhInitCtx.ekaProps->props = feedname2prop(feedName);

    efhInitCtx.push_back(newEfhInitCtx);

    /* -------------------------------------------------------
     */
    int i = 0;

    size_t numRunGroups = lastGrId - firstGrId + 1;
    auto pGroups = new EkaGroup[numRunGroups];
    if (pGroups == NULL)
      on_error("pGroups == NULL");

    for (EkaLSI grId = firstGrId; grId <= lastGrId;
         grId++) {
      pGroups[i].source = exch;
      pGroups[i].localId = grId;
      if (testCtx->grCtx[(int)exch][grId] != NULL)
        on_error("testCtx->grCtx[%d][%d] already exists",
                 (int)exch, grId);
      testCtx->grCtx[(int)exch][grId] =
          new McGrpCtx(exch, grId);
      if (testCtx->grCtx[(int)exch][grId] == NULL)
        on_error("failed creating testCtx->grCtx[%d][%d]",
                 (int)exch, grId);
      i++;
    }

    EfhRunCtx newEfhRunCtx = {
        .groups = pGroups,
        .numGroups = numRunGroups,
        .efhRunUserData = 0,
        .onEfhOptionDefinitionMsgCb = onOptionDefinition,
        .onEfhFutureDefinitionMsgCb = onFutureDefinition,
        .onEfhComplexDefinitionMsgCb = onComplexDefinition,
        .onEfhAuctionUpdateMsgCb = onAuctionUpdate,
        .onEfhTradeMsgCb = onTrade,
        .onEfhQuoteMsgCb = onQuote,
        .onEfhImbalanceMsgCb = onImbalance,
        .onEfhOrderMsgCb = onOrder,
        .onEfhGroupStateChangedMsgCb =
            onEfhGroupStateChange,
        .onEkaExceptionReportCb = onException,
        .onEfhMdCb = onMd,
    };
    efhRunCtx.push_back(newEfhRunCtx);

    /* -------------------------------------------------------
     */

    TEST_LOG(
        "RunGroup: %s, coreId=%d, %d..%d, numProps=%jd",
        EKA_EXCH_DECODE(exch), coreId, firstGrId, lastGrId,
        newEfhInitCtx.ekaProps->numProps);
    for (size_t j = 0; j < newEfhInitCtx.ekaProps->numProps;
         j++) {
      if (newEfhInitCtx.ekaProps == NULL)
        on_error("currEfhInitCtx.ekaProps == NULL");
      //      TEST_LOG("%s --
      //      %s",newEfhInitCtx.ekaProps->props[j].szKey,newEfhInitCtx.ekaProps->props[j].szVal);
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  testCtx = new TestCtx;
  if (!testCtx)
    on_error("testCtx == NULL");

  std::vector<TestRunGroup> testRunGroups;
  std::vector<EfhInitCtx> efhInitCtx;
  std::vector<EfhRunCtx> efhRunCtx;
  bool print_parsed_messages = false;

  getAttr(argc, argv, testRunGroups, testCtx->underlyings,
          &testCtx->print_tob_updates,
          &testCtx->subscribe_all, &print_parsed_messages,
          &testCtx->verbose_statistics);

  if (testRunGroups.size() == 0) {
    TEST_LOG("No test groups passed");
    print_usage(argv[0]);
    return 1;
  }

  createCtxts(testRunGroups, efhInitCtx, efhRunCtx);

  /* -------------------------------------------------------
   */
  signal(SIGINT, INThandler);

  /* -------------------------------------------------------
   */
  EkaDev *pEkaDev = NULL;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev,
             (const EkaDevInitCtx *)&ekaDevInitCtx);

  //  std::vector<EfhCtx*>      pEfhCtx[16];
  EfhCtx *pEfhCtx[16] = {};

  /* -------------------------------------------------------
   */

  for (size_t r = 0; r < testRunGroups.size(); r++) {
    try {
      efhInitCtx.at(r).printParsedMessages =
          print_parsed_messages;
      efhInit(&pEfhCtx[r], pEkaDev, &efhInitCtx.at(r));
      if (pEfhCtx[r] == NULL)
        on_error("pEfhCtx[r] == NULL");
      efhRunCtx.at(r).efhRunUserData =
          (EfhRunUserData)pEfhCtx[r];

      if (pEfhCtx[r]->fhId >= 16)
        on_error("pEfhCtx[r]->fhId = %u,pEfhCtx[r]=%p",
                 pEfhCtx[r]->fhId, pEfhCtx[r]);
      if (pEfhCtx[r]->fhId != (uint)r)
        on_error("i=%jd, fhId = %u, pEfhCtx[r]=%p", r,
                 pEfhCtx[r]->fhId, pEfhCtx[r]);

      pEfhCtx[r]->printQStatistics = true;

      /* -------------------------------------------------------
       */
      for (uint8_t i = 0; i < efhRunCtx.at(r).numGroups;
           i++) {
        EkaSource exch = efhRunCtx.at(r).groups[i].source;
        EkaLSI grId = efhRunCtx.at(r).groups[i].localId;
        auto gr = testCtx->grCtx[(int)exch][grId];
        if (gr == NULL)
          on_error("gr == NULL");

        printf("################ Group %u: %s:%u "
               "################\n",
               i, EKA_EXCH_DECODE(exch), grId);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
        printf("Skipping Definitions for "
               "EKA_TEST_IGNORE_DEFINITIONS\n");
#else
        auto rc = efhGetDefs(
            pEfhCtx[r], &efhRunCtx.at(r),
            (EkaGroup *)&efhRunCtx.at(r).groups[i], NULL);
        if (rc != EKA_OPRESULT__OK)
          on_error("efhGetDefs failed with rc=%d", rc);
#endif
      }
      /* -------------------------------------------------------
       */
      if (pEfhCtx[r]->fhId >= 16)
        on_error("pEfhCtx[r]->fhId = %u,pEfhCtx[r]=%p",
                 pEfhCtx[r]->fhId, pEfhCtx[r]);
      std::thread efh_run_thread =
          std::thread(efhRunGroups, pEfhCtx[r],
                      &efhRunCtx.at(r), (void **)NULL);
      efh_run_thread.detach();
      /* -------------------------------------------------------
       */
    } catch (const std::out_of_range &oor) {
      on_error("Out of Range error");
    }
  }

  /* -------------------------------------------------------
   */

  while (testCtx->keep_work)
    usleep(0);

  ekaDevClose(pEkaDev);

  for (auto exch = 0; exch < MAX_EXCH; exch++) {
    for (auto grId = 0; grId < MAX_GROUPS; grId++) {
      auto gr = testCtx->grCtx[exch][grId];
      if (gr == NULL)
        continue;
      fclose(gr->fullDict);
      fclose(gr->subscrDict);
      fclose(gr->MD);
    }
  }
  printf("Exitting normally...\n");

  return 0;
}
