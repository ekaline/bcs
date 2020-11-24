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
#include <string>
#include <algorithm>
#include <fstream>
#include <pthread.h>
#include <chrono>

#include "Eka.h"
#include "Efh.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "eka_fh.h"

#include "eka_macros.h"

#include "EfhPhlxOrdProps.h"
#include "EfhPhlxTopoProps.h"


#define MAX_SECURITIES 800000
#define MAX_UNDERLYINGS 2048
#define MAX_GROUPS 8
#define MAX_TEST_THREADS 16
#define AFFINITY_CRITICAL_BASE_CORE_ID 8
#define AFFINITY_NON_CRITICAL_BASE_CORE_ID 4
#define AFFINITY_CORE_RANGE 8

#define NOM_GROUPS 4

#define MEASURE_SAMPLE_PERIOD 10000

static volatile bool keep_work;

static FILE* md[MAX_GROUPS] = {};
static FILE* full_dict[MAX_GROUPS] = {};
static FILE* subscr_dict[MAX_GROUPS] = {};

static bool A_side = false;
static bool subscribe_all = false;
static bool print_tob_updates = false;
static bool runFH[256] = {};

static bool insertGaps = false;
static int  gap_period = 0;

static bool measure_latency = false;
static bool setAffinity = false;

enum { SYMBOL_SIZE = 32 };

static char today_str[SYMBOL_SIZE] = {};
static std::string underlying2subscr[MAX_UNDERLYINGS] = {};
uint valid_underlying2subscr = 0;

struct TestFhCtx {
  /* std::string mysecurity[MAX_SECURITIES] = {}; */
  /* std::string myunderlying[MAX_SECURITIES] = {}; */
  char mysecurity[MAX_SECURITIES][SYMBOL_SIZE] = {};
  char myunderlying[MAX_SECURITIES][SYMBOL_SIZE] = {};
  char classSymbol[MAX_SECURITIES][SYMBOL_SIZE] = {};
  uint subscr_cnt;
};
  
static TestFhCtx testFhCtx[MAX_GROUPS] = {};

void print_usage(char* cmd) {
  printf("USAGE: %s <flags> \n",cmd); 
  printf("\t-A \t\t\tUse A-side feed (default is B-side)\n"); 
  printf("\t-N \t\t\tRun NOM\n"); 
  printf("\t-P \t\t\tRun PHLX\n"); 
  printf("\t-G \t\t\tRun GEM\n"); 
  printf("\t-I \t\t\tRun ISE\n"); 
  printf("\t-u [Underlying Name]\tSingle Name to subscribe to all its Securities on all Feeds\n");
  printf("\t-f [File Name]\t\tFile with a list of Underlyings to subscribe to all their Securities on all Feeds(1 name per line)\n");
  printf("\t-m \t\t\tMeasure Latency (Exch ts --> Sample ts\n");
  printf("\t-a \t\t\tSubscribe ALL (on enabled feeds)\n");
  printf("\t-c \t\t\tSet Affinity\n");
  printf("\t-t \t\t\tPrint TOB updates (EFH)\n");
  printf("\t-g [Messages period]\tInsert Gap on GEM#0 every [Messages period]\n");
  //  printf("\t-p \t\t\tPrint Parsed Messages\n");
  return;
}

void  INThandler(int sig) {
  signal(sig, SIG_IGN);
  keep_work = false;
  printf("%s:Ctrl-C detected: keep_work = false\n",__func__);
  printf ("%s: exitting...\n",__func__);
  fflush(stdout);
  return;
}

static std::string eka_get_time () {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char t_str[100] = {};
  sprintf(t_str,"%02d:%02d:%02d",tm.tm_hour,tm.tm_min,tm.tm_sec);
  return std::string(t_str);
}

