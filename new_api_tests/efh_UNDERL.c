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

#include "eka_macros.h"
#include "ekaline.h"
#include "Eka.h"
#include "Efh.h"

#define MAX_SECURITIES 16000

static volatile bool keep_work;
static EfhCtx* pEfhCtx = NULL;
static FILE* md;
static FILE* secid_list;
static char hostname[1024];
static char underlying2subscr[16] = {};
static char today_str[32] = {};
static char mysecurity[MAX_SECURITIES][32] = {};
static char myunderlying[MAX_SECURITIES][8] = {};

#include "eka_default_callbacks4tests.incl"

void OnQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  uint name_idx = (uint) secData;
  uint8_t gr_id = (uint8_t)msg->header.group.localId;
  if (pEfhCtx->printQStatistics && (++pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr_id]->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  char ts[32];
  convert_ts(ts,msg->header.timeStamp);
  fprintf(md,"%s,%s,%s,%s,%s,%c,%u,%.*f,%u,%u,%.*f,%u,%c,%c,%s\n",
	  "OptionMarketDataNASDAQ4",
	  today_str,
	  "IrrelevantCaptureTime",
	  mysecurity[name_idx],
	  myunderlying[name_idx],
	  '1',
	  msg->bidSide.size,
	  EKA_DEC_POINTS_10000(msg->bidSide.price), ((float) msg->bidSide.price / 10000),
	  msg->bidSide.customerSize,
	  msg->askSide.size,
	  EKA_DEC_POINTS_10000(msg->askSide.price), ((float) msg->askSide.price / 10000),
	  msg->askSide.customerSize,
	  EKA_TS_DECODE(msg->tradeStatus),
	  EKA_TS_DECODE(msg->tradeStatus),
	  ts
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
  sprintf(dst+6,"%02u%02u%02u%c%08u",y,m,d,call_put,msg->strikePrice / 10);
  return;
}

void OnDefinition2subscr(const EfhDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (memcmp(msg->underlying,underlying2subscr,strlen(underlying2subscr)) == 0) {
    static uint subscr_cnt = 0;
    if (subscr_cnt > MAX_SECURITIES) {
      on_warning("Trying to subscibe on %u securities > %u MAX_SECURITIES",subscr_cnt,MAX_SECURITIES);
      return;
    }
    eka_create_avt_definition(mysecurity[subscr_cnt],msg);
    memcpy(myunderlying[subscr_cnt],msg->underlying,6);
    for (auto i = 0; i < 6; i++) if (myunderlying[subscr_cnt][i] == ' ') myunderlying[subscr_cnt][i] = 0;

    fprintf (secid_list,"%s,%ju\n",mysecurity[subscr_cnt],msg->header.securityId);
    printf ("%s,%ju\n",mysecurity[subscr_cnt],msg->header.securityId);
    efhSubscribeStatic(pEfhCtx, (EkaGroup*) &msg->header.group,  msg->header.securityId, EfhSecurityType::kOpt,(EfhSecUserData) subscr_cnt,0,0);
    subscr_cnt++;
  }

  return;
}

int main(int argc, char *argv[]) {
  if (argc != 2) on_error("Underlying symbol must be specified");
  printf ("Gathering data for %s\n",argv[1]);
  sprintf (underlying2subscr,"%s",argv[1]);


  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  
  sprintf(today_str,"%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);


  EkaDev* pEkaDev = NULL;

  EkaProp efhInitCtxEntries_side_A[] = {

    {"efh.NOM_ITTO.snapshot.auth","NGBAR2:HY4VXK"},

    {"efh.NOM_ITTO.group.0.mcast.addr","233.54.12.72:18000"},

    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},

    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"}
  };

  EkaProp efhInitCtxEntries_side_B[] = {

    {"efh.NOM_ITTO.snapshot.auth","NGBAR2:HY4VXK"},

    {"efh.NOM_ITTO.group.0.mcast.addr","233.54.12.76:18000"},
    //   {"efh.NOM_ITTO.group.0.mcast.addr","233.49.196.72:18000"},

    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},

    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"},

  };


  gethostname(hostname, 1024);

  bool side_A = true;

  if (strcmp(hostname, "nqxlxavt188d") == 0) side_A = false;
  if (strcmp(hostname, "nqxlxavt194d") == 0) side_A = false;
  if (strcmp(hostname, "nqxlxavt193d") == 0) side_A = false;

  if ((secid_list = fopen("secid_list.txt","w"))  == NULL) on_error ("Error opening secid_list file\n");
  char md_filename[24] = {};
  sprintf (md_filename,"%s_md.txt",underlying2subscr);
  if ((md = fopen(md_filename,"w")) == NULL) on_error ("Error opening %s file",md_filename);


  EkaProps ekaProps = {
    .numProps = side_A ? std::size(efhInitCtxEntries_side_A) : std::size(efhInitCtxEntries_side_B),
    .props = side_A ? efhInitCtxEntries_side_A : efhInitCtxEntries_side_B
  };

  const EfhInitCtx efhInitCtx = {
    .ekaProps = &ekaProps,
    .numOfGroups = 1,
    .coreId = 0,
    .recvSoftwareMd = true,
    //    .full_book = false,  //-- NOT NEEDED
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    .subscribe_all = true,
#else
    .subscribe_all = false,
#endif
    //    .feed_ver = EfhFeedVer::kNASDAQ
    //    .feed_ver = EfhFeedVer::kInvalid
  };

  const EfhInitCtx* pEfhInitCtx = &efhInitCtx;

  keep_work = true;
  signal(SIGINT, INThandler);

  EfhRunCtx runCtx[4] = {};

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  efhInit(&pEfhCtx,pEkaDev,pEfhInitCtx);
  pEfhCtx->dontPrintTobUpd = false;
  pEfhCtx->printQStatistics = true;

  pEfhCtx->dev->fh[pEfhCtx->fhId]->print_parsed_messages = true;

  std::thread efh_run_thread[4];

  for (uint8_t i = 0; i < efhInitCtx.numOfGroups; i++) {
    EkaGroup gr = {EkaSource::kNOM_ITTO, (EkaLSI)i};
    runCtx[i].numGroups = 1;
    runCtx[i].groups = &gr;

    runCtx[i].efhRunUserData = (EfhRunUserData) pEfhCtx;
    EfhRunCtx* pEfhRunCtx = &runCtx[i];

    pEfhRunCtx->onEfhQuoteMsgCb        = OnQuote;
    pEfhRunCtx->onEfhDefinitionMsgCb   = OnDefinition2subscr;
    pEfhRunCtx->onEfhOrderMsgCb        = efhDefaultOnOrder;
    pEfhRunCtx->onEfhTradeMsgCb        = efhDefaultOnTrade;
    pEfhRunCtx->onEfhFeedDownMsgCb     = efhDefaultOnFeedDown;
    pEfhRunCtx->onEfhFeedUpMsgCb       = efhDefaultOnFeedUp;
    pEfhRunCtx->onEkaExceptionReportCb = efhDefaultOnException;

    printf ("################ Group %u ################\n",runCtx[i].groups->localId);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    printf ("Skipping Definitions for EKA_TEST_IGNORE_DEFINITIONS\n");
#else
    efhGetDefs(pEfhCtx, &runCtx[i], &gr);
#endif
    efh_run_thread[i] = std::thread(efhRunGroups,pEfhCtx, &runCtx[i]);
    efh_run_thread[i].detach();
#ifdef EKA_TEST_IGNORE_GAP
    //    sleep (1);
#else
    //    sleep (60);
#endif
  }

  while (keep_work) usleep(0);

  efhDestroy(pEfhCtx);
  ekaDevClose(pEkaDev);
  fclose(md);
  fclose(secid_list);

  printf ("Exitting normally...\n");

  return 0;
}

