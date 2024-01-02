#ifndef _EKA_TCP_H_
#define _EKA_TCP_H_

struct LwipNetifState {
  struct EkaDev* pEkaDev;
  uint8_t        lane;
  uint8_t        pad[7];
};


#endif