static int grp2idx (EkaGroup* grp) {
  int ret = -1;
#if 1
  if (grp->source == EkaSource::kNOM_ITTO)  ret =  (uint)grp->localId;
  if (grp->source == EkaSource::kPHLX_TOPO) ret =  4;
  if (grp->source == EkaSource::kGEM_TQF)   ret =  5;
  if (grp->source == EkaSource::kISE_TQF)   ret =  6;
#endif
  /* if (grp->source == EkaSource::kNOM_ITTO)  ret =  0; */
  /* if (grp->source == EkaSource::kPHLX_TOPO) ret =  1; */
  /* if (grp->source == EkaSource::kGEM_TQF)   ret =  2; */
  /* if (grp->source == EkaSource::kISE_TQF)   ret =  3; */
  if ((ret < 0) || (ret > 6)) on_error ("Unsupported %s:%d",EKA_EXCH_DECODE(grp->source),grp->localId);
  return ret;
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

int createThread(const char* name, EkaThreadType type,  void *(*threadRoutine)(void*), void* arg, void* context, uintptr_t *handle) {
  pthread_create ((pthread_t*)handle,NULL,threadRoutine, arg);
  pthread_setname_np((pthread_t)*handle,name);
  if (setAffinity) {
    if (
	type == EkaThreadType::kFeedSnapshot ||
	type == EkaThreadType::kFeedRecovery ||
	type == EkaThreadType::kIGMP         ||
	type == EkaThreadType::kPacketIO     ||
	type == EkaThreadType::kHeartbeat
	) {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      for (auto i = 0; i < AFFINITY_CORE_RANGE; i++) CPU_SET(AFFINITY_NON_CRITICAL_BASE_CORE_ID + i, &cpuset);
      int rc = pthread_setaffinity_np((pthread_t)*handle,sizeof(cpu_set_t), &cpuset);
      if (rc != 0) on_error( "Error calling pthread_setaffinity_np for %s for range %d .. %d",name,AFFINITY_NON_CRITICAL_BASE_CORE_ID,AFFINITY_NON_CRITICAL_BASE_CORE_ID+AFFINITY_CORE_RANGE-1);
    }
  }    
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
  int file_idx = grp2idx((EkaGroup*)&msg->group);
  fprintf(md[file_idx],"%s: %s : FeedDown\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());
  printf ("=========================\n%s: %s -- %ju\n=========================\n",__func__,EKA_PRINT_GRP(&msg->group),msg->gapNum);
  return;
}

void onFeedUp(const EfhFeedUpMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  int file_idx = grp2idx((EkaGroup*)&msg->group);
  fprintf(md[file_idx],"%s: %s : FeedUp\n",EKA_PRINT_GRP(&msg->group), eka_get_time().c_str());
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
  static int upd_cnt = 0;
  static uint gemFhIndex = 0;
  static uint8_t gemGrIndex = 3;

  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  /* uint8_t gr_id = (uint8_t)msg->header.group.localId; */
  /* if (pEfhCtx->printQStatistics && (++pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr_id]->upd_ctr % 1000000 == 0)) { */
  /*   efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group); */
  /* } */

  if (insertGaps && ((uint8_t)msg->header.group.source == (uint8_t)(EkaSource::kGEM_TQF))) {
    if (upd_cnt == gap_period) {
      printf ("Inserting gap to %s:%u\n",EKA_EXCH_DECODE(pEfhCtx->dev->fh[gemFhIndex]->exch),gemGrIndex);
      pEfhCtx->dev->fh[gemFhIndex]->b_gr[gemGrIndex]->expected_sequence -= 400;
      upd_cnt = 0;
    } else {
      upd_cnt++;
    }
  }


  int file_idx = grp2idx((EkaGroup*)&msg->header.group);

  if (measure_latency) {
    if (msg->header.sequenceNumber % MEASURE_SAMPLE_PERIOD == 0) {
      struct timespec monotime;
      clock_gettime(CLOCK_MONOTONIC, &monotime);
      uint64_t now = ((uint64_t)monotime.tv_sec) * (uint64_t)SEC_TO_NANO +  ((uint64_t)monotime.tv_nsec);

      //      uint64_t now = (uint64_t)std::chrono::high_resolution_clock::now();
      fprintf(md[file_idx],"%ju,%ju,%jd,%jd,%jd\n",
	      now, 
	      msg->header.timeStamp, 
	      (int64_t)(now - msg->header.timeStamp), 
	      (int64_t)((now - msg->header.timeStamp)/1000000), 
	      (int64_t)((now - msg->header.timeStamp)%1000000)
	      );
    }
    return;
  }

  if (! print_tob_updates) return;

  /* struct timespec ts; */
  /* timespec_get(&ts, TIME_UTC); */
  /* char buff[100]; */
  /* strftime(buff, sizeof(buff), "%D %T", gmtime(&ts.tv_sec)); */
  /* sprintf(today_str,"%s.%09ld", buff, ts.tv_nsec); */
  
  fprintf(md[file_idx],"%s,%s,%s,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%s\n",
	  EKA_CTS_SOURCE(msg->header.group.source),
	  today_str,
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
	  (ts_ns2str(msg->header.timeStamp)).c_str()
	  );

  return;
}

#if 0
static std::string eka_create_avt_definition (const EfhDefinitionMsg* msg) {
  uint8_t y,m,d;
  char dst[SYMBOL_SIZE] = {};
  d = msg->expiryDate % 100;
  m = ((msg->expiryDate - d) / 100) % 100;
  y = msg->expiryDate / 10000 - 2000;

  memcpy(dst,msg->underlying,6);
  for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_';
  char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P';
  sprintf(dst+6,"%02u%02u%02u%c%08u",y,m,d,call_put,msg->strikePrice / 10);
  return std::string(dst);
}
#endif 

static void eka_create_avt_definition (char* dst, const EfhDefinitionMsg* msg) {
  uint8_t y,m,d;

  d = msg->expiryDate % 100;
  m = ((msg->expiryDate - d) / 100) % 100;
  y = msg->expiryDate / 10000 - 2000;

  memcpy(dst,msg->underlying,6);
  for (auto i = 0; i < 6; i++) if (dst[i] == 0 || dst[i] == ' ') dst[i] = '_';
  char call_put = msg->optionType == EfhOptionType::kCall ? 'C' : 'P';
  sprintf(dst+6,"%02u%02u%02u%c%08u",y,m,d,call_put,(uint)(msg->strikePrice / 10));
  return;
}


void onDefinition(const EfhDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
#ifdef EKA_TEST_IGNORE_DEFINITIONS
  on_warning("Ignoring Definition");
  return;
#endif
  /* std::string msg_underlying = std::string(msg->underlying); */
  /* std::replace(msg_underlying.begin(),msg_underlying.end(),' ','\0'); */

  int file_idx = grp2idx ((EkaGroup*)&msg->header.group);
  uint sec_idx = testFhCtx[file_idx].subscr_cnt;

  //  std::string sec = eka_create_avt_definition(msg);

  eka_create_avt_definition(testFhCtx[file_idx].mysecurity[sec_idx],msg);
  memcpy(testFhCtx[file_idx].myunderlying[sec_idx],msg->underlying,sizeof(msg->underlying));
  memcpy(testFhCtx[file_idx].classSymbol[sec_idx],msg->classSymbol,sizeof(msg->classSymbol));

  for (uint i=0; i < SYMBOL_SIZE; i++) {
    if (testFhCtx[file_idx].myunderlying[sec_idx][i] == ' ') testFhCtx[file_idx].myunderlying[sec_idx][i] = '\0';
    if (testFhCtx[file_idx].classSymbol[sec_idx][i] == ' ') testFhCtx[file_idx].classSymbol[sec_idx][i] = '\0';
  }
  fprintf (full_dict[file_idx],"%s,%ju,%s\n",testFhCtx[file_idx].mysecurity[sec_idx],msg->header.securityId,EKA_PRINT_GRP(&msg->header.group));

  if (! subscribe_all) {
    bool found = false;
    for (uint i = 0; i < valid_underlying2subscr; i ++) {
      if (underlying2subscr[i].compare(0,underlying2subscr[i].size(),std::string(msg->underlying),0,underlying2subscr[i].size()) == 0) {
    	found = true;
    	break;
      }
    }
    if (! found) return;
  }
 

  //  testFhCtx[file_idx].mysecurity[sec_idx] = eka_create_avt_definition(msg);
  //  testFhCtx[file_idx].myunderlying[sec_idx] = msg_underlying;

  if (sec_idx > MAX_SECURITIES) on_error("Trying to subscibe on %u securities > %u MAX_SECURITIES",sec_idx,MAX_SECURITIES);
  fprintf (subscr_dict[file_idx],"%s,%ju\n",testFhCtx[file_idx].mysecurity[sec_idx],msg->header.securityId);
  efhSubscribeStatic(pEfhCtx, (EkaGroup*) &msg->header.group,  msg->header.securityId, EfhSecurityType::kOpt,(EfhSecUserData) sec_idx,0,0);
  testFhCtx[file_idx].subscr_cnt++;
  return;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
  int opt; 
  std::string underlyings_filename;
  bool use_underlyings_filename = false;
  bool print_parsed_messages = false;

  while((opt = getopt(argc, argv, ":u:f:g:AhatNPGImc")) != -1) {  
    switch(opt) {  
      case 'A':  
	printf ("Use A-side feeds\n");
	A_side = true;
	break;
      case 'a':  
	printf ("Subscribe ALL\n");
	subscribe_all = true;
	break;
      case 'm':  
	printf ("Measure Latency\n");
	measure_latency = true;
	break;
      case 'p':  
	printf("Print Parsed Message\n");  
	print_parsed_messages = true;
	break;  
      case 'c':  
	printf("Set Threads Affinity\n");  
	setAffinity = true;
	break;  
      case 't':  
	printf("Print TOB Updates (EFH)\n");  
	print_tob_updates = true;
	break;  
      case 'u':  
	underlying2subscr[valid_underlying2subscr] = std::string(optarg);
	printf("Underlying to subscribe: %s\n", underlying2subscr[valid_underlying2subscr].c_str());  
	valid_underlying2subscr++;
	break;  
      case 'f':  
	use_underlyings_filename = true;
	underlyings_filename = std::string(optarg);
	printf("filename: %s\n", underlyings_filename.c_str());  
	break;  
      case 'g':  
	gap_period = atoi((std::string(optarg)).c_str());
	if (gap_period > 0) insertGaps = true;
	printf("Inserting Gaps on GEM#3 every %d updates\n", gap_period);  
	break;  
      case 'N':  
	printf("Running NOM\n");  
	runFH[(uint8_t)EkaSource::kNOM_ITTO] = true;
	break;  
      case 'P':  
	printf("Running PHLX TOPO\n");  
	runFH[(uint8_t)EkaSource::kPHLX_TOPO] = true;
	break;  
      case 'O':  
	printf("Running PHLX ORD\n");  
	runFH[(uint8_t)EkaSource::kPHLX_ORD] = true;
	break;  
      case 'G':  
	printf("Running GEM\n");  
	runFH[(uint8_t)EkaSource::kGEM_TQF] = true;
	break;  
      case 'I':  
	printf("Running ISE\n");  
	runFH[(uint8_t)EkaSource::kISE_TQF] = true;
	break;  
      case 'h':  
	print_usage(argv[0]);
	break;  
      case ':':  
	printf("option needs a value\n");  
	break;  
      case '?':  
	printf("unknown option: %c\n", optopt); 
      break;  
      }  
  }  

  if (use_underlyings_filename) {
    std::ifstream underlyings_file(underlyings_filename.c_str());
    if (! underlyings_file.is_open()) on_error("Cannot open %s",underlyings_filename.c_str());
    for(std::string line; std::getline(underlyings_file, line);) { 
      underlying2subscr[valid_underlying2subscr++] = line;
    }
    underlyings_file.close();
  }
  for (uint i = 0; i < valid_underlying2subscr; i++) printf ("UNDERLYING2SUBSCR %6u: %s\n",i,underlying2subscr[i].c_str());


  time_t lt = time(NULL);
  struct tm tm = *localtime(&lt);
  sprintf(today_str,"%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

  keep_work = true;
  signal(SIGINT, INThandler);
  EkaDev* pEkaDev = NULL;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInitCtx.credAcquire = credAcquire;
  ekaDevInitCtx.credRelease = credRelease;
  ekaDevInitCtx.createThread = createThread;

  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  EkaProps ekaPropsPhlxTopo = {
    std::size(efhPhlxTopoInitCtxEntries_A),
    efhPhlxTopoInitCtxEntries_A
  };

  EkaProps ekaPropsPhlxOrd = {
    std::size(efhPhlxOrdInitCtxEntries_A),
    efhPhlxOrdInitCtxEntries_A
  };



  //##########################################################
  struct TestFhCtx {
    EkaProps*         pEkaProps;
    EkaGroup*         pEkaGroup;
    uint              numGroups2init;
    uint              numGroups2GetDefs;
    uint              numGroups2run;
  };

  TestFhCtx t[] = {
    {&ekaPropsPhlxTopo, (EkaGroup*)&phlxTopoGroups, std::size(phlxTopoGroups), std::size(phlxTopoGroups), std::size(phlxTopoGroups) },
    {&ekaPropsPhlxOrd , (EkaGroup*)&phlxOrdGroups,  std::size(phlxOrdGroups),  std::size(phlxOrdGroups),  std::size(phlxOrdGroups) }
  };

  EfhRunCtx      runCtx[MAX_TEST_THREADS] = {};
  EfhInitCtx    initCtx[MAX_TEST_THREADS] = {};
  EfhCtx*       pEfhCtx[MAX_TEST_THREADS] = {};
  std::thread runThread[MAX_TEST_THREADS] = {};

  EkaDev* dev = pEkaDev;

  //##########################################################
  //###################### MAIN LOOP #########################
  //##########################################################


  for (uint i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
    EkaSource grpSource = t[i].pEkaGroup[0].source;
    uint8_t grpId = (uint8_t)t[i].pEkaGroup[0].localId;

    std::string exchName = std::string(EKA_EXCH_DECODE(grpSource)) + '_' + std::to_string(grpId);

    if (! runFH[(uint8_t)grpSource]) {
      EKA_LOG("Skipping for %s (%s)",exchName.c_str(),EKA_CTS_SOURCE(grpSource));
      continue;
    } 

    initCtx[i].ekaProps = t[i].pEkaProps;
    initCtx[i].numOfGroups = t[i].numGroups2init;
    initCtx[i].coreId = 0;
    initCtx[i].recvSoftwareMd = true;

    EKA_LOG("Running with: %s (%s) on %u numGroups2init and %u numGroups2run",exchName.c_str(),EKA_CTS_SOURCE(grpSource),t[i].numGroups2init,t[i].numGroups2run);

#ifdef EKA_TEST_IGNORE_DEFINITIONS
    initCtx[i].subscribe_all = true;
#endif
    printf("##########################################################\n");
    printf("################ INIT STAGE for %s:%u ################\n", EKA_EXCH_DECODE(grpSource),grpId);
    printf("##########################################################\n");


    if (t[i].numGroups2init == 0) pEfhCtx[i] = pEfhCtx[i-1];
  //##########################################################
    else  efhInit(&pEfhCtx[i],pEkaDev,&initCtx[i]);
  //##########################################################

    dev->fh[dev->numFh - 1]->print_parsed_messages = print_parsed_messages;

    runCtx[i].onEfhQuoteMsgCb        = onQuote;
    runCtx[i].onEfhDefinitionMsgCb   = onDefinition;
    runCtx[i].onEfhOrderMsgCb        = onOrder;
    runCtx[i].onEfhTradeMsgCb        = onTrade;
    runCtx[i].onEfhFeedDownMsgCb     = onFeedDown;
    runCtx[i].onEfhFeedUpMsgCb       = onFeedUp;
    runCtx[i].onEkaExceptionReportCb = onException;

    runCtx[i].efhRunUserData = (EfhRunUserData)pEfhCtx[i];

    runCtx[i].groups = t[i].pEkaGroup;
    runCtx[i].numGroups = t[i].numGroups2run;

    pEfhCtx[i]->dontPrintTobUpd = true;
    pEfhCtx[i]->printQStatistics = true;

    std::string full_dict_file_name   = std::string("FULL_DICT_")   + exchName + ".txt";
    std::string subscr_dict_file_name = std::string("SUBSCR_DICT_") + exchName + ".txt";

    int file_idx = grp2idx(&t[i].pEkaGroup[0]);

    testFhCtx[file_idx].subscr_cnt = 0;

    if ((full_dict[file_idx] = fopen(full_dict_file_name.c_str(),"w"))  == NULL) on_error ("Error opening %s file",full_dict_file_name.c_str());
    EKA_LOG("Opened %s corresponding to full_dict[%d]",full_dict_file_name.c_str(),file_idx);
    if ((subscr_dict[file_idx] = fopen(subscr_dict_file_name.c_str(),"w"))  == NULL) on_error ("Error opening %s file",subscr_dict_file_name.c_str());
    EKA_LOG("Opened %s corresponding to subscr_dict[%d]",full_dict_file_name.c_str(),file_idx);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
#else
    for (uint j = 0; j < t[i].numGroups2GetDefs; j++) {
      printf("##########################################################\n");
      printf("########## DEFINITIONS STAGE for %s:%u (%u) ##########\n", EKA_EXCH_DECODE(grpSource),grpId,j);
      printf("##########################################################\n");
      efhGetDefs(pEfhCtx[i], &runCtx[i], &t[i].pEkaGroup[j]);
      //##########################################################
    }
#endif

    fclose(full_dict[file_idx]);
    fclose(subscr_dict[file_idx]);

    std::string md_file_name = std::string("MD_") + exchName + ".txt";

    if ((md[file_idx] = fopen(md_file_name.c_str(),"w"))  == NULL) on_error ("Error opening %s file",md_file_name.c_str());
    EKA_LOG("Opened %s corresponding to md[%u] = %s",md_file_name.c_str(),file_idx,EKA_PRINT_GRP(&t[i].pEkaGroup[0]));

  //##########################################################
    printf("##########################################################\n");
    printf("################ RUN STAGE for %s:%u #################\n", EKA_EXCH_DECODE(grpSource),grpId);
    printf("##########################################################\n");

    runThread[i] = std::thread(efhRunGroups,pEfhCtx[i],&runCtx[i]);
    //    auto handle = runThread[i].native_handle();
    pthread_setname_np(runThread[i].native_handle(),exchName.c_str());
    if (setAffinity) {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(AFFINITY_CRITICAL_BASE_CORE_ID + i, &cpuset);
      int rc = pthread_setaffinity_np(runThread[i].native_handle(),sizeof(cpu_set_t), &cpuset);
      if (rc != 0) on_error( "Error calling pthread_setaffinity_np for %s on CPU %u",exchName.c_str(),AFFINITY_CRITICAL_BASE_CORE_ID + i);
    }


  //##########################################################

    runThread[i].detach();

    sleep (1);
  }


  while (keep_work) usleep(0);

  for (uint i = 0; i < MAX_GROUPS; i++) if (md[i] != NULL) fclose(md[i]);

  ekaDevClose(pEkaDev);

  printf ("Exitting normally...\n");

  return 0;
}

