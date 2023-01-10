/**
 * @file
 *
 * This header file covers the API for the Ekaline Prepared Message (EPM) feature.
 *
 * The Ekaline Prepared Message is a feature that allows for a low-latency message sending (within an open TCP session).
 * Low latency comes from the message sending being triggered in the HW, which eliminates the need to go through PCIE.
 *
 * It consists of the folowing:
 *   Ekaline Socket (exc) that must operate in TCP mode (managed by EXC part of the API)
 *   EPM Strategy (created by a |epmInitStrategies| call) that binds triggers, strategy list and a callback
 *     Triggers (a trigger is a source of UDP traffic - physical port x UDP multicast address x IP port)
 *   Actions that represent what the system should do, when the desired conditions arise. A valid action
 *       is a prescription to send some kind of message through the associated TCP stream. Note that the TCP stream
 *       writer[s] must operate in the right mode (write messages in atomic manner) for this to work.
 *
 * The rough idea of operation is to set up a startegy with triggers and actions all attached to a TCP socket.
 * If a trigger conditions are met, then a chain of actions is invoked (they boil down to sending messages into the TCP stream).
 * The messages are Ethernet frames with IPv4 and TCP Segmnent headers (produced by the ekaline library) containing tcp data
 * (so called payload) that is supplied by the ekaline client (and possibly instantiated by ekaline, depending on message type).
 *
 * Messages are prepared in advance - client code provides payload type and content, that is stored into ekaline internal memory.
 * This memory (called heap or payload memory) is managed by the client, with limitations indicated by how the heap is used.
 * Client-provided payload data is sandwitched by ekaline-provided headers and trailers (inaccessible by the application):
 *  - prefixed by Ethernet, IP and TCP headers (at least 54 bytes)
 *  - potentially suffixed by some other data (CRC, timestamps and such)
 * On top of that, each prepared message (including all headers and trailers) must be start-aligned.
 * The heap size, message alignment, header and trailer sizes can be retrieved by calls to |epmGetDeviceCapability|.
 *
 *
 * The client code should look like the following:
 *
 * EkaDev *device = nullptr;
 * EkaDevInitCtx initCtx {
 *   .logCallback = nullptr, // TODO(twozniak): supply this, maybe
 *   .logContext = nullptr,  // TODO(twozniak): supply this, maybe
 *   .credAcquire = nullptr,
 *   .credRelease = nullptr,
 *   .credContext = nullptr,
 *   .createThread = createPthread,
 *   .createThreadContext = nullptr,
 * };
 * ekaDevInit(&device, &initCtx);
 *
 * EkaCoreId phyPort{0}; // Physical port index
 * EkaCoreInitCtx portInitCtx{
 *   .coreId = phyPort,
 *   .attrs = { .host_ip = inet_addr("???"), .netmask = inet_addr("255.255.255.0"), .gateway = inet_addr("???"),
                //.nexthop_mac   resolved internally
                //.src_mac_addr  resolved internally
                //.dont_garp     what is it? }
 * };
 * ekaDevConfigurePort(device, &portInitCtx);
 *
 * ExcSocketHandle hSocket = excSocket(device, coreId, AF_INET, SOCK_STREAM, IPPROTO_TCP);
 *
 * //struct sockaddr_in myAddr{ .sin_family = AF_INET, .sin_port = 0x0102, .sin_addr = { .s_addr = 0x01020304 } };
 * //bind(hSocket, (const struct sockaddr *)&myAddr, sizeof(myAddr));
 * struct sockaddr_in peer{ .sin_family = AF_INET, .sin_port = 0x0102, .sin_addr = { .s_addr = 0x01020304 } };
 * ExcConnHandle hConnection = excConnect(device, hSocket, (const struct sockaddr *)&peer, sizeof(peer));
 *
 * void fireReportCallback(const EpmFireReport *reports, int nReports, void *ctx) {
 *   for (int idx = 0; idx < nReports; ++idx) {
 *     EpmFireReport const *report = &reports[idx];
 *     // work the |report|
 *   }
 * }
 *
 * EpmTriggerParams triggerSources[] = {{
 *   .coreId = phyPort,       // cable plug index
 *   .mcIp = "69.50.112.174", // CME
 *   .mcUdpPort = 61891       // CME
 * }};
 * EpmStrategyParams strategies{
 *   .numActions = 1,
 *   .triggerParams = triggerSources,
 *   .numTriggers = ARRAY_SIZE(triggerSources),
 *   .reportCb = fireReportCallback,
 *   .cbCtx = nullptr, // no user context
 * };
 * epmInitStrategies(device, &strategies, ARRAY_SIZE(strategies));
 *
 * epmEnableController(device, phyPort, true);
 * epm_enablebits_t enableBitmap = 0x01;
 * epmSetStrategyEnableBits(device, strategyId, enableBitmap);
 *
 * const void *cmeMassCancelMessageTemplateBytes = ;
 * uint32_t cmeMassCancelMessageTemplateSize = ;
 * uint32_t cmeMassCancelHeapOffset = ;
 * int strategyId = 0;
 * epmPayloadHeapCopy(device, strategyId,
 *      cmeMassCancelHeapOffset, cmeMassCancelMessageTemplateSize, cmeMassCancelMessageTemplateBytes);
 *
 * EpmAction actions[] = {{
 *   .type = CmeCancel,
 *   .token = ???,
 *   .hConn = hConnection,
 *   .offset = cmeMassCancelHeapOffset,
 *   .length = cmeMassCancelMessageTemplateSize,
 *   .actionFlags = AF_Valid,
 *   .nextAction = EPM_LAST_ACTION,
 *   .enable = 0x01,
 *   .postLocalMask = 0x01,
 *   .postStratMask = 0x01,
 *   .user = 0,  // just a single user
 * }};
 * EkaOpResult epmSetAction(device, strategyId, 0, &actions[0]);
 *
 *
 * Please note the implicit connection between the first trigger (embedded in the |strategies[0].triggerParams|)
 * and the first action (actions[0]). Each trigger (once activated) invokes an action chain,
 * starting with an action of the same index. Thus, all the triggers are directly tied to the firrst actions (as many as there are triggers).
 * There may be more actions, as a single trigger may start multiple actions (an action chain).
 *
 * The action chain is a list of actions chained by the |nextAction| field. It is not specified what happens when
 * the chain contains a loop, multiple chains have a commin tail or a chain is broken (an index outside the action array).
 *
 * It is not clear what the controller state may/must be for the configuration to take place.
 * It is not clear if/how the strategy list can be altered (can init be called many times?)
 * TODO(twozniak): the security token (zero should be OK for the test)
 */

