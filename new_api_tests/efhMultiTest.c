#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <thread>
#include <assert.h>
#include <vector>
#include <algorithm>


#include "EkaDev.h"
#include "eka_macros.h"
#include "Efh.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "EkaCtxs.h"

#include "EkaFhGroup.h"


#include "EkaFhBatsParser.h"

#include "EfhBatsProps.h"
#include "EfhBoxProps.h"
#include "EfhGemProps.h"
#include "EfhIseProps.h"
#include "EfhMiaxProps.h"
#include "EfhNomProps.h"
#include "EfhPhlxOrdProps.h"
#include "EfhPhlxTopoProps.h"
#include "EfhXdpProps.h"
#include "EfhCmeProps.h"

#define MAX_SECURITIES 600000
#define MAX_UNDERLYINGS 4000
#define MAX_EXCH   32
#define MAX_GROUPS 36
#define MAX_TEST_THREADS 16
#define SYMBOL_SIZE 32

static volatile bool keep_work = true;

static int fatalErrorCnt = 0;
static const int MaxFatalErrors = 4;

std::vector<std::string> underlyings;

struct TestSecurityCtx {
  std::string avtSecName;
  std::string underlying;
  std::string classSymbol;
};

class McGrpCtx {
 public:
  McGrpCtx(EkaSource _exch, EkaLSI _grId) {
    exch = _exch;
    grId = _grId;

    std::string groupName      = std::string(EKA_EXCH_DECODE(exch)) + std::to_string(grId);

    std::string fullDictName   = groupName + std::string("_FULL_DICT.txt");
    std::string subscrDictName = groupName + std::string("_SUBSCR_DICT.txt");
    std::string mdName         = groupName + std::string("_MD.txt");

    fullDict = fopen(fullDictName.c_str(),"w");
    if (fullDict == NULL) on_error("Failed to open %s",fullDictName.c_str());

    subscrDict = fopen(subscrDictName.c_str(),"w");
    if (subscrDict == NULL) on_error("Failed to open %s",subscrDictName.c_str());

    MD = fopen(mdName.c_str(),"w");
    if (MD == NULL) on_error("Failed to open %s",mdName.c_str());

    TEST_LOG("%s:%d TestGrCtx created",EKA_EXCH_DECODE(exch),grId);
  }

  EkaSource exch = static_cast<EkaSource>(-1);
  EkaLSI    grId = static_cast<EkaLSI>(-1);

  FILE* MD         = NULL;
  FILE* fullDict   = NULL;
  FILE* subscrDict = NULL;

  std::vector<TestSecurityCtx> security;
};

static McGrpCtx* grCtx[MAX_EXCH][MAX_GROUPS] = {};

struct TestRunGroup {
  std::string optArgStr;

};

static bool print_tob_updates = false;
static bool subscribe_all     = false;

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s: Ctrl-C detected:  exitting...\n",__func__);
  fflush(stdout);
  return;
}

static inline std::string ts_ns2str(uint64_t ts) {
  char dst[SYMBOL_SIZE] = {};
  uint ns = ts % 1000;
  uint64_t res = (ts - ns) / 1000;
  uint us = res % 1000;
  res = (res - us) / 1000;
  uint ms = res % 1000;
  res = (res - ms) / 1000;
  uint s = res % 60;
  res = (res - s) / 60;
  uint m = res % 60;
  res = (res - m) / 60;
  uint h = res % 24;
  sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns);
  return std::string(dst);
}

static std::string eka_get_date () {
  const char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str,"%d-%s-%02d",1900+tm.tm_year,months[tm.tm_mon],tm.tm_mday);
  return std::string(t_str);
}

static std::string eka_get_time () {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str,"%02d:%02d:%02d",tm.tm_hour,tm.tm_min,tm.tm_sec);
  return std::string(t_str);
}

int createThread(const char* name, EkaServiceType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  return 0;
}

int credAcquire(EkaCredentialType credType, EkaGroup group, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s:%hhu\n",user,EKA_EXCH_DECODE(group.source),group.localId);
  return 0;
}

int credRelease(EkaCredentialLease *lease, void* context) {
  return 0;
}

void* onOrder(const EfhOrderMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  return NULL;
}

void* onTrade(const EfhTradeMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  return NULL;
}

