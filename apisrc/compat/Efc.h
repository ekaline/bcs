/**
 *
 * Efc.h
 *     This header file covers the api for the Ekaline
 * firing controller.
 */

#ifndef __EKALINE_EFC_H__
#define __EKALINE_EFC_H__

#include "EfcCtxs.h"
#include "EfcMsgs.h"
#include "Efh.h"
#include "Eka.h"
#include "Epm.h"
#include "Exc.h"

#define EFC_HW_TTL 0x55
#define EFC_HW_ID 0xabcd
#define EFC_HW_UNARMABLE 0xf0f0f0f0

/**
 * Defines a frequency of setting current time in the FPGA
 * to be inserted to the HW Fire Msg
 */
#define EFC_DATE_UPDATE_PERIOD_MILLISEC 100

#ifdef __cplusplus
extern "C" {
#endif

enum EfcStrategyId : int {
  P4 = 0,
  CmeFastCancel,
  ItchFastSweep,
  QedFastSweep
};

static inline const char *
efcPrintStratName(EfcStrategyId id) {
  switch (id) {
  case EfcStrategyId::P4:
    return "P4";
  case EfcStrategyId::CmeFastCancel:
    return "CmeFastCancel";
  case EfcStrategyId::ItchFastSweep:
    return "ItchFastSweep";
  case EfcStrategyId::QedFastSweep:
    return "QedFastSweep";
  default:
    return "Unknown";
  }
}

typedef uint32_t EfcArmVer;

/**
 * Init config passed to efcInit()
 * Sets configuration for all strategies running
 * under Efc
 */
struct EfcInitCtx {
  bool report_only; // The HW generated Fires are not
                    // really sent, but generate Fire Report
  uint64_t watchdog_timeout_sec;
};

/**
 *
 * This will initialize the Ekaline firing controller.
 *
 * @oaram efcCtx     This is an output parameter that will
 * contain an initialized EfhCtx* if this function is
 * successful.
 * @param ekaDev     The device which will host the firing
 * controller
 * @param efcInitCtx This is a list of key value parameters
 * that replaces the old d values that were passed in the
 * old api.
 * @return This will return an appropriate EkalineOpResult
 * indicating success or an error code.
 */

/**
 * @brief Checks HW compatibility and creates Efc module
 *
 * @param efcCtx
 * @param ekaDev
 * @param efcInitCtx
 * @return EkaOpResult
 */
EkaOpResult efcInit(EfcCtx **efcCtx, EkaDev *ekaDev,
                    const EfcInitCtx *efcInitCtx);

/**
 * This will initialize global params of the Efc strategy
 *
 * @param efcCtx
 * @param EfcStratGlobCtx
 * @retval [See EkaOpResult].
 */

EkaOpResult
efcInitStrategy(EfcCtx *efcCtx,
                const EfcStratGlobCtx *efcStratGlobCtx);

/**
 * This will enable or disable the firing controller
 *
 * @param efcCtx
 * @param primaryCoreId If this is >= 0, then it will enable
 * firing on all cores, and make primaryCore the only core
 * that will fire if he opportunity should only only fired
 * on once. If this is < 0, then this will disable firing on
 * all cores.
 * @param ver           Arm version. Disable is done
 * unconditionally, Enable is done only if HW version
 * matches SW version
 * @retval [See EkaOpResult].
 */

EkaOpResult efcEnableController(EfcCtx *efcCtx,
                                EkaCoreId primaryCoreId,
                                EfcArmVer ver = 0);

/**
 * This will tell the hardware to consider firing on md
 * updates for a list of securities. This function can only
 * be called once after efcInit and must be called before
 * efcRun().
 *
 * @param efcCtx
 * @param securityIds    This is a pointer to the first
 * member of an array of securities that the firing
 * controller should consider as opportunities.  This value
 *                       should be the exchange specific
 * security id that is returned from EfhOptionDefinitionMsg.
 * @param numSecurityIds This is the number of elements in
 * the array securityIds.
 * @retval [See EkaOpResult].
 */
EkaOpResult
efcEnableFiringOnSec(EfcCtx *efcCtx,
                     const uint64_t *securityIds,
                     size_t numSecurityIds);

/**
 * This function will take a security that was passed to
 * efcEnableFiringOnSec() and return the corresponding
 * EfcSecCtxHandle.  It must be  called after
 * efcEnableFiringOnSec() but before efcRun().
 *
 * @param efcCtx
 * @param securityId This is a security id that was passed
 * to efcEnableFiringOnSec.
 * @retval [>=0] On success this will return an a value to
 * be interpreted as an EfcSecCtxHandle.
 * @retval [<0]  On failure this will return a value to be
 * interpreted as an error EkaOpResult.
 */
EfcSecCtxHandle getSecCtxHandle(EfcCtx *efcCtx,
                                uint64_t securityId);

/**
 * This will set a SecCtx for a static security.
 *
 * @param efcCtx
 * @param efcSecCtxHandle This is a handle to the SecCtx
 * that we are setting.
 * @param secCtx          This is a pointer to the local
 * source SecCtx.
 * @param writeChan       This is the channel that will be
 * used to write (see kEkaNumWriteChans).
 * @retval [See EkaOpResult].
 */
EkaOpResult efcSetStaticSecCtx(EfcCtx *efcCtx,
                               EfcSecCtxHandle hSecCtx,
                               const SecCtx *secCtx,
                               uint16_t writeChan);

/**
 * This is just like setStaticSecCtx except it will be for
 * dynamic securities.
 */
EkaOpResult efcSetDynamicSecCtx(EfcCtx *efcCtx,
                                EfcSecCtxHandle hSecCtx,
                                const SecCtx *secCtx,
                                uint16_t writeChan);

/**
 * This function is OBSOLETE. Use efcSetFireTemplate() below
 */
EkaOpResult efcSetSesCtx(EfcCtx *efcCtx,
                         ExcConnHandle hConn,
                         const SesCtx *sesCtx);

/**
 * This sets the Fire Message template for the hConn
 * session. The template must populate all fields that are
 * not managed by FPGA (fields managed by FPGA: size, price,
 * etc.).
 * @param efcCtx
 * @param hConn          This is the ExcSessionId that we
 * will be mapping to.
 * @param fireMsg        Application messge in Exchange
 * specific format: SQF, eQuote, etc.
 * @param fireMsgSize    Size of the template
 * @retval [See EkaOpResult].
 */
EkaOpResult efcSetFireTemplate(EfcCtx *efcCtx,
                               ExcConnHandle hConn,
                               const void *fireMsg,
                               size_t fireMsgSize);

/**
 * This will tell the controller which Session to first fire
 * on based on the multicast group that the opportunity
 * arrived on.
 * @param efcCtx
 * @param group  This is the multicast group that we will be
 * mapping from.
 * @param hConn  This is the ExcSessionId that we will be
 * mapping to.
 * @retval [See EkaOpResult].
 */

EkaOpResult efcSetGroupSesCtx(EfcCtx *efcCtx, uint8_t group,
                              ExcConnHandle hConn);

/**
 * This will print Efc Fire Report -- used for Ekaline tests
 * @retval [See EkaOpResult].
 */

/**
 * This sets value of the "binary" and "ASCII" counters that
 * are inserted into the firing payload. The "binary"
 * counter gets the value as is, while the "ASCII" counter
 * gets a result of sprintf(dest,"%08ju",cntr)
 *
 * @param dev
 * @param hConn  This is the ExcSessionId that we will be
 * mapping to.
 * @param cntr   8 byte Little Endian (normal) value of the
 * counters
 * @retval [See EkaOpResult].
 */

EkaOpResult efcSetSessionCntr(EkaDev *dev,
                              ExcConnHandle hConn,
                              uint64_t cntr);

/* ****************************************
 * Declaring callbacks.
 * ****************************************/

/* ****************************************
 * OnEkaExceptionReportCb and OnEfcFireReportCb
 * callbacks are obsolete!!!
 * replaced by onReportCb defined in Eka.h
 * ****************************************/
void efcPrintFireReport(const void *p, size_t len,
                        void *ctx);

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
  OnReportCb onEkaExceptionReportCb; // Not to be used
  OnReportCb onEfcFireReportCb;
  void *cbCtx;
};

