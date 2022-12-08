#ifndef _EKA_TCP_SESS_H_
#define _EKA_TCP_SESS_H_

#include <inttypes.h>
#include <string.h>
#include <mutex>

#include "eka_macros.h"

#include "EkaHwInternalStructs.h"
#include "EkaCore.h"
#include "EkaEpm.h"

class EkaDev;
class EkaEpmAction;

class EkaTcpSess {
 public:

  EkaTcpSess(EkaDev* pEkaDev, EkaCore* parent, uint8_t coreId, uint8_t sessId,  
	     uint32_t _srcIp,  uint32_t _dstIp, uint16_t _dstPort, uint8_t* macSa);

  int bind();
  int connect();

  int sendPayload(uint thr, void *buf, int len, int flags);
  int sendEthFrame(void *buf, int len);
  int sendStackEthFrame(void *buf, int len);
  int lwipDummyWrite(void *buf, int len);

  int updateRx(const uint8_t* pkt, uint32_t len);

  int preloadNwHeaders();

  ssize_t recv(void *buffer, size_t size, int flags);
  int     close();
  ExcConnHandle getConnHandle() {
    return coreId * 128 + sessId;
  }

  ~EkaTcpSess();

  inline bool myParams(uint32_t srcIp2check,uint16_t srcPort2check,uint32_t dstIp2check,uint16_t dstPort2check) {
    return (srcIp2check == srcIp && srcPort2check == srcPort && dstIp2check == dstIp && dstPort2check == dstPort);
  }

  inline bool myParams(int sock_fd) {
    return (sock == sock_fd);
  }

  static const uint MAX_SESS_PER_CORE       = EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID         = EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE = EkaDev::TOTAL_SESSIONS_PER_CORE;
  static const uint MAX_CTX_THREADS         = EkaDev::MAX_CTX_THREADS;

  static const uint MAX_ETH_FRAME_SIZE      = EkaDev::MAX_ETH_FRAME_SIZE;

  /*
   * NOTE: MAX_PAYLOAD_SIZE *must* match the value of TCP_MSS but we don't want
   * to include lwipopts.h here.
   */
  //  static const uint MAX_PAYLOAD_SIZE  = 1440;
  // matching more conservative MSS
  static const uint MAX_PAYLOAD_SIZE  = 1360;
  static const uint16_t EkaIpId       = 0x0;

  EkaEpmAction* fastPathAction = NULL;
  EkaEpmAction* fullPktAction = NULL;
  EkaEpmAction* emptyAckAction = NULL;

  epm_trig_desc_t emptyAcktrigger = {};

  EkaCore* parent = NULL;

  uint8_t  coreId = -1;
  uint8_t  sessId = -1;
  
  int      sock   = -1;
  uint32_t srcIp  = -1;
  uint16_t srcPort = -1;
  uint32_t dstIp   = -1;
  uint16_t dstPort = -1;

  uint32_t vlan_tag = 0;

  uint16_t tcpRcvWnd; // new
  uint16_t tcpSndWnd = 0;
  uint8_t tcpSndWndShift = 0;

  int appSeqId = 0;
  
  volatile uint32_t tcpLocalSeqNum = 0;
  volatile uint32_t tcpRemoteSeqNum = 0;
  volatile uint32_t tcpRemoteAckNum = 0;

  //  volatile uint64_t fastPathBytes = 0;
  volatile uint32_t fastPathBytes = 0;
  volatile uint64_t throttleCounter = 0;
  volatile uint64_t maxThrottleCounter = 0;
  volatile uint64_t txDriverBytes = 0;
  volatile uint64_t dummyBytes = 0;
  volatile uint64_t tcpLocalSeqNumBase = 0;

  volatile bool txLwipBp = false;

  volatile uint64_t fastBytesFromUserChannel = 0;

  uint8_t __attribute__ ((aligned(0x100)))  pktBuf[MAX_ETH_FRAME_SIZE] = {};
  EkaEthHdr* ethHdr     = (EkaEthHdr*) pktBuf;
  EkaIpHdr*  ipHdr      = (EkaIpHdr* ) (((uint8_t*)ethHdr) + sizeof(EkaEthHdr));
  EkaTcpHdr* tcpHdr     = (EkaTcpHdr*) (((uint8_t*)ipHdr ) + sizeof(EkaIpHdr));
  uint8_t*   tcpPayload = (uint8_t*)tcpHdr + sizeof(EkaTcpHdr);

  uint8_t   macSa[6] = {};
  uint8_t   macDa[6] = {};

  int  setRemoteSeqWnd2FPGA();
  int  setLocalSeqWnd2FPGA();
  bool isEstablished() const noexcept { return connectionEstablished; }
  bool isBlocking() const noexcept { return blocking; }
  int setBlocking(bool);

 private:
  typedef union exc_table_desc {
    uint64_t desc;
    struct fields {
      uint8_t source_bank;
      uint8_t source_thread;
      uint32_t target_idx : 24;
      uint8_t pad[3];
    } __attribute__((packed)) td;
  } __attribute__((packed)) exc_table_desc_t;

  EkaDev* dev = NULL;

  bool connectionEstablished = false;
  bool blocking = false;
};

#endif
