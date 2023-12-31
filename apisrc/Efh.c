#include <assert.h>
#include <mutex>
#include <stdio.h>

#include "Efh.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaFhGroup.h"
#include "EkaFhRunGroup.h"
#include "eka_macros.h"

#include "EkaFhBats.h"
#include "EkaFhBox.h"
#include "EkaFhBx.h"
#include "EkaFhCme.h"
#include "EkaFhGem.h"
#include "EkaFhMiax.h"
#include "EkaFhMrx2Top.h"
#include "EkaFhNasdaq.h"
#include "EkaFhNom.h"
#include "EkaFhPhlxOrd.h"
#include "EkaFhPhlxTopo.h"
#include "EkaFhPlr.h"
#include "EkaFhXdp.h"

extern EkaDev *g_ekaDev;

EkaOpResult efhInit(EfhCtx **ppEfhCtx, EkaDev *pEkaDev,
                    const EfhInitCtx *pEfhInitCtx) {
  assert(ppEfhCtx != NULL);
  assert(pEkaDev != NULL);
  assert(pEfhInitCtx != NULL);
  EkaDev *dev = pEkaDev;

  if (dev->core[pEfhInitCtx->coreId] == NULL) {
    on_error(
        "I/F %d is not connected or does not have IP addr",
        pEfhInitCtx->coreId);
  }

  if (!pEfhInitCtx->recvSoftwareMd) {
    EKA_LOG("skipping creating FH due to "
            "pEfhInitCtx->recvSoftwareMd == false");
    return EKA_OPRESULT__OK;
  }

  *ppEfhCtx = (EfhCtx *)calloc(1, sizeof(EfhCtx));
  assert(*ppEfhCtx != NULL);

  (*ppEfhCtx)->dev = pEkaDev;

  EkaSource exch =
      EFH_GET_SRC(pEfhInitCtx->ekaProps->props[0].szKey);

  uint8_t fhId = dev->numFh;
  dev->numFh = fhId + 1; //  warning: ‘++’ expression of
                         //  ‘volatile’-qualified type is
                         //  deprecated [-Wvolatile]
  (*ppEfhCtx)->fhId = fhId;

  EKA_LOG("Creating FH[%u] %s", fhId,
          EKA_EXCH_SOURCE_DECODE(exch));

  if (dev->fh[fhId] != NULL)
    on_error("dev->fh[%u] is already inited", fhId);

  switch (getEkaSourceType(exch)) {
  case EkaSourceType::kNASDAQ_ITTO:
    dev->fh[fhId] = new EkaFhNom();
    break;
  case EkaSourceType::kNASDAQ_TQF:
    dev->fh[fhId] = new EkaFhGem();
    break;
  case EkaSourceType::kNASDAQ_TOPO:
    dev->fh[fhId] = new EkaFhPhlxTopo();
    break;
  case EkaSourceType::kNASDAQ_PHLX_ORD:
    dev->fh[fhId] = new EkaFhPhlxOrd();
    break;
  case EkaSourceType::kMIAX_TOM:
    dev->fh[fhId] = new EkaFhMiax();
    break;
  case EkaSourceType::kCBOE_PITCH:
    dev->fh[fhId] = new EkaFhBats();
    break;
  case EkaSourceType::kNYSE_XDP:
    dev->fh[fhId] = new EkaFhXdp();
    break;
  case EkaSourceType::kNYSE_PLR:
    dev->fh[fhId] = new EkaFhPlr();
    break;
  case EkaSourceType::kBOX_HSVF:
    dev->fh[fhId] = new EkaFhBox();
    break;
  case EkaSourceType::kCME_SBE:
    dev->fh[fhId] = new EkaFhCme();
    break;
  case EkaSourceType::kNASDAQ_BX_DPTH:
    dev->fh[fhId] = new EkaFhBx();
    break;
  case EkaSourceType::kNASDAQ_MRX_TOP2:
    dev->fh[fhId] = new EkaFhMrx2Top();
    break;
  default:
    on_error("Invalid Exchange %s from: %s",
             EKA_EXCH_DECODE(exch),
             pEfhInitCtx->ekaProps->props[0].szKey);
  }
  assert(dev->fh[fhId] != NULL);
  dev->fh[fhId]->setId(*ppEfhCtx, exch, fhId);
  dev->fh[fhId]->openGroups(*ppEfhCtx, pEfhInitCtx);
  if (EkaOpResult r =
          dev->fh[fhId]->init(pEfhInitCtx, fhId)) {
    return r;
  }

  EKA_LOG(
      "Created %s with fhId=%u ppEfhCtx=%p, *ppEfhCtx=%p",
      EKA_EXCH_DECODE(exch), fhId, ppEfhCtx, *ppEfhCtx);

  auto swStatistics = eka_read(dev, SW_STATISTICS);
  eka_write(dev, SW_STATISTICS,
            swStatistics | (1ULL << 62));

  return EKA_OPRESULT__OK;
}

