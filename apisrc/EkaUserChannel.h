#ifndef _EKA_USER_CHANNEL_H_
#define _EKA_USER_CHANNEL_H_

#include "EkaDmaChannel.h"

#define USR_CH_DECODE(x)						\
  x == EkaUserChannel::TYPE::EPM_REPORT     ? "EPM_REPORT" :		\
    x == EkaUserChannel::TYPE::EPM_FEEDBACK ? "EPM_FEEDBACK"   :	\
    x == EkaUserChannel::TYPE::LWIP_RX      ? "LWIP_RX"   :		\
    "UNKNOWN"

class EkaUserChannel : public EkaDmaChannel<16> {
 public:
  enum class TYPE     : uint8_t {
				 EPM_REPORT = 0,
				 EPM_FEEDBACK = 1,
				 LWIP_RX  = 2
  } ; //user channel
  enum class DMA_TYPE : uint8_t {
                                 //below are used only in EPM_FEEDBACK channel
                                 EPM          = 3,
                                 //below are used only in LWIP_RX channel
 			         LWIP         = 4,
                                 //below are used only in EPM_REPORT channel
                                 FIRE         = 1,
				 EXCEPTION    = 6,
				 FAST_CANCEL  = 7,
				 NEWS         = 8,
			         SW_TRIGGERED = 9,
				 SWEEP        = 10
  }; // type in descriptor

  TYPE type;

  EkaUserChannel(EkaDev* pEkaDev,SN_DeviceId devId, TYPE t) :
    EkaDmaChannel(pEkaDev,devId) {
    
    type = t;

    EKA_LOG("Openning %s User Channel",
	    USR_CH_DECODE(type));

    ChannelId = SN_AllocateUserLogicChannel(dev_id,(int16_t)type, NULL);

    if (! ChannelId) {
      EKA_LOG("%s Channel is already acquired by other App instance",
	      USR_CH_DECODE(type));
      return;
    }

    EKA_LOG("UserChannel: SUCCESS for %s (=%u)",
	    USR_CH_DECODE(type),(int16_t)type);

    initPointers();
  }

  inline bool isOpen() {
    EKA_LOG("%s is %s",
	    USR_CH_DECODE(type),
	    ChannelId == NULL ? "CLOSED" : "OPEN");
    return (ChannelId != NULL);
  }

  inline const uint8_t* get() {
    return SN_GetUserLogicPayload(pIncomingUdpPacket);
  }
  
  ~EkaUserChannel() {};

  inline uint getPayloadSize() {
    return SN_GetUserLogicPayloadLength(pIncomingUdpPacket);
  }

};

#endif
