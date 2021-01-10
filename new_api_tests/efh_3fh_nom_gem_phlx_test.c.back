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
#include "EkaFhGroup.h"

#include "eka_macros.h"
#include "Efh.h"
#include "Exc.h"
#include "Eka.h"
#include "Efc.h"
#include "EkaCtxs.h"

#define MAX_SECURITIES 16000

#define NOM_GROUPS 4
static volatile bool keep_work;


#include "eka_default_callbacks4tests.incl"

int main(int argc, char *argv[]) {
  keep_work = true;
  signal(SIGINT, INThandler);
  EkaDev* pEkaDev = NULL;

  static EfhCtx* pPhlxEfhCtx = NULL;
  static EfhCtx* pNomEfhCtx  = NULL;
  static EfhCtx* pGemEfhCtx  = NULL;

  EkaDevInitCtx ekaDevInitCtx = {};
  ekaDevInit(&pEkaDev, (const EkaDevInitCtx*) &ekaDevInitCtx);

  //##########################################################
  // PHLX INIT
  //##########################################################
  EkaProp efhPhlxInitCtxEntries[] = {
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

    // No recovery
  };
  EkaProps ekaPropsPhlx = {
    .numProps = std::size(efhPhlxInitCtxEntries),
    .props = efhPhlxInitCtxEntries
  };

  const EfhInitCtx efhPhlxInitCtx = {
    .ekaProps = &ekaPropsPhlx,
    .numOfGroups = 16,
    .coreId = 0,
    .recvSoftwareMd = true,
    //    .full_book = false,
    .subscribe_all = false,
    //    .feed_ver = EfhFeedVer::kPHLX
  };

  const EfhInitCtx* pEfhPhlxInitCtx = &efhPhlxInitCtx;
  //---------------------------------------------------------
  EfhRunCtx phlxRunCtx = {};
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
  EfhRunCtx* pPhlxRunCtx = &phlxRunCtx;
  pPhlxRunCtx->numGroups = std::size(phlxGroups);
  pPhlxRunCtx->groups = phlxGroups;

  pPhlxRunCtx->onEfhQuoteMsgCb        = efhDefaultOnQuote;
  pPhlxRunCtx->onEfhDefinitionMsgCb   = efhDefaultOnDefinition;
  pPhlxRunCtx->onEfhOrderMsgCb        = efhDefaultOnOrder;
  pPhlxRunCtx->onEfhTradeMsgCb        = efhDefaultOnTrade;
  pPhlxRunCtx->onEfhFeedDownMsgCb     = efhDefaultOnFeedDown;
  pPhlxRunCtx->onEfhFeedUpMsgCb       = efhDefaultOnFeedUp;
  pPhlxRunCtx->onEkaExceptionReportCb = efhDefaultOnException;

  efhInit(&pPhlxEfhCtx,pEkaDev,pEfhPhlxInitCtx);
  pPhlxRunCtx->efhRunUserData = (EfhRunUserData) pPhlxEfhCtx;

  pPhlxEfhCtx->dontPrintTobUpd = true;
  pPhlxEfhCtx->printQStatistics = true;

  //##########################################################
  // NOM INIT
  //##########################################################
  EkaProp efhNomInitCtxEntries[] = {

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
  EkaProps ekaPropsNom = {
    .numProps = std::size(efhNomInitCtxEntries),
    .props = efhNomInitCtxEntries
  };

  const EfhInitCtx efhNomInitCtx = {
    .ekaProps = &ekaPropsNom,
    .numOfGroups = 4,
    .coreId = 0,
    .recvSoftwareMd = true,
    //    .full_book = true,
    .subscribe_all = false,
    //    .feed_ver = EfhFeedVer::kNASDAQ
  };

  const EfhInitCtx* pEfhNomInitCtx = &efhNomInitCtx;
  //---------------------------------------------------------
  EfhRunCtx nomRunCtx[NOM_GROUPS] = {};
  EfhRunCtx* pNomRunCtx[NOM_GROUPS] = {};
  const EkaGroup nomGroups[NOM_GROUPS] = {
    {EkaSource::kNOM_ITTO, (EkaLSI)0},
    {EkaSource::kNOM_ITTO, (EkaLSI)1},
    {EkaSource::kNOM_ITTO, (EkaLSI)2},
    {EkaSource::kNOM_ITTO, (EkaLSI)3},
  };

  efhInit(&pNomEfhCtx,pEkaDev,pEfhNomInitCtx);
  for (uint8_t i = 0; i < NOM_GROUPS; i++) {
    pNomRunCtx[i] = &nomRunCtx[i];
    pNomRunCtx[i]->numGroups = 1;
    pNomRunCtx[i]->groups = &nomGroups[i];

    pNomRunCtx[i]->onEfhQuoteMsgCb        = efhDefaultOnQuote;
    pNomRunCtx[i]->onEfhDefinitionMsgCb   = efhDefaultOnDefinition;
    pNomRunCtx[i]->onEfhOrderMsgCb        = efhDefaultOnOrder;
    pNomRunCtx[i]->onEfhTradeMsgCb        = efhDefaultOnTrade;
    pNomRunCtx[i]->onEfhFeedDownMsgCb     = efhDefaultOnFeedDown;
    pNomRunCtx[i]->onEfhFeedUpMsgCb       = efhDefaultOnFeedUp;
    pNomRunCtx[i]->onEkaExceptionReportCb = efhDefaultOnException;

    pNomRunCtx[i]->efhRunUserData = (EfhRunUserData) pNomEfhCtx;
  }
  pNomEfhCtx->dontPrintTobUpd = true;
  pNomEfhCtx->printQStatistics = true;
  //##########################################################
  // GEM INIT
  //##########################################################
  EkaProp efhGemInitCtxEntries[] = {

    {"efh.GEM_TQF.group.0.mcast.addr","233.54.12.164:18000"},
    {"efh.GEM_TQF.group.1.mcast.addr","233.54.12.164:18001"},
    {"efh.GEM_TQF.group.2.mcast.addr","233.54.12.164:18002"},
    {"efh.GEM_TQF.group.3.mcast.addr","233.54.12.164:18003"},

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
  EkaProps ekaPropsGem = {
    .numProps = std::size(efhGemInitCtxEntries),
    .props = efhGemInitCtxEntries
  };

  const EfhInitCtx efhGemInitCtx = {
    .ekaProps = &ekaPropsGem,
    .numOfGroups = 4,
    .coreId = 0,
    .recvSoftwareMd = true,
    //    .full_book = false,
    .subscribe_all = false,
    //    .feed_ver = EfhFeedVer::kGEMX
  };

  const EfhInitCtx* pEfhGemInitCtx = &efhGemInitCtx;
  //---------------------------------------------------------
  EfhRunCtx gemRunCtx = {};
  const EkaGroup gemGroups[] = {
    {EkaSource::kGEM_TQF, (EkaLSI)0},
    {EkaSource::kGEM_TQF, (EkaLSI)1},
    {EkaSource::kGEM_TQF, (EkaLSI)2},
    {EkaSource::kGEM_TQF, (EkaLSI)3}
  };
  EfhRunCtx* pGemRunCtx = &gemRunCtx;
  pGemRunCtx->numGroups = std::size(gemGroups);
  pGemRunCtx->groups = gemGroups;

  pGemRunCtx->onEfhQuoteMsgCb        = efhDefaultOnQuote;
  pGemRunCtx->onEfhDefinitionMsgCb   = efhDefaultOnDefinition;
  pGemRunCtx->onEfhOrderMsgCb        = efhDefaultOnOrder;
  pGemRunCtx->onEfhTradeMsgCb        = efhDefaultOnTrade;
  pGemRunCtx->onEfhFeedDownMsgCb     = efhDefaultOnFeedDown;
  pGemRunCtx->onEfhFeedUpMsgCb       = efhDefaultOnFeedUp;
  pGemRunCtx->onEkaExceptionReportCb = efhDefaultOnException;

  efhInit(&pGemEfhCtx,pEkaDev,pEfhGemInitCtx);
  pGemRunCtx->efhRunUserData = (EfhRunUserData) pGemEfhCtx;

  pGemEfhCtx->dontPrintTobUpd = true;
  pGemEfhCtx->printQStatistics = true;

  //##########################################################
  // Definitions Phase
  //##########################################################

  // NOM
  for (uint8_t i = 0; i < NOM_GROUPS; i++) { 
    printf ("################ NOM Definitions Group %u ################\n",i);
    efhGetDefs(pNomEfhCtx, pNomRunCtx[i], (EkaGroup*)&pNomRunCtx[i]->groups[0]);
    printf ("NOM_ITTO GR%2u: subscribed securities: %u\n",i,pEkaDev->fh[0]->b_gr[i]->numSecurities);
  }

  //##########################################################

  // PHLX
  for (uint8_t i = 0; i < pPhlxRunCtx->numGroups / 2; i++) { // half groups are trades
    printf ("################ PHLX Definitions Group %u ################\n",i);
    efhGetDefs(pPhlxEfhCtx, pPhlxRunCtx, (EkaGroup*)&pPhlxRunCtx->groups[i]);
    printf ("PHLX_TOPO GR%2u: subscribed securities: %u\n",i,pEkaDev->fh[0]->b_gr[i]->numSecurities);
  }

  //##########################################################

  // GEM
  for (uint8_t i = 0; i < pGemRunCtx->numGroups; i++) { 
    printf ("################ GEM Definitions Group %u ################\n",i);
    efhGetDefs(pGemEfhCtx, pGemRunCtx, (EkaGroup*)&pGemRunCtx->groups[i]);
    printf ("GEM_TQF GR%2u: subscribed securities: %u\n",i,pEkaDev->fh[0]->b_gr[i]->numSecurities);
  }

  //##########################################################
  // FH Run Phase
  //##########################################################

  std::thread nomRunThread[NOM_GROUPS];
  for (uint8_t i = 0; i < NOM_GROUPS; i++) {
    printf ("################ NOM EfhRunGroup %u ################\n",i);
    nomRunThread[i] = std::thread(efhRunGroups,pNomEfhCtx, pNomRunCtx[i]);
    nomRunThread[i].detach();
    sleep (60);
  }

  printf ("################ GEM EfhRunGroups ################\n");
  std::thread gemRunThread = std::thread(efhRunGroups,pGemEfhCtx, pGemRunCtx);
  gemRunThread.detach();
  sleep (60);

  printf ("################ PHLX EfhRunGroups ################\n");
  std::thread phlxRunThread = std::thread(efhRunGroups,pPhlxEfhCtx, pPhlxRunCtx);
  phlxRunThread.detach();

  while (keep_work) usleep(0);

  efhDestroy(pPhlxEfhCtx);
  efhDestroy(pNomEfhCtx);
  efhDestroy(pGemEfhCtx);
  ekaDevClose(pEkaDev);

  printf ("Exitting normally...\n");

  return 0;
}

