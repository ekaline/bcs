/**
 *
 * Efc.h
 *     This header file covers the api for the Ekaline
 * firing controller.
 */

#ifndef __EKALINE_EFC_H__
#define __EKALINE_EFC_H__

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

typedef uint32_t EfcArmVer;
typedef uint16_t FixedPrice;

typedef struct {
#define SecCtx_FIELD_ITER(_x)                              \
  _x(FixedPrice, bidMinPrice) _x(FixedPrice, askMaxPrice)  \
      _x(uint8_t, bidSize) _x(uint8_t, askSize)            \
          _x(uint8_t, versionKey)                          \
              _x(uint8_t, lowerBytesOfSecId)
  SecCtx_FIELD_ITER(EKA__FIELD_DEF)
} SecCtx;

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
 * @param ekaDev
 * @param efcInitCtx
 * @return EkaOpResult
 */
EkaOpResult efcInit(EkaDev *ekaDev,
                    const EfcInitCtx *efcInitCtx);

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
 * @param pEkaDev
 * @param efcRunCtx This is a pointer to the callbacks and
 * user context. Note: the report callback provides the
 * context, while the exception callback does not.
 * @retval [See EkaOpResult].
 */
EkaOpResult efcRun(EkaDev *pEkaDev,
                   const EfcRunCtx *efcRunCtx);

/**
 * This function send a Keep Alive signal (heartbeat) to
 * reset FPGAs watchdog. If FPGA does not get this heartbeat
 * during EfcStratGlobCtx.watchdog_timeout_sec, then EFC
 * controller is DISARMED
 *
 * @param pEkaDev
 * @param strategyId    For future Multi-strategy
 * implementation. NOT SUPPORTED!!!
 *
 * @retval [See EkaOpResult].
 */
EkaOpResult
efcSwKeepAliveSend(EkaDev *pEkaDev,
                   int strategyId = EFC_STRATEGY);

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
 * @brief Set the Action Physical Lane (= coreId). Used for
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

/**
 * @brief Send application message via HW action with
 * predefined template. Should be used for sending
 * Orders/Quotes/Cancels/Heartbeats if some fields like
 * Application Sequence or Timestamp must be managed by HW.
 *
 * @param pEkaDev
 * @param actionId
 * @param buffer
 * @param size
 * @return ssize_t
 */
ssize_t efcAppSend(EkaDev *pEkaDev, epm_actionid_t actionId,
                   const void *buffer, size_t size);

/* --------------------------------------------------- */

struct EfcUdpMcGroupParams {
  EkaCoreId coreId; ///< 10G lane to receive UDP MC
  const char *mcIp;
  uint16_t mcUdpPort;
};

struct EfcUdpMcParams {
  const EfcUdpMcGroupParams *groups;
  size_t nMcGroups;
};

/* --------------------------------------------------- */
/**
 * This will tell the hardware to consider firing on md
 * updates for a list of securities. This function can only
 * be called once after efcInit and must be called before
 * efcRun().
 *
 * @param pEkaDev
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
efcEnableFiringOnSec(EkaDev *pEkaDev,
                     const uint64_t *securityIds,
                     size_t numSecurityIds);

/**
 * This function will take a security that was passed to
 * efcEnableFiringOnSec() and return the corresponding
 * EfcSecCtxHandle.  It must be  called after
 * efcEnableFiringOnSec() but before efcRun().
 *
 * @param pEkaDev
 * @param securityId This is a security id that was passed
 * to efcEnableFiringOnSec.
 * @retval [>=0] On success this will return an a value to
 * be interpreted as an EfcSecCtxHandle.
 * @retval [<0]  On failure this will return a value to be
 * interpreted as an error EkaOpResult.
 */
EfcSecCtxHandle getSecCtxHandle(EkaDev *pEkaDev,
                                uint64_t securityId);

/**
 * This will set a SecCtx for a static security.
 *
 * @param pEkaDev
 * @param efcSecCtxHandle This is a handle to the SecCtx
 * that we are setting.
 * @param secCtx          This is a pointer to the local
 * source SecCtx.
 * @param writeChan       This is the channel that will be
 * used to write (see kEkaNumWriteChans).
 * @retval [See EkaOpResult].
 */
EkaOpResult efcSetStaticSecCtx(EkaDev *pEkaDev,
                               EfcSecCtxHandle hSecCtx,
                               const SecCtx *secCtx,
                               uint16_t writeChan);
struct EfcP4Params {
  EfhFeedVer feedVer;
  bool fireOnAllAddOrders = false;
  uint32_t max_size; // global value for all P4 securities
};

EkaOpResult
efcInitP4Strategy(EkaDev *pEkaDev,
                  const EfcUdpMcParams *mcParams,
                  const EfcP4Params *p4Params);

EkaOpResult efcArmP4(EkaDev *pEkaDev, EfcArmVer ver);

EkaOpResult efcDisArmP4(EkaDev *pEkaDev);
/* --------------------------------------------------- */

#define EKA_QED_PRODUCTS 4
#define EFC_MAX_MC_GROUPS_PER_LANE 36
#define EFC_PREALLOCATED_P4_ACTIONS_PER_LANE 64

struct EfcQEDParamsSingle {
  uint16_t ds_id;        ///
  uint8_t min_num_level; ///
  bool enable;
};

struct EfcQedParams {
  EfcQEDParamsSingle product[EKA_QED_PRODUCTS];
};

EkaOpResult
efcInitQedStrategy(EkaDev *pEkaDev,
                   const EfcUdpMcParams *mcParams,
                   const EfcQedParams *qedParams);

EkaOpResult efcQedSetFireAction(EkaDev *pEkaDev,
                                epm_actionid_t fireActionId,
                                int productId);

EkaOpResult efcArmQed(EkaDev *pEkaDev, EfcArmVer ver);

EkaOpResult efcDisArmQed(EkaDev *pEkaDev);
/* --------------------------------------------------- */

struct EfcCmeFcParams {
  uint16_t maxMsgSize; // msgSize (from msg hdr) -- only
                       // 1st msg in pkt!
  uint8_t
      minNoMDEntries; // NoMDEntries in
                      // MDIncrementalRefreshTradeSummary48
};

EkaOpResult
efcInitCmeFcStrategy(EkaDev *pEkaDev,
                     const EfcUdpMcParams *mcParams,
                     const EfcCmeFcParams *cmeParams);

EkaOpResult
efcCmeFcSetFireAction(EkaDev *pEkaDev,
                      epm_actionid_t fireActionId);

EkaOpResult efcArmCmeFc(EkaDev *pEkaDev, EfcArmVer ver);

EkaOpResult efcDisArmCmeFc(EkaDev *pEkaDev);
/* --------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif // __EKALINE_EFC_H__
