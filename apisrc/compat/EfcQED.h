/**
 * @file
 *
 * This header file covers the API for the QED Fast Sweep
 */

#ifndef __EFC_QED_H__
#define __EFC_QED_H__

#include "../eka_macros.h"
#include "Epm.h"

extern "C" {

/**
 *
 */
enum class EfcQEDActionId : epm_actionid_t {
  HwPurge = 0,
  Count
};

inline EpmActionType
efcQEDActionId2Type(EfcQEDActionId id) {
  switch (id) {
  case EfcQEDActionId::HwPurge:
    return EpmActionType::QEDHwPurge;
  default:
    on_error("Unexpected EfcQEDActionId id %u", (uint)id);
  }
}

struct EfcQEDParamsSingle {
  epm_actionid_t fireActionId; ///< 1st Action to be fired
  uint16_t ds_id;              ///
  uint8_t min_num_level;       ///
  uint64_t token;              ///< Security token
  bool enable;
};

struct EfcQEDParams {
  EfcQEDParamsSingle product[4];
};

EkaOpResult efcQEDInit(EkaDev *ekaDev,
                       const EfcQEDParams *params);

} // End of extern "C"

#endif
