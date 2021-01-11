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

#include "Efc.h"

#include "eka_data_structs.h"
#include "EkaDev.h"
#include "EkaSnDev.h"
#include "EkaCore.h"
#include "EkaTcpSess.h"
#include "EkaEfc.h"

/* void efc_run (EfcCtx* pEfcCtx, const EfcRunCtx* pEfcRunCtx); */
/* void download_conf2hw (EkaDev* dev); */
/* EkaOpResult efc_init ( EfcCtx** efcCtx, EkaDev *ekaCoreCtx, const EfcInitCtx* efcInitCtx ); */
/* void eka_arm_controller(EkaDev* dev, uint8_t arm); */
/* uint64_t eka_subscr_security2fire(EkaDev* dev, uint64_t id); */
/* int eka_init_strategy_params (EkaDev* dev,const struct global_params *params) ; */

/*   void download_subscr_table(EkaDev* dev, bool clear_all); */
/*   uint32_t get_subscription_id(EkaDev* dev, uint64_t id); */
/*   int eka_set_security_ctx_with_idx (EkaDev* dev, uint32_t ctx_idx, struct sec_ctx *ctx, uint8_t ch); */
/*   int eka_set_session_fire_app_ctx(EkaDev* dev, uint16_t sess_id , void *ctx); */
/*   int eka_set_group_session(EkaDev* dev, uint8_t gr, uint16_t sess_id); */
/*   int eka_open_udp_sockets(EkaDev* dev); */
/* //void  eka_set_core_addr(EkaDev* dev, uint8_t core, EkaCoreInitAttrs* attrs); */
/*   int eka_socket(EkaDev* dev, uint8_t core); */
/*   uint16_t eka_connect (EkaDev* dev, int sock, const struct sockaddr *dst, socklen_t addrlen); */
/*   ssize_t eka_send(EkaDev* dev, uint16_t sess_id, void *buf, size_t size); */
/*   ssize_t eka_recv(EkaDev* dev, uint16_t sess_id, void *buffer, size_t size); */
/* void print_new_compat_fire_report (EfcCtx* efcCtx, EfcReportHdr* p); */
/* int eka_socket_close(EkaDev* dev, uint16_t id); */


/**
 * This will initialize the Ekaline firing controller.
 *
 * @oaram pEfcCtx     This is an output parameter that will contain an initialized EfhCtx if this 
 *                    function is successful.
 * @param pEfcInitCtx This is a list of key value parameters that replaces the old eka_conf values 
 *                    that were passed in the old api.  
 * @return This will return an appropriate EkalineOpResult indicating success or an error code.
 */

EkaOpResult efcInit( EfcCtx** ppEfcCtx, EkaDev *pEkaDev, const EfcInitCtx* pEfcInitCtx ) {
  if (pEkaDev == NULL) on_error("pEkaDev == NULL");
  if (pEfcInitCtx == NULL) on_error("pEfcInitCtx == NULL");

  EkaDev* dev = pEkaDev;

  dev->efc = new EkaEfc(dev,dev->hwFeedVer,pEfcInitCtx);
  if (dev->efc == NULL) on_error("dev->efc == NULL");

  *ppEfcCtx = (EfcCtx*)malloc(sizeof(EfcCtx));
  if (*ppEfcCtx == NULL) on_error("*ppEfcCtx == NULL");
  (*ppEfcCtx)->dev = dev;

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
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  efc->initStrategy(efcStratGlobCtx);
  return EKA_OPRESULT__OK;
}

/**
 * This will enable or disable the firing controller
 *
 * @param pEfcCtx
 * @param primaryCoreId If this is >= 0, then it will enable firing on all cores, and make 
 *                      primaryCore the only core that will fire if he opportunity should only
 *                      only fired on once.
 *                      If this is < 0, then this will disable firing on all cores.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcEnableController( EfcCtx* pEfcCtx, EkaCoreId primaryCoreId ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  if (primaryCoreId < 0) efc->disArmController();
  else efc->armController();

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
 *                       EfhDefinitionMsg.
 * @param numSecurityIds This is the number of elements in the array pSecurityIds.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcEnableFiringOnSec( EfcCtx* pEfcCtx, const uint64_t* pSecurityIds, size_t numSecurityIds ) {
  if (pEfcCtx == NULL) on_error("pEfcCtx == NULL");
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  if (pSecurityIds == NULL) on_error("pSecurityIds == NULL");

  uint64_t* p = (uint64_t*) pSecurityIds;
  for (uint i = 0; i < numSecurityIds; i++) {
    efc->subscribeSec(*p);
    p++;
  }
  efc->downloadTable();

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
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  return (EfcSecCtxHandle) efc->getSubscriptionId(securityId);
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
EkaOpResult efcSetStaticSecCtx( EfcCtx* pEfcCtx, EfcSecCtxHandle hSecCtx, const SecCtx* pSecCtx, uint16_t writeChan ) {
  /* assert (pEfcCtx != NULL); */
  /* assert (pSecCtx != NULL); */

  /* struct sec_ctx ctx = {}; */
  /* ctx.bid_min_price = pSecCtx->bidMinPrice; */
  /* ctx.ask_max_price = pSecCtx->askMaxPrice; */
  /* ctx.size = pSecCtx->size; */
  /* ctx.ver_num = pSecCtx->verNum; */
  /* ctx.lower_bytes_of_sec_id = pSecCtx->lowerBytesOfSecId; */

  /* uint32_t ctx_idx = (uint32_t) (hSecCtx & 0xFFFFFFFF); */
 
  /* eka_set_security_ctx_with_idx (pEfcCtx->dev, ctx_idx, &ctx, (uint8_t) writeChan); */
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
  EkaDev* dev = pEfcCtx->dev;
  if (dev == NULL) on_error("dev == NULL");
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  efc->setActionPayload(hConn,fireMsg,fireMsgSize);

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
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  EkaCoreId coreId = excGetCoreId(hConn);
  if (dev->core[coreId] == NULL) on_error("dev->core[%u] == NULL",coreId);

  uint sessId = excGetSessionId(hConn);
  if (dev->core[coreId]->tcpSess[sessId] == NULL)
    on_error("hConn 0x%x is not connected",hConn);

  efc->createFireAction(group,hConn);
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
  EkaEfc* efc = dev->efc;
  if (efc == NULL) on_error("efc == NULL");

  efc->run();

  //  eka_open_udp_sockets(pEfcCtx->dev);
  //  download_conf2hw(pEfcCtx->dev);

  //  efc_run(pEfcCtx,pEfcRunCtx);

  return EKA_OPRESULT__OK;
}


EkaOpResult efcPrintFireReport( EfcCtx* pEfcCtx, EfcReportHdr* p ) {
  // PATCH
  //  print_new_compat_fire_report (pEfcCtx,p);
  return EKA_OPRESULT__OK;
}
