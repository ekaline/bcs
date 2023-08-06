#include "Eka.h"
#include <shared_mutex>

// #include "eka_data_structs.h"
#include "EkaDev.h"

EkaDev *g_ekaDev;
FILE *g_ekaLogFile;
EkaLogCallback g_ekaLogCB;

EkaOpResult ekaDevInit(EkaDev **ppEkaDev,
                       const EkaDevInitCtx *pInitCtx) {
  *ppEkaDev = new EkaDev(pInitCtx);
  if (!*ppEkaDev)
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;

  g_ekaDev = *ppEkaDev;
  return EKA_OPRESULT__OK;
}

EkaOpResult ekaDevClose(EkaDev *pEkaDev) {
  if (!pEkaDev)
    return EKA_OPRESULT__ERR_BAD_ADDRESS;
  delete pEkaDev;
  g_ekaDev = NULL;

  return EKA_OPRESULT__OK;
}

EkaOpResult
ekaDevConfigurePort(EkaDev *pEkaDev,
                    const EkaCoreInitCtx *pCoreInit) {
  if (pEkaDev->configurePort(pCoreInit) == -1)
    return EKA_OPRESULT__ERR_SYSTEM_ERROR;
  return EKA_OPRESULT__OK;
}

uint64_t eka_read(uint64_t addr) {
  if (!g_ekaDev)
    on_error("!g_ekaDev");
  return g_ekaDev->eka_read(addr);
}

void eka_write(uint64_t addr, uint64_t val) {
  if (!g_ekaDev)
    on_error("!g_ekaDev");
  return g_ekaDev->eka_write(addr, val);
}