/**
 * This function will start - the EFC thread, used for
 * callbacks, exceptions and such. The underlying thread is
 * created by std::thread call.
 *
 * @param efcCtx
 * @param efcRunCtx This is a pointer to the callbacks and
 * user context. Note: the report callback provides the
 * context, while the exception callback does not.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcRun(EfcCtx *efcCtx,
                   const EfcRunCtx *efcRunCtx);

/**
 * This function send a Keep Alive signal (heartbeat) to
 * reset FPGAs watchdog. If FPGA does not get this heartbeat
 * during EfcStratGlobCtx.watchdog_timeout_sec, then EFC
 * controller is DISARMED
 *
 * @param efcCtx
 * @param strategyId    For future Multi-strategy
 * implementation. NOT SUPPORTED!!!
 *
 * @retval [See EkaOpResult].
 */
EkaOpResult
efcSwKeepAliveSend(EfcCtx *efcCtx,
                   int strategyId = EFC_STRATEGY);

/**
 * This will close an Ekaline firing controller created with
 * efcInit.
 *
 * @oaram efcCtx  An initialized EfcCtx* obtained from
 * efcInit.
 * @return        An EkalineOpResult indicating success or
 * an error in closing the controller.
 */
EkaOpResult efcClose(EfcCtx *efcCtx);

