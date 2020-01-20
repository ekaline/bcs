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
#include "eka_dev.h"
#include "eka_fh.h"

#include "eka_macros.h"

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

int credAcquire(EkaCredentialType credType, EkaSource source, const char *user, const struct timespec *leaseTime, const struct timespec *timeout, void* context, EkaCredentialLease **lease) {
  printf ("Credential with USER %s is acquired for %s\n",user,EKA_EXCH_DECODE(source));
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
	printf("Running PHLX\n");  
	runFH[(uint8_t)EkaSource::kPHLX_TOPO] = true;
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

  //##########################################################
  // PHLX INIT
  //##########################################################
  EkaProp efhPhlxInitCtxEntries_A[] = {
    {"efh.PHLX_TOPO.group.0.mcast.addr","233.47.179.104:18016"},
    {"efh.PHLX_TOPO.group.1.mcast.addr","233.47.179.105:18017"},
    {"efh.PHLX_TOPO.group.2.mcast.addr","233.47.179.106:18018"},
    {"efh.PHLX_TOPO.group.3.mcast.addr","233.47.179.107:18019"},
    {"efh.PHLX_TOPO.group.4.mcast.addr","233.47.179.108:18020"},
    {"efh.PHLX_TOPO.group.5.mcast.addr","233.47.179.109:18021"},
    {"efh.PHLX_TOPO.group.6.mcast.addr","233.47.179.110:18022"},
    {"efh.PHLX_TOPO.group.7.mcast.addr","233.47.179.111:18023"},

    /* {"efh.PHLX_TOPO.group.8.mcast.addr" ,"233.47.179.112:18032"}, */
    /* {"efh.PHLX_TOPO.group.9.mcast.addr" ,"233.47.179.113:18033"}, */
    /* {"efh.PHLX_TOPO.group.10.mcast.addr","233.47.179.114:18034"}, */
    /* {"efh.PHLX_TOPO.group.11.mcast.addr","233.47.179.115:18035"}, */
    /* {"efh.PHLX_TOPO.group.12.mcast.addr","233.47.179.116:18036"}, */
    /* {"efh.PHLX_TOPO.group.13.mcast.addr","233.47.179.117:18037"}, */
    /* {"efh.PHLX_TOPO.group.14.mcast.addr","233.47.179.118:18038"}, */
    /* {"efh.PHLX_TOPO.group.15.mcast.addr","233.47.179.119:18039"}, */

    {"efh.PHLX_TOPO.group.0.snapshot.auth","BARZ20:02ZRAB"},
    {"efh.PHLX_TOPO.group.1.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.2.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.3.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.4.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.5.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.6.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.7.snapshot.auth","BARZ03:30ZRAB"},

    // quotes B side, No snapshots for Trades
    {"efh.PHLX_TOPO.group.0.snapshot.addr","206.200.151.82:18116"},
    {"efh.PHLX_TOPO.group.1.snapshot.addr","206.200.151.83:18117"},
    {"efh.PHLX_TOPO.group.2.snapshot.addr","206.200.151.84:18118"},
    {"efh.PHLX_TOPO.group.3.snapshot.addr","206.200.151.85:18119"},
    {"efh.PHLX_TOPO.group.4.snapshot.addr","206.200.151.86:18120"},
    {"efh.PHLX_TOPO.group.5.snapshot.addr","206.200.151.87:18121"},
    {"efh.PHLX_TOPO.group.6.snapshot.addr","206.200.151.88:18122"},
    {"efh.PHLX_TOPO.group.7.snapshot.addr","206.200.151.89:18123"},

    {"efh.PHLX_TOPO.group.0.recovery.addr","206.200.151.82:18116"},
    {"efh.PHLX_TOPO.group.1.recovery.addr","206.200.151.83:18117"},
    {"efh.PHLX_TOPO.group.2.recovery.addr","206.200.151.84:18118"},
    {"efh.PHLX_TOPO.group.3.recovery.addr","206.200.151.85:18119"},
    {"efh.PHLX_TOPO.group.4.recovery.addr","206.200.151.86:18120"},
    {"efh.PHLX_TOPO.group.5.recovery.addr","206.200.151.87:18121"},
    {"efh.PHLX_TOPO.group.6.recovery.addr","206.200.151.88:18122"},
    {"efh.PHLX_TOPO.group.7.recovery.addr","206.200.151.89:18123"},
  };

  EkaProp efhPhlxInitCtxEntries_B[] = {
    {"efh.PHLX_TOPO.group.0.mcast.addr","233.47.179.168:18016"},
    {"efh.PHLX_TOPO.group.1.mcast.addr","233.47.179.169:18017"},
    {"efh.PHLX_TOPO.group.2.mcast.addr","233.47.179.170:18018"},
    {"efh.PHLX_TOPO.group.3.mcast.addr","233.47.179.171:18019"},
    {"efh.PHLX_TOPO.group.4.mcast.addr","233.47.179.172:18020"},
    {"efh.PHLX_TOPO.group.5.mcast.addr","233.47.179.173:18021"},
    {"efh.PHLX_TOPO.group.6.mcast.addr","233.47.179.174:18022"},
    {"efh.PHLX_TOPO.group.7.mcast.addr","233.47.179.175:18023"},
    // trades B side
    /* {"efh.PHLX_TOPO.group.8.mcast.addr" ,"233.47.179.176:18032"}, */
    /* {"efh.PHLX_TOPO.group.9.mcast.addr" ,"233.47.179.177:18033"}, */
    /* {"efh.PHLX_TOPO.group.10.mcast.addr","233.47.179.178:18034"}, */
    /* {"efh.PHLX_TOPO.group.11.mcast.addr","233.47.179.179:18035"}, */
    /* {"efh.PHLX_TOPO.group.12.mcast.addr","233.47.179.180:18036"}, */
    /* {"efh.PHLX_TOPO.group.13.mcast.addr","233.47.179.181:18037"}, */
    /* {"efh.PHLX_TOPO.group.14.mcast.addr","233.47.179.182:18038"}, */
    /* {"efh.PHLX_TOPO.group.15.mcast.addr","233.47.179.183:18039"}, */

    {"efh.PHLX_TOPO.group.0.snapshot.auth","BARZ20:02ZRAB"},
    {"efh.PHLX_TOPO.group.1.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.2.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.3.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.4.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.5.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.6.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.7.snapshot.auth","BARZ03:30ZRAB"},

    // quotes B side, No snapshots for Trades
    {"efh.PHLX_TOPO.group.0.snapshot.addr","206.200.151.82:18116"},
    {"efh.PHLX_TOPO.group.1.snapshot.addr","206.200.151.83:18117"},
    {"efh.PHLX_TOPO.group.2.snapshot.addr","206.200.151.84:18118"},
    {"efh.PHLX_TOPO.group.3.snapshot.addr","206.200.151.85:18119"},
    {"efh.PHLX_TOPO.group.4.snapshot.addr","206.200.151.86:18120"},
    {"efh.PHLX_TOPO.group.5.snapshot.addr","206.200.151.87:18121"},
    {"efh.PHLX_TOPO.group.6.snapshot.addr","206.200.151.88:18122"},
    {"efh.PHLX_TOPO.group.7.snapshot.addr","206.200.151.89:18123"},

    {"efh.PHLX_TOPO.group.0.recovery.addr","206.200.151.82:18116"},
    {"efh.PHLX_TOPO.group.1.recovery.addr","206.200.151.83:18117"},
    {"efh.PHLX_TOPO.group.2.recovery.addr","206.200.151.84:18118"},
    {"efh.PHLX_TOPO.group.3.recovery.addr","206.200.151.85:18119"},
    {"efh.PHLX_TOPO.group.4.recovery.addr","206.200.151.86:18120"},
    {"efh.PHLX_TOPO.group.5.recovery.addr","206.200.151.87:18121"},
    {"efh.PHLX_TOPO.group.6.recovery.addr","206.200.151.88:18122"},
    {"efh.PHLX_TOPO.group.7.recovery.addr","206.200.151.89:18123"},
  };
  EkaProps ekaPropsPhlx = {};
  if (A_side) {
    ekaPropsPhlx.numProps = std::size(efhPhlxInitCtxEntries_A);
    ekaPropsPhlx.props = efhPhlxInitCtxEntries_A;
  } else {
    ekaPropsPhlx.numProps = std::size(efhPhlxInitCtxEntries_B);
    ekaPropsPhlx.props = efhPhlxInitCtxEntries_B;
  }

  EkaGroup phlxGroups[] = {
    {EkaSource::kPHLX_TOPO, (EkaLSI)0},
    {EkaSource::kPHLX_TOPO, (EkaLSI)1},
    {EkaSource::kPHLX_TOPO, (EkaLSI)2},
    {EkaSource::kPHLX_TOPO, (EkaLSI)3},
    {EkaSource::kPHLX_TOPO, (EkaLSI)4},
    {EkaSource::kPHLX_TOPO, (EkaLSI)5},
    {EkaSource::kPHLX_TOPO, (EkaLSI)6},
    {EkaSource::kPHLX_TOPO, (EkaLSI)7},
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)8}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)9}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)10}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)11}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)12}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)13}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)14}, */
    /* {EkaSource::kPHLX_TOPO, (EkaLSI)15}, */
  };

  //##########################################################
  // NOM INIT
  //##########################################################
  /* EkaProp efhNomInitCtxEntries_A_0[] = { */
  EkaProp efhNomInitCtxEntries_A[] = {
    {"efh.NOM_ITTO.group.0.mcast.addr","233.54.12.72:18000"},
    {"efh.NOM_ITTO.group.0.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},
    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"},
  /* }; */

  /* EkaProp efhNomInitCtxEntries_A_1[] = { */
    {"efh.NOM_ITTO.group.1.mcast.addr","233.54.12.73:18001"},
    {"efh.NOM_ITTO.group.1.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.1.snapshot.addr","206.200.43.73:18301"},
    {"efh.NOM_ITTO.group.1.recovery.addr","206.200.43.65:18101"},
  /* }; */

  /* EkaProp efhNomInitCtxEntries_A_2[] = { */
    {"efh.NOM_ITTO.group.2.mcast.addr","233.54.12.74:18002"},
    {"efh.NOM_ITTO.group.2.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.2.snapshot.addr","206.200.43.74:18302"},
    {"efh.NOM_ITTO.group.2.recovery.addr","206.200.43.66:18102"},
  /* }; */

  /* EkaProp efhNomInitCtxEntries_A_3[] = { */
    {"efh.NOM_ITTO.group.3.mcast.addr","233.54.12.75:18003"},
    {"efh.NOM_ITTO.group.3.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.3.snapshot.addr","206.200.43.75:18303"},
    {"efh.NOM_ITTO.group.3.recovery.addr","206.200.43.67:18103"},
  };

  //  EkaProp efhNomInitCtxEntries_B_0[] = {
  EkaProp efhNomInitCtxEntries_B[] = {
    {"efh.NOM_ITTO.group.0.mcast.addr","233.49.196.72:18000"},
    {"efh.NOM_ITTO.group.0.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},
    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"},
  /* }; */
  /* EkaProp efhNomInitCtxEntries_B_1[] = { */
    {"efh.NOM_ITTO.group.1.mcast.addr","233.49.196.73:18001"},
    {"efh.NOM_ITTO.group.1.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.1.snapshot.addr","206.200.43.73:18301"},
    {"efh.NOM_ITTO.group.1.recovery.addr","206.200.43.65:18101"},
  /* }; */
  /* EkaProp efhNomInitCtxEntries_B_2[] = { */
    {"efh.NOM_ITTO.group.2.mcast.addr","233.49.196.74:18002"},
    {"efh.NOM_ITTO.group.2.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.2.snapshot.addr","206.200.43.74:18302"},
    {"efh.NOM_ITTO.group.2.recovery.addr","206.200.43.66:18102"},
  /* }; */
  /* EkaProp efhNomInitCtxEntries_B_3[] = { */
    {"efh.NOM_ITTO.group.3.snapshot.auth","NGBAR2:HY4VXK"},
    {"efh.NOM_ITTO.group.3.snapshot.addr","206.200.43.75:18303"},
    {"efh.NOM_ITTO.group.3.recovery.addr","206.200.43.67:18103"},
    {"efh.NOM_ITTO.group.3.mcast.addr","233.49.196.75:18003"},
  };

    // SideB FPGA Feed - disconnected at AVT
    /* {"efh.NOM_ITTO.group.0.mcast.addr","233.54.12.76:18000"}, */
    /* {"efh.NOM_ITTO.group.1.mcast.addr","233.54.12.77:18001"}, */
    /* {"efh.NOM_ITTO.group.2.mcast.addr","233.54.12.78:18002"}, */
    /* {"efh.NOM_ITTO.group.3.mcast.addr","233.54.12.79:18003"}, */

  EkaProps ekaPropsNom = {};
  if (A_side) {
    ekaPropsNom.numProps = std::size(efhNomInitCtxEntries_A);
    ekaPropsNom.props = efhNomInitCtxEntries_A;
  } else {
    ekaPropsNom.numProps = std::size(efhNomInitCtxEntries_B);
    ekaPropsNom.props = efhNomInitCtxEntries_B;
  }

  /* EkaProps ekaPropsNom_0 = {}; */
  /* EkaProps ekaPropsNom_1 = {}; */
  /* EkaProps ekaPropsNom_2 = {}; */
  /* EkaProps ekaPropsNom_3 = {}; */
  /* if (A_side) { */
  /*   ekaPropsNom_0.numProps = std::size(efhNomInitCtxEntries_A_0); */
  /*   ekaPropsNom_0.props = efhNomInitCtxEntries_A_0; */
  /*   ekaPropsNom_1.numProps = std::size(efhNomInitCtxEntries_A_1); */
  /*   ekaPropsNom_1.props = efhNomInitCtxEntries_A_1; */
  /*   ekaPropsNom_2.numProps = std::size(efhNomInitCtxEntries_A_2); */
  /*   ekaPropsNom_2.props = efhNomInitCtxEntries_A_2; */
  /*   ekaPropsNom_3.numProps = std::size(efhNomInitCtxEntries_A_3); */
  /*   ekaPropsNom_3.props = efhNomInitCtxEntries_A_3; */
  /* } else { */
  /*   ekaPropsNom_0.numProps = std::size(efhNomInitCtxEntries_B_0); */
  /*   ekaPropsNom_0.props = efhNomInitCtxEntries_B_0; */
  /*   ekaPropsNom_1.numProps = std::size(efhNomInitCtxEntries_B_1); */
  /*   ekaPropsNom_1.props = efhNomInitCtxEntries_B_1; */
  /*   ekaPropsNom_2.numProps = std::size(efhNomInitCtxEntries_B_2); */
  /*   ekaPropsNom_2.props = efhNomInitCtxEntries_B_2; */
  /*   ekaPropsNom_3.numProps = std::size(efhNomInitCtxEntries_B_3); */
  /*   ekaPropsNom_3.props = efhNomInitCtxEntries_B_3; */
  /* } */


  EkaGroup nomGroups_0[] = {{EkaSource::kNOM_ITTO, (EkaLSI)0}};
  EkaGroup nomGroups_1[] = {{EkaSource::kNOM_ITTO, (EkaLSI)1}};
  EkaGroup nomGroups_2[] = {{EkaSource::kNOM_ITTO, (EkaLSI)2}};
  EkaGroup nomGroups_3[] = {{EkaSource::kNOM_ITTO, (EkaLSI)3}};

  /* EkaGroup nomGroups[] = { */
  /*   {EkaSource::kNOM_ITTO, (EkaLSI)0}, */
  /*   {EkaSource::kNOM_ITTO, (EkaLSI)1}, */
  /*   {EkaSource::kNOM_ITTO, (EkaLSI)2}, */
  /*   {EkaSource::kNOM_ITTO, (EkaLSI)3}, */
  /* }; */

  //##########################################################
  // GEM INIT
  //##########################################################
  EkaProp efhGemInitCtxEntries_A[] = {
    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.148:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.148:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.148:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.148:18003"},

    {"efh.GEM_TQF.group.0.snapshot.auth","GGTBC4:BR5ODP"},
    {"efh.GEM_TQF.group.1.snapshot.auth","GGTBC5:0WN9GH"},
    {"efh.GEM_TQF.group.2.snapshot.auth","GGTBC6:03BHXL"},
    {"efh.GEM_TQF.group.3.snapshot.auth","GGTBC7:C21TH1"},

    {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.120:18300"},
    {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.121:18301"},
    {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.122:18302"},
    {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.123:18303"},

    /* {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.128:18100"}, */
    /* {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.129:18101"}, */
    /* {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.130:18102"}, */
    /* {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.131:18103"}, */

    {"efh.GEM_TQF.group.0.recovery.addr","206.200.230.128:18100"},
    {"efh.GEM_TQF.group.1.recovery.addr","206.200.230.129:18101"},
    {"efh.GEM_TQF.group.2.recovery.addr","206.200.230.130:18102"},
    {"efh.GEM_TQF.group.3.recovery.addr","206.200.230.131:18103"},
  };
  EkaProp efhGemInitCtxEntries_B[] = {
    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.164:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.164:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.164:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.164:18003"},

    {"efh.GEM_TQF.group.0.snapshot.auth","GGTBC4:BR5ODP"},
    {"efh.GEM_TQF.group.1.snapshot.auth","GGTBC5:0WN9GH"},
    {"efh.GEM_TQF.group.2.snapshot.auth","GGTBC6:03BHXL"},
    {"efh.GEM_TQF.group.3.snapshot.auth","GGTBC7:C21TH1"},

    /* {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.120:18300"}, */
    /* {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.121:18301"}, */
    /* {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.122:18302"}, */
    /* {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.123:18303"}, */

    {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.248:18300"},
    {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.249:18301"},
    {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.250:18302"},
    {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.251:18303"},

    {"efh.GEM_TQF.group.0.recovery.addr","206.200.230.128:18100"},
    {"efh.GEM_TQF.group.1.recovery.addr","206.200.230.129:18101"},
    {"efh.GEM_TQF.group.2.recovery.addr","206.200.230.130:18102"},
    {"efh.GEM_TQF.group.3.recovery.addr","206.200.230.131:18103"},
  };
  EkaProps ekaPropsGem = {};
  if (A_side) {
    ekaPropsGem.numProps = std::size(efhGemInitCtxEntries_A);
    ekaPropsGem.props = efhGemInitCtxEntries_A;
  } else {
    ekaPropsGem.numProps = std::size(efhGemInitCtxEntries_B);
    ekaPropsGem.props = efhGemInitCtxEntries_B;
  }
  EkaGroup gemGroups[] = {
    {EkaSource::kGEM_TQF, (EkaLSI)0},
    {EkaSource::kGEM_TQF, (EkaLSI)1},
    {EkaSource::kGEM_TQF, (EkaLSI)2},
    {EkaSource::kGEM_TQF, (EkaLSI)3}
  };
  //##########################################################
  // ISE INIT
  //##########################################################
  EkaProp efhIseInitCtxEntries_A[] = {
    // side B
    {"efh.ISE_TQF.group.0.mcast.addr","233.54.12.152:18000"},
    {"efh.ISE_TQF.group.1.mcast.addr","233.54.12.152:18001"},
    {"efh.ISE_TQF.group.2.mcast.addr","233.54.12.152:18002"},
    {"efh.ISE_TQF.group.3.mcast.addr","233.54.12.152:18003"},

    {"efh.ISE_TQF.group.0.snapshot.auth","IGTBC9:NI8HKX"},
    {"efh.ISE_TQF.group.1.snapshot.auth","IGTB1B:AZK9CI"},
    {"efh.ISE_TQF.group.2.snapshot.auth","IGTB1D:6V1SWS"},
    {"efh.ISE_TQF.group.3.snapshot.auth","IGTB1F:4A6ZXQ"},

    {"efh.ISE_TQF.group.0.snapshot.addr","206.200.230.104:18300"},
    {"efh.ISE_TQF.group.1.snapshot.addr","206.200.230.105:18301"},
    {"efh.ISE_TQF.group.2.snapshot.addr","206.200.230.106:18302"},
    {"efh.ISE_TQF.group.3.snapshot.addr","206.200.230.107:18303"},

    {"efh.ISE_TQF.group.0.recovery.addr","206.200.230.104:18300"},
    {"efh.ISE_TQF.group.1.recovery.addr","206.200.230.105:18301"},
    {"efh.ISE_TQF.group.2.recovery.addr","206.200.230.106:18302"},
    {"efh.ISE_TQF.group.3.recovery.addr","206.200.230.107:18303"},
  };  
  EkaProp efhIseInitCtxEntries_B[] = {
    // side B
    {"efh.ISE_TQF.group.0.mcast.addr","233.54.12.168:18000"},
    {"efh.ISE_TQF.group.1.mcast.addr","233.54.12.168:18001"},
    {"efh.ISE_TQF.group.2.mcast.addr","233.54.12.168:18002"},
    {"efh.ISE_TQF.group.3.mcast.addr","233.54.12.168:18003"},

    {"efh.ISE_TQF.group.0.snapshot.auth","IGTBC9:NI8HKX"},
    {"efh.ISE_TQF.group.1.snapshot.auth","IGTB1B:AZK9CI"},
    {"efh.ISE_TQF.group.2.snapshot.auth","IGTB1D:6V1SWS"},
    {"efh.ISE_TQF.group.3.snapshot.auth","IGTB1F:4A6ZXQ"},

    {"efh.ISE_TQF.group.0.snapshot.addr","206.200.230.104:18300"},
    {"efh.ISE_TQF.group.1.snapshot.addr","206.200.230.105:18301"},
    {"efh.ISE_TQF.group.2.snapshot.addr","206.200.230.106:18302"},
    {"efh.ISE_TQF.group.3.snapshot.addr","206.200.230.107:18303"},

    {"efh.ISE_TQF.group.0.recovery.addr","206.200.230.160:18100"},
    {"efh.ISE_TQF.group.1.recovery.addr","206.200.230.161:18101"},
    {"efh.ISE_TQF.group.2.recovery.addr","206.200.230.162:18102"},
    {"efh.ISE_TQF.group.3.recovery.addr","206.200.230.163:18103"},
  };
  EkaProps ekaPropsIse = {};
  if (A_side) {
    ekaPropsIse.numProps = std::size(efhIseInitCtxEntries_A);
    ekaPropsIse.props = efhIseInitCtxEntries_A;
  } else {
    ekaPropsIse.numProps = std::size(efhIseInitCtxEntries_B);
    ekaPropsIse.props = efhIseInitCtxEntries_B;
  }
  EkaGroup iseGroups[] = {
    {EkaSource::kISE_TQF, (EkaLSI)0},
    {EkaSource::kISE_TQF, (EkaLSI)1},
    {EkaSource::kISE_TQF, (EkaLSI)2},
    {EkaSource::kISE_TQF, (EkaLSI)3}
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
    /* {&ekaPropsNom_0,   nomGroups_0, 4,                     1, 1                    }, */
    /* {&ekaPropsNom_1,   nomGroups_1, 0,                     1, 1                    }, */
    /* {&ekaPropsNom_2,   nomGroups_2, 0,                     1, 1                    }, */
    /* {&ekaPropsNom_3,   nomGroups_3, 0,                     1, 1                    }, */
    {&ekaPropsNom,        nomGroups_0, 4,                     1, 1                    },
    {NULL,                nomGroups_1, 0,                     1, 1                    },
    {NULL,                nomGroups_2, 0,                     1, 1                    },
    {NULL,                nomGroups_3, 0,                     1, 1                    },
    //    {&ekaPropsNom,   nomGroups,   4,                     4, 4                    },
    {&ekaPropsPhlx,  phlxGroups,  std::size(phlxGroups), 8, std::size(phlxGroups)},
    {&ekaPropsGem,   gemGroups,   std::size(gemGroups),  4, std::size(gemGroups) },
    {&ekaPropsIse,   iseGroups,   std::size(iseGroups),  4, std::size(iseGroups) }
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

