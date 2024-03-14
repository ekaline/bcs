#ifndef _EKA_BCS_H_
#define _EKA_BCS_H_
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <iostream>
#include <cstring>

class EkaDev;

namespace EkaBcs {

static const int MAX_LANES = 4;

enum OpResult {
  OPRESULT__OK = 0, /** General success message */
  OPRESULT__ERR_DEVICE_INIT = -1,
  OPRESULT__ALREADY_INITIALIZED = -11,
  OPRESULT__END_OF_SESSION = -12,
  OPRESULT__ERR_A =
      -100, /** Temporary filler error message.  Replace 'A'
               with actual error code. */
  OPRESULT__ERR_DOUBLE_SUBSCRIPTION =
      -101, // returned by efhSubscribeStatic() if trying to
            // subscribe to same security again
  OPRESULT__ERR_BAD_ADDRESS =
      -102, // returned if you pass NULL for something that
            // can't be NULL, similar to EFAULT
  OPRESULT__ERR_SYSTEM_ERROR =
      -103, // returned when a system call fails and errno
            // is set
  OPRESULT__ERR_NOT_IMPLEMENTED =
      -104, // returned when an API call is not implemented
  OPRESULT__ERR_GROUP_NOT_AVAILABLE =
      -105, // returned by test feed handler when group not
            // present in capture
  OPRESULT__ERR_EXCHANGE_RETRANSMIT_CONNECTION =
      -106, // returned if exchange retransmit connection
            // failed
  OPRESULT__ERR_EFC_SET_CTX_ON_UNSUBSCRIBED_SECURITY = -107,
  OPRESULT__ERR_STRIKE_PRICE_OVERFLOW = -108,
  OPRESULT__ERR_INVALID_CONFIG = -109,

  // EPM specific
  OPRESULT__ERR_EPM_DISABLED = -201,
  OPRESULT__ERR_INVALID_CORE = -202,
  OPRESULT__ERR_EPM_UNINITALIZED = -203,
  OPRESULT__ERR_INVALID_STRATEGY = -204,
  OPRESULT__ERR_INVALID_ACTION = -205,
  OPRESULT__ERR_NOT_CONNECTED = -206,
  OPRESULT__ERR_INVALID_OFFSET = -207,
  OPRESULT__ERR_INVALID_ALIGN = -208,
  OPRESULT__ERR_INVALID_LENGTH = -209,
  OPRESULT__ERR_UNKNOWN_FLAG = -210,
  OPRESULT__ERR_MAX_STRATEGIES = -211,

  // EFC specific
  OPRESULT__ERR_EFC_DISABLED = -301,
  OPRESULT__ERR_EFC_UNINITALIZED = -302,

  // EFH recovery specific
  OPRESULT__RECOVERY_IN_PROGRESS = -400,
  OPRESULT__ERR_RECOVERY_FAILED = -401,

  // EFH TCP/Protocol Handshake specific
  OPRESULT__ERR_TCP_SOCKET = -501,
  OPRESULT__ERR_UDP_SOCKET = -502,
  OPRESULT__ERR_PROTOCOL = -503,

