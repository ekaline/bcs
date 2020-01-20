#ifndef _EKA_FH_UDP_CHANNEL_H
#define _EKA_FH_UDP_CHANNEL_H
#include <stdint.h>
#include <unistd.h>
#include <thread>
#include <inttypes.h>

#include "smartnic.h"
#include "Exc.h"

#define EKA_UDP_PKT_MARK_P(pkt)  (((uint8_t*)pkt) - 28)


struct udphdr {
  u_int16_t source;
  u_int16_t dest;
  u_int16_t len;
  u_int16_t check;
};

class EkaDev;

class fh_udp_channel {
 public:
  fh_udp_channel(EfhCtx* pEfhCtx);
  ~fh_udp_channel();
  bool           has_data();
  const uint8_t* get();
  int16_t        getPayloadLen();
  uint16_t       getUdpPort();
  void           next();

  void igmp_mc_join (uint32_t src_ip, uint32_t mcast_ip, uint16_t port);


  private:
  EkaDev* dev;
  uint8_t core;

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