#ifndef __EKALINE_EPM_H__
#define __EKALINE_EPM_H__

#include "Eka.h"

/**
 * Defines a frequency of polling FPGA exceptions
 */
#define EPM_EXCPT_REQUEST_TRIGGER_TIMEOUT_MILLISEC 100

extern "C" {

struct EpmAction;

typedef uint64_t epm_token_t;
typedef uint64_t epm_enablebits_t;
typedef int32_t epm_strategyid_t;
typedef int32_t epm_actionid_t;

#define EPM_LAST_ACTION 0xFFFF

#define EFC_STRATEGY 0
#define EPM_INVALID_STRATEGY 255
#define EPM_NO_STRATEGY 254

/* The ekaline library provides the internal memory pool (heap) that is divided into two parts:
 *  - implicit used for actions (not directly accessible by applications)
 *  - explicit (managed by calls to epmPayloadHeapCopy) with prepared messages' data
 *
 * Each message (on the explicit heap) follows the layout depicted below.
 * Note: all packets share header, trailer and alignment, but each packet may have different payload size:
 * ------------------------------------------------------------------
 * |<--    single message address range      -->|     Unused        |
 * |<-- this address must be a multiple of alignment from heap begin
 * | Headers     |    Payload    |   Trailers   | Alignment padding |
 * ------------------------------------------------------------------
 * | ^Ekaline    |^client-provi- | ^ekaline data|                   |
 * | generated   |ded data (each |              |                   |
 * |  headers    | time altered) |              |                   |
 * ------------------------------------------------------------------
 * |<- EHC_Data- | Provided for  |<-EHC_Required| Implicit: begin of
 *  gramOffset-->| each message  | TailPadding_>| next message minus end of trailers
 * |<--------------- multiple of EHC_PayloadAlignment ------------->|
 * headers(sent) |TCP data (sent)| (not sent)   | (not sent)        |
 * */
/// Ekaline device limitations
enum EpmDeviceCapability : int {
  EHC_PayloadMemorySize,       ///< Total bytes available for messages, including headers, trailers and alignment (heap size).
  EHC_PayloadAlignment,        ///< Every message must be start-aligned to multiple of this value relative to heap start.
  EHC_DatagramOffset,          ///< TCP payload offset relative to a message start.
  EHC_UdpDatagramOffset,       ///< UDP payload offset relative to a message start.
  EHC_RequiredTailPadding,     ///< Reserved space after payload within a packet (after each payload this much bytes
                               ///< need to be reserved on the heap for ekaline internal use)
  EHC_MaxStrategies,           ///< Total strategy capacity (maximal number of strategies that may be created at the same time)
  EHC_MaxActions               ///< Total action capacity (maximal number of actions - shared by all strategies)
};

enum class EpmActionType : int {
  INVALID = 0,

    /// Service Actions - used by Ekaline internally
    /// These Actions use dedicated Heap partition protected from User
    TcpFullPkt  = 11,
    TcpFastPath = 12,
    TcpEmptyAck = 13,
    Igmp        = 14,
    
    /// Efc Actions - can be fired from SW and FPGA logic
    /// These Actions must belong only to EFC_STRATEGY
    /// Part of the payload fields are managed by FPGA (securityId, price, size, etc.)
    /// Each Action must have implicit template (manually prepared by ekaline)
    HwFireAction = 21,          ///< Default Fire Format (for backward compatibility) 
    SqfFire      = 22,
    SqfCancel    = 23,
    BoeFire      = 24,
    BoeCancel    = 25,
    CmeFire      = 26,
    CmeCancel    = 27,

    CmeHwCancel  = 31,          ///< propagating App sequence
    CmeSwFire    = 32,          ///< propagating App sequence
    CmeSwHeartbeat = 33,        ///< not propagating App sequence

    ItchHwFastSweep = 41,
    
    // User Actions
    UserAction   = 100          ///< EPM fire. No fields managed by HW
 };


/// Descriptor of an action to be performed; more documentation about these
/// fields can be found in the @ref epmGetAction function.
struct EpmAction {
  EpmActionType type;          ///< Action type
  epm_token_t token;           ///< Security token
  ExcConnHandle hConn;         ///< TCP connection where segments will be sent
  uint32_t offset;             ///< Offset to payload in payload heap
  uint32_t length;             ///< Payload length
  uint32_t actionFlags;        ///< Behavior flags (see EpmActionFlag)
  epm_actionid_t nextAction;   ///< Next action in sequence, or EPM_LAST_ACTION
  epm_enablebits_t enable;     ///< Enable bits
  epm_enablebits_t postLocalMask; ///< Post fire: enable & mask -> enable
  epm_enablebits_t postStratMask; ///< Post fire: strat-enable & mask -> strat-enable
  uintptr_t user;              ///< Opaque value copied into `EpmFireReport`.
};

/// Behavior flags for actions
enum EpmActionFlag {
  AF_Valid = 0x1,              ///< Action chain is valid (see @ref epmGetAction)
  AF_InValid = 0x0,              ///< Action chain is not valid (see @ref epmGetAction)
};

/**
 * Trigger that caused a fire report to be generated.
 *
 * The UDP trigger payload is conceptually an array of these, following the
 * structure layout rules of the AMD64 Itanium ABI:
 *
 *   struct TriggerPayload {
 *     std::uint64_t nTriggers: 8; // Number of triggers that follow.
 *     std::uint64_t : 56;         // unused alignment tail padding.
 *     EpmTrigger triggers[];
 *   };
 */
struct EpmTrigger {
  epm_token_t token;           ///< Security token
  epm_strategyid_t strategy;   ///< Strategy this trigger applies to
  epm_actionid_t action;       ///< First action in linked sequence
};

/// Action taken by the Ekaline device in response to receiving a fire trigger.
enum EpmTriggerAction : int {
  Unknown,             ///< Zero initialization yields an invalid value
  Sent,                ///< All action payloads sent successfully
  InvalidToken,        ///< Trigger security token did not match action token
  InvalidStrategy,     ///< Strategy id was not valid
  InvalidAction,       ///< Action id was not valid
  DisabledAction,      ///< Did not fire because an enable bit was not set
  SendError            ///< Send error occured (e.g., TCP session closed)
};

/// Every trigger generates one of these events and is reported to the caller
/// via an @ref EpmFireReportCb callback.
struct EpmFireReport {
  epm_strategyid_t strategyId;        ///< Strategy ID the report corresponds to
  epm_actionid_t   actionId;          ///< Action ID the report corresponds to
  epm_actionid_t   triggerActionId;   ///< Action ID of the Trigger
  epm_token_t      triggerToken;      ///< Security token of the Trigger
  EpmTriggerAction action;            ///< What device did in response to trigger
  EkaOpResult error;                  ///< Error code for SendError
  epm_enablebits_t preLocalEnable;    ///< Action-level enable bits before fire
  epm_enablebits_t postLocalEnable;   ///< Action-level enable bits after firing
  epm_enablebits_t preStratEnable;    ///< Strategy-level enable bits before fire
  epm_enablebits_t postStratEnable;   ///< Strategy-level enable bits after fire
  uintptr_t user;                     ///< Opaque value copied from EpmAction
  bool local;                         ///< True -> called from epmRaiseTrigger
};

struct EpmFastSweepReport {
  uint16_t        udpPayloadSize;  ///< Field from trigger MD
  uint16_t        locateID;        ///< Field from trigger MD
  uint16_t        lastMsgNum;      ///< Field from trigger MD
  char            firstMsgType;    ///< Field from trigger MD
  char            lastMsgType;     ///< Field from trigger MD
};

struct EpmFastCancelReport {
  uint8_t         numInGroup;        ///< Field from trigger MD
  uint16_t        headerSize;        ///< Field from trigger MD
  uint32_t        sequenceNumber;    ///< Field from trigger MD
};

struct EpmNewsReport {
  uint16_t        strategyIndex;      ///< Field from trigger MD
  uint8_t         strategyRegion;     ///< Field from trigger MD
  uint64_t        token;              ///< Field from trigger MD
};
typedef void (*EpmFireReportCb)(const EpmFireReport *report, int nReports, void *ctx);

/**
 * Strategy parameters passed to epmInitStrategies
 *
 * - triggerParams: A list of UDP triggers (10G port + MC IP + UDP Port)
 *     the device will look for. This can be NULL, in which case the user
 *     may only send software triggers via @ref epmRaiseTrigger.
 *
 * - reportCb:  When a trigger packet arrives, the device will
 *     respond in some way (performing the registered action, ignoring the
 *     packet because the security token does not match, etc.); whatever it
 *     does, it reports the result to the caller via this callback function.
 *     Because multiple actions may be performed, the callback is given an
 *     array of report structures.
 */

struct EpmTriggerParams {
  EkaCoreId   coreId;          ///< 10G port to receive UDP MC trigger
  const char* mcIp;            ///< MC IP address
  uint16_t    mcUdpPort;       ///< MC UDP Port
};

struct EpmStrategyParams {
  epm_actionid_t numActions;   ///< No. of actions entries used by this strategy
  const EpmTriggerParams *triggerParams; ///< list of triggers belonging to this strategy
  size_t numTriggers;          ///< size of triggerParams list
  //  EpmFireReportCb reportCb;    ///< Callback function to process fire reports
  OnReportCb reportCb;    ///< Callback function to process fire reports
  void *cbCtx;                 ///< Opaque value passed into reportCb
};

/**
 * Returns information about the EPM capabilities of the given Ekaline device.
 *
 * @param [in] ekaDev The device whose hardware capabilities will be queried.
 *
 * @param [in] cap The specific capability value (see @ref EpmDeviceCapability)
 *     that will be returned.
 *
 * @returns The capability value. If an invalid capability enumeration constant
 *     is provided, zero will be returned.
 */
uint64_t epmGetDeviceCapability(const EkaDev *ekaDev, EpmDeviceCapability cap);

/**
 * Initialize all strategies on the given Ekaline device.
 *
 * A strategy is table of actions. An action describes a prepared TCP payload to
 * be sent over an exc socket at a later time, when a "trigger" condition
 * occurs. An action's TCP payload usually contains one or more prepared
 * exchange messages, thus the API name "Ekaline Prepared Messages (EPM)".
 *
 * Actions are modeled in the API by the EpmAction structure, which specifies
 * the socket that will be used to send the action, the memory address where
 * the payload is located, behavior flags, and other fields (see
 * @ref epmGetAction).
 *
 * Conceptually, these action descriptors form an array (or "table") and an
 * array index (of integral type `epm_actionid_t`) is used to address a
 * specific action. The action index is used when setting the actions
 * properties and by the trigger mechanism to cause that action's payload to
 * be sent. One of the action fields allows the user to specify the index of
 * a *chained* action -- thus, actions can be part of a linked list. Zero is
 * not a valid action id and is used to indicate the end of the linked list.
 *
 *    .--------.-----------------.-------.-------------.-------.
 *    | socket | offset | length | flags | next action | token |
 *    .--------.--------.--------.-------.-------------.-------.
 *    |                  ... more actions ...                  |
 *    .--------------------------------------------------------.
 *               A strategy is a conceptual array of actions
 *
 * A table of actions is called a "strategy" in the API because a particular
 * market trading strategy (e.g., "news") is responsible for populating the
 * messages in the action table, such that those actions "carry out" the
 * trading objectives of that strategy.
 *
 * The TCP payloads are stored in an contiguous memory heap area called the
 * "payload heap." An action descriptor specifies the offset and length of a
 * payload within this heap. The heap area can be written to using the function
 * @ref epmPayloadHeapCopy. The EPM API performs no memory management: heap
 * management issues (fragmentation, etc.) are the responsibility of the
 * caller.
 *
 * There can be multiple such strategies (in the business sense) but they
 * share a common set of hardware resources. To simplify the allocation of
 * hardware resources to strategies, certain parameters (those in
 * EpmStrategyParams) must be provided for all strategies that will be
 * used when this initialization function is called. The maximum number of
 * possible strategies can be determined by calling @ref epmGetDeviceCapability.
 * The `EpmStrategyParams` structure does not have an explicitly
 * `epm_strategyid_t` field -- instead the array index implicitly specifies
 * the strategy being described.
 *
 * All action and payload contents are initialized to zero after allocation.
 *
 * @param [in] ekaDev The device where the action table memory and payload
 *     heap memory will be allocated.
 *
 * @param [in] params An array (of size numsStrategies) of `EpmStrategyParams`
 *     structures, which describe the fixed parameters of the strategies
 *     that affect hardware resource allocation that cannot change during
 *     operation.
 *
 * @param [in] numStrategies Size of the `params` array.
 *
 * @retval OK Strategy allocated successfully.
 *
 * @retval ERR_BAD_ADDRESS Returned if ekaDev or params was NULL.
 *
 * @retval ERR_MAX_STRATEGIES If `numStrategies` exceeded the maximum number
 *     of allowable strategies as reported by @ref epmGetDeviceCapability.
 */
EkaOpResult epmInitStrategies(EkaDev *ekaDev,
                              const EpmStrategyParams *params,
                              epm_strategyid_t numStrategies);

/**
 * Globally enable or disable firing on all strategies associated with the
 * given core.
 *
 * @param [in] ekaDev Device whose strategies will be enabled/disabled.
 *
 * @param [in] coreId Core whose strategies will enabled/disabled.
 *
 * @param [in] enable If true, firing will be allowed. If false, triggers that
 *     attempt to fire will be ignored and the callback will report
 *     EpmTriggerAction::DisarmedCore.
 */
EkaOpResult epmEnableController(EkaDev *ekaDev, EkaCoreId coreId, bool enable);

/**
 * Populates the action descriptor using the values currently set in the
 * strategy, for the specified action id.
 *
 * The action descriptor contains several pieces of information, including:
 *
 *   hConn - TCP connection where the action payload will be sent
 *   offset - Offset in the payload heap where the TCP segment payload starts
 *   length - Length of the TCP segment payload
 *   nextAction - id of the action to execute immediately after this one, or
 *                EPM_LAST_ACTION to stop
 *   actionFlags - behavior flags
 *   enable  - Enable bits, described below
 *   postLocalMask - see enable bits below
 *   postStratMask - see enable bits below
 *   token - Security token, described below
 *
 * AF_Valid and Enable Bits
 * ------------------------
 *
 * An action has no effect unless it is both valid and enabled. The "valid"
 * state is controlled by the AF_Valid behavior flag. Its purpose is to mark
 * an action entry (and the linked-list chain it is the head of) as containing
 * valid data. It is cleared on the head linked list entry to indicate that
 * the entry is invalid and should be ignored in every situation, including
 * traversal of the action chain by the hardware. Unlike the enable bits
 * described below, it does not have a "business-level" meaning -- it is
 * technical artifact used to implement atomic updates to chains of linked
 * actions. Also unlike the enable bits, the hardware interprets its meaning,
 * namely to abort linked list traversal of otherwise-meaningful actions.
 *
 * By contrast, the enable bits also play a role in permitting the action but
 * each bit has a user-defined meaning. The action will only fire if all of
 * its bits are currently enabled on the strategy level. Unlikely with
 * AF_Valid, even when an individual action is disabled, evaluation will
 * continue for other actions on the linked list.
 *
 * For example, suppose the user defines three bits:
 *
 * 1. Master Enable (M) - The user defines this bit (traditionally bit 0)
 *        such that it must be set on every action for it fire. This allows
 *        the clearing of this bit on the stategy level to disable everything.
 *
 * 2. Take Enable (T) - The user defines this bit such that it is set on
 *        packets that place new orders in a "taking" strategy.
 *
 * 3. Pull Enable (P) - The user defines this bit such that it is set on
 *        packets that pull quotes.
 *
 * Further suppose there is an action chain which places a taking order and
 * then pulls all associated quotes:
 *
 *                            M   T   P   next action
 *    .---------------------.---.-------.---
 *    |  take order action  | 1 | 1 | 0 | .-|----.
 *    .---------------------.---.---.---.---.    |
 *                                               |
 *    .---------------------.---.---.---.---.    |
 *    |  pull order action  | 1 | 0 | 1 | * | <--.
 *    .---------------------.---.---.---.---.
 *
 * In the first scenario, all enable bits are set at the strategy level
 * (strategyEnable = 0x3). This allows both actions on the chain to fire
 * because in both cases:
 *
 *     (actionEnable & strategyEnable) == actionEnable
 *
 * In a second scenario, suppose the strategy disables the take enable bit.
 * Now the action above (and all others that have the T bit set) will be
 * disabled *however* the hardware will continue to traverse the action linked
 * list, possibly executing other linked actions which are still enabled, e.g.,
 * the pull quote actions.
 *
 * Each action can also modify its own enable bits or the strategy
 * enable bits after a successful fire, using the local or strategy mask,
 * respectively. This allows an action to execute exactly once, or for action
 * to disable all of it's "sibling" actions in the same category (e.g., a
 * "taking" order disables all other "taking" orders to limit exposure).
 *
 * Security Tokens
 * ---------------
 *
 * The security token is a kind of safety check. The process that populates the
 * actions and the process that sends the trigger packets must agree on the
 * meaning of the strategy and action ids. For example, if the action payload
 * publisher believes that (1, 17) refers to an order payload that means
 * "buy IBM" and the trigger publisher believes (1, 17) refers to a payload
 * that means "sell GE", the wrong action will be taken.
 *
 * To avoid this, the action publisher generates an 8-byte token for each
 * action, which is also sent in by the trigger publisher. If the device
 * receives a trigger packet containing a valid (strategy id, action id)
 * pair but the trigger's token does not match the value in the action
 * descriptor, the device will *not* fire, but will generate an error fire
 * report with code EpmTriggerAction::InvalidToken.
 *
 * @retval OK
 *
 * @retval ERR_BAD_ADDRESS ekaDev or action was NULL.
 *
 * @retval ERR_INVALID_STRATEGY strategy was not a valid strategy id.
 *
 * @retval ERR_INVALID_ACTION action was not a valid action id.
 */
EkaOpResult epmGetAction(const EkaDev *ekaDev, epm_strategyid_t strategy, 
			 epm_actionid_t action, EpmAction *epmAction);

/**
 * Sets the action descriptor in the strategy according to the passed in
 * object.
 *
 * See epmGetAction for information about action descriptors.
 *
 * @retval OK
 *
 * @retval ERR_BAD_ADDRESS ekaDev or enable was NULL.
 *
 * @retval ERR_INVALID_STRATEGY strategy was not a valid strategy id.
 *
 * @retval ERR_INVALID_ACTION action or next action link was not a valid
 *     action id.
 *
 * @retval ERR_NOT_CONNECTED connection does not refer to an active TCP session.
 *
 * @retval ERR_INVALID_OFFSET offset field was invalid.
 *
 * @retval ERR_INVALID_ALIGN offset was not suitably aligned.
 *
 * @retval ERR_INVALID_LENGTH payload length field was invalid.
 *
 * @retval ERR_UNKNOWN_FLAG Caller tried to set an unrecognized behavior flag.
 *
 */
EkaOpResult epmSetAction(EkaDev *ekaDev, epm_strategyid_t strategy, 
			 epm_actionid_t action, const EpmAction *epmAction);

/**
 * Copy the contents of the given buffer into the payload heap.
 */
EkaOpResult epmPayloadHeapCopy(EkaDev *ekaDev, epm_strategyid_t strategy, 
			       uint32_t offset, uint32_t length, 
			       const void *contents, 
			       const bool isUdpDatagram = false);

/**
 * Gets strategy-level enable bits.
 *
 * See @ref epmGetAction for a description of action-level ("local") and
 * strategy-level ("global") enable bits.
 *
 * @param [in] strategy strategy whose flags will be modified.
 *
 * @param [out] enable When the function is successful, this will be set
 *     to the strategy's enable bits.
 *
 * @retval OK
 *
 * @retval ERR_BAD_ADDRESS ekaDev or enable was NULL.
 *
 * @retval ERR_INVALID_STRATEGY strategy was not a valid strategy id.
 * 
 */
EkaOpResult epmGetStrategyEnableBits(const EkaDev *ekaDev,
                                     epm_strategyid_t strategy,
                                     epm_enablebits_t *enable);
/**
 * Sets strategy-level enable bits.
 *
 * See @ref epmGetAction for a description of action-level ("local") and
 * strategy-level ("global") enable bits.
 *
 * @param [in] strategy strategy whose flags will be modified.
 *
 * @param [in] enable New value for the strategy-level enable bits.
 *
 * @retval OK
 *
 * @retval ERR_BAD_ADDRESS ekaDev was NULL.
 *
 * @retval ERR_INVALID_STRATEGY strategy was not a valid strategy id.
 *
 */
EkaOpResult epmSetStrategyEnableBits(EkaDev *ekaDev,
                                     epm_strategyid_t strategy,
                                     epm_enablebits_t enable);

/**
 * Allows software to directly trigger an action rather than waiting to receive
 * the external trigger.
 *
 * When a trigger is sent using this method, the "local" field of EpmFireReport
 * will be set to true.
 *
 * @param [in] trigger Trigger parameters.
 *
 * @retval OK This indicates that the trigger was successfully send to the
 *     device but not necessarily that the trigger successfully executed any
 *     actions. For consistency with UDP-based triggers, any errors that occur
 *     are reported through the callback mechanism.
 *
 * @retval ERR_BAD_ADDRESS ekaDev was NULL.
 *
 */
EkaOpResult epmRaiseTriggers(EkaDev *ekaDev,
                             const EpmTrigger *trigger);



} // End of extern "C"

#endif
