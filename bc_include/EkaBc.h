#ifndef _EKA_BC_H_
#define _EKA_BC_H_
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

extern "C" {
struct EkaDev;

#define EFC_BC_MAX_CORES 4

enum EkaBCOpResult {
  EKABC_OPRESULT__OK = 0, /** General success message */
  EKABC_OPRESULT__ALREADY_INITIALIZED = 1,
  EKABC_OPRESULT__END_OF_SESSION = 2,
  EKABC_OPRESULT__ERR_A =
      -100, /** Temporary filler error message.  Replace 'A'
               with actual error code. */
  EKABC_OPRESULT__ERR_DOUBLE_SUBSCRIPTION =
      -101, // returned by efhSubscribeStatic() if trying to
            // subscribe to same security again
  EKABC_OPRESULT__ERR_BAD_ADDRESS =
      -102, // returned if you pass NULL for something that
            // can't be NULL, similar to EFAULT
  EKABC_OPRESULT__ERR_SYSTEM_ERROR =
      -103, // returned when a system call fails and errno
            // is set
  EKABC_OPRESULT__ERR_NOT_IMPLEMENTED =
      -104, // returned when an API call is not implemented
  EKABC_OPRESULT__ERR_GROUP_NOT_AVAILABLE =
      -105, // returned by test feed handler when group not
            // present in capture
  EKABC_OPRESULT__ERR_EXCHANGE_RETRANSMIT_CONNECTION =
      -106, // returned if exchange retransmit connection
            // failed
  EKABC_OPRESULT__ERR_EFC_SET_CTX_ON_UNSUBSCRIBED_SECURITY =
      -107,
  EKABC_OPRESULT__ERR_STRIKE_PRICE_OVERFLOW = -108,
  EKABC_OPRESULT__ERR_INVALID_CONFIG = -109,

  // EPM specific
  EKABC_OPRESULT__ERR_EPM_DISABLED = -201,
  EKABC_OPRESULT__ERR_INVALID_CORE = -202,
  EKABC_OPRESULT__ERR_EPM_UNINITALIZED = -203,
  EKABC_OPRESULT__ERR_INVALID_STRATEGY = -204,
  EKABC_OPRESULT__ERR_INVALID_ACTION = -205,
  EKABC_OPRESULT__ERR_NOT_CONNECTED = -206,
  EKABC_OPRESULT__ERR_INVALID_OFFSET = -207,
  EKABC_OPRESULT__ERR_INVALID_ALIGN = -208,
  EKABC_OPRESULT__ERR_INVALID_LENGTH = -209,
  EKABC_OPRESULT__ERR_UNKNOWN_FLAG = -210,
  EKABC_OPRESULT__ERR_MAX_STRATEGIES = -211,

  // EFC specific
  EKABC_OPRESULT__ERR_EFC_DISABLED = -301,
  EKABC_OPRESULT__ERR_EFC_UNINITALIZED = -302,

  // EFH recovery specific
  EKABC_OPRESULT__RECOVERY_IN_PROGRESS = -400,
  EKABC_OPRESULT__ERR_RECOVERY_FAILED = -401,

  // EFH TCP/Protocol Handshake specific
  EKABC_OPRESULT__ERR_TCP_SOCKET = -501,
  EKABC_OPRESULT__ERR_UDP_SOCKET = -502,
  EKABC_OPRESULT__ERR_PROTOCOL = -503,

  // Product specific
  EKABC_OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST = -600,
  EKABC_OPRESULT__ERR_MAX_PRODUCTS_EXCEEDED = -601,
  EKABC_OPRESULT__ERR_BAD_PRODUCT_HANDLE = -602,
  EKABC_OPRESULT__ERR_PRODUCT_ALREADY_INITED = -603,

};

/**
 * @brief Affinity settings for the span off threads
 *        negative CpuId means no config
 *
 */
struct EkaBcAffinityConfig {
  int servThreadCpuId;  // critical
  int tcpRxThreadCpuId; // critical
  int bookThreadCpuId;  // critical
  int fireReportThreadCpuId;
  int igmpThreadCpuId;
};

/**
 * @brief Callback function to be used for printing
 *        Ekaline logs
 *
 */
typedef int (*EkaLogCallback)(void *ctx,
                              const char *function,
                              const char *file, int line,
                              int priority,
                              const char *format, ...)
    __attribute__((format(printf, 6, 7)));

/**
 * @brief List of CallBack parameters:
 *            logCb: function
 *            cbCtx: log file pointer (stdout if not set)
 */
struct EkaBcCallbacks {
  EkaLogCallback logCb;
  void *cbCtx;
};

/**
 * @brief Initializing Ekaline Device
 *
 * @param affinityConf optional affinity config
 * @param cb optional callbacks struct
 * @return EkaDev* Ekaline Device pointer to be
 *                 provided to all API calls
 */

EkaDev *
ekaBcOpenDev(const EkaBcAffinityConfig *affinityConf = NULL,
             const EkaBcCallbacks *cb = NULL);

/**
 * @brief Closes Ekaline Device and destroys all data
 * structs
 *
 * @param pEkaDev
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcCloseDev(EkaDev *pEkaDev);

/* ==================================================== */
typedef int EkaBcSock;
typedef int8_t EkaBcLane;

/**
 * @brief Opens Ekaline TCP socket implemented in
 *        user space SW and FPGA HW.
 *
 *        First call of this function spins off
 *        TCP RX Thread
 *
 * @param pEkaDev
 * @param lane Physical I/F number: (0 for feth0, etc.)
 * @param ip   Destination IP address
 * @param port Destination TCP Port
 * @return EkaBcSock
 *             File descriptor of the Ekaline socket.
 *             Negative if connection failed.
 *             Should not be used as
 *             Linux socket descriptor!!!
 */
EkaBcSock ekaBcTcpConnect(EkaDev *pEkaDev, EkaBcLane lane,
                          const char *ip, uint16_t port);

/**
 * @brief Sets blocking/non-blocking attribute for ekaSock
 *
 * @param pEkaDev
 * @param hConn
 * @param blocking
 * @return int
 */
int ekaBcSetBlocking(EkaDev *pEkaDev, EkaBcSock ekaSock,
                     bool blocking);

/**
 * @brief Sends data segment via Ekaline TCP socket.
 *
 * @param pEkaDev
 * @param sock
 * @param buf
 * @param size
 * @return ssize_t
 */
ssize_t ekaBcSend(EkaDev *pEkaDev, EkaBcSock ekaSock,
                  const void *buf, size_t size);

/**
 * @brief Receives Data segment via Ekaline TCP socket.
 *
 * @param pEkaDev
 * @param sock
 * @param buf
 * @param size
 * @return ssize_t
 */
ssize_t ekaBcRecv(EkaDev *pEkaDev, EkaBcSock ekaSock,
                  void *buf, size_t size);

/**
 * @brief Disconnects TCP session of ekaSock
 *
 * @param pEkaDev
 * @param ekaSock
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcCloseSock(EkaDev *pEkaDev,
                             EkaBcSock ekaSock);

/* ==================================================== */

/**
 * Init config passed to efcInit()
 * Sets configuration for all strategies running
 * under Efc
 */
struct EkaBcInitCtx {
  bool report_only; // The HW generated Fires are not
                    // really sent, but generate Fire Report
  uint64_t watchdog_timeout_sec;
};

/**
 * @brief Required before initializing any Strategy.
 *        Checks HW compatibility and creates Efc module
 *
 * @param ekaDev
 * @param efcInitCtx
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcInit(EkaDev *ekaDev,
                        const EkaBcInitCtx *ekaBcInitCtx);

/**
 * @brief Must be called in time period shorter than
 *        ekaBcInitCtx.watchdog_timeout_sec
 *        Otherwise the FPGA decides that the SW is died
 *        disarms itself
 *
 * @param pEkaDev
 */
void ekaBcSwKeepAliveSend(EkaDev *pEkaDev);

/* ==================================================== */

/**
 * @brief
 *
 */
struct EkaBcMcGroupParams {
  EkaBcLane lane;   ///< 10G port to receive UDP MC trigger
  const char *mcIp; ///< MC IP address
  uint16_t mcUdpPort; ///< MC UDP Port
};

/**
 * @brief list of MC groups belonging to the strategy
 *
 */
struct EkaBcUdpMcParams {
  const EkaBcMcGroupParams *groups;
  size_t nMcGroups;
};
/* ==================================================== */

/**
 * @brief Used to override default Physical Lane
 *        (=Port, =Core) attributes.
 *        Might be needed for establishing TCP connections.
 *
 */
struct EkaBcPortAttrs {
  uint32_t host_ip; ///<
  uint32_t netmask;
  uint32_t gateway;
  char nexthop_mac[6];  ///< used for setup with L1 switch
  char src_mac_addr[6]; ///< used for setup with L1 switch
};

/**
 * @brief Overrides default (set by Linux) params
 *
 * @param pEkaDev
 * @param lane
 * @param pPortAttrs
 */
void ekaBcConfigurePort(EkaDev *pEkaDev, EkaBcLane lane,
                        const EkaBcPortAttrs *pPortAttrs);

/* ==================================================== */

/**
 * Action is an entity associated with a Firing Packet.
 * It has:
 *  Control parameters:
 *      - Type: See EkaBcActionType.
 *              Set by ekaBcAllocateNewAction().
 *      - ekaSock: socket descriptor of the TCP Session
 *              the Action (=Packet) is sent on.
 *              Set by ekaBcSetActionTcpSock()
 *      - nextAction: next in the chain
 *              or EKA_BC_LAST_ACTION
 *              Set by ekaBcSetActionNext()
 *
 *  Payload: The payload is modified before being sent
 *     according the internal template which corresponds to
 *     the EkaBcActionType. Usually the Payload has a format
 *     Excgange connectivity protocol.
 *     Example of the fields usually overwritten by the
 * Template:
 *       - Firing security ID
 *       - Price
 *       - Size
 *       - Side
 *       - Timestamp
 *       - Application Sequence Number
 *       - etc
 *
 *     Payload is set by ekaBcSetActionPayload()
 */
typedef int EkaBcActionIdx;

#define EKA_BC_LAST_ACTION 0xFFFF

/**
 * @brief List of available Action Types
 *
 */
enum class EkaBcActionType : int {
  INVALID = 0,
  CmeFc = 60,
  EurFire = 61,
};

/**
 * @brief Allocates new Action from the pool and
 *        initializes its type.
 *        Every action has its own reserved space of
 *        1536 Bytes in the Payload (=Heap) memory
 *        for the network headers and the payload
 *
 * @param pEkaDev
 * @param type
 * @return Index (=handle) to be used or -1 if failed
 */
EkaBcActionIdx ekaBcAllocateNewAction(EkaDev *pEkaDev,
                                      EkaBcActionType type);

/**
 * @brief Initializes the Payload of the Fire (=TX) Packet.
 *        Some fields are overridden by the template.
 *
 * @param ekaDev
 * @param actionIdx handle of the action from
 *                  ekaBcAllocateNewAction()
 * @param payload
 * @param len
 * @return * EkaBCOpResult
 */
EkaBCOpResult
ekaBcSetActionPayload(EkaDev *ekaDev,
                      EkaBcActionIdx actionIdx,
                      const void *payload, size_t len);

/**
 * @brief Set the Action to previously connected Ekaline
 *        (Exc) Tcp Socket object
 *
 * @param ekaDev
 * @param globalIdx handle of the action from
 *                  ekaBcAllocateNewAction()
 * @param ekaSock
 * @return EkaBCOpResult
 */
EkaBCOpResult
ekaBcSetActionTcpSock(EkaDev *ekaDev,
                      EkaBcActionIdx actionIdx,
                      EkaBcSock ekaSock);

/**
 * @brief Set the Action Next hop in the chain. Use
 * EKA_BC_LAST_ACTION for the end-of-chain. Action is
 * created with a default value of EKA_BC_LAST_ACTION
 *
 *
 * @param ekaDev
 * @param globalIdx
 * @param nextActionGlobalIdx
 * @return EkaBCOpResult
 */
EkaBCOpResult
ekaBcSetActionNext(EkaDev *ekaDev, EkaBcActionIdx actionIdx,
                   EkaBcActionIdx nextActionIdx);

/**
 * @brief Sends application message via HW action with
 *        predefined template. Should be used for sending
 *        Orders/Quotes/Cancels/Heartbeats if some fields
 *        like Application Sequence or Timestamp
 *        must be managed by HW.
 *
 * @param pEkaDev
 * @param actionIdx Handle of the Action pointing to:
 *                  - TCP Session (ekaSock)
 *                  - Payload template
 *                  - next Action
 * @param buffer
 * @param size
 * @return ssize_t
 */
ssize_t ekaBcAppSend(EkaDev *pEkaDev,
                     EkaBcActionIdx actionIdx,
                     const void *buffer, size_t size);

/**
 * @brief Sets Application layer counter for Ekaline TCP
 *        session. Works for both ASCII and Binary counter
 *        representations.
 *
 * @param dev
 * @param ekaSock TCP sock handle
 * @param cntr Binary counter's value
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcSetSessionCntr(EkaDev *dev,
                                  EkaBcSock ekaSock,
                                  uint64_t cntr);

/* ======================================================
 */

///////////////////////
// Reports
///////////////////////

enum EkaBCEventType {
  EkaBCFireEvent = 1,
  EkaBCEpmEvent = 2,
  EkaBCExceptionEvent = 3,
  EkaBCFastCancelEvent = 4
};

#define EkaBCEventType2STR(x)                              \
  x == EkaBCFireEvent         ? "FireEvent"                \
  : x == EkaBCEpmEvent        ? "EpmEvent"                 \
  : x == EkaBCExceptionEvent  ? "ExceptionEvent"           \
  : x == EkaBCFastCancelEvent ? "FastCancelEvent"          \
                              : "UnknownReport"

struct EkaBCContainerGlobalHdr {
  EkaBCEventType type;
  uint32_t num_of_reports;
};

enum EfcBCReportType {
  BcControllerState = 1000,
  BcMdReport = 2000,
  BcExceptionReport = 4000,
  BcFirePkt = 5000,
  BcEpmReport = 6000,
  BcFastCancelReport = 7000
};

// every report is pre-pended by this header
struct EfcBCReportHdr {
  EfcBCReportType type;
  uint8_t idx;
  size_t size;
};

struct BCExceptionReport {
  uint32_t globalVector;
  uint32_t portVector[EFC_BC_MAX_CORES];
};

struct BCArmStatusReport {
  uint8_t armFlag;
  uint32_t expectedVersion;
};

struct EfcBCExceptionsReport {
  BCArmStatusReport armStatus;
  BCExceptionReport exceptionStatus;
};

enum EpmBCTriggerAction {
  EkaBCUnknown, ///< Zero initialization yields an invalid
                ///< value
  EkaBCSent,    ///< All action payloads sent successfully
  EkaBCInvalidToken,    ///< Trigger security token did not
                        ///< match action token
  EkaBCInvalidStrategy, ///< Strategy id was not valid
  EkaBCInvalidAction,   ///< Action id was not valid
  EkaBCDisabledAction,  ///< Did not fire because an enable
                        ///< bit was not set
  EkaBCSendError ///< Send error occured (e.g., TCP session
                 ///< closed)
};

/* ==================================================== */

/**
 * @brief Algo params of BC CmeFC strategy
 *
 */
struct EkaBcCmeFcAlgoParams {
  int fireActionId; ///< 1st Action fired
  uint64_t minTimeDiff;
  uint8_t minNoMDEntries;
};

/**
 * @brief Initializes internal infrastructure for
 *        BC CmeFC strategy
 *
 * @param pEkaDev
 * @param mcParams MC groups of the market data
 * @param cmeFcParams
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcInitCmeFcStrategy(
    EkaDev *pEkaDev, const EkaBcUdpMcParams *mcParams,
    const EkaBcCmeFcAlgoParams *cmeFcParams);

/* ====================================================== */

/**
 * @brief Initializes internal infrastructure for
 *        Eurex Jump and RefJump logic
 *        must be called before configuring Eurex products
 *
 * @param dev
 * @param mcParams MC groups of the market data
 * @return EkaBCOpResult
 */
EkaBCOpResult
ekaBcInitEurStrategy(EkaDev *dev,
                     const EkaBcUdpMcParams *mcParams);

/**
 * @brief Security handle to be used for
 *        configuring security's params
 *
 */
typedef int64_t EkaBcSecHandle;

/**
 * @brief Security ID as listed on the exchange
 *        Used only for initializing the securities list
 *
 */
typedef int64_t EkaBcEurSecId;
typedef int64_t EkaBcEurPrice;
typedef uint32_t EkaBcEurFireSize;
typedef int32_t EkaBcEurMdSize;
typedef uint64_t EkaBcEurTimeNs;

#define EKA_BC_EUR_MAX_PRODS 16

/**
 * @brief Subscribing on list of Securities (products)
 *
 * @param dev
 * @param prodList List of securities in Exchange encoding
 * @param nProducts number of products
 * @return EkaBCOpResult
 */
EkaBCOpResult
ekaBcSetProducts(EkaDev *dev, const EkaBcEurSecId *prodList,
                 size_t nProducts);

/**
 * @brief Mapping the exchange security encoding to internal
 * Handle. Allocation of the handles might not correspond to
 * the order of the securities on the subscription list is
 * not.
 *
 * @param dev
 * @param secId Exchange encoded ID
 * @return EkaBcSecHandle Handle ID.
 *         Negative value means the Security did not fetch
 * the internal data structure and so ignored!!!
 */
EkaBcSecHandle ekaBcGetSecHandle(EkaDev *dev,
                                 EkaBcEurSecId secId);

/**
 * @brief Config params for Eurex product
 *
 */
struct EkaBcEurProductInitParams {
  EkaBcEurSecId secId;
  EkaBcEurFireSize maxBidSize; // limit fire size
  EkaBcEurFireSize maxAskSize; // limit fire size
  EkaBcEurPrice maxBookSpread;
  EkaBcEurPrice midPoint;
  EkaBcEurPrice
      priceDiv; // for price normalization for prints only
  EkaBcEurPrice step;
  bool isBook;
  uint8_t eiPriceFlavor;
  EkaBcActionIdx fireActionIdx;
};

/**
 * @brief Setting basic params for Eurex product
 *
 * @param dev
 * @param prodHande
 * @param params
 * @return EkaBCOpResult
 */
EkaBCOpResult
ekaBcInitEurProd(EkaDev *dev, EkaBcSecHandle prodHande,
                 const EkaBcEurProductInitParams *params);

#define EKA_JUMP_ATBEST_SETS 4
#define EKA_JUMP_BETTERBEST_SETS 5

#define EKA_RJUMP_ATBEST_SETS 4
#define EKA_RJUMP_BETTERBEST_SETS 6

struct JumpParams {
  EkaBcEurMdSize max_tob_size;
  EkaBcEurMdSize min_tob_size;
  EkaBcEurMdSize max_post_size;
  EkaBcEurMdSize min_ticker_size;
  EkaBcEurPrice min_price_delta;
  EkaBcEurFireSize buy_size;  // limit fire size
  EkaBcEurFireSize sell_size; // limit fire size
  bool strat_en;
  bool boc; // Book Or Cancel
};

/**
 * @brief Used by ekaBcEurSetJumpParams()
 *
 */
struct EkaBcEurJumpParams {
  JumpParams betterBest[EKA_JUMP_BETTERBEST_SETS];
  JumpParams atBest[EKA_JUMP_ATBEST_SETS];
};

struct ReferenceJumpParams {
  EkaBcEurMdSize max_tob_size;
  EkaBcEurMdSize min_tob_size;
  EkaBcEurMdSize max_opposit_tob_size;
  EkaBcEurTimeNs time_delta_ns;
  EkaBcEurMdSize tickersize_lots;
  EkaBcEurFireSize buy_size;
  EkaBcEurFireSize sell_size;
  uint8_t min_spread;
  bool strat_en;
  bool boc; // Book Or Cancel
};

/**
 * @brief Used by ekaBcEurSetReferenceJumpParams()
 *
 */
struct EkaBcEurReferenceJumpParams {
  ReferenceJumpParams betterBest[EKA_RJUMP_BETTERBEST_SETS];
  ReferenceJumpParams atBest[EKA_RJUMP_ATBEST_SETS];
};

/**
 * @brief Setting Eurex Jump params
 *
 * @param dev
 * @param prodHande
 * @param params
 * @return EkaBCOpResult
 */
EkaBCOpResult
ekaBcEurSetJumpParams(EkaDev *dev, EkaBcSecHandle prodHande,
                      const EkaBcEurJumpParams *params);

/**
 * @brief Setting Eurex Reference Jump params
 *
 * @param dev
 * @param triggerProd Product Handle of a product getting
 *                    Market Data trigger
 * @param fireProd    Product Handle of the firing product
 * @param params
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcEurSetReferenceJumpParams(
    EkaDev *dev, EkaBcSecHandle triggerProd,
    EkaBcSecHandle fireProd,
    const EkaBcEurReferenceJumpParams *params);

/**
 * @brief EkaBcArmVer is a mechanism to guarantee controlled
 *        arming of the FPGA firing logic:
 *        Every time the FPGA disarms itself as a result of
 *        the Fire or Watchdog timeout, it increments the
 *        ArmVer counter, so next time it can be armed only
 *        if the SW provides ArmVer matching the expected
 *        number by the FPGA
 */
typedef uint32_t EkaBcArmVer;

/**
 * @brief Changing Arm/Disarm state of the Product
 *
 * @param dev
 * @param prodHande
 * @param armBid
 * @param armAsk
 * @param ver Optional parameter needed only for Arming
 *            (can be skipped if
 *                armBid == false and armAsk == false)
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcArmEur(EkaDev *dev,
                          EkaBcSecHandle prodHande,
                          bool armBid, bool armAsk,
                          EkaBcArmVer ver = 0);

/**
@brief Arming CmeFc strategy Firing logic
 *
 * @param dev
 * @param ver
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcArmCmeFc(EkaDev *dev, EkaBcArmVer ver);

/**
@brief DisArming CmeFc strategy Firing logic
 *
 * @param dev
 * @return EkaBCOpResult
 */
EkaBCOpResult ekaBcDisArmCmeFc(EkaDev *dev);

/**
@brief Callback function pointer. Called every time the
       Fire Report received from the FPGA
 *
 */
typedef void (*onEkaBcReportCb)(const void *report,
                                size_t len, void *ctx);

struct EkaBcRunCtx {
  onEkaBcReportCb onReportCb;
  void *cbCtx; ///< optional opaque field looped back to CB
};

/**
 * @brief Downloads all the configs to the HW.
 *        Subscribes on the MC.
 *        Spins off:
 *             - Service Thread
 *             - Fire Report Thread
 *             - IGMP Thread
 *
 * @param pEkaDev
 * @param pEkaBcRunCtx
 */

void ekaBcEurRun(EkaDev *pEkaDev,
                 const EkaBcRunCtx *pEkaBcRunCtx);

} // End of extern "C"
#endif
