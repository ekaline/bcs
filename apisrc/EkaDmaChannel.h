#ifndef _EKA_DMA_CHANNEL_H_
#define _EKA_DMA_CHANNEL_H_

#include "smartnic.h"

#include "EkaDev.h"
#include "eka_macros.h"

template <const int PtrUpdateLimit> class EkaDmaChannel {
protected:
  EkaDmaChannel(EkaDev *pEkaDev, SN_DeviceId devId) {
    dev = pEkaDev;
    dev_id = devId;

    ptrUpdateCtr = 0;
    pIncomingUdpPacket = NULL;
    pPreviousUdpPacket = NULL;
  }

  inline void initPointers() {
    EKA_LOG("Initializing DMA Channel pointers");
    while (1) {
      pPreviousUdpPacket = SC_GetNextPacket(
          ChannelId, pPreviousUdpPacket, SC_TIMEOUT_NONE);
      if (!pPreviousUdpPacket)
        break;
      if (SC_UpdateReceivePtr(ChannelId,
                              pPreviousUdpPacket) !=
          SN_ERR_SUCCESS)
        on_error("Failed to sync DMA ReceivePtr");
    }
  }

public:
  inline bool has_data() {
    pIncomingUdpPacket = SN_GetNextPacket(
        ChannelId, pPreviousUdpPacket, SN_TIMEOUT_NONE);

    if (pIncomingUdpPacket) {
      EKA_LOG("got pkt");
      fflush(g_ekaLogFile);
    }
    return pIncomingUdpPacket != NULL;
  }

  inline void next() {
    if (!pIncomingUdpPacket)
      on_error("!pIncomingUdpPacket");

    if (++ptrUpdateCtr % PtrUpdateLimit == 0)
      if (SN_UpdateReceivePtr(ChannelId,
                              pIncomingUdpPacket) !=
          SN_ERR_SUCCESS)
        on_error("Failed on SN_UpdateReceivePtr");
    pPreviousUdpPacket = pIncomingUdpPacket;
    return;
  }

  ~EkaDmaChannel() {
    SN_Error errorCode = SN_DeallocateChannel(ChannelId);
    if (errorCode != SN_ERR_SUCCESS)
      on_error(
          "SN_DeallocateChannel failed with error code %d",
          errorCode);
  }

protected:
  EkaDev *dev = NULL;
  SN_DeviceId dev_id;

  uint ptrUpdateCtr = 0;

  SN_ChannelId ChannelId;

  const SN_Packet *pIncomingUdpPacket = NULL;
  const SN_Packet *pPreviousUdpPacket = NULL;
};

#endif
