#ifndef _EKA_MC_STATE_H_
#define _EKA_MC_STATE_H_

struct EkaMcState {
  uint32_t   ip;
  uint16_t   port;
  uint8_t    coreId;
  uint8_t    pad;
};

#endif