  // Product specific
  OPRESULT__ERR_PRODUCT_DOES_NOT_EXIST = -600,
  OPRESULT__ERR_MAX_PRODUCTS_EXCEEDED = -601,
  OPRESULT__ERR_BAD_PRODUCT_HANDLE = -602,
  OPRESULT__ERR_PRODUCT_ALREADY_INITED = -603,

};

/**
 * @brief Affinity settings for the span off threads
 *        negative CpuId means no config
 *
 */
struct AffinityConfig {
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
struct EkaCallbacks {
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

OpResult openDev(const AffinityConfig *affinityConf = NULL,
                 const EkaCallbacks *cb = NULL);

/**
 * @brief Closes Ekaline Device and destroys all data
 * structs
 *
 * @return OpResult
 */
OpResult closeDev();

/* ==================================================== */
typedef int EkaSock;
typedef int8_t EkaLane;

static const EkaSock EkaDummySock = -1;

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
 * @return EkaSock
 *             File descriptor of the Ekaline socket.
 *             Negative if connection failed.
 *             Should not be used as
 *             Linux socket descriptor!!!
 */
EkaSock tcpConnect(EkaLane lane, const char *ip,
                   uint16_t port);

/**
 * @brief Sets blocking/non-blocking attribute for ekaSock
 *
 * @param pEkaDev
 * @param hConn
 * @param blocking
 * @return int
 */
int setTcpBlocking(EkaSock ekaSock, bool blocking);

/**
 * @brief Sends data segment via Ekaline TCP socket.
 *
 * @param sock
 * @param buf
 * @param size
 * @return ssize_t
 */
ssize_t tcpSend(EkaSock ekaSock, const void *buf,
                size_t size);

/**
 * @brief Receives Data segment via Ekaline TCP socket.
 *
 * @param sock
 * @param buf
 * @param size
 * @return ssize_t
 */
ssize_t tcpRecv(EkaSock ekaSock, void *buf, size_t size);

/**
 * @brief Disconnects TCP session of ekaSock
 *
 * @param ekaSock
 * @return OpResult
 */
OpResult closeSock(EkaSock ekaSock);

/* ==================================================== */

/**
 * Init config passed to efcInit()
 * Sets configuration for all strategies running
 * under Efc
 */
struct HwEngInitCtx {
  bool report_only; // The HW generated Fires are not
                    // really sent, but generate Fire Report
  uint64_t watchdog_timeout_sec;
};

/**
 * @brief Required before initializing any Strategy.
 *        Checks HW compatibility and creates Efc module
 *
 * @param efcInitCtx
 * @return OpResult
 */
OpResult hwEngInit(const HwEngInitCtx *pHwEngInitCtx);

/**
 * @brief Must be called in time period shorter than
 *        ekaBcInitCtx.watchdog_timeout_sec
 *        Otherwise the FPGA decides that the SW is died
 *        disarms itself
 *
 * @param pEkaDev
 */
void swKeepAliveSend();

/* ==================================================== */

/**
 * @brief
 *
 */
struct McGroupParams {
  EkaLane lane;     ///< 10G port to receive UDP MC trigger
  const char *mcIp; ///< MC IP address
  uint16_t mcUdpPort; ///< MC UDP Port
};

/**
 * @brief list of MC groups belonging to the strategy
 *
 */
struct UdpMcParams {
  const McGroupParams *groups;
  size_t nMcGroups;
};
/* ==================================================== */

/**
 * @brief Used to override default Physical Lane
 *        (=Port, =Core) attributes.
 *        Might be needed for establishing TCP connections.
 *
 */
struct PortAttrs {
  uint32_t host_ip;
  uint32_t netmask;
  uint32_t gateway;
  uint8_t nexthop_mac[6];
  uint8_t src_mac_addr[6];
};

/**
 * @brief Overrides default (set by Linux) params
 *
 * @param lane
 * @param pPortAttrs
 */
OpResult configurePort(EkaLane lane,
                       const PortAttrs *pPortAttrs);

/**
 * @brief
 *
 */

typedef int (*MdProcessCallback)(const void *pkt,
                                 size_t pktLen, void *ctx);

/**
 * @brief Configures MC parameters to receive on Feed A
 *        physical I/F (feth0)
 *        USUALLY SHOULD BE CALLED IN SEPARATE THREAD!!!
 *
 * @param mcParams
 * @param cb      Callback function to be executed on every
 *                MD packet
 * @param ctx     Optional parameter. Can be used as FILE*
 * @return OpResult
 */
OpResult configureRcvMd_A(const UdpMcParams *mcParams,
                          MdProcessCallback cb,
                          void *ctx = NULL);

OpResult configureRcvMd_B(const UdpMcParams *mcParams,
                          MdProcessCallback cb,
                          void *ctx = NULL);

void startRcvMd_A();

void startRcvMd_B();

OpResult stopRcvMd_A();

OpResult stopRcvMd_B();
/* ==================================================== */

/**
 * Action is an entity associated with a Firing Packet.
 * It has:
 *  Control parameters:
 *      - Type: See ActionType.
 *              Set by allocateNewAction().
 *      - ekaSock: socket descriptor of the TCP Session
 *              the Action (=Packet) is sent on.
 *              Set by setActionTcpSock()
 *      - nextAction: next in the chain
 *              or CHAIN_LAST_ACTION
 *              Set by setActionNext()
 *
 *  Payload: The payload is modified before being sent
 *     according the internal template which corresponds to
 *     the ActionType. Usually the Payload has a format
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
 *     Payload is set by setActionPayload()
 */
typedef int EkaActionIdx;

#define CHAIN_LAST_ACTION 0xFFFF

/**
 * @brief List of available Action Types
 *
 */
enum class ActionType : int {
  INVALID = 0,
  MoexSwSend = 70,
  MoexFireNew = 71,
  MoexFireReplace = 72,
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
EkaActionIdx allocateNewAction(ActionType type);

/**
 * @brief Initializes the Payload of the Fire (=TX) Packet.
 *        Some fields are overridden by the template.
 *
 * @param actionIdx handle of the action from
 *                  allocateNewAction()
 * @param payload
 * @param len
 * @return * OpResult
 */
OpResult setActionPayload(EkaActionIdx actionIdx,
                          const void *payload, size_t len);

/**
 * @brief Set the Action to previously connected Ekaline
 *        (Exc) Tcp Socket object
 *
 * @param globalIdx handle of the action from
 *                  allocateNewAction()
 * @param ekaSock
 * @return OpResult
 */
OpResult setActionTcpSock(EkaActionIdx actionIdx,
                          EkaSock ekaSock);

/**
 * @brief Set the Action Next hop in the chain. Use
 * CHAIN_LAST_ACTION for the end-of-chain. Action is
 * created with a default value of CHAIN_LAST_ACTION
 *
 *
 * @param globalIdx
 * @param nextActionGlobalIdx
 * @return OpResult
 */
OpResult setActionNext(EkaActionIdx actionIdx,
                       EkaActionIdx nextActionIdx);

/**
 * @brief Sends application message via HW action with
 *        predefined template. Should be used for sending
 *        Orders/Quotes/Cancels/Heartbeats if some fields
 *        like Application Sequence or Timestamp
 *        must be managed by HW.
 *
 * @param actionIdx Handle of the Action pointing to:
 *                  - TCP Session (ekaSock)
 *                  - Payload template
 *                  - next Action
 * @param buffer
 * @param size
 * @return ssize_t
 */
ssize_t appTcpSend(EkaActionIdx actionIdx,
                   const void *buffer, size_t size);

/**
 * @brief Sets Application layer counter for Ekaline TCP
 *        session. Works for both ASCII and Binary counter
 *        representations.
 *
 * @param cntr Binary counter's value
 * @return OpResult
 */
OpResult setClOrdId(uint64_t cntr);

/* ==================================================== */

typedef void (*onMdRecvCb)(const void *md, size_t len,
                           void *ctx);

/**
 * @brief Initializes internal infrastructure for
 *        BC CmeFC strategy
 *
 * @param mcParams MC groups of the market data
 * @param cbFunc point to function to be called on receiving
 *               every Market Data packet
 * @param cbCtx  opaque field passed to the function
 *               (can be used to pass FILE* ptr)
 *
 * @return OpResult
 */
OpResult setMdRcvParams(const UdpMcParams *mcParams,
                        onMdRecvCb cbFunc,
                        void *cbCtx = NULL);

/* ====================================================== */

/**
 * @brief Initializes BCS Moex with MC params
 *
 * @param mcParams MC groups of the market data
 * @return OpResult
 */
OpResult initMoexStrategy(const UdpMcParams *mcParams);

/**
 * @brief Security handle to be used for
 *        configuring security's params
 *
 */

typedef char MoexSecurityIdName[12];

class MoexSecurityId {
public:
  MoexSecurityId();
  MoexSecurityId(const char *name);
  MoexSecurityId &operator=(const MoexSecurityId &other);
  std::string getName() const;
  std::string getSwapName() const;
  void getName(void *dst) const;
  void getSwapName(void *dst) const;

private:
  MoexSecurityIdName data_;
};

/**
 * @brief Security ID as listed on the exchange
 *        Used only for initializing the securities list
 *
 */

typedef int64_t MoexPrice;
typedef int64_t MoexSize;
typedef int64_t MoexTimeNs;
typedef int64_t MoexClOrdId;

typedef int32_t MoexMdSize; // tbd

/**
 * @brief Config params for Moex product.
 *        Initialized only once per Product.
 *
 */
struct ProdPairInitParams {
  MoexSecurityId secBase;
  MoexSecurityId secQuote;
  EkaActionIdx fireBaseNewIdx;
  EkaActionIdx fireQuoteReplaceIdx;
};

#define MOEX_MAX_PROD_PAIRS 1

typedef int PairIdx;

OpResult initProdPair(PairIdx idx,
                      const ProdPairInitParams *params);

// enum class MoexOrderType : int {
//   MY_ORDER = 1,
//   HEDGE_ORDER = 2,
// };

enum class OrderSide : int {
  BUY = 1,
  SELL = 2,
};

OpResult setNewOrderPrice(PairIdx idx, OrderSide side,
                          MoexPrice price);

OpResult setReplaceOrderParams(PairIdx idx, OrderSide side,
                               MoexPrice price,
                               MoexClOrdId clordid);

struct ProdPairDynamicParams {
  MoexPrice markupBuy;
  MoexPrice markupSell;
  MoexPrice fixSpread;
  MoexPrice tolerance;
  MoexPrice slippage;
  MoexSize quoteSize;
  MoexTimeNs timeTolerance;
};

OpResult setProdPairDynamicParams(
    PairIdx idx, const ProdPairDynamicParams *params);

/**
 * @brief ArmVer is a mechanism to guarantee controlled
 *        arming of the FPGA firing logic:
 *        Every time the FPGA disarms itself as a result of
 *        the Fire or Watchdog timeout, it increments the
 *        ArmVer counter, so next time it can be armed only
 *        if the SW provides ArmVer matching the expected
 *        number by the FPGA
 */
typedef uint32_t ArmVer;

OpResult armProductPair(PairIdx idx, bool arm,
                        ArmVer ver = 0);

OpResult resetReplaceCnt();
OpResult setReplaceThreshold(uint32_t threshold);

/**
@brief Callback function pointer. Called every time the
       Fire Report received from the FPGA
 *
 */
typedef void (*OnReportCb)(const void *report, size_t len,
                           void *ctx);

struct RunCtx {
  OnReportCb onReportCb;
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
 * @param pRunCtx
 */

void runMoexStrategy(const RunCtx *pRunCtx);

///////////////////////
// Reports
///////////////////////

enum class EventType : int {
  ExceptionEvent = 1,
  EpmEvent,
  FireEvent,
  FastCancelEvent
};

enum class ReportType : int {
  ControllerState = 1,
  ExceptionsReport,
  FirePkt = 5000,
  MoexFireReport,
};

inline const char *decodeReportType(ReportType type) {
  switch (type) {
  case ReportType::ControllerState:
    return "ControllerState";
  case ReportType::ExceptionsReport:
    return "ExceptionsReport";
  case ReportType::FirePkt:
    return "FirePkt";
  case ReportType::MoexFireReport:
    return "MoexFireReport";
  default:
    return "Unknown";
  }
}

// every report is pre-pended by this header
struct ReportHdr {
  ReportType type;
  int idx;
  size_t size;
};

struct ContainerGlobalHdr {
  EventType eventType;
  int nReports;
};

enum class HwFireStatus : uint8_t {
  Unknown = 0,
  Sent = 1,
  InvalidToken = 2,
  InvalidStrategy = 3,
  InvalidAction = 4,
  DisabledAction = 5,
  SendError = 6,
  HWPeriodicStatus = 255
};

struct EkaBcSwReport {
  uint8_t pad[256];
  uint64_t __unused1;
  uint16_t currentActionIdx; // in the chain
  uint16_t firstActionIdx;   // in the chain
  uint8_t __unused2;
  HwFireStatus fireStatus;
  uint8_t errCode;
  uint16_t __unused3;
  uint16_t __unused4;
  uint16_t __unused5;
  uint16_t __unused6;
  uint64_t __unused7;
  uint8_t __unused8;
} __attribute__((packed));

enum class ArmSide : uint8_t {
  NONE = 0x0,
  BID = 0x1,
  ASk = 0x2,
  BOTH = 0x3
};

struct EkaBcArmReport {
  uint8_t expectedVer; // 1
  ArmSide side;        // 1
} __attribute__((packed));

struct EkaBcExceptionVector {
  uint32_t globalVector;
  uint32_t portVector[MAX_LANES];
};

struct EkaBcExceptionsReport {
  EkaBcArmReport nwReport__unused[16];   // 32
  EkaBcArmReport statusReport[16];       // 32
  EkaBcExceptionVector vector;           // 20
} __attribute__((packed, aligned(256))); // 256

#define EKA_FIRE_TYPE2STRING(x)                            \
  x == 1 ? "Hedge" : x == 2 ? "Replace" : "Unknown"

#define EKA_FIRE_SUBTYPE2STRING(x)                         \
  x == 1 ? "Aggressive" : x == 2 ? "Passive" : "Unknown"

#define EKA_FIRE_SIDE2STRING(x)                            \
  x == 1 ? "Buy" : x == 2 ? "Sell" : "Unknown"

enum class StratType : uint8_t { Hedge = 1, Replace = 2 };
inline const char *decodeStratType(StratType t) {
  switch (t) {
  case StratType::Hedge:
    return "Hedge";
  case StratType::Replace:
    return "Replace";
  default:
    return "Unexpected";
  }
}
enum class StratSubType : uint8_t {
  Aggressive = 1,
  Passive = 2
};
inline const char *decodeStratSubType(StratSubType t) {
  switch (t) {
  case StratSubType::Aggressive:
    return "Aggressive";
  case StratSubType::Passive:
    return "Passive";
  default:
    return "Unexpected";
  }
}
enum class StratSide : uint8_t { Buy = 1, Sell = 2 };
inline const char *decodeStratSide(StratSide s) {
  switch (s) {
  case StratSide::Buy:
    return "Buy";
  case StratSide::Sell:
    return "Sell";
  default:
    return "Unexpected";
  }
}
struct MoexHwFireReport {
  uint64_t ReplaceOrigClOrdID;
  uint64_t RTCounterInternal;
  uint64_t OrderUpdateTime;
  uint64_t Delta;
  uint64_t GoodPrice;
  uint64_t MDAskPrice;
  uint64_t MDBidPrice;
  uint64_t MyOrderSellPrice;
  uint64_t MyOrderBuyPrice;
  MoexSecurityIdName MDSecID;
  uint8_t PairID;
  StratType stratType;       // 1-hedge,2-replace
  StratSubType stratSubType; // 1-aggressive,2-passive
  StratSide stratSide;       // 1-buy,2-sell
} __attribute__((packed, aligned(256)));

struct MoexFireReport {
  MoexHwFireReport hwReport; //
  uint64_t __unused1;
  uint16_t currentActionIdx; // in the chain
  uint16_t firstActionIdx;   // in the chain
  uint8_t __unused2;
  HwFireStatus fireStatus;
  uint8_t errCode;
  uint16_t __unused3;
  uint16_t __unused4;
  uint16_t __unused5;
  uint16_t __unused6;
  uint64_t __unused7;
  uint8_t __unused8;
} __attribute__((packed));

} // End of namespace EkaBcs
#endif
