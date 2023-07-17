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
#include "Epm.h"
#include "EfcMultiStrategies.h"

#define EFC_HW_TTL 0x55
#define EFC_HW_ID  0xabcd
#define EFC_HW_UNARMABLE 0xf0f0f0f0

/**
 * Defines a frequency of setting current time in the FPGA
 * to be inserted to the HW Fire Msg
 */
#define EFC_DATE_UPDATE_PERIOD_MILLISEC 100


#ifdef __cplusplus
    extern "C" {
#endif


typedef uint32_t EfcArmVer;

/**
 * This is passed to EfhInit().  This will replace the eka_conf values that we passed in the old api.  
 * This function must be called before efcInit().
 */
struct EfcInitCtx {
  EfhFeedVer feedVer;  
  // For CBOE testRun enables firing on Short, Long, and Expanded orders
  // while for the non-testRun it fires only on Expanded Customer orders
  bool       testRun = false;

  
  // replaced by Trigger config
  //  EkaProps*  ekaProps;
  //  EkaCoreId  mdCoreId; // what 10G port get MD on
  
  
  /** This should be a pointer to a valid EfhCtx created by efhInit(). */
  // EfhCtx* efhCtx; -- removed by Vitaly
  
  /* Efh and Efc must be initialized independently using EfhInitCtx and EfcInitCtx */
};

/**
 * This will initialize the Ekaline firing controller.
 *
 * @oaram efcCtx     This is an output parameter that will contain an initialized EfhCtx* if
 *                   this function is successful.
 * @param ekaDev     The device which will host the firing controller
 * @param efcInitCtx This is a list of key value parameters that replaces the old d values 
 *                    that were passed in the old api.  
 * @return This will return an appropriate EkalineOpResult indicating success or an error code.
 */
EkaOpResult efcInit( EfcCtx** efcCtx, EkaDev *ekaDev, const EfcInitCtx* efcInitCtx );

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
 * @param ver           Arm version. Disable is done unconditionally, Enable is done only if 
 *                      HW version matches SW version
 * @retval [See EkaOpResult].
 */

EkaOpResult efcEnableController( EfcCtx* efcCtx, EkaCoreId primaryCoreId,  EfcArmVer ver=0 );

/**
 * This will tell the hardware to consider firing on md updates for a list of securities.
 * This function can only be called once after efcInit and must be called before efcRun().
 * 
 * @param efcCtx
 * @param securityIds    This is a pointer to the first member of an array of securities that 
 *                       the firing controller should consider as opportunities.  This value
 *                       should be the exchange specific security id that is returned from 
 *                       EfhOptionDefinitionMsg.
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
 * This function is OBSOLETE. Use efcSetFireTemplate() below
 */
EkaOpResult efcSetSesCtx( EfcCtx* efcCtx, ExcConnHandle hConn, const SesCtx* sesCtx );


/**
 * This sets the Fire Message template for the hConn session. The template must populate all
 * fields that are not managed by FPGA (fields managed by FPGA: size, price, etc.).
 * @param efcCtx
 * @param hConn          This is the ExcSessionId that we will be mapping to.
 * @param fireMsg        Application messge in Exchange specific format: SQF, eQuote, etc.
 * @param fireMsgSize    Size of the template
 * @retval [See EkaOpResult].
 */
 EkaOpResult efcSetFireTemplate( EfcCtx* efcCtx, ExcConnHandle hConn, const void* fireMsg, size_t fireMsgSize );


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

/**
 * This sets value of the "binary" and "ASCII" counters that are inserted
 * into the firing payload. The "binary" counter gets the value as is,
 * while the "ASCII" counter gets a result of sprintf(dest,"%08ju",cntr)
 * 
 * @param dev
 * @param hConn  This is the ExcSessionId that we will be mapping to.
 * @param cntr   8 byte Little Endian (normal) value of the counters
 * @retval [See EkaOpResult].
 */

EkaOpResult efcSetSessionCntr(EkaDev *dev,ExcConnHandle hConn,uint64_t cntr);


			
/* ****************************************
 * Declaring callbacks.
 * ****************************************/

/* ****************************************
 * OnEkaExceptionReportCb and OnEfcFireReportCb
 * callbacks are obsolete!!!
 * replaced by onReportCb defined in Eka.h
 * ****************************************/
void efcPrintFireReport(const void* p, size_t len, void* ctx);

#if 0      
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
    /* const EfcFireReport*      efcFireReport, */
    const void*               efcFireReport,
    size_t size,
    void* cbCtx
  );
#endif
      
/*
 * 
 */
struct EfcRunCtx {
    /** These can be either fires or exceptions. */
  /* OnEkaExceptionReportCb onEkaExceptionReportCb;  */
  /* OnEfcFireReportCb      onEfcFireReportCb; */
  OnReportCb      onEkaExceptionReportCb; //Not to be used
  OnReportCb      onEfcFireReportCb;  
  void *cbCtx;
};

/**
 * This function will start - the EFC thread, used for callbacks, exceptions and such.
 * The underlying thread is created by std::thread call.
 *
 * @param efcCtx 
 * @param efcRunCtx This is a pointer to the callbacks and user context.
 *                   Note: the report callback provides the context, while the exception callback does not.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcRun( EfcCtx* efcCtx, const EfcRunCtx* efcRunCtx );


/**
 * This function send a Keep Alive signal (heartbeat) to reset FPGAs watchdog.
 * If FPGA does not get this heartbeat during EfcStratGlobCtx.watchdog_timeout_sec,
 * then EFC controller is DISARMED
 *
 * @param efcCtx 
 * @param strategyId    For future Multi-strategy implementation. NOT SUPPORTED!!!
 *
 * @retval [See EkaOpResult].
 */
 EkaOpResult efcSwKeepAliveSend( EfcCtx* efcCtx, int strategyId = EFC_STRATEGY );

      
/**
 * This will close an Ekaline firing controller created with efcInit.
 *
 * @oaram efcCtx  An initialized EfcCtx* obtained from efcInit.
 * @return        An EkalineOpResult indicating success or an error in closing the controller.
 */
EkaOpResult efcClose( EfcCtx* efcCtx );

#ifdef __cplusplus
    }
#endif

#endif // __EKALINE_EFC_H__
