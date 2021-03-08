#include <shared_mutex>
#include "Eka.h"

//#include "eka_data_structs.h"
#include "EkaDev.h"

EkaDev *g_allDevs;               // List of all open devices
std::shared_mutex g_allDevsMtx;  // rwlock protecting g_allDevs

EkaOpResult ekaDevInit( EkaDev** ppEkaDev, const EkaDevInitCtx *pInitCtx ) {
  *ppEkaDev = new EkaDev(pInitCtx);
  if (*ppEkaDev == NULL) return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  std::unique_lock<std::shared_mutex> lck{g_allDevsMtx};
  (*ppEkaDev)->next = g_allDevs;
  g_allDevs = *ppEkaDev;
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevClose( EkaDev* pEkaDev ) {
  if (pEkaDev == NULL) return EKA_OPRESULT__ERR_BAD_ADDRESS;
  {
    std::unique_lock<std::shared_mutex> lck{g_allDevsMtx};
    if (g_allDevs == pEkaDev)
      g_allDevs = pEkaDev->next;
    else {
      EkaDev *prev = g_allDevs;
      while (prev && prev->next != pEkaDev)
        prev = prev->next;
      if (!prev)
        return EKA_OPRESULT__ERR_BAD_ADDRESS;
      prev->next = prev->next->next;
    }
  }
  //delete pEkaDev;
  pEkaDev->~EkaDev();
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevConfigurePort( EkaDev *pEkaDev, const EkaCoreInitCtx *pCoreInit ) {
  if (pEkaDev->configurePort(pCoreInit) == -1)
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  return EKA_OPRESULT__OK;
}
