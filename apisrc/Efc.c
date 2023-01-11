#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>
#include <byteswap.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <linux/sockios.h>
#include <errno.h>
#include <thread>

#include "Efh.h"
#include "Efc.h"
#include "EkaCtxs.h"
#include "EkaDev.h"
#include "EkaSnDev.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "EkaEfc.h"

#include "EkaEfcDataStructs.h"

/**
 * This will initialize the Ekaline firing controller.
 *
 * @oaram pEfcCtx     This is an output parameter that will contain an initialized EfhCtx if this 
 *                    function is successful.
 * @param pEfcInitCtx This is a list of key value parameters that replaces the old eka_conf values 
 *                    that were passed in the old api.  
 * @return This will return an appropriate EkalineOpResult indicating success or an error code.
 */

int printSecCtx(EkaDev* dev, const SecCtx* msg);
int printMdReport(EkaDev* dev, const EfcMdReport* msg);
int printControllerStateReport(EkaDev* dev, const EfcControllerState* msg);
int printBoeFire(EkaDev* dev,const BoeNewOrderMsg* msg);

EkaOpResult efcInit( EfcCtx** ppEfcCtx, EkaDev *pEkaDev, const EfcInitCtx* pEfcInitCtx ) {
  if (pEkaDev == NULL) on_error("pEkaDev == NULL");

  EkaDev* dev = pEkaDev;

  if (! dev->epmEnabled) {
    EKA_WARN("This SW instance cannot run EFC functionality. Check other Ekaline processes running (or suspended) on this machine.");
    return EKA_OPRESULT__ERR_EFC_DISABLED;
  }

  //  *ppEfcCtx = (EfcCtx*)malloc(sizeof(EfcCtx));
  if (pEfcInitCtx == NULL) on_error("pEfcInitCtx == NULL");
  *ppEfcCtx = new EfcCtx;
  if (*ppEfcCtx == NULL) on_error("*ppEfcCtx == NULL");
  
  (*ppEfcCtx)->dev = dev;
  dev->efcFeedVer  = pEfcInitCtx->feedVer;

  return EKA_OPRESULT__OK;
}

/**
 * This will initialize the Ekaline firing controller.
 *
 * @oaram efcCtx     This is an output parameter that will contain an initialized EfhCtx if this 
 *                    function is successful.
 * @param efcInitCtx This is a list of key value parameters that replaces the old d values 
 *                    that were passed in the old api.  
 * @return This will return an appropriate EkalineOpResult indicating success or an error code.
 */

EkaOpResult efcInitStrategy( EfcCtx* pEfcCtx, const EfcStratGlobCtx* efcStratGlobCtx ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  efc->initStrategy(efcStratGlobCtx);

  EKA_LOG("Initializing EFC global params");

  return EKA_OPRESULT__OK;
}

/**
 * This will enable or disable the firing controller
 *
 * @param pEfcCtx
 * @param primaryCoreId If this is >= 0, then it will enable firing on all cores, and make 
 *                      primaryCore the only core that will fire if the opportunity should only
 *                      only fired on once.
 *                      If this is < 0, then this will disable firing on all cores.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcEnableController( EfcCtx* pEfcCtx, EkaCoreId primaryCoreId,  EfcArmVer ver ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  if (primaryCoreId < 0) efc->disArmController();
  else efc->armController(ver);

  /* assert (pEfcCtx != NULL); */
  /* download_conf2hw(pEfcCtx->dev); */
  /* if (primaryCoreId < 0) eka_arm_controller(pEfcCtx->dev, 0); */
  /* eka_arm_controller(pEfcCtx->dev, 1); */
  return EKA_OPRESULT__OK;
}

/**
 * This will tell the hardware to consider firing on md updates for a list of securities.
 * This function can only be called once after efcInit and must be called before efcRun().
 * 
 * @param pEfcCtx
 * @param pSecurityIds   This is a pointer to the first member of an array of securities that 
 *                       the firing controller should consider as opportunities.  This value
 *                       should be the exchange specific security id that is returned from 
 *                       EfhOptionDefinitionMsg.
 * @param numSecurityIds This is the number of elements in the array pSecurityIds.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcEnableFiringOnSec( EfcCtx* pEfcCtx, const uint64_t* pSecurityIds, size_t numSecurityIds ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  if (pSecurityIds == NULL) on_error("pSecurityIds == NULL");

  uint64_t* p = (uint64_t*) pSecurityIds;
  for (uint i = 0; i < numSecurityIds; i++) {
    //    EKA_LOG("Subscribing on 0x%jx",*p);
    efc->subscribeSec(*p);
    p++;
  }
  efc->downloadTable();

  uint64_t subscrCnt = (numSecurityIds << 32) | efc->numSecurities;
  eka_write(dev,SCRPAD_EFC_SUBSCR_CNT,subscrCnt);

  EKA_LOG("Tried to subscribe on %u, succeeded on %d",
	  (uint)numSecurityIds,efc->numSecurities);
  
  return EKA_OPRESULT__OK;
}

/**
 * This function will take a security that was passed to efcEnableFiringOnSec() and return
 * the corresponding EfcSecCtxHandle.  It must be  called after efcEnableFiringOnSec() but 
 * before efcRun().
 *
 * @param pEfcCtx
 * @param securityId This is a security id that was passed to efcEnableFiringOnSec.
 * @retval [>=0] On success this will return an a value to be interpreted as an EfcSecCtxHandle.
 * @retval [<0]  On failure this will return a value to be interpreted as an error EkaOpResult.
 */
