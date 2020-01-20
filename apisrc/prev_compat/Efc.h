/*
 * Efc.h
 *     This header file covers the api for the Ekaline firing controller.
 */

#ifndef __EKALINE_EFC_H__
#define __EKALINE_EFC_H__

#include "Eka.h"
#include "EfcCtxs.h"
#include "EfcMsgs.h"
#include "Exc.h"
#include "Efh.h"

#ifdef __cplusplus
    extern "C" {
#endif

/**
 * This is passed to EfhInit().  This will replace the eka_conf values that we passed in the old api.  
 * This function must be called before efcInit().
 */
struct EfcInitCtx {
    EkaProps* ekaProps;

    /** This should be a pointer to a valid EfhCtx created by efhInit(). */
    EfhCtx* efhCtx;

   /* This is true if we expect to receive marketdata updates when we run efhRun().  If this is false, 
    * we dont expect updates, and so Ekaline can save memory and avoid creating structures that arent needed.  */
};

/**
 * This will initialize the Ekaline firing controller.
 *
 * @oaram efcCtx     This is an output parameter that will contain an initialized EfhCtx* if
 *                   this function is successful.
 * @param ekaCoreCtx The core that the newly-created EfhCtx object will be associated with
 * @param efcInitCtx This is a list of key value parameters that replaces the old d values 
 *                    that were passed in the old api.  
 * @return This will return an appropriate EkalineOpResult indicating success or an error code.
 */
EkaOpResult efcInit( EfcCtx** efcCtx, EkaCtx *ekaCoreCtx, const EfcInitCtx* efcInitCtx );

/**
 * This will initialize global params of the Efc strategy
 *
 * @param efcCtx
 * @param EfcStratGlobCtx 
 * @retval [See EkaOpResult].
 */

EkaOpResult efcInitStrategy( EfcCtx* efcCtx, const EfcStratGlobCtx* efcStratGlobCtx );

/**
 * This will enable or disable the firing controller
 *
 * @param efcCtx
 * @param primaryCoreId If this is >= 0, then it will enable firing on all cores, and make 
 *                      primaryCore the only core that will fire if he opportunity should only
 *                      only fired on once.
 *                      If this is < 0, then this will disable firing on all cores.
 * @retval [See EkaOpResult].
 */

EkaOpResult efcEnableController( EfcCtx* efcCtx, EkaCoreId primaryCoreId );

/**
 * This will tell the hardware to consider firing on md updates for a list of securities.
 * This function can only be called once after efcInit and must be called before efcRun().
 * 
 * @param efcCtx
 * @param securityIds    This is a pointer to the first member of an array of securities that 
 *                       the firing controller should consider as opportunities.  This value
 *                       should be the exchange specific security id that is returned from 
 *                       EfhDefinitionMsg.
 * @param numSecurityIds This is the number of elements in the array securityIds.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcEnableFiringOnSec( EfcCtx* efcCtx, const uint64_t* securityIds, size_t numSecurityIds );

/**
 * This function will take a security that was passed to efcEnableFiringOnSec() and return
 * the corresponding EfcSecCtxHandle.  It must be  called after efcEnableFiringOnSec() but 
 * before efcRun().
 *
 * @param efcCtx
 * @param securityId This is a security id that was passed to efcEnableFiringOnSec.
 * @retval [>=0] On success this will return an a value to be interpreted as an EfcSecCtxHandle.
 * @retval [<0]  On failure this will return a value to be interpreted as an error EkaOpResult.
 */
EfcSecCtxHandle getSecCtxHandle( EfcCtx* efcCtx, uint64_t securityId );

/**
 * This will set a SecCtx for a static security.
 *
 * @param efcCtx
 * @param efcSecCtxHandle This is a handle to the SecCtx that we are setting.
 * @param secCtx          This is a pointer to the local source SecCtx.
 * @param writeChan       This is the channel that will be used to write (see kEkaNumWriteChans).
 * @retval [See EkaOpResult].
 */
EkaOpResult efcSetStaticSecCtx( EfcCtx* efcCtx, EfcSecCtxHandle hSecCtx, const SecCtx* secCtx, uint16_t writeChan );

/**
 * This is just like setStaticSecCtx except it will be for dynamic securities.
 */
EkaOpResult efcSetDynamicSecCtx( EfcCtx* efcCtx, EfcSecCtxHandle hSecCtx, const SecCtx* secCtx, uint16_t writeChan );

/**
 * This is just like setStaticSectx except it is for SesCtxs.
 */
EkaOpResult efcSetSesCtx( EfcCtx* efcCtx, ExcConnHandle hConn, const SesCtx* sesCtx );

/**
 * This will tell the controller which Session to first fire on based on the multicast group that the 
 * opportunity arrived on.  
 * @param efcCtx
 * @param group  This is the multicast group that we will be mapping from.
 * @param hConn  This is the ExcSessionId that we will be mapping to.
 * @retval [See EkaOpResult].
 */

EkaOpResult efcSetGroupSesCtx( EfcCtx* efcCtx, uint8_t group, ExcConnHandle hConn );

/**
 * This will print Efc Fire Report -- used for Ekaline tests
 * @retval [See EkaOpResult].
 */

EkaOpResult efcPrintFireReport( EfcCtx* efcCtx, EfcReportHdr* p );

/* ****************************************
 * Declaring callbacks.
 * ****************************************/

typedef 
  void 
  ( *OnEkaExceptionReportCb )
  ( 
    EfcCtx*                   efcCtx, 
    const EkaExceptionReport* ekaExceptionReport, 
    size_t size 
  );

typedef 
  void 
  ( *OnEfcFireReportCb )
  ( 
    EfcCtx*                   efcCtx, 
    const EfcFireReport*      efcFireReport,
    size_t size 
  );

/*
 * 
 */
struct EfcRunCtx {
    /** These can be either fires or exceptions. */
  OnEkaExceptionReportCb onEkaExceptionReportCb; 
  OnEfcFireReportCb      onEfcFireReportCb;
};

/**
 * This function will run the ekaline Fh on the current thread and make callbacks for messages processed.
 * This function should not return until we shut the Ekaline system down via ekaClose().
 *
 * @param efcCtx 
 * @param efcRunCtx This is a pointer to the callbacks (and possibly other information needed) that
 *                   will be called as the Ekaline feedhandler processes messages.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcRun( EfcCtx* efcCtx, const EfcRunCtx* efcRunCtx );

/**
 * This will close an Ekaline firing controller created with efcInit.
 *
 * @oaram efcCtx  An initialized EfcCtx* obtained from efhInit.
 * @return        An EkalineOpResult indicating success or an error in closing the controller.
 */
EkaOpResult efcClose( EfcCtx* efcCtx );

#ifdef __cplusplus
    }
#endif

#endif // __EKALINE_EFC_H__
