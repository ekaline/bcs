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
#define MAX_GROUPS 36
#define MAX_TEST_THREADS 16
#define SYMBOL_SIZE 32

static volatile bool keep_work = true;
static EfhCtx* pEfhCtx = NULL;

static int fatalErrorCnt = 0;
static const int MaxFatalErrors = 4;
/* static FILE* md[MAX_GROUPS] = {}; */
/* static FILE* full_dict[MAX_GROUPS] = {}; */
/* static FILE* subscr_dict[MAX_GROUPS] = {}; */

static FILE* fullDict;
static FILE* subscrDict;
static FILE* MD;

static bool print_tob_updates = false;
static bool subscribe_all     = false;

static char underlying2subscr[MAX_UNDERLYINGS][SYMBOL_SIZE] = {};
uint valid_underlying2subscr = 0;

struct TestFhCtx {
  char mysecurity[MAX_SECURITIES][SYMBOL_SIZE] = {};
  char myunderlying[MAX_SECURITIES][SYMBOL_SIZE] = {};
  char classSymbol[MAX_SECURITIES][SYMBOL_SIZE] = {};
  uint subscr_cnt;
};
  
static TestFhCtx* testFhCtx[MAX_GROUPS] = {};

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s: Ctrl-C detected:  exitting...\n",__func__);
  fflush(stdout);
  return;
}