EfcSecCtxHandle getSecCtxHandle( EfcCtx* pEfcCtx, uint64_t securityId ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  return efc->getSubscriptionId(securityId);
}

/**
 * This will set a SecCtx for a static security.
 *
 * @param pEfcCtx
 * @param efcSecCtxHandle This is a handle to the SecCtx that we are setting.
 * @param pSecCtx         This is a pointer to the local source SecCtx.
 * @param writeChan       This is the channel that will be used to write (see kEkaNumWriteChans).
 * @retval [See EkaOpResult].
 */
EkaOpResult efcSetStaticSecCtx( EfcCtx* pEfcCtx, EfcSecCtxHandle hSecCtx,
				const SecCtx* pSecCtx, uint16_t writeChan ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  if (writeChan >= EkaDev::MAX_CTX_THREADS)
    on_error("writeChan %u > EkaDev::MAX_CTX_THREADS %u",
	     writeChan,EkaDev::MAX_CTX_THREADS);
  
  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  if (hSecCtx < 0) 
#if EFC_CTX_SANITY_CHECK
    on_error("hSecCtx = %jd",hSecCtx);
#else
    return EKA_OPRESULT__ERR_EFC_SET_CTX_ON_UNSUBSCRIBED_SECURITY;
#endif
    
  if (pSecCtx == NULL) on_error("pSecCtx == NULL");

#if EFC_CTX_SANITY_CHECK
  if (static_cast<uint8_t>(efc->secIdList[hSecCtx] & 0xFF) != pSecCtx->lowerBytesOfSecId)
    on_error("EFC_CTX_SANITY_CHECK error: secIdList[%jd] 0x%016jx & 0xFF != %02x",
	     hSecCtx,efc->secIdList[hSecCtx],pSecCtx->lowerBytesOfSecId);
#endif
  
  // This copy is needed because EkaHwSecCtx is packed and SecCtx is not!
  const EkaHwSecCtx hwSecCtx = {
    .bidMinPrice       = pSecCtx->bidMinPrice,
    .askMaxPrice       = pSecCtx->askMaxPrice,
    .bidSize           = pSecCtx->bidSize,
    .askSize           = pSecCtx->askSize,
    .versionKey        = pSecCtx->versionKey,
    .lowerBytesOfSecId = pSecCtx->lowerBytesOfSecId
  };

  /* uint64_t ctxWrAddr = P4_CTX_CHANNEL_BASE +  */
  /*   writeChan * EKA_BANKS_PER_CTX_THREAD * EKA_WORDS_PER_CTX_BANK * 8 +  */
  /*   efc->ctxWriteBank[writeChan] * EKA_WORDS_PER_CTX_BANK * 8; */

  /* // EkaHwSecCtx is 8 Bytes ==> single write */
  /* eka_write(dev,ctxWrAddr,*(uint64_t*)&hwSecCtx);  */

  /* union large_table_desc done_val = {}; */
  /* done_val.ltd.src_bank           = efc->ctxWriteBank[writeChan]; */
  /* done_val.ltd.src_thread         = writeChan; */
  /* done_val.ltd.target_idx         = hSecCtx; */
  /* eka_write(dev, P4_CONFIRM_REG, done_val.lt_desc); */

  /* efc->ctxWriteBank[writeChan] = (efc->ctxWriteBank[writeChan] + 1) % EKA_BANKS_PER_CTX_THREAD; */

  efc->writeSecHwCtx(hSecCtx,&hwSecCtx,writeChan);
  
  return EKA_OPRESULT__OK;
}

/**
 * This is just like setStaticSecCtx except it will be for dynamic securities.
 */
EkaOpResult efcSetDynamicSecCtx( EfcCtx* pEfcCtx, EfcSecCtxHandle hSecCtx, const SecCtx* pSecCtx, uint16_t writeChan ) {
  assert (pEfcCtx != NULL);
  assert (pSecCtx != NULL);

  return EKA_OPRESULT__OK;
}

/**
 * This is just like setStaticSectx except it is for SesCtxs.
 */