EkaOpResult efhDestroy(EfhCtx *pEfhCtx) {
  auto dev = g_ekaDev;
  if (!dev)
    on_error("!dev");
  EKA_LOG("Destroying all EFHs!");
  for (auto i = 0; i < dev->numRunGr; i++)
    if (dev->runGr[i]) {
      dev->runGr[i]->thread_active = false;
    }

  return EKA_OPRESULT__OK;
}

/**
 * This will return the supported parameters that are
 * supported in this implementation. they szKey values in
 * pEntries are the names of the supported keys, and szValue
 * will contain some documentation about each parameter.
 *
 * @return This will return a filled out EfhInitCtx.
 */
const EfhInitCtx *efhGetSupportedParams() { return NULL; }

/**
 * This will tell ekaline that the application is ready to
 * receive data from a multicast group. Calling this will
 * perform any gap filling necessary.  This should be called
 * both for when we first want to start receiving data as
 * well as after a gap has been signaled and we are ready to
 * start recovering that gap and receiving data again. This
 * function should not return until we shut the Ekaline
 * system down via ekaClose() or receiving a Gap.
 *
 * @param pEfhCtx
 * This must be called after efhDoneStaticSubscriptions().
 *
 * @param groupNum This is the number of the group that we
 * want this to apply to.
 * @retval [See EkaOpResult].
 * @param pEfhRunCtx This is a pointer to the callbacks (and
 * possibly other information needed) that will be called as
 * the Ekaline feedhandler processes messages.
 *
 */

EkaOpResult efhRunGroups(EfhCtx *pEfhCtx,
                         const EfhRunCtx *pEfhRunCtx,
                         void **retval) {
  assert(pEfhCtx != NULL);
  assert(pEfhRunCtx != NULL);
  assert(pEfhCtx->dev->fh[pEfhCtx->fhId] != NULL);

  EkaDev *dev = pEfhCtx->dev;
  dev->mtx.lock();
  uint runGrId = dev->numRunGr;
  dev->numRunGr =
      runGrId +
      1; // warning: ‘++’ expression of ‘volatile’-qualified
         // type is deprecated [-Wvolatile]
  dev->runGr[runGrId] =
      new EkaFhRunGroup(pEfhCtx, pEfhRunCtx, runGrId);
  assert(dev->runGr[runGrId] != NULL);
  dev->mtx.unlock();

  auto fh = pEfhCtx->dev->fh[pEfhCtx->fhId];
  if (fh == NULL)
    on_error("fh == NULL");

  EKA_LOG("invoking runGroups: pEfhCtx->fhId = %u, "
          "fh->id = %u, runId = %u",
          pEfhCtx->fhId, fh->id, runGrId);

  //  return
  //  (pEfhCtx->dev->fh[pEfhCtx->fhId])->runGroups(pEfhCtx,
  //  pEfhRunCtx, runGrId);
  return fh->runGroups(pEfhCtx, pEfhRunCtx, runGrId);
}

/**
 * This function prints out state of internal resources
 * currently used by FH Group Used for tests and diagnostics
 * only
 */