/* static inline std::string ts_ns2str(uint64_t ts) { */
/*   char dst[SYMBOL_SIZE] = {}; */
/*   uint ns = ts % 1000; */
/*   uint64_t res = (ts - ns) / 1000; */
/*   uint us = res % 1000; */
/*   res = (res - us) / 1000; */
/*   uint ms = res % 1000; */
/*   res = (res - ms) / 1000; */
/*   uint s = res % 60; */
/*   res = (res - s) / 60; */
/*   uint m = res % 60; */
/*   res = (res - m) / 60; */
/*   uint h = res % 24; */
/*   sprintf (dst,"%02d:%02d:%02d.%03d.%03d.%03d",h,m,s,ms,us,ns); */
/*   return std::string(dst); */
/* } */

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
  fprintf(MD,"%s: %s : FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());

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
    fprintf(MD,"%s: %s : %s FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
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
    fprintf(MD,"%s: %s : %s \n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str(),gapType.c_str());
    printf ("=========================\n%s: %s: %s %ju\n=========================\n",
	    EKA_PRINT_GRP(&msg->group),eka_get_time().c_str(),gapType.c_str(),msg->code);
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
  uint8_t gr_id = (uint8_t)msg->header.group.localId;
  if (pEfhCtx->printQStatistics && (++pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr_id]->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  if (! print_tob_updates) return NULL;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  int file_idx = (uint8_t)(msg->header.group.localId);
#endif

  //  fprintf(md[file_idx],"%s,%s,%s,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%s\n",
  fprintf(MD,"%s,%s,%s,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%d,%d,%s\n",
	  EKA_CTS_SOURCE(msg->header.group.source),
	  eka_get_date().c_str(),
	  eka_get_time().c_str(),
#ifdef EKA_TEST_IGNORE_DEFINITIONS
	  "DEFAULT_SEC_ID",
	  "DEFAULT_UNDERLYING_ID",
#else
	  testFhCtx[file_idx]->mysecurity[(uint)secData],
	  testFhCtx[file_idx]->classSymbol[(uint)secData],
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

/* static void eka_create_avt_definition (char* dst, const EfhOptionDefinitionMsg* msg) { */
/*   uint8_t y,m,d; */

/*   d = msg->expiryDate % 100; */
/*   m = ((msg->expiryDate - d) / 100) % 100; */
/*   y = msg->expiryDate / 10000 - 2000; */

/*   memcpy(dst,msg->underlying,6); */
/*   for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_'; */
/*   char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P'; */
/*   sprintf(dst+6,"%02u%02u%02u%c%08jd",y,m,d,call_put,msg->strikePrice); */
/*   return; */
/* } */

/* ------------------------------------------------------------ */
uint testSubscribeSec(int file_idx,const EfhOptionDefinitionMsg* msg, EfhRunUserData userData, char* avtSecName, char* underlyingName, char* classSymbol) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  uint sec_idx = testFhCtx[file_idx]->subscr_cnt;

  memcpy(testFhCtx[file_idx]->mysecurity[sec_idx]  ,avtSecName,     SYMBOL_SIZE);
  memcpy(testFhCtx[file_idx]->myunderlying[sec_idx],underlyingName, SYMBOL_SIZE);
  memcpy(testFhCtx[file_idx]->classSymbol[sec_idx] ,classSymbol,    SYMBOL_SIZE);

  fprintf (subscrDict,"%s,%ju,%s,%s\n",
	   avtSecName,
	   msg->header.securityId,
	   EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA),
	   EKA_PRINT_GRP(&msg->header.group)
	   );

  efhSubscribeStatic(pEfhCtx, (EkaGroup*) &msg->header.group,  msg->header.securityId, EfhSecurityType::kOption,(EfhSecUserData) sec_idx,0,0);
  testFhCtx[file_idx]->subscr_cnt++;
  return testFhCtx[file_idx]->subscr_cnt;
}
/* ------------------------------------------------------------ */

void* onDefinition(const EfhOptionDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  //  EfhCtx* pEfhCtx = (EfhCtx*) userData;

  char avtSecName[SYMBOL_SIZE] = {};
  eka_create_avt_definition(avtSecName,msg);
  fprintf (fullDict,"%s,%ju,%s,%s\n",
	   avtSecName,
	   msg->header.securityId,
	   EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA),
	   EKA_PRINT_GRP(&msg->header.group)
	   );

  int file_idx = (uint8_t)(msg->header.group.localId);

  if (testFhCtx[file_idx]->subscr_cnt >= MAX_SECURITIES) 
    on_error("Trying to subscibe on %u securities > %u MAX_SECURITIES",testFhCtx[file_idx]->subscr_cnt,MAX_SECURITIES);

  char classSymbol[SYMBOL_SIZE] = {};
  char underlyingName[SYMBOL_SIZE] = {};

  memcpy(underlyingName,msg->commonDef.underlying,sizeof(msg->commonDef.underlying));
  memcpy(classSymbol,msg->commonDef.classSymbol,sizeof(msg->commonDef.classSymbol));
  for (uint i=0; i < SYMBOL_SIZE; i++) {
    if (underlyingName[i] == ' ') underlyingName[i] = '\0';
    if (classSymbol[i]    == ' ') classSymbol[i]    = '\0';
  }

  if (subscribe_all) goto subscr;
  
  for (uint i = 0; i < valid_underlying2subscr; i ++) {
    if (strncmp(underlyingName,underlying2subscr[i],strlen(underlying2subscr[i])) == 0) {
      goto subscr;
    }
  }

  return NULL;

 subscr:
  testSubscribeSec(file_idx,msg,userData,avtSecName,underlyingName,classSymbol);
  return NULL;
}
/* ------------------------------------------------------------ */

void print_usage(char* cmd) {
  printf("USAGE: %s <flags> \n",cmd); 
  printf("\t-F <Feed Code>\n");
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
  printf("\t-c <core ID>         - 10G port ID to use (default 0)\n");
  printf("\t-s run single MC group #0\n");
  printf("\t-t Print TOB updates (EFH)\n");
  printf("\t-a subscribe all\n");

  /* printf("\t-f [File Name]\t\tFile with a list of Underlyings to subscribe to all their Securities on all Feeds(1 name per line)\n"); */
  /* printf("\t-m \t\t\tMeasure Latency (Exch ts --> Sample ts\n"); */
  /* printf("\t-c \t\t\tSet Affinity\n"); */
  return;
}

int main(int argc, char *argv[]) {
  if (argc < 3) { print_usage(argv[0]);	return 1; }

  bool singleGrp   = false;
  EkaCoreId coreId = 0;

  int opt; 
  std::string feedName = std::string("NO FEED");

  while((opt = getopt(argc, argv, ":u:c:F:htsa")) != -1) {  
    switch(opt) {  
      case 's':  
	printf("Running for single Grp#0\n");  
	singleGrp = true;
	break; 
      case 't':  
	printf("Print TOB Updates (EFH)\n");  
	print_tob_updates = true;
	break;  
      case 'a':  
	printf("subscribe all\n");
	subscribe_all = true;
	break;  
      case 'u':  
	strcpy(underlying2subscr[valid_underlying2subscr],optarg);
	printf("Underlying to subscribe: %s\n", underlying2subscr[valid_underlying2subscr]);  
	valid_underlying2subscr++;
	break;  
      case 'c':  
	coreId = atoi(optarg);
	printf("coreId: %d\n", coreId);  
	valid_underlying2subscr++;
	break; 
      case 'F':  
	feedName = std::string(optarg);
	printf("feedName: %s\n", feedName.c_str());  
	break;  
      case 'h':  
	print_usage(argv[0]);
	return 1;
	break;  
      case ':':  
	printf("option needs a value\n");  
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  for (auto i = 0; i < MAX_GROUPS; i++) {
    testFhCtx[i] = new TestFhCtx;
    testFhCtx[i]->subscr_cnt = 0;
  }


  EkaDev* pEkaDev = NULL;

  EkaProps ekaProps = {};
  EfhRunCtx runCtx = {};

/* ------------------------------------------------------- */
  if (feedName == std::string("CA")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_A);
    ekaProps.props    = efhBatsC1InitCtxEntries_A;
    runCtx.numGroups  = std::size(batsC1Groups);
    runCtx.groups     = batsC1Groups;
  } else if (feedName == std::string("CB")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_B);
    ekaProps.props    = efhBatsC1InitCtxEntries_B;
    runCtx.numGroups  = std::size(batsC1Groups);
    runCtx.groups     = batsC1Groups;
  } else if (feedName == std::string("CC")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_C);
    ekaProps.props    = efhBatsC1InitCtxEntries_C;
    runCtx.numGroups  = std::size(batsC1Groups);
    runCtx.groups     = batsC1Groups;
  } else if (feedName == std::string("CD")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_D);
    ekaProps.props    = efhBatsC1InitCtxEntries_D;
    runCtx.numGroups  = std::size(batsC1Groups);
    runCtx.groups     = batsC1Groups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("MA")) {
    ekaProps.numProps = std::size(efhMiaxInitCtxEntries_A);
    ekaProps.props    = efhMiaxInitCtxEntries_A;
    runCtx.numGroups  = std::size(miaxGroups);
    runCtx.groups     = miaxGroups;
  } else if (feedName == std::string("MB")) {
    ekaProps.numProps = std::size(efhMiaxInitCtxEntries_B);
    ekaProps.props    = efhMiaxInitCtxEntries_B;
    runCtx.numGroups  = std::size(miaxGroups);
    runCtx.groups     = miaxGroups;
  } else if (feedName == std::string("PA")) {
    ekaProps.numProps = std::size(efhPearlInitCtxEntries_A);
    ekaProps.props    = efhPearlInitCtxEntries_A;
    runCtx.numGroups  = std::size(pearlGroups);
    runCtx.groups     = pearlGroups;
  } else if (feedName == std::string("PB")) {
    ekaProps.numProps = std::size(efhPearlInitCtxEntries_B);
    ekaProps.props    = efhPearlInitCtxEntries_B;
    runCtx.numGroups  = std::size(pearlGroups);
    runCtx.groups     = pearlGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("RA")) {
    ekaProps.numProps = std::size(efhArcaInitCtxEntries_A);
    ekaProps.props    = efhArcaInitCtxEntries_A;
    runCtx.numGroups  = std::size(arcaGroups);
    runCtx.groups     = arcaGroups;
  } else if (feedName == std::string("RB")) {
    ekaProps.numProps = std::size(efhArcaInitCtxEntries_B);
    ekaProps.props    = efhArcaInitCtxEntries_B;
    runCtx.numGroups  = std::size(arcaGroups);
    runCtx.groups     = arcaGroups;
  } else if (feedName == std::string("XA")) {
    ekaProps.numProps = std::size(efhAmexInitCtxEntries_A);
    ekaProps.props    = efhAmexInitCtxEntries_A;
    runCtx.numGroups  = std::size(amexGroups);
    runCtx.groups     = amexGroups;
  } else if (feedName == std::string("XB")) {
    ekaProps.numProps = std::size(efhAmexInitCtxEntries_B);
    ekaProps.props    = efhAmexInitCtxEntries_B;
    runCtx.numGroups  = std::size(amexGroups);
    runCtx.groups     = amexGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("BA")) {
    ekaProps.numProps = std::size(efhBoxInitCtxEntries_A);
    ekaProps.props    = efhBoxInitCtxEntries_A;
    runCtx.numGroups  = std::size(boxGroups);
    runCtx.groups     = boxGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("BB")) {
    ekaProps.numProps = std::size(efhBoxInitCtxEntries_B);
    ekaProps.props    = efhBoxInitCtxEntries_B;
    runCtx.numGroups  = std::size(boxGroups);
    runCtx.groups     = boxGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("NA")) {
    ekaProps.numProps = std::size(efhNomInitCtxEntries_A);
    ekaProps.props    = efhNomInitCtxEntries_A;
    runCtx.numGroups  = std::size(nomGroups);
    runCtx.groups     = nomGroups;
  } else if (feedName == std::string("NB")) {
    ekaProps.numProps = std::size(efhNomInitCtxEntries_B);
    ekaProps.props    = efhNomInitCtxEntries_B;
    runCtx.numGroups  = std::size(nomGroups);
    runCtx.groups     = nomGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("GA")) {
    ekaProps.numProps = std::size(efhGemInitCtxEntries_A);
    ekaProps.props    = efhGemInitCtxEntries_A;
    runCtx.numGroups  = std::size(gemGroups);
    runCtx.groups     = gemGroups;
  } else if (feedName == std::string("GB")) {
    ekaProps.numProps = std::size(efhGemInitCtxEntries_B);
    ekaProps.props    = efhGemInitCtxEntries_B;
    runCtx.numGroups  = std::size(gemGroups);
    runCtx.groups     = gemGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("IA")) {
    ekaProps.numProps = std::size(efhIseInitCtxEntries_A);
    ekaProps.props    = efhIseInitCtxEntries_A;
    runCtx.numGroups  = std::size(iseGroups);
    runCtx.groups     = iseGroups;
  } else if (feedName == std::string("IB")) {
    ekaProps.numProps = std::size(efhIseInitCtxEntries_B);
    ekaProps.props    = efhIseInitCtxEntries_B;
    runCtx.numGroups  = std::size(iseGroups);
    runCtx.groups     = iseGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("TA")) {
    ekaProps.numProps = std::size(efhPhlxTopoInitCtxEntries_A);
    ekaProps.props    = efhPhlxTopoInitCtxEntries_A;
    runCtx.numGroups  = std::size(phlxTopoGroups);
    runCtx.groups     = phlxTopoGroups;
  } else if (feedName == std::string("TB")) {
    ekaProps.numProps = std::size(efhPhlxTopoInitCtxEntries_B);
    ekaProps.props    = efhPhlxTopoInitCtxEntries_B;
    runCtx.numGroups  = std::size(phlxTopoGroups);
    runCtx.groups     = phlxTopoGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("OA")) {
    ekaProps.numProps = std::size(efhPhlxOrdInitCtxEntries_A);
    ekaProps.props    = efhPhlxOrdInitCtxEntries_A;
    runCtx.numGroups  = std::size(phlxOrdGroups);
    runCtx.groups     = phlxOrdGroups;
  } else if (feedName == std::string("OB")) {
    ekaProps.numProps = std::size(efhPhlxOrdInitCtxEntries_B);
    ekaProps.props    = efhPhlxOrdInitCtxEntries_B;
    runCtx.numGroups  = std::size(phlxOrdGroups);
    runCtx.groups     = phlxOrdGroups;
/* ------------------------------------------------------- */
  } else if (feedName == std::string("EA")) {
    ekaProps.numProps = std::size(efhCmeInitCtxEntries_A);
    ekaProps.props    = efhCmeInitCtxEntries_A;
    runCtx.numGroups  = std::size(cmeGroups);
    runCtx.groups     = cmeGroups;
  } else if (feedName == std::string("EB")) {
    ekaProps.numProps = std::size(efhCmeInitCtxEntries_B);
    ekaProps.props    = efhCmeInitCtxEntries_B;
    runCtx.numGroups  = std::size(cmeGroups);
    runCtx.groups     = cmeGroups;
/* ------------------------------------------------------- */
  } else {
    on_error("Unsupported feed name \"%s\". Supported: CA, CB, CC, CD",feedName.c_str());
  }

  if (singleGrp) runCtx.numGroups = 1;

  const EfhInitCtx efhInitCtx = {
    .ekaProps    = &ekaProps,
    .numOfGroups = runCtx.numGroups,
    .coreId      = coreId,
    .recvSoftwareMd = true,
  };


  signal(SIGINT, INThandler);

  runCtx.onEfhQuoteMsgCb        = onQuote;
  runCtx.onEfhOptionDefinitionMsgCb   = onDefinition;
  runCtx.onEfhOrderMsgCb        = onOrder;
  runCtx.onEfhTradeMsgCb        = onTrade;
  runCtx.onEfhGroupStateChangedMsgCb = onEfhGroupStateChange;
  runCtx.onEkaExceptionReportCb = onException;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);


  efhInit(&pEfhCtx,pEkaDev,&efhInitCtx);
  runCtx.efhRunUserData = (EfhRunUserData) pEfhCtx;

  pEfhCtx->printQStatistics = true;

  EkaSource exch = EFH_GET_SRC(efhInitCtx.ekaProps->props[0].szKey);
  
  std::string fullDictName   = std::string(EKA_EXCH_SOURCE_DECODE(exch)) + std::string("_FULL_DICT.txt");
  std::string subscrDictName = std::string(EKA_EXCH_SOURCE_DECODE(exch)) + std::string("_SUBSCR_DICT.txt");
  std::string mdName         = std::string(EKA_EXCH_SOURCE_DECODE(exch)) + std::string("_MD.txt");

  if ((fullDict   = fopen(fullDictName.c_str(),"w")) == NULL) on_error("Failed to open %s",fullDictName.c_str());
  if ((subscrDict = fopen(subscrDictName.c_str(),"w")) == NULL) on_error("Failed to open %s",subscrDictName.c_str());
  if ((MD = fopen(mdName.c_str(),"w")) == NULL) on_error("Failed to open %s",mdName.c_str());

  for (uint8_t i = 0; i < runCtx.numGroups; i++) {
    printf ("################ Group %u ################\n",i);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    printf ("Skipping Definitions for EKA_TEST_IGNORE_DEFINITIONS\n");
#else
    testFhCtx[i]->subscr_cnt = 0;
    efhGetDefs(pEfhCtx, &runCtx, (EkaGroup*)&runCtx.groups[i], NULL);
#endif
  }

  std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx, &runCtx,(void**)NULL);
  efh_run_thread.detach();

  while (keep_work) usleep(0);

  ekaDevClose(pEkaDev);

  fclose(fullDict);
  fclose(subscrDict);
  fclose(MD);
  printf ("Exitting normally...\n");

  return 0;
}


