#ifndef _EKA_TCP_SESS_H_
#define _EKA_TCP_SESS_H_

#include <inttypes.h>
#include <string.h>
#include <mutex>

#include "eka_macros.h"

class EkaDev;
class EkaCore;

class EkaTcpSess {
 public:

  EkaTcpSess(EkaDev* pEkaDev, EkaCore* parent, uint8_t coreId, uint8_t sessId,  
	     uint32_t _srcIp,  uint32_t _dstIp, uint16_t _dstPort, 
	     uint8_t* macSa, uint8_t* macDa, 
	     const uint _MAX_SESS_PER_CORE);

  int bind();
  int connect();

  int sendPayload(void *buf, int len);
  int sendFullPkt(void *buf, int len);
  int sendStackPkt(void *buf, int len);
  int sendThruStack(void *buf, int len);

  int sendDummyPkt(void *buf, int len);

  int updateRx(const uint8_t* pkt, uint32_t len);

  int preloadNwHeaders();

  int readyToRecv();

  ssize_t recv(void *buffer, size_t size);
  int     close();

  ~EkaTcpSess();

  inline bool myParams(uint32_t srcIp2check,uint16_t srcPort2check,uint32_t dstIp2check,uint16_t dstPort2check) {
    /* EKA_LOG("mine: %s:%u --> %s:%u vs  %s:%u --> %s:%u", */
    /* 	    EKA_IP2STR(srcIp),srcPort, EKA_IP2STR(dstIp),dstPort, */
    /* 	    EKA_IP2STR(srcIp2check),srcPort2check,  EKA_IP2STR(dstIp2check),dstPort2check */
    /* 	    ); */

    return (srcIp2check == srcIp && srcPort2check == srcPort && dstIp2check == dstIp && dstPort2check == dstPort);
  }

  inline bool myParams(int sock_fd) {
    return (sock == sock_fd);
  }

  const uint MAX_PKT_SIZE      = 1536;

  EkaCore* parent;

  uint8_t  coreId;
  uint8_t  sessId;
  
  int      sock;
  uint32_t srcIp;
  uint16_t srcPort;
  uint32_t dstIp;
  uint16_t dstPort;

  uint32_t vlan_tag;

  //  uint16_t tcp_window; // old
  uint16_t tcpWindow; // new

  volatile uint32_t tcpLocalSeqNum;
  volatile uint32_t tcpRemoteSeqNum;

  volatile uint64_t fastPathBytes;
  volatile uint64_t txDriverBytes;
  //  volatile uint64_t dummyBytes;
  uint64_t dummyBytes;

  volatile uint64_t fastBytesFromUserChannel;

  uint32_t ipPreliminaryPseudoCsum;
  uint32_t tcpPreliminaryPseudoCsum;


  //  std::mutex mtx;

  uint8_t __attribute__ ((aligned(0x100)))  pktBuf[256];

  uint8_t   macSa[6];
  uint8_t   macDa[6];

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

  union tcp_fast_send_desc {
    uint64_t desc;
    struct tcp_desc {
      uint16_t src_index;
      uint16_t length;
      uint8_t session;
      uint16_t ip_checksum;
      uint8_t core_send_attr; // 
      // [7]    - skip
      // [6]    - raw (don't recalc seq, dont send fact path dummy
      // [5..3] - unused
      // [2..0] - coreId

    } __attribute__((packed)) tcpd;
  } __attribute__((packed));


  EkaDev* dev;

  bool connectionEstablished;

  uint payloadFastSendSlot; // TX pkt buffer slot
  uint fullPktFastSendSlot; // TX pkt buffer slot

  tcp_fast_send_desc payloadFastSendDescr;
  tcp_fast_send_desc fullPktFastSendDescr;

};

#endif
