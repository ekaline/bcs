#include "eka_macros.h"
#include "EkaDev.h"
#include "EkaMcState.h"


void saveMcState(EkaDev* dev, int grId, int chId, uint8_t coreId, uint32_t mcast_ip, uint16_t mcast_port, uint64_t pktCnt) {
  if (grId < 0 || grId > 63) on_error("Wrong grId %d",grId);
  if (chId < 0 || chId > 31) on_error("Wrong chId %d",chId);

  EkaMcState state = {
    .ip     = mcast_ip,
    .port   = mcast_port,
    .coreId = coreId,
    .pad    = 0,
    .pktCnt = pktCnt
  };

  uint64_t chBaseAddr = SCRPAD_MC_STATE_BASE + chId * MAX_MC_GROUPS_PER_UDP_CH * sizeof(EkaMcState);
  uint64_t addr = chBaseAddr + grId * sizeof(EkaMcState);

  uint64_t* pData = (uint64_t*)&state;
  eka_write(dev,addr,    *pData++);
  eka_write(dev,addr + 8,*pData++);
}
