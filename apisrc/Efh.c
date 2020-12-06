#include <assert.h>
#include <stdio.h>
#include <mutex>

#include "Efh.h"
#include "eka_macros.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "eka_fh.h"
#include "EkaFhRunGr.h"

EkaOpResult efhInit( EfhCtx** ppEfhCtx, EkaDev* pEkaDev, const EfhInitCtx* pEfhInitCtx ) {
  assert (ppEfhCtx != NULL);
  assert (pEkaDev != NULL);
  assert (pEfhInitCtx != NULL);
  EkaDev* dev = pEkaDev;

  if (! pEfhInitCtx->recvSoftwareMd) {
    EKA_LOG("skipping creating FH due to pEfhInitCtx->recvSoftwareMd == false");
    return EKA_OPRESULT__OK;
  }

  *ppEfhCtx = (EfhCtx*) calloc(1, sizeof(EfhCtx));
  assert (*ppEfhCtx != NULL);

  (*ppEfhCtx)->dev  = pEkaDev;
  (*ppEfhCtx)->fhId = pEkaDev->numFh;

  EkaSource exch = EFH_GET_SRC(pEfhInitCtx->ekaProps->props[0].szKey);
  uint8_t fhId = dev->numFh++;
  switch (exch) {
  case EkaSource::kNOM_ITTO:
    dev->fh[fhId] = new FhNom();
    break;
  case EkaSource::kGEM_TQF:
  case EkaSource::kISE_TQF:
  case EkaSource::kMRX_TQF:
    dev->fh[fhId] = new FhGem();
    break;
  case EkaSource::kPHLX_TOPO:
    dev->fh[fhId] = new FhPhlx();
    break;
  case EkaSource::kPHLX_ORD:
    dev->fh[fhId] = new FhPhlxOrd();
    break;
  case EkaSource::kMIAX_TOM:
  case EkaSource::kPEARL_TOM:
    dev->fh[fhId] = new FhMiax();
    break;
  case EkaSource::kC1_PITCH:
  case EkaSource::kC2_PITCH:
  case EkaSource::kBZX_PITCH:
  case EkaSource::kEDGX_PITCH:
    dev->fh[fhId] = new FhBats();
    break;
  case EkaSource::kARCA_XDP:
  case EkaSource::kAMEX_XDP:
    dev->fh[fhId] = new FhXdp();
    break;
  case EkaSource::kBOX_HSVF:
    dev->fh[fhId] = new FhBox();
    break;
  default:
    on_error ("Invalid Exchange %s from: %s",EKA_EXCH_DECODE(exch),pEfhInitCtx->ekaProps->props[0].szKey);
  }
  assert (dev->fh[fhId] != NULL);
  dev->fh[fhId]->setId(*ppEfhCtx,exch,fhId);
  dev->fh[fhId]->openGroups(*ppEfhCtx,pEfhInitCtx);
  dev->fh[fhId]->init(pEfhInitCtx,fhId);

  return EKA_OPRESULT__OK;

}

EkaOpResult efhDestroy( EfhCtx* pEfhCtx ) {  
  assert (pEfhCtx != NULL);
  assert (pEfhCtx->dev != NULL);
  if (pEfhCtx->dev->fh[pEfhCtx->fhId] != NULL) pEfhCtx->dev->fh[pEfhCtx->fhId]->stop();
  return EKA_OPRESULT__OK;
}


/**
 * This will return the supported parameters that are supported in this implementation.
 * they szKey values in pEntries are the names of the supported keys, and szValue
 * will contain some documentation about each parameter.
 *
 * @return This will return a filled out EfhInitCtx.
 */
const EfhInitCtx* efhGetSupportedParams( ) {

  return NULL;
}

/**
 * This will tell ekaline that the application is ready to receive data from a multicast group.
 * Calling this will perform any gap filling necessary.  This should be called both for when we
 * first want to start receiving data as well as after a gap has been signaled and we are ready
 * to start recovering that gap and receiving data again.
 * This function should not return until we shut the Ekaline system down via ekaClose()
 * or receiving a Gap.
 *
 * @param pEfhCtx 
 * This must be called after efhDoneStaticSubscriptions().
 *
 * @param groupNum This is the number of the group that we want this to apply to.
 * @retval [See EkaOpResult].
 * @param pEfhRunCtx This is a pointer to the callbacks (and possibly other information needed) that
 *                   will be called as the Ekaline feedhandler processes messages.
 *
 */
class FhNomGr;

EkaOpResult efhRunGroups( EfhCtx* pEfhCtx, const EfhRunCtx* pEfhRunCtx ) {
  assert (pEfhCtx != NULL);
  assert (pEfhRunCtx != NULL);
  assert (pEfhCtx->dev->fh[pEfhCtx->fhId] != NULL);

  
  EkaDev* dev = pEfhCtx->dev;
  dev->mtx.lock();
  uint runGrId = dev->numRunGr++;
  dev->runGr[runGrId] = new FhRunGr(pEfhCtx,pEfhRunCtx,runGrId);
  assert (dev->runGr[runGrId] != NULL);
  dev->mtx.unlock();

  //  EKA_DEBUG("invoking runGroups with runId = %u",runGrId);
  return ((FhNom*)pEfhCtx->dev->fh[pEfhCtx->fhId])->runGroups(pEfhCtx, pEfhRunCtx, runGrId);

  return EKA_OPRESULT__ERR_NOT_IMPLEMENTED;
}

