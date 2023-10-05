#ifndef _EKA_CORE_H_
#define _EKA_CORE_H_

#include <inttypes.h>
#include <string.h>

#include "eka_hw_conf.h"
#include "eka_macros.h"

#include "EkaDev.h"

class EkaUdpChannel;
class EkaTcpSess;
class EkaUdpTxSess;

class EkaCore {
public:
  EkaCore(EkaDev *pEkaDev, uint8_t lane, uint32_t ip,
          uint8_t *mac, bool epmEnabled);

  ~EkaCore();

  bool initTcp();
  uint8_t getFreeTcpSess();
  uint addTcpSess();
  uint
  addUdpTxSess(eka_ether_addr srcMac, eka_ether_addr dstMac,
               eka_in_addr_t srcIp, eka_in_addr_t dstIp,
               uint16_t srcUpdPort, uint16_t dstUpdPort);
  EkaTcpSess *findTcpSess(uint32_t ipSrc, uint16_t udpSrc,
                          uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess *findTcpSess(int sock);

  void suppressOldTcpSess(uint8_t newSessId, uint32_t srcIp,
                          uint16_t srcPort, uint32_t dstIp,
                          uint16_t dstPort);

  //  int tcpConnect(uint32_t dstIp, uint16_t port);

  static const uint MAX_SESS_PER_CORE =
      EkaDev::MAX_SESS_PER_CORE;
  static const uint MAX_UDP_TX_SESS_PER_CORE =
      64; // Just a number
  static const uint CONTROL_SESS_ID =
      EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE =
      EkaDev::TOTAL_SESSIONS_PER_CORE;

  EkaCoreId coreId = -1;
  uint32_t srcIp = 0;
  uint32_t gwIp = 0;
  uint32_t netmask = 0;
  uint8_t macSa[6] = {};
  uint8_t macDa[6] = {};
  uint16_t vlanTag = 0;

  bool connected = false;
  bool isTcpCore = false;

  bool connectedL1Switch =
      false; // Metamux case
             // set if Src Mac configured

  EkaTcpSess *tcpSess[MAX_SESS_PER_CORE + 1] = {};

  EkaUdpTxSess *udpTxSess[MAX_UDP_TX_SESS_PER_CORE] = {};
  uint8_t udpTxSessions = 0;

  struct netif *pLwipNetIf = NULL;

private:
  EkaDev *dev = NULL;
};

#endif
