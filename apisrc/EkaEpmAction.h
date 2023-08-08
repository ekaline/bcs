#ifndef _EKA_EPM_ACTION_H_
#define _EKA_EPM_ACTION_H_

#include "EkaEpm.h"
#include "EkaWc.h"

class EkaTcpSess;
class EkaUdpSess;

class EkaEpmAction {
public:
  EkaEpmAction(EkaEpm::ActionType type, uint idx,
               uint regionId);

  /* ------------------------------------------------ */
  /*   int updateAttrs(uint8_t _coreId, uint8_t _sessId,
                    const EpmAction *epmAction); */
  /* ------------------------------------------------ */
  void setPayload(const void *buf, size_t len);
  /* ------------------------------------------------ */
  void printHwAction();
  /* ------------------------------------------------ */
  void printHeap();
  /* ------------------------------------------------ */
  void print(const char *msg);
  /* ------------------------------------------------ */
  int setUdpMcNwHdrs(uint8_t *macSa, uint32_t srcIp,
                     uint32_t dstIp, uint16_t srcPort,
                     uint16_t dstPort, uint payloadSize);
  /* ------------------------------------------------ */
  void setTcpSess(EkaTcpSess *tcpSess);
  /* ------------------------------------------------ */
  void setUdpSess(EkaUdpSess *udpSess);
  /* ------------------------------------------------ */
  void setCoreId(EkaCoreId coreId);
  /* ------------------------------------------------ */
  void setNextAction(epm_actionid_t nextActionIdx);
  /* ------------------------------------------------ */
  int setNwHdrs(uint8_t *macDa, uint8_t *macSa,
                uint32_t srcIp, uint32_t dstIp,
                uint16_t srcPort, uint16_t dstPort);
  /* ------------------------------------------------ */
  int sendFullPkt(const void *buf, uint len);
  /* ------------------------------------------------ */
  int preloadFullPkt(const void *buf, uint len);
  /* ------------------------------------------------ */
  int setPktPayloadAndSendWC(const void *buf, uint len);
  /* ------------------------------------------------ */
  int setUdpPktPayload(const void *buf, uint len);
  /* ------------------------------------------------ */
  int send(uint32_t _tcpCSum);
  /* ------------------------------------------------ */
  int send();
  /* ------------------------------------------------ */
  int fastSend(const void *buf, uint len);
  /* ------------------------------------------------ */
  bool isTcp();
  /* ------------------------------------------------ */
  bool isUdp();
  /* ------------------------------------------------ */
  uint getPayloadLen() { return payloadLen_; }
  /* ------------------------------------------------ */
  /* ------------------------------------------------ */
  void updatePayload();

private:
  int setActionBitmap();
  int setTemplate();
  int setName();
  int initEpmActionLocalCopy();
  int setHwAction();
  void setIpTtl();
  EkaEpm::FrameType getFrameType();
  size_t getPayloadOffset();
  uint16_t getL3L4len();
  uint globalIdx() {
    return EkaEpmRegion::getBaseActionIdx(regionId_) + idx_;
  }
  // uint64_t actionAddr();

public:
  inline void copyHeap2Fpga() {
    dev_->ekaWc->epmCopyWcBuf(heapOffs_,
                              &epm_->heap[heapOffs_],
                              pktSize_, idx_, regionId_);
  }

  inline void copyHeap2FpgaAndSend() {
    dev_->ekaWc->epmCopyAndSendWcBuf(&epm_->heap[heapOffs_],
                                     pktSize_, idx_,
                                     regionId_, tcpCSum_);
  }
  /* ------------------------------------------------ */

  static void copyHwActionParams2Fpga(const epm_action_t *,
                                      uint actionGlobalIdx);

public:
private:
  EkaEpm::ActionType type_ = EkaEpm::ActionType::INVALID;
  uint idx_ = -1; // local
  uint regionId_ = -1;
  uint8_t coreId_ = -1;

  uint8_t sessId_ = -1;
  uint productIdx_ = -1;
  EpmTemplate *epmTemplate_ = NULL;

  bool initialized_ = false;

  epm_actionid_t nextIdx_ = EPM_LAST_ACTION;

  epm_action_t hwAction_ = {};
  EpmAction epmActionLocalCopy_ = {};

private:
  EpmActionBitmap actionBitParams_ = {};
  std::string name_ = "Unset";

  EkaDev *dev_ = nullptr;
  EkaEpm *epm_ = nullptr;

  int heapOffs_ = -1;
  uint payloadLen_ = 0;
  uint pktSize_ = 0;
  uint32_t tcpCSum_ = 0;

  EkaEthHdr *ethHdr_ = nullptr;
  EkaIpHdr *ipHdr_ = nullptr;
  EkaTcpHdr *tcpHdr_ = nullptr;
  EkaUdpHdr *udpHdr_ = nullptr;
  uint8_t *payload_ = nullptr;

  volatile uint64_t *snDevWCPtr_ = nullptr;

  EkaTcpSess *tcpSess_ = nullptr;
  EkaUdpSess *udpSess_ = nullptr;
};

#endif
