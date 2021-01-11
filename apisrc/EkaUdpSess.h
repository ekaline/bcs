#ifndef _EKA_UDP_SESS_H_
#define _EKA_UDP_SESS_H_

#include <inttypes.h>
#include <string.h>

class EkaDev;

class EkaUdpSess {
 public:
  EkaUdpSess(EkaDev* pEkaDev, int idx, uint32_t mcIp, uint16_t mcPort) {
    dev  = pEkaDev;
    if (dev == NULL) on_error("dev == NULL");

    ip   = mcIp;
    port = mcPort;
    id   = idx;

    EKA_LOG("New UDP Sess %u: %s:%u",id, EKA_IP2STR(ip),port);
  }

  bool myParams(uint32_t mcIp,uint16_t mcPort) {
    return (ip == mcIp && port == mcPort);
  }

  uint32_t ip   = -1;
  uint16_t port = -1;
  int      id   = -1;
  uint8_t  firstSessId = -1;
 private:
  EkaDev* dev = NULL;
};

#endif
