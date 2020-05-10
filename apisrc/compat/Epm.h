/**
 * @file
 *
 * This header file covers the API for the Ekaline Prepared Message (EPM) feature.
 */

#ifndef __EKALINE_EPM_H__
#define __EKALINE_EPM_H__

struct EpmAction;

typedef uint64_t epm_token_t;
typedef uint64_t epm_enablebits_t;
typedef int32_t epm_strategyid_t;
typedef int32_t epm_actionid_t;

#define EPM_LAST_ACTION 0

/// Eklaine device limitations
enum EpmDeviceCapability {
  EHC_PayloadMemorySize,       ///< Bytes available for payload packets
  EHC_PayloadAlignment,        ///< Payload memory bufs must have this alignment
  EHC_DatagramOffset,          ///< Payload buf offset where UDP datagram starts
  EHC_RequiredTailPadding,     ///< Payload buf must be oversized to hold padding
  EHC_MaxStrategies,           ///< Total no. of strategies available
  EHC_MaxActions               ///< Total no. of actions (shared by all strats)
};

/// Descriptor of an action to be performed; more documentation about these
/// fields can be found in the @ref epmGetAction function.
struct EpmAction {
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
  const EpmTrigger *trigger;          ///< Contents of the trigger message
  EpmTriggerAction action;            ///< What device did in response to trigger
  EkaOpResult error;                  ///< Error code for SendError
  epm_enablebits_t preLocalEnable;    ///< Action-level enable bits before fire
  epm_enablebits_t postLocalEnable;   ///< Action-level enable bits after firing
  epm_enablebits_t preStratEnable;    ///< Strategy-level enable bits before fire
  epm_enablebits_t postStratEnable;   ///< Strategy-level enable bits after fire
  uintptr_t user;                     ///< Opaque value copied from EpmAction
  bool local;                         ///< True -> called from epmRaiseTrigger
};

typedef void (*EpmFireReportCb)(const EpmFireReport *report, int nReports, void *ctx);

/**
 * Strategy parameters passed to epmInitStrategies
 *
 * - triggerAddr: A destination socket address of the UDP triggers
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
struct EpmStrategyParams {
  epm_actionid_t numActions;   ///< No. of actions entries used by this strategy
  const sockaddr *triggerAddr; ///< Address to receive trigger packets
  EpmFireReportCb reportCb;    ///< Callback function to process fire reports
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
 * @param [in] coreId Identifier of core where the action table will be
 *     allocated; the exc socket will need to exist on this same core.
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
 * @retval ERR_INVALID_CORE coreId did not refer to a valid core.
 *
 * @retval ERR_MAX_STRATEGIES If `numStrategies` exceeded the maximum number
 *     of allowable strategies as reported by @ref epmGetDeviceCapability.
 */
EkaOpResult epmInitStrategies(EkaDev *ekaDev, EkaCoreId coreId,
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
 * @retval ERR_INVALID_CORE coreId does not refer to a valid port.
 *
 * @retvla ERR_EPM_UNINITALIZED coreId refers to a core which has not called
 *     @ref epmInitStrategies.
 *
 * @retval ERR_INVALID_STRATEGY strategy was not a valid strategy id.
 *
 * @retval ERR_INVALID_ACTION action was not a valid action id.
 */
EkaOpResult epmGetAction(const EkaDev *ekaDev, EkaCoreId coreId,
                         epm_strategyid_t strategy, epm_actionid_t action,
                         EpmAction *epmAction);

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
 * @retval ERR_INVALID_CORE coreId does not refer to a valid port.
 *
 * @retvla ERR_EPM_UNINITALIZED coreId refers to a core which has not called
 *     @ref epmInitStrategies.
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
EkaOpResult epmSetAction(EkaDev *ekaDev, EkaCoreId coreId,
                         epm_strategyid_t strategy, epm_actionid_t action,
                         const EpmAction *epmAction);

/**
 * Copy the contents of the given buffer into the payload heap.
 */
EkaOpResult epmPayloadHeapCopy(EkaDev *ekaDev, EkaCoreId coreId,
                               epm_strategyid_t strategy, uint32_t offset,
                               uint32_t length, const void *contents);

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
 * @retval ERR_INVALID_CORE coreId does not refer to a valid port.
 *
 * @retvla ERR_EPM_UNINITALIZED coreId refers to a core which has not called
 *     @ref epmInitStrategies.
 */
EkaOpResult epmGetStrategyEnableBits(const EkaDev *ekaDev, EkaCoreId coreId,
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
 * @retval ERR_INVALID_CORE coreId does not refer to a valid port.
 *
 * @retvla ERR_EPM_UNINITALIZED coreId refers to a core which has not called
 *     @ref epmInitStrategies.
 */
EkaOpResult epmSetStrategyEnableBits(EkaDev *ekaDev, EkaCoreId coreId,
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
 * @retval ERR_INVALID_CORE coreId does not refer to a valid port.
 *
 * @retvla ERR_EPM_UNINITALIZED coreId refers to a core which has not called
 *     @ref epmInitStrategies.
 */
EkaOpResult epmRaiseTriggers(EkaDev *ekaDev, EkaCoreId coreId,
                             const EpmTrigger *trigger);

#endif
