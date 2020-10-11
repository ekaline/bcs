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
#include "eka_fh.h"
#include "eka_macros.h"
#include "Efh.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "EkaCtxs.h"


#include "eka_fh_batspitch_messages.h"

#include "EfhBatsProps.h"

#define MAX_SECURITIES 64000
#define MAX_UNDERLYINGS 8
#define MAX_GROUPS 36
#define MAX_TEST_THREADS 16
#define SYMBOL_SIZE 32

static volatile bool keep_work;
static EfhCtx* pEfhCtx = NULL;

/* static FILE* md[MAX_GROUPS] = {}; */
/* static FILE* full_dict[MAX_GROUPS] = {}; */
/* static FILE* subscr_dict[MAX_GROUPS] = {}; */

static FILE* fullDict;
static FILE* subscrDict;
static FILE* MD;

static bool subscribe_all = false;
static bool print_tob_updates = true;

static char underlying2subscr[MAX_UNDERLYINGS][SYMBOL_SIZE] = {};
uint valid_underlying2subscr = 0;

struct TestFhCtx {
  char mysecurity[MAX_SECURITIES][SYMBOL_SIZE] = {};
  char myunderlying[MAX_SECURITIES][SYMBOL_SIZE] = {};
  char classSymbol[MAX_SECURITIES][SYMBOL_SIZE] = {};
  uint subscr_cnt;
};
  
static TestFhCtx testFhCtx[MAX_GROUPS] = {};

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

static std::string eka_get_time () {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str,"%02d:%02d:%02d",tm.tm_hour,tm.tm_min,tm.tm_sec);
  return std::string(t_str);
}

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
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

void onOrder(const EfhOrderMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  return;
}

void onTrade(const EfhTradeMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  return;
}

