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

#include "ekaline.h"
#include "Eka.h"
#include "Efh.h"

#define MAX_SECURITIES 16000

static volatile bool keep_work;
static EfhCtx* pEfhCtx = NULL;

#include "eka_default_callbacks4tests.incl"

void testDelayCB(const EfhQuoteMsg* msg, EfhSecUserData secData, EfhRunUserData userData) {
  //  EfhCtx* pEfhCtx = (EfhCtx*) userData;

  static uint64_t visits = 0;
  if (++visits == 91) {
    uint delay_time = 1;
    printf ("%s: DELAYING CALLBACK for %u seconds for GR#%u\n",__func__,delay_time,msg->header.group.localId);
    sleep (delay_time);
    printf ("%s: DELAYING CALLBACK released\n",__func__);
  }
  efhDefaultOnQuote(msg,secData,userData);
  return;
}

int main(int argc, char *argv[]) {

  EkaDev* pEkaDev = NULL;

  EkaProp efhInitCtxEntries[] = {
#ifdef FH_LAB
    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.148:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.148:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.148:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.148:18003"},
#else
    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.164:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.164:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.164:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.164:18003"},
#endif
    {"efh.GEM_TQF.group.0.snapshot.auth","GGTBC4:BR5ODP"},
    {"efh.GEM_TQF.group.1.snapshot.auth","GGTBC5:0WN9GH"},
    {"efh.GEM_TQF.group.2.snapshot.auth","GGTBC6:03BHXL"},
    {"efh.GEM_TQF.group.3.snapshot.auth","GGTBC7:C21TH1"},

    {"efh.GEM_TQF.group.0.snapshot.addr","206.200.230.120:18300"},
    {"efh.GEM_TQF.group.1.snapshot.addr","206.200.230.121:18301"},
    {"efh.GEM_TQF.group.2.snapshot.addr","206.200.230.122:18302"},
    {"efh.GEM_TQF.group.3.snapshot.addr","206.200.230.123:18303"},

    {"efh.GEM_TQF.group.0.recovery.addr","0.0.0.0:0"},
    {"efh.GEM_TQF.group.1.recovery.addr","0.0.0.0:0"},
    {"efh.GEM_TQF.group.2.recovery.addr","0.0.0.0:0"},
    {"efh.GEM_TQF.group.3.recovery.addr","0.0.0.0:0"},
  };
  EkaProps ekaProps = {
    .numProps = std::size(efhInitCtxEntries),
    .props = efhInitCtxEntries
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
    //    .feed_ver = EfhFeedVer::kInvalid
  };

  const EfhInitCtx* pEfhInitCtx = &efhInitCtx;

  keep_work = true;
  signal(SIGINT, INThandler);

  EfhRunCtx runCtx = {};
  const EkaGroup gemGroups[] = {
    {EkaSource::kGEM_TQF, (EkaLSI)0},
    {EkaSource::kGEM_TQF, (EkaLSI)1},
    {EkaSource::kGEM_TQF, (EkaLSI)2},
    {EkaSource::kGEM_TQF, (EkaLSI)3}
  };

  runCtx.numGroups = std::size(gemGroups);
  runCtx.groups = gemGroups;

  EfhRunCtx* pEfhRunCtx = &runCtx;

  pEfhRunCtx->onEfhQuoteMsgCb        = efhDefaultOnQuote;
  pEfhRunCtx->onEfhDefinitionMsgCb   = efhDefaultOnDefinition;
  pEfhRunCtx->onEfhOrderMsgCb        = efhDefaultOnOrder;
  pEfhRunCtx->onEfhTradeMsgCb        = efhDefaultOnTrade;
  pEfhRunCtx->onEfhFeedDownMsgCb     = efhDefaultOnFeedDown;
  pEfhRunCtx->onEfhFeedUpMsgCb       = efhDefaultOnFeedUp;
  pEfhRunCtx->onEkaExceptionReportCb = efhDefaultOnException;

  pEfhRunCtx->onEfhQuoteMsgCb        = testDelayCB;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  efhInit(&pEfhCtx,pEkaDev,pEfhInitCtx);
  runCtx.efhRunUserData = (EfhRunUserData) pEfhCtx;

  pEfhCtx->dontPrintTobUpd = true;
  pEfhCtx->printQStatistics = true;

  for (uint8_t i = 0; i < runCtx.numGroups; i++) {
    printf ("################ Group %u ################\n",i);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    printf ("Skipping Definitions for EKA_TEST_IGNORE_DEFINITIONS\n");
#else
    efhGetDefs(pEfhCtx, &runCtx, (EkaGroup*)&runCtx.groups[i]);
#endif
    sleep (1);
  }
  std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx, &runCtx);
  efh_run_thread.detach();

  while (keep_work) usleep(0);

  efhDestroy(pEfhCtx);
  ekaDevClose(pEkaDev);

  printf ("Exitting normally...\n");

  return 0;
}