EkaOpResult efhMonitorFhGroupState(EfhCtx *pEfhCtx,
                                   EkaGroup *group,
                                   bool verbose) {
  assert(pEfhCtx != NULL);
  assert(group->localId <
         pEfhCtx->dev->fh[pEfhCtx->fhId]->groups);

  EkaFhGroup *gr =
      pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[group->localId];

  EKA_LOG("%s:%u: gapNum=%4u, seq=%10ju",
          EKA_EXCH_DECODE(group->source), group->localId,
          gr->gapNum, gr->expected_sequence);

  if (verbose) {
    gr->print_q_state();
    gr->printBookState();
  }

  return EKA_OPRESULT__OK;
}

/**
 * This function will stop all internal loops in the ekaline
 Fh.
 * Usually usefull in the INThandlers for graceful app
 termination. It does not replace ekaClose().
 *
 * @param pEfhCtx

 * @retval [See EkaOpResult].
 */
EkaOpResult efhStopGroups(EfhCtx *pEfhCtx) {
  assert(pEfhCtx != NULL);
  assert(pEfhCtx->dev != NULL);
  if (pEfhCtx->dev->fh[pEfhCtx->fhId] != NULL)
    pEfhCtx->dev->fh[pEfhCtx->fhId]->stop();

  return EKA_OPRESULT__OK;
}
/**
 * This function will play back all EfhOptionDefinitionMsg
 * to our callback from EfhRunCtx.  This function will block
 * until the last callback has returned.
 *
 * @param pEfhCtx
 * @retval [See EkaOpResult].
 */
EkaOpResult efhGetDefs(EfhCtx *pEfhCtx,
                       const struct EfhRunCtx *pEfhRunCtx,
                       const EkaGroup *group,
                       void **retval) {
  assert(pEfhCtx != NULL && group != NULL);

  return pEfhCtx->dev->fh[pEfhCtx->fhId]->getDefinitions(
      pEfhCtx, pEfhRunCtx, group);
  //  eka_fh_request_group_definitions(pEfhCtx, pEfhRunCtx,
  //  group); return EKA_OPRESULT__OK;
}

/**
 * This function will tell the Ekaline feedhandler that we
 * are interested in updates for a specific static security.
 * This function must be called before
 * efhDoneStaticSubscriptions().
 *
 * @param pEfhCtx
 * @param securityId      This is the exchange specific
 * value of the security.
 *                        $$TODO$$ - Should this be an
 * arbitrary handle?
 * @param efhSecurityType This is a type of EfhSecurityType.
 * @param efhUserData     This is the value that we want
 * returned to us with every marketdata update for this
 * security.
 * @retval [See EkaOpResult].
 */

EkaOpResult efhSubscribeStatic(
    EfhCtx *pEfhCtx, const EkaGroup *group,
    uint64_t securityId, EfhSecurityType efhSecurityType,
    EfhSecUserData efhSecUserData, uint64_t opaqueAttrA,
    uint64_t opaqueAttrB) {
  assert(pEfhCtx != NULL);

  return pEfhCtx->dev->fh[pEfhCtx->fhId]
      ->subscribeStaticSecurity(
          group->localId, securityId, efhSecurityType,
          efhSecUserData, opaqueAttrA, opaqueAttrB);
}

/**
 * In past was: Call this function when you are done with
 * all your efhSubscribeStatic() calls. NOT NEEDED FOR EFH
 * !!!
 */
/* EkaOpResult efhDoneStaticSubscriptions(EfhCtx* pEfhCtx) {
 */
/*   return EKA_OPRESULT__OK; */
/* } */

/**
 * This is just like efhSubscribeStatic()
 */

EkaOpResult efhSubscribeDynamic(
    EfhCtx *pEfhCtx, const EkaGroup *group,
    uint64_t securityId, EfhSecurityType efhSecurityType,
    EfhSecUserData efhSecUserData, uint64_t opaqueAttrA,
    uint64_t opaqueAttrB) {
  assert(pEfhCtx != NULL);

  return pEfhCtx->dev->fh[pEfhCtx->fhId]
      ->subscribeStaticSecurity(
          group->localId, securityId, efhSecurityType,
          efhSecUserData, opaqueAttrA, opaqueAttrB);
}

EkaOpResult efhSetTradeTimeCtx(EfhCtx *pEfhCtx,
                               void *tradeTimeCtx) {
  return pEfhCtx->dev->fh[pEfhCtx->fhId]->setTradeTimeCtx(
      tradeTimeCtx);
}
