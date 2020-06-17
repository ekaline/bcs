#ifndef _EKA_UDP_SESS_H_
#define _EKA_UDP_SESS_H_

#include <inttypes.h>
#include <string.h>

class EkaDev;

class EkaUdpSess {
 public:

  EkaUdpSess(EkaDev* pEkaDev, uint idx, uint32_t mcIp, uint16_t mcPort) {
    dev  = pEkaDev;
    ip   = mcIp;
    port = mcPort;

    sessIdx = idx;
    expectedSeq       = 0;

    EKA_LOG("New UDP Sess %u: %s:%u",sessIdx, EKA_IP2STR(ip),port);
  }

  inline bool myParams(uint32_t mcIp,uint16_t mcPort) {
    return (ip == mcIp && port == mcPort);
  }

  inline bool myParams(int sock_fd) {
    return (sock == sock_fd);
  }


  inline bool isCorrectSeq(uint32_t seq, uint32_t num) {
    if (expectedSeq == 0) expectedSeq = seq;
    if (expectedSeq == seq) {
	expectedSeq = seq + num;
	return true;
    }
    
    EKA_LOG("ERROR: Sequence GAP: expectedSeq (%u) != seq(%u) while num = %u",expectedSeq,seq,num);
    expectedSeq = seq + num;
    return false;
  }

  int      sock;
  uint32_t ip;
  uint16_t port;

  uint     sessIdx;

 private:
  uint32_t     expectedSeq;


  EkaDev* dev;
};

#endif
