#ifndef _EKA_UDP_CHANNEL_H
#define _EKA_UDP_CHANNEL_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>

#include "EkaDmaChannel.h"

//----------------------------------------------
class EkaUdpChannel : public EkaDmaChannel<1024> {
 public:
  EkaUdpChannel(EkaDev* dev, SN_DeviceId devId,
		uint8_t coreId, int requestedChId) :
    EkaDmaChannel(dev,devId) {
    core = coreId;

    if (!SN_IsUdpLane(dev_id, core))
      on_error("Lane %u is not configured as an UDP lane!",core);
    
    ChannelId = SN_AllocateUdpChannel(dev_id, core,
				      requestedChId, NULL);
    if (!ChannelId)
      on_error("Cannot open UDP channel");
    
    chId = SC_GetChannelNumber(ChannelId);
    if (requestedChId != -1 && requestedChId != chId) {
      on_error("requestedChId %d != chId %d",
	       requestedChId, chId);
    }

    EKA_LOG("UdpChannel %d: SUCCESS for lane %u",chId,core);

    initPointers();
  }

  //----------------------------------------------
  inline uint64_t emptyBuffer() {
    uint64_t pktCtr = 0;
    while (has_data()) {
      next();
      pktCtr++;
    }
    return pktCtr;
  } 
  //----------------------------------------------
  inline const uint8_t* get() {
    return SN_GetUdpPayload(pIncomingUdpPacket);
  }
  //----------------------------------------------
  inline int16_t getPayloadLen() {
    return SN_GetPacketPayloadLength(ChannelId,pIncomingUdpPacket);
  }
  //----------------------------------------------
  inline void igmp_mc_join (uint32_t mcast_ip,
			    uint16_t mcast_port,
			    int16_t vlanTag) {
  // MC Port here is in BE (Human) format

  SN_IgmpOptions igmpOptions = {};
  igmpOptions.StructSize = sizeof(igmpOptions);
  igmpOptions.EnableMulticastBypass = true;
  igmpOptions.VLanTag = vlanTag;

  char ip[20] = {};
  sprintf (ip, "%s",EKA_IP2STR(mcast_ip));

  SN_Error errorCode = SN_IgmpJoin(ChannelId,core,
				   (const char*)ip,mcast_port,
				   NULL /* vlanTag ? &igmpOptions : NULL */);
  if (errorCode != SN_ERR_SUCCESS) 
    on_error("Failed to join on core %u MC %s:%u, vlanTag=%d, error code %d",
	     core,ip,mcast_port,vlanTag,(int)errorCode);
  EKA_LOG("IGMP joined %s:%u for HW UDP Channel %d from coreId = %u",
	  ip,mcast_port,chId,core);

  return;
}
  //----------------------------------------------
  ~EkaUdpChannel() {};
  
public:
  int                     chId               = -1;
  
private:
  uint8_t                 core               = -1;
};
#endif
