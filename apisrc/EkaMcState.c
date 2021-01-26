#include "eka_macros.h"
#include "EkaDev.h"
#include "EkaMcState.h"


void saveMcState(EkaDev* dev, int grId, int chId, uint8_t coreId, uint32_t mcast_ip, uint16_t mcast_port) {
  if (grId < 0 || grId > 63) on_error("Wrong grId %d",grId);
  if (chId < 0 || chId > 31) on_error("Wrong chId %d",chId);

  EkaMcState state = {
    .ip     = mcast_ip,
    .port   = mcast_port,
    .coreId = coreId,
    .pad    = 0
  };

  uint64_t chBaseAddr = SCRPAD_MC_STATE_BASE + chId * MAX_MC_GROUPS_PER_UDP_CH * 8;
  uint64_t addr = chBaseAddr + grId * 8;

  eka_write(dev,addr,*(uint64_t*)&state);
}
