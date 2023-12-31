/**
 * @file
 *
 * This header file covers the API for the ITCH Fast Sweep
 */

#ifndef __EFC_ITCH_FS_H__
#define __EFC_ITCH_FS_H__

#include "../eka_macros.h"
#include "Epm.h"

extern "C" {

/**
 *
 */
enum class EfcItchFSActionId : epm_actionid_t {
  HwSweep = 0,
  Count
};

inline EpmActionType
efcItchFSActionId2Type(EfcItchFSActionId id) {
  switch (id) {
  case EfcItchFSActionId::HwSweep:
    return EpmActionType::ItchHwFastSweep;
  default:
    on_error("Unexpected EfcItchFSActionId id %u",
             (uint)id);
  }
}

struct EfcItchFastSweepParams {
  uint16_t minUDPSize;         ///
  uint8_t minMsgCount;         ///
  uint64_t token;              ///< Security token
  epm_actionid_t fireActionId; ///< 1st Action to be fired
};

EkaOpResult
efcItchFastSweepInit(EkaDev *ekaDev,
                     const EfcItchFastSweepParams *params);

} // End of extern "C"

#endif
