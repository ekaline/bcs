#ifndef _EKA_UDP_CHANNEL_H
#define _EKA_UDP_CHANNEL_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>

#include "smartnic.h"

#define EKA_UDP_PKT_MARK_P(pkt)  (((uint8_t*)pkt) - 28)


/* struct udphdr { */
/*   u_int16_t source; */
/*   u_int16_t dest; */
/*   u_int16_t len; */
/*   u_int16_t check; */
/* }; */

class EkaDev;

struct EkaUdpChIgmpEntry {
  uint32_t ip;
  uint16_t port;
  int16_t  vlanTag;
};

class EkaUdpChannel {
 public:
  EkaUdpChannel(EkaDev* dev, uint8_t coreId);
  ~EkaUdpChannel();
  bool           has_data();
  const uint8_t* get();
  int16_t        getPayloadLen();
  uint16_t       getUdpPort();
  void           next();

  void igmp_mc_join (uint32_t src_ip, uint32_t mcast_ip, uint16_t port, int16_t vlanTag);


  private:
  EkaDev* dev;
  uint8_t core;

  static const uint MAX_IGMP_ENTRIES = 64;
  EkaUdpChIgmpEntry entry[MAX_IGMP_ENTRIES] = {};
  uint              subscribedIgmps = 0;

  //  int            sock_fd;   // for SW IGMP
  uint           ptr_update_ctr;
  uint64_t       packetBytesTotal;
  SN_ChannelId   ChannelId;
  int16_t        payloadLength;

  const uint8_t*          pkt_payload_ptr;

  const SN_Packet * pIncomingUdpPacket;
  const SN_Packet * pPreviousUdpPacket;
};



#endif
