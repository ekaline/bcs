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

#define MAX_SECURITIES 16000

static volatile bool keep_work;
static EfhCtx* pEfhCtx = NULL;
FILE* md;
FILE* secid_list;
char hostname[1024];

#include "eka_default_callbacks4tests.incl"

void OnQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;

  uint8_t gr_id = (uint8_t)msg->header.group.localId;
  if (pEfhCtx->printQStatistics && (++pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr_id]->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  char ts[32];
  convert_ts(ts,msg->header.timeStamp);
  fprintf(md,"%s:%u, SId:%016jx, SN:%06ju,TS:%s, gapNum:%u, TS: %c, BidPrice:%10u, BidSize:%4u, BidCustSize:%4u, AskPrice:%10u, AskSize:%4u, AskCustSize:%4u \n",
	  EKA_EXCH_DECODE(msg->header.group.source),
	  msg->header.group.localId,
	  msg->header.securityId,
	  msg->header.sequenceNumber,
	  ts,
	  msg->header.gapNum,
	  EKA_TS_DECODE(msg->tradeStatus),
	  msg->bidSide.price,
	  msg->bidSide.size,
	  msg->bidSide.customerSize,
	  msg->askSide.price,
	  msg->askSide.size,
	  msg->askSide.customerSize
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

void OnDefinitionAAPL(const EfhDefinitionMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;
  if (memcmp(msg->underlying,"AAPL",4) == 0) {
    char sec[32] = {};
    eka_create_avt_definition(sec,msg);
    fprintf (secid_list,"%s,%ju\n",sec,msg->header.securityId);
    printf ("%s,%ju\n",sec,msg->header.securityId);
    efhSubscribeStatic(pEfhCtx, (EkaGroup*) &msg->header.group,  msg->header.securityId, EfhSecurityType::kOpt,(EfhSecUserData) 0,0,0);
  }

  return;
}

int main(int argc, char *argv[]) {

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

    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},

    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"},

  };


  gethostname(hostname, 1024);

  bool side_A = true;

  if (strcmp(hostname, "nqxlxavt188d") == 0) side_A = false;
  if (strcmp(hostname, "nqxlxavt194d") == 0) side_A = false;
  if (strcmp(hostname, "nqxlxavt193d") == 0) side_A = false;

  if ((secid_list = fopen("secid_list.txt","w"))  == NULL) on_error ("Error opening secid_list file\n");
  if ((md = fopen("AAPL_MD.txt","w")) == NULL) on_error ("Error opening AAPL_MD file\n");


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

  std::thread efh_run_thread[4];

  for (uint8_t i = 0; i < efhInitCtx.numOfGroups; i++) {
    EkaGroup gr = {EkaSource::kNOM_ITTO, (EkaLSI)i};
    runCtx[i].numGroups = 1;
    runCtx[i].groups = &gr;

    runCtx[i].efhRunUserData = (EfhRunUserData) pEfhCtx;
    EfhRunCtx* pEfhRunCtx = &runCtx[i];

    pEfhRunCtx->onEfhQuoteMsgCb        = OnQuote;
    pEfhRunCtx->onEfhDefinitionMsgCb   = OnDefinitionAAPL;
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