void* onEfhGroupStateChange(const EfhGroupStateChangedMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  //  int file_idx = (uint8_t)(msg->group.localId);
  //  EfhCtx* pEfhCtx = (EfhCtx*) userData;

  EkaSource exch = msg->group.source;
  EkaLSI    grId = msg->group.localId;

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);


  fprintf(gr->MD,"%s: %s : FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());

  switch (msg->groupState) {
    /* ----------------------------- */
  case EfhGroupState::kError :
    switch (msg->errorDomain) {
    case EfhErrorDomain::kUpdateTimeout :
      printf ("=========================\n%s: MdTimeOut\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      break;
      
    case EfhErrorDomain::kExchangeError :
      printf ("=========================\n%s: ExchangeError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);
      break;

    case EfhErrorDomain::kSocketError :
      printf ("=========================\n%s: SocketError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);
      break;

    case EfhErrorDomain::kCredentialError :
      printf ("=========================\n%s: CredentialError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);

      break;

    case EfhErrorDomain::kOSError :
      printf ("=========================\n%s: OSError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);

      break;

    case EfhErrorDomain::kDeviceError :
      printf ("=========================\n%s: DeviceError\n=========================\n",
	      EKA_PRINT_GRP(&msg->group));
      if (++fatalErrorCnt == MaxFatalErrors) on_error("MaxFatalErrors %d reached",MaxFatalErrors);

      break;

    default:
      printf ("=========================\n%s: errorDomain = \'%c\'\n=========================\n",
	      EKA_PRINT_GRP(&msg->group),(char)msg->errorDomain);
    }

    break;
    /* ----------------------------- */

  case EfhGroupState::kInitializing :
  case EfhGroupState::kClosed : 
  case EfhGroupState::kGap : {
    std::string gapType = std::string("Unknown");
    switch (msg->systemState) {
    case EfhSystemState::kInitial :
      gapType = std::string("InitialGap");
      break;
    case EfhSystemState::kTrading :
      gapType = std::string("NormalGap");
      break;
    case EfhSystemState::kClosed :
      gapType = std::string("System Closed");
      break;
    default:
      gapType = std::string("Unknown Gap");
    }
    fprintf(gr->MD,"%s: %s : %s FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
    printf ("=========================\n%s: %s: %s %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),gapType.c_str(),msg->code);
  }
    break;
    /* ----------------------------- */
  case EfhGroupState::kNormal : {
    std::string gapType = std::string("Unknown");
    switch (msg->systemState) {
    case EfhSystemState::kInitial :
      gapType = std::string("InitialFeedUp");
      break;
    case EfhSystemState::kTrading :
      gapType = std::string("NormalFeedUp");
      break;
    default:
      gapType = std::string("Unknown Gap");
    }
    fprintf(gr->MD,"%s: %s : %s \n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
    printf ("=========================\n%s: %s: %s %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),gapType.c_str(),msg->code);
  }
    break;
    /* ----------------------------- */
  case EfhGroupState::kWarning : {
    fprintf(gr->MD,"%s: %s : BACK-IN-TIME \n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());
    printf ("=========================\n%s: %s: BACK-IN-TIME %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),msg->code);
  }
    break;
    /* ----------------------------- */
  default:
    on_error("Unexpected EfhGroupState \'%c\'",(char)msg->groupState);
  }
  return NULL;
}

void onException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData) {
  printf("%s: Doing nothing\n",__func__);
  return;
}

void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size) {
  printf ("%s: Doing nothing \n",__func__);
  return;	 
}

void* onQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  int secIdx = (int)secData;
#endif

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);

  auto efhGr = pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[grId];

  if (pEfhCtx->printQStatistics && (++efhGr->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  if (! print_tob_updates) return NULL;

  fprintf(gr->MD,"%s,%s,%s,%ju,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%d,%d,%s\n",
	  EKA_CTS_SOURCE(msg->header.group.source),
	  eka_get_date().c_str(),
	  eka_get_time().c_str(),
	  msg->header.securityId,
#ifdef EKA_TEST_IGNORE_DEFINITIONS
	  "DEFAULT_UNDERLYING_ID",
#else
	  gr->security.at(secIdx).classSymbol.c_str(),
#endif
	  '1',
	  msg->bidSide.size,
	  EKA_DEC_POINTS_10000(msg->bidSide.price), ((float) msg->bidSide.price / 10000),
	  msg->bidSide.customerSize,
	  msg->askSide.size,
	  EKA_DEC_POINTS_10000(msg->askSide.price), ((float) msg->askSide.price / 10000),
	  msg->askSide.customerSize,
	  EKA_TS_DECODE(msg->tradeStatus),
	  EKA_TS_DECODE(msg->tradeStatus),
	  0,0, // Size Breakdown
	  (ts_ns2str(msg->header.timeStamp)).c_str()
	  );

  return NULL;
}

static void eka_create_avt_definition (char* dst, const EfhDefinitionMsg* msg) {
  uint8_t y,m,d;

  d = msg->expiryDate % 100;
  m = ((msg->expiryDate - d) / 100) % 100;
  y = msg->expiryDate / 10000 - 2000;

  memcpy(dst,msg->underlying,6);
  for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_';
  char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P';
  sprintf(dst+6,"%02u%02u%02u%c%08u",y,m,d,call_put,msg->strikePrice);
  return;
}

/* ------------------------------------------------------------ */

void* onDefinition(const EfhDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (pEfhCtx == NULL) on_error("pEfhCtx == NULL");

  EkaSource exch = msg->header.group.source;
  EkaLSI    grId = msg->header.group.localId;

  auto gr = grCtx[(int)exch][grId];
  if (gr == NULL) on_error("Uninitialized grCtx[%d][%d]",(int)exch,grId);

  std::string underlyingName = std::string(msg->underlying, sizeof(msg->underlying));
  std::string classSymbol    = std::string(msg->classSymbol,sizeof(msg->classSymbol));

  std::replace(underlyingName.begin(), underlyingName.end(), ' ', '\0');
  std::replace(classSymbol.begin(),    classSymbol.end(),    ' ', '\0');
  underlyingName.resize(strlen(underlyingName.c_str()));
  classSymbol.resize   (strlen(classSymbol.c_str()));

  char avtSecName[SYMBOL_SIZE] = {};
  eka_create_avt_definition(avtSecName,msg);
  fprintf (gr->fullDict,"%s,%ju,%s,%s,%s,%s\n",
	   avtSecName,
	   msg->header.securityId,
	   exch == EkaSource::kC1_PITCH ? EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA) : " ",
	   underlyingName.c_str(),
	   classSymbol.c_str(),
	   EKA_PRINT_GRP(&msg->header.group)
	   );


  if (subscribe_all || (std::find(underlyings.begin(), underlyings.end(), underlyingName) != underlyings.end())) {

    TestSecurityCtx newSecurity = {
      .avtSecName  = std::string(avtSecName),
      .underlying  = underlyingName,
      .classSymbol = classSymbol
    };
    
    gr->security.push_back(newSecurity);
    auto sec_idx = gr->security.size() - 1;
    
    efhSubscribeStatic(pEfhCtx, (EkaGroup*)&msg->header.group, msg->header.securityId, EfhSecurityType::kOpt,(EfhSecUserData) sec_idx,0,0);

    fprintf (gr->subscrDict,"%s,%ju,%s,%s\n",
	     avtSecName,
	     msg->header.securityId,
	     exch == EkaSource::kC1_PITCH ? EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA) : " ",
	     EKA_PRINT_GRP(&msg->header.group)
	     );
  }

  return NULL;
}
/* ------------------------------------------------------------ */

void print_usage(char* cmd) {
  printf("USAGE: %s -g <RunGroup> <flags> \n",cmd); 
  printf("\tRunGroup Format: \"[coreId]:[Feed Code]:[First MC Gr ID]..[Last MC Gr ID]\"\n");
  printf("\tRunGroup Format Example: \"2:CC:0..11\"\n");
  printf("\t\tSupported Feed Codes:\n"); 
  printf("\t\t\tNA - NOM       A feed\n"); 
  printf("\t\t\tNB - NOM       B feed\n"); 
  printf("\t\t\tGA - GEM       A feed\n"); 
  printf("\t\t\tGB - GEM       B feed\n"); 
  printf("\t\t\tIA - ISE       A feed\n"); 
  printf("\t\t\tIB - ISE       B feed\n"); 
  printf("\t\t\tTA - PHLX TOPO A feed\n"); 
  printf("\t\t\tTB - PHLX TOPO B feed\n"); 
  printf("\t\t\tOA - PHLX ORD  A feed\n"); 
  printf("\t\t\tOB - PHLX ORD  B feed\n"); 
  printf("\t\t\tCA - C1        A feed\n"); 
  printf("\t\t\tCB - C1        B feed\n"); 
  printf("\t\t\tCC - C1        C feed\n"); 
  printf("\t\t\tCD - C1        D feed\n"); 
  printf("\t\t\tMA - MIAX TOM  A feed\n"); 
  printf("\t\t\tMB - MIAX TOM  B feed\n"); 
  printf("\t\t\tPA - PEARL TOM A feed\n"); 
  printf("\t\t\tPB - PEARL TOM B feed\n"); 
  printf("\t\t\tRA - ARCA      A feed\n"); 
  printf("\t\t\tRB - ARCA      B feed\n"); 
  printf("\t\t\tXA - AMEX      A feed\n"); 
  printf("\t\t\tXB - AMEX      B feed\n"); 
  printf("\t\t\tBA - BOX       A feed\n"); 
  printf("\t\t\tEA - CME       A feed\n"); 
  printf("\t\t\tEB - CME       B feed\n"); 
  printf("\t-u <Underlying Name> - subscribe on all options belonging to\n");
  printf("\t-t Print TOB updates (EFH)\n");
  printf("\t-a subscribe all\n");
  return;
}

static EkaSource feedname2source(std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return EkaSource::kC1_PITCH;
  if (feedName == std::string("CB")) return EkaSource::kC1_PITCH;
  if (feedName == std::string("CC")) return EkaSource::kC1_PITCH;
  if (feedName == std::string("CD")) return EkaSource::kC1_PITCH;
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return EkaSource::kMIAX_TOM;
  if (feedName == std::string("MB")) return EkaSource::kMIAX_TOM;

  if (feedName == std::string("PA")) return EkaSource::kPEARL_TOM;
  if (feedName == std::string("PB")) return EkaSource::kPEARL_TOM;
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return EkaSource::kARCA_XDP;
  if (feedName == std::string("RB")) return EkaSource::kARCA_XDP;

  if (feedName == std::string("XA")) return EkaSource::kAMEX_XDP;
  if (feedName == std::string("XB")) return EkaSource::kAMEX_XDP;
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return EkaSource::kBOX_HSVF;
  if (feedName == std::string("BB")) return EkaSource::kBOX_HSVF;
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return EkaSource::kNOM_ITTO;
  if (feedName == std::string("NB")) return EkaSource::kNOM_ITTO;
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return EkaSource::kGEM_TQF;
  if (feedName == std::string("GB")) return EkaSource::kGEM_TQF;
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return EkaSource::kISE_TQF;
  if (feedName == std::string("IB")) return EkaSource::kISE_TQF;
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return EkaSource::kPHLX_TOPO;
  if (feedName == std::string("TB")) return EkaSource::kPHLX_TOPO;
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return EkaSource::kPHLX_ORD;
  if (feedName == std::string("OB")) return EkaSource::kPHLX_ORD;
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return EkaSource::kCME_SBE;
  if (feedName == std::string("EB")) return EkaSource::kCME_SBE;
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static EkaProp* feedname2prop (std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return efhBatsC1InitCtxEntries_A;
  if (feedName == std::string("CB")) return efhBatsC1InitCtxEntries_B;
  if (feedName == std::string("CC")) return efhBatsC1InitCtxEntries_C;
  if (feedName == std::string("CD")) return efhBatsC1InitCtxEntries_D;
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return efhMiaxInitCtxEntries_A;
  if (feedName == std::string("MB")) return efhMiaxInitCtxEntries_B;

  if (feedName == std::string("PA")) return efhPearlInitCtxEntries_A;
  if (feedName == std::string("PB")) return efhPearlInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return efhArcaInitCtxEntries_A;
  if (feedName == std::string("RB")) return efhArcaInitCtxEntries_B;

  if (feedName == std::string("XA")) return efhAmexInitCtxEntries_A;
  if (feedName == std::string("XB")) return efhAmexInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return efhBoxInitCtxEntries_A;
  if (feedName == std::string("BB")) return efhBoxInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return efhNomInitCtxEntries_A;
  if (feedName == std::string("NB")) return efhNomInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return efhGemInitCtxEntries_A;
  if (feedName == std::string("GB")) return efhGemInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return efhIseInitCtxEntries_A;
  if (feedName == std::string("IB")) return efhIseInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return efhPhlxTopoInitCtxEntries_A;
  if (feedName == std::string("TB")) return efhPhlxTopoInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return efhPhlxOrdInitCtxEntries_A;
  if (feedName == std::string("OB")) return efhPhlxOrdInitCtxEntries_B;
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return efhCmeInitCtxEntries_A;
  if (feedName == std::string("EB")) return efhCmeInitCtxEntries_B;
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static size_t feedname2numProps (std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return std::size(efhBatsC1InitCtxEntries_A);
  if (feedName == std::string("CB")) return std::size(efhBatsC1InitCtxEntries_B);
  if (feedName == std::string("CC")) return std::size(efhBatsC1InitCtxEntries_C);
  if (feedName == std::string("CD")) return std::size(efhBatsC1InitCtxEntries_D);
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return std::size(efhMiaxInitCtxEntries_A);
  if (feedName == std::string("MB")) return std::size(efhMiaxInitCtxEntries_B);

  if (feedName == std::string("PA")) return std::size(efhPearlInitCtxEntries_A);
  if (feedName == std::string("PB")) return std::size(efhPearlInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return std::size(efhArcaInitCtxEntries_A);
  if (feedName == std::string("RB")) return std::size(efhArcaInitCtxEntries_B);

  if (feedName == std::string("XA")) return std::size(efhAmexInitCtxEntries_A);
  if (feedName == std::string("XB")) return std::size(efhAmexInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return std::size(efhBoxInitCtxEntries_A);
  if (feedName == std::string("BB")) return std::size(efhBoxInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return std::size(efhNomInitCtxEntries_A);
  if (feedName == std::string("NB")) return std::size(efhNomInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return std::size(efhGemInitCtxEntries_A);
  if (feedName == std::string("GB")) return std::size(efhGemInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return std::size(efhIseInitCtxEntries_A);
  if (feedName == std::string("IB")) return std::size(efhIseInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return std::size(efhPhlxTopoInitCtxEntries_A);
  if (feedName == std::string("TB")) return std::size(efhPhlxTopoInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return std::size(efhPhlxOrdInitCtxEntries_A);
  if (feedName == std::string("OB")) return std::size(efhPhlxOrdInitCtxEntries_B);
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return std::size(efhCmeInitCtxEntries_A);
  if (feedName == std::string("EB")) return std::size(efhCmeInitCtxEntries_B);
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static size_t feedname2numGroups(std::string feedName) {
  /* ------------------------------------------------------- */
  if (feedName == std::string("CA")) return std::size(batsC1Groups);
  if (feedName == std::string("CB")) return std::size(batsC1Groups);
  if (feedName == std::string("CC")) return std::size(batsC1Groups);
  if (feedName == std::string("CD")) return std::size(batsC1Groups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("MA")) return std::size(miaxGroups);
  if (feedName == std::string("MB")) return std::size(miaxGroups);

  if (feedName == std::string("PA")) return std::size(pearlGroups);
  if (feedName == std::string("PB")) return std::size(pearlGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("RA")) return std::size(arcaGroups);
  if (feedName == std::string("RB")) return std::size(arcaGroups);

  if (feedName == std::string("XA")) return std::size(amexGroups);
  if (feedName == std::string("XB")) return std::size(amexGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("BA")) return std::size(boxGroups);
  if (feedName == std::string("BB")) return std::size(boxGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("NA")) return std::size(nomGroups);
  if (feedName == std::string("NB")) return std::size(nomGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("GA")) return std::size(gemGroups);
  if (feedName == std::string("GB")) return std::size(gemGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("IA")) return std::size(iseGroups);
  if (feedName == std::string("IB")) return std::size(iseGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("TA")) return std::size(phlxTopoGroups);
  if (feedName == std::string("TB")) return std::size(phlxTopoGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("OA")) return std::size(phlxOrdGroups);
  if (feedName == std::string("OB")) return std::size(phlxOrdGroups);
  /* ------------------------------------------------------- */
  if (feedName == std::string("EA")) return std::size(cmeGroups);
  if (feedName == std::string("EB")) return std::size(cmeGroups);
  /* ------------------------------------------------------- */
  on_error("Unsupported feed name \"%s\"",feedName.c_str());
}

static int getAttr(int argc, char *argv[],
		   std::vector<TestRunGroup>& testRunGroups,
		   std::vector<std::string>&  underlyings,
		   bool *print_tob_updates,
		   bool *subscribe_all,
		   bool *print_parsed_messages) {
  int opt; 
  while((opt = getopt(argc, argv, ":u:g:htap")) != -1) {  
    switch(opt) {  
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
    case 'h':  
      print_usage(argv[0]);
      exit (1);
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

int createCtxts(std::vector<TestRunGroup>& testRunGroups,
		std::vector<EfhInitCtx>&   efhInitCtx,
		std::vector<EfhRunCtx>&    efhRunCtx) {
  const std::regex rg_regex("([0-9])\\:([A-Z][A-Z])\\:([0-9]+)\\.\\.([0-9]+)");

  for (auto &runGr : testRunGroups) {
    std::smatch base_match;
    if (! std::regex_match(runGr.optArgStr, base_match, rg_regex))
      on_error("%s is not valid format run Gr",runGr.optArgStr.c_str());

    auto feedName  = base_match[2].str();
    auto exch      = static_cast<EkaSource>(feedname2source(feedName));
    auto coreId    = static_cast<EkaCoreId>(std::stoi(base_match[1].str()));
    auto firstGrId = static_cast<EkaLSI>   (std::stoi(base_match[3].str()));
    auto lastGrId  = static_cast<EkaLSI>   (std::stoi(base_match[4].str()));
    /* ------------------------------------------------------- */

    EfhInitCtx newEfhInitCtx = {
      .ekaProps       = new EkaProps(),
      .numOfGroups    = feedname2numGroups(feedName),
      .coreId         = coreId,
      .recvSoftwareMd = true,
    };
    newEfhInitCtx.ekaProps->numProps = feedname2numProps(feedName);
    newEfhInitCtx.ekaProps->props    = feedname2prop(feedName);

    efhInitCtx.push_back(newEfhInitCtx);

    /* ------------------------------------------------------- */
    int i = 0;

    size_t numRunGroups = lastGrId - firstGrId + 1;
    auto pGroups = new EkaGroup[numRunGroups];
    if (pGroups == NULL) on_error("pGroups == NULL");

    for (EkaLSI grId = firstGrId; grId <= lastGrId; grId++) {
      pGroups[i].source  = exch;
      pGroups[i].localId = grId;
      if (grCtx[(int)exch][grId] != NULL)
	on_error("grCtx[%d][%d] already exists",(int)exch,grId);
      grCtx[(int)exch][grId] = new McGrpCtx(exch,grId);
      if (grCtx[(int)exch][grId] == NULL)
	on_error("failed creating grCtx[%d][%d]",(int)exch,grId);
      i++;
    }

    EfhRunCtx newEfhRunCtx = {
      .groups                      = pGroups,
      .numGroups                   = numRunGroups,
      .efhRunUserData              = 0,
      .onEfhDefinitionMsgCb        = onDefinition,
      .onEfhTradeMsgCb             = onTrade,
      .onEfhQuoteMsgCb             = onQuote,
      .onEfhOrderMsgCb             = onOrder,
      .onEfhGroupStateChangedMsgCb = onEfhGroupStateChange,
      .onEkaExceptionReportCb      = onException,
    };
    efhRunCtx.push_back(newEfhRunCtx);

    /* ------------------------------------------------------- */

    TEST_LOG("RunGroup: %s, coreId=%d, %d..%d, numProps=%jd",
	     EKA_EXCH_DECODE(exch),
	     coreId,
	     firstGrId,
	     lastGrId,
	     newEfhInitCtx.ekaProps->numProps
	     );
    for (size_t j = 0; j < newEfhInitCtx.ekaProps->numProps; j++) {
      if (newEfhInitCtx.ekaProps == NULL) on_error("currEfhInitCtx.ekaProps == NULL");
      //      TEST_LOG("%s -- %s",newEfhInitCtx.ekaProps->props[j].szKey,newEfhInitCtx.ekaProps->props[j].szVal);
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  std::vector<TestRunGroup> testRunGroups;
  std::vector<EfhInitCtx>   efhInitCtx;
  std::vector<EfhRunCtx>    efhRunCtx;
  bool print_parsed_messages = false;

  getAttr(argc,argv,testRunGroups,underlyings,&print_tob_updates,&subscribe_all,&print_parsed_messages);

  if (testRunGroups.size() == 0) { 
    TEST_LOG("No test groups passed");
    print_usage(argv[0]);	
    return 1; 
  }

  createCtxts(testRunGroups,efhInitCtx,efhRunCtx);

/* ------------------------------------------------------- */
  signal(SIGINT, INThandler);

/* ------------------------------------------------------- */
  EkaDev* pEkaDev = NULL;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  //  std::vector<EfhCtx*>      pEfhCtx[16];
  EfhCtx*      pEfhCtx[16] = {};

/* ------------------------------------------------------- */

  for (size_t r = 0; r < testRunGroups.size(); r++) {
    try {
      efhInitCtx.at(r).printParsedMessages = print_parsed_messages;
      efhInit(&pEfhCtx[r],pEkaDev,&efhInitCtx.at(r));
      if (pEfhCtx[r] == NULL) on_error("pEfhCtx[r] == NULL");
      efhRunCtx.at(r).efhRunUserData = (EfhRunUserData)pEfhCtx[r];

      if (pEfhCtx[r]->fhId >= 16) on_error("pEfhCtx[r]->fhId = %u,pEfhCtx[r]=%p",
					  pEfhCtx[r]->fhId,pEfhCtx[r]);
      if (pEfhCtx[r]->fhId != (uint)r) 
	on_error("i=%jd, fhId = %u, pEfhCtx[r]=%p",r,pEfhCtx[r]->fhId,pEfhCtx[r]);

      pEfhCtx[r]->printQStatistics = true;

      /* ------------------------------------------------------- */
      for (uint8_t i = 0; i < efhRunCtx.at(r).numGroups; i++) {
	EkaSource exch = efhRunCtx.at(r).groups[i].source;
	EkaLSI    grId = efhRunCtx.at(r).groups[i].localId;
	auto gr = grCtx[(int)exch][grId];
	if (gr == NULL) on_error("gr == NULL");

	printf ("################ Group %u: %s:%u ################\n",
		i,EKA_EXCH_DECODE(exch),grId);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
	printf ("Skipping Definitions for EKA_TEST_IGNORE_DEFINITIONS\n");
#else
	efhGetDefs(pEfhCtx[r], &efhRunCtx.at(r), (EkaGroup*)&efhRunCtx.at(r).groups[i], NULL);
#endif
	/* fclose (gr->fullDict); */
	/* fclose (gr->subscrDict); */
      }
      /* ------------------------------------------------------- */
      if (pEfhCtx[r]->fhId >= 16) on_error("pEfhCtx[r]->fhId = %u,pEfhCtx[r]=%p",
					  pEfhCtx[r]->fhId,pEfhCtx[r]);
      /* for (auto k = 0; k < (int)efhRunCtx.at(r).numGroups; k++) { */
      /* 	EkaSource exch = efhRunCtx.at(r).groups[k].source; */
      /* 	EkaLSI    grId = efhRunCtx.at(r).groups[k].localId; */
      /* } */
      std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx[r], &efhRunCtx.at(r),(void**)NULL);
      efh_run_thread.detach();
      /* ------------------------------------------------------- */
      //      sleep(10);
    }
    catch (const std::out_of_range& oor) {
      //      std::cerr << "Out of Range error: " << oor.what() << '\n';
      on_error("Out of Range error");
    }
  }

  /* ------------------------------------------------------- */

  while (keep_work) usleep(0);

  ekaDevClose(pEkaDev);

  for (auto exch = 0; exch < MAX_EXCH; exch++) {
    for (auto grId = 0; grId < MAX_GROUPS; grId++) {
      auto gr = grCtx[exch][grId];
      if (gr == NULL) continue;
      fclose (gr->fullDict);
      fclose (gr->subscrDict);
      fclose(gr->MD);      
    }
  }
  printf ("Exitting normally...\n");

  return 0;
}