EkaOpResult efcSetSesCtx( EfcCtx* pEfcCtx, ExcConnHandle hConn, const SesCtx* pSesCtx) {
  on_error("This function is obsolete. Use efcSetFireTemplate()");
  /* assert (pEfcCtx != NULL); */
  /* assert (pSesCtx != NULL); */

  /* assert (pEfcCtx != NULL); */
  //  session_fire_app_ctx_t ctx = {};
  /* struct session_fire_app_ctx ctx = {}; */
  /* ctx.clid = pSesCtx->clOrdId; */
  /* ctx.next_session = pSesCtx->nextSessionId; */

  /* auto feedVer = pEfcCtx->dev->hwFeedVer; */
  /* switch( feedVer ) { */
  /* case SN_MIAX: */
  /*   ctx.equote_mpid_sqf_badge = reinterpret_cast< const MeiSesCtx* >( pSesCtx )->mpid; */
  /*   break; */

  /* case SN_NASDAQ: */
  /* case SN_PHLX: */
  /* case SN_GEMX: */
  /*   ctx.equote_mpid_sqf_badge = reinterpret_cast< const SqfSesCtx* >( pSesCtx )->badge; */
  /*   break; */

  /* default: */
  /*   on_error( "Unsupported feed_ver: %d", feedVer ); */
  /* } */
  
  /* eka_set_session_fire_app_ctx(pEfcCtx->dev,hConn,&ctx); */

  return EKA_OPRESULT__OK;
}

/**
 * This sets the Fire Message template for the hConn session. The template must populate all
 * fields that are not managed by FPGA (fields managed by FPGA: size, price, etc.).
 * @param efcCtx
 * @param hConn          This is the ExcSessionId that we will be mapping to.
 * @param fireMsg        Application messge in Exchange specific format: SQF, eQuote, etc.
 * @param fireMsgSize    Size of the template
 * @retval [See EkaOpResult].
 */

EkaOpResult efcSetFireTemplate( EfcCtx* pEfcCtx, ExcConnHandle hConn, const void* fireMsg, size_t fireMsgSize ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  //  EkaDev* dev = pEfcCtx->dev;
  /* if (dev == NULL) on_error("dev == NULL"); */
  /* EkaEfc* efc = dev->efc; */
  /* if (efc == NULL) on_error("efc == NULL"); */

  /* efc->setActionPayload(hConn,fireMsg,fireMsgSize); */

  EKA_WARN("Obsolete function!!!");

  return EKA_OPRESULT__OK;
}

/**
 * This will tell the controller which Session to first fire on based on the multicast group that the 
 * opportunity arrived on.  
 * @param pEfcCtx
 * @param group       This is the multicast group that we will be mapping from.
 * @param hConnection This is the ExcSessionId that we will be mapping to.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcSetGroupSesCtx( EfcCtx* pEfcCtx, uint8_t group, ExcConnHandle hConn ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  /* EkaEfc* efc = dev->efc; */
  /* if (efc == NULL) on_error("efc == NULL"); */

  EkaCoreId coreId = excGetCoreId(hConn);
  if (dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId);

  uint sessId = excGetSessionId(hConn);
  if (dev->core[coreId]->tcpSess[sessId] == NULL)
    on_error("hConn 0x%x is not connected",hConn);

  //  efc->createFireAction(group,hConn);
  //  eka_set_group_session(pEfcCtx->dev, group, hConn);
  return EKA_OPRESULT__OK;

}

/* ****************************************
 * Declaring callbacks.
 * ****************************************/


/**
 * This function will run the ekaline Fh on the current thread and make callbacks for messages processed.
 * This function should not return until we shut the Ekaline system down via ekaClose().
 *
 * @param pEfcCtx 
 * @param pEfcRunCtx This is a pointer to the callbacks (and possibly other information needed) that
 *                   will be called as the Ekaline feedhandler processes messages.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcRun( EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");

  dev->pEfcRunCtx = new EfcRunCtx;
  if (!dev->pEfcRunCtx) on_error("fauiled on new EfcRunCtx");

  memcpy (dev->pEfcRunCtx,pEfcRunCtx,sizeof(*pEfcRunCtx));


  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[EFC_STRATEGY])};
  if (efc == NULL) on_error("efc == NULL");

  efc->run(pEfcCtx,pEfcRunCtx);

  //  eka_open_udp_sockets(pEfcCtx->dev);
  //  download_conf2hw(pEfcCtx->dev);

  //  efc_run(pEfcCtx,pEfcRunCtx);

  //  eka_write(dev,0xf0f00,0xefa0beda); // REMOVE!!!
  
  return EKA_OPRESULT__OK;
}


EkaOpResult efcSwKeepAliveSend( EfcCtx* pEfcCtx, int strategyId ) {
  if (!pEfcCtx)
    on_error("!pEfcCtx");
  EkaDev* dev = pEfcCtx->dev;
  if (!dev)
    on_error("!dev");
  auto efc {dynamic_cast<EkaEfc*>(dev->epm->strategy[strategyId])};
  if (!efc)
    on_error("!strategy[%d]",strategyId);

  eka_write(dev,0xf0608,0);
  return EKA_OPRESULT__OK;
}

EkaOpResult efcClose( EfcCtx* efcCtx ) {
  return EKA_OPRESULT__OK;
}
