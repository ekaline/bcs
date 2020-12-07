#ifndef _EKA_CORE_H_
#define _EKA_CORE_H_

#include <inttypes.h>
#include <string.h>

#include "eka_macros.h"
#include "eka_hw_conf.h"

#include "EkaDev.h"

class EkaUdpChannel;
class EkaTcpSess;
class EkaUdpSess;

class EkaCore {
 public:
  EkaCore(EkaDev* pEkaDev, uint8_t lane, uint32_t ip, uint8_t* mac);

  ~EkaCore();

  uint8_t     getFreeTcpSess();
  uint        addTcpSess();
  EkaTcpSess* findTcpSess(uint32_t ipSrc, uint16_t udpSrc, 
			  uint32_t ipDst, uint16_t udpDst);
  EkaTcpSess* findTcpSess(int sock);

  void  suppressOldTcpSess(uint8_t newSessId,
			   uint32_t srcIp,uint16_t srcPort,
			   uint32_t dstIp,uint16_t dstPort);

  int tcpConnect(uint32_t dstIp, uint16_t port);

  static const uint MAX_SESS_PER_CORE       = EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID         = EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE = EkaDev::TOTAL_SESSIONS_PER_CORE;

  uint8_t       coreId;
  uint32_t      srcIp;
  uint8_t       macSa[6] = {};
  uint8_t       macDa[6] = {};
  uint16_t      vlanTag = 0;

  bool          connected = false;
  bool          macDa_set_externally = false;

  EkaTcpSess*   tcpSess[MAX_SESS_PER_CORE + 1] = {};
  uint8_t       tcpSessions = 0;

  struct netif* pLwipNetIf = NULL;

 private:
  EkaDev*       dev = NULL;

};

#endif