void onFeedDown(const EfhFeedDownMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  /* int file_idx = (uint8_t)(msg->group.localId); */
  /* fprintf(md[file_idx],"%s: %s : FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str()); */
  fprintf(MD,"%s: %s : FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());
  printf ("=========================\n%s: %s -- %ju\n=========================\n",__func__,EKA_PRINT_GRP(&msg->group),msg->gapNum);
  return;
}

void onFeedUp(const EfhFeedUpMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  /* int file_idx = (uint8_t)(msg->group.localId); */
  /* fprintf(md[file_idx],"%s: %s : FeedUp\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str()); */
  fprintf(MD,"%s: %s : FeedUp\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());
  printf ("=========================\n%s: %s -- %ju\n=========================\n",__func__,EKA_PRINT_GRP(&msg->group),msg->gapNum);
  return;
}

void onException(EkaExceptionReport* msg, EfhRunUserData efhRunUserData) {
  printf("%s: Doing nothing\n",__func__);
  return;
}

void onFireReport (EfcCtx* pEfcCtx, const EfcFireReport* fire_report_buf, size_t size) {
  printf ("%s: Doing nothing \n",__func__);
  return;	 
}

void onQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  uint8_t gr_id = (uint8_t)msg->header.group.localId;
  if (pEfhCtx->printQStatistics && (++pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr_id]->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  if (! print_tob_updates) return;

#ifndef EKA_TEST_IGNORE_DEFINITIONS
  int file_idx = (uint8_t)(msg->header.group.localId);
#endif

  //  fprintf(md[file_idx],"%s,%s,%s,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%s\n",
  fprintf(MD,"%s,%s,%s,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%d,%d,%s\n",
	  EKA_CTS_SOURCE(msg->header.group.source),
	  "today",
	  eka_get_time().c_str(),
#ifdef EKA_TEST_IGNORE_DEFINITIONS
	  "DEFAULT_SEC_ID",
	  "DEFAULT_UNDERLYING_ID",
#else
	  testFhCtx[file_idx].mysecurity[(uint)secData],
	  testFhCtx[file_idx].classSymbol[(uint)secData],
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

  return;
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
uint testSubscribeSec(int file_idx,const EfhDefinitionMsg* msg, EfhRunUserData userData, char* avtSecName, char* underlyingName, char* classSymbol) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  uint sec_idx = testFhCtx[file_idx].subscr_cnt;

  memcpy(testFhCtx[file_idx].mysecurity[sec_idx]  ,avtSecName,     SYMBOL_SIZE);
  memcpy(testFhCtx[file_idx].myunderlying[sec_idx],underlyingName, SYMBOL_SIZE);
  memcpy(testFhCtx[file_idx].classSymbol[sec_idx] ,classSymbol,    SYMBOL_SIZE);

  fprintf (subscrDict,"%s,%ju,%s,%s\n",
	   avtSecName,
	   msg->header.securityId,
	   EKA_PRINT_BATS_SYMBOL((char*)&msg->opaqueAttrA),
	   EKA_PRINT_GRP(&msg->header.group)
	   );

  efhSubscribeStatic(pEfhCtx, (EkaGroup*) &msg->header.group,  msg->header.securityId, EfhSecurityType::kOpt,(EfhSecUserData) sec_idx,0,0);
  testFhCtx[file_idx].subscr_cnt++;
  return testFhCtx[file_idx].subscr_cnt;
}
/* ------------------------------------------------------------ */

void onDefinition(const EfhDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
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

  if (testFhCtx[file_idx].subscr_cnt >= MAX_SECURITIES) 
    on_error("Trying to subscibe on %u securities > %u MAX_SECURITIES",testFhCtx[file_idx].subscr_cnt,MAX_SECURITIES);

  char classSymbol[SYMBOL_SIZE] = {};
  char underlyingName[SYMBOL_SIZE] = {};

  memcpy(underlyingName,msg->underlying,sizeof(msg->underlying));
  memcpy(classSymbol,msg->classSymbol,sizeof(msg->classSymbol));
  for (uint i=0; i < SYMBOL_SIZE; i++) {
    if (underlyingName[i] == ' ') underlyingName[i] = '\0';
    if (classSymbol[i]    == ' ') classSymbol[i]    = '\0';
  }
  
  for (uint i = 0; i < valid_underlying2subscr; i ++) {
    if (strncmp(underlyingName,underlying2subscr[i],strlen(underlying2subscr[i])) == 0) {
      testSubscribeSec(file_idx,msg,userData,avtSecName,underlyingName,classSymbol);
      return;
    }
  }

  if (subscribe_all) {
    testSubscribeSec(file_idx,msg,userData,avtSecName,underlyingName,classSymbol);
    return;
  }

  return;
}
/* ------------------------------------------------------------ */

void print_usage(char* cmd) {
  printf("USAGE: %s <flags> \n",cmd); 
  printf("\t-F [Feed Code]\n"); 
  printf("\t\t\tCA - C1 A feed\n"); 
  printf("\t\t\tCB - C1 B feed\n"); 
  printf("\t\t\tCC - C1 C feed\n"); 
  printf("\t\t\tCD - C1 D feed\n"); 
  printf("\t-p Print Parsed Messages\n");
  printf("\t-u [Underlying Name]\tSingle Name to subscribe to all its Securities on all Feeds\n");
  printf("\t-a Subscribe ALL (on enabled feeds)\n");
  printf("\t-t Print TOB updates (EFH)\n");

  /* printf("\t-f [File Name]\t\tFile with a list of Underlyings to subscribe to all their Securities on all Feeds(1 name per line)\n"); */
  /* printf("\t-m \t\t\tMeasure Latency (Exch ts --> Sample ts\n"); */
  /* printf("\t-c \t\t\tSet Affinity\n"); */
  return;
}

int main(int argc, char *argv[]) {
  if (argc < 3) { print_usage(argv[0]);	return 1; }

  int opt; 
  std::string underlyings_filename;
  //  bool use_underlyings_filename = false;
  bool print_parsed_messages = false;
  std::string feedName = std::string("NO FEED");

  while((opt = getopt(argc, argv, ":u:f:F:Ahatmcp")) != -1) {  
    switch(opt) {  
      case 'a':  
	printf ("Subscribe ALL\n");
	subscribe_all = true;
	break;
      case 'm':  
	printf ("Measure Latency\n");
	//	measure_latency = true;
	break;
      case 'p':  
	printf("Print Parsed Message\n");  
	print_parsed_messages = true;
	break;  
      case 'c':  
	printf("Set Threads Affinity\n");  
	//	setAffinity = true;
	break;  
      case 't':  
	printf("Print TOB Updates (EFH)\n");  
	print_tob_updates = true;
	break;  
      case 'u':  
	strcpy(underlying2subscr[valid_underlying2subscr],optarg);
	printf("Underlying to subscribe: %s\n", underlying2subscr[valid_underlying2subscr]);  
	valid_underlying2subscr++;
	break;  
      case 'F':  
	feedName = std::string(optarg);
	printf("feedName: %s\n", feedName.c_str());  
	break;  
      case 'f':  
	//	use_underlyings_filename = true;
	underlyings_filename = std::string(optarg);
	printf("filename: %s\n", underlyings_filename.c_str());  
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

  EkaDev* pEkaDev = NULL;

  EkaProps ekaProps = {};
  if (feedName == std::string("CA")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_A);
    ekaProps.props    = efhBatsC1InitCtxEntries_A;
  } else if (feedName == std::string("CB")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_B);
    ekaProps.props    = efhBatsC1InitCtxEntries_B;
  } else if (feedName == std::string("CC")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_C);
    ekaProps.props    = efhBatsC1InitCtxEntries_C;
  } else if (feedName == std::string("CD")) {
    ekaProps.numProps = std::size(efhBatsC1InitCtxEntries_D);
    ekaProps.props    = efhBatsC1InitCtxEntries_D;
  } else {
    on_error("Unsupported feed name \"%s\". Supported: CA, CB, CC, CD",feedName.c_str());
  }

  const EfhInitCtx efhInitCtx = {
    .ekaProps = &ekaProps,
    .numOfGroups = std::size(batsC1Groups),
    .coreId = 0,
    .recvSoftwareMd = true,
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    .subscribe_all = true,
#else
    .subscribe_all = false,
#endif
  };


  keep_work = true;
  signal(SIGINT, INThandler);

  EfhRunCtx runCtx = {};

  runCtx.numGroups = std::size(batsC1Groups);
  runCtx.groups = batsC1Groups;

  EfhRunCtx* pEfhRunCtx = &runCtx;

  pEfhRunCtx->onEfhQuoteMsgCb        = onQuote;
  pEfhRunCtx->onEfhDefinitionMsgCb   = onDefinition;
  pEfhRunCtx->onEfhOrderMsgCb        = onOrder;
  pEfhRunCtx->onEfhTradeMsgCb        = onTrade;
  pEfhRunCtx->onEfhFeedDownMsgCb     = onFeedDown;
  pEfhRunCtx->onEfhFeedUpMsgCb       = onFeedUp;
  pEfhRunCtx->onEkaExceptionReportCb = onException;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  pEkaDev->print_parsed_messages = print_parsed_messages;

  efhInit(&pEfhCtx,pEkaDev,&efhInitCtx);
  runCtx.efhRunUserData = (EfhRunUserData) pEfhCtx;

  pEfhCtx->dontPrintTobUpd = true;
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
    testFhCtx[i].subscr_cnt = 0;
    efhGetDefs(pEfhCtx, &runCtx, (EkaGroup*)&runCtx.groups[i]);
#endif
  }

  std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx, &runCtx);
  efh_run_thread.detach();

  while (keep_work) usleep(0);

  ekaDevClose(pEkaDev);

  fclose(fullDict);
  fclose(subscrDict);
  fclose(MD);
  printf ("Exitting normally...\n");

  return 0;
}


