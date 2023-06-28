/**
 * @file
 *
 * This header file covers the API for EPM Multi Strategies
 * functionality
 *
 */

#ifndef __EKALINE_EFC_MULTI_STRATEGIES_H__
#define __EKALINE_EFC_MULTI_STRATEGIES_H__

extern "C" {
#include "Epm.h"

/**
 * Allocates a new Action from the Strategy pool.
 *
 * This function should not be called for the P4 strategies
 * as the firing Actions are implicetly allocated from the
 * reserved range 0..63, so every Market Data MC group
 * has corresponding firing action. For example,
 * at CBOE: actions # 0..34, at NOM: actions # 0..3
 *
 * For other strategies (CME Fast Cancel, Fast Sweep, etc.),
 * a new action is allocated int the range of 64..255
 * the function returns an index of the allocated action.
 *
 * Every action has it's own reserved Heap space of
 * 1536 Bytes for the network headers and the payload
 *
 * @param [in] pEkaDev
 *
 * @param [in] type EpmActionType of the requested Action
 *
 * @retval index of the allocated Action or -1 if failed
 */
epm_actionid_t efcAllocateNewAction(const EkaDev *ekaDev,
                                    EpmActionType type);

/// Descriptor of a Startegy action to be set. It is a
/// subset of @ref EpmAction to be used by @ref
/// efcSetAction() unlike @ref epmSetAction(), following
/// Action's parametrs are not controlled:
///	   * payload offset - implicitely set
///    * payload length - set by efcSetActionPayload()
///

struct EfcAction {
  EpmActionType type;        ///< Action type
  ExcConnHandle hConn;       ///< TCP connection
  epm_actionid_t nextAction; ///< Next action in sequence
};

/**
 * @brief
 *
 * @param ekaDev
 * @param actionIdx
 * @param efcAction
 * @return * EkaOpResult
 */
EkaOpResult efcSetAction(EkaDev *ekaDev,
                         epm_actionid_t actionIdx,
                         const EfcAction *efcAction);

/**
 * The function performs:
 *    - Copy of the payload to the Heap
 *    - Set of the payload length
 *    - Clear the "Hw controlled fields" according to the
 *      Epm Template
 *    - Calculate pseudo TCP (or UDP) checksum
 *
 *  If provided Action type == INVALID, then this type is
 *  not set, but the previously configured type is preserved
 */

/**
 * @brief
 *
 * @param ekaDev
 * @param actionIdx
 * @param payload
 * @param len
 * @return * EkaOpResult
 */
EkaOpResult efcSetActionPayload(EkaDev *ekaDev,
                                epm_actionid_t actionIdx,
                                const void *payload,
                                size_t len);

} // End of extern "C"

#endif
