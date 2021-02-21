#include "Eka.h"

//#include "eka_data_structs.h"
#include "EkaDev.h"

EkaOpResult ekaDevInit( EkaDev** ppEkaDev, const EkaDevInitCtx *pInitCtx ) {
  *ppEkaDev = new EkaDev(pInitCtx);
  if (*ppEkaDev == NULL) return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevClose( EkaDev* pEkaDev ) {
  if (pEkaDev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  //delete pEkaDev;
  pEkaDev->~EkaDev();
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevConfigurePort( EkaDev *pEkaDev, const EkaCoreInitCtx *pCoreInit ) {
  if (pEkaDev->configurePort(pCoreInit) == -1)
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  return EKA_OPRESULT__OK;
}
