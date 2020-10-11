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
char hostname[1024];

#include "eka_default_callbacks4tests.incl"

void OnQuote(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  EfhCtx* pEfhCtx = (EfhCtx*) userData;

  uint8_t gr_id = (uint8_t)msg->header.group.localId;
  if (pEfhCtx->printQStatistics && (++pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr_id]->upd_ctr % 1000000 == 0)) {
    efhMonitorFhGroupState(pEfhCtx,(EkaGroup*)&msg->header.group);
  }

  if (pEfhCtx->dontPrintTobUpd) return;

#if 1
  if (strcmp(hostname, "xn01") == 0) {
    char ts[32];
    convert_ts(ts,msg->header.timeStamp);
    fprintf(md,"%s:%u, SId:%016jx, SN:%06ju,TS:%s, gapNum:%u, BidPrice:%10u, BidSize:%4u, BidCustSize:%4u, AskPrice:%10u, AskSize:%4u, AskCustSize:%4u \n",
	    EKA_EXCH_DECODE(msg->header.group.source),
	    msg->header.group.localId,
	    msg->header.securityId,
	    msg->header.sequenceNumber,
	    ts,
	    msg->header.gapNum,
	    msg->bidSide.price,
	    msg->bidSide.size,
	    msg->bidSide.customerSize,
	    msg->askSide.price,
	    msg->askSide.size,
	    msg->askSide.customerSize
	    );
  }
#endif
  return;
}

int main(int argc, char *argv[]) {

  EkaDev* pEkaDev = NULL;

  EkaProp efhInitCtxEntries_side_A[] = {

    {"efh.NOM_ITTO.snapshot.auth","NGBAR2:HY4VXK"},

    {"efh.NOM_ITTO.group.0.mcast.addr","233.54.12.72:18000"},
    {"efh.NOM_ITTO.group.1.mcast.addr","233.54.12.73:18001"},
    {"efh.NOM_ITTO.group.2.mcast.addr","233.54.12.74:18002"},
    {"efh.NOM_ITTO.group.3.mcast.addr","233.54.12.75:18003"},

    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},
    {"efh.NOM_ITTO.group.1.snapshot.addr","206.200.43.73:18301"},
    {"efh.NOM_ITTO.group.2.snapshot.addr","206.200.43.74:18302"},
    {"efh.NOM_ITTO.group.3.snapshot.addr","206.200.43.75:18303"},
    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"},
    {"efh.NOM_ITTO.group.1.recovery.addr","206.200.43.65:18101"},
    {"efh.NOM_ITTO.group.2.recovery.addr","206.200.43.66:18102"},
    {"efh.NOM_ITTO.group.3.recovery.addr","206.200.43.67:18103"},
  };

  EkaProp efhInitCtxEntries_side_B[] = {

    {"efh.NOM_ITTO.snapshot.auth","NGBAR2:HY4VXK"},

    {"efh.NOM_ITTO.group.0.mcast.addr","233.54.12.76:18000"},
    {"efh.NOM_ITTO.group.1.mcast.addr","233.54.12.77:18001"},
    {"efh.NOM_ITTO.group.2.mcast.addr","233.54.12.78:18002"},
    {"efh.NOM_ITTO.group.3.mcast.addr","233.54.12.79:18003"},

    {"efh.NOM_ITTO.group.0.snapshot.addr","206.200.43.72:18300"},
    {"efh.NOM_ITTO.group.1.snapshot.addr","206.200.43.73:18301"},
    {"efh.NOM_ITTO.group.2.snapshot.addr","206.200.43.74:18302"},
    {"efh.NOM_ITTO.group.3.snapshot.addr","206.200.43.75:18303"},
    {"efh.NOM_ITTO.group.0.recovery.addr","206.200.43.64:18100"},
    {"efh.NOM_ITTO.group.1.recovery.addr","206.200.43.65:18101"},
    {"efh.NOM_ITTO.group.2.recovery.addr","206.200.43.66:18102"},
    {"efh.NOM_ITTO.group.3.recovery.addr","206.200.43.67:18103"},
  };


  gethostname(hostname, 1024);

  bool side_A = true;

  if (strcmp(hostname, "nqxlxavt188d") == 0) side_A = false;
  if (strcmp(hostname, "nqxlxavt194d") == 0) side_A = false;
  if (strcmp(hostname, "nqxlxavt193d") == 0) side_A = false;
  if (strcmp(hostname, "xn01") == 0) if ((md = fopen("efh_nom.txt","w")) == NULL) on_error("cannot open efh_nom.txt");

  EkaProps ekaProps = {
    .numProps = side_A ? std::size(efhInitCtxEntries_side_A) : std::size(efhInitCtxEntries_side_B),
    .props = side_A ? efhInitCtxEntries_side_A : efhInitCtxEntries_side_B
  };

  const EfhInitCtx efhInitCtx = {
    .ekaProps = &ekaProps,
    .numOfGroups = 4,
    .coreId = 0,
    .recvSoftwareMd = true,
    //    .full_book = false,
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
    pEfhRunCtx->onEfhDefinitionMsgCb   = efhDefaultOnDefinition;
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
    sleep (1);
#else
    sleep (60);
#endif
  }

  while (keep_work) usleep(0);

  efhDestroy(pEfhCtx);
  ekaDevClose(pEkaDev);
  if (strcmp(hostname, "xn01") == 0) fclose(md);

  printf ("Exitting normally...\n");

  return 0;
}

