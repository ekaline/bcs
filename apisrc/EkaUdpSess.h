#ifndef _EKA_UDP_SESS_H_
#define _EKA_UDP_SESS_H_

#include <inttypes.h>
#include <string.h>

class EkaDev;

class EkaUdpSess {
 public:
  EkaUdpSess(EkaDev*   pEkaDev, 
	     int       _id, 
	     EkaCoreId _coreId, 
	     uint32_t  _ip, 
	     uint16_t  _port) {
    dev  = pEkaDev;
    if (dev == NULL) on_error("dev == NULL");

    id     = _id;
    coreId = _coreId;
    ip     = _ip;
    port   = _port;

    EKA_LOG("New UDP Sess %u: %s:%u",id, EKA_IP2STR(ip),port);
  }

  bool myParams(EkaCoreId _coreId, uint32_t _ip,uint16_t _port) {
    return (coreId == _coreId && ip == _ip && port == _port);
  }

  uint32_t  ip   = -1;
  uint16_t  port = -1;
  int       id   = -1;
  EkaCoreId coreId = -1;

  //  uint8_t  firstSessId = -1;
 private:
  EkaDev* dev = NULL;
};

#endif
