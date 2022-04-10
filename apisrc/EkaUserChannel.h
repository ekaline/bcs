#ifndef _EKA_USER_CHANNEL_H_
#define _EKA_USER_CHANNEL_H_

#include "smartnic.h"

#include "eka_macros.h"
#include "EkaDev.h"

#define USR_CH_DECODE(x)					\
  x == EkaUserChannel::TYPE::EPM_REPORT ? "EPM_REPORT" :	\
    x == EkaUserChannel::TYPE::LWIP_PATH   ? "LWIP_PATH"   :	\
    "UNKNOWN"

class EkaUserChannel {
 public:
  enum class TYPE     : uint8_t {
				 EPM_REPORT = 0,
				 LWIP_PATH = 1} ; //user channel
  enum class DMA_TYPE : uint8_t {
				 FIRE = 1,
				 EPM = 3,
				 TCPRX = 4 }; // type in descriptor

  TYPE type;

  EkaUserChannel(EkaDev* pEkaDev,SN_DeviceId devId, TYPE t) {
    type = t;

    dev = pEkaDev;
    EKA_LOG("Openning %s User Channel",USR_CH_DECODE(type));
    fflush(stderr);
    dev_id = devId;

    pkt_payload_ptr = NULL;

    ptr_update_ctr = 0;
    packetBytesTotal = 0;

    pIncomingUdpPacket = NULL;
    pPreviousUdpPacket = NULL;

    ChannelId = SN_AllocateUserLogicChannel(dev_id,(int16_t)type, NULL);

    if (ChannelId == NULL) {
      EKA_LOG("%s Channel is already acquired by other App instance",
	      USR_CH_DECODE(type));
      return;
    }

    EKA_LOG("SN_AllocateUserLogicChannel: OK for %s (=%u)",
	    USR_CH_DECODE(type),(int16_t)type);

    while (1) {
      pPreviousUdpPacket = SC_GetNextPacket(ChannelId,
					    pPreviousUdpPacket,
					    SC_TIMEOUT_NONE);      
      if (pPreviousUdpPacket == NULL) break;
      if (SC_UpdateReceivePtr(ChannelId, pPreviousUdpPacket) != SN_ERR_SUCCESS) 
	on_error ("Failed to sync DMA ReceivePtr");
    }
    
    if ((pPreviousUdpPacket = SN_GetNextPacket(ChannelId, NULL,
					       SN_TIMEOUT_NONE)) != NULL) 
      on_error("Packet is arriving on User channel before any packet was sent");
  }

  bool isOpen() {
    EKA_LOG("%s is %s",
	    USR_CH_DECODE(type),
	    ChannelId == NULL ? "CLOSED" : "OPEN");
    return (ChannelId != NULL);
  }

  ~EkaUserChannel();

  bool  hasData() {
    pIncomingUdpPacket = SN_GetNextPacket(ChannelId,
					  pPreviousUdpPacket,
					  SN_TIMEOUT_NONE);
    return  pIncomingUdpPacket == NULL ? false : true;
  }

  const uint8_t*    get() {
    pkt_payload_ptr = SN_GetUserLogicPayload(pIncomingUdpPacket);
    return pkt_payload_ptr;
  }

  uint getPayloadSize() {
    return SN_GetUserLogicPayloadLength(pIncomingUdpPacket);
  }

  void next() {
    if (pIncomingUdpPacket == NULL)
      on_error("pIncomingUdpPacket == NULL");
    if (SN_UpdateReceivePtr(ChannelId, pIncomingUdpPacket) != SN_ERR_SUCCESS)
      on_error ("Failed on SN_UpdateReceivePtr");
    pPreviousUdpPacket = pIncomingUdpPacket;
  }

  private:
  EkaDev*           dev = NULL;
  SN_DeviceId       dev_id;

  uint              ptr_update_ctr = 0;
  uint64_t          packetBytesTotal = 0;
  SN_ChannelId      ChannelId;

  const uint8_t*    pkt_payload_ptr = NULL;

  const SN_Packet * pIncomingUdpPacket = NULL;
  const SN_Packet * pPreviousUdpPacket = NULL;
};

#endif
