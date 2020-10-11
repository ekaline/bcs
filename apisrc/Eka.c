#include "Eka.h"

#include "eka_data_structs.h"
#include "EkaDev.h"

EkaOpResult ekaDevInit( EkaDev** ppEkaDev, const EkaDevInitCtx *pInitCtx ) {
  *ppEkaDev = new EkaDev(pInitCtx);
  if (*ppEkaDev == NULL) return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevClose( EkaDev* pEkaDev ) {
  if (pEkaDev == NULL) return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  printf("%s:%d Im here\n",__func__,__LINE__);
  //delete pEkaDev;
  pEkaDev->~EkaDev();

  printf("%s:%d Im here\n",__func__,__LINE__);
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevConfigurePort( EkaDev *pEkaDev, const EkaCoreInitCtx *pCoreInit ) {
  pEkaDev->configurePort(pCoreInit);
  return EKA_OPRESULT__OK;
}