/**
 * This function prints out state of internal resources currently used by FH Group
 * Used for tests and diagnostics only
 */

EkaOpResult efhMonitorFhGroupState( EfhCtx* pEfhCtx, EkaGroup* group) { 
  assert (pEfhCtx != NULL);
  assert (group->localId < pEfhCtx->dev->fh[pEfhCtx->fhId]->groups);
  fprintf (stderr,"%s:%u: gapNum=%4u, seq=%10ju, ",
	   EKA_EXCH_DECODE(group->source),
	   group->localId,
	   pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[group->localId]->gapNum,
	   pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[group->localId]->expected_sequence
	   );
  //  uint8_t gr2monitor_q = group->source == EkaSource::kNOM_ITTO ? group->localId : 0;
  uint8_t gr2monitor_q = group->localId;

  pEfhCtx->dev->fh[pEfhCtx->fhId]->b_gr[gr2monitor_q]->print_q_state(); // tmp patch

  return EKA_OPRESULT__OK;
}


/**
 * This function will stop all internal loops in the ekaline Fh.
 * Usually usefull in the INThandlers for graceful app termination. It does not replace ekaClose().
 *
 * @param pEfhCtx 

 * @retval [See EkaOpResult].
 */
EkaOpResult efhStopGroups( EfhCtx* pEfhCtx ) {
  assert (pEfhCtx != NULL);
  assert (pEfhCtx->dev != NULL);
  if (pEfhCtx->dev->fh[pEfhCtx->fhId] != NULL) pEfhCtx->dev->fh[pEfhCtx->fhId]->stop();

  return EKA_OPRESULT__OK;
}
/**
 * This function will play back all EfhDefinitionMsg to our callback from EfhRunCtx.  This function will
 * block until the last callback has returned.
 *
 * @param pEfhCtx 
 * @retval [See EkaOpResult].
 */
EkaOpResult efhGetDefs( EfhCtx* pEfhCtx, const struct EfhRunCtx* pEfhRunCtx, EkaGroup* group) {
  assert (pEfhCtx != NULL);

  return pEfhCtx->dev->fh[pEfhCtx->fhId]->getDefinitions(pEfhCtx, pEfhRunCtx, group);
  //  eka_fh_request_group_definitions(pEfhCtx, pEfhRunCtx, group);
  //  return EKA_OPRESULT__OK;
}

/**
 * This function will tell the Ekaline feedhandler that we are interested in updates for a specific static
 * security.  This function must be called before efhDoneStaticSubscriptions().
 *
 * @param pEfhCtx 
 * @param securityId      This is the exchange specific value of the security.  
 *                        $$TODO$$ - Should this be an arbitrary handle?
 * @param efhSecurityType This is a type of EfhSecurityType.
 * @param efhUserData     This is the value that we want returned to us with every
 *                        marketdata update for this security.
 * @retval [See EkaOpResult].
 */

/* EkaOpResult efhSubscribeStatic( EfhCtx* pEfhCtx, EkaGroup* group, uint64_t securityId, EfhSecurityType efhSecurityType,EfhSecUserData efhSecUserData) { */
/*   assert (pEfhCtx != NULL); */

/*   return pEfhCtx->dev->fh[pEfhCtx->fhId]->subscribeStaticSecurity(group->localId, securityId, efhSecurityType, efhSecUserData); */
/*   //  return eka_fh_subscribe_static(pEfhCtx, group->localId, securityId, efhSecurityType, efhSecUserData); */
/* } */

EkaOpResult efhSubscribeStatic( EfhCtx* pEfhCtx, EkaGroup* group, uint64_t securityId, EfhSecurityType efhSecurityType,EfhSecUserData efhSecUserData,uint64_t opaqueAttrA,uint64_t opaqueAttrB) {
  assert (pEfhCtx != NULL);

  return pEfhCtx->dev->fh[pEfhCtx->fhId]->subscribeStaticSecurity(group->localId, securityId, efhSecurityType, efhSecUserData,opaqueAttrA,opaqueAttrB);
  //  return eka_fh_subscribe_static(pEfhCtx, group->localId, securityId, efhSecurityType, efhSecUserData);
}

/**
 * In past was: Call this function when you are done with all your efhSubscribeStatic() calls.
 * NOT NEEDED FOR EFH !!!
 */
/* EkaOpResult efhDoneStaticSubscriptions(EfhCtx* pEfhCtx) { */
/*   return EKA_OPRESULT__OK; */
/* } */

/**
 * This is just like efhSubscribeStatic() except it is for dynamic securities.
 * This must be called after efhDoneStaticSubscriptions().
 */
EkaOpResult efhSubscribeDynamic(EfhCtx* pEfhCtx, uint64_t securityId, EfhSecurityType efhSecurityType, EfhSecUserData efhSecUserData ) {
  assert (pEfhCtx != NULL);

  return EKA_OPRESULT__ERR_NOT_IMPLEMENTED;
}

/**
 * This function will run the ekaline Fh on the current thread and make callbacks for messages processed.
 * This function should not return until we shut the Ekaline system down via ekaClose().
 *
 * @param pEfhCtx 
 * @param pEfhRunCtx This is a pointer to the callbacks (and possibly other information needed) that
 *                   will be called as the Ekaline feedhandler processes messages.
 * @retval [See EkaOpResult].
 */
