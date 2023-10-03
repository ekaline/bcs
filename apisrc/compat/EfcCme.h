/**
 * @file
 *
 * This header file covers the API for the CME EFC (Fast Cancels).
 */

#ifndef __EFC_CME_H__
#define __EFC_CME_H__

#include "Epm.h"
#include "EfcMultiStrategies.h"

extern "C" {

/**
 *
 */
  enum class EfcCmeActionId : epm_actionid_t {
    HwCancel,
      SwFire,
      Heartbeat,
      Count
      };

  inline EpmActionType efcCmeActionId2Type (EfcCmeActionId id) {
    switch (id) {
    case EfcCmeActionId::HwCancel :
      return EpmActionType::CmeHwCancel;
    case EfcCmeActionId::SwFire :
      return EpmActionType::CmeSwFire;
    case EfcCmeActionId::Heartbeat :
      return EpmActionType::CmeSwHeartbeat;
    default:
      return EpmActionType::INVALID;
    }
  }
  
  struct EfcCmeFastCancelParams {
    uint16_t       maxMsgSize;     ///< msgSize (from msg hdr) -- only 1st msg in pkt!
    uint8_t        minNoMDEntries; ///< NoMDEntries in MDIncrementalRefreshTradeSummary48
    uint64_t       token;          ///< Security token
		epm_actionid_t fireActionId;   ///< 1st Action to be fired
  };
  
  EkaOpResult efcCmeFastCancelInit(EkaDev *ekaDev,
				   const EfcCmeFastCancelParams* params);

  ssize_t efcCmeSend(EkaDev* pEkaDev, ExcConnHandle hConn,
		     const void* buffer, size_t size, int tcpFlags,
		     bool incrAppSequence);
  
  EkaOpResult efcCmeSetILinkAppseq(EkaDev *ekaDev,
				   ExcConnHandle hCon,
				   int32_t appSequence);
  
} // End of extern "C"

#endif
