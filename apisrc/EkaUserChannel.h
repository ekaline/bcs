#ifndef _EKA_USER_CHANNEL_H_
#define _EKA_USER_CHANNEL_H_

#include "smartnic.h"

#include "eka_macros.h"
#include "EkaDev.h"

class EkaDev;

#define USR_CH_DECODE(x)					\
  x == EkaUserChannel::TYPE::FIRE_REPORT ? "FIRE_REPORT" :	\
    x == EkaUserChannel::TYPE::FAST_PATH   ? "FAST_PATH"   :	\
    x == EkaUserChannel::TYPE::TCP_RX      ? "TCP_RX"      :	\
    "UNKNOWN"

class EkaUserChannel {
 public:
  enum class TYPE     : uint8_t { FIRE_REPORT = 0, FAST_PATH = 1, TCP_RX = 2 } ;
  enum class DMA_TYPE : uint8_t { FIRE = 1, FAST_PATH_DUMMY_PKT = 2 };

  TYPE type;

  EkaUserChannel(EkaDev* pEkaDev,SN_DeviceId devId, TYPE t) {
    type = t;

    dev = pEkaDev;
    if (dev == NULL) on_error("dev == NULL");

    EKA_LOG("Openning %s User Channel",USR_CH_DECODE(type));
    fflush(stderr);
    dev_id = devId;

    pkt_payload_ptr = NULL;

    ptr_update_ctr = 0;
    packetBytesTotal = 0;

    ChannelId = SN_AllocateUserLogicChannel(dev_id,(int16_t)type, NULL);
    if (ChannelId == NULL) on_error("Cannot open User channel for %s",USR_CH_DECODE(type));
    EKA_LOG("SN_AllocateUserLogicChannel: OK for %s (=%u)",USR_CH_DECODE(type),(int16_t)type);

    if ((pPreviousUdpPacket = SN_GetNextPacket(ChannelId, NULL, SN_TIMEOUT_NONE)) != NULL) on_error("Packet is arriving on User channel before any packet was sent");
    pIncomingUdpPacket = NULL;
  }


  ~EkaUserChannel() {}

  bool              hasData() {
    pIncomingUdpPacket = SN_GetNextPacket(ChannelId, pPreviousUdpPacket, SN_TIMEOUT_NONE);
    return  pIncomingUdpPacket == NULL ? false : true;
  }

  const uint8_t*    get() {
    pkt_payload_ptr = SN_GetUserLogicPayload(pIncomingUdpPacket);
    return pkt_payload_ptr;
  }

  uint              getPayloadSize() {
    return SN_GetUserLogicPayloadLength(pIncomingUdpPacket);
  }

  void              next() {
    if (pIncomingUdpPacket == NULL) on_error("pIncomingUdpPacket == NULL");
    if (SN_UpdateReceivePtr(ChannelId, pIncomingUdpPacket) != SN_ERR_SUCCESS) on_error ("PIZDETS");
    pPreviousUdpPacket = pIncomingUdpPacket;
  }

  private:
  EkaDev*           dev;
  SN_DeviceId       dev_id;

  uint              ptr_update_ctr;
  uint64_t          packetBytesTotal;
  SN_ChannelId      ChannelId;

  const uint8_t*    pkt_payload_ptr;

  const SN_Packet * pIncomingUdpPacket;
  const SN_Packet * pPreviousUdpPacket;
};

#endif