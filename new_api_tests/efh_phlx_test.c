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


int main(int argc, char *argv[]) {

  EkaDev* pEkaDev = NULL;

  EkaProp efhInitCtxEntries[] = {
#ifdef FH_LAB
    // quotes A side
    {"efh.PHLX_TOPO.group.0.mcast.addr","233.47.179.104:18016"},
    {"efh.PHLX_TOPO.group.1.mcast.addr","233.47.179.105:18017"},
    {"efh.PHLX_TOPO.group.2.mcast.addr","233.47.179.106:18018"},
    {"efh.PHLX_TOPO.group.3.mcast.addr","233.47.179.107:18019"},
    {"efh.PHLX_TOPO.group.4.mcast.addr","233.47.179.108:18020"},
    {"efh.PHLX_TOPO.group.5.mcast.addr","233.47.179.109:18021"},
    {"efh.PHLX_TOPO.group.6.mcast.addr","233.47.179.110:18022"},
    {"efh.PHLX_TOPO.group.7.mcast.addr","233.47.179.111:18023"},
    // trades A side
    {"efh.PHLX_TOPO.group.8.mcast.addr" ,"233.47.179.112:18032"},
    {"efh.PHLX_TOPO.group.9.mcast.addr" ,"233.47.179.113:18033"},
    {"efh.PHLX_TOPO.group.10.mcast.addr","233.47.179.114:18034"},
    {"efh.PHLX_TOPO.group.11.mcast.addr","233.47.179.115:18035"},
    {"efh.PHLX_TOPO.group.12.mcast.addr","233.47.179.116:18036"},
    {"efh.PHLX_TOPO.group.13.mcast.addr","233.47.179.117:18037"},
    {"efh.PHLX_TOPO.group.14.mcast.addr","233.47.179.118:18038"},
    {"efh.PHLX_TOPO.group.15.mcast.addr","233.47.179.119:18039"},
#else
    // quotes B side
    {"efh.PHLX_TOPO.group.0.mcast.addr","233.47.179.168:18016"},
    {"efh.PHLX_TOPO.group.1.mcast.addr","233.47.179.169:18017"},
    {"efh.PHLX_TOPO.group.2.mcast.addr","233.47.179.170:18018"},
    {"efh.PHLX_TOPO.group.3.mcast.addr","233.47.179.171:18019"},
    {"efh.PHLX_TOPO.group.4.mcast.addr","233.47.179.172:18020"},
    {"efh.PHLX_TOPO.group.5.mcast.addr","233.47.179.173:18021"},
    {"efh.PHLX_TOPO.group.6.mcast.addr","233.47.179.174:18022"},
    {"efh.PHLX_TOPO.group.7.mcast.addr","233.47.179.175:18023"},
    // trades B side
    {"efh.PHLX_TOPO.group.8.mcast.addr" ,"233.47.179.176:18032"},
    {"efh.PHLX_TOPO.group.9.mcast.addr" ,"233.47.179.177:18033"},
    {"efh.PHLX_TOPO.group.10.mcast.addr","233.47.179.178:18034"},
    {"efh.PHLX_TOPO.group.11.mcast.addr","233.47.179.179:18035"},
    {"efh.PHLX_TOPO.group.12.mcast.addr","233.47.179.180:18036"},
    {"efh.PHLX_TOPO.group.13.mcast.addr","233.47.179.181:18037"},
    {"efh.PHLX_TOPO.group.14.mcast.addr","233.47.179.182:18038"},
    {"efh.PHLX_TOPO.group.15.mcast.addr","233.47.179.183:18039"},
#endif
    {"efh.PHLX_TOPO.group.0.snapshot.auth","BARZ20:02ZRAB"},
    {"efh.PHLX_TOPO.group.1.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.2.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.3.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.4.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.5.snapshot.auth","BARZ03:30ZRAB"},
    {"efh.PHLX_TOPO.group.6.snapshot.auth","BARZ02:20ZRAB"},
    {"efh.PHLX_TOPO.group.7.snapshot.auth","BARZ03:30ZRAB"},

    /* {"efh.PHLX_TOPO.group.8.snapshot.auth","BARZ20:02ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.9.snapshot.auth","BARZ03:30ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.10.snapshot.auth","BARZ02:20ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.11.snapshot.auth","BARZ03:30ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.12.snapshot.auth","BARZ02:20ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.13.snapshot.auth","BARZ03:30ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.14.snapshot.auth","BARZ02:20ZRAB"}, */
    /* {"efh.PHLX_TOPO.group.15.snapshot.auth","BARZ03:30ZRAB"}, */

    // quotes B side
    {"efh.PHLX_TOPO.group.0.snapshot.addr","206.200.151.82:18116"},
    {"efh.PHLX_TOPO.group.1.snapshot.addr","206.200.151.83:18117"},
    {"efh.PHLX_TOPO.group.2.snapshot.addr","206.200.151.84:18118"},
    {"efh.PHLX_TOPO.group.3.snapshot.addr","206.200.151.85:18119"},
    {"efh.PHLX_TOPO.group.4.snapshot.addr","206.200.151.86:18120"},
    {"efh.PHLX_TOPO.group.5.snapshot.addr","206.200.151.87:18121"},
    {"efh.PHLX_TOPO.group.6.snapshot.addr","206.200.151.88:18122"},
    {"efh.PHLX_TOPO.group.7.snapshot.addr","206.200.151.89:18123"},

    // trades B side
    /* {"efh.PHLX_TOPO.group.8.snapshot.addr" ,"206.200.151.90:18116"}, */
    /* {"efh.PHLX_TOPO.group.9.snapshot.addr" ,"206.200.151.91:18117"}, */
    /* {"efh.PHLX_TOPO.group.10.snapshot.addr","206.200.151.92:18118"}, */
    /* {"efh.PHLX_TOPO.group.11.snapshot.addr","206.200.151.93:18119"}, */
    /* {"efh.PHLX_TOPO.group.12.snapshot.addr","206.200.151.94:18120"}, */
    /* {"efh.PHLX_TOPO.group.13.snapshot.addr","206.200.151.95:18121"}, */
    /* {"efh.PHLX_TOPO.group.14.snapshot.addr","206.200.151.96:18122"}, */
    /* {"efh.PHLX_TOPO.group.15.snapshot.addr","206.200.151.97:18123"}, */

    // dummy recovery
    {"efh.PHLX_TOPO.group.0.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.1.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.2.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.3.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.4.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.5.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.6.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.7.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.8.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.9.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.10.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.11.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.12.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.13.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.14.recovery.addr","0.0.0.0:0"},
    {"efh.PHLX_TOPO.group.15.recovery.addr","0.0.0.0:0"},
  };
  EkaProps ekaProps = {
    .numProps = std::size(efhInitCtxEntries),
    .props = efhInitCtxEntries
  };

  const EfhInitCtx efhInitCtx = {
    .ekaProps = &ekaProps,
    .numOfGroups = 16,
    .coreId = 0,
    .recvSoftwareMd = true,
    //    .full_book = false,
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    .subscribe_all = true,
#else
    .subscribe_all = false,
#endif
    //    .feed_ver = EfhFeedVer::kPHLX
  };

  const EfhInitCtx* pEfhInitCtx = &efhInitCtx;

  keep_work = true;
  signal(SIGINT, INThandler);

  EfhRunCtx runCtx = {};
  const EkaGroup phlxGroups[] = {
    {EkaSource::kPHLX_TOPO, (EkaLSI)0},
    {EkaSource::kPHLX_TOPO, (EkaLSI)1},
    {EkaSource::kPHLX_TOPO, (EkaLSI)2},
    {EkaSource::kPHLX_TOPO, (EkaLSI)3},
    {EkaSource::kPHLX_TOPO, (EkaLSI)4},
    {EkaSource::kPHLX_TOPO, (EkaLSI)5},
    {EkaSource::kPHLX_TOPO, (EkaLSI)6},
    {EkaSource::kPHLX_TOPO, (EkaLSI)7},
    {EkaSource::kPHLX_TOPO, (EkaLSI)8},
    {EkaSource::kPHLX_TOPO, (EkaLSI)9},
    {EkaSource::kPHLX_TOPO, (EkaLSI)10},
    {EkaSource::kPHLX_TOPO, (EkaLSI)11},
    {EkaSource::kPHLX_TOPO, (EkaLSI)12},
    {EkaSource::kPHLX_TOPO, (EkaLSI)13},
    {EkaSource::kPHLX_TOPO, (EkaLSI)14},
    {EkaSource::kPHLX_TOPO, (EkaLSI)15},
  };

  runCtx.numGroups = std::size(phlxGroups);
  runCtx.groups = phlxGroups;

  EfhRunCtx* pEfhRunCtx = &runCtx;

  pEfhRunCtx->onEfhQuoteMsgCb        = efhDefaultOnQuote;
  pEfhRunCtx->onEfhDefinitionMsgCb   = efhDefaultOnDefinition;
  pEfhRunCtx->onEfhOrderMsgCb        = efhDefaultOnOrder;
  pEfhRunCtx->onEfhTradeMsgCb        = efhDefaultOnTrade;
  pEfhRunCtx->onEfhFeedDownMsgCb     = efhDefaultOnFeedDown;
  pEfhRunCtx->onEfhFeedUpMsgCb       = efhDefaultOnFeedUp;
  pEfhRunCtx->onEkaExceptionReportCb = efhDefaultOnException;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  efhInit(&pEfhCtx,pEkaDev,pEfhInitCtx);
  runCtx.efhRunUserData = (EfhRunUserData) pEfhCtx;

  pEfhCtx->dontPrintTobUpd = false;
  pEfhCtx->printQStatistics = true;

  for (uint8_t i = 0; i < runCtx.numGroups / 2; i++) { // half groups are trades
    printf ("################ Group %u ################\n",i);
#ifdef EKA_TEST_IGNORE_DEFINITIONS
    printf ("Skipping Definitions for EKA_TEST_IGNORE_DEFINITIONS\n");
#else
    efhGetDefs(pEfhCtx, pEfhRunCtx, (EkaGroup*)&pEfhRunCtx->groups[i]);
#endif
    sleep (1);
  }
  printf ("Definitions phase done:\n");
  for (uint8_t i = 0; i < runCtx.numGroups; i++) 
    printf ("\tGR%u: subscribed securities: %u\n",i,pEkaDev->fh[0]->b_gr[i]->book->total_securities);
  std::thread efh_run_thread = std::thread(efhRunGroups,pEfhCtx, pEfhRunCtx);
  efh_run_thread.detach();

  while (keep_work) usleep(0);

  efhDestroy(pEfhCtx);
  ekaDevClose(pEkaDev);

  printf ("Exitting normally...\n");

  return 0;
}