/**
 * Allocates a new Action from the Strategy pool.
 *
 * This function should not be called for the P4 strategies
 * as the "1st" firing Actions are implicetly allocated from
 * the reserved range 0..63, so every Market Data MC group
 * has corresponding firing action. For example,
 * at CBOE: actions # 0..34, at NOM: actions # 0..3
 *
 * For other strategies (CME Fast Cancel, Fast Sweep, etc.),
 * a new action is allocated int the range of 64..2047
 * the function returns an index of the allocated action.
 *
 * Every action has it's own reserved Heap space of
 * 1536 Bytes for the network headers and the payload
 *
 * @param [in] pEkaDev
 *
 * @param [in] type EpmActionType of the requested Action
 *
 * @retval global index of the allocated Action or -1 if
 * failed
 */
epm_actionid_t efcAllocateNewAction(EkaDev *ekaDev,
                                    EpmActionType type);

/**
 * @brief
 *
 * @param ekaDev
 * @param actionIdx - global idx in range of 0..8K-1
 * @param payload
 * @param len
 * @return * EkaOpResult
 */
EkaOpResult efcSetActionPayload(EkaDev *ekaDev,
                                epm_actionid_t actionIdx,
                                const void *payload,
                                size_t len);

/**
 * @brief Set the Action to previously connected Ekaline
 * (Exc) Tcp Socket object
 *
 * @param ekaDev
 * @param globalIdx
 * @param excSock
 * @return EkaOpResult
 */
EkaOpResult setActionTcpSock(EkaDev *ekaDev,
                             epm_actionid_t globalIdx,
                             ExcSocketHandle excSock);

/**
 * @brief Set the Action Next hop in the chain. Use
 * EPM_LAST_ACTION for the end-of-chain. Action is created
 * with a default value of EPM_LAST_ACTION
 *
 *
 * @param ekaDev
 * @param globalIdx
 * @param nextActionGlobalIdx
 * @return EkaOpResult
 */
EkaOpResult
setActionNext(EkaDev *ekaDev, epm_actionid_t globalIdx,
              epm_actionid_t nextActionGlobalIdx);

/**
 * @brief Set the Action Physical Lane (or coreId). Used for
 * "Raw" Actions (not belonging to a TCP connection, but
 * sent as is)
 *
 * @param ekaDev
 * @param globalIdx
 * @param lane
 * @return EkaOpResult
 */
EkaOpResult setActionPhysicalLane(EkaDev *ekaDev,
                                  epm_actionid_t globalIdx,
                                  EkaCoreId lane);

struct EfcUdpMcGroupParams {
  EkaCoreId coreId; ///< 10G lane to receive UDP MC
  const char *mcIp;
  uint16_t mcUdpPort;
};

struct EfcUdpMcParams {
  const EfcUdpMcGroupParams *groups;
  size_t nMcGroups;
};

struct EfcP4Params {
  EfhFeedVer feedVer;
  bool fireOnAllAddOrders = false;
  uint32_t max_size; // global value for all P4 securities
};

EkaOpResult
efcInitP4Strategy(EfcCtx *pEfcCtx,
                  const EfcUdpMcParams *mcParams,
                  const EfcP4Params *p4Params);

EkaOpResult efcArmP4(EfcCtx *pEfcCtx, EfcArmVer ver);

EkaOpResult efcDisArmP4(EfcCtx *pEfcCtx);

#define EKA_QED_PRODUCTS 4

struct EfcQEDParamsSingle {
  uint16_t ds_id;        ///
  uint8_t min_num_level; ///
  bool enable;
};

struct EfcQedParams {
  EfcQEDParamsSingle product[EKA_QED_PRODUCTS];
};

EkaOpResult
efcInitQedStrategy(EfcCtx *pEfcCtx,
                   const EfcUdpMcParams *mcParams,
                   const EfcQedParams *qedParams);

EkaOpResult efcQedSetFireAction(EfcCtx *pEfcCtx,
                                epm_actionid_t fireActionId,
                                int productId);

EkaOpResult efcArmQed(EfcCtx *pEfcCtx, EfcArmVer ver);

EkaOpResult efcDisArmQed(EfcCtx *pEfcCtx);

#ifdef __cplusplus
}
#endif

#endif // __EKALINE_EFC_H__
