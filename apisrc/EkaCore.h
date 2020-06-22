#ifndef _EKA_CORE_H_
#define _EKA_CORE_H_

#include <inttypes.h>
#include <string.h>

#include "eka_macros.h"
#include "eka_hw_conf.h"

class EkaDev;
class EkaUdpChannel;
class EkaTcpSess;
class EkaUdpSess;

class EkaCore {
 public:
  EkaCore(EkaDev* pEkaDev, uint8_t lane, uint32_t ip, uint8_t* mac);

  ~EkaCore();

  uint        addTcpSess();
  EkaTcpSess* findTcpSess(uint32_t ipSrc, uint16_t udpSrc, uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess* findTcpSess(int sock);

  EkaUdpSess* findUdpSess(uint32_t ip, uint16_t port);

  int tcpConnect(uint32_t dstIp, uint16_t port);

  uint addUdpSess(uint32_t ip, uint16_t port);

  uint8_t       coreId;
  uint32_t      srcIp;
  uint8_t       macSa[6];
  uint8_t       macDa[6];
  uint16_t      vlanTag;

  bool          connected;
  bool          macDa_set_externally = false;

  static const uint MAX_SESS_PER_CORE = 8;

  EkaTcpSess*   tcpSess[MAX_SESS_PER_CORE + 1];
  uint8_t       tcpSessions;

  EkaUdpSess*   udpSess[EKA_MAX_UDP_SESSIONS_PER_CORE];
  uint8_t       udpSessions;

  EkaUdpChannel* udpChannel;

  struct netif* pLwipNetIf;

  //  EkaBook*     book[EKA_MAX_BOOK_PRODUCTS];
  //  uint         numBooks;

 private:
  EkaDev*       dev;

};

#endif
