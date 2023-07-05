#ifndef _EKA_TCP_SESS_H_
#define _EKA_TCP_SESS_H_

#include <atomic>
#include <inttypes.h>
#include <mutex>
#include <string.h>

#include "eka_macros.h"

#include "EkaCore.h"
#include "EkaEpm.h"
#include "EkaHwInternalStructs.h"

class EkaDev;
class EkaEpmAction;

class EkaTcpSess {
public:
  EkaTcpSess(EkaDev *pEkaDev, EkaCore *parent,
             uint8_t coreId, uint8_t sessId,
             uint32_t _srcIp, uint32_t _dstIp,
             uint16_t _dstPort, uint8_t *macSa);

  int bind();
  int connect();

  int preSendCheck(int sendSize, int flags = 0);

  int sendPayload(uint thr, void *buf, int len, int flags);
  int sendEthFrame(void *buf, int len);
  int sendStackEthFrame(void *buf, int len);
  int lwipDummyWrite(void *buf, int len,
                     uint8_t originatedFromHw = 0);

  int updateRx(const uint8_t *pkt, uint32_t len);

  int preloadNwHeaders();

  ssize_t recv(void *buffer, size_t size, int flags);

  int close();

  inline ExcConnHandle getConnHandle() {
    return coreId * 128 + sessId;
  }

  ~EkaTcpSess();

  inline bool myParams(uint32_t srcIp2check,
                       uint16_t srcPort2check,
                       uint32_t dstIp2check,
                       uint16_t dstPort2check) {
    return (
        srcIp2check == srcIp && srcPort2check == srcPort &&
        dstIp2check == dstIp && dstPort2check == dstPort);
  }

  inline bool myParams(int sock_fd) {
    return (sock == sock_fd);
  }

  enum SessFpgaUpdateType {
    AppSeqAscii = 0,
    AppSeqBin,
    RemoteSeqWnd,
    LocalSeqWnd
  };

  template <const SessFpgaUpdateType tableId>
  inline void updateFpgaCtx(uint64_t data) {

    // 0x40000 + ADDR_DECODE[15:0]

    // ADDR_DECODE[15:14] - core
    // ADDR_DECODE[13:12] - tableID
    // ADDR_DECODE[11:0]   - byte addr (8 byte aligned)

    // tableID 0 = AsciiSEQ
    // tableID 1 = BinarySEQ
    // tableID 2 = TCPACK
    // tableID 3 = TCPSEQ

    // All tables have 64 entries

    uint64_t baseAddr = 0x40000;
    const uint64_t TableSize = 4096; // ??? 64 * 8 = 512
    uint64_t tableOffs = tableId * TableSize;
    uint64_t coreOffs = 4 * TableSize * coreId;
    uint64_t wrAddr =
        baseAddr + coreOffs + tableOffs + sessId * 8;

    eka_write(dev, wrAddr, data);
    // TEST_LOG("coreId = %d, sessId = %d, tableId = %d,
    // wrAddr = 0x%jx, data = 0x%jx",
    // 	     coreId,sessId,tableId,wrAddr,data);
    return;
  }

  static const uint MAX_SESS_PER_CORE =
      EkaDev::MAX_SESS_PER_CORE;
  static const uint CONTROL_SESS_ID =
      EkaDev::CONTROL_SESS_ID;
  static const uint TOTAL_SESSIONS_PER_CORE =
      EkaDev::TOTAL_SESSIONS_PER_CORE;
  static const uint MAX_CTX_THREADS =
      EkaDev::MAX_CTX_THREADS;

  static const uint MAX_ETH_FRAME_SIZE =
      EkaDev::MAX_ETH_FRAME_SIZE;

  /*
   * NOTE: MAX_PAYLOAD_SIZE *must* match the value of
   * TCP_MSS but we don't want to include lwipopts.h here.
   */
  //  static const uint MAX_PAYLOAD_SIZE  = 1440;
  // matching more conservative MSS
  static const uint MAX_PAYLOAD_SIZE = 1360;
  static const uint16_t EkaIpId = 0x0;

  EkaTcpSess *controlTcpSess = NULL;

  EkaEpmAction *fastPathAction = {};
  EkaEpmAction *fullPktAction = {};

  EkaEpmAction *emptyAckAction = NULL;

  epm_trig_desc_t emptyAcktrigger = {};

  EkaCore *parent = NULL;

  uint8_t coreId = -1;
  uint8_t sessId = -1;

  int lwip_errno = 0;

  int sock = -1;
  uint32_t srcIp = -1;
  uint16_t srcPort = -1;
  uint32_t dstIp = -1;
  uint16_t dstPort = -1;

  uint32_t vlan_tag = 0;

  uint16_t tcpRcvWnd; // new
  std::atomic<uint32_t> tcpSndWnd = 0;
  uint8_t tcpSndWndShift = 0;
  uint16_t mss = 0; // MSS sent by Exchange

  int appSeqId = 0;

  std::atomic<uint32_t> tcpLocalSeqNum = 0;

  std::atomic<uint32_t> tcpRemoteSeqNum = 0;

  std::atomic<uint32_t> tcpRemoteAckNum = 0;
  std::atomic<uint64_t> realTcpRemoteAckNum = 0;

  std::atomic<uint64_t> realFastPathBytes = 0;

  std::atomic<uint64_t> realDummyBytes = 0;

  std::atomic<uint32_t> tcpLocalSeqNumBase = 0;

  std::atomic<uint64_t> realTxDriverBytes = 0;

  uint64_t lastInsertedEmptyAck = 0;

  std::atomic<bool> txLwipBp = false;

  uint8_t __attribute__((aligned(0x100)))
  pktBuf[MAX_ETH_FRAME_SIZE] = {};
  EkaEthHdr *ethHdr = (EkaEthHdr *)pktBuf;
  EkaIpHdr *ipHdr =
      (EkaIpHdr *)(((uint8_t *)ethHdr) + sizeof(EkaEthHdr));
  EkaTcpHdr *tcpHdr =
      (EkaTcpHdr *)(((uint8_t *)ipHdr) + sizeof(EkaIpHdr));
  uint8_t *tcpPayload =
      (uint8_t *)tcpHdr + sizeof(EkaTcpHdr);

  uint8_t macSa[6] = {};
  uint8_t macDa[6] = {};

  int setRemoteSeqWnd2FPGA();
  int setLocalSeqWnd2FPGA();
  bool isEstablished() const noexcept {
    return connectionEstablished;
  }
  bool isBlocking() const noexcept { return blocking; }
  int setBlocking(bool);

private:
  void processSynAck(const void *pkt, size_t len);
  void insertEmptyRemoteAck(uint64_t seq, const void *pkt);

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

  EkaDev *dev = NULL;

  bool connectionEstablished = false;
  bool blocking = false;
};

#endif
